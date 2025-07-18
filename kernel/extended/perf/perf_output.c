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

#include "perf_output_pri.h"
/**
 * @brief 性能缓冲区水位通知钩子
 * @note 当缓冲区使用量达到水印阈值时触发
 */
STATIC PERF_BUF_NOTIFY_HOOK g_perfBufNotifyHook = NULL;  // 水位通知钩子函数指针
/**
 * @brief 性能缓冲区刷新钩子
 * @note 用于自定义缓冲区数据刷新逻辑
 */
STATIC PERF_BUF_FLUSH_HOOK g_perfBufFlushHook = NULL;    // 缓冲区刷新钩子函数指针
/**
 * @brief 性能输出控制块
 * @note 管理性能数据输出的缓冲区及控制参数
 */
STATIC PerfOutputCB g_perfOutputCb;                      // 性能输出控制块实例

/**
 * @brief 默认的性能缓冲区水位通知函数
 * @return 无
 * @note 输出缓冲区达到水印阈值的提示信息
 */
STATIC VOID OsPerfDefaultNotify(VOID)
{
    PRINT_INFO("perf buf waterline notify!\n");  // 打印水位通知日志
}

/**
 * @brief 初始化性能输出缓冲区
 * @param buf 外部提供的缓冲区地址，为NULL时自动分配
 * @param size 缓冲区大小(字节)
 * @return LOS_OK 初始化成功；LOS_NOK 初始化失败
 * @note 若buf为NULL，将从系统内存池分配内存并自动释放
 */
UINT32 OsPerfOutputInit(VOID *buf, UINT32 size)
{
    UINT32 ret;                                      // 函数返回值
    BOOL releaseFlag = FALSE;                        // 内存释放标志：TRUE表示需要自动释放
    if (buf == NULL) {                               // 检查是否需要内部分配缓冲区
        buf = LOS_MemAlloc(m_aucSysMem1, size);      // 从系统内存池分配缓冲区
        if (buf == NULL) {                           // 检查分配结果
            return LOS_NOK;                          // 分配失败，返回错误
        } else {
            releaseFlag = TRUE;                      // 设置自动释放标志
        }
    }
    ret = LOS_CirBufInit(&g_perfOutputCb.ringbuf, buf, size);  // 初始化循环缓冲区
    if (ret != LOS_OK) {                             // 检查循环缓冲区初始化结果
        goto RELEASE;                                // 初始化失败，跳转到释放逻辑
    }
    g_perfOutputCb.waterMark = size / PERF_BUFFER_WATERMARK_ONE_N;  // 计算水印阈值(缓冲区大小的1/N)
    g_perfBufNotifyHook = OsPerfDefaultNotify;       // 设置默认水位通知钩子
    return ret;                                      // 返回成功
RELEASE:
    if (releaseFlag) {                               // 检查是否需要释放内存
        (VOID)LOS_MemFree(m_aucSysMem1, buf);        // 释放已分配的内存
    }
    return ret;                                      // 返回错误
}

/**
 * @brief 刷新性能缓冲区数据
 * @return 无
 * @note 调用注册的刷新钩子函数，将缓冲区数据输出
 */
VOID OsPerfOutputFlush(VOID)
{
    if (g_perfBufFlushHook != NULL) {                // 检查刷新钩子是否注册
        g_perfBufFlushHook(g_perfOutputCb.ringbuf.fifo, g_perfOutputCb.ringbuf.size);  // 调用钩子函数
    }
}

/**
 * @brief 从性能缓冲区读取数据
 * @param dest 目标缓冲区地址
 * @param size 要读取的字节数
 * @return 实际读取的字节数
 * @note 读取前会先刷新缓冲区
 */
UINT32 OsPerfOutputRead(CHAR *dest, UINT32 size)
{
    OsPerfOutputFlush();                             // 读取前刷新缓冲区
    return LOS_CirBufRead(&g_perfOutputCb.ringbuf, dest, size);  // 从循环缓冲区读取数据
}

/**
 * @brief 检查性能缓冲区是否有足够空间
 * @param size 待写入的数据大小(字节)
 * @return TRUE 空间足够；FALSE 空间不足
 * @note 内部使用的静态辅助函数
 */
STATIC BOOL OsPerfOutputBegin(UINT32 size)
{
    if (g_perfOutputCb.ringbuf.remain < size) {      // 检查剩余空间是否充足
        PRINT_INFO("perf buf has no enough space for 0x%x\n", size);  // 打印空间不足日志
        return FALSE;                                // 返回失败
    }
    return TRUE;                                     // 返回成功
}

/**
 * @brief 完成性能数据写入后处理
 * @return 无
 * @note 刷新缓冲区并检查是否达到水印阈值
 */
STATIC VOID OsPerfOutputEnd(VOID)
{
    OsPerfOutputFlush();                             // 刷新缓冲区数据
    // 检查缓冲区使用量是否达到水印阈值
    if (LOS_CirBufUsedSize(&g_perfOutputCb.ringbuf) >= g_perfOutputCb.waterMark) {
        if (g_perfBufNotifyHook != NULL) {           // 检查通知钩子是否注册
            g_perfBufNotifyHook();                   // 调用水位通知钩子
        }
    }
}

/**
 * @brief 向性能缓冲区写入数据
 * @param data 待写入数据地址
 * @param size 数据大小(字节)
 * @return LOS_OK 写入成功；LOS_NOK 写入失败
 * @note 内部会先检查空间，写入后触发后处理
 */
UINT32 OsPerfOutputWrite(CHAR *data, UINT32 size)
{
    if (!OsPerfOutputBegin(size)) {                  // 检查空间是否充足
        return LOS_NOK;                              // 空间不足，返回错误
    }

    LOS_CirBufWrite(&g_perfOutputCb.ringbuf, data, size);  // 写入数据到循环缓冲区

    OsPerfOutputEnd();                               // 执行写入后处理
    return LOS_OK;                                   // 返回成功
}

/**
 * @brief 输出性能缓冲区信息
 * @return 无
 * @note 打印缓冲区地址和大小等调试信息
 */
VOID OsPerfOutputInfo(VOID)
{
    PRINT_EMG("dump perf data, addr: %p length: %#x\n", g_perfOutputCb.ringbuf.fifo, g_perfOutputCb.ringbuf.size);  // 打印缓冲区信息
}

/**
 * @brief 注册性能缓冲区水位通知钩子
 * @param func 通知钩子函数指针
 * @return 无
 * @note 注册后将覆盖默认通知函数
 */
VOID OsPerfNotifyHookReg(const PERF_BUF_NOTIFY_HOOK func)
{
    g_perfBufNotifyHook = func;                      // 更新通知钩子函数指针
}

/**
 * @brief 注册性能缓冲区刷新钩子
 * @param func 刷新钩子函数指针
 * @return 无
 * @note 用于自定义数据刷新逻辑
 */
VOID OsPerfFlushHookReg(const PERF_BUF_FLUSH_HOOK func)
{
    g_perfBufFlushHook = func;                       // 更新刷新钩子函数指针
}
