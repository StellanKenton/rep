# VS Code Portable Migration Guide

This directory is the copy source for Keil build, J-Link flash, RTT read, RTT write, and serial monitor support.

Use this directory when you want to move the same workflow to:

- another project on this computer
- the same project on another computer
- another project on another computer

## What To Copy

Copy these two folders from `USER/Rep/scripts/vscode_portable` into the target project root:

1. `.vscode`
2. `scripts`

After copying, the target project should have this layout:

```text
<target-project>/
  .vscode/
    tasks.json
    extensions.json
    settings.json
  scripts/
    vscode/
      keil-build.ps1
      jlink.ps1
      serial-monitor.ps1
      workspace.config.psd1
```

## Minimum Project Requirements

The target project must provide:

1. a valid Keil `.uvprojx` project file
2. a build target name that exists inside that Keil project
3. a generated `.hex` file path after build
4. a board and chip that J-Link can connect to
5. PowerShell available in Windows

## Step By Step: Another Project On This Computer

1. Open the target project root.
2. Copy `.vscode` from `USER/Rep/scripts/vscode_portable/.vscode` into the target project root.
3. Copy `scripts` from `USER/Rep/scripts/vscode_portable/scripts` into the target project root.
4. Edit `scripts/vscode/workspace.config.psd1`.
5. Change `ProjectPath` to the target project's `.uvprojx` relative path.
6. Change `KeilTarget` to the actual Keil target name.
7. Change `HexFilePath` to the target project's generated `.hex` relative path.
8. Change `JLink.Device` to the real MCU name recognized by J-Link.
9. If needed, change `JLink.Interface`, `SpeedKHz`, and RTT/GDB ports.
10. Install the recommended VS Code extensions, especially `usernamehw.commands`, if you want the bottom status bar buttons.
11. Open the project in VS Code.
12. Run `Tasks: Run Task` and execute `Keil: Build`.
13. If build succeeds, run `J-Link: Flash`.
14. If RTT is needed, run `J-Link: RTT Open` or use the bottom `RTT` status bar button.
15. If UART console access is needed, run `Serial: Monitor` or use the bottom `Serial` button.

## Step By Step: Same Project On Another Computer

1. Copy the whole repository to the new computer.
2. Install Keil MDK.
3. Install SEGGER J-Link software.
4. Open the repository in VS Code.
5. If Keil or J-Link are installed in nonstandard locations, set environment variables:
   - `KEIL_UV4`
   - `JLINK_PATH`
6. Install the recommended VS Code extensions, especially `usernamehw.commands`, if you want the bottom status bar buttons.
7. Run `Keil: Build`.
8. Run `J-Link: Flash`.
9. Run `J-Link: RTT Open` if RTT interaction is needed.
10. Run `Serial: Monitor` if UART interaction is needed.

## Step By Step: Another Project On Another Computer

1. On the source side, copy `USER/Rep/scripts/vscode_portable/.vscode` and `USER/Rep/scripts/vscode_portable/scripts`.
2. On the target computer, place them into the target project root.
3. Install Keil MDK and SEGGER J-Link.
4. Edit `scripts/vscode/workspace.config.psd1` for the new project.
5. Verify the Keil project builds manually once if the project is new to that machine.
6. Install the recommended VS Code extensions, especially `usernamehw.commands`, if you want the bottom status bar buttons.
7. Open the target project in VS Code.
8. Run `Keil: Build`.
9. Run `J-Link: Flash`.
10. Run `J-Link: RTT Open` if RTT is needed.
11. Run `Serial: Monitor` if UART interaction is needed.

## How To Edit workspace.config.psd1

Example:

```powershell
@{
    ProjectPath = 'Project/MDK-ARM/demo.uvprojx'
    KeilTarget = 'DEMO-BOARD'
    HexFilePath = 'Project/MDK-ARM/Objects/demo.hex'

    JLink = @{
        Device = 'STM32F407VG'
        Interface = 'SWD'
        SpeedKHz = 4000
      GdbPort = 3331
      SwoPort = 3332
      TelnetPort = 3333
        RttTelnetPort = 19021
    }

    Serial = @{
      BaudRate = 115200
      DataBits = 8
      Parity = 'None'
      StopBits = 'One'
      DtrEnable = $false
      RtsEnable = $false
    }
}
```

## Optional Environment Variables

Use these only when the default auto-discovery is not enough:

- `KEIL_UV4`: full path to `UV4.exe`
- `JLINK_PATH`: J-Link installation directory
- `JLINK_SERIAL`: choose one probe when multiple J-Link probes are connected
- `JLINK_SPEED_KHZ`: override configured speed
- `JLINK_INTERFACE`: override configured interface
- `JLINK_DEVICE`: override configured device
- `SERIAL_PORT`: choose the COM port dynamically at runtime
- `SERIAL_BAUD`: override configured baud rate

## Typical Problems

### Build Works In Keil But Fails In VS Code

1. Check that `ProjectPath` is correct.
2. Check that `KeilTarget` exactly matches the Keil target name.
3. Check whether `UV4.exe` is installed or `KEIL_UV4` is set.

### Flash Fails

1. Check whether the hex file is really generated at `HexFilePath`.
2. Check whether the device name matches a J-Link supported device name.
3. Check probe connection, power, and SWD interface wiring.

### RTT Server Fails To Start

1. Check whether port `19021` is already in use.
2. Avoid `2331` to `2333` on Windows machines where Hyper-V or the OS excludes that range.
3. Use ports like `3331` to `3333` in `workspace.config.psd1` if the copied project inherits a reserved range problem.
4. Stop the old RTT server process.
5. Start `J-Link: RTT Server` again.

### RTT Terminal Opens But No Command Response

1. Confirm the firmware actually initializes RTT input and output.
2. Confirm the target is running and not halted by the debug server. The portable `jlink.ps1` starts RTT server with `-nohalt` for this reason.
3. Confirm the firmware has a task or loop that consumes RTT input.

## Bottom Status Bar Shortcuts

If the `Commands` extension (`usernamehw.commands`) is installed, the copied workspace also shows these buttons in the VS Code bottom status bar:

- `Build` runs `Keil: Build`
- `Rebuild` runs `Keil: Rebuild`
- `Flash` runs `J-Link: Flash`
- `Reset` runs `J-Link: Reset`
- `RTT` starts `J-Link: RTT Server` and then opens `J-Link: RTT Terminal`
- `Serial` runs `Serial: Monitor`

The buttons are configured by `.vscode/settings.json` and call the copied tasks directly.

### Serial Monitor Cannot Decide Which Port To Open

1. Set `SERIAL_PORT` before launching `Serial: Monitor`.
2. Or add `Serial.PortName` to `scripts/vscode/workspace.config.psd1` for that copied project.
3. Or enter the COM port in the `Serial: Monitor` task prompt.
4. If only one serial port is connected, the script will select it automatically.

## Why This Layout Is Recommended

This is the smallest stable migration unit because:

1. VS Code tasks still need `.vscode/tasks.json`.
2. Real logic stays in `scripts/vscode`, so later maintenance is centralized.
3. Project-specific values are isolated in one file.
4. Cross-project migration only needs one config change point.