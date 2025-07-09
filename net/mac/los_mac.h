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

#ifndef _LOS_MAC_H
#define _LOS_MAC_H

#include "lwip/netif.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

/**
 * @brief 以太网驱动程序结构体
 * 
 * 包含以太网驱动的上下文信息和网络接口
 */
struct los_eth_driver {
    void* driver_context;       // 驱动上下文指针，指向具体硬件驱动的私有数据
    struct netif ac_if;         // LwIP网络接口结构体，包含IP地址、子网掩码等网络配置
};                              // los_eth_driver结构体结束

/**
 * @brief 以太网驱动操作函数集
 * 
 * 定义以太网驱动必须实现的初始化、接收和发送完成回调函数
 */
struct los_eth_funs {           // 以太网功能函数结构体
    /**
     * @brief 初始化以太网驱动
     * @param drv 以太网驱动结构体指针
     * @param mac_addr MAC地址指针（6字节）
     */
    void (*init)(struct los_eth_driver *drv, unsigned char *mac_addr);
    
    /**
     * @brief 接收数据回调函数
     * @param drv 以太网驱动结构体指针
     * @param len 接收数据长度（字节）
     */
    void (*recv)(struct los_eth_driver *drv, int len);
    
    /**
     * @brief 发送完成回调函数
     * @param drv 以太网驱动结构体指针
     * @param key 发送请求标识
     * @param state 发送状态（0表示成功，非0表示失败）
     */
    void (*send_complete)(struct los_eth_driver *drv, unsigned int key, int state);
};                              // los_eth_funs结构体结束

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_MAC_H */
