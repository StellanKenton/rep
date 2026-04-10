---
doc_role: repo-rule
layer: rule
module: coderule
status: active
portability: project-bound
public_headers: []
core_files:
	- coderule.md
port_files: []
debug_files: []
depends_on:
	- projectrule.md
forbidden_depends_on: []
required_hooks: []
optional_hooks: []
common_utils: []
copy_minimal_set:
	- rule/coderule.md
read_next:
	- ../example/example.md
---

# 规则

## 1. 环境与文本约束

- 环境：Visual Studio Code + Windows 11。
- 代码注释使用英文；新增 md 默认使用中文。
- 文本编码统一为 UTF-8，缩进统一为 4 个空格，换行统一为 LF。
- 每个文本文件以且仅以一个换行符结尾。
- 文件名和目录名保持小写。

## 2. 通用工程原则

- 在引入新模式前，先遵循现有局部主流实现。
- 除非任务明确要求，否则不要把重构与功能变更混在一起。
- 非平凡修改都要考虑错误处理、日志和可测试性。
- 新增 `typedef` 结构体和宏定义尽量放在 `.h` 中。
- 新增文件时先阅读 `example/` 下的同类示例；如果新增的是项目绑定内容，默认放进 `example/`，不要再在 `rep/` 顶层新增 `manager/`、`system/` 一类目录。
- 新增 `.c`、`.h` 文件时，默认按 `newfile/` 下同类示例补齐统一文件头和文件尾，不能省略。

## 2.1 新增 C/H 文件模板约束

- 默认模板来源：`newfile/example.c`、`newfile/example.h`。
- 新增 `.c` 文件时，必须包含统一注释文件头、正文实现和 `End of file` 文件尾注释。
- 新增 `.h` 文件时，必须包含统一注释文件头、include guard、`extern "C"` 骨架（如适用）和 `End of file` 文件尾注释。
- 文件头中的 `@file` 必须与当前文件名一致；其余字段按实际信息填写，未知项可暂留空，但头部结构不能删。
- 文件尾统一保留示例中的结束注释风格，不要改成其他样式。
- 若当前目录已有更具体的同类模板，则优先复用该目录主流模板；若没有，则回退到 `newfile/`。

## 3. 命名规则

- C 标识符默认使用 camelCase。
- 全局变量和文件作用域静态变量使用 `g` 前缀。
- 临时局部变量使用 `l` 前缀。
- 结构体类型使用 `st` 前缀，枚举类型使用 `e` 前缀。
- 函数名尽量以前缀模块名开头，例如 `drvSpiInit`、`mpu6050Init`、`frmProcProcess`。
- 缩写可以使用，但必须保持可读性与唯一性。

## 4. 文档命名与术语规则

- 目录主文档尽量与目录同名，例如 `drvuart.md`、`frameprocess.md`。
- 补充文档可以使用 `architecture.md`、`migration.md`、`plan.md` 等名称，但不能与主文档竞争权威性。
- 文档中统一使用下面术语：
	- `core`：稳定语义层。
	- `port` 或 `platform hook`：项目绑定层或注入点。
	- `assembly`：装配期配置、默认 linkId / bus / transport 绑定。
	- `debug`：可裁剪的调试与 console 能力。

推荐命名：

- adapter：`xxxHardIicReadRegAdpt`、`xxxSpiTransferAdpt`
- hook：`xxxPlatformDelayMs`、`xxxLoadPlatformDefaultCfg`
- assembly 配置：`stXxxAssembleCfg`
- provider：`xxxGetPlatformInterface`、`xxxLoadPlatformDefaultCfg`

不推荐命名：

- 含义模糊的 `doXxx`、`commonFunc`、`tmpHook`
- 把具体 MCU 或引脚名直接塞进 core 公共 API

## 5. 代码结构规则

- 每个文件保持单一职责。
- 头文件只暴露最小必要接口，内部辅助函数保持为 `static`。
- 仅包含实际使用的头文件。
- 修改驱动或模块前先读对应父目录总文档。

## 6. C 规则

### 6.1 风格与语言使用

- 大括号使用同行风格。
- 优先写可移植 C 代码，避免无必要的编译器扩展。
- 尽量使用 `const`、定宽整数类型和显式初始化。
- `NULL` 仅用于空指针。

### 6.2 函数与参数

- 函数保持简短、单一职责。
- 模块边界必须校验指针、长度和枚举值。
- 失败路径返回明确状态码，不允许静默吞错。
- 函数声明、定义和函数指针类型的参数列表默认单行书写。

### 6.3 并发与中断

- ISR 只做短小、有界、非阻塞动作。
- 跨上下文共享数据必须明确 ownership、更新顺序和临界区规则。
- 文档中要写明哪些 API 允许在任务、ISR 或两者中调用。

## 7. 文档写作风格

- 主文档优先写 contract，不写散文式计划。
- 表格优先于大段散文，尤其是 hook 和公共函数使用规则。
- “改哪里”要直接写成矩阵，不要让维护者自己猜。
- 文档中的文件职责描述必须与当前目录真实结构一致，不能引用不存在的 `_port.*` 路径。
- 项目示例文档必须显式写清自己位于 `example/` 下，避免把示例层误写成仓库顶层公共层。

## 8. Brace Style: Same line
```c
void func() {
}
if (x) {
}
```