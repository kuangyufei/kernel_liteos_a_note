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

#ifdef LOSCFG_FS_VFS
/**
 * @brief 虚拟串口设备路径定义
 * @details 定义不同类型虚拟串口的标准文件系统路径，用户空间通过这些路径访问串口设备
 * @note 这些路径需与文件系统挂载点保持一致
 */
#define SERIAL         "/dev/serial"         /* 默认虚拟串口设备路径 */
#define SERIAL_TTYGS0  "/dev/ttyGS0"        /* USB虚拟串口设备路径 */
#define SERIAL_UARTDEV "/dev/uartdev"       /* 硬件UART串口设备路径 */

/**
 * @brief 串口设备类型定义
 * @details 标识不同类型的串口设备，用于在驱动中区分处理逻辑
 */
#define SERIAL_TYPE_UART_DEV   1               /* UART硬件串口类型 */
#define SERIAL_TYPE_USBTTY_DEV 2               /* USB虚拟串口类型 */

/**
 * @brief 虚拟串口初始化函数
 * @details 创建并初始化虚拟串口设备节点，注册文件操作接口
 * @param[in] deviceName 设备名称，指定要创建的串口设备节点名称
 * @retval LOS_OK 初始化成功
 * @retval LOS_NOK 初始化失败，可能原因：设备节点创建失败或资源分配失败
 * @note 必须在VFS初始化完成后调用
 */
extern INT32 virtual_serial_init(const CHAR *deviceName);

/**
 * @brief 虚拟串口去初始化函数
 * @details 销毁虚拟串口设备节点，释放相关资源
 * @retval LOS_OK 去初始化成功
 * @retval LOS_NOK 去初始化失败，可能原因：设备不存在或资源释放失败
 * @note 通常在系统关机或串口模块卸载时调用
 */
extern INT32 virtual_serial_deinit(VOID);

/**
 * @brief 获取当前串口设备类型
 * @details 返回当前系统中激活的串口设备类型，用于上层应用适配不同硬件
 * @return UINT32 当前串口类型，取值为SERIAL_TYPE_UART_DEV或SERIAL_TYPE_USBTTY_DEV
 * @note 需在virtual_serial_init之后调用才返回有效结果
 */
extern UINT32 SerialTypeGet(VOID);

/**
 * @brief 虚拟串口控制块结构体
 * @details 维护虚拟串口的运行时状态信息，包括文件句柄和事件掩码
 */
typedef struct {
    struct file *filep;                        /* 文件操作句柄，关联VFS文件对象 */
    UINT32 mask;                               /* 事件掩码，记录串口待处理事件 */
} LOS_VIRSERIAL_CB;
#endif


#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _VIRTUAL_SERIAL_H */
