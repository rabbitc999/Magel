#ifndef __GUI_FONT_DATA_H
#define __GUI_FONT_DATA_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern const uint8_t LCD_F24X12[][48];

typedef struct __GUI_GB2312Font_T
{
    uint16_t Width;          /* 字模宽度 */
    uint16_t Height;         /* 字模长度 */
    uint16_t singleSize;     /* 字模单个大小 */
    uint32_t flashTotalSize; /* 字库闪存大小 */
    uint32_t flashOffset;    /* 字库闪存偏移量 */
    const char *filePath;    /* 文件存放路径 */
    uint8_t InitFlag;        /* 初始化标志 */
    uint8_t *buffer;         /* 字符缓存 */
} GUI_GB2312Font_T;

extern GUI_GB2312Font_T g_GUI_Font16; /* 2424字体 */

#ifdef __cplusplus
}
#endif

#endif
