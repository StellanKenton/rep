# VS Code Portable Mac

这个目录是 macOS 下可复用的 VS Code 开发环境打包目录。

目标是把下面这套能力抽成“少量项目差异信息 + 一个固定脚本”：

- CMake 配置、编译、重编译、清理
- J-Link 烧录和复位
- Cortex-Debug + J-Link 调试
- RTT 单终端交互
- VS Code 底部按钮显示和功能

## 目录内容

- `deploy.sh`: 固定入口脚本，优先读取同目录下的 `project.json`
- `deploy.py`: 实际部署逻辑
- `project.template.json`: 新项目配置模板
- `project.cprsensorboot.json`: 当前工程的示例配置

## 下一次迁移到新项目时怎么用

1. 复制整个 `vscode_protable_mac` 目录到任意方便维护的位置。
2. 复制 `project.template.json` 为 `project.json`。
3. 只填写新项目和当前项目不同的必要信息。
4. 在终端执行 `./deploy.sh`。
5. 打开目标工程，安装推荐扩展。

## 最少需要填写的信息

- `targetWorkspace`: 目标工程根目录
- `projectName`: 项目标识，会体现在调试配置和底部按钮名称里
- `build.artifactName`: ELF/HEX 产物名前缀
- `build.toolchainFile`: 工程里的 CMake toolchain 文件相对路径
- `build.armToolchainBinDir`: Arm GNU Toolchain 的 bin 目录
- `jlink.device`: J-Link 识别的芯片名

其他字段都有默认值，只有项目差异明显时再改。

## 部署后会生成什么

脚本会直接写入目标工程：

- `.vscode/extensions.json`
- `.vscode/settings.json`
- `.vscode/tasks.json`
- `.vscode/launch.json`
- `.vscode/scripts/jlink_flash.sh`
- `.vscode/scripts/jlink_reset.sh`
- `.vscode/scripts/jlink_rtt_console.sh`
- `.vscode/scripts/jlink_rtt_server.sh`
- `.vscode/scripts/jlink_rtt_monitor.sh`
- `.vscode/scripts/jlink_write_rtt.sh`

## 固定入口示例

```sh
cd User/rep/scripts/vscode_protable_mac
cp project.template.json project.json
./deploy.sh
```

如果想显式指定配置文件或目标目录：

```sh
./deploy.sh --config ./project.cprsensorboot.json
./deploy.sh --config ./project.json --target /path/to/project
```

## 依赖

- VS Code 扩展：`ms-vscode.cmake-tools`、`marus25.cortex-debug`、`usernamehw.commands`、`seunlanlege.action-buttons`
- 主机工具：`cmake`、`ninja`、`JLinkExe`、`JLinkGDBServer`
- 调试工具链：Arm GNU Toolchain
- 终端工具：`nc`、`lsof`

## 说明

- 部署脚本会覆盖目标工程中同名的 `.vscode` 文件和 `.vscode/scripts` 脚本。
- 这套方案把“固定逻辑”和“项目差异参数”分开了；后续迁移只需要改 `project.json`，不需要手工改 `.vscode`。