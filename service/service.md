---
doc_role: layer-guide
layer: service
module: service
status: active
portability: layer-dependent
public_headers: []
core_files:
    - service.md
port_files: []
debug_files: []
depends_on:
    - ../rule/projectrule.md
forbidden_depends_on:
    - 把当前工程 manager 流程直接塞进公共 service core
    - service core 直接依赖 User/bsp 或具体 HAL 句柄
required_hooks: []
optional_hooks: []
common_utils:
    - log
    - driver/drvmcuflash
copy_minimal_set:
    - service/
read_next:
    - log/log.md
    - rtos/rtos.md
    - update/update.md
---

# Service 层总文档

这是 `rep/service/` 的权威入口文档。

## 1. 本层目标和边界

`service` 层承载跨项目可复用、但比 `driver` / `module` 更偏流程和生命周期的公共能力。

本层负责：

- 沉淀通用服务型状态机或生命周期能力。
- 用 core/port/debug 分离方式收敛项目绑定点。
- 复用已有 `driver`、`module`、`console` 能力，不重新发明底层接口。

本层不负责：

- 当前工程的 manager、system、mode 切换和任务编排。
- 直接依赖具体 BSP、HAL 句柄或裸中断清理实现。

## 2. 当前子目录与入口

| 子目录 | 作用 | 权威入口 |
| --- | --- | --- |
| `log/` | 统一 log/console 输入输出抽象 | `log/log.md` |
| `rtos/` | RTOS 适配和最小并发抽象 | `rtos/rtos.md` |
| `update/` | 通用升级状态机与逻辑区域编排 | `update/update.md` |

## 3. 目录内推荐模式

`service` 层优先采用下面的拆分：

- `xxx.h/.c`：稳定公共语义和状态机。
- `xxx_port.h/.c`：项目存储、transport、tick、watchdog 等绑定。
- `xxx_debug.h/.c`：可裁剪的诊断和状态跟踪。

只有当服务确实没有项目绑定点时，才允许省略 `port` 文件。

## 4. 允许依赖与禁止依赖

允许依赖：

- `driver/` 公共驱动接口。
- `module/` 公共模块接口。
- `log` 统一日志。
- `rtos` 最小互斥、tick 和临界区抽象。

禁止依赖：

- 直接 include `User/` 下的项目头文件进入 service core。
- 把具体芯片寄存器地址、具体 UART 口、具体 SPI 设备号写死在 service core。

## 5. 下级主文档最低要求

每个 service 叶子目录主文档至少要写清：

- 本目录的稳定公共 API。
- core 与 port 的边界。
- 必需/可选 hook 契约表。
- 公共函数使用契约表。
- 改动落点矩阵和复制到其他工程的最小步骤。

## 6. 复制到其他工程时的处理方式

`service` 子目录通常属于 `layer-dependent`：

- `core` 可复用。
- `port` 需要重写或重新绑定。
- `debug` 可按需要裁剪。

复制时先读对应叶子目录主文档，再确认是否需要一并带走下层 `driver`/`module` 依赖。

## 7. AI 推荐阅读顺序

1. 先读本文件。
2. 再读目标子目录主文档。
3. 再看对应 `*_port.h` 或 `*_port.c`。
4. 最后读 `.h/.c` 与 `*_debug.*`。