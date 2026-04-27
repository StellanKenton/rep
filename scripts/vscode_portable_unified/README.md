# VS Code Portable Unified

这个目录是 Windows Keil 环境和 macOS CMake/J-Link 环境的统一入口。

同一套底部按钮在两个系统中保持一致：

- `Configure`: macOS 下配置 CMake；Windows 下通常不需要。
- `Build` / `Rebuild`: Windows 调 Keil，macOS 调 CMake。
- `Flash` / `Reset`: 两边都调 J-Link，但各自使用本机脚本。
- `RTT`: Windows 使用 J-Link RTT Server/Client，macOS 使用 J-Link GDB Server + `nc`。
- `Stop`: 停止当前 VS Code task。

## 使用

```sh
cd rep/scripts/vscode_portable_unified
./deploy.sh --target /path/to/workspace
```

如果在当前工程内运行，可以省略 `--target`：

```sh
./deploy.sh
```

## 项目差异

当前脚本内置的是 `CprSensor` 这份工程的参数：

- Keil 工程：`MDK-ARM/CprSensor.uvprojx`
- Keil target：`CprSensor`
- macOS CMake 产物：`build/Debug/CprSensor.hex`
- J-Link device：`STM32F103RE`

迁移到新项目时，优先修改 `deploy.py` 顶部的 `PROJECT` 配置。

