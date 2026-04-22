---
doc_role: repo-lib
layer: parent
module: lib
status: active
portability: reusable
public_headers: []
core_files:
  - lib.md
port_files: []
debug_files: []
depends_on:
  - ../rule/rule.md
  - ../rule/map.md
forbidden_depends_on:
  - 在库目录中放项目设备 glue
  - 在库目录中放系统任务或 manager 逻辑
required_hooks: []
optional_hooks: []
common_utils: []
copy_minimal_set:
  - lib/
read_next:
  - littlefs/littlefs.md
  - fatfs/fatfs.md
---

# rep/lib 入口文档

这是当前目录的权威入口文档。

## 1. 目录定位

`rep/lib/` 用于存放第三方库本体，或少量与具体工程无关的轻量公共库源码。

这里放的是“库源码”和“库级配置头”，不是项目绑定层。

## 2. 当前子目录

| 目录 | 作用 | 入口 |
| --- | --- | --- |
| `littlefs/` | littlefs 第三方库源码和项目统一配置头 | `littlefs/littlefs.md` |
| `fatfs/` | FatFs 第三方库源码和通用头文件 | `fatfs/fatfs.md` |

## 3. 允许与禁止

允许放在 `rep/lib/` 的内容：

- 上游库 `.c/.h` 源码。
- 与当前工程无关、仅用于约束库编译行为的配置头。
- 说明上游源码边界和项目接入方式的主文档。

禁止放在 `rep/lib/` 的内容：

- 具体板级 `diskio.c`、SPI/QSPI/SDIO 设备绑定。
- RTOS 互斥、任务、manager、system 接线。
- 仅当前产品使用的示例命令、测试程序、调试脚本。

## 4. 接入原则

- 库本体可进入 `rep/lib/`，项目设备 glue 保持在 `User/` 或 `User/port/`。
- 如果库需要外部回调、块设备接口、OS 适配函数，应在文档里明确这些 hook 应落在哪一层。
- 若修改了参与编译的库文件路径，必须同步更新工程文件，不要保留旧路径兼容层。

## 5. 推荐阅读顺序

1. 先读本文件。
2. 再读目标库目录主文档。
3. 最后再看库头文件与源文件。