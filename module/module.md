---
doc_role: layer-guide
layer: module
module: module
status: active
portability: layer-dependent
public_headers: []
core_files:
    - module.md
port_files: []
debug_files: []
depends_on:
    - ../rule/projectrule.md
forbidden_depends_on:
    - core 直连 bsp 或下层 _port.h
required_hooks: []
optional_hooks: []
common_utils: []
copy_minimal_set:
    - module/
read_next:
    - fc41d/fc41d.md
    - mpu6050/mpu6050.md
    - w25qxxx/w25qxxx.md
---

# Module 层总文档

这是 `module/` 的权威入口文档。

## 1. 本层目标和边界

`module` 层承载具体器件或功能模块的稳定语义，位于 `driver` 之上、业务编排之下。

本层负责：

- 定义模块公共 API。
- 管理 `Cfg`、运行态、`isReady` 和初始化流程。
- 把寄存器访问、协议访问或器件语义留在 core。
- 通过 assembly / platform hook 适配下层 drv 公共接口。

本层不负责：

- 直接处理 BSP 或 MCU 细节。
- 在公共 API 中暴露 `PortBinding`、`PortInterface` 之类的项目绑定类型。

## 2. 生命周期模板

本层优先采用 `passive module` 模式：

- `GetDefCfg()`：向调用者写入稳定默认配置。
- `GetCfg()`：返回模块当前持有的配置快照。
- `SetCfg()`：写入新配置并清掉 ready / 缓存。
- `Init()`：只消费当前 cfg，不顺手生成默认值。
- `IsReady()` / `GetInfo()`：暴露运行态只读查询。

只有真正需要周期运行时，才升级为 `active service` 或 `recoverable service`。

## 3. 公共 API 与 assembly API 的分离方式

- 公共 API：写在 `<module>.h`，只暴露稳定语义。
- assembly API：写在 `*_assembly.h` 或 `XxxPlatform*` provider 中，用于默认 transport / linkId / bus 绑定。
- debug API：写在 `*_debug.*`，只能提供可裁剪联调能力。

推荐 API 顺序：

1. `GetDefCfg`
2. `GetCfg`
3. `SetCfg`
4. `Init`
5. `IsReady` / `GetInfo`
6. 业务读写接口

## 4. 下级目录主文档必须写清的内容

每个模块主文档至少要写清：

- `GetDefCfg/GetCfg/SetCfg/Init` 的真实 contract。
- ready 条件、repeat init、hot reconfig 和 recover path。
- assembly / platform hook 契约表。
- 对下层 drv 的公共函数使用表。
- 哪些文件属于 core，哪些属于 assembly/debug。

## 5. 本层通用命名模式

推荐模式：

- `<module>.h/.c`：公共语义与业务流程。
- `<module>_assembly.h`：装配期 contract。
- `xxxLoadPlatformDefaultCfg()`：默认 cfg provider。
- `xxxGetPlatformInterface()`：最小底层动作 provider。
- `xxxPlatformDelayMs()`：平台等待钩子。

## 6. 常见错误写法和反例

- 在公共头文件中直接 include `drviic_port.h`、`drvspi_port.h` 等下层绑定头。
- 把默认 bus / linkId 等项目绑定字段塞进公共 API。
- 把寄存器语义放进 assembly adapter，而不是留在 core。
- 文档只写“依赖 drviic”却不写实际调用哪些 `drvIic*` 接口。

## 7. 复制到其他工程时如何处理

本层大多数目录属于 `layer-dependent`：

- core 可复用。
- assembly / platform hook 需要重写或重绑。
- debug 文件通常可裁剪。

复制时先保留 `<module>.h/.c` 和 `*_assembly.h`，再根据主文档重写默认 provider。

## 8. AI 推荐阅读顺序

1. 先读本文件。
2. 再读目标模块主文档。
3. 再读 `*_assembly.h`。
4. 最后读 `.h/.c` 和 `*_debug.*`。
6. 初始化底层总线。
7. 清理 `isReady` 和旧缓存。
8. 读取关键 ID 或在线状态。
9. 下发复位、时序、模式、容量等初始化配置。
10. 所有步骤成功后再置 `isReady = true`。

这样做的目的：

- 初始化失败时状态清晰。
- 旧缓存不会被误用。
- 调试时容易定位失败阶段。

## 12. 状态码规则

新增模块时优先复用 `eDrvStatus`。

建议规则：

- 参数错误直接返回 `DRV_STATUS_INVALID_PARAM`。
- 未初始化或 port 接口未就绪返回 `DRV_STATUS_NOT_READY`。
- 设备 ID 不匹配返回 `DRV_STATUS_ID_NOTMATCH`。
- 底层通信失败直接透传 drv 状态。
- 模块业务独有错误，例如越界、对齐错误、协议不支持，可从 `DRV_STATUS_ERROR + 1` 往后扩展。

## 13. 默认值放在哪里

下面这些通常放 port 层：

- 默认 bus。
- 默认总线类型。
- 默认地址。
- 默认片选。
- 平台相关延时常量。

下面这些通常放 core 层：

- 协议命令。
- 寄存器地址。
- 数据解析公式。
- 初始化流程语义。

判断标准只有一句话：

- 更像“器件怎么工作”的，放 core。
- 更像“这块板子怎么接”的，放 port。

## 13. 新模块落地步骤

建议按下面顺序生成新模块：

1. 建立目录和五个基础文件。
2. 在 `<module>.h` 中先定义状态码、设备编号、配置结构、上下文结构、公共 API。
3. 在 `<module>_port.h` 中定义绑定结构、port 接口表、默认配置函数、延时接口。
4. 在 `<module>_port.c` 中先写默认映射，再写 adapter，再写绑定校验函数。
5. 在 `<module>.c` 中实现默认配置装载、配置校验、初始化流程和业务接口。
6. 在 `<module>.md` 中补齐“内部文件分工”和“port 如何满足 core”的说明。

## 14. 每个模块自己的 md 应该写什么

每个 `<module>.md` 至少应包含下面内容：

```text
# <module> 模块设计说明

## 1. 模块目标
## 2. 文件分工
## 3. core 需要的最小底层能力
## 4. port 链接函数设计
## 5. 默认配置与默认映射
## 6. 初始化流程
## 7. 公共 API 使用顺序
## 8. 错误处理与 ready 约束
## 9. 后续扩展点
```

## 15. 自检清单

写完一个新模块后，用下面清单检查：

- core 是否仍然不依赖 bsp。
- port 接口表是否只保留最小动作。
- adapter 是否只负责翻译，不负责业务决策。
- `Cfg` 和运行态是否已分离。
- 默认映射是否放在 port 层。
- `Init()` 是否分阶段并在最后置 ready。
- 公共接口是否区分参数错误、未就绪、通信失败。
- 模块自己的 md 是否已经写清 core 和 port 的契约。

当前仓库里新增 FC41D 这类串口 AT 模块时，优先沿用“module core + User/port transport binding + background poll”模式，不要把 UART 轮询或系统 tick 直接写进 `rep/module` 之外的随机业务文件。

满足以上要求后，这个模块通常就符合当前工程的 module 层风格。