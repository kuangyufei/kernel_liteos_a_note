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

#ifndef __LOS_RANDOM_H__
#define __LOS_RANDOM_H__

#include "los_typedef.h"
#include "sys/ioctl.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */


/* 随机数设备IO控制宏定义 */
#define RAMDOM_IOC_MAGIC    'r'                     /* 随机数设备IO控制幻数 (ASCII: 'r') */
#define RANDOM_SET_MAX      _IO(RAMDOM_IOC_MAGIC, 1) /* 设置随机数最大值命令 (命令编号1) */

/* 随机数设备注册函数 */
int DevRandomRegister(void);                        /* 注册/dev/random设备 */
int DevUrandomRegister(void);                       /* 注册/dev/urandom设备 */

/**
 * @brief 硬件随机数生成器操作接口结构体
 * @details 定义硬件随机数生成器的标准操作集合，包括设备检测、初始化、去初始化、数据读取和控制命令
 */
typedef struct {
    int (*support)(void);        /* 检测硬件随机数支持状态 (返回0表示支持) */
    void (*init)(void);          /* 初始化硬件随机数生成器 */
    void (*deinit)(void);        /* 去初始化硬件随机数生成器 */
    int (*read)(char *buffer, size_t buflen); /* 读取随机数到缓冲区
                                              @param buffer [out] 接收随机数的缓冲区
                                              @param buflen [in] 期望读取的字节数
                                              @return 实际读取的字节数，负数表示失败 */
    int (*ioctl)(int cmd, unsigned long arg); /* 硬件随机数控制命令
                                              @param cmd [in] 控制命令码
                                              @param arg [in/out] 命令参数
                                              @return 0表示成功，负数表示失败 */
} RandomOperations;

/**
 * @brief 初始化随机数操作接口
 * @param r [in] 指向RandomOperations结构体的指针
 */
void RandomOperationsInit(const RandomOperations *r);


#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif
