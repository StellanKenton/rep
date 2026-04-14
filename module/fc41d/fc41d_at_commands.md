# FC41D AT 指令说明

本文基于在线检索到的 Quectel 官方手册整理。

- 文档来源：Quectel FC41D AT Commands Manual V1.0
- 手册日期：2022-09-27
- 官方 PDF：https://quectel.com/content/uploads/2024/02/Quectel_FC41D_AT_Commands_Manual_V1.0-1.pdf
- 本地模块对应实现：`fc41d_at.c` 中已建立完整命令目录，覆盖手册目录中的全部 Wi-Fi、BLE、TCP/UDP、SSL、MQTT、HTTP(S) AT 指令。

## 1. 模块接口约定

- 旧接口 `eFc41dAtCmd` + `fc41dExecAtCmd()` 继续保留，只覆盖当前工程里常用的少量“可直接执行”命令。
- 新接口 `eFc41dAtCatalogCmd` + `fc41dAtGetCmdInfo*()` 提供完整命令目录。
- 新接口 `fc41dAtBuildExecCmd()`、`fc41dAtBuildQueryCmd()`、`fc41dAtBuildTestCmd()`、`fc41dAtBuildSetCmd()` 用于统一构造命令文本。
- BLE 现有便捷函数 `fc41dAtBuildBleNameCmd()`、`fc41dAtBuildBleGattServiceCmd()`、`fc41dAtBuildBleGattCharCmd()`、`fc41dAtBuildBleAdvParamCmd()`、`fc41dAtBuildBleAdvDataCmd()` 仍保留，声明收敛到 `fc41d_ble.h`，实现放在 `fc41d_ble.c`，内部已对齐到完整命令目录。

## 2. 一般命令

| 命令 | 说明 | 常见构造 |
| --- | --- | --- |
| `AT` | AT 测试命令，用于检查模块在线 | `fc41dAtBuildExecCmd(..., FC41D_AT_CATALOG_CMD_AT)` |

## 3. Wi-Fi 相关命令

| 命令 | 说明 | 常见构造 |
| --- | --- | --- |
| `AT+QRST` | 重启模块 | 执行型 |
| `AT+QVERSION` | 查询固件版本 | 执行型 |
| `AT+QECHO` | 开关串口回显 | 设置、查询、测试 |
| `AT+QURCCFG` | 开关 URC 上报 | 设置、查询、测试 |
| `AT+QPING` | Ping 外部 IP | 设置、执行 |
| `AT+QGETIP` | 查询 IP 信息 | 执行型 |
| `AT+QSETBAND` | 配置串口波特率 | 设置、查询、测试 |
| `AT+QWLANOTA` | 启动 OTA | 设置、执行 |
| `AT+QLOWPOWER` | 进入低功耗模式 | 设置、执行 |
| `AT+QDEEPSLEEP` | 进入深度睡眠模式 | 设置、执行 |
| `AT+QWLMAC` | 查询 MAC 地址 | 查询型 |
| `AT+QAIRKISS` | 开关 AirKiss | 设置、查询、测试 |
| `AT+QSTAST` | 查询 STA 模式状态 | 执行型 |
| `AT+QSTADHCP` | 配置 STA DHCP | 设置、查询、测试 |
| `AT+QSTADHCPDEF` | 配置并保存 STA DHCP | 设置、查询、测试 |
| `AT+QSTASTATIC` | 配置 STA 静态 IP | 设置、查询、测试 |
| `AT+QSTASTOP` | 关闭 STA 模式 | 执行型 |
| `AT+QSOFTAP` | 开启 AP 模式 | 设置、执行 |
| `AT+QAPSTATE` | 查询 AP 模式状态 | 执行型 |
| `AT+QAPSTATIC` | 配置 AP 静态 IP | 设置、查询、测试 |
| `AT+QSOFTAPSTOP` | 关闭 AP 模式 | 执行型 |
| `AT+QSTAAPINFO` | 连接热点 | 设置型 |
| `AT+QSTAAPINFODEF` | 连接并保存热点信息 | 设置型 |
| `AT+QGETWIFISTATE` | 查询当前已连接热点 | 执行型 |
| `AT+QWSCAN` | 扫描热点信息 | 执行型 |
| `AT+QWEBCFG` | 开关 Web 配网 | 设置、查询、测试 |

## 4. BLE 相关命令

| 命令 | 说明 | 常见构造 |
| --- | --- | --- |
| `AT+QBLEINIT` | 初始化 BLE 服务 | 设置型 |
| `AT+QBLEADDR` | 查询 BLE 地址 | 查询型 |
| `AT+QBLENAME` | 设置 BLE 名称 | `fc41dAtBuildBleNameCmd()` |
| `AT+QBLEADVPARAM` | 配置 BLE 广播参数 | `fc41dAtBuildBleAdvParamCmd()` |
| `AT+QBLEADVDATA` | 配置 BLE 广播数据 | `fc41dAtBuildBleAdvDataCmd()` |
| `AT+QBLEGATTSSRV` | 建立 BLE 服务 | `fc41dAtBuildBleGattServiceCmd()` |
| `AT+QBLEGATTSCHAR` | 配置 BLE 特征 UUID | `fc41dAtBuildBleGattCharCmd()` |
| `AT+QBLEADVSTART` | 启动 BLE 广播 | 执行型 |
| `AT+QBLEADVSTOP` | 停止 BLE 广播 | 执行型 |
| `AT+QBLEGATTSNTFY` | 发送 GATT 数据 | 设置型 |
| `AT+QBLESCAN` | 开始或停止 BLE 扫描 | 设置型 |
| `AT+QBLESCANPARAM` | 配置 BLE 扫描参数 | 设置、查询、测试 |
| `AT+QBLECONN` | 连接 BLE 外设 | 设置型 |
| `AT+QBLECONNPARAM` | 配置 BLE 连接参数 | 设置、查询、测试 |
| `AT+QBLECFGMTU` | 配置 BLE MTU | 设置、查询、测试 |
| `AT+QBLEGATTCNTFCFG` | 开关 Notification | 设置型 |
| `AT+QBLEGATTCWR` | 发送 BLE 中心端数据 | 设置型 |
| `AT+QBLEGATTCRD` | 读取 BLE 中心端数据 | 设置型 |
| `AT+QBLEDISCONN` | 断开 BLE 连接 | 执行型或设置型 |
| `AT+QBLESTAT` | 查询 BLE 状态 | 执行型 |

## 5. TCP/UDP 相关命令

| 命令 | 说明 | 常见构造 |
| --- | --- | --- |
| `AT+QICFG` | 配置 TCP/UDP socket 可选参数 | 设置、查询、测试 |
| `AT+QIOPEN` | 打开 TCP/UDP socket | 设置型 |
| `AT+QISTATE` | 查询 TCP/UDP socket 状态 | 执行型 |
| `AT+QISEND` | 发送 TCP/UDP 数据 | 设置型 |
| `AT+QIRD` | 读取 TCP/UDP 接收数据 | 设置型 |
| `AT+QIACCEPT` | 接受或拒绝来连 | 设置型 |
| `AT+QISWTMD` | 切换数据访问模式 | 设置型 |
| `AT+QICLOSE` | 关闭 TCP/UDP socket | 设置型 |
| `AT+QIGETERROR` | 查询 TCP/UDP 结果码 | 执行型 |
| `ATO` | 进入透传模式 | 执行型 |
| `+++` | 退出透传模式 | 特殊转义序列 |

## 6. SSL 相关命令

| 命令 | 说明 | 常见构造 |
| --- | --- | --- |
| `AT+QSSLCFG` | 配置 SSL 上下文参数 | 设置、查询、测试 |
| `AT+QSSLCERT` | 上传、下载、删除证书 | 设置型 |
| `AT+QSSLOPEN` | 打开 SSL 客户端 | 设置型 |
| `AT+QSSLSEND` | 发送 SSL 数据 | 设置型 |
| `AT+QSSLRECV` | 读取 SSL 接收数据 | 设置型 |
| `AT+QSSLSTATE` | 查询 SSL 客户端状态 | 执行型 |
| `AT+QSSLCLOSE` | 关闭 SSL 客户端 | 设置型 |

## 7. MQTT 相关命令

| 命令 | 说明 | 常见构造 |
| --- | --- | --- |
| `AT+QMTCFG` | 配置 MQTT 客户端可选参数 | 设置、查询、测试 |
| `AT+QMTOPEN` | 打开 MQTT 会话 | 设置型 |
| `AT+QMTCLOSE` | 关闭 MQTT 会话 | 设置型 |
| `AT+QMTCONN` | 连接 MQTT 服务器 | 设置型 |
| `AT+QMTDISC` | 断开 MQTT 服务器连接 | 设置型 |
| `AT+QMTSUB` | 订阅主题 | 设置型 |
| `AT+QMTUNS` | 取消订阅主题 | 设置型 |
| `AT+QMTPUB` | 发布 MQTT 消息 | 设置型 |
| `AT+QMTRECV` | 读取 MQTT 下行消息 | 设置型 |

## 8. HTTP(S) 相关命令

| 命令 | 说明 | 常见构造 |
| --- | --- | --- |
| `AT+QHTTPCFG` | 配置 HTTP(S) 客户端参数 | 设置、查询、测试 |
| `AT+QHTTPGET` | 发送 HTTP(S) GET 请求 | 设置型 |
| `AT+QHTTPPOST` | 发送 HTTP(S) POST 请求 | 设置型 |
| `AT+QHTTPPUT` | 发送 HTTP(S) PUT 请求 | 设置型 |
| `AT+QHTTPREAD` | 读取 HTTP(S) 响应数据 | 设置型 |

## 9. 当前工程最常用的一组 BLE 初始化命令

当前工程原始示例里实际使用到的一组 BLE 初始化流程如下：

1. `AT+QRST`
2. `AT+QSTASTOP`
3. `AT+QBLEINIT=2`
4. `AT+QBLENAME=...`
5. `AT+QBLEGATTSSRV=...`
6. `AT+QBLEGATTSCHAR=...`
7. `AT+QBLEADVPARAM=...`
8. `AT+QBLEADVDATA=...`
9. `AT+QBLEADDR?`
10. `AT+QVERSION`
11. `AT+QBLEADVSTART`

这部分仍然由旧接口和 BLE 便捷构造函数直接支持，便于继续兼容现有工程代码。