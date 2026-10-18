/**
 * *****************************************************************************
 * @file    usbd_conf_template.c
 * @author  MCD Application Team
 * @brief   USB Device configuration and interface file
 *          This template should be copied to the user folder,
 *          renamed and customized following user needs.
 * *****************************************************************************
 * @attention
 *
 * Copyright (c) 2015 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 * *****************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "usbd_core.h"
#include "usbd_msc.h" /* Include class header file */
/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/

/* PCD_USB_OTG_FS 定义*/
PCD_HandleTypeDef PCD_HandleTypeStruct;

/* USB连接状态
 * 0,没有连接;
 * 1,已经连接;
 */
volatile uint8_t USB_DeviceState = 0; /* 默认没有连接 */

USBD_StatusTypeDef USBD_GetDeviceStatus(HAL_StatusTypeDef tmpStatus); /* 返回USB的状态 */

/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/**
 * @brief  初始化PCD MSP.
 * @param  hpcd: PCD 结构体指针
 * @retval 无
 */
void HAL_PCD_MspInit(PCD_HandleTypeDef *hpcd)
{
    GPIO_InitTypeDef GPIO_InitStruct;
    RCC_PeriphCLKInitTypeDef PeriphClkInitStruct;

    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_USB;
    PeriphClkInitStruct.UsbClockSelection = RCC_USBCLKSOURCE_HSI48;
    HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct);

    if (hpcd->Instance == USB_OTG_FS)
    {
        __HAL_RCC_GPIOA_CLK_ENABLE();      /* 使能GPIOA时钟 */
        __HAL_RCC_USB_OTG_FS_CLK_ENABLE(); /* 使能OTG FS时钟 */

        /* 配置PA11 12 */
        GPIO_InitStruct.Pin = GPIO_PIN_11 | GPIO_PIN_12;   /* PA11 12 */
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;            /* 复用 */
        GPIO_InitStruct.Pull = GPIO_NOPULL;                /* 无上下拉 */
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH; /* 高速 */
        GPIO_InitStruct.Alternate = GPIO_AF10_OTG1_FS;     /* 复用为OTG FS */
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);            /* 初始化 */

        HAL_NVIC_SetPriority(OTG_FS_IRQn, 1, 0);
        HAL_NVIC_EnableIRQ(OTG_FS_IRQn); /* 使能 OTG FS 中断 */
    }
}

/**
 * @brief       USB OTG 中断服务函数
 * @note        处理所有USB中断
 * @param       无
 * @retval      无
 */
void OTG_FS_IRQHandler(void)
{
    HAL_PCD_IRQHandler(&PCD_HandleTypeStruct);
}

/**
 * @brief       USBD 配置阶段回调函数
 * @param       hpcd    : PCD结构体指针
 * @retval      无
 */
void HAL_PCD_SetupStageCallback(PCD_HandleTypeDef *hpcd)
{
    USBD_LL_SetupStage(hpcd->pData, (uint8_t *)hpcd->Setup);
}

/**
 * @brief       USBD OUT 阶段回调函数
 * @param       hpcd    : PCD结构体指针
 * @param       epnum   : 端点号
 * @retval      无
 */
void HAL_PCD_DataOutStageCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
{
    USBD_LL_DataOutStage(hpcd->pData, epnum, hpcd->OUT_ep[epnum].xfer_buff);
}

/**
 * @brief       USBD IN 阶段回调函数
 * @param       hpcd    : PCD结构体指针
 * @param       epnum   : 端点号
 * @retval      无
 */
void HAL_PCD_DataInStageCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
{
    USBD_LL_DataInStage((USBD_HandleTypeDef *)hpcd->pData, epnum, hpcd->IN_ep[epnum].xfer_buff);
}

/**
 * @brief       USBD SOF 回调函数
 * @param       hpcd    : PCD结构体指针
 * @retval      无
 */
void HAL_PCD_SOFCallback(PCD_HandleTypeDef *hpcd)
{
    USBD_LL_SOF((USBD_HandleTypeDef *)hpcd->pData);
}

/**
 * @brief       USBD 复位回调函数
 * @param       hpcd    : PCD结构体指针
 * @retval      无
 */
void HAL_PCD_ResetCallback(PCD_HandleTypeDef *hpcd)
{
    USBD_SpeedTypeDef speed = USBD_SPEED_FULL;

    /* Set USB Current Speed */
    switch (hpcd->Init.speed)
    {
    case PCD_SPEED_HIGH:
        USBD_DbgLog("已重置USB设备库  [HS]\r\n");
        speed = USBD_SPEED_HIGH;
        break;

    case PCD_SPEED_FULL:
        USBD_DbgLog("已重置USB设备库  [FS]\r\n");
        speed = USBD_SPEED_FULL;
        break;

    default:
        USBD_DbgLog("已重置USB设备库  [FS]\r\n");
        speed = USBD_SPEED_FULL;
        break;
    }

    USBD_LL_Reset((USBD_HandleTypeDef *)hpcd->pData); /* 复位设备 */
    USBD_LL_SetSpeed((USBD_HandleTypeDef *)hpcd->pData, speed);
}

/**
 * @brief       USBD 挂起回调函数
 * @param       hpcd    : PCD结构体指针
 * @retval      无
 */
void HAL_PCD_SuspendCallback(PCD_HandleTypeDef *hpcd)
{
    USB_DeviceState = 0;
    USBD_DbgLog("USB设备处于挂起.\r\n");
    USBD_LL_Suspend((USBD_HandleTypeDef *)hpcd->pData);
    __HAL_PCD_GATE_PHYCLOCK(hpcd);

    if (hpcd->Init.low_power_enable)
    {
        /* Set SLEEPDEEP bit and SleepOnExit of Cortex System Control Register. */
        SCB->SCR |= (uint32_t)((uint32_t)(SCB_SCR_SLEEPDEEP_Msk | SCB_SCR_SLEEPONEXIT_Msk));
    }
}

/**
 * @brief       USBD 恢复回调函数
 * @param       hpcd    : PCD结构体指针
 * @retval      无
 */
void HAL_PCD_ResumeCallback(PCD_HandleTypeDef *hpcd)
{
    USBD_DbgLog("USB设备已恢复.\r\n");
    USBD_LL_Resume((USBD_HandleTypeDef *)hpcd->pData);
}

/**
 * @brief       USBD ISO OUT 事务完成回调函数
 * @param       hpcd    : PCD结构体指针
 * @param       epnum   : 端点号
 * @retval      无
 */
void HAL_PCD_ISOOUTIncompleteCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
{
    USBD_LL_IsoOUTIncomplete((USBD_HandleTypeDef *)hpcd->pData, epnum);
}

/**
 * @brief       USBD ISO IN 事务完成回调函数
 * @param       hpcd    : PCD结构体指针
 * @param       epnum   : 端点号
 * @retval      无
 */
void HAL_PCD_ISOINIncompleteCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
{
    USBD_LL_IsoINIncomplete((USBD_HandleTypeDef *)hpcd->pData, epnum);
}

/**
 * @brief       USBD 连接成功回调函数
 * @param       hpcd    : PCD结构体指针
 * @retval      无
 */
void HAL_PCD_ConnectCallback(PCD_HandleTypeDef *hpcd)
{
    USB_DeviceState = 1;
    USBD_LL_DevConnected((USBD_HandleTypeDef *)hpcd->pData);
}

/**
 * @brief       USBD 断开连接回调函数
 * @param       hpcd    : PCD结构体指针
 * @retval      无
 */
void HAL_PCD_DisconnectCallback(PCD_HandleTypeDef *hpcd)
{
    USB_DeviceState = 0;
    USBD_DbgLog("USB已断开连接.\r\n");
    USBD_LL_DevDisconnected((USBD_HandleTypeDef *)hpcd->pData);
}

/**
 * @brief  Initializes the Low Level portion of the Device driver.
 * @param  pdev: Device handle
 * @retval USBD Status
 */
USBD_StatusTypeDef USBD_LL_Init(USBD_HandleTypeDef *pdev)
{
    PCD_HandleTypeStruct.pData = pdev;   /* PCD_HandleTypeStruct的pData指向pdev */
    pdev->pData = &PCD_HandleTypeStruct; /* pdev的pData指向PCD_HandleTypeStruct */

    PCD_HandleTypeStruct.Instance = USB_OTG_FS; /* 使用USB OTG FS */
    PCD_HandleTypeStruct.Init.dev_endpoints = 9;
    PCD_HandleTypeStruct.Init.speed = PCD_SPEED_FULL;
    PCD_HandleTypeStruct.Init.dma_enable = DISABLE;
    PCD_HandleTypeStruct.Init.phy_itface = PCD_PHY_EMBEDDED;
    PCD_HandleTypeStruct.Init.Sof_enable = DISABLE;
    PCD_HandleTypeStruct.Init.low_power_enable = DISABLE;
    PCD_HandleTypeStruct.Init.lpm_enable = DISABLE;
    PCD_HandleTypeStruct.Init.battery_charging_enable = DISABLE;
    PCD_HandleTypeStruct.Init.vbus_sensing_enable = DISABLE;
    PCD_HandleTypeStruct.Init.use_dedicated_ep1 = DISABLE;
    PCD_HandleTypeStruct.pData = pdev;   /* hpcd的pData指向pdev */
    pdev->pData = &PCD_HandleTypeStruct; /* pdev的pData指向hpcd */
    HAL_PCD_Init(&PCD_HandleTypeStruct);

    HAL_PCDEx_SetRxFiFo(&PCD_HandleTypeStruct, 0x80);    /* 设置接收FIFO大小为0X80(128字节) */
    HAL_PCDEx_SetTxFiFo(&PCD_HandleTypeStruct, 0, 0x40); /* 设置发送FIFO 0的大小为0X40(64字节) */
    HAL_PCDEx_SetTxFiFo(&PCD_HandleTypeStruct, 1, 0x80); /* 设置发送FIFO 1的大小为0X80(128字节) */

    return USBD_OK;
}

/**
 * @brief  De-Initializes the Low Level portion of the Device driver.
 * @param  pdev: Device handle
 * @retval USBD Status
 */
USBD_StatusTypeDef USBD_LL_DeInit(USBD_HandleTypeDef *pdev)
{
    HAL_StatusTypeDef tmpStatus = HAL_OK;
    USBD_StatusTypeDef status = USBD_OK;
    tmpStatus = HAL_PCD_DeInit(pdev->pData);
    status = USBD_GetDeviceStatus(tmpStatus);
    return status;
}

/**
 * @brief  Starts the Low Level portion of the Device driver.
 * @param  pdev: Device handle
 * @retval USBD Status
 */
USBD_StatusTypeDef USBD_LL_Start(USBD_HandleTypeDef *pdev)
{
    HAL_StatusTypeDef tmpStatus = HAL_OK;
    USBD_StatusTypeDef status = USBD_OK;
    tmpStatus = HAL_PCD_Start(pdev->pData);
    status = USBD_GetDeviceStatus(tmpStatus);
    return status;
}

/**
 * @brief  Stops the Low Level portion of the Device driver.
 * @param  pdev: Device handle
 * @retval USBD Status
 */
USBD_StatusTypeDef USBD_LL_Stop(USBD_HandleTypeDef *pdev)
{
    HAL_StatusTypeDef tmpStatus = HAL_OK;
    USBD_StatusTypeDef status = USBD_OK;
    tmpStatus = HAL_PCD_Stop(pdev->pData);
    status = USBD_GetDeviceStatus(tmpStatus);
    return status;
}

/**
 * @brief  Opens an endpoint of the Low Level Driver.
 * @param  pdev: Device handle
 * @param  ep_addr: Endpoint Number
 * @param  ep_type: Endpoint Type
 * @param  ep_mps: Endpoint Max Packet Size
 * @retval USBD Status
 */
USBD_StatusTypeDef USBD_LL_OpenEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr,
                                  uint8_t ep_type, uint16_t ep_mps)
{
    HAL_StatusTypeDef tmpStatus = HAL_OK;
    USBD_StatusTypeDef status = USBD_OK;
    tmpStatus = HAL_PCD_EP_Open(pdev->pData, ep_addr, ep_mps, ep_type);
    status = USBD_GetDeviceStatus(tmpStatus);
    return status;
}

/**
 * @brief  Closes an endpoint of the Low Level Driver.
 * @param  pdev: Device handle
 * @param  ep_addr: Endpoint Number
 * @retval USBD Status
 */
USBD_StatusTypeDef USBD_LL_CloseEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
    HAL_StatusTypeDef tmpStatus = HAL_OK;
    USBD_StatusTypeDef status = USBD_OK;
    tmpStatus = HAL_PCD_EP_Close(pdev->pData, ep_addr);
    status = USBD_GetDeviceStatus(tmpStatus);
    return status;
}

/**
 * @brief  Flushes an endpoint of the Low Level Driver.
 * @param  pdev: Device handle
 * @param  ep_addr: Endpoint Number
 * @retval USBD Status
 */
USBD_StatusTypeDef USBD_LL_FlushEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
    HAL_StatusTypeDef tmpStatus = HAL_OK;
    USBD_StatusTypeDef status = USBD_OK;
    tmpStatus = HAL_PCD_EP_Flush(pdev->pData, ep_addr);
    status = USBD_GetDeviceStatus(tmpStatus);
    return status;
}

/**
 * @brief  Sets a Stall condition on an endpoint of the Low Level Driver.
 * @param  pdev: Device handle
 * @param  ep_addr: Endpoint Number
 * @retval USBD Status
 */
USBD_StatusTypeDef USBD_LL_StallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
    HAL_StatusTypeDef tmpStatus = HAL_OK;
    USBD_StatusTypeDef status = USBD_OK;
    tmpStatus = HAL_PCD_EP_SetStall(pdev->pData, ep_addr);
    status = USBD_GetDeviceStatus(tmpStatus);
    return status;
}

/**
 * @brief  Clears a Stall condition on an endpoint of the Low Level Driver.
 * @param  pdev: Device handle
 * @param  ep_addr: Endpoint Number
 * @retval USBD Status
 */
USBD_StatusTypeDef USBD_LL_ClearStallEP(USBD_HandleTypeDef *pdev,
                                        uint8_t ep_addr)
{
    HAL_StatusTypeDef tmpStatus = HAL_OK;
    USBD_StatusTypeDef status = USBD_OK;
    tmpStatus = HAL_PCD_EP_ClrStall(pdev->pData, ep_addr);
    status = USBD_GetDeviceStatus(tmpStatus);
    return status;
}

/**
 * @brief  Returns Stall condition.
 * @param  pdev: Device handle
 * @param  ep_addr: Endpoint Number
 * @retval Stall (1: Yes, 0: No)
 */
uint8_t USBD_LL_IsStallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
    PCD_HandleTypeDef *hpcd = (PCD_HandleTypeDef *)pdev->pData;

    if ((ep_addr & 0x80) == 0x80)
    {
        return hpcd->IN_ep[ep_addr & 0x7F].is_stall;
    }
    else
    {
        return hpcd->OUT_ep[ep_addr & 0x7F].is_stall;
    }
}

/**
 * @brief  Assigns a USB address to the device.
 * @param  pdev: Device handle
 * @param  dev_addr: Endpoint Number
 * @retval USBD Status
 */
USBD_StatusTypeDef USBD_LL_SetUSBAddress(USBD_HandleTypeDef *pdev,
                                         uint8_t dev_addr)
{
    HAL_StatusTypeDef tmpStatus = HAL_OK;
    USBD_StatusTypeDef status = USBD_OK;
    tmpStatus = HAL_PCD_SetAddress(pdev->pData, dev_addr);
    status = USBD_GetDeviceStatus(tmpStatus);
    return status;
}

/**
 * @brief  Transmits data over an endpoint.
 * @param  pdev: Device handle
 * @param  ep_addr: Endpoint Number
 * @param  pbuf: Pointer to data to be sent
 * @param  size: Data size
 * @retval USBD Status
 */
USBD_StatusTypeDef USBD_LL_Transmit(USBD_HandleTypeDef *pdev, uint8_t ep_addr,
                                    uint8_t *pbuf, uint32_t size)
{
    HAL_StatusTypeDef tmpStatus = HAL_OK;
    USBD_StatusTypeDef status = USBD_OK;
    tmpStatus = HAL_PCD_EP_Transmit(pdev->pData, ep_addr, pbuf, size);
    status = USBD_GetDeviceStatus(tmpStatus);
    return status;
}

/**
 * @brief  Prepares an endpoint for reception.
 * @param  pdev: Device handle
 * @param  ep_addr: Endpoint Number
 * @param  pbuf: Pointer to data to be received
 * @param  size: Data size
 * @retval USBD Status
 */
USBD_StatusTypeDef USBD_LL_PrepareReceive(USBD_HandleTypeDef *pdev,
                                          uint8_t ep_addr, uint8_t *pbuf,
                                          uint32_t size)
{
    HAL_StatusTypeDef tmpStatus = HAL_OK;
    USBD_StatusTypeDef status = USBD_OK;
    tmpStatus = HAL_PCD_EP_Receive(pdev->pData, ep_addr, pbuf, size);
    status = USBD_GetDeviceStatus(tmpStatus);
    return status;
}

/**
 * @brief  Returns the last transferred packet size.
 * @param  pdev: Device handle
 * @param  ep_addr: Endpoint Number
 * @retval Received Data Size
 */
uint32_t USBD_LL_GetRxDataSize(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
    return HAL_PCD_EP_GetRxCount((PCD_HandleTypeDef *)pdev->pData, ep_addr);
}

#ifdef USBD_HS_TESTMODE_ENABLE
/**
 * @brief  Set High speed Test mode.
 * @param  pdev: Device handle
 * @param  testmode: test mode
 * @retval USBD Status
 */
USBD_StatusTypeDef USBD_LL_SetTestMode(USBD_HandleTypeDef *pdev, uint8_t testmode)
{
    HAL_StatusTypeDef tmpStatus = HAL_OK;
    USBD_StatusTypeDef status = USBD_OK;
    tmpStatus = HAL_PCD_SetTestMode((PCD_HandleTypeDef *)pdev->pData, testmode);
    status = USBD_GetDeviceStatus(tmpStatus);
    return status;
}
#endif /* USBD_HS_TESTMODE_ENABLE */

#include "bsp_system.h"

/**
 * @brief  Static single allocation.
 * @param  size: Size of allocated memory
 * @retval None
 */
void *USBD_malloc(uint32_t size)
{
    return Memory_Malloc(MP_RAM_EX, size);
}

/**
 * @brief  Dummy memory free
 * @param  p: Pointer to allocated  memory address
 * @retval None
 */
void USBD_free(void *p)
{
    Memory_Free(MP_RAM_EX, p);
}

/**
 * @brief       USBD 延时函数(以ms为单位)
 * @param       Delay   : 延时的ms数
 * @retval      无
 */
void USBD_LL_Delay(uint32_t Delay)
{
    HAL_Delay(Delay);
}

/**
 * @brief       返回USB的状态(HAL状态)
 * @param       tmpStatus : 当前的状态
 * @retval      无
 */
USBD_StatusTypeDef USBD_GetDeviceStatus(HAL_StatusTypeDef tmpStatus)
{
    USBD_StatusTypeDef status = USBD_OK;

    switch (tmpStatus)
    {
    case HAL_OK:
        status = USBD_OK;
        break;

    case HAL_ERROR:
        status = USBD_FAIL;
        break;

    case HAL_BUSY:
        status = USBD_BUSY;
        break;

    case HAL_TIMEOUT:
        status = USBD_FAIL;
        break;

    default:
        status = USBD_FAIL;
        break;
    }

    return status;
}
