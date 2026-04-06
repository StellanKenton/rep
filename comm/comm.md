---
doc_role: layer-guide
layer: comm
module: comm
status: active
portability: layer-dependent
public_headers: []
core_files:
  - comm.md
port_files: []
debug_files: []
depends_on:
  - ../tools/ringbuffer/ringbuffer.md
forbidden_depends_on:
  - parser core 直连 UART 或 tick 私有实现
required_hooks: []
optional_hooks: []
common_utils:
  - tools/ringbuffer
copy_minimal_set:
  - comm/
read_next:
  - flowparser/flowparser.md
  - frameparser/frameparser.md
  - frameprocess/frameprocess.md
---

# Comm 层总文档

这是 `comm/` 的权威入口文档。

## 1. 本层目标和边界

`comm` 目录负责字节流解析、帧解析与链路流程编排，不直接承载具体 UART/BSP 细节。

本层负责：

- 命令流事务解析。
- 通用帧解析与组包。
- 帧接收、发送、ACK、队列和协议流程编排。

本层不负责：

- 直接操作底层 UART。
- 把 tick、默认协议或链路资源硬编码进 core。

## 2. 子目录分工

| 目录 | 职责 |
| --- | --- |
| `flowparser/` | 面向 AT / 命令事务流的 tokenizer + stream 状态机 |
| `frameparser/` | 面向二进制帧的完整包解析和发送组包 |
| `frameprocess/` | 基于 `frameparser` 的接收、发送、ACK 和队列编排 |
| `cprsensorprotol.md` | 当前 CPR 协议补充说明，不是父目录主文档 |

## 3. 下级目录主文档必须写清的内容

- 输入输出所有权。
- 状态机或阶段模型。
- 平台 hook / parser hook 契约表。
- 公共函数使用表。
- 默认协议、tick、缓冲区由谁提供。

## 4. 本层通用命名模式

- `xxxProc`：处理流程入口。
- `xxxMkPkt`：组包。
- `xxxLoadPlatformDefaultCfg`：默认协议或运行配置 provider。
- `xxxPlatform*`：链路、tick、默认格式等平台 hook。

## 5. 常见错误写法和反例

- 在 parser core 中直接 include UART 驱动私有头。
- 不说明 ring buffer ownership。
- 只写“依赖 frameparser / ringbuffer”，却不写函数级调用顺序。

## 6. 复制到其他工程时如何处理

`comm` 子目录大多属于 `layer-dependent`：core 可复用，但默认协议、tick、链路收发和默认缓冲必须在外部工程重绑。

## 7. AI 推荐阅读顺序

1. 先读本文件。
2. 需要事务流时读 `flowparser.md`。
3. 需要二进制帧时读 `frameparser.md`。
4. 需要完整链路流程时读 `frameprocess.md`。
