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

#include "los_random.h"
#include "fcntl.h"
#include "hisoc/random.h"
#include "linux/kernel.h"
#include "fs/driver.h"
/** 随机数操作接口结构体实例 */
static RandomOperations g_randomOp;

/**
 * @brief   初始化随机数操作接口
 * @param   r   随机数操作接口结构体指针
 */
void RandomOperationsInit(const RandomOperations *r)
{
    if (r != NULL) {
        // 复制操作接口到全局变量
        (void)memcpy_s(&g_randomOp, sizeof(RandomOperations), r, sizeof(RandomOperations));
    } else {
        PRINT_ERR("%s %d param is invalid\n", __FUNCTION__, __LINE__);
    }
    return;
}

/**
 * @brief   随机数硬件设备打开操作
 * @param   filep   文件操作指针
 * @return  ENOERR表示成功，-1表示失败
 */
static int RandomHwOpen(struct file *filep)
{
    if (g_randomOp.init != NULL) {
        g_randomOp.init();  // 调用初始化接口
        return ENOERR;
    }
    return -1;  // 初始化接口不存在
}

/**
 * @brief   随机数硬件设备关闭操作
 * @param   filep   文件操作指针
 * @return  ENOERR表示成功，-1表示失败
 */
static int RandomHwClose(struct file *filep)
{
    if (g_randomOp.deinit != NULL) {
        g_randomOp.deinit();  // 调用反初始化接口
        return ENOERR;
    }
    return -1;  // 反初始化接口不存在
}

/**
 * @brief   随机数硬件设备IO控制操作
 * @param   filep   文件操作指针
 * @param   cmd     IO控制命令
 * @param   arg     命令参数
 * @return  -EINVAL表示无效命令
 */
static int RandomHwIoctl(struct file *filep, int cmd, unsigned long arg)
{
    int ret = -1;

    switch (cmd) {
        default:
            PRINT_ERR("!!!bad command!!!\n");
            return -EINVAL;  // 未知命令
    }
    return ret;
}

/**
 * @brief   读取随机数数据
 * @param   filep   文件操作指针
 * @param   buffer  数据缓冲区
 * @param   buflen  缓冲区长度
 * @return  成功返回读取字节数，失败返回-1
 */
static ssize_t RandomHwRead(struct file *filep, char *buffer, size_t buflen)
{
    int ret = -1;

    if (g_randomOp.read != NULL) {
        ret = g_randomOp.read(buffer, buflen);  // 调用读取接口
        if (ret == ENOERR) {
            ret = buflen;  // 读取成功，返回实际读取长度
        }
    } else {
        ret = -1;  // 读取接口不存在
    }
    return ret;
}

/**
 * @brief   随机数设备内存映射操作（不支持）
 * @param   filep   文件操作指针
 * @param   region  内存映射区域结构体
 * @return  0（始终返回0，实际不支持映射）
 */
static ssize_t RandomMap(struct file *filep, LosVmMapRegion *region)
{
    PRINTK("%s %d, mmap is not support\n", __FUNCTION__, __LINE__);
    return 0;
}

/**
 * @brief   随机数硬件设备文件操作结构体
 */
static const struct file_operations_vfs g_randomHwDevOps = {
    RandomHwOpen,  /* open：打开设备 */
    RandomHwClose, /* close：关闭设备 */
    RandomHwRead,  /* read：读取随机数 */
    NULL,            /* write：未实现 */
    NULL,            /* seek：未实现 */
    RandomHwIoctl, /* ioctl：设备控制 */
    RandomMap,      /* mmap：内存映射（不支持） */
#ifndef CONFIG_DISABLE_POLL
    NULL,            /* poll：未实现 */
#endif
    NULL,            /* unlink：未实现 */
};

/**
 * @brief   注册urandom设备驱动
 * @return  0表示成功，-EPERM表示权限被拒绝
 */
int DevUrandomRegister(void)
{
    if (g_randomOp.support != NULL) {
        int ret = g_randomOp.support();  // 检查硬件支持情况
        if (ret) {
            // 注册字符设备，文件权限为0666
            return register_driver("/dev/urandom", &g_randomHwDevOps, 0666, 0); /* 0666: 文件权限 */
        }
    }
    return -EPERM;  // 硬件不支持或接口未初始化
}