# Rep 目录地图

本文档用于快速说明 `USER/Rep` 下有哪些目录、各自负责什么，以及在不同任务场景下应该优先查看哪些文件。

目标不是替代各子目录里的详细文档，而是让 AI 或维护者先建立目录级上下文，再去阅读最相关的文件，避免无差别通读整个 `Rep`。

## 1. 顶层结构

`USER/Rep` 当前主要包含以下内容：

- `rule/`
    全局规则、代码风格、项目规则、目录地图等入口文档。
- `console/`
    命令控制台与日志相关设计文档及参考实现。
- `drvlayer/`
    驱动公共层设计规则，以及各个 `drvxxx` 模块的结构说明。
- `module/`
    面向具体功能模块的通用生成规则，以及已有模块说明。
- `protocolparser/`
    协议包解析器与流式数据解析相关文档和实现。
- `ringbuffer/`
    环形缓冲区实现与架构说明。
- `system/`
    系统启动编排、任务组织、system 调试命令相关内容。
- `scripts/`
    可迁移脚本包与 VS Code 工具链说明。
- `example/`
    新建模块或文件时可参考的最小示例。
- `rep_config.h`
    `Rep` 相关的公共配置入口头文件。

## 2. rule 目录

当任务刚开始、还没有明确要改哪个模块时，先看这里：

- `rule/rule.md`
    总入口规则，规定每次开始工作时应先读哪些文档。
- `rule/coderule.md`
    C 代码风格、命名、分层、接口和错误处理规则。
- `rule/projectrule.md`
    本工程特有的架构约束和设计偏好。
- `rule/map.md`
    也就是当前文件，用于决定下一步该读哪个目录、哪个文档。
- `rule/memory.md`
    memory 使用规则；当前文件为空，后续如补充应按其约束执行。

## 3. 各目录作用与入口文件

### 3.1 console

适用场景：

- 修改命令行控制台。
- 修改日志输入输出通道。
- 新增 console 命令注册。

优先查看：

- `console/console.md`
- `console/console.h`
- `console/log.md`
- `console/log.h`

### 3.2 drvlayer

适用场景：

- 新增 `drvxxx` 驱动。
- 修改 GPIO、UART、SPI、IIC、模拟量等公共驱动层。
- 处理 core / port / BSP 分层问题。

优先查看：

- `drvlayer/drvrule.md`
- `drvlayer/drvgpio/drvgpio.md`
- `drvlayer/drvuart/drvuart.md`
- `drvlayer/drvspi/drvspi.md`
- `drvlayer/drviic/drviic.md`
- `drvlayer/drvanlogiic/drvanlogiic.md`

说明：

- 如果任务明确落在某个 `drvxxx` 模块，先看 `drvrule.md`，再看对应子目录下的 `.md`、`.h`、`.c`。

### 3.3 module

适用场景：

- 新增功能模块。
- 修改 `module` 层对 `drvlayer` 的适配方式。
- 梳理 core / port 拆分。

优先查看：

- `module/module.md`
- `module/mpu6050/mpu6050.md`
- `module/w25qxxx/w25qxxx.md`

说明：

- `module.md` 讲通用模块生成规则。
- 具体模块细节看对应子目录中的 `<module>.md`。

### 3.4 protocolparser

适用场景：

- 修改协议包解析流程。
- 处理帧头、长度、CRC、超时等待等逻辑。
- 新增流式协议格式。

优先查看：

- `protocolparser/protocol_parser.md`
- `protocolparser/streamdata_parser.md`
- `protocolparser/framepareser.h`

### 3.5 ringbuffer

适用场景：

- 修改环形缓冲区结构。
- 排查上层输入输出缓存问题。

优先查看：

- `ringbuffer/ringbuffer_architecture.md`
- `ringbuffer/ringbuffer.h`

### 3.6 system

适用场景：

- 修改系统启动流程。
- 修改 system mode、任务创建、console 接线。
- 修改 system 调试命令。

优先查看：

- `system/system.md`
- `system/system.h`
- `system/systask.h`
- `system/system_debug.h`

### 3.7 scripts

适用场景：

- 迁移 VS Code + Keil + J-Link 工具链。
- 调整脚本包文档。

优先查看：

- `scripts/readme.md`
- `scripts/vscode_portable/readme.md`
- `scripts/vscode_portable/migration.md`
- `scripts/vscode_portable/manifest.md`

### 3.8 example

适用场景：

- 新建文件前寻找最小参考模板。
- 不确定局部写法时先看示例。

优先查看：

- `example/example.h`
- `example/example.c`

## 4. 常见任务的推荐阅读顺序

### 4.1 刚接手任务但还不知道该看哪些文件

推荐顺序：

1. `rule/rule.md`
2. `rule/map.md`
3. 根据任务类型进入对应目录的入口 `.md`
4. 再阅读该目录对应的 `.h` / `.c`

### 4.2 新增或修改驱动

推荐顺序：

1. `rule/rule.md`
2. `rule/map.md`
3. `drvlayer/drvrule.md`
4. 对应 `drvxxx` 目录下的 `.md`
5. 对应 `drvxxx` 的 `.h`、`.c`、`_port.h`、`_port.c`

### 4.3 新增或修改 module

推荐顺序：

1. `rule/rule.md`
2. `rule/map.md`
3. `module/module.md`
4. 对应模块目录下的 `.md`
5. 对应模块的 `.h`、`.c`、`_port.h`、`_port.c`

### 4.4 修改 system / console / protocolparser

推荐顺序：

1. `rule/rule.md`
2. `rule/map.md`
3. 进入对应目录的说明文档
4. 再阅读实际头文件和源文件

## 5. 使用原则

- 先把 `map.md` 当作目录导航，而不是实现细节文档。
- 先按任务类型缩小范围，再进入对应目录阅读详细文档。
- 除非任务本身就是梳理文档结构，否则不要一开始就遍历 `USER/Rep` 下所有文件。
- 如果任务涉及多个层次，先读更上层的规则文档，再读具体模块文档。
- 当某个目录已有专门 `.md` 说明时，优先先读该 `.md`，再看代码实现。

## 6. 一句话说明

`map.md` 是 `USER/Rep` 的导航页：先用它判断应该进入哪个目录，再定向阅读对应规则、说明文档和源码文件。
