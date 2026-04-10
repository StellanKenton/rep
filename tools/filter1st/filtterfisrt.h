/************************************************************************************
* @file     : filtterfisrt.h
* @brief    : 
* @details  : 
* @author   : 
* @date     : 
* @version  : 
* @copyright: Copyright (c) 2050
***********************************************************************************/
/**
 * @file filtterfisrt.h
 * @brief 一阶滤波算法对外接口。
 *
 * 该文件定义了一阶离散滤波器对象、控制用简化滤波器对象，以及
 * 对应的初始化、系数访问、状态访问和运行接口。
 */
#ifndef _UNIT_ALGO_1ST_ORD_FILTER_H_
#define _UNIT_ALGO_1ST_ORD_FILTER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"

/** 一阶 IIR 滤波器对象。 */
typedef struct _FILTER_1ST_ORD_OBJ_
{    
  float  a1;
  float  b0;
  float  b1;
  float  x1;
  float  y1;
} Filter1stOrdObj ;

/** 一阶滤波器句柄。 */
typedef struct _FILTER_1ST_ORD_OBJ_ *Filter1stOrdHandle ;

/** 控制环节使用的一阶简化滤波器对象。 */
typedef struct 
{
  float m_Gian ;
  float m_Xn1 ;
  float m_Yn ;
  float m_Yn1 ;
} Filter1stOrdForCtrlObj ;

/** 读取分母系数 a1。 */
float Filter1stOrdGet_a1(Filter1stOrdHandle handle) ;


/** 读取分子系数 b0。 */
float Filter1stOrdGet_b0(Filter1stOrdHandle handle) ;

/** 读取分子系数 b1。 */
float Filter1stOrdGet_b1(Filter1stOrdHandle handle) ;

/** 读取上一拍输入 x1。 */
float Filter1stOrdGet_x1(Filter1stOrdHandle handle) ;


/** 读取上一拍输出 y1。 */
float Filter1stOrdGet_y1(Filter1stOrdHandle handle) ;

/** 读取分母系数集合。 */
extern void Filter1stOrdGetDenCoeffs(Filter1stOrdHandle handle, float *pa1) ;

/** 读取一阶滤波器状态量。 */
extern void Filter1stOrdGetInitialConditions(Filter1stOrdHandle handle, float *px1, float *py1) ;

/** 读取分子系数集合。 */
extern void Filter1stOrdGetNumCoeffs(Filter1stOrdHandle handle, float *pb0, float *pb1) ;
 
/** 初始化一阶滤波器系数与状态。 */
extern void Filter1stOrdInit(Filter1stOrdHandle handle, float b0, float b1, float a1) ;

/** 按完整一阶差分方程执行一次滤波。 */
float Filter1stOrdRun(Filter1stOrdHandle handle, const float inputValue) ;

/** 按简化一阶差分方程执行一次滤波。 */
float Filter1stOrdRunForm0(Filter1stOrdHandle handle, const float inputValue) ;

/** 写入分母系数 a1。 */
void Filter1stOrdSet_a1(Filter1stOrdHandle handle,const float a1) ;

/** 写入分子系数 b0。 */
void Filter1stOrdSet_b0(Filter1stOrdHandle handle, const float b0) ;

/** 写入分子系数 b1。 */
void Filter1stOrdSet_b1(Filter1stOrdHandle handle, const float b1) ;

/** 写入上一拍输入 x1。 */
void Filter1stOrdSet_x1(Filter1stOrdHandle handle, const float x1) ;

/** 写入上一拍输出 y1。 */
void Filter1stOrdSet_y1(Filter1stOrdHandle handle, const float y1) ;

/** 批量写入分母系数。 */
extern void Filter1stOrdSetDenCoeffs(Filter1stOrdHandle handle,const float a1) ;

/** 批量写入一阶滤波器状态。 */
extern void Filter1stOrdSetInitialConditions(Filter1stOrdHandle handle, const float x1, const float y1) ;

/** 批量写入分子系数。 */
extern void Filter1stOrdSetNumCoeffs(Filter1stOrdHandle handle, const float b0, const float b1) ;

/** 初始化控制用一阶滤波器。 */
void Filter1stOrdForCtrlInit(Filter1stOrdForCtrlObj *pxHand, float fGain) ;

/** 更新控制用一阶滤波器增益。 */
void Filter1stOrdForCtrlParamSet(Filter1stOrdForCtrlObj *pxHand, float fGain) ;

/** 清零控制用一阶滤波器状态。 */
void Filter1stOrdForCtrlReset(Filter1stOrdForCtrlObj *pxHand) ;

/** 执行控制用一阶滤波器更新。 */
float Filter1stOrdForCtrlUpdate(Filter1stOrdForCtrlObj *pxHand, float fNewValue) ;


#ifdef __cplusplus
}
#endif

#endif
/**************************End of file********************************/
