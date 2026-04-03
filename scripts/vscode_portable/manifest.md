# VS Code Portable Template Manifest

This file is the final copyable template checklist for `USER/Rep/scripts/vscode_portable`.

Use this file as the single source of truth when you copy this template into another project.

## Final Template Directory

```text
vscode_portable/
  .vscode/
    extensions.json
    settings.json
    tasks.json
  scripts/
    vscode/
      jlink.ps1
      keil-build.ps1
      serial-monitor.ps1
      workspace.config.psd1
  manifest.md
  migration.md
  readme.md
```

## Copy Boundary

Only copy these two directories into the target project root:

1. `.vscode`
2. `scripts`

Do not copy `manifest.md`, `migration.md`, or `readme.md` into the target project unless you also want to keep the documentation with that project.

## File Inventory

### `.vscode/extensions.json`

- Purpose: recommends the required VS Code extensions
- Current recommendations:
  - `ms-vscode.cpptools`
  - `marus25.cortex-debug`
  - `usernamehw.commands`

### `.vscode/settings.json`

- Purpose: exposes bottom status bar buttons through `usernamehw.commands`
- Buttons included:
  - `Build`
  - `Rebuild`
  - `Flash`
  - `Reset`
  - `RTT`
  - `Serial`
- RTT behavior: the `RTT` button starts `J-Link: RTT Server` first, then `J-Link: RTT Terminal`

### `.vscode/tasks.json`

- Purpose: defines the runnable VS Code tasks
- Tasks included:
  - `Keil: Build`
  - `Keil: Rebuild`
  - `J-Link: Flash`
  - `J-Link: Reset`
  - `J-Link: RTT Server`
  - `J-Link: RTT Terminal`
  - `J-Link: RTT Open`
  - `Serial: Monitor`

### `scripts/vscode/keil-build.ps1`

- Purpose: runs Keil build or rebuild from VS Code
- Copy status: required

### `scripts/vscode/jlink.ps1`

- Purpose: flash, reset, RTT server, and RTT terminal entry point
- Important behavior:
  - starts RTT server with `-nohalt`
  - keeps RTT telnet on `19021`
  - auto-resolves free GDB, SWO, and Telnet control ports when configured ports are unavailable

### `scripts/vscode/serial-monitor.ps1`

- Purpose: serial monitor with keyboard input forwarding
- Copy status: required

### `scripts/vscode/workspace.config.psd1`

- Purpose: the only required per-project edit point
- Default values currently included:
  - `ProjectPath = 'Project/MDK-ARM/mcos-gd32.uvprojx'`
  - `KeilTarget = 'MCOS-GD32'`
  - `HexFilePath = 'Project/MDK-ARM/GD32F4xx-OBJ/mcos-gd32.hex'`
  - `JLink.Device = 'GD32F407VG'`
  - `JLink.Interface = 'SWD'`
  - `JLink.SpeedKHz = 10000`
  - `JLink.GdbPort = 3331`
  - `JLink.SwoPort = 3332`
  - `JLink.TelnetPort = 3333`
  - `JLink.RttTelnetPort = 19021`
  - `Serial.BaudRate = 115200`

## Required Edits After Copy

After copying into another project, edit only `scripts/vscode/workspace.config.psd1` first.

You must verify or change:

1. `ProjectPath`
2. `KeilTarget`
3. `HexFilePath`
4. `JLink.Device`
5. `JLink.Interface` if the target is not SWD
6. `JLink.SpeedKHz` if the board needs a lower speed
7. `Serial` settings if the UART configuration differs

## Expected Target Layout After Copy

```text
<target-project>/
  .vscode/
    extensions.json
    settings.json
    tasks.json
  scripts/
    vscode/
      jlink.ps1
      keil-build.ps1
      serial-monitor.ps1
      workspace.config.psd1
```

## First Validation Order

1. Install the recommended VS Code extensions.
2. Run `Keil: Build`.
3. Run `J-Link: Flash`.
4. Run `J-Link: RTT Server`.
5. Run `J-Link: RTT Terminal`.
6. Run `Serial: Monitor` if UART access is needed.

## Known Stable Defaults

1. Use `3331`, `3332`, and `3333` for J-Link control ports on Windows instead of `2331`, `2332`, and `2333`.
2. Keep RTT telnet at `19021` because `JLinkRTTClient.exe` expects that default local port.
3. Keep `-nohalt` enabled for RTT server startup so firmware log and console tasks keep running.

## Template Acceptance Checklist

Mark this template as ready only when all items below are true:

1. `.vscode/extensions.json` exists.
2. `.vscode/settings.json` exists.
3. `.vscode/tasks.json` exists.
4. `scripts/vscode/keil-build.ps1` exists.
5. `scripts/vscode/jlink.ps1` exists.
6. `scripts/vscode/serial-monitor.ps1` exists.
7. `scripts/vscode/workspace.config.psd1` exists.
8. The bottom status bar buttons show `Build`, `Rebuild`, `Flash`, `Reset`, `RTT`, and `Serial` after installing `usernamehw.commands`.
9. RTT server starts without halting the target.
10. RTT terminal can read output and send input.

## Document Roles

- `manifest.md`: final template inventory and copy checklist
- `readme.md`: quick start for fast reuse
- `migration.md`: longer procedure and troubleshooting notes