---
doc_role: module-spec
layer: module
module: fc41d
status: active
portability: layer-dependent
public_headers:
  - fc41d.h
core_files:
  - fc41d.c
  - fc41d_at.c
  - fc41d_ble.c
  - fc41d_wifi.c
  - fc41d_mode.c
port_files:
  - fc41d_port.c
  - fc41d_port.h
  - fc41d_assembly.h
debug_files: []
depends_on:
  - ../module.md
  - ../../comm/flowparser/flowparser.md
  - ../../drvlayer/drvuart/drvuart.md
forbidden_depends_on:
  - core 直连 BSP 或 UART 私有实现
required_hooks:
  - fc41dPlatformInitTransport
  - fc41dPlatformInitRxBuffers
  - fc41dPlatformInitAtStream
  - fc41dPlatformRouteLine
optional_hooks: []
common_utils:
  - tools/ringbuffer
copy_minimal_set:
  - fc41d.h
  - fc41d.c
  - fc41d_at.c
  - fc41d_ble.c
  - fc41d_wifi.c
  - fc41d_mode.c
  - fc41d_assembly.h
read_next:
  - ../module.md
  - ../../comm/flowparser/flowparser.md
---

# FC41D 模块说明

这是当前目录的权威入口文档。

## 1. 模块定位

`fc41d` 负责两类能力：

- 通过单 UART 上的 AT 事务流对 FC41D 做配置。
- 将 FC41D 上报的 BLE / WiFi 上行数据分别写入各自 ring buffer，供其他模块读取。

## 2. 文件分工

| 文件 | 职责 |
| --- | --- |
| `fc41d.h/.c` | 核心状态、初始化、处理循环、通用 AT 执行接口 |
| `fc41d_at.c` | FC41D AT 命令表、默认响应规则、文本命令辅助接口 |
| `fc41d_ble.c` | BLE 接收 ring buffer 访问接口 |
| `fc41d_wifi.c` | WiFi 接收 ring buffer 访问接口 |
| `fc41d_mode.c` | 软件态 mode 标记接口 |
| `fc41d_assembly.h` | core 与 port 的 hook 契约 |
| `fc41d_port.h/.c` | UART / flowparser / 默认缓存绑定 |

## 3. 对外公共接口

- `fc41dGetDefCfg/GetCfg/SetCfg/Init/IsReady/Process/GetInfo`
- `fc41dExecAt/fc41dExecAtText/fc41dAtCheckAlive`
- `fc41dBle*` 与 `fc41dWifi*` ring buffer 访问接口
- `fc41dSetModeState/fc41dGetModeState`

当前目录内现在同时维护两层 FC41D 命令接口：

- 兼容层：保留少量固定文本命令和 BLE 便捷构造函数，继续服务当前工程现有初始化流程。
- 完整目录层：`fc41d_at.c` 内新增完整 `eFc41dAtCatalogCmd` 命令目录，覆盖官方手册中的全部 Wi-Fi、BLE、TCP/UDP、SSL、MQTT、HTTP(S) AT 指令。

详细命令解释见 `fc41d_at_commands.md`。

## 4. assembly / port 契约

- UART 初始化和发送能力由 `fc41dPlatformInitTransport` 与 `fc41dPlatformInitAtStream` 提供。
- 数据上报分流由 `fc41dPlatformRouteLine` 提供，默认实现放在 `fc41d_port.c`。
- BLE/WiFi 接收缓存的存储区由 port 层提供，core 只持有 `stRingBuffer` 对象。

## 5. 复制到其他工程的最小步骤

带走 `fc41d` 目录全部核心文件，并重写 `fc41d_port.c` 的 UART 绑定、tick 获取和数据上报格式解析。