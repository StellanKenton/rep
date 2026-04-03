/**
 * @file filtterfisrt.c
 * @brief 一阶滤波算法实现。
 *
 * 该文件实现一阶离散滤波器的系数管理、状态管理和滤波运算，
 * 同时提供一个控制环节使用的简化一阶滤波器。
 */

#include "filtterfisrt.h"

#include <stddef.h>

/* 一阶标准滤波器接口。 */

float Filter1stOrdGet_a1(Filter1stOrdHandle handle)
{
    return (handle != NULL) ? handle->a1 : 0.0f;
}

float Filter1stOrdGet_b0(Filter1stOrdHandle handle)
{
    return (handle != NULL) ? handle->b0 : 0.0f;
}

float Filter1stOrdGet_b1(Filter1stOrdHandle handle)
{
    return (handle != NULL) ? handle->b1 : 0.0f;
}

float Filter1stOrdGet_x1(Filter1stOrdHandle handle)
{
    return (handle != NULL) ? handle->x1 : 0.0f;
}

float Filter1stOrdGet_y1(Filter1stOrdHandle handle)
{
    return (handle != NULL) ? handle->y1 : 0.0f;
}

void Filter1stOrdGetDenCoeffs(Filter1stOrdHandle handle, float *pa1)
{
    if ((handle != NULL) && (pa1 != NULL))
    {
        *pa1 = handle->a1;
    }
}

void Filter1stOrdGetInitialConditions(Filter1stOrdHandle handle, float *px1, float *py1)
{
    if (handle == NULL)
    {
        return;
    }

    if (px1 != NULL)
    {
        *px1 = handle->x1;
    }

    if (py1 != NULL)
    {
        *py1 = handle->y1;
    }
}

void Filter1stOrdGetNumCoeffs(Filter1stOrdHandle handle, float *pb0, float *pb1)
{
    if (handle == NULL)
    {
        return;
    }

    if (pb0 != NULL)
    {
        *pb0 = handle->b0;
    }

    if (pb1 != NULL)
    {
        *pb1 = handle->b1;
    }
}

void Filter1stOrdInit(Filter1stOrdHandle handle, float b0, float b1, float a1)
{
    if (handle == NULL)
    {
        return;
    }

    handle->a1 = a1;
    handle->b0 = b0;
    handle->b1 = b1;
    handle->x1 = 0.0f;
    handle->y1 = 0.0f;
}

float Filter1stOrdRun(Filter1stOrdHandle handle, const float inputValue)
{
    float output;

    if (handle == NULL)
    {
        return inputValue;
    }

    output = (handle->b0 * inputValue) + (handle->b1 * handle->x1) - (handle->a1 * handle->y1);
    handle->x1 = inputValue;
    handle->y1 = output;

    return output;
}

float Filter1stOrdRunForm0(Filter1stOrdHandle handle, const float inputValue)
{
    float output;

    if (handle == NULL)
    {
        return inputValue;
    }

    output = (handle->b0 * inputValue) - (handle->a1 * handle->y1);
    handle->x1 = inputValue;
    handle->y1 = output;

    return output;
}

void Filter1stOrdSet_a1(Filter1stOrdHandle handle, const float a1)
{
    if (handle != NULL)
    {
        handle->a1 = a1;
    }
}

void Filter1stOrdSet_b0(Filter1stOrdHandle handle, const float b0)
{
    if (handle != NULL)
    {
        handle->b0 = b0;
    }
}

void Filter1stOrdSet_b1(Filter1stOrdHandle handle, const float b1)
{
    if (handle != NULL)
    {
        handle->b1 = b1;
    }
}

void Filter1stOrdSet_x1(Filter1stOrdHandle handle, const float x1)
{
    if (handle != NULL)
    {
        handle->x1 = x1;
    }
}

void Filter1stOrdSet_y1(Filter1stOrdHandle handle, const float y1)
{
    if (handle != NULL)
    {
        handle->y1 = y1;
    }
}

void Filter1stOrdSetDenCoeffs(Filter1stOrdHandle handle, const float a1)
{
    if (handle != NULL)
    {
        handle->a1 = a1;
    }
}

void Filter1stOrdSetInitialConditions(Filter1stOrdHandle handle, const float x1, const float y1)
{
    if (handle == NULL)
    {
        return;
    }

    handle->x1 = x1;
    handle->y1 = y1;
}

void Filter1stOrdSetNumCoeffs(Filter1stOrdHandle handle, const float b0, const float b1)
{
    if (handle == NULL)
    {
        return;
    }

    handle->b0 = b0;
    handle->b1 = b1;
}

/* 控制环节用简化滤波器接口。 */

void Filter1stOrdForCtrlInit(Filter1stOrdForCtrlObj *pxHand, float fGain)
{
    if (pxHand == NULL)
    {
        return;
    }

    pxHand->m_Gian = fGain;
    pxHand->m_Xn1 = 0.0f;
    pxHand->m_Yn = 0.0f;
    pxHand->m_Yn1 = 0.0f;
}

void Filter1stOrdForCtrlParamSet(Filter1stOrdForCtrlObj *pxHand, float fGain)
{
    if (pxHand != NULL)
    {
        pxHand->m_Gian = fGain;
    }
}

void Filter1stOrdForCtrlReset(Filter1stOrdForCtrlObj *pxHand)
{
    if (pxHand == NULL)
    {
        return;
    }

    pxHand->m_Xn1 = 0.0f;
    pxHand->m_Yn = 0.0f;
    pxHand->m_Yn1 = 0.0f;
}

float Filter1stOrdForCtrlUpdate(Filter1stOrdForCtrlObj *pxHand, float fNewValue)
{
    float output;

    if (pxHand == NULL)
    {
        return fNewValue;
    }

    output = (pxHand->m_Gian * fNewValue) + ((1.0f - pxHand->m_Gian) * pxHand->m_Yn1);
    pxHand->m_Xn1 = fNewValue;
    pxHand->m_Yn = output;
    pxHand->m_Yn1 = output;

    return output;
}