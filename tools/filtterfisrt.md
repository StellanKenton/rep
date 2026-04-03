# filtterfisrt 说明

## 文件概览
`filtterfisrt.h` 与 `filtterfisrt.c` 共同实现一阶离散滤波器，以及控制环节使用的简化一阶滤波器。

模块主要提供：
- 标准一阶滤波对象 `Filter1stOrdObj`
- 控制用滤波对象 `Filter1stOrdForCtrlObj`
- 滤波器系数读写接口
- 状态量读写接口
- 初始化、复位和单步更新接口

## 对外接口与实现职责
头文件负责定义对象结构和公开接口，实现文件负责完成差分方程计算与状态推进。

实现重点包括：
- 一阶滤波器对象的系数访问函数
- 一阶滤波器状态访问函数
- 标准差分方程更新逻辑
- 控制用简化一阶滤波器的参数设置、复位和更新逻辑

## 如何使用
### 1. 使用标准一阶滤波器

```c
Filter1stOrdObj filter;
Filter1stOrdInit(&filter, b0, b1, a1);
float y = Filter1stOrdRun(&filter, input);
```

如果只需要指定形式的差分方程，也可以使用：
- `Filter1stOrdRunForm0`
- `Filter1stOrdRun`

### 2. 使用控制用一阶滤波器

```c
Filter1stOrdForCtrlObj ctrlFilter;
Filter1stOrdForCtrlInit(&ctrlFilter);
Filter1stOrdForCtrlParamSet(&ctrlFilter, coeff, initValue);
float y = Filter1stOrdForCtrlUpdate(&ctrlFilter, input);
```

如需清零内部状态，可调用：

```c
Filter1stOrdForCtrlReset(&ctrlFilter, initValue);
```

## 适用场景
- 传感器信号一阶低通
- 控制量平滑处理
- 执行器输入去抖和缓变处理

## 实现特点
- 对空指针做了基础保护
- 每次更新后会自动刷新历史输入和历史输出
- 控制用对象适合固定周期控制任务内调用