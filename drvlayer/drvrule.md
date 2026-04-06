---
doc_role: layer-guide
layer: drvlayer
module: drvrule
status: active
portability: layer-dependent
public_headers: []
core_files:
    - drvrule.md
port_files: []
debug_files: []
depends_on:
    - ../rule/projectrule.md
forbidden_depends_on:
    - core 直接依赖 BSP
required_hooks: []
optional_hooks: []
common_utils: []
copy_minimal_set:
    - drvlayer/
read_next:
    - drvuart/drvuart.md
    - drviic/drviic.md
    - drvspi/drvspi.md
---

# DrvLayer 总文档

这是 `drvlayer/` 的权威入口文档。

## 1. 本层目标和边界

`drvlayer` 负责把项目逻辑资源与具体 MCU / BSP 彻底拆开，让上层只依赖稳定的 `drvXxx*` 公共接口。

本层负责：

- 公共驱动语义、状态码、参数校验和互斥保护。
- 对 BSP hook 的最小 contract 定义。
- 逻辑资源编号、默认超时、默认缓冲和 debug 可选能力的约束。

本层不负责：

- 业务状态机。
- 板级寄存器细节泄漏到上层。
- 让应用层绕过 `drvxxx` 直连 BSP。

## 2. 允许依赖与禁止依赖

- 允许依赖：`rep_config.h`、`tools/` 下的公共工具、同层共享的稳定头文件。
- 禁止依赖：MCU SDK 头文件直接进入 `drvxxx.c`、板级引脚细节、业务层私有结构。
- 应用层和 module 层只能依赖 `drvxxx.h`，不能直连 BSP。

## 3. 下级目录主文档必须包含的内容

每个 `drvxxx.md` 必须至少包含：

1. 模块定位。
2. 目录内文件职责。
3. 对外公共接口。
4. 配置、状态与生命周期。
5. 依赖白名单与黑名单。
6. 函数指针 / port / assembly 契约。
7. 公共函数使用契约。
8. 改动落点矩阵。
9. 复制到其他工程的最小步骤。
10. 验证清单。

## 4. 本层通用命名模式

推荐模式：

- `drvxxx.h`：稳定公共接口。
- `drvxxx.c`：公共语义与状态管理。
- `drvxxx_debug.h/.c`：可选调试与 console 注册。
- `bspxxx.*` 或平台 provider：具体外设实现。
- `xxxGetPlatformBspInterfaces()`、`gDrvXxxBspInterface[]`：当前工程绑定入口。

说明：

- 当前仓库里部分驱动没有独立 `_port.*` 文件，这种情况下也必须在主文档中把平台 hook 契约写清楚，不能假装存在 `_port.*`。

## 5. 常见错误写法和反例

- 在 `drvxxx.c` 里直接写 GPIO 端口号、DMA 通道号或具体实例名。
- 把可选 debug 注册做成核心初始化的硬依赖。
- 可选 hook 缺失时伪装成功，而不是返回 `DRV_STATUS_UNSUPPORTED`。
- 在主文档里只写“依赖某驱动”，却不写到函数级调用表。

## 6. 复制到其他工程时如何处理

`drvlayer` 子目录通常属于 `layer-dependent`：

- 可以复用 core 语义。
- 需要重写或重新绑定 BSP hook。
- debug/console 文件通常可裁剪。

子目录主文档必须明确：

- 哪些 hook 是必需的。
- 哪些默认值属于当前工程，可在外部项目替换。
- 哪些 debug 文件可不带走。

## 7. AI 推荐阅读顺序

1. 先读本文件。
2. 再读目标驱动主文档。
3. 再看对应 `.h/.c` 和 `*_debug.*`。
4. 最后再看 BSP 或外部平台实现。
