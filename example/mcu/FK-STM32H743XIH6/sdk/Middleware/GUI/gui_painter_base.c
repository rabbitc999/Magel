#include "gui.h"

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/**
 * @brief 绘制直线
 * @param deviceInfo 设备信息结构体
 * @param X0 起点X坐标
 * @param Y0 起点Y坐标
 * @param X1 终点X坐标
 * @param Y1 终点Y坐标
 * @param color 线条颜色
 * @note 使用Bresenham算法绘制直线
 */
void GUI_BasePainter_DrawLine(GUI_DeviceInfo_T *deviceInfo, uint16_t X0, uint16_t Y0, uint16_t X1, uint16_t Y1, uint32_t color)
{
    if (deviceInfo == NULL || deviceInfo->drawPointCallback == NULL) return;

    int16_t x, y, dx, dy, d, incrE, incrNE, temp;
    int16_t x0 = X0, y0 = Y0, x1 = X1, y1 = Y1;
    uint8_t yflag = 0, xyflag = 0;

    if (y0 == y1)  // 横线单独处理
    {
        /* 0号点X坐标大于1号点X坐标，则交换两点X坐标 */
        if (x0 > x1)
        {
            temp = x0;
            x0 = x1;
            x1 = temp;
        }

        /*遍历Y坐标*/
        for (uint16_t x = x0; x < x1; x++)
        {
            deviceInfo->drawPointCallback(deviceInfo, x, y0, color); /* 上边 */
        }
    }
    else if (x0 == x1)  // 竖线单独处理
    {
        /*0号点Y坐标大于1号点Y坐标，则交换两点Y坐标*/
        if (y0 > y1)
        {
            temp = y0;
            y0 = y1;
            y1 = temp;
        }

        /*遍历Y坐标*/
        for (uint16_t y = y0; y < y1; y++)
        {
            deviceInfo->drawPointCallback(deviceInfo, x0, y, color); /* 上边 */
        }
    }
    else  // 斜线
    {
        /* 使用Bresenham算法画直线，可以避免耗时的浮点运算，效率更高 */
        /* 参考文档：https://www.cs.montana.edu/courses/spring2009/425/dslectures/Bresenham.pdf */
        /* 参考教程：https://www.bilibili.com/video/BV1364y1d7Lo */

        if (x0 > x1)  // 0号点X坐标大于1号点X坐标
        {
            temp = x0;
            x0 = x1;
            x1 = temp;
            temp = y0;
            y0 = y1;
            y1 = temp;
        }

        if (y0 > y1)  // 0号点Y坐标大于1号点Y坐标
        {
            y0 = -y0;
            y1 = -y1;
            yflag = 1;
        }

        if (y1 - y0 > x1 - x0)  // 画线斜率大于1
        {
            temp = x0;
            x0 = y0;
            y0 = temp;
            temp = x1;
            x1 = y1;
            y1 = temp;
            xyflag = 1;
        }

        dx = x1 - x0;
        dy = y1 - y0;
        incrE = 2 * dy;
        incrNE = 2 * (dy - dx);
        d = 2 * dy - dx;
        x = x0;
        y = y0;

        if (yflag && xyflag)
        {
            deviceInfo->drawPointCallback(deviceInfo, y, -x, color);
        }
        else if (yflag)
        {
            deviceInfo->drawPointCallback(deviceInfo, x, -y, color);
        }
        else if (xyflag)
        {
            deviceInfo->drawPointCallback(deviceInfo, y, x, color);
        }
        else
        {
            deviceInfo->drawPointCallback(deviceInfo, x, y, color);
        }

        while (x < x1)  // 遍历X轴的每个点
        {
            x++;
            if (d < 0)  // 下一个点在当前点东方
            {
                d += incrE;
            }
            else  // 下一个点在当前点东北方
            {
                y++;
                d += incrNE;
            }

            /* 画每一个点，同时判断标志位，将坐标换回来 */
            if (yflag && xyflag)
            {
                deviceInfo->drawPointCallback(deviceInfo, y, -x, color);
            }
            else if (yflag)
            {
                deviceInfo->drawPointCallback(deviceInfo, x, -y, color);
            }
            else if (xyflag)
            {
                deviceInfo->drawPointCallback(deviceInfo, y, x, color);
            }
            else
            {
                deviceInfo->drawPointCallback(deviceInfo, x, y, color);
            }
        }
    }
}

/**
 * @brief 绘制矩形
 * @param deviceInfo 设备信息结构体
 * @param x 矩形左上角X坐标
 * @param y 矩形左上角Y坐标
 * @param Width 矩形宽度
 * @param Height 矩形高度
 * @param color 矩形颜色
 */
void GUI_BasePainter_DrawRectangle(GUI_DeviceInfo_T *deviceInfo, uint16_t x, uint16_t y, uint16_t Width, uint16_t Height, uint32_t color)
{
    if (deviceInfo == NULL) return;

    if (deviceInfo->fillMode == GUI_PAINTER_FILL_COLOR)
    {
        /* 填充矩形 */
        GUI_BasePainter_Fill(deviceInfo, x, y, Width, Height, color);
    }
    else
    {
        /* 绘制矩形边框 */
        if (deviceInfo->drawPointCallback == NULL) return;

        for (uint16_t i = 0; i < Width; i++)
        {
            deviceInfo->drawPointCallback(deviceInfo, x + i, y, color);              /* 上边 */
            deviceInfo->drawPointCallback(deviceInfo, x + i, y + Height - 1, color); /* 下边 */
        }

        for (uint16_t i = 1; i < Height - 1; i++)
        {
            deviceInfo->drawPointCallback(deviceInfo, x, y + i, color);             /* 左边 */
            deviceInfo->drawPointCallback(deviceInfo, x + Width - 1, y + i, color); /* 右边 */
        }
    }
}

/**
 * @brief 绘制正方形
 * @param deviceInfo 设备信息结构体
 * @param x 正方形左上角X坐标
 * @param y 正方形左上角Y坐标
 * @param size 正方形边长
 * @param color 正方形颜色
 */
void GUI_BasePainter_DrawSquare(GUI_DeviceInfo_T *deviceInfo, uint16_t x, uint16_t y, uint16_t size, uint32_t color)
{
    GUI_BasePainter_DrawRectangle(deviceInfo, x, y, size, size, color);
}

/**
 * @brief 绘制圆形
 * @param deviceInfo 设备信息结构体
 * @param x 圆心X坐标
 * @param y 圆心Y坐标
 * @param radius 圆半径
 * @param color 圆形颜色
 * @param fillMode 填充模式(填充或仅边框)
 * @note 使用中点圆算法绘制圆形
 */
void GUI_BasePainter_DrawCircle(GUI_DeviceInfo_T *deviceInfo, uint16_t x, uint16_t y, uint8_t radius, uint32_t color)
{
    if (deviceInfo == NULL || deviceInfo->drawPointCallback == NULL) return;

    if (deviceInfo->fillMode == GUI_PAINTER_FILL_COLOR)
    {
        /* 填充圆 */
        for (int16_t dy = -radius; dy <= radius; dy++)
        {
            for (int16_t dx = -radius; dx <= radius; dx++)
            {
                if (dx * dx + dy * dy <= radius * radius)
                {
                    deviceInfo->drawPointCallback(deviceInfo, x + dx, y + dy, color);
                }
            }
        }
    }
    else
    {
        /* 绘制圆边框 */
        int16_t x0 = 0;
        int16_t y0 = radius;
        int16_t d = 3 - 2 * radius;

        while (x0 <= y0)
        {
            deviceInfo->drawPointCallback(deviceInfo, x + x0, y + y0, color);
            deviceInfo->drawPointCallback(deviceInfo, x - x0, y + y0, color);
            deviceInfo->drawPointCallback(deviceInfo, x + x0, y - y0, color);
            deviceInfo->drawPointCallback(deviceInfo, x - x0, y - y0, color);
            deviceInfo->drawPointCallback(deviceInfo, x + y0, y + x0, color);
            deviceInfo->drawPointCallback(deviceInfo, x - y0, y + x0, color);
            deviceInfo->drawPointCallback(deviceInfo, x + y0, y - x0, color);
            deviceInfo->drawPointCallback(deviceInfo, x - y0, y - x0, color);

            if (d < 0)
            {
                d += 4 * x0 + 6;
            }
            else
            {
                d += 4 * (x0 - y0) + 10;
                y0--;
            }
            x0++;
        }
    }
}

/**
 * @brief 绘制椭圆
 * @param deviceInfo 设备信息结构体
 * @param X 椭圆中心X坐标
 * @param Y 椭圆中心Y坐标
 * @param A 椭圆长半轴
 * @param B 椭圆短半轴
 * @param Color 椭圆颜色
 */
void GUI_BasePainter_DrawEllipse(GUI_DeviceInfo_T *deviceInfo, uint16_t X, uint16_t Y, uint8_t A, uint8_t B, uint32_t Color)
{
    if (deviceInfo == NULL || deviceInfo->drawPointCallback == NULL) return;

    uint16_t x, y, j;
    uint16_t a = A, b = B;
    float d1, d2;

    /* 使用Bresenham算法画椭圆，可以避免部分耗时的浮点运算，效率更高 */
    /* 参考链接：https://blog.csdn.net/myf_666/article/details/128167392 */

    x = 0;
    y = b;
    d1 = b * b + a * a * (-b + 0.5);

    if (g_LCD.MonoFill == LCD_Cover)  // 指定椭圆填充
    {
        /* 遍历起始点Y坐标 */
        for (j = -y; j < y; j++)
        {
            /* 在指定区域画点，填充部分椭圆 */
            deviceInfo->drawPointCallback(deviceInfo, X, Y + j, Color);
            deviceInfo->drawPointCallback(deviceInfo, X, Y + j, Color);
        }
        GUI_BasePainter_Fill(deviceInfo, X, Y - y, X + 1, Y + y, Color);
    }

    /* 画椭圆弧的起始点 */
    deviceInfo->drawPointCallback(deviceInfo, X + x, Y + y, Color);
    deviceInfo->drawPointCallback(deviceInfo, X - x, Y - y, Color);
    deviceInfo->drawPointCallback(deviceInfo, X - x, Y + y, Color);
    deviceInfo->drawPointCallback(deviceInfo, X + x, Y - y, Color);

    /* 画椭圆中间部分 */
    while (b * b * (x + 1) < a * a * (y - 0.5))
    {
        if (d1 <= 0) /* 下一个点在当前点东方 */
        {
            d1 += b * b * (2 * x + 3);
        }
        else /* 下一个点在当前点东南方 */
        {
            d1 += b * b * (2 * x + 3) + a * a * (-2 * y + 2);
            y--;
        }
        x++;

        if (g_LCD.MonoFill == LCD_Cover) /* 指定椭圆填充 */
        {
            /* 遍历中间部分 */
            GUI_BasePainter_Fill(deviceInfo, X + x, Y - y, X + x + 1, Y + y, Color);
            GUI_BasePainter_Fill(deviceInfo, X - x, Y - y, X - x + 1, Y + y, Color);
        }

        /* 画椭圆中间部分圆弧 */
        deviceInfo->drawPointCallback(deviceInfo, X + x, Y + y, Color);
        deviceInfo->drawPointCallback(deviceInfo, X - x, Y - y, Color);
        deviceInfo->drawPointCallback(deviceInfo, X - x, Y + y, Color);
        deviceInfo->drawPointCallback(deviceInfo, X + x, Y - y, Color);
    }

    /* 画椭圆两侧部分 */
    d2 = b * b * (x + 0.5) * (x + 0.5) + a * a * (y - 1) * (y - 1) - a * a * b * b;

    while (y > 0)
    {
        if (d2 <= 0)  // 下一个点在当前点东方
        {
            d2 += b * b * (2 * x + 2) + a * a * (-2 * y + 3);
            x++;
        }
        else  // 下一个点在当前点东南方
        {
            d2 += a * a * (-2 * y + 3);
        }
        y--;

        if (g_LCD.MonoFill == LCD_Cover)  // 指定椭圆填充
        {
            /* 遍历两侧部分 */
            GUI_BasePainter_Fill(deviceInfo, X + x, Y - y, X + x + 1, Y + y, Color);
            GUI_BasePainter_Fill(deviceInfo, X - x, Y - y, X - x + 1, Y + y, Color);
        }

        /* 画椭圆两侧部分圆弧 */
        deviceInfo->drawPointCallback(deviceInfo, X + x, Y + y, Color);
        deviceInfo->drawPointCallback(deviceInfo, X - x, Y - y, Color);
        deviceInfo->drawPointCallback(deviceInfo, X - x, Y + y, Color);
        deviceInfo->drawPointCallback(deviceInfo, X + x, Y - y, Color);
    }
}

/**
 * @brief 绘制圆弧
 * @param deviceInfo 设备信息结构体
 * @param x 圆心X坐标
 * @param y 圆心Y坐标
 * @param radius 圆弧半径
 * @param startAngle 起始角度(度)
 * @param endAngle 结束角度(度)
 * @param color 圆弧颜色
 * @param fillMode 填充模式(填充扇形或仅边框)
 * @note 角度范围0-360度，0度为正右方向，顺时针增加
 */
void GUI_BasePainter_DrawArc(GUI_DeviceInfo_T *deviceInfo, uint16_t x, uint16_t y, uint8_t radius, int16_t startAngle, int16_t endAngle, uint32_t color)
{
    if (deviceInfo == NULL || deviceInfo->drawPointCallback == NULL) return;

    /* 角度归一化到0-360度 */
    while (startAngle < 0) startAngle += 360;
    while (endAngle < 0) endAngle += 360;
    startAngle %= 360;
    endAngle %= 360;

    if (deviceInfo->fillMode == GUI_PAINTER_FILL_COLOR)
    {
        /* 填充圆弧（扇形） */
        /* 计算扇形边界 */
        int16_t minX = x - radius;
        int16_t maxX = x + radius;
        int16_t minY = y - radius;
        int16_t maxY = y + radius;

        /* 遍历扇形区域 */
        for (int16_t py = minY; py <= maxY; py++)
        {
            for (int16_t px = minX; px <= maxX; px++)
            {
                /* 计算点到圆心的距离 */
                int16_t dx = px - x;
                int16_t dy = py - y;
                float distance = sqrt(dx * dx + dy * dy);

                /* 检查点是否在圆内 */
                if (distance <= radius)
                {
                    /* 计算点的角度 */
                    float angle = atan2(dy, dx) * 180.0f / M_PI;
                    if (angle < 0) angle += 360.0f;

                    /* 检查点是否在角度范围内 */
                    if (startAngle <= endAngle)
                    {
                        if (angle >= startAngle && angle <= endAngle)
                        {
                            deviceInfo->drawPointCallback(deviceInfo, px, py, color);
                        }
                    }
                    else
                    {
                        if (angle >= startAngle || angle <= endAngle)
                        {
                            deviceInfo->drawPointCallback(deviceInfo, px, py, color);
                        }
                    }
                }
            }
        }
    }
    else
    {
        /* 绘制圆弧边框 */
        int16_t x0 = 0;
        int16_t y0 = radius;
        int16_t d = 3 - 2 * radius;

        while (x0 <= y0)
        {
            /* 检查八个对称点是否在角度范围内 */
            int16_t points[8][2] = {
                { x + x0, y + y0 }, { x - x0, y + y0 }, { x + x0, y - y0 }, { x - x0, y - y0 }, { x + y0, y + x0 }, { x - y0, y + x0 }, { x + y0, y - x0 }, { x - y0, y - x0 }
            };

            for (int i = 0; i < 8; i++)
            {
                int16_t px = points[i][0];
                int16_t py = points[i][1];

                /* 计算点的角度 */
                float angle = atan2(py - y, px - x) * 180.0f / M_PI;
                if (angle < 0) angle += 360.0f;

                /* 检查点是否在角度范围内 */
                if (startAngle <= endAngle)
                {
                    if (angle >= startAngle && angle <= endAngle)
                    {
                        deviceInfo->drawPointCallback(deviceInfo, px, py, color);
                    }
                }
                else
                {
                    if (angle >= startAngle || angle <= endAngle)
                    {
                        deviceInfo->drawPointCallback(deviceInfo, px, py, color);
                    }
                }
            }

            if (d < 0)
            {
                d += 4 * x0 + 6;
            }
            else
            {
                d += 4 * (x0 - y0) + 10;
                y0--;
            }
            x0++;
        }
    }
}

/**
 * @brief 填充矩形区域
 * @param deviceInfo 设备信息结构体
 * @param x 矩形左上角X坐标
 * @param y 矩形左上角Y坐标
 * @param Width 矩形宽度
 * @param Height 矩形高度
 * @param color 填充颜色
 */
void GUI_BasePainter_Fill(GUI_DeviceInfo_T *deviceInfo, uint16_t x, uint16_t y, uint16_t Width, uint16_t Height, uint32_t color)
{
    if (deviceInfo == NULL || deviceInfo->drawPointCallback == NULL) return;

    if (deviceInfo->fillColorCallback != NULL)
    {
        deviceInfo->fillColorCallback(deviceInfo, x, y, Width, Height, color);
        return;
    }

    uint16_t showColor = deviceInfo->swapFlag ? GUI_SwapBytes(color) : color;
    uint16_t *colorBuffer = (uint16_t *)Memory_Malloc(MP_RAM_EX, Width * Height * 2);

    for (uint32_t i = 0; i < Width * Height; i++)
    {
        colorBuffer[i] = showColor;
    }

    GUI_BasePainter_ShowColorImage(deviceInfo, x, y, Width, Height, (uint8_t *)colorBuffer);

    Memory_Free(MP_RAM_EX, colorBuffer);
}

/**
 * @brief 显示单色图像
 * @param deviceInfo 设备信息结构体
 * @param x 图像左上角X坐标
 * @param y 图像左上角Y坐标
 * @param Width 图像宽度
 * @param Height 图像高度
 * @param image 单色图像数据(每字节8个像素)
 * @param foreColor 前景色
 * @param backColor 背景色
 * @note 图像数据每字节表示8个像素，高位在前
 */
void GUI_BasePainter_ShowMonoImage(GUI_DeviceInfo_T *deviceInfo, uint16_t x, uint16_t y, uint16_t Width, uint16_t Height, const uint8_t *image, uint32_t foreColor, uint32_t backColor)
{
    if (deviceInfo == NULL) return;

    /* 检查是否有专门的drawMonoImageCallback */
    if (deviceInfo->drawMonoImageCallback != NULL)
    {
        deviceInfo->drawMonoImageCallback(deviceInfo, x, y, (uint8_t *)image, Width, Height, foreColor, backColor);
        return;
    }

    /* 如果没有专门的回调，使用默认实现 */
    if (deviceInfo->drawPointCallback == NULL) return;

    /* 分配临时缓冲区用于转换后的彩色图像数据 */
    uint16_t *colorBuffer = (uint16_t *)Memory_Malloc(MP_RAM_EX, Width * Height * 2);
    if (colorBuffer == NULL) return;

    /* 将单色图像转换为彩色图像 */
    uint8_t mask = 0x01;
    uint16_t xPos = 0, yPos = 0;

    /* 大小端交换 */
    foreColor = deviceInfo->swapFlag ? GUI_SwapBytes(foreColor) : foreColor;
    backColor = deviceInfo->swapFlag ? GUI_SwapBytes(backColor) : backColor;

    for (uint32_t i = 0; yPos < Height; i++)
    {
        mask = 0x01;
        for (uint8_t j = 0; j < 8 && xPos < Width; j++)
        {
            colorBuffer[yPos * Width + xPos] = image[i] & mask ? foreColor : backColor;
            mask <<= 1;
            xPos++;
        }

        if (xPos >= Width)
        {
            xPos = 0;
            yPos++;
        }
    }

    GUI_BasePainter_ShowColorImage(deviceInfo, x, y, Width, Height, (uint8_t *)colorBuffer);

    /* 释放临时缓冲区 */
    Memory_Free(MP_RAM_EX, colorBuffer);
}

/**
 * @brief 显示彩色图像
 * @param deviceInfo 设备信息结构体
 * @param x 图像左上角X坐标
 * @param y 图像左上角Y坐标
 * @param Width 图像宽度
 * @param Height 图像高度
 * @param image 彩色图像数据(16位RGB565格式)
 */
void GUI_BasePainter_ShowColorImage(GUI_DeviceInfo_T *deviceInfo, uint16_t x, uint16_t y, uint16_t Width, uint16_t Height, const uint8_t *image)
{
    if (deviceInfo == NULL || deviceInfo->drawPointCallback == NULL) return;

    if (deviceInfo->drawBufferCallback != NULL)
    {
        deviceInfo->drawBufferCallback(deviceInfo, x, y, image, Width, Height);
        return;
    }

    for (uint16_t yPos = 0; yPos < Height; yPos++)
    {
        for (uint16_t xPos = 0; xPos < Width; xPos++)
        {
            uint16_t color = ((uint16_t *)image)[yPos * Width + xPos];
            deviceInfo->drawPointCallback(deviceInfo, x + xPos, y + yPos, color);
        }
    }
}

/**
 * @brief 绘制三角形
 * @param deviceInfo 设备信息结构体
 * @param x0 第一个顶点X坐标
 * @param y0 第一个顶点Y坐标
 * @param x1 第二个顶点X坐标
 * @param y1 第二个顶点Y坐标
 * @param x2 第三个顶点X坐标
 * @param y2 第三个顶点Y坐标
 * @param color 三角形颜色
 * @param fillMode 填充模式(填充或仅边框)
 */
void GUI_BasePainter_DrawTriangle(GUI_DeviceInfo_T *deviceInfo, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint32_t color)
{
    if (deviceInfo == NULL || deviceInfo->drawPointCallback == NULL) return;

    if (deviceInfo->fillMode == GUI_PAINTER_FILL_COLOR)
    {
        /* 填充三角形 */
        /* 找到三角形的边界 */
        int16_t minX = (x0 < x1) ? ((x0 < x2) ? x0 : x2) : ((x1 < x2) ? x1 : x2);
        int16_t maxX = (x0 > x1) ? ((x0 > x2) ? x0 : x2) : ((x1 > x2) ? x1 : x2);
        int16_t minY = (y0 < y1) ? ((y0 < y2) ? y0 : y2) : ((y1 < y2) ? y1 : y2);
        int16_t maxY = (y0 > y1) ? ((y0 > y2) ? y0 : y2) : ((y1 > y2) ? y1 : y2);

        /* 遍历三角形区域 */
        for (int16_t py = minY; py <= maxY; py++)
        {
            for (int16_t px = minX; px <= maxX; px++)
            {
                /* 使用叉积判断点是否在三角形内 */
                int16_t sign1 = (px - x1) * (y0 - y1) - (x0 - x1) * (py - y1);
                int16_t sign2 = (px - x2) * (y1 - y2) - (x1 - x2) * (py - y2);
                int16_t sign3 = (px - x0) * (y2 - y0) - (x2 - x0) * (py - y0);

                uint8_t hasNeg = (sign1 < 0) || (sign2 < 0) || (sign3 < 0);
                uint8_t hasPos = (sign1 > 0) || (sign2 > 0) || (sign3 > 0);

                if (!(hasNeg && hasPos))
                {
                    deviceInfo->drawPointCallback(deviceInfo, px, py, color);
                }
            }
        }
    }
    else
    {
        /* 绘制三角形边框 */
        GUI_BasePainter_DrawLine(deviceInfo, x0, y0, x1, y1, color);
        GUI_BasePainter_DrawLine(deviceInfo, x1, y1, x2, y2, color);
        GUI_BasePainter_DrawLine(deviceInfo, x2, y2, x0, y0, color);
    }
}

/**
 * @brief 绘制多边形
 * @param deviceInfo 设备信息结构体
 * @param vx 顶点X坐标数组
 * @param vy 顶点Y坐标数组
 * @param n 顶点数量
 * @param color 多边形颜色
 * @param fillMode 填充模式(填充或仅边框)
 * @note 顶点按顺时针或逆时针顺序排列
 */
void GUI_BasePainter_DrawPoly(GUI_DeviceInfo_T *deviceInfo, uint16_t vx[], uint16_t vy[], int n, uint32_t color)
{
    if (deviceInfo == NULL || deviceInfo->drawPointCallback == NULL) return;
    return;

    if (deviceInfo->fillMode == GUI_PAINTER_FILL_COLOR)
    {
        /* 填充多边形 */
        /* 找到多边形的边界 */
        int16_t minX = vx[0], maxX = vx[0];
        int16_t minY = vy[0], maxY = vy[0];

        for (int i = 1; i < n; i++)
        {
            if (vx[i] < minX) minX = vx[i];
            if (vx[i] > maxX) maxX = vx[i];
            if (vy[i] < minY) minY = vy[i];
            if (vy[i] > maxY) maxY = vy[i];
        }

        /* 遍历多边形区域 */
        for (int16_t py = minY; py <= maxY; py++)
        {
            for (int16_t px = minX; px <= maxX; px++)
            {
                /* 使用射线法判断点是否在多边形内 */
                int intersections = 0;

                for (int i = 0; i < n; i++)
                {
                    int j = (i + 1) % n;

                    if (((vy[i] > py) != (vy[j] > py)) && (px < (vx[j] - vx[i]) * (py - vy[i]) / (vy[j] - vy[i]) + vx[i]))
                    {
                        intersections++;
                    }
                }

                if (intersections % 2 == 1)
                {
                    deviceInfo->drawPointCallback(deviceInfo, px, py, color);
                }
            }
        }
    }
    else
    {
        /* 绘制多边形边框 */
        for (int i = 0; i < n - 1; i++)
        {
            GUI_BasePainter_DrawLine(deviceInfo, vx[i], vy[i], vx[i + 1], vy[i + 1], color);
        }
        GUI_BasePainter_DrawLine(deviceInfo, vx[n - 1], vy[n - 1], vx[0], vy[0], color);
    }
}

/**
 * @brief 绘制粗圆形
 * @param deviceInfo 设备信息结构体
 * @param x 圆心X坐标
 * @param y 圆心Y坐标
 * @param radius 外圆半径
 * @param thickness 圆环厚度
 * @param color 圆形颜色
 * @param fillMode 填充模式(填充或仅边框)
 * @note 使用改进的算法，减少重复计算
 */
void GUI_BasePainter_DrawThickCircle(GUI_DeviceInfo_T *deviceInfo, uint16_t x, uint16_t y, uint8_t radius, uint8_t thickness, uint32_t color)
{
    if (deviceInfo == NULL || deviceInfo->drawPointCallback == NULL || thickness == 0) return;

    if (deviceInfo->fillMode == GUI_PAINTER_FILL_COLOR)
    {
        /* 填充粗圆 */
        uint8_t innerRadius = (radius > thickness) ? (radius - thickness) : 0;
        int16_t innerRadiusSq = innerRadius * innerRadius;
        int16_t outerRadiusSq = radius * radius;

        /* 使用中点圆算法的优化版本 */
        int16_t x0 = 0;
        int16_t y0 = radius;
        int16_t d = 3 - 2 * radius;

        while (x0 <= y0)
        {
            /* 计算当前y值对应的x范围 */
            int16_t yVal = y0;
            int16_t xStart = x0;
            int16_t xEnd = x0;

            /* 找到圆环的边界 */
            while (xStart * xStart + yVal * yVal > outerRadiusSq && xStart > 0) xStart--;
            while (xEnd * xEnd + yVal * yVal <= outerRadiusSq && xEnd < yVal) xEnd++;

            /* 绘制八分圆 */
            for (int16_t i = xStart; i <= xEnd; i++)
            {
                /* 检查是否在内圆外 */
                if (i * i + yVal * yVal >= innerRadiusSq)
                {
                    deviceInfo->drawPointCallback(deviceInfo, x + i, y + yVal, color);
                    deviceInfo->drawPointCallback(deviceInfo, x - i, y + yVal, color);
                    deviceInfo->drawPointCallback(deviceInfo, x + i, y - yVal, color);
                    deviceInfo->drawPointCallback(deviceInfo, x - i, y - yVal, color);
                    deviceInfo->drawPointCallback(deviceInfo, x + yVal, y + i, color);
                    deviceInfo->drawPointCallback(deviceInfo, x - yVal, y + i, color);
                    deviceInfo->drawPointCallback(deviceInfo, x + yVal, y - i, color);
                    deviceInfo->drawPointCallback(deviceInfo, x - yVal, y - i, color);
                }
            }

            if (d < 0)
            {
                d += 4 * x0 + 6;
            }
            else
            {
                d += 4 * (x0 - y0) + 10;
                y0--;
            }
            x0++;
        }
    }
    else
    {
        /* 绘制多个同心圆形成粗圆 */
        for (uint8_t i = 0; i < thickness; i++)
        {
            if (radius >= i)
            {
                GUI_BasePainter_DrawCircle(deviceInfo, x, y, radius - i, color);
            }
        }
    }
}

/**
 * @brief 绘制粗椭圆
 * @param deviceInfo 设备信息结构体
 * @param x 椭圆中心X坐标
 * @param y 椭圆中心Y坐标
 * @param a 椭圆长半轴
 * @param b 椭圆短半轴
 * @param thickness 椭圆环厚度
 * @param color 椭圆颜色
 * @param fillMode 填充模式(填充或仅边框)
 * @note 使用改进的算法，减少重复计算
 */
void GUI_BasePainter_DrawThickEllipse(GUI_DeviceInfo_T *deviceInfo, uint16_t x, uint16_t y, uint8_t a, uint8_t b, uint8_t thickness, uint32_t color)
{
    if (deviceInfo == NULL || deviceInfo->drawPointCallback == NULL || thickness == 0) return;

    if (deviceInfo->fillMode == GUI_PAINTER_FILL_COLOR)
    {
        /* 填充粗椭圆 */
        uint8_t innerA = (a > thickness) ? (a - thickness) : 0;
        uint8_t innerB = (b > thickness) ? (b - thickness) : 0;

        int32_t a2 = a * a;
        int32_t b2 = b * b;
        int32_t innerA2 = innerA * innerA;
        int32_t innerB2 = innerB * innerB;

        /* 使用中点椭圆算法的优化版本 */
        int16_t x0 = 0, y0 = b;
        int32_t d = b2 - a2 * b + a2 / 4;

        while (a2 * y0 > b2 * x0)
        {
            /* 计算当前y值对应的x范围 */
            int16_t xStart = x0;
            int16_t xEnd = x0;

            /* 找到椭圆环的边界 */
            while (xStart * xStart * b2 + y0 * y0 * a2 > a2 * b2 && xStart > 0) xStart--;
            while (xEnd * xEnd * b2 + y0 * y0 * a2 <= a2 * b2 && xEnd < y0) xEnd++;

            /* 绘制四分椭圆 */
            for (int16_t i = xStart; i <= xEnd; i++)
            {
                /* 检查是否在内椭圆外 */
                if (i * i * b2 + y0 * y0 * a2 >= innerA2 * innerB2)
                {
                    deviceInfo->drawPointCallback(deviceInfo, x + i, y + y0, color);
                    deviceInfo->drawPointCallback(deviceInfo, x - i, y + y0, color);
                    deviceInfo->drawPointCallback(deviceInfo, x + i, y - y0, color);
                    deviceInfo->drawPointCallback(deviceInfo, x - i, y - y0, color);
                }
            }

            if (d < 0)
            {
                d += 2 * b2 * x0 + 3 * b2;
            }
            else
            {
                d += 2 * b2 * x0 - 2 * a2 * y0 + 3 * b2 + 2 * a2;
                y0--;
            }
            x0++;
        }

        d = b2 * (x0 + 0.5) * (x0 + 0.5) + a2 * (y0 - 1) * (y0 - 1) - a2 * b2;
        while (y0 >= 0)
        {
            /* 计算当前y值对应的x范围 */
            int16_t xStart = x0;
            int16_t xEnd = x0;

            /* 找到椭圆环的边界 */
            while (xStart * xStart * b2 + y0 * y0 * a2 > a2 * b2 && xStart > 0) xStart--;
            while (xEnd * xEnd * b2 + y0 * y0 * a2 <= a2 * b2 && xEnd < a) xEnd++;

            /* 绘制四分椭圆 */
            for (int16_t i = xStart; i <= xEnd; i++)
            {
                /* 检查是否在内椭圆外 */
                if (i * i * b2 + y0 * y0 * a2 >= innerA2 * innerB2)
                {
                    deviceInfo->drawPointCallback(deviceInfo, x + i, y + y0, color);
                    deviceInfo->drawPointCallback(deviceInfo, x - i, y + y0, color);
                    deviceInfo->drawPointCallback(deviceInfo, x + i, y - y0, color);
                    deviceInfo->drawPointCallback(deviceInfo, x - i, y - y0, color);
                }
            }

            if (d > 0)
            {
                d -= 2 * a2 * y0 + a2;
            }
            else
            {
                d += 2 * b2 * x0 - 2 * a2 * y0 + 2 * a2;
                x0++;
            }
            y0--;
        }
    }
    else
    {
        /* 绘制多个同心椭圆形成粗椭圆 */
        for (uint8_t i = 0; i < thickness; i++)
        {
            if (a >= i && b >= i)
            {
                GUI_BasePainter_DrawEllipse(deviceInfo, x, y, a - i, b - i, color);
            }
        }
    }
}

/**
 * @brief 绘制粗圆弧
 * @param deviceInfo 设备信息结构体
 * @param x 圆心X坐标
 * @param y 圆心Y坐标
 * @param radius 圆弧外半径
 * @param thickness 圆弧厚度
 * @param startAngle 起始角度(度)
 * @param endAngle 结束角度(度)
 * @param color 圆弧颜色
 * @param fillMode 填充模式(填充扇形或仅边框)
 * @note 使用改进的算法，减少重复计算
 */
void GUI_BasePainter_DrawThickArc(GUI_DeviceInfo_T *deviceInfo, uint16_t x, uint16_t y, uint8_t radius, uint8_t thickness, int16_t startAngle, int16_t endAngle, uint32_t color)
{
    if (deviceInfo == NULL || deviceInfo->drawPointCallback == NULL || thickness == 0) return;

    /* 角度归一化到0-360度 */
    while (startAngle < 0) startAngle += 360;
    while (endAngle < 0) endAngle += 360;
    startAngle %= 360;
    endAngle %= 360;

    if (deviceInfo->fillMode == GUI_PAINTER_FILL_COLOR)
    {
        /* 填充粗圆弧 */
        uint8_t innerRadius = (radius > thickness) ? (radius - thickness) : 0;
        int16_t innerRadiusSq = innerRadius * innerRadius;
        int16_t outerRadiusSq = radius * radius;

        /* 计算扇形边界 */
        int16_t minX = x - radius;
        int16_t maxX = x + radius;
        int16_t minY = y - radius;
        int16_t maxY = y + radius;

        /* 遍历扇形区域 */
        for (int16_t py = minY; py <= maxY; py++)
        {
            for (int16_t px = minX; px <= maxX; px++)
            {
                /* 计算点到圆心的距离平方 */
                int16_t dx = px - x;
                int16_t dy = py - y;
                int16_t distanceSq = dx * dx + dy * dy;

                /* 检查点是否在圆环内 */
                if (distanceSq <= outerRadiusSq && distanceSq >= innerRadiusSq)
                {
                    /* 计算点的角度 */
                    float angle = atan2(dy, dx) * 180.0f / M_PI;
                    if (angle < 0) angle += 360.0f;

                    /* 检查点是否在角度范围内 */
                    if (startAngle <= endAngle)
                    {
                        if (angle >= startAngle && angle <= endAngle)
                        {
                            deviceInfo->drawPointCallback(deviceInfo, px, py, color);
                        }
                    }
                    else
                    {
                        if (angle >= startAngle || angle <= endAngle)
                        {
                            deviceInfo->drawPointCallback(deviceInfo, px, py, color);
                        }
                    }
                }
            }
        }
    }
    else
    {
        /* 绘制多个同心圆弧形成粗圆弧 */
        for (uint8_t i = 0; i < thickness; i++)
        {
            if (radius >= i)
            {
                GUI_BasePainter_DrawArc(deviceInfo, x, y, radius - i, startAngle, endAngle, color);
            }
        }
    }
}

/**
 * @brief 绘制粗直线（纯整数运算版）
 * @param deviceInfo 设备信息结构体
 * @param x0 起点X坐标
 * @param y0 起点Y坐标
 * @param x1 终点X坐标
 * @param y1 终点Y坐标
 * @param thickness 线条粗细(像素)
 * @param color 线条颜色
 * @note 使用纯整数运算，直接绘制粗线
 */
void GUI_BasePainter_DrawThickLine(GUI_DeviceInfo_T *deviceInfo, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint8_t thickness, uint32_t color)
{
    if (deviceInfo == NULL || deviceInfo->drawPointCallback == NULL || thickness == 0) return;

    /* 计算线的方向向量 */
    int16_t dx = x1 - x0;
    int16_t dy = y1 - y0;
    int16_t length = (int16_t)sqrt((float)(dx * dx + dy * dy));

    if (length == 0)
    {
        /* 点的情况，绘制一个圆 */
        GUI_PainterFillMode_T temp = deviceInfo->fillMode;
        deviceInfo->fillMode = GUI_PAINTER_FILL_COLOR;
        GUI_BasePainter_DrawThickCircle(deviceInfo, x0, y0, thickness / 2, thickness, color);
        deviceInfo->fillMode = temp;
        return;
    }

    /* 判断线的方向，选择扫描方向 */
    uint8_t isHorizontal = abs(dx) > abs(dy);

    if (isHorizontal)
    {
        /* 水平主导，按X扫描 */
        /* 确保x0 <= x1 */
        if (x0 > x1)
        {
            uint16_t tempX = x0;
            x0 = x1;
            x1 = tempX;
            uint16_t tempY = y0;
            y0 = y1;
            y1 = tempY;
            dx = x1 - x0;
            dy = y1 - y0;
        }

        /* 使用定点数计算斜率（精度为1/1000） */
        int16_t slopeFixed = (dy * 1000) / dx;
        int16_t yFixed = y0 * 1000;

        /* 扫描每一条垂直线 */
        for (int16_t x = x0; x <= x1; x++)
        {
            /* 计算中心点Y坐标（定点数） */
            int16_t centerYFixed = yFixed;

            /* 转换为整数 */
            int16_t centerY = centerYFixed / 1000;

            /* 计算垂直范围 */
            int16_t StartY = centerY - thickness / 2;
            int16_t endY = StartY + thickness - 1;

            /* 绘制垂直线段 */
            for (int16_t y = StartY; y <= endY; y++)
            {
                deviceInfo->drawPointCallback(deviceInfo, x, y, color);
            }

            /* 更新Y坐标 */
            yFixed += slopeFixed;
        }
    }
    else
    {
        /* 垂直主导，按Y扫描 */
        /* 确保y0 <= y1 */
        if (y0 > y1)
        {
            uint16_t tempX = x0;
            x0 = x1;
            x1 = tempX;
            uint16_t tempY = y0;
            y0 = y1;
            y1 = tempY;
            dx = x1 - x0;
            dy = y1 - y0;
        }

        /* 使用定点数计算斜率（精度为1/1000） */
        int16_t slopeFixed = (dx * 1000) / dy;
        int16_t xFixed = x0 * 1000;

        /* 扫描每一条水平线 */
        for (int16_t y = y0; y <= y1; y++)
        {
            /* 计算中心点X坐标（定点数） */
            int16_t centerXFixed = xFixed;

            /* 转换为整数 */
            int16_t centerX = centerXFixed / 1000;

            /* 计算水平范围 */
            int16_t StartX = centerX - thickness / 2;
            int16_t endX = StartX + thickness - 1;

            /* 绘制水平线段 */
            for (int16_t x = StartX; x <= endX; x++)
            {
                deviceInfo->drawPointCallback(deviceInfo, x, y, color);
            }

            /* 更新X坐标 */
            xFixed += slopeFixed;
        }
    }
}

/**
 * @brief 绘制单个字符
 * @param deviceInfo 设备信息结构体
 * @param pFont 字体信息结构体
 * @param x 字符左上角X坐标
 * @param y 字符左上角Y坐标
 * @param ch 字符数据(支持多字节字符)
 * @param foreColor 前景色
 * @param backColor 背景色
 */
void GUI_BasePainter_DrawChar(GUI_DeviceInfo_T *deviceInfo, uint16_t x, uint16_t y, char ch[4], uint32_t foreColor, uint32_t backColor)
{
    if (deviceInfo == NULL || deviceInfo->showFont == NULL || deviceInfo->showFont->getFontBuffer == NULL) return;

    /* 获取字符数组详情 */
    GUI_FontInfo_T *pFont = deviceInfo->showFont;
    uint8_t *FontBuffer = NULL;
    uint16_t FontWidth = 0;

    /* 判断字符类型并确定宽度 */
    if (pFont->encoding == GUI_FONT_ENCODING_ASCII)
    {
        FontWidth = pFont->asciiFontWidth;
    }
    else
    {
        /* 简单判断是否为ASCII字符 */
        if ((uint8_t)ch[0] < 0x80)
        {
            FontWidth = pFont->asciiFontWidth;
        }
        else
        {
            FontWidth = pFont->otherFontWidth;
        }
    }

    /* 获取字体数据 */
    pFont->getFontBuffer(pFont, ch, &FontBuffer, FontWidth, pFont->FontHeight);
    if (FontBuffer == NULL) return;

    /* 使用drawMonoImageCallback绘制字符 */
    if (deviceInfo->drawMonoImageCallback != NULL)
    {
        deviceInfo->drawMonoImageCallback(deviceInfo, x, y, FontBuffer, FontWidth, pFont->FontHeight, foreColor, backColor);
    }
    else
    {
        /* 使用默认实现绘制字符 */
        GUI_BasePainter_ShowMonoImage(deviceInfo, x, y, FontWidth, pFont->FontHeight, FontBuffer, foreColor, backColor);
    }
}

/**
 * @brief 绘制字符串
 * @param deviceInfo 设备信息结构体
 * @param pFont 字体信息结构体
 * @param x 字符串左上角X坐标
 * @param y 字符串左上角Y坐标
 * @param str 字符串
 * @param foreColor 前景色
 * @param backColor 背景色
 */
void GUI_BasePainter_ShowString(GUI_DeviceInfo_T *deviceInfo, uint16_t x, uint16_t y, const char *str, uint32_t foreColor, uint32_t backColor)
{
    if (deviceInfo == NULL || deviceInfo->showFont == NULL || str == NULL) return;

    /* 计算每行可容纳的字符数 */
    GUI_FontInfo_T *pFont = deviceInfo->showFont;
    uint16_t posX = x;
    uint16_t posY = y;
    const char *p = str;
    char ch[4] = { 0 };

    while (*p != '\0' && posX < deviceInfo->Width)
    {
        /* 根据编码格式解析字符 */
        if (pFont->encoding == GUI_FONT_ENCODING_ASCII)
        {
            /* ASCII编码，单字节字符 */
            ch[0] = *p++;
            ch[1] = '\0';

            /* 绘制字符 */
            GUI_BasePainter_DrawChar(deviceInfo, posX, posY, ch, foreColor, backColor);

            /* 更新位置 */
            posX += pFont->asciiFontWidth;
        }
        else if (pFont->encoding == GUI_FONT_ENCODING_GB2312)
        {
            /* GB2312编码，ASCII为单字节，中文为双字节 */
            if ((uint8_t)*p < 0x80)
            {
                /* ASCII字符 */
                ch[0] = *p++;
                ch[1] = '\0';

                /* 绘制字符 */
                GUI_BasePainter_DrawChar(deviceInfo, posX, posY, ch, foreColor, backColor);

                /* 更新位置 */
                posX += pFont->asciiFontWidth;
            }
            else
            {
                /* 中文字符 */
                ch[0] = *p++;
                ch[1] = *p++;
                ch[2] = '\0';

                /* 绘制字符 */
                GUI_BasePainter_DrawChar(deviceInfo, posX, posY, ch, foreColor, backColor);

                /* 更新位置 */
                posX += pFont->otherFontWidth;
            }
        }
        else if (pFont->encoding == GUI_FONT_ENCODING_UTF8)
        {
            /* UTF8编码，ASCII为单字节，中文通常为3字节 */
            if ((uint8_t)*p < 0x80)
            {
                /* ASCII字符 */
                ch[0] = *p++;
                ch[1] = '\0';

                /* 绘制字符 */
                GUI_BasePainter_DrawChar(deviceInfo, posX, posY, ch, foreColor, backColor);

                /* 更新位置 */
                posX += pFont->asciiFontWidth;
            }
            else
            {
                /* 获取UTF8字符长度 */
                uint8_t len = 0;
                if ((uint8_t)*p >= 0xFC)
                {
                    len = 6; /* 6字节UTF8字符 */
                }
                else if ((uint8_t)*p >= 0xF8)
                {
                    len = 5; /* 5字节UTF8字符 */
                }
                else if ((uint8_t)*p >= 0xF0)
                {
                    len = 4; /* 4字节UTF8字符 */
                }
                else if ((uint8_t)*p >= 0xE0)
                {
                    len = 3; /* 3字节UTF8字符（常见中文） */
                }
                else if ((uint8_t)*p >= 0xC0)
                {
                    len = 2; /* 2字节UTF8字符 */
                }
                else
                {
                    len = 1; /* 单字节字符 */
                }

                /* 复制字符 */
                for (uint8_t i = 0; i < len && *p != '\0'; i++)
                {
                    ch[i] = *p++;
                }
                ch[len] = '\0';

                /* 绘制字符 */
                GUI_BasePainter_DrawChar(deviceInfo, posX, posY, ch, foreColor, backColor);

                /* 更新位置 */
                posX += pFont->otherFontWidth;
            }
        }
    }
}

/**
 * @brief 绘制文本块
 * @param deviceInfo 设备信息结构体
 * @param pFont 字体信息结构体
 * @param x 文本块左上角X坐标
 * @param y 文本块左上角Y坐标
 * @param Width 文本块宽度
 * @param Height 文本块高度
 * @param str 文本字符串
 * @param foreColor 前景色
 * @param backColor 背景色
 * @note 支持自动换行和文本截断
 */
void GUI_BasePainter_ShowStringBlock(GUI_DeviceInfo_T *deviceInfo, uint16_t x, uint16_t y, uint16_t Width, uint16_t Height, const char *str, uint32_t foreColor, uint32_t backColor)
{
    if (deviceInfo == NULL || deviceInfo->showFont == NULL || str == 0 || Width == 0 || Height == 0) return;

    /* 计算每行可容纳的字符数 */
    GUI_FontInfo_T *pFont = deviceInfo->showFont;
    uint16_t charsPerLine = Width / pFont->asciiFontWidth;
    uint16_t linesPerBlock = Height / pFont->FontHeight;

    if (charsPerLine == 0 || linesPerBlock == 0) return;

    uint16_t currentLine = 0;
    uint16_t currentX = x;
    uint16_t currentY = y;
    const char *p = str;
    char ch[4] = { 0 };
    char wordBuffer[64] = { 0 }; /* 用于存储单词 */
    uint8_t wordLen = 0;
    uint16_t wordWidth = 0;

    /* 首先填充背景 */
    GUI_BasePainter_Fill(deviceInfo, x, y, Width, Height, backColor);

    while (*p != '\0' && currentLine < linesPerBlock)
    {
        /* 处理空白字符 */
        if (*p == ' ' || *p == '\t' || *p == '\n')
        {
            /* 绘制已缓存的单词 */
            if (wordLen > 0)
            {
                /* 检查单词是否能放在当前行 */
                if (currentX + wordWidth > x + Width)
                {
                    /* 换行 */
                    currentLine++;
                    currentY += pFont->FontHeight;
                    currentX = x;

                    if (currentLine >= linesPerBlock) break;
                }

                /* 绘制单词 */
                const char *wordP = wordBuffer;
                while (*wordP != '\0')
                {
                    /* 根据编码格式解析字符 */
                    if (pFont->encoding == GUI_FONT_ENCODING_ASCII)
                    {
                        ch[0] = *wordP++;
                        ch[1] = '\0';
                        GUI_BasePainter_DrawChar(deviceInfo, currentX, currentY, ch, foreColor, backColor);
                        currentX += pFont->asciiFontWidth;
                    }
                    else if (pFont->encoding == GUI_FONT_ENCODING_GB2312)
                    {
                        if ((uint8_t)*wordP < 0x80)
                        {
                            ch[0] = *wordP++;
                            ch[1] = '\0';
                            GUI_BasePainter_DrawChar(deviceInfo, currentX, currentY, ch, foreColor, backColor);
                            currentX += pFont->asciiFontWidth;
                        }
                        else
                        {
                            ch[0] = *wordP++;
                            ch[1] = *wordP++;
                            ch[2] = '\0';
                            GUI_BasePainter_DrawChar(deviceInfo, currentX, currentY, ch, foreColor, backColor);
                            currentX += pFont->otherFontWidth;
                        }
                    }
                    else if (pFont->encoding == GUI_FONT_ENCODING_UTF8)
                    {
                        if ((uint8_t)*wordP < 0x80)
                        {
                            ch[0] = *wordP++;
                            ch[1] = '\0';
                            GUI_BasePainter_DrawChar(deviceInfo, currentX, currentY, ch, foreColor, backColor);
                            currentX += pFont->asciiFontWidth;
                        }
                        else
                        {
                            /* 获取UTF8字符长度 */
                            uint8_t len = 0;
                            if ((uint8_t)*wordP >= 0xFC)
                            {
                                len = 6;
                            }
                            else if ((uint8_t)*wordP >= 0xF8)
                            {
                                len = 5;
                            }
                            else if ((uint8_t)*wordP >= 0xF0)
                            {
                                len = 4;
                            }
                            else if ((uint8_t)*wordP >= 0xE0)
                            {
                                len = 3;
                            }
                            else if ((uint8_t)*wordP >= 0xC0)
                            {
                                len = 2;
                            }
                            else
                            {
                                len = 1;
                            }

                            for (uint8_t i = 0; i < len && *wordP != '\0'; i++)
                            {
                                ch[i] = *wordP++;
                            }
                            ch[len] = '\0';

                            GUI_BasePainter_DrawChar(deviceInfo, currentX, currentY, ch, foreColor, backColor);
                            currentX += pFont->otherFontWidth;
                        }
                    }
                }

                /* 清空单词缓存 */
                Memory_Set(wordBuffer, 0, sizeof(wordBuffer));
                wordLen = 0;
                wordWidth = 0;
            }

            /* 处理空白字符 */
            if (*p == '\n')
            {
                /* 换行 */
                currentLine++;
                currentY += pFont->FontHeight;
                currentX = x;
                p++;
            }
            else if (*p == ' ')
            {
                /* 空格 */
                if (currentX + pFont->asciiFontWidth <= x + Width)
                {
                    ch[0] = ' ';
                    ch[1] = '\0';
                    GUI_BasePainter_DrawChar(deviceInfo, currentX, currentY, ch, foreColor, backColor);
                    currentX += pFont->asciiFontWidth;
                }
                p++;
            }
            else if (*p == '\t')
            {
                /* 制表符，相当于4个空格 */
                for (uint8_t i = 0; i < 4; i++)
                {
                    if (currentX + pFont->asciiFontWidth <= x + Width)
                    {
                        ch[0] = ' ';
                        ch[1] = '\0';
                        GUI_BasePainter_DrawChar(deviceInfo, currentX, currentY, ch, foreColor, backColor);
                        currentX += pFont->asciiFontWidth;
                    }
                }
                p++;
            }
        }
        else
        {
            /* 将字符添加到单词缓存 */
            if (wordLen < sizeof(wordBuffer) - 1)
            {
                wordBuffer[wordLen++] = *p;

                /* 计算字符宽度 */
                if (pFont->encoding == GUI_FONT_ENCODING_ASCII || (uint8_t)*p < 0x80)
                {
                    wordWidth += pFont->asciiFontWidth;
                }
                else
                {
                    wordWidth += pFont->otherFontWidth;
                }

                p++;
            }
            else
            {
                /* 单词太长，强制换行 */
                wordBuffer[wordLen] = '\0';

                /* 绘制单词 */
                const char *wordP = wordBuffer;
                while (*wordP != '\0')
                {
                    if (pFont->encoding == GUI_FONT_ENCODING_ASCII)
                    {
                        ch[0] = *wordP++;
                        ch[1] = '\0';
                        GUI_BasePainter_DrawChar(deviceInfo, currentX, currentY, ch, foreColor, backColor);
                        currentX += pFont->asciiFontWidth;
                    }
                    else
                    {
                        /* 简化处理，假设都是双字节字符 */
                        ch[0] = *wordP++;
                        ch[1] = *wordP++;
                        ch[2] = '\0';
                        GUI_BasePainter_DrawChar(deviceInfo, currentX, currentY, ch, foreColor, backColor);
                        currentX += pFont->otherFontWidth;
                    }
                }

                /* 清空单词缓存并换行 */
                Memory_Set(wordBuffer, 0, sizeof(wordBuffer));
                wordLen = 0;
                wordWidth = 0;

                currentLine++;
                currentY += pFont->FontHeight;
                currentX = x;
            }
        }
    }

    /* 绘制剩余的单词 */
    if (wordLen > 0 && currentLine < linesPerBlock)
    {
        /* 检查单词是否能放在当前行 */
        if (currentX + wordWidth > x + Width)
        {
            /* 换行 */
            currentLine++;
            currentY += pFont->FontHeight;
            currentX = x;

            if (currentLine >= linesPerBlock) return;
        }

        /* 绘制单词 */
        const char *wordP = wordBuffer;
        while (*wordP != '\0')
        {
            if (pFont->encoding == GUI_FONT_ENCODING_ASCII)
            {
                ch[0] = *wordP++;
                ch[1] = '\0';
                GUI_BasePainter_DrawChar(deviceInfo, currentX, currentY, ch, foreColor, backColor);
                currentX += pFont->asciiFontWidth;
            }
            else if (pFont->encoding == GUI_FONT_ENCODING_GB2312)
            {
                if ((uint8_t)*wordP < 0x80)
                {
                    ch[0] = *wordP++;
                    ch[1] = '\0';
                    GUI_BasePainter_DrawChar(deviceInfo, currentX, currentY, ch, foreColor, backColor);
                    currentX += pFont->asciiFontWidth;
                }
                else
                {
                    ch[0] = *wordP++;
                    ch[1] = *wordP++;
                    ch[2] = '\0';
                    GUI_BasePainter_DrawChar(deviceInfo, currentX, currentY, ch, foreColor, backColor);
                    currentX += pFont->otherFontWidth;
                }
            }
            else if (pFont->encoding == GUI_FONT_ENCODING_UTF8)
            {
                if ((uint8_t)*wordP < 0x80)
                {
                    ch[0] = *wordP++;
                    ch[1] = '\0';
                    GUI_BasePainter_DrawChar(deviceInfo, currentX, currentY, ch, foreColor, backColor);
                    currentX += pFont->asciiFontWidth;
                }
                else
                {
                    /* 获取UTF8字符长度 */
                    uint8_t len = 0;
                    if ((uint8_t)*wordP >= 0xFC)
                    {
                        len = 6;
                    }
                    else if ((uint8_t)*wordP >= 0xF8)
                    {
                        len = 5;
                    }
                    else if ((uint8_t)*wordP >= 0xF0)
                    {
                        len = 4;
                    }
                    else if ((uint8_t)*wordP >= 0xE0)
                    {
                        len = 3;
                    }
                    else if ((uint8_t)*wordP >= 0xC0)
                    {
                        len = 2;
                    }
                    else
                    {
                        len = 1;
                    }

                    for (uint8_t i = 0; i < len && *wordP != '\0'; i++)
                    {
                        ch[i] = *wordP++;
                    }
                    ch[len] = '\0';

                    GUI_BasePainter_DrawChar(deviceInfo, currentX, currentY, ch, foreColor, backColor);
                    currentX += pFont->otherFontWidth;
                }
            }
        }
    }
}

/**
 * @brief 计算数字的幂次方
 * @param base 底数
 * @param exponent 指数
 * @return 计算结果
 */
static uint32_t GUI_PowUint32(uint32_t base, uint8_t exponent)
{
    uint32_t result = 1;
    for (uint8_t i = 0; i < exponent; i++)
    {
        result *= base;
    }
    return result;
}

/**
 * @brief 显示整数
 * @param deviceInfo 设备信息结构体
 * @param x 文本左上角X坐标
 * @param y 文本左上角Y坐标
 * @param number 十进制数字
 * @param length 显示长度(位数)
 * @param foreColor 前景色
 * @param backColor 背景色
 * @param showSign 是否显示符号
 */
void GUI_BasePainter_ShowNumber(GUI_DeviceInfo_T *deviceInfo, uint16_t x, uint16_t y, int32_t number, uint8_t length, uint32_t foreColor, uint32_t backColor, uint8_t showSign)
{
    if (deviceInfo == NULL || deviceInfo->showFont == NULL) return;

    uint16_t currentX = x;
    uint16_t FontWidth = deviceInfo->showFont->asciiFontWidth;

    /* 处理符号 */
    if (showSign)
    {
        if (number >= 0)
        {
            GUI_BasePainter_DrawChar(deviceInfo, currentX, y, "+", foreColor, backColor);
        }
        else
        {
            GUI_BasePainter_DrawChar(deviceInfo, currentX, y, "-", foreColor, backColor);
            number = -number;
        }
        currentX += FontWidth;
    }

    /* 逐位显示数字（与原始LCD函数保持一致） */
    for (uint8_t i = 0; i < length; i++)
    {
        char digitChar = number / GUI_PowUint32(10, length - i - 1) % 10 + '0';
        GUI_BasePainter_DrawChar(deviceInfo, currentX, y, &digitChar, foreColor, backColor);
        currentX += FontWidth;
    }
}

/**
 * @brief 显示十六进制数字
 * @param deviceInfo 设备信息结构体
 * @param x 文本左上角X坐标
 * @param y 文本左上角Y坐标
 * @param number 十六进制数字
 * @param length 显示长度(位数)
 * @param foreColor 前景色
 * @param backColor 背景色
 */
void GUI_BasePainter_ShowHexNumber(GUI_DeviceInfo_T *deviceInfo, uint16_t x, uint16_t y, uint32_t number, uint8_t length, uint32_t foreColor, uint32_t backColor)
{
    if (deviceInfo == NULL || deviceInfo->showFont == NULL) return;

    uint16_t currentX = x;
    uint16_t FontWidth = deviceInfo->showFont->asciiFontWidth;

    /* 逐位显示十六进制数字（与原始LCD函数保持一致） */
    for (uint8_t i = 0; i < length; i++)
    {
        uint8_t digit = number / GUI_PowUint32(16, length - i - 1) % 16;
        char hexChar;

        if (digit < 10)
        {
            hexChar = digit + '0';
        }
        else
        {
            hexChar = digit - 10 + 'A';
        }

        GUI_BasePainter_DrawChar(deviceInfo, currentX, y, &hexChar, foreColor, backColor);
        currentX += FontWidth;
    }
}

/**
 * @brief 显示二进制数字
 * @param deviceInfo 设备信息结构体
 * @param x 文本左上角X坐标
 * @param y 文本左上角Y坐标
 * @param number 二进制数字
 * @param length 显示长度(位数)
 * @param foreColor 前景色
 * @param backColor 背景色
 */
void GUI_BasePainter_ShowBinNumber(GUI_DeviceInfo_T *deviceInfo, uint16_t x, uint16_t y, uint32_t number, uint8_t length, uint32_t foreColor, uint32_t backColor)
{
    if (deviceInfo == NULL || deviceInfo->showFont == NULL) return;

    uint16_t currentX = x;
    uint16_t FontWidth = deviceInfo->showFont->asciiFontWidth;

    /* 逐位显示二进制数字（与原始LCD函数保持一致） */
    for (uint8_t i = 0; i < length; i++)
    {
        char bitChar = number / GUI_PowUint32(2, length - i - 1) % 2 + '0';
        GUI_BasePainter_DrawChar(deviceInfo, currentX, y, &bitChar, foreColor, backColor);
        currentX += FontWidth;
    }
}

/**
 * @brief 显示浮点数
 * @param deviceInfo 设备信息结构体
 * @param x 文本左上角X坐标
 * @param y 文本左上角Y坐标
 * @param number 浮点数字
 * @param intLength 整数部分长度
 * @param fraLength 小数部分长度
 * @param foreColor 前景色
 * @param backColor 背景色
 * @param showSign 是否显示符号
 */
void GUI_BasePainter_ShowFloatNumber(GUI_DeviceInfo_T *deviceInfo, uint16_t x, uint16_t y, double number, uint8_t intLength, uint8_t fraLength, uint32_t foreColor, uint32_t backColor, uint8_t showSign)
{
    if (deviceInfo == NULL || deviceInfo->showFont == NULL) return;

    uint16_t currentX = x;
    uint16_t FontWidth = deviceInfo->showFont->asciiFontWidth;
    uint32_t powNum, intNum, fraNum;

    /* 处理符号 */
    if (showSign)
    {
        if (number >= 0)
        {
            GUI_BasePainter_DrawChar(deviceInfo, currentX, y, "+", foreColor, backColor);
        }
        else
        {
            GUI_BasePainter_DrawChar(deviceInfo, currentX, y, "-", foreColor, backColor);
            number = -number;
        }
        currentX += FontWidth;
    }

    /* 提取整数部分和小数部分（与原始LCD函数保持一致） */
    intNum = number;
    number -= intNum;
    powNum = GUI_PowUint32(10, fraLength);
    fraNum = roundf(number * powNum);
    intNum += fraNum / powNum;

    /* 显示整数部分 */
    for (uint8_t i = 0; i < intLength; i++)
    {
        char digitChar = intNum / GUI_PowUint32(10, intLength - i - 1) % 10 + '0';
        GUI_BasePainter_DrawChar(deviceInfo, currentX, y, &digitChar, foreColor, backColor);
        currentX += FontWidth;
    }

    /* 显示小数点和小数部分 */
    if (fraLength > 0)
    {
        GUI_BasePainter_DrawChar(deviceInfo, currentX, y, ".", foreColor, backColor);
        currentX += FontWidth;

        for (uint8_t i = 0; i < fraLength; i++)
        {
            char digitChar = fraNum / GUI_PowUint32(10, fraLength - i - 1) % 10 + '0';
            GUI_BasePainter_DrawChar(deviceInfo, currentX, y, &digitChar, foreColor, backColor);
            currentX += FontWidth;
        }
    }
}

/**
 * @brief 格式化字符串打印
 * @param deviceInfo 设备信息结构体
 * @param x 文本左上角X坐标
 * @param y 文本左上角Y坐标
 * @param foreColor 前景色
 * @param backColor 背景色
 * @param format 格式化字符串
 * @param ... 可变参数
 */
void GUI_BasePainter_Printf(GUI_DeviceInfo_T *deviceInfo, uint16_t x, uint16_t y, uint32_t foreColor, uint32_t backColor, const char *format, ...)
{
    if (deviceInfo == NULL || deviceInfo->showFont == NULL || format == NULL) return;

    static char buffer[256]; /* 静态缓冲区，避免栈溢出 */
    va_list args;

    /* 格式化字符串 */
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    /* 显示格式化后的字符串 */
    GUI_BasePainter_ShowString(deviceInfo, x, y, buffer, foreColor, backColor);
}

/**
 * @brief 格式化字符串打印
 * @param deviceInfo 设备信息结构体
 * @param x 文本块左上角X坐标
 * @param y 文本块左上角Y坐标
 * @param Width 文本块宽度
 * @param Height 文本块高度
 * @param foreColor 前景色
 * @param backColor 背景色
 * @param format 格式化字符串
 * @param ... 可变参数
 */
void GUI_BasePainter_PrintfBlock(GUI_DeviceInfo_T *deviceInfo, uint16_t x, uint16_t y, uint16_t Width, uint16_t Height, uint32_t foreColor, uint32_t backColor, const char *format, ...)
{
    if (deviceInfo == NULL || deviceInfo->showFont == NULL || format == NULL) return;

    static char buffer[256]; /* 静态缓冲区，避免栈溢出 */
    va_list args;

    /* 格式化字符串 */
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    /* 显示格式化后的字符串 */
    GUI_BasePainter_ShowStringBlock(deviceInfo, x, y, Width, Height, buffer, foreColor, backColor);
}

/**
 * @brief 显示进度条
 * @param deviceInfo 设备信息结构体
 * @param x 进度条左上角X坐标
 * @param y 进度条左上角Y坐标
 * @param Width 进度条宽度
 * @param Height 进度条高度
 * @param progress 进度值(0-100)
 * @param foreColor 前景色
 * @param backColor 背景色
 * @param showPercent 是否显示百分比文本
 */
void GUI_BasePainter_ShowProgress(GUI_DeviceInfo_T *deviceInfo, uint16_t x, uint16_t y, uint16_t Width, uint16_t Height, uint8_t progress, uint32_t foreColor, uint32_t backColor, uint8_t showPercent)
{
    if (deviceInfo == NULL || deviceInfo->showFont == NULL) return;

    /* 限制进度值在0-100范围内 */
    if (progress > 100) progress = 100;

    /* 绘制进度条边框 */
    GUI_PainterFillMode_T temp = deviceInfo->fillMode;
    deviceInfo->fillMode = GUI_PAINTER_FILL_NONE;
    GUI_BasePainter_DrawRectangle(deviceInfo, x, y, Width, Height, foreColor);
    deviceInfo->fillMode = temp;

    /* 绘制进度条内部 */
    if (Width > 2 && Height > 2)
    {
        uint16_t progressWidth = (Width - 2) * progress / 100;
        GUI_BasePainter_Fill(deviceInfo, x + 1, y + 1, progressWidth, Height - 2, foreColor);
    }

    /* 显示百分比 */
    if (showPercent)
    {
        char percentStr[8];
        snprintf(percentStr, sizeof(percentStr), "%d%%", progress);

        /* 计算文本位置（居中） */
        uint16_t textWidth = String_GetLength(percentStr, sizeof(percentStr)) * deviceInfo->showFont->asciiFontWidth;
        uint16_t textX = x + (Width - textWidth) / 2;
        uint16_t textY = y + (Height - deviceInfo->showFont->FontHeight) / 2;

        /* 显示百分比文本 */
        GUI_BasePainter_ShowString(deviceInfo, textX, textY, percentStr, backColor, foreColor);
    }
}

/**
 * @brief 设置显示字体
 * @param deviceInfo 设备信息结构体
 * @param pFont 字体信息结构体
 */
void GUI_BasePainter_SetShowFont(GUI_DeviceInfo_T *deviceInfo, GUI_FontInfo_T *pFont)
{
    deviceInfo->showFont = pFont;
}

/**
 * @brief 设置填充模式
 * @param deviceInfo 设备信息结构体
 * @param mode 填充模式
 */
void GUI_BasePainter_SetFillMode(GUI_DeviceInfo_T *deviceInfo, GUI_PainterFillMode_T mode)
{
    deviceInfo->fillMode = mode;
}
