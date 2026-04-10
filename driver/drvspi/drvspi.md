---
doc_role: module-spec
layer: driver
module: drvspi
status: active
portability: layer-dependent
public_headers:
  - drvspi.h
core_files:
  - drvspi.c
port_files: []
debug_files:
  - drvspi_debug.h
  - drvspi_debug.c
depends_on: []
forbidden_depends_on:
  - 在 core 中直连 CS GPIO 或 SPI 控制器私有实现
required_hooks:
  - drvSpiBspInterface.init
  - drvSpiBspInterface.transfer
  - drvSpiBspInterface.csControl.write
optional_hooks:
  - drvSpiBspInterface.csControl.init
common_utils: []
copy_minimal_set:
  - drvspi.h
  - drvspi.c
read_next:
  - ../drvrule.md
---

# DrvSpi 模块说明

这是当前目录的权威入口文档。

## 1. 模块定位

`drvspi` 提供面向上层模块的 SPI 主机抽象，并把“总线收发”和“片选控制”拆成两套能力：控制器收发由 BSP 提供，CS 行为由 `csControl` 注入。

## 2. 目录内文件职责

| 文件 | 职责 |
| --- | --- |
| `drvspi.h` | 事务结构体、CS 控制结构、BSP hook 类型、公共 API |
| `drvspi.c` | 参数检查、总线互斥、CS 时序、事务封装 |
| `drvspi_debug.h/.c` | 可选 debug / console 能力 |
| `drvspi.md` | 当前目录 contract |

当前目录没有独立 `_port.*` 文件，默认总线和 CS 绑定由外部 provider 提供。

## 3. 对外公共接口

稳定公共头文件：`drvspi.h`

稳定 API：

- `drvSpiInit()`
- `drvSpiSetCsControl()`
- `drvSpiTransfer()` / `drvSpiTransferTimeout()`
- `drvSpiWrite()` / `drvSpiRead()` / `drvSpiWriteRead()`
- `drvSpiExchange()`

调用顺序：

1. 先 `drvSpiInit(spi)`。
2. 若需重绑片选，先 `drvSpiSetCsControl()`。
3. 再调用读写或交换接口。

## 4. 配置、状态与生命周期

- `stDrvSpiTransfer` 支持第一段写、第二段写和最后一段读。
- 公共层固定负责 CS 拉有效、传输、再释放。
- 默认超时和默认 CS 策略属于项目绑定，不属于核心 SPI 语义。

## 5. 依赖白名单与黑名单

- 允许依赖：无额外公共工具硬依赖。
- 禁止依赖：在 `drvspi.c` 中写死 CS GPIO、片选极性或控制器实例名。
- 禁止做法：让 BSP 在内部私自切换 CS，破坏整事务语义。

## 6. 函数指针 / port / assembly 契约

| 名称 | 必需/可选 | 由谁实现 | 在哪里被调用 | 原型摘要 | 成功语义 | 失败语义 | 前置条件 | 备注 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `init` | 必需 | 当前工程 SPI BSP | `drvSpiInit()` | `eDrvStatus (*)(uint8_t spi)` | 总线可用 | 返回明确错误码 | spi 合法 | 不负责 CS 初始化 |
| `transfer` | 必需 | 当前工程 SPI BSP | 所有事务 helper | `eDrvStatus (*)(uint8_t, const uint8_t *, uint8_t *, uint16_t, uint8_t, uint32_t)` | 一段原始字节流收发成功 | 超时/忙/错误 | 已初始化 | `txBuffer == NULL` 时必须发 `fillData` |
| `csControl.init` | 可选 | 当前工程 CS provider | `drvSpiInit()` | `void (*)(void *context)` | CS 进入可控状态 | `NULL` 表示无需额外初始化 | context 合法 | 初始化后应保持非选中状态 |
| `csControl.write` | 必需 | 当前工程 CS provider | `drvSpiTransfer*()` | `void (*)(void *context, bool isActive)` | CS 进入/退出选中状态 | 无返回值，必须行为稳定 | context 合法 | 极性转换只能留在 provider 内 |

## 7. 公共函数使用契约

当前目录外部依赖均经由 `stDrvSpiBspInterface` 注入，不直接调用其他公共模块函数。

## 8. 改动落点矩阵

| 需求 | 应改文件 | 不该改的文件 |
| --- | --- | --- |
| 改事务语义或 helper | `drvspi.c/.h` | BSP 控制器实现 |
| 改默认片选、极性或超时 | 当前工程 SPI/CS provider | `drvspi.c` 主流程 |
| 增加调试命令 | `drvspi_debug.*` | `drvspi.c` |

## 9. 复制到其他工程的最小步骤

最小依赖集：`drvspi.h/.c` 与新的 SPI / CS provider。

外部项目必须补齐：`init`、`transfer`、`csControl.write`；若需要额外初始化，再补 `csControl.init`。若不需要 debug，可不带 `drvspi_debug.*`。

## 10. 验证清单

- 初始化后默认 CS 处于非选中状态。
- 整个 `WriteRead` 事务期间 CS 一直保持有效。
- 纯读场景真实发送 `readFillData`。
- 更换 CS 绑定后无需修改上层模块代码。