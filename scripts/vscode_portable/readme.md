# VS Code Portable Quick Start

This directory is the portable copy source for the VS Code workflow used in this repository.

Read `manifest.md` first if you want the final copyable template inventory and acceptance checklist.

Use it when you want to copy the same workflow to:

- another computer
- another project
- another computer and another project

## What This Package Contains

- `.vscode/tasks.json`: build, rebuild, flash, reset, RTT, and serial monitor tasks
- `.vscode/settings.json`: bottom status bar buttons for Build, Rebuild, Flash, Reset, RTT, and Serial
- `.vscode/extensions.json`: recommended VS Code extensions
- `scripts/vscode/keil-build.ps1`: Keil build entry point
- `scripts/vscode/jlink.ps1`: J-Link flash, reset, and RTT entry point
- `scripts/vscode/serial-monitor.ps1`: UART monitor with keyboard input forwarding
- `scripts/vscode/workspace.config.psd1`: project-specific values

## Copy Steps

1. Copy `.vscode` into the target project root.
2. Copy `scripts` into the target project root.
3. Open `scripts/vscode/workspace.config.psd1`.
4. Set `ProjectPath` to the target `.uvprojx` file.
5. Set `KeilTarget` to the real Keil target name.
6. Set `HexFilePath` to the generated `.hex` file path.
7. Set `JLink.Device` to the actual MCU device name recognized by J-Link.
8. Adjust `JLink.Interface`, `SpeedKHz`, or the ports if the target project needs different values.

## First Use In VS Code

1. Open the target project in VS Code.
2. Install the recommended extensions from `.vscode/extensions.json`.
3. If you want the bottom status bar buttons, install `usernamehw.commands`.
4. Run `Keil: Build` once.
5. Run `J-Link: Flash` once after a successful build.
6. Use the bottom status bar buttons or the task runner for daily work.

## Bottom Status Bar Buttons

After `usernamehw.commands` is installed and VS Code is reloaded, the bottom status bar shows:

- `Build`
- `Rebuild`
- `Flash`
- `Reset`
- `RTT`
- `Serial`

The `RTT` button starts `J-Link: RTT Server` first and then opens `J-Link: RTT Terminal`.

## Important RTT Notes

- This package uses `-nohalt` when starting the J-Link RTT server, so the firmware keeps running while RTT is attached.
- On some Windows machines, ports `2331` to `2333` are reserved by the OS or Hyper-V.
- For that reason, the default J-Link control ports in `workspace.config.psd1` are `3331`, `3332`, and `3333`.
- The script can also pick fallback ports automatically if the configured control ports are unavailable.
- The RTT terminal still uses `19021` because `JLinkRTTClient.exe` expects the default RTT telnet port.

## Serial Monitor Notes

- `Serial: Monitor` can prompt for a COM port.
- If only one serial port is connected, it can auto-select it.
- You can also set `SERIAL_PORT` and `SERIAL_BAUD` in the environment.

## When To Read migration.md

Read `migration.md` if you want the longer checklist and troubleshooting notes.

Read `manifest.md` if you want the final template directory list, copy boundary, required edit points, and acceptance checklist in one file.