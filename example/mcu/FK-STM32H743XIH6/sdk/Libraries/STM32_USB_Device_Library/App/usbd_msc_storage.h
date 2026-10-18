/**
 * *****************************************************************************
 * @file    usbd_msc_storage.h
 * @author  MCD Application Team
 * @brief   Header file for the usbd_msc_storage.c file
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __USBD_MSC_STORAGE_H
#define __USBD_MSC_STORAGE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "usbd_msc.h"

extern USBD_StorageTypeDef USBD_MSC_Interface_fops_FS;

#ifdef __cplusplus
}
#endif

#endif /* __USBD_MSC_STORAGE_H */
