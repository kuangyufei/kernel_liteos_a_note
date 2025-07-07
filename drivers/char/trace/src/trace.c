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
#include "linux/kernel.h"
#include "sys/ioctl.h"
#include "fs/driver.h"
#include "los_dev_trace.h"
#include "los_trace.h"
#include "los_hook.h"
#include "los_init.h"

#define TRACE_DRIVER "/dev/trace"
#define TRACE_DRIVER_MODE 0666

/* trace ioctl */
#define TRACE_IOC_MAGIC     'T'
#define TRACE_START         _IO(TRACE_IOC_MAGIC, 1)
#define TRACE_STOP          _IO(TRACE_IOC_MAGIC, 2)
#define TRACE_RESET         _IO(TRACE_IOC_MAGIC, 3)
#define TRACE_DUMP          _IO(TRACE_IOC_MAGIC, 4)
#define TRACE_SET_MASK      _IO(TRACE_IOC_MAGIC, 5)

static int TraceOpen(struct file *filep)
{
    return 0;
}

static int TraceClose(struct file *filep)
{
    return 0;
}
/**
 * @brief 读取跟踪记录缓冲区数据
 * @param[in] filep 文件指针
 * @param[out] buffer 用户空间缓冲区
 * @param[in] buflen 请求读取的长度
 * @return 成功读取的字节数；负数 失败
 * @note 要求读取长度必须4字节对齐，仅支持离线模式下的跟踪记录读取
 */
static ssize_t TraceRead(struct file *filep, char *buffer, size_t buflen)
{
    /* 跟踪记录缓冲区读取 */
    ssize_t len = buflen;        // 请求读取的长度
    OfflineHead *records;        // 跟踪记录头指针
    int ret;                     // 系统调用返回值
    int realLen;                 // 实际可读取的长度

    if (len % sizeof(unsigned int)) {                  // 检查缓冲区是否4字节对齐
        PRINT_ERR("Buffer size not aligned by 4 bytes\n");
        return -EINVAL;                               // 返回参数错误
    }

    records = LOS_TraceRecordGet();                    // 获取跟踪记录缓冲区
    if (records == NULL) {                            // 检查缓冲区是否有效
        PRINT_ERR("Trace read failed, check whether trace mode is set to offline\n");
        return -EINVAL;                               // 返回无效参数错误
    }

    // 计算实际可读取长度（取请求长度与缓冲区总长度的较小值）
    realLen = buflen < records->totalLen ? buflen : records->totalLen;
    // 将跟踪记录从内核空间拷贝到用户空间
    ret = LOS_CopyFromKernel((void *)buffer, buflen, (void *)records, realLen);
    if (ret != 0) {                                    // 检查拷贝是否成功
        return -EINVAL;                               // 拷贝失败返回错误
    }

    return realLen;                                    // 返回实际读取的字节数
}

/**
 * @brief 写入用户事件到跟踪系统
 * @param[in] filep 文件指针
 * @param[in] buffer 用户空间缓冲区，存储事件信息
 * @param[in] buflen 写入数据长度
 * @return 0 成功；负数 失败
 * @note 要求写入长度必须等于UsrEventInfo结构体大小
 */
static ssize_t TraceWrite(struct file *filep, const char *buffer, size_t buflen)
{
    /* 此处处理用户事件跟踪 */
    int ret;                                          // 系统调用返回值
    UsrEventInfo *info = NULL;                        // 用户事件信息结构体指针
    int infoLen = sizeof(UsrEventInfo);               // 用户事件信息结构体大小

    if (buflen != infoLen) {                           // 检查输入长度是否匹配结构体大小
        PRINT_ERR("Buffer size not %d bytes\n", infoLen);
        return -EINVAL;                               // 返回参数错误
    }

    // 从系统内存分配用户事件信息结构体
    info = LOS_MemAlloc(m_aucSysMem0, infoLen);
    if (info == NULL) {                               // 检查内存分配是否成功
        return -ENOMEM;                               // 返回内存不足错误
    }
    (void)memset_s(info, infoLen, 0, infoLen);         // 初始化结构体内存

    // 将用户空间数据拷贝到内核空间
    ret = LOS_CopyToKernel(info, infoLen, buffer, buflen);
    if (ret != 0) {                                    // 检查拷贝是否成功
        LOS_MemFree(m_aucSysMem0, info);               // 拷贝失败释放内存
        return -EINVAL;                               // 返回参数错误
    }
    // 调用用户事件钩子函数处理事件
    OsHookCall(LOS_HOOK_TYPE_USR_EVENT, info, infoLen);
    return 0;
}

/**
 * @brief 跟踪设备IO控制函数
 * @param[in] filep 文件指针
 * @param[in] cmd 控制命令
 * @param[in] arg 命令参数
 * @return 0 成功；负数 失败
 * @note 支持的命令包括：启动跟踪、停止跟踪、重置跟踪、导出跟踪数据、设置事件掩码
 */
static int TraceIoctl(struct file *filep, int cmd, unsigned long arg)
{
    switch (cmd) {
        case TRACE_START:                              // 启动跟踪命令
            return LOS_TraceStart();                   // 调用启动跟踪函数
        case TRACE_STOP:                               // 停止跟踪命令
            LOS_TraceStop();                           // 调用停止跟踪函数
            break;
        case TRACE_RESET:                              // 重置跟踪命令
            LOS_TraceReset();                          // 调用重置跟踪函数
            break;
        case TRACE_DUMP:                               // 导出跟踪数据命令
            LOS_TraceRecordDump((BOOL)arg);            // 调用导出跟踪记录函数
            break;
        case TRACE_SET_MASK:                           // 设置事件掩码命令
            LOS_TraceEventMaskSet((UINT32)arg);        // 调用设置事件掩码函数
            break;
        default:                                       // 未知命令
            PRINT_ERR("Unknown trace ioctl cmd:%d\n", cmd);
            return -EINVAL;                           // 返回无效命令错误
    }
    return 0;
}

// 跟踪设备文件操作结构体
static const struct file_operations_vfs g_traceDevOps = {
    TraceOpen,       /* 打开设备 */
    TraceClose,      /* 关闭设备 */
    TraceRead,       /* 读取数据 */
    TraceWrite,      /* 写入数据 */
    NULL,            /* 定位操作（未实现） */
    TraceIoctl,      /* IO控制 */
    NULL,            /* 内存映射（未实现） */
#ifndef CONFIG_DISABLE_POLL
    NULL,            /* 轮询操作（未实现） */
#endif
    NULL,            /* 删除操作（未实现） */
};

/**
 * @brief 注册跟踪设备驱动
 * @return 0 成功；其他 失败
 * @note 设备路径由TRACE_DRIVER宏定义，权限由TRACE_DRIVER_MODE宏定义
 */
int DevTraceRegister(void)
{
    // 注册字符设备驱动
    return register_driver(TRACE_DRIVER, &g_traceDevOps, TRACE_DRIVER_MODE, 0); /* 0666: 文件权限 */
}

// 模块初始化：在扩展内核模块初始化阶段注册跟踪设备
LOS_MODULE_INIT(DevTraceRegister, LOS_INIT_LEVEL_KMOD_EXTENDED);