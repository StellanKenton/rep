#!/usr/bin/env python3

from __future__ import annotations

import argparse
import json
import stat
import sys
from copy import deepcopy
from pathlib import Path, PurePosixPath
from typing import Any, Dict, List


DEFAULT_SPEC: Dict[str, Any] = {
    "build": {
        "sourceDirectory": ".",
        "buildDirectory": "build/Debug",
        "buildType": "Debug",
        "generator": "Ninja",
        "toolchainFile": "cmake/toolchains/arm-none-eabi.cmake",
        "extraConfigureArgs": [],
        "exportCompileCommands": True,
        "clangdCompileCommandsDir": None,
        "elfPath": None,
        "hexPath": None,
        "cmakePath": "cmake",
        "ninjaPath": "ninja",
        "armGdbPath": None,
    },
    "jlink": {
        "interface": "swd",
        "speed": 4000,
        "jlinkExe": "JLinkExe",
        "jlinkGdbServer": "JLinkGDBServer",
    },
    "ports": {
        "debug": {
            "gdb": 2331,
            "swo": 2332,
            "telnet": 2333,
            "rtt": 19021,
        },
        "rttServer": {
            "gdb": 2341,
            "swo": 2342,
            "telnet": 2343,
            "rtt": 19031,
        },
    },
    "debug": {
        "runToEntryPoint": "main",
        "postLaunchCommands": ["monitor reset", "monitor halt"],
        "rttEnabled": True,
    },
    "extensions": [
        "ms-vscode.cpptools",
        "ms-vscode.cmake-tools",
        "llvm-vs-code-extensions.vscode-clangd",
        "marus25.cortex-debug",
        "seunlanlege.action-buttons",
        "usernamehw.commands",
    ],
}

SCRIPT_CONTENTS: Dict[str, str] = {
    ".vscode/scripts/cmake_configure.sh": "#!/bin/zsh\n\nset -euo pipefail\n\nworkspace=\"$(cd \"$(dirname \"$0\")/../..\" && pwd)\"\npython_user_bin=\"$HOME/Library/Python/3.9/bin\"\ntool_bin=\"$workspace/.tools/arm-gnu-toolchain/bin\"\nexport PATH=\"$python_user_bin:/usr/local/bin:/opt/homebrew/bin:$tool_bin:$PATH\"\n\nsource_dir=\"$1\"\nbuild_dir=\"$2\"\ngenerator=\"$3\"\ntoolchain_file=\"$4\"\narm_toolchain_bin_dir=\"$5\"\nbuild_type=\"$6\"\nexport_compile_commands=\"$7\"\ncmake_bin=\"${8:-cmake}\"\nninja_bin=\"${9:-ninja}\"\nshift 9\n\ncache_file=\"$build_dir/CMakeCache.txt\"\nexpected_compiler=\"$arm_toolchain_bin_dir/arm-none-eabi-gcc\"\n\nrequire_tool() {\n  local tool=\"$1\"\n\n  if ! command -v \"$tool\" >/dev/null 2>&1; then\n    print -ru2 -- \"[cmake-configure] Required tool not found: $tool\"\n    print -ru2 -- \"[cmake-configure] PATH=$PATH\"\n    exit 127\n  fi\n}\n\ncache_value() {\n  local key=\"$1\"\n\n  if [[ ! -f \"$cache_file\" ]]; then\n    return 0\n  fi\n\n  sed -n \"s|^${key}:[^=]*=||p\" \"$cache_file\" | head -n 1\n}\n\nreset_build_dir() {\n  print -r -- \"[cmake-configure] Removing stale CMake cache from $build_dir\"\n  rm -f \"$build_dir/CMakeCache.txt\"\n  rm -rf \"$build_dir/CMakeFiles\" \"$build_dir/.cmake\"\n  rm -f \"$build_dir/build.ninja\" \"$build_dir/cmake_install.cmake\" \"$build_dir/compile_commands.json\"\n}\n\nmkdir -p \"$build_dir\"\n\nrequire_tool \"$cmake_bin\"\nrequire_tool \"$ninja_bin\"\n\nif [[ ! -x \"$expected_compiler\" ]]; then\n  print -ru2 -- \"[cmake-configure] Arm compiler not found or not executable: $expected_compiler\"\n  exit 127\nfi\n\nif [[ -f \"$cache_file\" ]]; then\n  cached_toolchain_file=\"$(cache_value CMAKE_TOOLCHAIN_FILE)\"\n  cached_arm_toolchain_bin_dir=\"$(cache_value ARM_GNU_TOOLCHAIN_BIN_DIR)\"\n  cached_c_compiler=\"$(cache_value CMAKE_C_COMPILER)\"\n  cached_asm_compiler=\"$(cache_value CMAKE_ASM_COMPILER)\"\n\n  if [[ \"$cached_toolchain_file\" != \"$toolchain_file\" ]] \\\n    || [[ \"$cached_arm_toolchain_bin_dir\" != \"$arm_toolchain_bin_dir\" ]] \\\n    || [[ \"$cached_c_compiler\" != \"$expected_compiler\" ]] \\\n    || [[ \"$cached_asm_compiler\" != \"$expected_compiler\" ]]; then\n    reset_build_dir\n  fi\nfi\n\nexec \"$cmake_bin\" \\\n  -S \"$source_dir\" \\\n  -B \"$build_dir\" \\\n  -G \"$generator\" \\\n  -DCMAKE_MAKE_PROGRAM=\"$(command -v \"$ninja_bin\")\" \\\n  -DCMAKE_TOOLCHAIN_FILE=$toolchain_file \\\n  -DARM_GNU_TOOLCHAIN_BIN_DIR=$arm_toolchain_bin_dir \\\n  -DCMAKE_BUILD_TYPE=$build_type \\\n  -DCMAKE_EXPORT_COMPILE_COMMANDS=$export_compile_commands \\\n  \"$@\"\n",
    ".vscode/scripts/jlink_flash.sh": "#!/bin/zsh\n\nset -euo pipefail\n\ndevice=\"$1\"\ninterface=\"$2\"\nspeed=\"$3\"\nimage=\"$4\"\njlink_exe=\"${5:-JLinkExe}\"\n\nif [[ ! -f \"$image\" ]]; then\n  echo \"Firmware image not found: $image\" >&2\n  exit 1\nfi\n\n{\n  print -r -- \"r\"\n  print -r -- \"h\"\n  print -r -- \"loadfile $image\"\n  print -r -- \"r\"\n  print -r -- \"g\"\n  print -r -- \"qc\"\n} | \"$jlink_exe\" -device \"$device\" -if \"$interface\" -speed \"$speed\"\n",
    ".vscode/scripts/jlink_reset.sh": "#!/bin/zsh\n\nset -euo pipefail\n\ndevice=\"$1\"\ninterface=\"$2\"\nspeed=\"$3\"\njlink_exe=\"${4:-JLinkExe}\"\n\n{\n  print -r -- \"r\"\n  print -r -- \"g\"\n  print -r -- \"qc\"\n} | \"$jlink_exe\" -device \"$device\" -if \"$interface\" -speed \"$speed\"\n",
    ".vscode/scripts/jlink_rtt_console.sh": "#!/bin/zsh\n\nset -euo pipefail\n\ndevice=\"$1\"\ninterface=\"$2\"\nspeed=\"$3\"\ngdb_port=\"$4\"\nswo_port=\"$5\"\ntelnet_port=\"$6\"\nrtt_port=\"$7\"\nserver_bin=\"${8:-JLinkGDBServer}\"\nhost=\"127.0.0.1\"\nserver_pid=\"\"\nserver_started=0\nserver_log=\"\"\n\ncleanup() {\n  if [[ \"$server_started\" -eq 1 && -n \"$server_pid\" ]]; then\n    kill \"$server_pid\" 2>/dev/null || true\n    wait \"$server_pid\" 2>/dev/null || true\n  fi\n\n  if [[ -n \"$server_log\" && -f \"$server_log\" ]]; then\n    rm -f \"$server_log\"\n  fi\n}\n\ntrap cleanup EXIT INT TERM\n\nport_listening() {\n  lsof -tiTCP:\"$1\" -sTCP:LISTEN >/dev/null 2>&1\n}\n\nfor port in \"$gdb_port\" \"$swo_port\" \"$telnet_port\" \"$rtt_port\"; do\n  pids=\"$(lsof -tiTCP:\"$port\" -sTCP:LISTEN 2>/dev/null || true)\"\n  if [[ -n \"$pids\" ]]; then\n    kill $pids 2>/dev/null || true\n  fi\ndone\n\nsleep 0.3\n\nserver_log=\"$(mktemp -t cpr-rtt.XXXXXX.log)\"\n\n\"$server_bin\" \\\n  -device \"$device\" \\\n  -if \"$interface\" \\\n  -speed \"$speed\" \\\n  -nohalt \\\n  -port \"$gdb_port\" \\\n  -swoport \"$swo_port\" \\\n  -telnetport \"$telnet_port\" \\\n  -RTTTelnetPort \"$rtt_port\" \\\n  >\"$server_log\" 2>&1 &\n\nserver_pid=\"$!\"\nserver_started=1\n\nfor _ in {1..50}; do\n  if port_listening \"$rtt_port\"; then\n    break\n  fi\n\n  if ! kill -0 \"$server_pid\" 2>/dev/null; then\n    cat \"$server_log\"\n    exit 1\n  fi\n\n  sleep 0.2\ndone\n\nif ! port_listening \"$rtt_port\"; then\n  cat \"$server_log\"\n  exit 1\nfi\n\nprint \"RTT console ready on ${host}:${rtt_port}\"\nprint \"Type directly in this terminal and press Enter to send data. Press Ctrl+C to close RTT.\"\n\nnc \"$host\" \"$rtt_port\"\n",
    ".vscode/scripts/jlink_rtt_server.sh": "#!/bin/zsh\n\nset -euo pipefail\n\ndevice=\"$1\"\ninterface=\"$2\"\nspeed=\"$3\"\ngdb_port=\"$4\"\nswo_port=\"$5\"\ntelnet_port=\"$6\"\nrtt_port=\"$7\"\nserver_bin=\"${8:-JLinkGDBServer}\"\n\nexec \"$server_bin\" \\\n  -device \"$device\" \\\n  -if \"$interface\" \\\n  -speed \"$speed\" \\\n  -nohalt \\\n  -port \"$gdb_port\" \\\n  -swoport \"$swo_port\" \\\n  -telnetport \"$telnet_port\" \\\n  -RTTTelnetPort \"$rtt_port\"\n",
    ".vscode/scripts/jlink_rtt_monitor.sh": "#!/bin/zsh\n\nset -euo pipefail\n\nhost=\"$1\"\nport=\"$2\"\n\nexec nc \"$host\" \"$port\"\n",
    ".vscode/scripts/jlink_write_rtt.sh": "#!/bin/zsh\n\nset -euo pipefail\n\nhost=\"$1\"\nport=\"$2\"\npayload=\"$3\"\n\nprint -r -- \"$payload\" | nc \"$host\" \"$port\"\n",
}


class DeployError(RuntimeError):
    pass


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Deploy a reusable macOS VS Code CMake/J-Link/RTT workspace setup."
    )
    parser.add_argument(
        "--config",
        default=str(Path(__file__).with_name("project.json")),
        help="Path to the project JSON file. Defaults to project.json next to deploy.py.",
    )
    parser.add_argument(
        "--target",
        help="Override the target workspace root from the config file.",
    )
    return parser.parse_args()


def deep_merge(base: Dict[str, Any], overrides: Dict[str, Any]) -> Dict[str, Any]:
    merged = deepcopy(base)
    for key, value in overrides.items():
        if isinstance(value, dict) and isinstance(merged.get(key), dict):
            merged[key] = deep_merge(merged[key], value)
        else:
            merged[key] = value
    return merged


def load_spec(config_path: Path) -> Dict[str, Any]:
    if not config_path.is_file():
        raise DeployError(f"Config file not found: {config_path}")

    try:
        payload = json.loads(config_path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        raise DeployError(f"Config JSON is invalid: {exc}") from exc

    spec = deep_merge(DEFAULT_SPEC, payload)
    spec["_config_dir"] = str(config_path.parent.resolve())
    return spec


def require_string(value: Any, field_name: str) -> str:
    if not isinstance(value, str) or not value.strip():
        raise DeployError(f"{field_name} must be a non-empty string.")
    return value.strip()


def normalize_relative_path(path_text: str, field_name: str) -> str:
    raw = require_string(path_text, field_name)
    candidate = Path(raw)
    if candidate.is_absolute():
        return PurePosixPath(candidate.as_posix()).as_posix()

    normalized = PurePosixPath(raw).as_posix()
    if normalized in {"", "."}:
        return "."

    if normalized.startswith("../") or normalized == "..":
        raise DeployError(f"{field_name} must stay inside the workspace: {raw}")

    return normalized


def ensure_workspace_path(path_text: str, config_dir: Path) -> Path:
    candidate = Path(path_text)
    if not candidate.is_absolute():
        candidate = (config_dir / candidate).resolve()
    return candidate


def to_vscode_path(relative_or_absolute: str) -> str:
    candidate = Path(relative_or_absolute)
    if candidate.is_absolute():
        return PurePosixPath(candidate.as_posix()).as_posix()

    normalized = PurePosixPath(relative_or_absolute).as_posix()
    if normalized in {"", "."}:
        return "${workspaceFolder}"
    return "${workspaceFolder}/" + normalized


def as_string(value: Any, field_name: str) -> str:
    if isinstance(value, bool):
        raise DeployError(f"{field_name} must be a string or number.")
    if isinstance(value, int):
        return str(value)
    if isinstance(value, float):
        return str(int(value)) if value.is_integer() else str(value)
    if isinstance(value, str) and value.strip():
        return value.strip()
    raise DeployError(f"{field_name} must be a string or number.")


def normalize_list(value: Any, field_name: str) -> List[str]:
    if value is None:
        return []
    if not isinstance(value, list) or not all(isinstance(item, str) for item in value):
        raise DeployError(f"{field_name} must be a string array.")
    return value


def build_runtime_spec(spec: Dict[str, Any], target_override: str | None) -> Dict[str, Any]:
    config_dir = Path(spec["_config_dir"])

    target_workspace_text = target_override or spec.get("targetWorkspace")
    target_workspace = ensure_workspace_path(
        require_string(target_workspace_text, "targetWorkspace"),
        config_dir,
    )

    project_name = require_string(spec.get("projectName"), "projectName")
    build_spec = spec.get("build", {})
    jlink_spec = spec.get("jlink", {})
    ports_spec = spec.get("ports", {})
    debug_spec = spec.get("debug", {})

    artifact_name = require_string(build_spec.get("artifactName"), "build.artifactName")
    source_directory = normalize_relative_path(
        build_spec.get("sourceDirectory", "."),
        "build.sourceDirectory",
    )
    build_directory = normalize_relative_path(
        build_spec.get("buildDirectory", "build/Debug"),
        "build.buildDirectory",
    )
    toolchain_file = normalize_relative_path(
        build_spec.get("toolchainFile"),
        "build.toolchainFile",
    )
    arm_toolchain_bin_dir = require_string(
        build_spec.get("armToolchainBinDir"),
        "build.armToolchainBinDir",
    )
    arm_gdb_path = build_spec.get("armGdbPath")
    if arm_gdb_path is None:
        arm_gdb_path = str(Path(arm_toolchain_bin_dir) / "arm-none-eabi-gdb")
    else:
        arm_gdb_path = require_string(arm_gdb_path, "build.armGdbPath")

    export_compile_commands = bool(build_spec.get("exportCompileCommands", True))
    clangd_compile_commands_dir = build_spec.get("clangdCompileCommandsDir")
    if clangd_compile_commands_dir is None:
        clangd_compile_commands_dir = build_directory
    else:
        clangd_compile_commands_dir = normalize_relative_path(
            clangd_compile_commands_dir,
            "build.clangdCompileCommandsDir",
        )

    elf_path = build_spec.get("elfPath") or f"{build_directory}/{artifact_name}.elf"
    hex_path = build_spec.get("hexPath") or f"{build_directory}/{artifact_name}.hex"
    elf_path = normalize_relative_path(elf_path, "build.elfPath")
    hex_path = normalize_relative_path(hex_path, "build.hexPath")

    extra_configure_args = normalize_list(
        build_spec.get("extraConfigureArgs"),
        "build.extraConfigureArgs",
    )
    cmake_path = require_string(build_spec.get("cmakePath"), "build.cmakePath")
    ninja_path = require_string(build_spec.get("ninjaPath"), "build.ninjaPath")
    extensions = normalize_list(spec.get("extensions"), "extensions")

    debug_ports = ports_spec.get("debug", {})
    rtt_server_ports = ports_spec.get("rttServer", {})

    runtime = {
        "targetWorkspace": target_workspace,
        "projectName": project_name,
        "build": {
            "sourceDirectory": source_directory,
            "buildDirectory": build_directory,
            "buildType": require_string(build_spec.get("buildType"), "build.buildType"),
            "generator": require_string(build_spec.get("generator"), "build.generator"),
            "artifactName": artifact_name,
            "toolchainFile": toolchain_file,
            "armToolchainBinDir": arm_toolchain_bin_dir,
            "armGdbPath": arm_gdb_path,
            "exportCompileCommands": export_compile_commands,
            "clangdCompileCommandsDir": clangd_compile_commands_dir,
            "elfPath": elf_path,
            "hexPath": hex_path,
            "extraConfigureArgs": extra_configure_args,
            "cmakePath": cmake_path,
            "ninjaPath": ninja_path,
        },
        "jlink": {
            "device": require_string(jlink_spec.get("device"), "jlink.device"),
            "interface": require_string(jlink_spec.get("interface"), "jlink.interface"),
            "speed": as_string(jlink_spec.get("speed"), "jlink.speed"),
            "jlinkExe": require_string(jlink_spec.get("jlinkExe"), "jlink.jlinkExe"),
            "jlinkGdbServer": require_string(
                jlink_spec.get("jlinkGdbServer"),
                "jlink.jlinkGdbServer",
            ),
        },
        "ports": {
            "debug": {
                "gdb": as_string(debug_ports.get("gdb"), "ports.debug.gdb"),
                "swo": as_string(debug_ports.get("swo"), "ports.debug.swo"),
                "telnet": as_string(debug_ports.get("telnet"), "ports.debug.telnet"),
                "rtt": as_string(debug_ports.get("rtt"), "ports.debug.rtt"),
            },
            "rttServer": {
                "gdb": as_string(rtt_server_ports.get("gdb"), "ports.rttServer.gdb"),
                "swo": as_string(rtt_server_ports.get("swo"), "ports.rttServer.swo"),
                "telnet": as_string(rtt_server_ports.get("telnet"), "ports.rttServer.telnet"),
                "rtt": as_string(rtt_server_ports.get("rtt"), "ports.rttServer.rtt"),
            },
        },
        "debug": {
            "runToEntryPoint": require_string(
                debug_spec.get("runToEntryPoint"),
                "debug.runToEntryPoint",
            ),
            "postLaunchCommands": normalize_list(
                debug_spec.get("postLaunchCommands"),
                "debug.postLaunchCommands",
            ),
            "rttEnabled": bool(debug_spec.get("rttEnabled", True)),
        },
        "extensions": extensions,
    }

    return runtime


def build_extensions_json(runtime: Dict[str, Any]) -> Dict[str, Any]:
    return {"recommendations": runtime["extensions"]}


def build_tasks_json(runtime: Dict[str, Any]) -> Dict[str, Any]:
    build = runtime["build"]

    build_directory = to_vscode_path(build["buildDirectory"])
    source_directory = to_vscode_path(build["sourceDirectory"])
    toolchain_file = to_vscode_path(build["toolchainFile"])
    hex_path = to_vscode_path(build["hexPath"])

    shared_presentation = {
        "reveal": "always",
        "panel": "shared",
        "clear": True,
    }

    return {
        "version": "2.0.0",
        "tasks": [
            {
                "label": "Firmware: Configure",
                "type": "shell",
                "command": "${workspaceFolder}/.vscode/scripts/cmake_configure.sh",
                "args": [
                    source_directory,
                    build_directory,
                    build["generator"],
                    toolchain_file,
                    "${config:cpr.tools.armToolchainBinDir}",
                    build["buildType"],
                    "ON" if build["exportCompileCommands"] else "OFF",
                    "${config:cpr.tools.cmakePath}",
                    "${config:cpr.tools.ninjaPath}",
                    *build["extraConfigureArgs"],
                ],
                "problemMatcher": [],
                "presentation": shared_presentation,
                "options": {"cwd": "${workspaceFolder}"},
            },
            {
                "label": "Firmware: Build",
                "type": "shell",
                "command": "${config:cpr.tools.cmakePath}",
                "args": ["--build", build_directory, "--parallel"],
                "dependsOn": "Firmware: Configure",
                "dependsOrder": "sequence",
                "group": {"kind": "build", "isDefault": True},
                "problemMatcher": [],
                "presentation": shared_presentation,
                "options": {"cwd": "${workspaceFolder}"},
            },
            {
                "label": "Firmware: Rebuild",
                "type": "shell",
                "command": "${config:cpr.tools.cmakePath}",
                "args": ["--build", build_directory, "--clean-first", "--parallel"],
                "dependsOn": "Firmware: Configure",
                "dependsOrder": "sequence",
                "problemMatcher": [],
                "presentation": shared_presentation,
                "options": {"cwd": "${workspaceFolder}"},
            },
            {
                "label": "Firmware: Clean",
                "type": "shell",
                "command": "${config:cpr.tools.cmakePath}",
                "args": ["--build", build_directory, "--target", "clean"],
                "problemMatcher": [],
                "presentation": shared_presentation,
                "options": {"cwd": "${workspaceFolder}"},
            },
            {
                "label": "J-Link: Flash",
                "type": "shell",
                "command": "${workspaceFolder}/.vscode/scripts/jlink_flash.sh",
                "args": [
                    "${config:cpr.target.device}",
                    "${config:cpr.target.interface}",
                    "${config:cpr.target.speed}",
                    hex_path,
                    "${config:cpr.tools.jlinkExe}",
                ],
                "dependsOn": "Firmware: Build",
                "dependsOrder": "sequence",
                "problemMatcher": [],
                "presentation": shared_presentation,
                "options": {"cwd": "${workspaceFolder}"},
            },
            {
                "label": "J-Link: Reset",
                "type": "shell",
                "command": "${workspaceFolder}/.vscode/scripts/jlink_reset.sh",
                "args": [
                    "${config:cpr.target.device}",
                    "${config:cpr.target.interface}",
                    "${config:cpr.target.speed}",
                    "${config:cpr.tools.jlinkExe}",
                ],
                "problemMatcher": [],
                "presentation": shared_presentation,
                "options": {"cwd": "${workspaceFolder}"},
            },
            {
                "label": "RTT: Console",
                "type": "shell",
                "command": "${workspaceFolder}/.vscode/scripts/jlink_rtt_console.sh",
                "args": [
                    "${config:cpr.target.device}",
                    "${config:cpr.target.interface}",
                    "${config:cpr.target.speed}",
                    "${config:cpr.rttServer.gdbPort}",
                    "${config:cpr.rttServer.swoPort}",
                    "${config:cpr.rttServer.telnetPort}",
                    "${config:cpr.rttServer.rttTelnetPort}",
                    "${config:cpr.tools.jlinkGdbServer}",
                ],
                "problemMatcher": [],
                "presentation": {
                    "echo": True,
                    "reveal": "always",
                    "focus": True,
                    "panel": "dedicated",
                    "clear": False,
                    "showReuseMessage": False,
                },
                "options": {"cwd": "${workspaceFolder}"},
            },
        ],
    }


def build_launch_json(runtime: Dict[str, Any]) -> Dict[str, Any]:
    build = runtime["build"]
    debug = runtime["debug"]

    configuration: Dict[str, Any] = {
        "name": f"J-Link Debug ({runtime['projectName']})",
        "type": "cortex-debug",
        "request": "launch",
        "cwd": "${workspaceFolder}",
        "servertype": "jlink",
        "serverpath": "${config:cpr.tools.jlinkGdbServer}",
        "device": "${config:cpr.target.device}",
        "interface": "${config:cpr.target.interface}",
        "executable": to_vscode_path(build["elfPath"]),
        "gdbPath": "${config:cpr.tools.armGdbPath}",
        "armToolchainPath": "${config:cpr.tools.armToolchainBinDir}",
        "runToEntryPoint": debug["runToEntryPoint"],
        "preLaunchTask": "Firmware: Build",
        "showDevDebugOutput": "raw",
        "serverArgs": [
            "-speed",
            "${config:cpr.target.speed}",
        ],
        "postLaunchCommands": debug["postLaunchCommands"],
    }

    if debug["rttEnabled"]:
        configuration["rttConfig"] = {
            "enabled": True,
            "address": "auto",
            "clearSearch": False,
            "decoders": [
                {
                    "label": "RTT",
                    "port": 0,
                    "encoding": "utf8",
                }
            ],
        }

    return {"version": "0.2.0", "configurations": [configuration]}


def build_settings_json(runtime: Dict[str, Any]) -> Dict[str, Any]:
    build = runtime["build"]
    jlink = runtime["jlink"]
    ports = runtime["ports"]
    project_name = runtime["projectName"]

    build_directory = to_vscode_path(build["buildDirectory"])
    source_directory = to_vscode_path(build["sourceDirectory"])
    clangd_dir = to_vscode_path(build["clangdCompileCommandsDir"])

    commands = [
        ("Configure", "gear", "Firmware: Configure", 400, "Configure the CMake build directory."),
        ("Build", "tools", "Firmware: Build", 399, "Build the firmware."),
        ("Clean", "trash", "Firmware: Clean", 398, "Clean the current firmware build output."),
        ("Rebuild", "refresh", "Firmware: Rebuild", 397, "Clean and rebuild the firmware."),
        ("Debug", "debug", None, 396, "Start the Cortex-Debug J-Link session."),
        ("Flash", "arrow-up", "J-Link: Flash", 395, "Program the latest hex file through J-Link."),
        ("Reset", "debug-restart", "J-Link: Reset", 394, "Reset and run the target through J-Link."),
        ("RTT", "radio-tower", "RTT: Console", 393, "Open one interactive RTT terminal for both output and input."),
        ("Stop", "stop-circle", None, 392, "Stop a running task such as RTT or a helper server."),
    ]

    workspace_commands: Dict[str, Any] = {}
    action_buttons: List[Dict[str, Any]] = []
    for label, icon, task_name, priority, tooltip in commands:
        command_id = f"{project_name} {label}"
        if label == "Debug":
            workspace_commands[command_id] = {
                "command": "workbench.action.debug.start",
                "icon": icon,
                "statusBar": {
                    "alignment": "left",
                    "text": f" {label}",
                    "name": command_id,
                    "priority": priority,
                    "tooltip": tooltip,
                },
            }
            action_buttons.append(
                {
                    "name": f"$({icon}) {label}",
                    "tooltip": tooltip,
                    "useVsCodeApi": True,
                    "command": "workbench.action.debug.start",
                }
            )
            continue

        if label == "Stop":
            workspace_commands[command_id] = {
                "command": "workbench.action.tasks.terminate",
                "icon": icon,
                "statusBar": {
                    "alignment": "left",
                    "text": f" {label}",
                    "name": command_id,
                    "priority": priority,
                    "tooltip": tooltip,
                },
            }
            action_buttons.append(
                {
                    "name": f"$({icon}) Stop Task",
                    "tooltip": tooltip,
                    "useVsCodeApi": True,
                    "command": "workbench.action.tasks.terminate",
                }
            )
            continue

        workspace_commands[command_id] = {
            "command": "workbench.action.tasks.runTask",
            "args": task_name,
            "icon": icon,
            "statusBar": {
                "alignment": "left",
                "text": f" {label}",
                "name": command_id,
                "priority": priority,
                "tooltip": tooltip,
            },
        }
        action_buttons.append(
            {
                "name": f"$({icon}) {label}",
                "tooltip": tooltip,
                "useVsCodeApi": True,
                "command": "workbench.action.tasks.runTask",
                "args": [task_name],
            }
        )

    return {
        "cmake.generator": build["generator"],
        "cmake.cmakePath": build["cmakePath"],
        "cmake.configureOnOpen": True,
        "cmake.buildDirectory": build_directory,
        "cmake.sourceDirectory": source_directory,
        "cmake.preferredGenerators": [build["generator"]],
        "cmake.configureSettings": {
            "CMAKE_TOOLCHAIN_FILE": to_vscode_path(build["toolchainFile"]),
            "CMAKE_MAKE_PROGRAM": build["ninjaPath"],
            "ARM_GNU_TOOLCHAIN_BIN_DIR": build["armToolchainBinDir"],
            "CMAKE_BUILD_TYPE": build["buildType"],
            "CMAKE_EXPORT_COMPILE_COMMANDS": "ON" if build["exportCompileCommands"] else "OFF",
        },
        "clangd.arguments": [f"--compile-commands-dir={clangd_dir}"],
        "files.associations": {"*.ld": "ld"},
        "cpr.tools.armToolchainBinDir": build["armToolchainBinDir"],
        "cpr.tools.armGdbPath": build["armGdbPath"],
        "cpr.tools.cmakePath": build["cmakePath"],
        "cpr.tools.ninjaPath": build["ninjaPath"],
        "cpr.tools.jlinkExe": jlink["jlinkExe"],
        "cpr.tools.jlinkGdbServer": jlink["jlinkGdbServer"],
        "cpr.target.device": jlink["device"],
        "cpr.target.interface": jlink["interface"],
        "cpr.target.speed": jlink["speed"],
        "cpr.target.gdbPort": ports["debug"]["gdb"],
        "cpr.target.swoPort": ports["debug"]["swo"],
        "cpr.target.telnetPort": ports["debug"]["telnet"],
        "cpr.target.rttTelnetPort": ports["debug"]["rtt"],
        "cpr.rttServer.gdbPort": ports["rttServer"]["gdb"],
        "cpr.rttServer.swoPort": ports["rttServer"]["swo"],
        "cpr.rttServer.telnetPort": ports["rttServer"]["telnet"],
        "cpr.rttServer.rttTelnetPort": ports["rttServer"]["rtt"],
        "commands.workspaceCommands": workspace_commands,
        "actionButtons": {
            "reloadButton": None,
            "defaultColor": "none",
            "loadNpmCommands": False,
            "commands": action_buttons,
        },
    }


def write_json(path: Path, payload: Dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def write_scripts(root: Path) -> None:
    for relative_path, content in SCRIPT_CONTENTS.items():
        file_path = root / relative_path
        file_path.parent.mkdir(parents=True, exist_ok=True)
        file_path.write_text(content, encoding="utf-8")
        current_mode = file_path.stat().st_mode
        file_path.chmod(current_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)


def deploy(runtime: Dict[str, Any]) -> List[Path]:
    target_workspace = runtime["targetWorkspace"]
    target_workspace.mkdir(parents=True, exist_ok=True)

    written_files = [
        target_workspace / ".vscode/extensions.json",
        target_workspace / ".vscode/settings.json",
        target_workspace / ".vscode/tasks.json",
        target_workspace / ".vscode/launch.json",
    ]

    write_json(target_workspace / ".vscode/extensions.json", build_extensions_json(runtime))
    write_json(target_workspace / ".vscode/settings.json", build_settings_json(runtime))
    write_json(target_workspace / ".vscode/tasks.json", build_tasks_json(runtime))
    write_json(target_workspace / ".vscode/launch.json", build_launch_json(runtime))
    write_scripts(target_workspace)

    for relative_path in SCRIPT_CONTENTS:
        written_files.append(target_workspace / relative_path)

    return written_files


def main() -> int:
    args = parse_args()

    try:
        spec = load_spec(Path(args.config).resolve())
        runtime = build_runtime_spec(spec, args.target)
        written_files = deploy(runtime)
    except DeployError as exc:
        print(f"deploy failed: {exc}", file=sys.stderr)
        return 1

    print(f"Deployed VS Code bundle to: {runtime['targetWorkspace']}")
    for path in written_files:
        print(f"  - {path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
