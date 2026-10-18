#include "bsp_usb.h"

#include "usbd_conf.h"
#include "usbd_core.h"
#include "usbd_desc.h"
#include "usbd_msc.h"
#include "usbd_msc_storage.h"

static USBD_HandleTypeDef s_USB_Device;        /* USB Device处理结构体 */
extern PCD_HandleTypeDef PCD_HandleTypeStruct; /* PCD_USB_OTG_FS 定义*/

/**
 * @brief  USB初始化（配置为MSC大容量存储类设备并启动）
 */
void USB_Init(void)
{
    USBD_Init(&s_USB_Device, &USBD_ClassDesc, 0);                         /* 初始化USB */
    USBD_RegisterClass(&s_USB_Device, &USBD_MSC);                         /* 添加类 */
    USBD_MSC_RegisterStorage(&s_USB_Device, &USBD_MSC_Interface_fops_FS); /* 为MSC类添加回调函数 */
    USBD_Start(&s_USB_Device);                                            /* 开启USB */
    HAL_PWREx_EnableUSBVoltageDetector();                                 /* 使能USB电压检测 */
}
