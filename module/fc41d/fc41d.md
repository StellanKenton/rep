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
  - fc41d_ctrl.h
  - fc41d_ctrl.c
  - fc41d_data.h
  - fc41d_data.c
  - fc41d_assembly.h
port_files: []
debug_files: []
depends_on:
  - ../../comm/flowparser/flowparser.md
  - ../../driver/drvuart/drvuart.h
  - ../../tools/ringbuffer/ringbuffer.md
forbidden_depends_on:
  - 在 core 中直连具体 UART BSP 或 system 私有头
required_hooks:
  - fc41dLoadPlatformDefaultCfg
  - fc41dGetPlatformTransportInterface
  - fc41dGetPlatformControlInterface
optional_hooks:
  - fc41dPlatformIsValidCfg
common_utils:
  - ../../comm/flowparser
copy_minimal_set:
  - fc41d.h
  - fc41d.c
  - fc41d_assembly.h
read_next:
  - ../module.md
  - ../../comm/flowparser/flowparser.md
---

# FC41D 模块说明

这是当前目录的权威入口文档。

## 1. 模块定位

`fc41d` 当前按“对外公共 API + 内部控制面 + 内部数据面”组织，为后台运行的 FC41D 设备服务。

当前边界如下：

- `flowparser` 只负责单条文本事务的发送、收字节、分行、prompt、done、timeout。
- `fc41d.c` 负责 FC41D 的对外总入口、设备实例管理和后台主循环。
- `fc41d_ctrl.c` 负责启动状态机、控制事务提交、URC 状态映射和重试策略。
- `fc41d_data.c` 负责 BLE payload 提取、RX/TX ring 管理和通知组包。
- `stFc41dCfg` 同时持有 transport linkId 和 reset pin 这类板级绑定，由项目侧 `User/port` 注入默认值。
- 若项目侧注册了 `fc41dSetRawMatcher()`，则 BLE connect 后命中的二进制 raw frame 会先经 `flowparser` raw hook 转进 `dataPlane->rxUsed`，未命中的文本数据仍继续走 URC/事务解析。

统一后台入口为 `fc41dProcess()`，上层不再直接触碰内部事务推进细节。

## 2. 目录内文件职责

| 文件 | 职责 |
| --- | --- |
| `fc41d.h` | 对外公共 API、配置、状态快照、数据面 API |
| `fc41d.c` | 对外总入口、后台主循环、设备实例管理 |
| `fc41d_ctrl.h` | 内部控制面状态机声明 |
| `fc41d_ctrl.c` | 控制动作、启动流程、URC 状态映射 |
| `fc41d_data.h` | 内部数据面声明 |
| `fc41d_data.c` | BLE payload 提取、RX/TX ring 管理 |
| `fc41d_assembly.h` | 项目侧 transport/tick/reset control 绑定契约 |
| `fc41d.md` | 当前目录 contract |

## 3. 对外公共接口

稳定公共头文件：`fc41d.h`

稳定 API：

- `fc41dGetDefCfg()`
- `fc41dGetCfg()`
- `fc41dSetCfg()`
- `fc41dGetDefBleCfg()`
- `fc41dSetBleCfg()`
- `fc41dInit()`
- `fc41dStart()`
- `fc41dDisconnectBle()`
- `fc41dStop()`
- `fc41dProcess()`
- `fc41dReset()`
- `fc41dIsReady()` / `fc41dGetInfo()`
- `fc41dGetState()`
- `fc41dGetRxLength()` / `fc41dReadData()` / `fc41dWriteData()`
- `fc41dGetCachedMac()`
- `fc41dSetUrcHandler()` / `fc41dSetUrcMatcher()`
- `fc41dSetRawMatcher()`

上层推荐顺序：

1. `fc41dGetDefCfg()` + `fc41dSetCfg()`。
2. `fc41dGetDefBleCfg()` + `fc41dSetBleCfg()`。
3. `fc41dInit()`。
4. `fc41dStart(..., FC41D_ROLE_BLE_PERIPHERAL)`。
5. 在调度器中周期调用 `fc41dProcess(nowTickMs)`。
6. 用 `fc41dGetState()` / `fc41dGetRxLength()` / `fc41dReadData()` / `fc41dWriteData()` / `fc41dGetCachedMac()` 读写 BLE 数据面。

## 4. 配置、状态与生命周期

`fc41d` 当前采用 `active module + background pump` 模式：

- `Init()` 只完成 transport / control hook / flowparser stream 初始化，并把 reset pin 交给项目侧 control hook 做引脚初始化。
- `Start()` 只写入目标角色并挂起后台启动序列，不阻塞等待。
- `Process()` 统一推进 RX、URC、控制事务、复位时序、等待 `ready`、ready settle 和错误重试。
- `fc41dDisconnectBle()` 只在 `READY` 且当前存在 BLE 外连时提交一次主动断连事务，不阻塞等待。

运行态分两层：

- `stFc41dInfo`：偏 flowparser / transport 的底层状态。
- `stFc41dState`：偏业务可见的角色、ready、advertising、BLE 连接、缓存 MAC、最近错误。

BLE ready 条件：

- 复位脚已被拉低 1 s 后再释放。
- `ready` URC 已出现并完成 settle。
- `AT` re-probe 成功。
- `QSTASTOP`、`QBLEINIT`、可选 `QBLENAME`、可选 `QBLEGATTSSRV`、可选 `QBLEGATTSCHAR`、`QBLEADVSTART` 完成。
- 主动断开外部 BLE 连接由 `fc41dDisconnectBle()` 触发，默认在 `fc41d_ctrl.c` 中集中下发对应 `Q*` AT 命令。

## 5. 函数指针 / port / assembly 契约

| 名称 | 必需/可选 | 由谁实现 | 在哪里被调用 | 原型摘要 | 成功语义 | 失败语义 | 前置条件 | 备注 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `fc41dLoadPlatformDefaultCfg` | 必需 | 项目侧 `User/port` | `fc41dGetDefCfg` | `void (*)(device, cfg)` | 写入默认 linkId / reset pin / timeout | 不写则退回 weak 默认值 | `cfg != NULL` | 默认值应体现当前板级接线 |
| `fc41dGetPlatformTransportInterface` | 必需 | 项目侧 `User/port` | `fc41dInit`、`fc41dBackGourd`、发送 hook | `const stFc41dTransportInterface *(*)(cfg)` | 返回可用 transport 接口 | 返回 `NULL` 视为未绑定 | cfg 已装载 | core 不直接 include `drvuart_port.h` |
| `fc41dGetPlatformControlInterface` | 必需 | 项目侧 `User/port` | `fc41dInit`、`fc41dStart`、`fc41dProcess` | `const stFc41dControlInterface *(*)(device)` | 返回可用 reset control hook | 返回 `NULL` 时 `Start` 失败 | device 合法 | 当前最少要提供 `init(resetPin)` 和 `setResetLevel(resetPin, isActive)` |
| `fc41dPlatformIsValidCfg` | 建议 | 项目侧 `User/port` | `fc41dSetCfg`、`fc41dInit` | `bool (*)(cfg)` | 返回 `true` | 返回 `false` 视为配置非法 | `cfg != NULL` | 用于限制 linkId、reset pin 到项目允许范围 |

## 6. 公共函数使用契约

| 来源模块 | 公共函数 | 允许在哪些文件调用 | 用途 | 调用前提 | 典型调用顺序 | 返回值处理 | 禁止做法 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `flowparser_stream` | `flowparserStreamInit/Reset/Feed/Submit/Proc/IsBusy/GetStage` | `fc41d.c`、`fc41d_ctrl.c` | 背景泵和控制事务推进 | stream cfg 已准备好 | `Init -> Start -> Process` | 显式映射为 `fc41d` 状态码 | 让上层直接操作 `stFlowParserStream` |
| `flowparser_stream` raw hook | `flowparserStreamSetRawHook` | `fc41d.c` | BLE 连接后的原始帧旁路到 `dataPlane` | 已注册项目侧 raw matcher | `SetRawMatcher -> Process -> ReadData` | 命中后写入 `rxUsed` ring，未命中继续走文本流 | 在 `rep/module/fc41d` 内硬编码项目私有帧格式 |

## 7. 改动落点矩阵

| 需求 | 应改文件 | 不该改的文件 |
| --- | --- | --- |
| 改 FC41D 启动流程、ready 判定、MAC 缓存 | `fc41d_ctrl.c/.h` | 项目 manager |
| 改 payload 提取、RX/TX ring、通知组包 | `fc41d_data.c/.h` | 项目 manager |
| 改 BLE connect 后的 raw frame 判定 | 项目侧 `wireless_mgr` + `fc41dSetRawMatcher` | `fc41d.c` 中硬编码项目私有协议 |
| 改默认 UART linkId、tick 来源、reset pin 绑定 | 项目侧 `fc41d_port.*` | `fc41d.c` |
| 改 FC41D 常用 AT 命令文本、超时、控制顺序 | `fc41d_ctrl.c/.h` | `wireless_mgr.c` |
| 改业务默认 BLE 名称、service UUID、char UUID | 项目侧 manager / cfg | `fc41d.c` |

当前工程的 BLE 相关 capability 默认按 FC41D `Q*` 命令族组包，例如 `AT+QSTASTOP`、`AT+QBLEINIT`、`AT+QBLENAME`、`AT+QBLEADVSTART`；如果后续切换到其他固件分支，应优先在 `fc41d_ctrl.*` 统一调整，而不是在业务层散落 AT 文本。

当前 BLE 数据发送同样由 `fc41d.c` 持有：

- 上层通过 `fc41dWriteData()` 提交原始字节流。
- `fc41d` 在 `RUNNING` 阶段按块缓存、等待连接态后再下发通知命令。
- 当前通知命令格式固定为 `AT+QBLEGATTSNTFY=<tx-char-uuid>,<raw-byte-len>,<hex-bytes>`，其中 `<hex-bytes>` 是原始字节流按大写十六进制展开后的文本。
- 发送组包和重试留在模块内，不再让 manager 直接拼 AT 文本。

当前 BLE 数据接收对 `+QBLE...WRITE` 这类 URC 也保持“少猜测”策略：

- BLE 已连接时，`fc41dBackGround()` 会把 UART 收到的每个 RX chunk 直接镜像写入 `dataPlane->rxUsed`，不再等待整条 `WRITE` URC 结束。
- `fc41dDataTryStoreUrcPayload()` 只保留为未进入连接态时的行级兜底，不再在模块内猜测引号区、hex 文本区或业务 payload 边界。
- 上层 manager 自己创建的协议解析器负责按同步头重同步，并丢弃前导的 URC 文本噪声。

## 8. 复制到其他工程的最小步骤

最小依赖集：`fc41d.h`、`fc41d.c`、`fc41d_ctrl.h`、`fc41d_ctrl.c`、`fc41d_data.h`、`fc41d_data.c`、`fc41d_assembly.h`、`flowparser`。

外部工程至少要补齐：

- transport init
- transport write
- transport getRxLen
- transport read
- tick provider
- reset control hook

## 9. 验证清单

- `fc41dInit()` 后 transport ready 为真。
- `fc41dStart()` 后持续调用 `fc41dProcess()`，状态能从 `BOOTING/CONFIGURING` 进入 `READY`。
- `ready`、BLE connect/disconnect、BLE write 这类 URC 不再要求 manager 自己解析。
- `fc41dReadData()` 能读出 BLE payload，`fc41dGetCachedMac()` 能返回后台缓存的 MAC。
- `fc41d_ctrl.*` 和 `fc41d_data.*` 内部职责明确，manager 不再持有 FC41D 底层 stage 和 payload 解析细节。