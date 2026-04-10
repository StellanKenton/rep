---
doc_role: module-spec
layer: module
module: w25qxxx
status: active
portability: layer-dependent
public_headers:
	- w25qxxx.h
core_files:
	- w25qxxx.c
port_files: []
debug_files:
	- w25qxxx_debug.h
	- w25qxxx_debug.c
depends_on:
	- ../../driver/drvspi/drvspi.md
forbidden_depends_on:
	- 在 core 中直连具体 SPI 绑定或 CS 细节
required_hooks:
	- stW25qxxxSpiInterface.init
	- stW25qxxxSpiInterface.transfer
optional_hooks: []
common_utils: []
copy_minimal_set:
	- w25qxxx.h
	- w25qxxx.c
read_next:
	- ../module.md
	- ../../driver/drvspi/drvspi.md
---

# W25QXXX 模块说明

这是当前目录的权威入口文档。

## 1. 模块定位

`w25qxxx` 是 SPI NOR Flash 模块，对上提供 JEDEC 探测、容量信息、读、页编程、扇区擦除、64K 擦除和整片擦除能力；对下只依赖 `drvspi` 的稳定公共接口语义。

## 2. 目录内文件职责

| 文件 | 职责 |
| --- | --- |
| `w25qxxx.h` | 配置、识别信息、状态码、最小 SPI interface 契约、公共 API |
| `w25qxxx.c` | 默认配置、JEDEC probe、读写擦、busy 轮询、ready 维护 |
| `w25qxxx_debug.h/.c` | 可选调试与 console 能力 |
| `w25qxxx.md` | 当前目录 contract |

当前目录没有独立 `_port.*` 文件，当前工程绑定通过 `stW25qxxxSpiInterface` 和默认 cfg provider 完成。

## 3. 对外公共接口

稳定公共头文件：`w25qxxx.h`

稳定 API：

- `w25qxxxGetDefCfg()`
- `w25qxxxGetCfg()`
- `w25qxxxSetCfg()`
- `w25qxxxInit()`
- `w25qxxxIsReady()` / `w25qxxxGetInfo()`
- `w25qxxxReadJedecId()` / `w25qxxxReadStatus1()` / `w25qxxxWaitReady()`
- `w25qxxxRead()` / `w25qxxxWrite()`
- `w25qxxxEraseSector()` / `w25qxxxEraseBlock64k()` / `w25qxxxEraseChip()`

调用顺序：

1. 先取默认 cfg。
2. 如需改默认 linkId，调用 `SetCfg()`。
3. 再 `Init()`。
4. 用 `IsReady()` / `GetInfo()` 确认可用后再执行读写擦。

## 4. 配置、状态与生命周期

`w25qxxx` 属于 `passive module`：

- `GetDefCfg()` 只写默认配置。
- `SetCfg()` 写入模块配置快照并清空 ready。
- `Init()` 完成 JEDEC probe 和容量识别。
- ready 条件：厂商 ID 合法、容量 ID 支持、`info` 填充完成。

## 5. 依赖白名单与黑名单

- 允许依赖：`drvspi` 公共接口语义，经由最小 SPI interface 注入。
- 禁止依赖：在 `w25qxxx.c` 中直接包含 SPI 绑定或片选私有头。
- 禁止做法：让 adapter 负责容量推导、地址宽度选择或分页写策略。

## 6. 函数指针 / port / assembly 契约

| 名称 | 必需/可选 | 由谁实现 | 在哪里被调用 | 原型摘要 | 成功语义 | 失败语义 | 前置条件 | 备注 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `init` | 必需 | 当前工程 SPI provider | `w25qxxxInit()` | `eDrvStatus (*)(uint8_t bus)` | 底层 SPI 可访问目标器件 | 返回明确错误码 | cfg 中 linkId 合法 | 只做总线 bring-up |
| `transfer` | 必需 | 当前工程 SPI provider | 读 ID、读状态、读写擦流程 | `eDrvStatus (*)(uint8_t, const uint8_t *, uint16_t, const uint8_t *, uint16_t, uint8_t *, uint16_t, uint8_t)` | 一次命令头 + 数据/读回事务成功 | 超时/忙/错误 | 已初始化 | 必须支持两段写再读的组合事务 |

## 7. 公共函数使用契约

| 来源模块 | 公共函数 | 允许在哪些文件调用 | 用途 | 调用前提 | 典型调用顺序 | 返回值处理 | 禁止做法 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `drvspi` | `drvSpiInit` / `drvSpiTransfer` | 当前工程 SPI adapter | 把底层 SPI 翻译给 core | linkId 映射合法 | `Init -> Transfer` | 透传底层状态 | 在 `w25qxxx.c` 中直接 include `drvspi_port.h` |

## 8. 改动落点矩阵

| 需求 | 应改文件 | 不该改的文件 |
| --- | --- | --- |
| 改 JEDEC 识别、命令选择、分页写逻辑 | `w25qxxx.c/.h` | SPI provider |
| 改默认总线、linkId、片选接线 | 当前工程 SPI provider | `w25qxxx.c` 业务语义 |
| 加调试命令 | `w25qxxx_debug.*` | `w25qxxx.c` 主流程 |

## 9. 复制到其他工程的最小步骤

最小依赖集：`w25qxxx.h/.c` 与新的 SPI adapter/provider。

外部项目必须补齐：`init`、`transfer` 对应的最小 SPI 动作；若不需要调试，可不带 `w25qxxx_debug.*`。

## 10. 验证清单

- `Init()` 后 JEDEC 三字节识别正确。
- 只支持的容量 ID 才会置 ready。
- 读、页编程、扇区擦除都遵循 ready 与范围检查。
- 地址越界和擦除未对齐返回明确错误语义。
