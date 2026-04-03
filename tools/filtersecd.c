/**
 * @file filtersecd.c
 * @brief 二阶滤波算法实现。
 *
 * 该文件实现二阶离散滤波器的系数访问、状态管理和多种差分方程更新逻辑。
 */

#include "filtersecd.h"

#include <stddef.h>

/* 二阶滤波器参数读取接口。 */

float Filter2ndOrdGet_a1(Filter2ndOrdHandle handle)
{
    return (handle != NULL) ? handle->a1 : 0.0f;
}

float Filter2ndOrdGet_a2(Filter2ndOrdHandle handle)
{
    return (handle != NULL) ? handle->a2 : 0.0f;
}

float Filter2ndOrdGet_b0(Filter2ndOrdHandle handle)
{
    return (handle != NULL) ? handle->b0 : 0.0f;
}

float Filter2ndOrdGet_b1(Filter2ndOrdHandle handle)
{
    return (handle != NULL) ? handle->b1 : 0.0f;
}

float Filter2ndOrdGet_b2(Filter2ndOrdHandle handle)
{
    return (handle != NULL) ? handle->b2 : 0.0f;
}

float Filter2ndOrdGet_x1(Filter2ndOrdHandle handle)
{
    return (handle != NULL) ? handle->x1 : 0.0f;
}

float Filter2ndOrdGet_x2(Filter2ndOrdHandle handle)
{
    return (handle != NULL) ? handle->x2 : 0.0f;
}

float Filter2ndOrdGet_y1(Filter2ndOrdHandle handle)
{
    return (handle != NULL) ? handle->y1 : 0.0f;
}

float Filter2ndOrdGet_y2(Filter2ndOrdHandle handle)
{
    return (handle != NULL) ? handle->y2 : 0.0f;
}

/* 二阶滤波器运行接口。 */
float Filter2ndOrdRun(Filter2ndOrdHandle handle, const float inputValue)
{
    return Filter2ndOrdRunFull(handle, inputValue);
}

float Filter2ndOrdRunForm0(Filter2ndOrdHandle handle, const float inputValue)
{
    float output;

    if (handle == NULL)
    {
        return inputValue;
    }

    output = (handle->b0 * inputValue) - (handle->a1 * handle->y1) - (handle->a2 * handle->y2);
    handle->x2 = handle->x1;
    handle->x1 = inputValue;
    handle->y2 = handle->y1;
    handle->y1 = output;

    return output;
}

float Filter2ndOrdRunForm1(Filter2ndOrdHandle handle, const float inputValue)
{
    float output;

    if (handle == NULL)
    {
        return inputValue;
    }

    output = (handle->b0 * inputValue) + (handle->b1 * handle->x1) - (handle->a1 * handle->y1) - (handle->a2 * handle->y2);
    handle->x2 = handle->x1;
    handle->x1 = inputValue;
    handle->y2 = handle->y1;
    handle->y1 = output;

    return output;
}

float Filter2ndOrdRunFull(Filter2ndOrdHandle handle, const float inputValue)
{
    float output;

    if (handle == NULL)
    {
        return inputValue;
    }

    output = (handle->b0 * inputValue) + (handle->b1 * handle->x1) + (handle->b2 * handle->x2)
           - (handle->a1 * handle->y1) - (handle->a2 * handle->y2);

    handle->x2 = handle->x1;
    handle->x1 = inputValue;
    handle->y2 = handle->y1;
    handle->y1 = output;

    return output;
}

/* 二阶滤波器参数写入接口。 */
void Filter2ndOrdSet_a1(Filter2ndOrdHandle handle, const float a1)
{
    if (handle != NULL)
    {
        handle->a1 = a1;
    }
}

void Filter2ndOrdSet_a2(Filter2ndOrdHandle handle, const float a2)
{
    if (handle != NULL)
    {
        handle->a2 = a2;
    }
}

void Filter2ndOrdSet_b0(Filter2ndOrdHandle handle, const float b0)
{
    if (handle != NULL)
    {
        handle->b0 = b0;
    }
}

void Filter2ndOrdSet_b1(Filter2ndOrdHandle handle, const float b1)
{
    if (handle != NULL)
    {
        handle->b1 = b1;
    }
}

void Filter2ndOrdSet_b2(Filter2ndOrdHandle handle, const float b2)
{
    if (handle != NULL)
    {
        handle->b2 = b2;
    }
}

void Filter2ndOrdSet_x1(Filter2ndOrdHandle handle, const float x1)
{
    if (handle != NULL)
    {
        handle->x1 = x1;
    }
}

void Filter2ndOrdSet_x2(Filter2ndOrdHandle handle, const float x2)
{
    if (handle != NULL)
    {
        handle->x2 = x2;
    }
}

void Filter2ndOrdSet_y1(Filter2ndOrdHandle handle, const float y1)
{
    if (handle != NULL)
    {
        handle->y1 = y1;
    }
}

void Filter2ndOrdSet_y2(Filter2ndOrdHandle handle, const float y2)
{
    if (handle != NULL)
    {
        handle->y2 = y2;
    }
}

/* 二阶滤波器批量配置接口。 */
void Filter2ndOrdGetDenCoeffs(Filter2ndOrdHandle handle, float *pa1, float *pa2)
{
    if (handle == NULL)
    {
        return;
    }

    if (pa1 != NULL)
    {
        *pa1 = handle->a1;
    }

    if (pa2 != NULL)
    {
        *pa2 = handle->a2;
    }
}

void Filter2ndOrdGetInitialConditions(Filter2ndOrdHandle handle, float *px1, float *px2, float *py1, float *py2)
{
    if (handle == NULL)
    {
        return;
    }

    if (px1 != NULL)
    {
        *px1 = handle->x1;
    }

    if (px2 != NULL)
    {
        *px2 = handle->x2;
    }

    if (py1 != NULL)
    {
        *py1 = handle->y1;
    }

    if (py2 != NULL)
    {
        *py2 = handle->y2;
    }
}

void Filter2ndOrdGetNumCoeffs(Filter2ndOrdHandle handle, float *pb0, float *pb1, float *pb2)
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

    if (pb2 != NULL)
    {
        *pb2 = handle->b2;
    }
}

void Filter2ndOrdInit(Filter2ndOrdHandle handle, float b0, float b1, float b2, float a1, float a2)
{
    if (handle == NULL)
    {
        return;
    }

    handle->a1 = a1;
    handle->a2 = a2;
    handle->b0 = b0;
    handle->b1 = b1;
    handle->b2 = b2;
    handle->x1 = 0.0f;
    handle->x2 = 0.0f;
    handle->y1 = 0.0f;
    handle->y2 = 0.0f;
}

void Filter2ndOrdSetDenCoeffs(Filter2ndOrdHandle handle, const float a1, const float a2)
{
    if (handle == NULL)
    {
        return;
    }

    handle->a1 = a1;
    handle->a2 = a2;
}

void Filter2ndOrdSetInitialConditions(Filter2ndOrdHandle handle, const float x1, const float x2, const float y1, const float y2)
{
    if (handle == NULL)
    {
        return;
    }

    handle->x1 = x1;
    handle->x2 = x2;
    handle->y1 = y1;
    handle->y2 = y2;
}

void Filter2ndOrdSetNumCoeffs(Filter2ndOrdHandle handle, const float b0, const float b1, const float b2)
{
    if (handle == NULL)
    {
        return;
    }

    handle->b0 = b0;
    handle->b1 = b1;
    handle->b2 = b2;
}