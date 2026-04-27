#!/usr/bin/env python3

from __future__ import annotations

import argparse
import json
import shutil
import stat
from pathlib import Path


PROJECT = {
    "name": "CprSensor",
    "device": "STM32F103RE",
    "interface": "SWD",
    "speed": "4000",
    "keil_project": "MDK-ARM/CprSensor.uvprojx",
    "keil_target": "CprSensor",
    "keil_hex": "MDK-ARM/CprSensor/CprSensor.hex",
    "cmake_build_dir": "build/Debug",
    "cmake_hex": "build/Debug/CprSensor.hex",
    "toolchain_bin": "${workspaceFolder}/.tools/arm-gnu-toolchain/bin",
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Deploy unified Windows/macOS VS Code MCU tasks.")
    parser.add_argument("--target", default=None, help="Workspace root. Defaults to the repo root.")
    return parser.parse_args()


def repo_root_from_script() -> Path:
    return Path(__file__).resolve().parents[3]


def write_json(path: Path, payload: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def make_executable(path: Path) -> None:
    path.chmod(path.stat().st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)


def copy_windows_scripts(source_root: Path, target: Path) -> None:
    source = source_root / "rep/scripts/vscode_portable_windows/scripts/vscode"
    dest = target / "scripts/vscode"
    dest.mkdir(parents=True, exist_ok=True)
    for name in ["keil-build.ps1", "jlink.ps1", "serial-monitor.ps1"]:
        shutil.copy2(source / name, dest / name)

    config = f"""@{{
    ProjectPath = '{PROJECT["keil_project"]}'
    KeilTarget = '{PROJECT["keil_target"]}'
    HexFilePath = '{PROJECT["keil_hex"]}'

    JLink = @{{
        Device = '{PROJECT["device"]}'
        Interface = '{PROJECT["interface"]}'
        SpeedKHz = {PROJECT["speed"]}
        GdbPort = 3331
        SwoPort = 3332
        TelnetPort = 3333
        RttTelnetPort = 19021
    }}

    Serial = @{{
        BaudRate = 115200
        DataBits = 8
        Parity = 'None'
        StopBits = 'One'
        DtrEnable = $false
        RtsEnable = $false
    }}
}}
"""
    (dest / "workspace.config.psd1").write_text(config, encoding="utf-8")


def copy_mac_scripts(source_root: Path, target: Path) -> None:
    source = source_root / ".vscode/scripts"
    dest = target / ".vscode/scripts"
    dest.mkdir(parents=True, exist_ok=True)
    for path in source.glob("*.sh"):
        target_path = dest / path.name
        if path.resolve() != target_path.resolve():
            shutil.copy2(path, target_path)
        make_executable(target_path)


def deploy_templates(source_root: Path, target: Path) -> None:
    for name in ["tasks.json", "settings.json", "launch.json", "extensions.json"]:
        source = source_root / ".vscode" / name
        dest = target / ".vscode" / name
        if source.resolve() != dest.resolve():
            shutil.copy2(source, dest)


def main() -> int:
    args = parse_args()
    source_root = repo_root_from_script()
    target = Path(args.target).expanduser().resolve() if args.target else source_root

    copy_windows_scripts(source_root, target)
    copy_mac_scripts(source_root, target)
    deploy_templates(source_root, target)

    print(f"Deployed unified VS Code MCU environment to: {target}")
    print("Buttons are shared; VS Code selects Keil on Windows and CMake on macOS.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
