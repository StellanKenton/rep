---
doc_role: module-spec
layer: module
module: fc41d
status: active
portability: layer-dependent
public_headers:
  - fc41d_base.h
  - fc41d.h
  - fc41d_ble.h
  - fc41d_wifi.h
core_files:
  - fc41d.c
  - fc41d_at.c
  - fc41d_ble.c
  - fc41d_wifi.c
  - fc41d_mode.c
  - fc41d_priv.h
port_files:
  - fc41d_assembly.h
debug_files: []
depends_on:
  - ../module.md
  - ../../comm/flowparser/flowparser.md
  - ../../driver/drvuart/drvuart.md
forbidden_depends_on:
  - core 直连 BSP 或 UART 私有实现
required_hooks:
  - fc41dLoadPlatformDefaultCfg
  - fc41dPlatformIsValidAssemble
  - fc41dPlatformGetTickMs
  - fc41dPlatformInitTransport
  - fc41dPlatformPollRx
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
  - fc41d_ble.h
  - fc41d_wifi.c
  - fc41d_wifi.h
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
| `fc41d_base.h` | 核心公共类型、非阻塞 AT 事务接口 |
| `fc41d.h/.c` | 模块总入口、上下文管理、AT/URC 总控、`fc41dProcess()` 调度 |
| `fc41d_at.c` | FC41D 通用 AT 命令目录、默认响应规则、通用文本构造接口 |
| `fc41d_ble.h/.c` | BLE AT 便捷构造接口、BLE 接收接口与 BLE 状态机 |
| `fc41d_wifi.h/.c` | WiFi 接收接口与 WiFi 状态机 |
| `fc41d_mode.c` | 软件态 mode 标记接口 |
| `fc41d_priv.h` | 目录内部上下文、私有 helper 与子状态机声明 |
| `fc41d_assembly.h` | core 对平台装配层的 hook 契约 |

说明：当前目录没有 `ble/`、`wifi/` 子目录，也没有 `fc41d_port.*`。平台绑定通过 `fc41d_assembly.h` 声明的 hook 注入，`fc41d.c` 中提供 weak 默认实现兜底。

## 3. 对外公共接口

- `fc41d.h` 作为 umbrella 头，仅负责聚合 `fc41d_base.h`、`fc41d_ble.h`、`fc41d_wifi.h`。
- `fc41d_base.h` 承载公共类型与核心 API，子头直接依赖 `fc41d_base.h`，避免循环包含。
- `stFc41dCfg` 与 `stFc41dInfo` 按链路拆成 `ble` / `wifi` 子结构，模块级公共字段保留在顶层。
- `fc41dExecAt*()` 改为非阻塞提交接口，只负责把事务提交给 flowparser；完成由周期 `fc41dProcess()` 驱动。
- `fc41dProcess()` 先统一处理 AT/URC flowparser，再调用 BLE/WiFi 状态机。
- 稳定接口包括：
  - `fc41dGetDefCfg/GetCfg/SetCfg/Init/IsReady/Process/GetInfo`
  - `fc41dExecAt/fc41dExecAtCmd/fc41dExecAtText/fc41dAtCheckAlive`
  - `fc41dExecAtIsBusy/fc41dGetLastExecResult/fc41dRecover`
  - `fc41dAtGetCmdInfo*` 与 `fc41dAtBuild*`
  - `fc41dBle*` 与 `fc41dWifi*` ring buffer 访问接口
  - `fc41dSetModeState/fc41dGetModeState`

## 4. 当前状态机约定

- BLE 按 `workMode` 区分 `disabled / peripheral / central`，状态覆盖初始化、广播/扫描启动、等待连接、连接、断联和错误。
- WiFi 按 `workMode` 区分 `disabled / sta / ap`，状态覆盖初始化、连接中、已连接、断联、AP 启动、AP 活跃和错误。
- 默认配置下两套状态机都不自动启动；调用方需要在 `cfg.ble` / `cfg.wifi` 里设置 `workMode`，并按需要覆盖 `initCmdText/startCmdText/stopCmdText`。
- BLE 在未覆盖命令文本时，默认外设流程使用 `AT+QBLEINIT=2` + `AT+QBLEADVSTART`，中心流程使用 `AT+QBLEINIT=1` + `AT+QBLESCAN=1`。
- WiFi 不提供默认 init/start 命令，原因是 STA/AP 启动参数通常依赖项目侧凭证与装配配置；仅 stop 命令提供默认值：STA 用 `AT+QSTASTOP`，AP 用 `AT+QSOFTAPSTOP`。
- BLE/WiFi 状态机里的 AT 步骤通过 `submit + process` 非阻塞推进；若进入 `ERROR` 状态，可调用 `fc41dRecover()` 复位内部事务与子状态机。

## 5. assembly / platform hook 契约

- `fc41dLoadPlatformDefaultCfg`：提供项目默认配置。
- `fc41dPlatformIsValidAssemble`：检查当前设备映射是否完成装配。
- `fc41dPlatformGetTickMs`：提供单调 tick，供阻塞式 AT 执行超时判断使用。
- `fc41dPlatformInitTransport`：初始化底层 transport。
- `fc41dPlatformPollRx`：把 transport 上的新字节灌入 flowparser 对应输入流。
- `fc41dPlatformInitRxBuffers`：提供 BLE/WiFi 接收 ring buffer 的实际存储。
- `fc41dPlatformInitAtStream`：完成 flowparser stream 初始化与 send/path 绑定。
- `fc41dPlatformRouteLine`：把 URC 行解析为 BLE/WiFi 通道和 payload。

core 不直接包含 BSP 或 UART 私有头，只依赖这些 hook 与 `flowparser`、`ringbuffer` 公共能力。

## 6. 改动落点矩阵

| 需求 | 应改文件 | 不该改的文件 |
| --- | --- | --- |
| 改模块上下文、AT 总控、阻塞执行语义 | `fc41d.c` / `fc41d_priv.h` | 平台 UART 绑定实现 |
| 增删通用 AT 目录与命令文本构造 | `fc41d_at.c` / `fc41d.h` | BLE/WiFi 状态机 |
| 改 BLE 默认命令或连接状态迁移 | `fc41d_ble.c` / `fc41d_ble.h` | WiFi 状态机 |
| 改 WiFi 默认命令或连接状态迁移 | `fc41d_wifi.c` / `fc41d_wifi.h` | BLE 状态机 |
| 改 transport、tick、URC 路由、默认 buffer | 当前工程的 hook 实现文件 | `fc41d.c` core 逻辑 |

## 7. 复制到其他工程的最小步骤

带走 `fc41d` 目录全部核心文件，并在目标工程实现 `fc41d_assembly.h` 中声明的 hook。若链路格式不同，重点重写 transport 初始化、AT stream 绑定、tick 获取和 URC 路由逻辑。