---
doc_role: module-spec
layer: drvlayer
module: drviic
status: active
portability: layer-dependent
public_headers:
	- drviic.h
core_files:
	- drviic.c
port_files: []
debug_files:
	- drviic_debug.h
	- drviic_debug.c
depends_on: []
forbidden_depends_on:
	- 在 core 中直连 IIC 控制器私有实现
required_hooks:
	- drvIicBspInterface.init
	- drvIicBspInterface.transfer
optional_hooks:
	- drvIicBspInterface.recoverBus
common_utils: []
copy_minimal_set:
	- drviic.h
	- drviic.c
read_next:
	- ../drvrule.md
---

# DrvIic 模块说明

这是当前目录的权威入口文档。

## 1. 模块定位

`drviic` 提供硬件 IIC 主机的稳定事务接口。它只关心“逻辑 IIC 总线是否能完成一次完整事务”，不关心具体控制器、引脚和时钟细节。

## 2. 目录内文件职责

| 文件 | 职责 |
| --- | --- |
| `drviic.h` | 事务结构体、BSP hook 类型、公共 API |
| `drviic.c` | 参数检查、互斥、默认超时、helper 封装 |
| `drviic_debug.h/.c` | 可选 debug / console 能力 |
| `drviic.md` | 当前目录 contract |

当前目录没有独立 `_port.*` 文件，平台绑定通过 `stDrvIicBspInterface` provider 完成。

## 3. 对外公共接口

稳定公共头文件：`drviic.h`

稳定 API：

- `drvIicInit()`
- `drvIicRecoverBus()`
- `drvIicTransfer()` / `drvIicTransferTimeout()`
- `drvIicWrite()` / `drvIicRead()`
- `drvIicWriteRegister()` / `drvIicReadRegister()`

调用顺序：

1. 先 `drvIicInit(iic)`。
2. 再调用一次事务或 helper 接口。
3. 总线异常时按需调用 `drvIicRecoverBus()`。

## 4. 配置、状态与生命周期

- `stDrvIicTransfer` 表达一次完整事务，支持纯写、纯读、先写后读、双写再读。
- `drvIicTransfer()` 使用平台默认超时；`drvIicTransferTimeout()` 使用显式超时。
- 公共层维护总线互斥和忙状态，不要求 BSP 再实现上层级别锁语义。

## 5. 依赖白名单与黑名单

- 允许依赖：无额外公共工具依赖。
- 禁止依赖：在 `drviic.c` 中直连具体 IIC 控制器头文件或 GPIO 配置。
- 禁止做法：把 repeated start 语义拆成多个无关事务。

## 6. 函数指针 / port / assembly 契约

| 名称 | 必需/可选 | 由谁实现 | 在哪里被调用 | 原型摘要 | 成功语义 | 失败语义 | 前置条件 | 备注 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `init` | 必需 | 当前工程 IIC BSP | `drvIicInit()` | `eDrvStatus (*)(uint8_t iic)` | 总线可用 | 返回明确错误码 | iic 合法 | 可重复初始化 |
| `transfer` | 必需 | 当前工程 IIC BSP | 所有事务 helper | `eDrvStatus (*)(uint8_t, const stDrvIicTransfer *, uint32_t)` | 一次事务完整完成 | 超时/NACK/忙/错误 | 已初始化 | 必须保留 repeated start 语义 |
| `recoverBus` | 可选 | 当前工程 IIC BSP | `drvIicRecoverBus()` | `eDrvStatus (*)(uint8_t iic)` | 总线恢复到可再试状态 | 未实现时公共层返回 `UNSUPPORTED` | 总线异常 | 不支持时不要伪实现 |

## 7. 公共函数使用契约

当前目录不直接依赖其他公共模块函数，核心外部依赖全部经由 `stDrvIicBspInterface` 注入。

## 8. 改动落点矩阵

| 需求 | 应改文件 | 不该改的文件 |
| --- | --- | --- |
| 改事务语义或 helper 行为 | `drviic.c/.h` | BSP 控制器实现 |
| 改默认超时或总线绑定 | 当前工程 IIC provider / BSP | `drviic.c` helper 流程 |
| 加调试命令 | `drviic_debug.*` | `drviic.c` 主流程 |

## 9. 复制到其他工程的最小步骤

最小依赖集：`drviic.h/.c` 与新的 IIC BSP hook 实现。

外部项目必须补齐：`init`、`transfer`，可选补 `recoverBus`。若不需要 debug，可不带 `drviic_debug.*`。

## 10. 验证清单

- 初始化后可访问已知从机。
- 寄存器读路径的 repeated start 行为正确。
- 超时、NACK、总线忙都返回稳定错误码。
- `recoverBus` 缺失时错误语义明确。
