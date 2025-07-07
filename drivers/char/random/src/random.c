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
#include "linux/kernel.h"
#include "fs/driver.h"

// 随机数最大值，设置为 0x7FFFFFFF（2^31 - 1）
static unsigned long g_randomMax =  0x7FFFFFFF;

/**
 * @brief 生成下一个伪随机数
 * @param[in,out] value 输入当前种子值，输出新的种子值
 * @return 生成的伪随机数（范围：0 ~ g_randomMax）
 * @note 使用线性同余法（LCG）实现，公式为：t = 16807 * (value % 127773) - 2836 * (value / 127773)
 */
static long DoRand(unsigned long *value)
{
    long quotient, remainder, t;

    quotient = *value / 127773L;  // 计算商
    remainder = *value % 127773L; // 计算余数
    t = 16807L * remainder - 2836L * quotient; // LCG核心计算
    if (t <= 0) {                  // 确保结果为正数
        t += 0x7fffffff;
    }
    return ((*value = t) % (g_randomMax + 1)); // 更新种子并返回范围内随机数
}

static unsigned long g_seed = 1; // 随机数种子，初始值为1

/**
 * @brief 打开随机设备时初始化种子
 * @param[in] filep 文件指针
 * @return 0 成功；其他 失败
 * @note 使用当前纳秒时间作为种子，提高随机性
 */
int RanOpen(struct file *filep)
{
    g_seed = (unsigned long)(LOS_CurrNanosec() & 0xffffffff); // 取当前纳秒时间低32位作为种子
    return 0;
}

/**
 * @brief 关闭随机设备（空实现）
 * @param[in] filep 文件指针
 * @return 0 成功
 */
static int RanClose(struct file *filep)
{
    return 0;
}

/**
 * @brief 随机设备IO控制（不支持）
 * @param[in] filep 文件指针
 * @param[in] cmd 控制命令
 * @param[in] arg 参数
 * @return -ENOTSUP 不支持该操作
 */
int RanIoctl(struct file *filep, int cmd, unsigned long arg)
{
    PRINT_ERR("random ioctl is not supported\n"); // 打印不支持信息
    return -ENOTSUP;                               // 返回不支持错误码
}

/**
 * @brief 从随机设备读取数据
 * @param[in] filep 文件指针
 * @param[out] buffer 用户空间缓冲区
 * @param[in] buflen 读取长度
 * @return 成功读取的字节数；负数 失败
 * @note 要求读取长度必须是4字节对齐，每次生成4字节随机数并拷贝到用户空间
 */
ssize_t RanRead(struct file *filep, char *buffer, size_t buflen)
{
    ssize_t len = buflen;       // 剩余需要读取的长度
    char *buf = buffer;         // 用户空间缓冲区指针
    unsigned int temp;          // 临时存储生成的随机数
    int ret;                    // 系统调用返回值

    if (len % sizeof(unsigned int)) {                  // 检查长度是否4字节对齐
        PRINT_ERR("random size not aligned by 4 bytes\n");
        return -EINVAL;                               // 返回参数错误
    }
    while (len > 0) {                                 // 循环生成并拷贝随机数
        temp = DoRand(&g_seed);                       // 生成4字节随机数
        // 将随机数从内核空间拷贝到用户空间
        ret = LOS_CopyFromKernel((void *)buf, sizeof(unsigned int), (void *)&temp, sizeof(unsigned int));
        if (ret) {                                    // 拷贝失败则退出循环
            break;
        }
        len -= sizeof(unsigned int);                  // 更新剩余长度
        buf += sizeof(unsigned int);                  // 移动用户缓冲区指针
    }
    return (buflen - len); /* 返回成功读取的字节数 */
}

/**
 * @brief 随机设备内存映射（不支持）
 * @param[in] filep 文件指针
 * @param[in] region 内存映射区域
 * @return 0 成功
 */
static ssize_t RanMap(struct file *filep, LosVmMapRegion *region)
{
    PRINTK("%s %d, mmap is not support\n", __FUNCTION__, __LINE__); // 打印不支持信息
    return 0;
}

// 随机设备文件操作结构体
static const struct file_operations_vfs g_ranDevOps = {
    RanOpen,  /* 打开设备 */
    RanClose, /* 关闭设备 */
    RanRead,  /* 读取数据 */
    NULL,      /* 写入数据（未实现） */
    NULL,      /* 定位操作（未实现） */
    RanIoctl, /* IO控制 */
    RanMap,   /* 内存映射 */
#ifndef CONFIG_DISABLE_POLL
    NULL,      /* 轮询操作（未实现） */
#endif
    NULL,      /* 删除操作（未实现） */
};

/**
 * @brief 注册随机设备
 * @return 0 成功；其他 失败
 * @note 设备路径为/dev/random，权限为0666（所有用户可读写）
 */
int DevRandomRegister(void)
{
    return register_driver("/dev/random", &g_ranDevOps, 0666, 0); /* 0666: 文件权限 */
}
