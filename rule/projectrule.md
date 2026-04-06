---
doc_role: repo-rule
layer: rule
module: projectrule
status: active
portability: project-bound
public_headers: []
core_files:
	- projectrule.md
port_files: []
debug_files: []
depends_on:
	- rule.md
	- coderule.md
forbidden_depends_on:
	- core 反向依赖 port
required_hooks: []
optional_hooks: []
common_utils: []
copy_minimal_set:
	- rule/projectrule.md
read_next:
	- ../drvlayer/drvrule.md
	- ../module/module.md
---

# 项目规则

本文档定义仓库级架构硬约束，同时补充文档层面的硬约束。

## 1. 基本原则

- 涉及 console 的能力必须由宏开关显式控制。
- 新驱动优先参考 `drvgpio`、`drvuart` 等现有主流结构，不自行发明新的分层模式。
- 若某模块依赖其他驱动或工具，只能依赖其公共层接口，不能跨层直连 BSP 或私有结构。
- `drvlayer` 的核心目标是让 BSP 适配 `drvxxx` 的公共契约。
- `module` 的核心目标是让 assembly / hook 契约适配下层 drv 公共接口。

## 2. Core-Port 强约束

- core 只允许依赖本模块公共头文件以及更底层模块的公共接口。
- 非 port、非 debug 文件禁止 include `_port.h`，也禁止直接调用 `XxxPort*`、`XxxPlatform*` 私有绑定接口。
- 公共头文件禁止暴露板级绑定字段、port 私有类型和 BSP 句柄。
- 默认资源、tick、总线、缓冲区、延时等项目绑定项必须通过 platform hook、assembly 配置或默认 cfg 注入给 core。
- `system` 和业务层如果需要项目绑定信息，必须经由模块公共 API 获取，不能跨层读取下层绑定细节。

## 3. 文档层硬约束

每个叶子目录主文档必须满足：

1. 文件名尽量与目录同名。
2. 开头明确声明“这是当前目录的权威入口文档”。
3. 具有统一 front matter。
4. 明确区分公共 API、assembly / hook API、debug API。
5. 补齐 `函数指针 / port / assembly 契约表`。
6. 补齐 `公共函数使用契约表`。
7. 写出 `复制到其他工程的最小步骤`。
8. 写出 `改动落点矩阵`。

父目录总文档必须写清：

- 本层目标和边界。
- 允许依赖与禁止依赖。
- 下级目录主文档必须包含哪些章节。
- 本层命名模式与常见反例。
- 本层复制到其他工程时，port / assembly 一般如何处理。

## 4. 公共 API / assembly API / debug API 分离规则

- 公共 API：面向上层稳定调用，只写在主文档 `对外公共接口` 章节。
- assembly API：面向项目装配、默认绑定和 hook 注入，只写在 `函数指针 / port / assembly 契约` 章节。
- debug API：只用于联调、console 和诊断，必须说明可选性和宏开关。

禁止把 debug 注册函数写成核心初始化强依赖，也禁止让 debug 入口承担真正业务逻辑。

## 5. 复制到外部项目的最低说明要求

当目录被标记为 `standalone` 或 `layer-dependent` 时，主文档必须明确：

- 最小依赖文件集。
- 需要额外补齐的外部符号或 hook。
- 可以裁剪掉的 debug / console 文件。
- 默认映射和默认资源需要在哪一层重写。

当目录被标记为 `project-bound` 时，主文档必须明确哪些内容只能参考，不能直接搬运。

## 6. 模块改造模板

1. 先清理公共头文件中的 port 概念暴露。
2. 识别 core 对平台的真实依赖，并归类成 cfg、hook、延时、资源映射等最小动作。
3. 用 platform hook 或 assembly 配置把这些依赖收敛成稳定 contract。
4. 把板级实现、默认资源和 debug 注册留在绑定层。
5. 改完后检查非 port、非 debug 文件中是否仍残留 `_port.h` include、`XxxPort*` 调用或 port extern。