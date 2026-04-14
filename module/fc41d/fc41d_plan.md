# FC41D 模块改进计划

本文件是 `fc41d.md` 的补充文档，不是权威契约。记录当前模块已知问题与改进方向。

## 1. 已知问题

### 1.1 URC token 匹配过于宽泛（Bug）

`fc41dBleUpdateLinkStateByUrc` 和 `fc41dWifiUpdateLinkStateByUrc` 使用 `fc41dLineHasToken` 做大小写不敏感子串匹配。

- `"AP"` 会匹配 `"ESCAPE"`、`"QAPSTATE"` 等非目标行。
- `"STA"` 会匹配 `"START"`、`"STATE"` 等非目标行。
- `"CONN"` 会匹配 `"DISCONN"`，虽然 `"DISCONN"` 优先判断但逻辑脆弱。

**影响**：AT 常规响应行中含有这些子串时，可能误修改 BLE/WiFi 连接状态。

**修复方向**：
- 改为前缀匹配（`+BLECONN`、`+BLEDISCONN`、`+STACONNECTED`、`+STADISCONN` 等）。
- 在 URC handler 入口增加 `lineBuf[0] == '+'` 前置守卫。
- 对 BLE 和 WiFi 分别维护精确的 URC 模式数组，复用 `fc41dAtMatchPattern` 而非 `fc41dLineHasToken`。

### 1.2 AT 响应只保留最后一行

`fc41dExecLineHandler` 每次用 memcpy 覆盖 `stFc41dAtResp.lineBuf`，只有 `lineCount` 在累加。版本查询、扫描结果等多行响应只能拿到最后一行。

**影响**：多行 AT 响应场景（`AT+QVERSION`、`AT+QWSCAN`、`AT+QBLESTAT` 等）实际不可用。

**修复方向**：
- 方案 A：在 `stFc41dAtResp` 中增加 `pfLineCallback`，每行回调给调用方处理。
- 方案 B：`lineBuf` 改为追加写，行与行之间用 `\0` 分隔，记录每行偏移表。
- 推荐方案 A，回调模式扩展性更好，不受 buffer 大小限制。

### 1.3 状态机只支持单条命令的 init/start/stop

`stFc41dBleCfg` / `stFc41dWifiCfg` 的 `initCmdText` / `startCmdText` / `stopCmdText` 各是单个 `const char *`。真实 BLE 外设初始化通常需要 5～7 步命令序列。

**影响**：自动状态机只能覆盖最简场景，复杂初始化必须由外部逐条编排。

**修复方向**：
- 将单字符串改为命令序列数组 `const char *const *initCmdSeq` + `uint8_t initCmdSeqLen`。
- 状态机内部用 `stepIndex` 逐条推进，每步 submit → wait done → next。
- 任意一步失败进入 ERROR 态。

### 1.4 mode 语义与实际不匹配

`fc41dUpdateModeFromStates` 把 BLE connected 等同于 `FC41D_MODE_BLE_DATA`。但 FC41D 在 command 模式下也可以保持 BLE 连接（通过 `AT+QBLEGATTSNTFY` 收发数据），只有 ATO/+++ 切换才真正进入 data 模式。

**影响**：`info.mode` 误报，上层依赖 mode 判断逻辑会出错。

**修复方向**：
- `mode` 只在收到 ATO 确认 / `+++` 退出时切换。
- BLE/WiFi connected 状态不自动改 mode，保持 `FC41D_MODE_COMMAND`。
- 或者去掉 `eFc41dMode`，让上层自行判断，因为 FC41D 实际使用中绝大多数时候处于 command 模式收发。

### 1.5 TX 发送 API 缺失

RX 侧有 ring buffer + read/peek/discard 完整接口，TX 侧没有封装。BLE send (`AT+QBLEGATTSNTFY`) 和 WiFi send (`AT+QISEND`) 需要用户自己拼 AT 命令。

**影响**：读写 API 不对称，使用不便。

**修复方向**：
- 在 `fc41d_ble.h` 增加 `fc41dBleSend(device, data, len)` → 内部构建 `AT+QBLEGATTSNTFY=...`。
- 在 `fc41d_wifi.h` 增加 `fc41dWifiSend(device, linkId, data, len)` → 内部构建 `AT+QISEND=...`。
- 需要处理 prompt 模式（`>`），利用 flowparser 的 `needPrompt` 机制。

### 1.6 WiFi STA 断连无自动重连

`FC41D_WIFI_STATE_STA_DISCONNECTED` 是终态，状态机不会触发重新连接。BLE 断连后会自动回到 ADV_START 重新广播，WiFi 行为不一致。

**影响**：WiFi STA 断开后需要上层干预才能重连。

**修复方向**：
- 增加 `bool autoReconnect` 配置项到 `stFc41dWifiCfg`。
- STA_DISCONNECTED 在 `autoReconnect` 为 true 时回退到 STA_CONNECTING，并增加重连间隔/次数限制。

### 1.7 无线程安全标注

`stFc41dCtx` 上没有互斥保护。当前项目在单任务中 poll 是安全的，但 core 作为可复用代码缺少约束说明。

**修复方向**：
- 在 `fc41d.md` 和 `fc41d_assembly.h` 中明确标注"single-task constraint"。
- 可选：增加 `fc41dPlatformLock/Unlock` optional hook，供多任务场景使用。

## 2. 改进优先级

| 优先级 | 编号 | 项目 | 原因 |
| --- | --- | --- | --- |
| P0 | 1.1 | URC 匹配修复 | 已有 bug，可能导致运行时状态错乱 |
| P1 | 1.2 | 多行响应支持 | 阻塞版本查询等基本功能 |
| P1 | 1.3 | 命令序列引擎 | 限制了自动初始化流程的可用性 |
| P2 | 1.4 | mode 语义修正 | 误报但当前产品侧未依赖 mode |
| P2 | 1.5 | TX 发送封装 | 便利性改进，不阻塞功能 |
| P2 | 1.6 | WiFi 自动重连 | 当前产品 WiFi 未启用 |
| P3 | 1.7 | 线程安全标注 | 文档补充 |

## 3. 改进架构参考

对于 AT modem 类模块，推荐三层模型：

```
┌──────────────────────────────┐
│  Feature State Machine       │  fc41d_ble.c / fc41d_wifi.c
│  命令序列引擎 + 重连策略      │
├──────────────────────────────┤
│  AT Session                  │  fc41d.c (exec 系列)
│  命令提交、超时、结果映射      │
├──────────────────────────────┤
│  AT Transport                │  flowparser_stream
│  字节收发、行分割、URC 路由    │
└──────────────────────────────┘
```

当前 Transport 和 Session 层已完成，Feature 层是改进重点。核心变更集中在：

1. **命令序列引擎**（影响 `fc41d_priv.h` + `fc41d.c` + `fc41d_ble.c` + `fc41d_wifi.c`）
2. **URC 精确匹配**（影响 `fc41d_ble.c` + `fc41d_wifi.c`）
3. **多行响应回调**（影响 `fc41d_base.h` + `fc41d.c`）

改动不涉及 `fc41d_assembly.h` 的 hook 签名，平台侧代码无需同步修改。
