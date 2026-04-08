#!/usr/bin/env python3

import argparse
import hashlib
import json
import os
import shutil
import sys
from datetime import datetime
from pathlib import Path
from typing import Dict, Iterable, List, Optional, Tuple


PORT_SUFFIXES = ("_port.c", "_port.h")
MANAGED_ROOT_DIRS = {"port_packs", ".port_switch"}
IGNORED_DIR_NAMES = {".git", "__pycache__"}
STATE_DIR_NAME = ".port_switch"
BACKUP_DIR_NAME = "backup"
ACTIVE_STATE_FILE_NAME = "active_project.json"
BACKUP_META_FILE_NAME = "meta.json"
PACK_MANIFEST_FILE_NAME = "manifest.json"
BLANK_PROJECT_LABEL = "<blank>"


class PortPackError(RuntimeError):
    pass


SCRIPT_PATH = Path(__file__).resolve()
REPO_ROOT = SCRIPT_PATH.parents[1]
PACKS_ROOT = REPO_ROOT / "port_packs"
STATE_ROOT = REPO_ROOT / STATE_DIR_NAME
BACKUP_ROOT = STATE_ROOT / BACKUP_DIR_NAME
ACTIVE_STATE_FILE = STATE_ROOT / ACTIVE_STATE_FILE_NAME


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Capture, diff, switch, and restore project port file packs."
    )
    subparsers = parser.add_subparsers(dest="action")

    subparsers.add_parser("ui", help="Open the project selection UI.")

    parser_capture = subparsers.add_parser(
        "capture", help="Capture current workspace port files into a project pack."
    )
    parser_capture.add_argument("name", help="Project pack name.")

    parser_switch = subparsers.add_parser(
        "switch", help="Switch workspace managed files to a project pack."
    )
    parser_switch.add_argument("name", help="Project pack name.")

    parser_diff = subparsers.add_parser(
        "diff", help="Diff current workspace managed files against a project pack."
    )
    parser_diff.add_argument("name", help="Project pack name.")

    subparsers.add_parser("list", help="List available project packs and active state.")

    parser_restore = subparsers.add_parser(
        "restore", help="Restore workspace managed files from a backup."
    )
    parser_restore.add_argument(
        "name",
        nargs="?",
        help="Backup timestamp. If omitted, the latest backup is restored.",
    )

    args = parser.parse_args()
    if not args.action:
        args.action = "ui"

    return args


def validate_simple_name(name: str, label: str) -> str:
    if not name:
        raise PortPackError(f"{label} is required.")

    normalized = Path(name)
    if normalized.name != name or name in {".", ".."}:
        raise PortPackError(
            f"{label} must be a simple directory name without path separators: {name}"
        )

    return name


def normalize_relative_repo_path(relative_path: str, label: str) -> str:
    if not relative_path:
        raise PortPackError(f"{label} must not be empty.")

    candidate = Path(relative_path)
    if candidate.is_absolute() or not candidate.parts:
        raise PortPackError(f"{label} must be a repository-relative path: {relative_path}")

    normalized_parts: List[str] = []
    for part in candidate.parts:
        if part in {"", ".", ".."}:
            raise PortPackError(f"{label} contains invalid path segments: {relative_path}")
        normalized_parts.append(part)

    normalized = Path(*normalized_parts).as_posix()
    if Path(normalized).parts[0] in MANAGED_ROOT_DIRS:
        raise PortPackError(
            f"{label} must not point into managed runtime directories: {relative_path}"
        )

    return normalized


def should_skip_dir(relative_dir: Path, skip_managed_dirs: bool) -> bool:
    if not relative_dir.parts:
        return False

    if any(part in IGNORED_DIR_NAMES for part in relative_dir.parts):
        return True

    if skip_managed_dirs and relative_dir.parts[0] in MANAGED_ROOT_DIRS:
        return True

    return False


def scan_port_files(base_dir: Path, skip_managed_dirs: bool) -> Dict[str, Path]:
    file_map: Dict[str, Path] = {}

    for dirpath, dirnames, filenames in os.walk(base_dir, topdown=True):
        current_dir = Path(dirpath)
        relative_dir = current_dir.relative_to(base_dir)

        if should_skip_dir(relative_dir, skip_managed_dirs):
            dirnames[:] = []
            continue

        pruned_dirnames: List[str] = []
        for dirname in dirnames:
            child_relative = relative_dir / dirname if relative_dir.parts else Path(dirname)
            if should_skip_dir(child_relative, skip_managed_dirs):
                continue
            pruned_dirnames.append(dirname)
        dirnames[:] = pruned_dirnames

        for filename in filenames:
            if not filename.endswith(PORT_SUFFIXES):
                continue

            absolute_path = current_dir / filename
            relative_path = absolute_path.relative_to(base_dir).as_posix()
            file_map[relative_path] = absolute_path

    return dict(sorted(file_map.items()))


def scan_all_files(base_dir: Path, ignored_relative_paths: Optional[Iterable[str]] = None) -> Dict[str, Path]:
    ignored = set(ignored_relative_paths or [])
    file_map: Dict[str, Path] = {}

    for dirpath, _dirnames, filenames in os.walk(base_dir, topdown=True):
        current_dir = Path(dirpath)
        for filename in filenames:
            absolute_path = current_dir / filename
            relative_path = absolute_path.relative_to(base_dir).as_posix()
            if relative_path in ignored:
                continue
            file_map[relative_path] = absolute_path

    return dict(sorted(file_map.items()))


def hash_file(file_path: Path) -> str:
    digest = hashlib.sha256()
    with file_path.open("rb") as handle:
        while True:
            chunk = handle.read(1024 * 1024)
            if not chunk:
                break
            digest.update(chunk)
    return digest.hexdigest()


def compare_shared_files(
    left_map: Dict[str, Path], right_map: Dict[str, Path]
) -> Tuple[List[str], int]:
    shared_keys = sorted(set(left_map.keys()) & set(right_map.keys()))
    changed: List[str] = []
    same_count = 0

    for relative_path in shared_keys:
        if hash_file(left_map[relative_path]) == hash_file(right_map[relative_path]):
            same_count += 1
        else:
            changed.append(relative_path)

    return changed, same_count


def compare_file_maps(
    left_map: Dict[str, Path], right_map: Dict[str, Path]
) -> Tuple[List[str], List[str], List[str], int]:
    left_keys = set(left_map.keys())
    right_keys = set(right_map.keys())

    left_only = sorted(left_keys - right_keys)
    right_only = sorted(right_keys - left_keys)
    changed, same_count = compare_shared_files(left_map, right_map)
    return left_only, right_only, changed, same_count


def ensure_directory(path: Path) -> None:
    path.mkdir(parents=True, exist_ok=True)


def copy_file_map(file_map: Dict[str, Path], destination_root: Path) -> None:
    for relative_path, source_path in file_map.items():
        destination_path = destination_root / Path(relative_path)
        ensure_directory(destination_path.parent)
        shutil.copy2(source_path, destination_path)


def delete_relative_files(root_dir: Path, relative_paths: Iterable[str]) -> List[str]:
    deleted_paths: List[str] = []

    for relative_path in sorted(set(relative_paths)):
        file_path = root_dir / Path(relative_path)
        if not file_path.is_file():
            continue
        file_path.unlink()
        deleted_paths.append(relative_path)

    return deleted_paths


def delete_pack_port_files(pack_root: Path) -> List[str]:
    existing_port_files = scan_port_files(pack_root, skip_managed_dirs=False)
    return delete_relative_files(pack_root, existing_port_files.keys())


def read_json(path: Path) -> Optional[dict]:
    if not path.exists():
        return None

    with path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def write_json(path: Path, payload: dict) -> None:
    ensure_directory(path.parent)
    with path.open("w", encoding="utf-8", newline="\n") as handle:
        json.dump(payload, handle, indent=2, sort_keys=True)
        handle.write("\n")


def timestamp_now() -> str:
    return datetime.now().strftime("%Y%m%d_%H%M%S")


def get_pack_root(project_name: str) -> Path:
    return PACKS_ROOT / project_name


def get_backup_root(backup_name: str) -> Path:
    return BACKUP_ROOT / backup_name


def get_project_pack_names() -> List[str]:
    ensure_directory(PACKS_ROOT)
    return sorted(path.name for path in PACKS_ROOT.iterdir() if path.is_dir())


def workspace_port_files(require_any: bool = True) -> Dict[str, Path]:
    file_map = scan_port_files(REPO_ROOT, skip_managed_dirs=True)
    if require_any and not file_map:
        raise PortPackError("No workspace port files were found.")
    return file_map


def collect_workspace_files(relative_paths: Iterable[str]) -> Tuple[Dict[str, Path], List[str]]:
    file_map: Dict[str, Path] = {}
    missing: List[str] = []

    for relative_path in sorted(set(relative_paths)):
        workspace_path = REPO_ROOT / Path(relative_path)
        if workspace_path.is_file():
            file_map[relative_path] = workspace_path
        else:
            missing.append(relative_path)

    return file_map, missing


def collect_existing_workspace_files(relative_paths: Iterable[str]) -> Dict[str, Path]:
    file_map: Dict[str, Path] = {}

    for relative_path in sorted(set(relative_paths)):
        workspace_path = REPO_ROOT / Path(relative_path)
        if workspace_path.is_file():
            file_map[relative_path] = workspace_path

    return file_map


def read_pack_manifest_extra_files(project_name: str) -> List[str]:
    pack_root = get_pack_root(project_name)
    manifest_path = pack_root / PACK_MANIFEST_FILE_NAME
    payload = read_json(manifest_path)

    if payload is None:
        return []
    if not isinstance(payload, dict):
        raise PortPackError(f"Pack manifest must be a JSON object: {manifest_path}")

    extra_files_raw = payload.get("extraFiles", [])
    if extra_files_raw is None:
        return []
    if not isinstance(extra_files_raw, list):
        raise PortPackError(f"manifest extraFiles must be a JSON array: {manifest_path}")

    extra_files: List[str] = []
    seen: set[str] = set()

    for item in extra_files_raw:
        if not isinstance(item, str):
            raise PortPackError(f"manifest extraFiles entries must be strings: {manifest_path}")

        normalized = normalize_relative_repo_path(item, "manifest extra file")
        if normalized in seen:
            raise PortPackError(f"manifest extraFiles contains duplicates: {normalized}")
        seen.add(normalized)
        extra_files.append(normalized)

    return extra_files


def pack_managed_files(project_name: str) -> Tuple[Dict[str, Path], List[str]]:
    pack_root = get_pack_root(project_name)
    if not pack_root.is_dir():
        raise PortPackError(f"Project pack does not exist: {pack_root}")

    port_files = scan_port_files(pack_root, skip_managed_dirs=False)
    if not port_files:
        raise PortPackError(f"Project pack does not contain any port files: {pack_root}")

    managed_files = dict(port_files)
    extra_files = read_pack_manifest_extra_files(project_name)
    for relative_path in extra_files:
        if relative_path in managed_files:
            raise PortPackError(
                f"manifest extraFiles must not repeat port-managed files: {relative_path}"
            )

        pack_file_path = pack_root / Path(relative_path)
        if not pack_file_path.is_file():
            raise PortPackError(
                f"manifest extra file does not exist inside the pack: {pack_file_path}"
            )

        managed_files[relative_path] = pack_file_path

    return dict(sorted(managed_files.items())), extra_files


def read_active_project_name() -> Optional[str]:
    active_state = read_json(ACTIVE_STATE_FILE) or {}
    active_project = active_state.get("active_project")
    if isinstance(active_project, str) and active_project:
        return active_project
    return None


def save_current_workspace_to_project(project_name: str) -> Tuple[int, int]:
    validate_simple_name(project_name, "project name")

    current_port_files = workspace_port_files(require_any=True)
    pack_root = get_pack_root(project_name)
    existing_extra_files: List[str] = []

    if pack_root.is_dir():
        existing_extra_files = read_pack_manifest_extra_files(project_name)
        delete_pack_port_files(pack_root)
    else:
        ensure_directory(pack_root)

    current_extra_files, missing_extra_files = collect_workspace_files(existing_extra_files)
    if missing_extra_files:
        raise PortPackError(
            "Workspace is missing manifest-declared extra files for the current project:\n"
            + "\n".join(missing_extra_files)
        )

    managed_files = dict(current_port_files)
    managed_files.update(current_extra_files)
    copy_file_map(managed_files, pack_root)

    return len(current_port_files), len(existing_extra_files)


def build_transition_workspace_files(
    current_project: Optional[str], target_extra_files: Iterable[str]
) -> Tuple[Dict[str, Path], Dict[str, Path], List[str]]:
    current_port_files = workspace_port_files(require_any=False)
    current_extra_paths: List[str] = []

    if current_project and get_pack_root(current_project).is_dir():
        current_extra_paths = read_pack_manifest_extra_files(current_project)

    workspace_extra_candidates = set(current_extra_paths) | set(target_extra_files)
    workspace_extra_files = collect_existing_workspace_files(workspace_extra_candidates)

    current_managed_files = dict(current_port_files)
    current_managed_files.update(workspace_extra_files)

    return current_port_files, current_managed_files, current_extra_paths


def backup_managed_files(
    current_files: Dict[str, Path], active_project: Optional[str], target_project: str
) -> str:
    backup_name = timestamp_now()
    backup_root = get_backup_root(backup_name)
    ensure_directory(backup_root)
    copy_file_map(current_files, backup_root)

    write_json(
        backup_root / BACKUP_META_FILE_NAME,
        {
            "active_project_before": active_project,
            "backup_created_at": datetime.now().isoformat(timespec="seconds"),
            "managed_file_count": len(current_files),
            "managed_files": sorted(current_files.keys()),
            "switch_target": target_project,
        },
    )

    return backup_name


def write_active_state(
    action: str, active_project: Optional[str], backup_name: Optional[str]
) -> None:
    payload = {
        "action": action,
        "active_project": active_project,
        "updated_at": datetime.now().isoformat(timespec="seconds"),
    }
    if backup_name:
        payload["backup_name"] = backup_name

    write_json(ACTIVE_STATE_FILE, payload)


def list_project_packs() -> int:
    project_names = get_project_pack_names()
    active_project = read_active_project_name() or "none"

    print(f"repo_root: {REPO_ROOT}")
    print(f"active_project: {active_project}")
    print(f"project_pack_count: {len(project_names)}")

    for project_name in project_names:
        try:
            _managed_files, extra_files = pack_managed_files(project_name)
            print(f"  {project_name} extra_files={len(extra_files)}")
        except PortPackError as error:
            print(f"  {project_name} invalid_manifest={error}")

    if BACKUP_ROOT.exists():
        backup_names = sorted(path.name for path in BACKUP_ROOT.iterdir() if path.is_dir())
        print(f"backup_count: {len(backup_names)}")
        if backup_names:
            print(f"latest_backup: {backup_names[-1]}")
    else:
        print("backup_count: 0")

    return 0


def capture_project_pack(project_name: str) -> int:
    validate_simple_name(project_name, "project name")
    current_files = workspace_port_files(require_any=True)
    pack_root = get_pack_root(project_name)

    if pack_root.exists():
        raise PortPackError(f"Project pack already exists: {pack_root}")

    ensure_directory(PACKS_ROOT)
    copy_file_map(current_files, pack_root)

    print(f"captured_project: {project_name}")
    print(f"captured_port_file_count: {len(current_files)}")
    print(f"pack_root: {pack_root}")
    return 0


def diff_project_pack(project_name: str) -> int:
    validate_simple_name(project_name, "project name")
    target_files, target_extra_files = pack_managed_files(project_name)
    current_port_files, current_managed_files, _current_extra_paths = build_transition_workspace_files(
        read_active_project_name(), target_extra_files
    )
    extra_in_workspace, missing_in_workspace, changed, same_count = compare_file_maps(
        current_managed_files, target_files
    )

    print(f"project: {project_name}")
    print(f"workspace_port_file_count: {len(current_port_files)}")
    print(f"pack_managed_file_count: {len(target_files)}")
    print(f"manifest_extra_file_count: {len(target_extra_files)}")
    print(f"same_file_count: {same_count}")
    print(f"changed_file_count: {len(changed)}")
    print(f"extra_in_workspace_count: {len(extra_in_workspace)}")
    print(f"missing_in_workspace_count: {len(missing_in_workspace)}")

    if target_extra_files:
        print("manifest_extra_files:")
        for relative_path in target_extra_files:
            print(f"  {relative_path}")

    if extra_in_workspace:
        print("extra_in_workspace:")
        for relative_path in extra_in_workspace:
            print(f"  {relative_path}")

    if missing_in_workspace:
        print("missing_in_workspace:")
        for relative_path in missing_in_workspace:
            print(f"  {relative_path}")

    if changed:
        print("changed_files:")
        for relative_path in changed:
            print(f"  {relative_path}")

    if not extra_in_workspace and not missing_in_workspace and not changed:
        print("workspace matches project pack exactly.")

    return 0


def switch_project_pack(project_name: str, current_project: Optional[str] = None) -> int:
    validate_simple_name(project_name, "project name")
    active_project_before = current_project or read_active_project_name()
    target_files, target_extra_files = pack_managed_files(project_name)

    current_port_files, current_managed_files, _current_extra_paths = build_transition_workspace_files(
        active_project_before, target_extra_files
    )
    extra_in_workspace, missing_in_workspace, changed, _same_count = compare_file_maps(
        current_managed_files, target_files
    )

    if not extra_in_workspace and not missing_in_workspace and not changed:
        write_active_state("switch", project_name, backup_name=None)
        print(f"project pack already matches workspace: {project_name}")
        print(f"managed_file_count: {len(target_files)}")
        print(f"manifest_extra_file_count: {len(target_extra_files)}")
        return 0

    backup_name = backup_managed_files(current_managed_files, active_project_before, project_name)
    deleted_port_files = delete_relative_files(REPO_ROOT, current_port_files.keys())
    copy_file_map(target_files, REPO_ROOT)
    write_active_state("switch", project_name, backup_name=backup_name)

    print(f"switched_project: {project_name}")
    print(f"backup_name: {backup_name}")
    print(f"managed_file_count: {len(target_files)}")
    print(f"manifest_extra_file_count: {len(target_extra_files)}")
    print(f"deleted_port_file_count: {len(deleted_port_files)}")
    print(f"created_file_count: {len(missing_in_workspace)}")
    print(f"changed_file_count: {len(changed)}")

    if target_extra_files:
        print("managed_extra_files:")
        for relative_path in target_extra_files:
            print(f"  {relative_path}")

    if changed or missing_in_workspace:
        print("applied_files:")
        for relative_path in sorted(set(changed + missing_in_workspace)):
            print(f"  {relative_path}")

    return 0


def switch_to_blank_project(current_project: Optional[str]) -> int:
    current_port_files, current_managed_files, current_extra_paths = build_transition_workspace_files(
        current_project, []
    )

    if not current_project and current_port_files:
        raise PortPackError(
            "Current project must be selected before switching to the blank project."
        )

    backup_name = backup_managed_files(current_managed_files, current_project, BLANK_PROJECT_LABEL)
    deleted_port_files = delete_relative_files(REPO_ROOT, current_port_files.keys())
    write_active_state("blank", None, backup_name=backup_name)

    print(f"switched_project: {BLANK_PROJECT_LABEL}")
    print(f"backup_name: {backup_name}")
    print(f"deleted_port_file_count: {len(deleted_port_files)}")
    print(f"retained_extra_file_count: {len(current_extra_paths)}")

    if deleted_port_files:
        print("deleted_port_files:")
        for relative_path in deleted_port_files:
            print(f"  {relative_path}")

    return 0


def latest_backup_name() -> str:
    if not BACKUP_ROOT.exists():
        raise PortPackError("No backups exist.")

    backup_names = sorted(path.name for path in BACKUP_ROOT.iterdir() if path.is_dir())
    if not backup_names:
        raise PortPackError("No backups exist.")

    return backup_names[-1]


def restore_backup(backup_name: Optional[str]) -> int:
    selected_backup_name = backup_name or latest_backup_name()
    validate_simple_name(selected_backup_name, "backup name")
    backup_root = get_backup_root(selected_backup_name)

    if not backup_root.is_dir():
        raise PortPackError(f"Backup does not exist: {backup_root}")

    backup_meta = read_json(backup_root / BACKUP_META_FILE_NAME) or {}
    managed_paths_raw = backup_meta.get("managed_files")

    if isinstance(managed_paths_raw, list):
        backup_files: Dict[str, Path] = {}
        missing_in_backup: List[str] = []

        for item in managed_paths_raw:
            if not isinstance(item, str):
                raise PortPackError(
                    f"Backup metadata contains a non-string managed file path: {backup_root}"
                )

            relative_path = normalize_relative_repo_path(item, "backup managed file")
            backup_file_path = backup_root / Path(relative_path)
            if backup_file_path.is_file():
                backup_files[relative_path] = backup_file_path
            else:
                missing_in_backup.append(relative_path)

        if missing_in_backup:
            raise PortPackError(
                "Backup is incomplete. Missing managed files:\n" + "\n".join(missing_in_backup)
            )
    else:
        backup_files = scan_all_files(
            backup_root, ignored_relative_paths={BACKUP_META_FILE_NAME}
        )

    changed: List[str] = []
    for relative_path, backup_file_path in backup_files.items():
        workspace_path = REPO_ROOT / Path(relative_path)
        if not workspace_path.is_file():
            changed.append(relative_path)
            continue

        if hash_file(workspace_path) != hash_file(backup_file_path):
            changed.append(relative_path)

    copy_file_map(backup_files, REPO_ROOT)

    restored_project = backup_meta.get("active_project_before") or "restored"
    write_active_state("restore", str(restored_project), backup_name=selected_backup_name)

    print(f"restored_backup: {selected_backup_name}")
    print(f"restored_project: {restored_project}")
    print(f"restored_file_count: {len(changed)}")

    if changed:
        print("restored_files:")
        for relative_path in changed:
            print(f"  {relative_path}")

    return 0


def normalize_ui_project_name(raw_value: str) -> Optional[str]:
    project_name = raw_value.strip()
    if not project_name or project_name == BLANK_PROJECT_LABEL:
        return None
    return validate_simple_name(project_name, "project name")


def apply_ui_selection(current_value: str, target_value: str) -> str:
    current_project = normalize_ui_project_name(current_value)
    target_project = normalize_ui_project_name(target_value)
    workspace_ports = workspace_port_files(require_any=False)

    summary_lines: List[str] = []

    if workspace_ports:
        if not current_project:
            raise PortPackError(
                "Current project is required so the existing workspace port files can be saved first."
            )
        saved_port_count, saved_extra_count = save_current_workspace_to_project(current_project)
        summary_lines.append(
            f"saved_current_project: {current_project} port_files={saved_port_count} extra_files={saved_extra_count}"
        )

    if target_project is None:
        switch_to_blank_project(current_project)
        summary_lines.append(f"target_project: {BLANK_PROJECT_LABEL}")
    else:
        switch_project_pack(target_project, current_project=current_project)
        summary_lines.append(f"target_project: {target_project}")

    return "\n".join(summary_lines)


def open_project_selection_ui() -> int:
    try:
        import tkinter as tk
        from tkinter import messagebox, ttk
    except ImportError as error:
        raise PortPackError(f"Tkinter is not available: {error}") from error

    root = tk.Tk()
    root.title("Port Pack Manager")
    root.resizable(False, False)

    current_var = tk.StringVar()
    target_var = tk.StringVar()
    status_var = tk.StringVar()
    active_var = tk.StringVar()
    port_count_var = tk.StringVar()

    frame = ttk.Frame(root, padding=12)
    frame.grid(row=0, column=0, sticky="nsew")

    ttk.Label(frame, text="Repository").grid(row=0, column=0, sticky="w")
    ttk.Label(frame, text=str(REPO_ROOT)).grid(row=0, column=1, columnspan=2, sticky="w")

    ttk.Label(frame, textvariable=active_var).grid(row=1, column=0, columnspan=3, sticky="w", pady=(8, 0))
    ttk.Label(frame, textvariable=port_count_var).grid(row=2, column=0, columnspan=3, sticky="w")

    ttk.Label(frame, text="Current Project").grid(row=3, column=0, sticky="w", pady=(12, 4))
    current_combo = ttk.Combobox(frame, textvariable=current_var, width=36)
    current_combo.grid(row=3, column=1, columnspan=2, sticky="we", pady=(12, 4))

    ttk.Label(frame, text="Target Project").grid(row=4, column=0, sticky="w", pady=4)
    target_combo = ttk.Combobox(frame, textvariable=target_var, width=36)
    target_combo.grid(row=4, column=1, columnspan=2, sticky="we", pady=4)

    ttk.Label(
        frame,
        text="Select a target project or choose <blank> to save the current project first and then delete workspace port files.",
        wraplength=520,
        justify="left",
    ).grid(row=5, column=0, columnspan=3, sticky="w", pady=(8, 12))

    ttk.Label(frame, textvariable=status_var).grid(row=6, column=0, columnspan=3, sticky="w", pady=(0, 8))

    def refresh_ui_state() -> None:
        project_names = get_project_pack_names()
        active_project = read_active_project_name()
        current_combo["values"] = project_names
        target_combo["values"] = [BLANK_PROJECT_LABEL] + project_names

        active_var.set(f"Active project: {active_project or BLANK_PROJECT_LABEL}")
        port_count_var.set(
            f"Workspace port files: {len(workspace_port_files(require_any=False))}"
        )
        status_var.set(f"Available project packs: {len(project_names)}")

        if not current_var.get().strip():
            current_var.set(active_project or "")
        if not target_var.get().strip():
            target_var.set(active_project or BLANK_PROJECT_LABEL)

    def apply_selection() -> None:
        current_project = normalize_ui_project_name(current_var.get())
        target_project = normalize_ui_project_name(target_var.get())
        confirm_current = current_project or "<unspecified>"
        confirm_target = target_project or BLANK_PROJECT_LABEL

        if not messagebox.askyesno(
            title="Apply Project Switch",
            message=(
                "Current project: "
                + confirm_current
                + "\nTarget project: "
                + confirm_target
                + "\n\nApply this operation?"
            ),
            parent=root,
        ):
            return

        try:
            summary = apply_ui_selection(current_var.get(), target_var.get())
        except PortPackError as error:
            messagebox.showerror("Port Pack Manager", str(error), parent=root)
            status_var.set(f"Error: {error}")
            return

        refresh_ui_state()
        status_var.set("Operation completed successfully.")
        messagebox.showinfo("Port Pack Manager", summary or "Operation completed.", parent=root)

    button_row = ttk.Frame(frame)
    button_row.grid(row=7, column=0, columnspan=3, sticky="e")
    ttk.Button(button_row, text="Refresh", command=refresh_ui_state).grid(row=0, column=0, padx=(0, 8))
    ttk.Button(button_row, text="Apply", command=apply_selection).grid(row=0, column=1, padx=(0, 8))
    ttk.Button(button_row, text="Close", command=root.destroy).grid(row=0, column=2)

    refresh_ui_state()
    root.mainloop()
    return 0


def main() -> int:
    args = parse_args()

    try:
        if args.action == "ui":
            return open_project_selection_ui()
        if args.action == "capture":
            return capture_project_pack(args.name)
        if args.action == "switch":
            return switch_project_pack(args.name)
        if args.action == "diff":
            return diff_project_pack(args.name)
        if args.action == "list":
            return list_project_packs()
        if args.action == "restore":
            return restore_backup(args.name)

        raise PortPackError(f"Unsupported action: {args.action}")
    except PortPackError as error:
        print(f"error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())