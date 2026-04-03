# filtersecd 说明

## 文件概览
`filtersecd.h` 与 `filtersecd.c` 共同实现二阶离散滤波器模块。

模块主要用于：
- 定义二阶滤波器对象 `Filter2ndOrdObj`
- 管理二阶滤波器的分子系数和分母系数
- 管理历史输入和历史输出状态
- 提供不同形式的二阶滤波更新函数

## 对外接口与实现职责
头文件定义对象和接口，实现文件完成参数访问、状态管理和差分方程计算。

实现内容包括：
- 二阶滤波器参数读取函数
- 二阶滤波器参数写入函数
- 历史输入输出状态管理
- 多种二阶差分方程更新方式

## 如何使用
### 1. 定义并初始化对象

```c
Filter2ndOrdObj filter;
Filter2ndOrdInit(&filter, b0, b1, b2, a1, a2);
```

### 2. 周期调用滤波更新
完整形式：

```c
float y = Filter2ndOrdRun(&filter, input);
```

如果你的模型只需要部分分子项，也可以调用：
- `Filter2ndOrdRunForm0`
- `Filter2ndOrdRunForm1`
- `Filter2ndOrdRunFull`

### 3. 动态修改参数或状态
可以在运行期间修改系数或历史量，例如：

```c
Filter2ndOrdSetNumCoeffs(&filter, b0, b1, b2);
Filter2ndOrdSetDenCoeffs(&filter, a1, a2);
Filter2ndOrdSetInitialConditions(&filter, x1, x2, y1, y2);
```

如果需要调试滤波内部状态，可以调用：
- `Filter2ndOrdGetNumCoeffs`
- `Filter2ndOrdGetDenCoeffs`
- `Filter2ndOrdGetInitialConditions`

## 适用场景
- 二阶低通滤波
- 二阶补偿环节
- 对一阶滤波不足的噪声信号进一步平滑

## 实现特点
- 对空句柄做了保护
- 更新输出后会自动推进历史状态
- 支持从简单形式到完整形式的二阶差分方程