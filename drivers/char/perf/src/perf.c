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

#include "fcntl.h"
#include "user_copy.h"
#include "sys/ioctl.h"
#include "fs/driver.h"
#include "los_dev_perf.h"
#include "los_perf.h"
#include "los_init.h"

#define PERF_DRIVER "/dev/perf"
#define PERF_DRIVER_MODE 0666
/* perf ioctl 命令定义 */
#define PERF_IOC_MAGIC     'T'                     // IOCTL魔术字
#define PERF_START         _IO(PERF_IOC_MAGIC, 1)  // 开始性能采样命令
#define PERF_STOP          _IO(PERF_IOC_MAGIC, 2)  // 停止性能采样命令

/**
 * @brief   性能设备打开操作
 * @param   filep   文件操作指针
 * @return  0表示成功
 */
static int PerfOpen(struct file *filep)
{
    (void)filep;  // 未使用的参数
    return 0;     // 始终返回成功
}

/**
 * @brief   性能设备关闭操作
 * @param   filep   文件操作指针
 * @return  0表示成功
 */
static int PerfClose(struct file *filep)
{
    (void)filep;  // 未使用的参数
    return 0;     // 始终返回成功
}

/**
 * @brief   读取性能采样数据
 * @param   filep   文件操作指针
 * @param   buffer  用户空间缓冲区
 * @param   buflen  缓冲区长度
 * @return  成功返回读取字节数，失败返回负数错误码
 */
static ssize_t PerfRead(struct file *filep, char *buffer, size_t buflen)
{
    /* perf 记录缓冲区读取 */
    (void)filep;  // 未使用的参数
    int ret;      // 返回值
    int realLen;  // 实际读取长度

    // 分配内核缓冲区
    char *records = LOS_MemAlloc(m_aucSysMem0, buflen);
    if (records == NULL) {
        return -ENOMEM;  // 内存分配失败
    }

    realLen = LOS_PerfDataRead(records, buflen); /* 获取采样数据 */
    if (realLen == 0) {
        PRINT_ERR("Perf read failed, check whether perf is configured to sample mode.\n");
        ret = -EINVAL;  // 读取失败，返回无效参数错误
        goto EXIT;      // 跳转到退出处理
    }

    // 将数据从内核空间复制到用户空间
    ret = LOS_CopyFromKernel((void *)buffer, buflen, (void *)records, realLen);
    if (ret != 0) {
        ret = -EINVAL;  // 复制失败
        goto EXIT;      // 跳转到退出处理
    }

    ret = realLen;  // 成功，返回实际读取长度
EXIT:
    LOS_MemFree(m_aucSysMem0, records);  // 释放内核缓冲区
    return ret;
}

/**
 * @brief   配置性能采样参数
 * @param   filep   文件操作指针
 * @param   buffer  用户空间配置数据
 * @param   buflen  配置数据长度
 * @return  0表示成功，负数表示失败
 */
static ssize_t PerfConfig(struct file *filep, const char *buffer, size_t buflen)
{
    (void)filep;  // 未使用的参数
    int ret;      // 返回值
    PerfConfigAttr attr = {0};  // 配置属性结构体
    int attrlen = sizeof(PerfConfigAttr);  // 结构体大小

    // 检查输入长度是否匹配
    if (buflen != attrlen) {
        PRINT_ERR("PerfConfigAttr is %d bytes not %d\n", attrlen, buflen);
        return -EINVAL;  // 长度不匹配，返回错误
    }

    // 将配置数据从用户空间复制到内核空间
    ret = LOS_CopyToKernel(&attr, attrlen, buffer, buflen);
    if (ret != 0) {
        return -EINVAL;  // 复制失败
    }

    // 应用性能配置
    ret = LOS_PerfConfig(&attr);
    if (ret != LOS_OK) {
        PRINT_ERR("perf config error %u\n", ret);
        return -EINVAL;  // 配置失败
    }

    return 0;  // 配置成功
}

/**
 * @brief   性能设备IO控制操作
 * @param   filep   文件操作指针
 * @param   cmd     IO控制命令
 * @param   arg     命令参数
 * @return  0表示成功，负数表示失败
 */
static int PerfIoctl(struct file *filep, int cmd, unsigned long arg)
{
    (void)filep;  // 未使用的参数
    switch (cmd) {
        case PERF_START:
            LOS_PerfStart((UINT32)arg);  // 开始性能采样
            break;
        case PERF_STOP:
            LOS_PerfStop();  // 停止性能采样
            break;
        default:
            PRINT_ERR("Unknown perf ioctl cmd:%d\n", cmd);
            return -EINVAL;  // 未知命令
    }
    return 0;  // 成功
}

/**
 * @brief   性能设备文件操作结构体
 */
static const struct file_operations_vfs g_perfDevOps = {
    PerfOpen,        /* open：打开设备 */
    PerfClose,       /* close：关闭设备 */
    PerfRead,        /* read：读取采样数据 */
    PerfConfig,      /* write：配置采样参数 */
    NULL,            /* seek：未实现 */
    PerfIoctl,       /* ioctl：控制采样 */
    NULL,            /* mmap：未实现 */
#ifndef CONFIG_DISABLE_POLL
    NULL,            /* poll：未实现 */
#endif
    NULL,            /* unlink：未实现 */
};

/**
 * @brief   注册性能设备驱动
 * @return  0表示成功，负数表示失败
 */
int DevPerfRegister(void)
{
    // 注册字符设备，文件权限为0666
    return register_driver(PERF_DRIVER, &g_perfDevOps, PERF_DRIVER_MODE, 0); /* 0666: 文件权限 */
}

// 模块初始化：在扩展内核模块阶段注册性能设备
LOS_MODULE_INIT(DevPerfRegister, LOS_INIT_LEVEL_KMOD_EXTENDED);