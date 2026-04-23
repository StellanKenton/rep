---
doc_role: module-spec
layer: module
module: esp32c5
status: active
portability: layer-dependent
public_headers:
  - esp32c5.h
core_files:
  - esp32c5.c
  - esp32c5_ctrl.h
  - esp32c5_ctrl.c
  - esp32c5_data.h
  - esp32c5_data.c
  - esp32c5_assembly.h
port_files: []
debug_files: []
depends_on:
  - ../../comm/flowparser/flowparser.md
  - ../../driver/drvuart/drvuart.h
  - ../../tools/ringbuffer/ringbuffer.md
forbidden_depends_on:
  - 在 core 中直连具体 UART BSP 或 system 私有头
required_hooks:
  - esp32c5LoadPlatformDefaultCfg
  - esp32c5GetPlatformTransportInterface
  - esp32c5GetPlatformControlInterface
optional_hooks:
  - esp32c5PlatformIsValidCfg
common_utils:
  - ../../comm/flowparser
copy_minimal_set:
  - esp32c5.h
  - esp32c5.c
  - esp32c5_assembly.h
read_next:
  - ../module.md
  - ../../comm/flowparser/flowparser.md
---

# ESP32C5 模块说明

这是当前目录的权威入口文档。

## 1. 模块定位

`esp32c5` 采用和 `fc41d` 相同的“对外公共 API + 内部控制面 + 内部数据面”组织，但控制命令切换为 ESP-AT 的 BLE 指令族。

当前边界如下：

- `flowparser` 只负责 AT 文本事务、prompt 和 done/error/timeout。
- `esp32c5.c` 负责设备实例、公共 API 和后台主循环。
- `esp32c5_ctrl.c` 负责启动状态机、URC 状态映射、MAC 查询、广告配置和通知事务提交。
- `esp32c5_data.c` 负责 BLE RX/TX ring 和通知 payload 缓存。
- transport、tick、上电控制都由项目侧 `User/port` 注入。

## 2. 目录内文件职责

| 文件 | 职责 |
| --- | --- |
| `esp32c5.h` | 对外公共 API、配置、状态、数据面 API |
| `esp32c5.c` | 对外总入口、后台主循环、设备实例管理 |
| `esp32c5_ctrl.h` | 内部控制面状态机声明 |
| `esp32c5_ctrl.c` | 启动流程、广告配置、URC 状态映射 |
| `esp32c5_data.h` | 内部数据面声明 |
| `esp32c5_data.c` | RX/TX ring 管理、通知 prompt 组包 |
| `esp32c5_assembly.h` | 项目侧 transport/tick/control 绑定契约 |
| `esp32c5.md` | 当前目录 contract |

## 3. 对外公共接口

稳定公共头文件：`esp32c5.h`

稳定 API：

- `esp32c5GetDefCfg()` / `esp32c5SetCfg()`
- `esp32c5GetDefBleCfg()` / `esp32c5SetBleCfg()`
- `esp32c5Init()` / `esp32c5Start()` / `esp32c5Stop()` / `esp32c5Process()`
- `esp32c5DisconnectBle()`
- `esp32c5GetState()` / `esp32c5IsReady()` / `esp32c5GetInfo()`
- `esp32c5GetRxLength()` / `esp32c5ReadData()` / `esp32c5WriteData()`
- `esp32c5GetCachedMac()`
- `esp32c5SetUrcHandler()` / `esp32c5SetUrcMatcher()` / `esp32c5SetRawMatcher()`

## 4. 生命周期与 ready 条件

当前控制流程按下面阶段推进：

1. 控制脚拉低一段时间后释放模块。
2. 在 ready deadline 内等待模块输出 `ready` URC，收到后进入后续配置。
3. 关闭 echo，初始化 BLE server。
4. 设置名称、广告参数、广告数据并启动广播。
5. 查询 BLE MAC。
6. 进入 `RUNNING`，连接后支持 `AT+BLEGATTSNTFY` prompt 发送。

ready 条件：

- transport 已初始化。
- 启动状态机进入 `RUNNING`。
- BLE 广播已启动。
- `AT+BLEADDR?` 已完成或允许无 MAC 降级继续。

响应长度约束：

- `flowparser_stream` 的单行上限等于配置的 `lineBufSize - 1`。
- `esp32c5` 当前默认把 `ESP32C5_STREAM_LINE_BUFFER_SIZE` 和 `ESP32C5_STREAM_RX_STORAGE_SIZE` 提升到 `512` 字节，以覆盖常见 BLE/Wi-Fi AT 长响应。
- 若后续接入 `AT+CWLAP` 一类更长结果，仍应按目标固件的最长单行响应审视该缓冲，而不是假设 `flowparser` 会自动做长行分页。

## 5. port / assembly 契约

| 名称 | 必需/可选 | 由谁实现 | 在哪里被调用 | 备注 |
| --- | --- | --- | --- | --- |
| `esp32c5LoadPlatformDefaultCfg` | 必需 | 项目侧 `User/port` | `esp32c5GetDefCfg` | 写默认 UART/linkId/control pin |
| `esp32c5GetPlatformTransportInterface` | 必需 | 项目侧 `User/port` | `esp32c5Init`、后台收发 | 必须提供 init/write/getRxLen/read/getTickMs |
| `esp32c5GetPlatformControlInterface` | 必需 | 项目侧 `User/port` | `esp32c5Init`、`esp32c5Start`、`esp32c5Process` | 当前最少要能初始化和拉高/拉低上电控制脚 |
| `esp32c5PlatformIsValidCfg` | 建议 | 项目侧 `User/port` | `esp32c5SetCfg`、`esp32c5Init` | 约束项目允许的 linkId/resetPin |

## 6. 公共函数使用契约

| 来源模块 | 公共函数 | 允许在哪些文件调用 | 用途 |
| --- | --- | --- | --- |
| `flowparser_stream` | `flowparserStreamInit/Reset/Feed/Submit/Proc` | `esp32c5.c`、`esp32c5_ctrl.c` | 文本事务、prompt 事务、URC 派发 |
| `flowparser_stream` raw hook | `flowparserStreamSetRawHook` | `esp32c5.c` | 把项目注册的 raw frame 直接写入数据面 |

## 7. 改动落点矩阵

| 需求 | 应改文件 | 不该改的文件 |
| --- | --- | --- |
| 改 ESP32-C5 启动顺序、URC 判定、广告命令 | `esp32c5_ctrl.c/.h` | 项目 manager |
| 改 payload 缓冲、prompt 发送组包 | `esp32c5_data.c/.h` | 项目 manager |
| 改默认 UART / 控制脚绑定 | 项目侧 `esp32c5_port.*` | `esp32c5.c` |
| 改业务默认 BLE 名称和 service/char index | 项目侧 manager / cfg | `esp32c5.c` |

## 8. 验证清单

- `esp32c5Init()` 后 transport ready 为真。
- `esp32c5Start()` 后持续调用 `esp32c5Process()`，状态能从 `BOOTING/CONFIGURING` 进入 `READY`。
- `+BLECONN` / `+BLEDISCONN` / `+WRITE` 这类活动消息不需要 manager 自己硬解析生命周期。
- `esp32c5WriteData()` 在连接后能触发 `AT+BLEGATTSNTFY` prompt 发送。