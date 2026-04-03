# 项目规则

- 所有文件夹下的代码如果涉及console相关，需要有宏定义确定是否打开。
- 编写drvXxx模块时，先参考drvgpio、drvuart等现有模块的结构和接口，优先复用局部主流设计。
- 如果编写某个模块的驱动时，需要依赖其他驱动，那么把模块的接口设计成只依赖于该驱动的公共层接口，并用函数指针的方式指向其他驱动的公共接口。
- drvlayer层的核心思想是让bsp层的代码来适配drvxxx的函数指针，drvxxx层的代码不依赖于bsp层的代码，这样就可以实现drvxxx层的代码复用。
- module层的核心思想是让module层的接口函数指针去适配drvxxx的公共接口。
- 函数、变量名称、宏定义、数据类型不要太长，尽可能使用缩写，但要保证可读性和唯一性。

## Core-Port 分层强约束

- core 只允许依赖本模块 core 公共头文件以及更底层模块的 core 公共接口，禁止 include 本模块或其他模块的 `_port.h`。
- 允许直接接触 `_port.h`、`XxxPort*`、板级绑定细节的文件只限于 `*_port.c`、`*_port.h` 和 `*_debug.*`。
- 所有 core 公共头文件禁止暴露 `PortBinding`、`PortInterface`、`PortType`、`PortProtoCfg` 这类 port 概念，公共 API 只能暴露稳定语义和抽象配置。
- 如果 core 初始化需要默认资源、tick、总线、缓冲区或 IO 动作，统一改为由 port 预组装普通配置数据，或由 port 在初始化阶段注入适配回调，禁止 core 通过 extern 或直接调用反向依赖 port。
- system 和业务层如果需要项目绑定信息，必须经由模块 core 公共接口获取，不能跨层直接 include 下层模块 `_port.h`。

## 模块改造模板

1. 先清理公共头文件，把 `_port.h` include、Port 命名类型和板级绑定字段移出公共 API。
2. 识别 core `.c` 里的 port 依赖，把它们归类为默认配置、资源映射、底层 IO、时序/延时、缓冲区、临界区。
3. 把这些依赖收敛成 core 可消费的抽象配置或私有适配结构，不在公共头文件暴露 port 概念。
4. 在 `*_port.c` 中完成实际绑定，把 BSP、驱动和 core 串起来，保持 core 代码不需要感知板级差异。
5. 每改完一个模块，立刻扫描非 port、非 debug 文件中的 `_port.h` include、`XxxPort*` 调用和 `extern` port 符号，确认没有回归再进入下一个模块。