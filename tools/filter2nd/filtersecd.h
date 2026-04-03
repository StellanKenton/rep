/**
 * @file filtersecd.h
 * @brief 二阶滤波算法对外接口。
 *
 * 该文件定义二阶离散滤波器对象，并提供系数访问、状态访问、
 * 初始化和多种差分方程执行接口。
 */
#ifndef _UNIT_ALGO_2ND_ORD_FILTER_H_
#define _UNIT_ALGO_2ND_ORD_FILTER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "stddef.h"
#include "stdint.h"

/** 二阶 IIR 滤波器对象。 */
typedef struct _FILTER_2ND_ORD_OBJ_
{
    float a1;
    float a2;
    float b0;
    float b1;
    float b2;
    float x1;
    float x2;
    float y1;
    float y2;
} Filter2ndOrdObj ;

/** 二阶滤波器句柄。 */
typedef struct _FILTER_2ND_ORD_OBJ_ *Filter2ndOrdHandle ;

/** 读取分母系数 a1。 */
float Filter2ndOrdGet_a1(Filter2ndOrdHandle handle) ;

/** 读取分母系数 a2。 */
float Filter2ndOrdGet_a2(Filter2ndOrdHandle handle) ;

/** 读取分子系数 b0。 */
float Filter2ndOrdGet_b0(Filter2ndOrdHandle handle) ;

/** 读取分子系数 b1。 */
float Filter2ndOrdGet_b1(Filter2ndOrdHandle handle) ;

/** 读取分子系数 b2。 */
float Filter2ndOrdGet_b2(Filter2ndOrdHandle handle) ;

/** 读取上一拍输入 x1。 */
float Filter2ndOrdGet_x1(Filter2ndOrdHandle handle) ;

/** 读取上两拍输入 x2。 */
float Filter2ndOrdGet_x2(Filter2ndOrdHandle handle) ;

/** 读取上一拍输出 y1。 */
float Filter2ndOrdGet_y1(Filter2ndOrdHandle handle) ;

/** 读取上两拍输出 y2。 */
float Filter2ndOrdGet_y2(Filter2ndOrdHandle handle) ;

/** 按完整二阶差分方程执行一次滤波。 */
float Filter2ndOrdRun(Filter2ndOrdHandle handle, const float inputValue) ;

/** 按只含 b0 的形式执行一次滤波。 */
float Filter2ndOrdRunForm0(Filter2ndOrdHandle handle, const float inputValue) ;

/** 按含 b0、b1 的形式执行一次滤波。 */
float Filter2ndOrdRunForm1(Filter2ndOrdHandle handle, const float inputValue) ;

/** 按含 b0、b1、b2 的完整形式执行滤波。 */
float Filter2ndOrdRunFull(Filter2ndOrdHandle handle, const float inputValue) ;

/** 写入分母系数 a1。 */
void Filter2ndOrdSet_a1(Filter2ndOrdHandle handle, const float a1) ;

/** 写入分母系数 a2。 */
void Filter2ndOrdSet_a2(Filter2ndOrdHandle handle, const float a2) ;

/** 写入分子系数 b0。 */
void Filter2ndOrdSet_b0(Filter2ndOrdHandle handle, const float b0) ;

/** 写入分子系数 b1。 */
void Filter2ndOrdSet_b1(Filter2ndOrdHandle handle, const float b1) ;

/** 写入分子系数 b2。 */
void Filter2ndOrdSet_b2(Filter2ndOrdHandle handle, const float b2) ;

/** 写入上一拍输入 x1。 */
void Filter2ndOrdSet_x1(Filter2ndOrdHandle handle, const float x1) ;

/** 写入上两拍输入 x2。 */
void Filter2ndOrdSet_x2(Filter2ndOrdHandle handle, const float x2) ;

/** 写入上一拍输出 y1。 */
void Filter2ndOrdSet_y1(Filter2ndOrdHandle handle,const float y1) ;

/** 写入上两拍输出 y2。 */
void Filter2ndOrdSet_y2(Filter2ndOrdHandle handle, const float y2) ;

/** 批量读取分母系数。 */
void Filter2ndOrdGetDenCoeffs(Filter2ndOrdHandle handle,float *pa1,float *pa2) ;

/** 批量读取二阶滤波器状态。 */
void Filter2ndOrdGetInitialConditions(Filter2ndOrdHandle handle, float *px1,
        float *px2, float *py1,float *py2) ;

/** 批量读取分子系数。 */
void Filter2ndOrdGetNumCoeffs(Filter2ndOrdHandle handle, float *pb0, float *pb1, float *pb2) ;

/** 初始化二阶滤波器系数与状态。 */
void Filter2ndOrdInit(Filter2ndOrdHandle handle, float b0, float b1, float b2, float a1, float a2) ;

/** 批量写入分母系数。 */
void Filter2ndOrdSetDenCoeffs(Filter2ndOrdHandle handle, const float a1, const float a2) ;

/** 批量写入二阶滤波器状态。 */
void Filter2ndOrdSetInitialConditions(Filter2ndOrdHandle handle, const float x1,
        const float x2, const float y1, const float y2) ;

/** 批量写入分子系数。 */
void Filter2ndOrdSetNumCoeffs(Filter2ndOrdHandle handle, const float b0, const float b1, const float b2) ;

#ifdef __cplusplus
}
#endif

#endif
