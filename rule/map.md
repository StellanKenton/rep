---
doc_role: repo-rule
layer: rule
module: map
status: active
portability: project-bound
public_headers: []
core_files:
  - map.md
port_files: []
debug_files: []
depends_on:
  - rule.md
forbidden_depends_on:
  - 过时目录名
required_hooks: []
optional_hooks: []
common_utils: []
copy_minimal_set:
  - rule/map.md
read_next:
  - ../drvlayer/drvrule.md
  - ../module/module.md
  - ../comm/comm.md
---

# Rep 目录地图

这是仓库级目录导航文档，不是实现细节文档。

## 1. 顶层结构与入口

当前顶层目录及其权威入口如下：

| 目录 | 作用 | 权威入口 |
| --- | --- | --- |
| `rule/` | 仓库规则、地图、命名和文档约束 | `rule/rule.md` |
| `drvlayer/` | 公共驱动层与 BSP hook 契约 | `drvlayer/drvrule.md` |
| `module/` | passive module 与 assembly 契约 | `module/module.md` |
| `manager/` | 服务编排与生命周期 contract | `manager/manager.md` |
| `console/` | console 与 log 公共 contract | `console/console.md`、`console/log.md` |
| `comm/` | 流解析、帧解析、帧流程编排 | `comm/comm.md` |
| `system/` | 系统模式与任务编排边界 | `system/system.md` |
| `tools/` | 算法与基础容器工具 | `tools/tools.md` |
| `example/` | 标准主文档示例 | `example/example.md` |
| `scripts/` | 脚本与工具链迁移说明 | `scripts/readme.md` |

说明：

- 旧文档中的 `protocolparser/`、顶层 `ringbuffer/` 等路径已不再代表当前结构，相关内容现在归属 `comm/` 和 `tools/`。
- `rep_config.h` 是公共配置头，不是文档入口。

## 2. 按任务类型找文档

| 任务类型 | 先读 | 再读 |
| --- | --- | --- |
| 新增或修改驱动 | `rule/rule.md`、`drvlayer/drvrule.md` | 对应 `drvxxx/drvxxx.md`、`.h/.c` |
| 新增或修改功能模块 | `rule/rule.md`、`module/module.md` | 对应模块主文档、assembly 头、`.h/.c` |
| 修改服务生命周期或系统编排 | `manager/manager.md` 或 `system/system.md` | 对应 service 文档、`service_lifecycle.*`、`.h/.c` |
| 修改 console / log | `console/console.md`、`console/log.md` | 对应头文件和实现 |
| 修改协议解析或链路流程 | `comm/comm.md` | `flowparser.md`、`frameparser.md`、`frameprocess.md` |
| 修改基础算法或容器 | `tools/tools.md` | 对应工具目录主文档与 `.h/.c` |
| 新建文档或套模板 | `example/example.md` | 对应父目录总文档 |

## 3. 按复制目标找依赖

| 复制目标 | 至少先读 | 额外关注 |
| --- | --- | --- |
| `drvxxx` 目录 | `drvlayer/drvrule.md` + 对应 `drvxxx.md` | BSP hook、默认资源映射、debug 可裁剪项 |
| `module/xxx` 目录 | `module/module.md` + 对应模块主文档 | assembly hook、下层 drv 调用表、默认绑定 |
| `comm/frameparser` | `comm/comm.md` + `frameparser.md` | ringbuffer 依赖、协议格式回调、输出缓冲 ownership |
| `comm/frameprocess` | `comm/comm.md` + `frameprocess.md` | frameparser 依赖、tx/rx 钩子、ACK 策略 |
| `tools/ringbuffer` | `tools/tools.md` + `tools/ringbuffer/ringbuffer.md` | 并发模型、调用方 ownership |
| `system/` | `system/system.md` | 仅可参考，默认视为 `project-bound` |

## 4. 目录入口关系

### 4.1 `drvlayer/`

- 入口：`drvlayer/drvrule.md`
- 高复用叶子目录：`drvuart/`、`drviic/`、`drvspi/`、`drvgpio/`、`drvanlogiic/`、`drvadc/`、`drvmcuflash/`

### 4.2 `module/`

- 入口：`module/module.md`
- 高复用叶子目录：`mpu6050/`、`pca9535/`、`tm1651/`、`w25qxxx/`、`gd25qxxx/`

### 4.3 `comm/`

- 入口：`comm/comm.md`
- 叶子目录：`flowparser/`、`frameparser/`、`frameprocess/`
- 协议说明补充文档：`cprsensorprotol.md`

### 4.4 `tools/`

- 入口：`tools/tools.md`
- 基础容器：`ringbuffer/`
- 算法工具：`numfilter/`、`butterworthfilter/`、`filter1st/`、`filter2nd/`

## 5. 推荐阅读顺序

### 5.1 不知道改哪里

1. `rule/rule.md`
2. 当前文件
3. 对应父目录总文档
4. 对应叶子目录主文档
5. 目标头文件与源文件

### 5.2 文档改造任务

1. `rule/rule.md`
2. 当前文件
3. 目标父目录总文档
4. 目标叶子目录主文档
5. 用源码核对公共 API、hook、调用顺序

## 6. 使用原则

- 先缩小目录范围，再读代码。
- 父目录总文档解决“本层共性”，叶子目录主文档解决“当前目录 contract”。
- 如果目录里既有主文档又有架构草稿，默认先信主文档，再按需读补充文档。
