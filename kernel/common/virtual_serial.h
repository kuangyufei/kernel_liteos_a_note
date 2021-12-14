/*
 * Copyright (c) 2013-2019 Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020-2021 Huawei Device Co., Ltd. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this list of
 *    conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list
 *    of conditions and the following disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors may be used
 *    to endorse or promote products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _VIRTUAL_SERIAL_H
#define _VIRTUAL_SERIAL_H

#include "los_config.h"
#include "fs/file.h"
#if defined(LOSCFG_DRIVERS_USB_SERIAL_GADGET) || defined(LOSCFG_DRIVERS_USB_ETH_SER_GADGET)
#include "implementation/usb_api_pri.h"
#endif

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */
/*
	鸿蒙将 PIN、I2C、SPI、USB、UART 等作为外设设备，统一通过设备注册完成。
	实现了按名称访问的设备管理子系统，可按照统一的 API 界面访问硬件设备。
	在设备驱动接口上，根据嵌入式系统的特点，对不同的设备可以挂接相应的事件。
	当设备事件触发时，由驱动程序通知给上层的应用程序。
*/

#ifdef LOSCFG_FS_VFS //将设备虚拟为文件统一来操作,对鸿蒙来说一切皆为文件
#define SERIAL         "/dev/serial"	///< 虚拟串口设备
#define SERIAL_TTYGS0  "/dev/ttyGS0" 	///< USB类型的串口
#define SERIAL_UARTDEV "/dev/uartdev"	///< Uart类型的串口 

//UART（Universal Asynchronous Receiver/Transmitter）通用异步收发传输器，UART 作为异步串口通信协议的一种，
//工作原理是将传输数据的每个字符一位接一位地传输。是在应用程序开发过程中使用频率最高的数据总线。

#define SERIAL_TYPE_UART_DEV   1
#define SERIAL_TYPE_USBTTY_DEV 2

extern INT32 virtual_serial_init(const CHAR *deviceName);
extern INT32 virtual_serial_deinit(VOID);

extern UINT32 SerialTypeGet(VOID);

typedef struct {
    struct file *filep;
    UINT32 mask;
} LOS_VIRSERIAL_CB;

#endif

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _VIRTUAL_SERIAL_H */
