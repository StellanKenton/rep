# DrvGpio 模块设计说明

本文档不再把重点放在“生成提示词”，而是明确 `drvgpio` 这个模块内部各文件的职责，以及 `bspgpio.c/.h` 需要怎样实现，才能满足 core 层的调用需求。

## 1. 模块定位

`drvgpio` 提供最小而稳定的逻辑 GPIO 接口，当前公共层只暴露四个动作:

- `drvGpioInit()`
- `drvGpioWrite()`
- `drvGpioRead()`
- `drvGpioToggle()`

这个模块故意保持简单，不在公共层引入方向配置、复用配置或中断控制。GPIO 的物理细节全部下沉到 BSP。

## 2. 目录内文件职责

- `drvgpio.h`: 定义公共 API 和 `stDrvGpioBspInterface`。
- `drvgpio.c`: 负责公共层参数检查、初始化检查和对 BSP 钩子的转发。
- `drvgpio_port.h`: 定义逻辑引脚枚举，以及 log/console 开关宏。
- `drvgpio_port.c`: 负责绑定 `gDrvGpioBspInterface`。
- `drvgpio_debug.c/.h`: 可选调试子模块，不应污染主流程。
- `bspgpio.c/.h`: 负责当前板卡上 GPIO 时钟、端口、引脚、模式和读写实现。

说明：

- `drvgpio.h` 的公共 API 使用 `uint8_t pin` 表示逻辑引脚编号。
- `eDrvGpioPinMap` 只定义在 `drvgpio_port.h`。
- `eDrvGpioPinState` 属于公共语义，仍定义在 `drvgpio.h`。

## 3. 当前 core 对 port 的直接依赖

`drvgpio.c` 最终只依赖一个全局钩子表:

```c
stDrvGpioBspInterface gDrvGpioBspInterface;
```

该结构体的四个成员都是必需项:

- `init`
- `write`
- `read`
- `toggle`

只要少一个，公共层初始化或后续调用就不应该被视为完整可用。

## 4. `drvgpio_port.h` 应承载的内容

当前 `drvgpio_port.h` 已经承担两类配置:

- 逻辑功能开关，如 `DRVGPIO_LOG_SUPPORT`、`DRVGPIO_CONSOLE_SUPPORT`
- 逻辑资源枚举，如 `DRVGPIO_LEDR`、`DRVGPIO_KEY`

而逻辑电平语义 `eDrvGpioPinState` 保留在 `drvgpio.h`，因为它属于公共 API 的返回值和输入值，不属于 port 绑定细节。

这里的枚举必须表示“项目逻辑引脚”，而不是“GPIOE pin4”这种物理资源名。以后如果换板卡，只需要 BSP 重映射，不应该改上层调用语义。

## 5. `bspgpio.c` 必须满足的契约

### 5.1 `bspGpioInit(void)`

职责:

- 初始化当前工程中所有已定义的逻辑引脚。
- 使能实际用到的 GPIO 时钟。
- 为每个逻辑引脚配置正确的输入输出模式。
- 为输出引脚设置安全默认值。

实现约束:

- 只能做硬件初始化，不能掺入应用业务。
- 未映射的逻辑引脚必须在 BSP 内防御性处理。
- 如果某个引脚是输入脚，初始化时就要配置好上下拉或浮空策略。

### 5.2 `bspGpioWrite(uint8_t pin, eDrvGpioPinState state)`

职责:

- 把逻辑引脚写成目标状态。

实现约束:

- 只接受有效的逻辑电平语义。
- 对输入型引脚要保护，不能错误输出。
- 如果某个输出脚是低电平有效，极性转换必须封装在 BSP 内。
- 不要要求上层知道该脚是否低有效。

### 5.3 `bspGpioRead(uint8_t pin)`

职责:

- 返回逻辑引脚当前状态。

实现约束:

- 需要把底层 SDK 的读值统一成 `DRVGPIO_PIN_RESET`、`DRVGPIO_PIN_SET` 或 `DRVGPIO_PIN_STATE_INVALID`。
- 返回语义在整个模块内必须一致。推荐返回“逻辑态”而不是“裸物理电平”。
- 对无效映射或无法判定的情况返回 `DRVGPIO_PIN_STATE_INVALID`。

### 5.4 `bspGpioToggle(uint8_t pin)`

职责:

- 翻转一个输出型逻辑引脚。

实现约束:

- 该函数必须真实可用，不能留空。
- 如果芯片支持原子翻转，优先使用。
- 如果只能先读后写，也必须保证整体语义是一次翻转。
- 输入引脚要保护，不能被错误翻转。

## 6. 推荐的 BSP 内部组织方式

推荐在 `bspgpio.c` 内部维护集中映射表，至少描述:

- GPIO 时钟
- GPIO 端口
- GPIO 引脚
- 方向属性
- 有效电平极性

如果局部代码风格更适合 `switch-case`，也可以不用表，但仍然要保证映射集中，不要把同一个逻辑引脚的物理信息散落到多个函数里。

## 7. 当前工程的默认逻辑引脚

根据 `drvgpio_port.h`，当前工程逻辑引脚包括:

- `DRVGPIO_LEDR`
- `DRVGPIO_LEDG`
- `DRVGPIO_LEDB`
- `DRVGPIO_KEY`

如果以后要增加新的 LED、按键、复位脚或片选脚，先改 `drvgpio_port.h` 的逻辑枚举，再补 BSP 映射，不要先在 BSP 里私自扩展物理引脚。

## 8. port 层绑定要求

`drvgpio_port.c` 目前只做一件事: 绑定 BSP 钩子。

目标结构应保持如下形式:

```c
stDrvGpioBspInterface gDrvGpioBspInterface = {
    .init = bspGpioInit,
    .write = bspGpioWrite,
    .read = bspGpioRead,
    .toggle = bspGpioToggle,
};
```

如果后续要扩展 console/debug，也应尽量放在独立 debug 文件或 port 层辅助入口，不要把调试流程塞回 `drvgpio.c`。

## 9. 修改或重写 BSP 时的检查点

- 每个逻辑引脚是否都已有唯一映射。
- 输出脚默认态是否安全。
- 输入脚读取语义是否统一。
- 低电平有效的归一化是否只留在 BSP 内。
- `toggle` 是否对输出脚可用。
- 新板卡接入时，上层是否仍然只需要调用 `drvGpio*`。

