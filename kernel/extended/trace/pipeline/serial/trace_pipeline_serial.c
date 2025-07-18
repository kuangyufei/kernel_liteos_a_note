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

#include "trace_pipeline_serial.h"
#include "trace_pipeline.h"
/**
 * @brief 串口管道初始化函数
 * @details 根据LOSCFG_TRACE_CONTROL_AGENT宏定义，提供不同的初始化实现
 * @return 成功返回LOS_OK，失败返回错误码
 */
#ifdef LOSCFG_TRACE_CONTROL_AGENT
/**
 * @brief 使能跟踪控制代理时的串口管道初始化
 * @return 串口硬件初始化结果，成功返回0，失败返回非0
 */
UINT32 SerialPipelineInit(VOID)
{
    return uart_hwiCreate();  // 创建UART硬件接口
}

/**
 * @brief 使能跟踪控制代理时的串口数据接收
 * @param[in] data 接收缓冲区指针
 * @param[in] size 期望接收的数据大小
 * @param[in] timeout 超时时间(ms)
 * @return 实际接收的字节数
 */
UINT32 SerialDataReceive(UINT8 *data, UINT32 size, UINT32 timeout)
{
    return uart_read(data, size, timeout);  // 从UART读取数据
}

/**
 * @brief 使能跟踪控制代理时的串口等待
 * @return 等待结果，成功返回0，失败返回非0
 */
UINT32 SerialWait(VOID)
{
    return uart_wait_adapt();  // 等待UART操作完成
}

#else

/**
 * @brief 未使能跟踪控制代理时的串口管道初始化
 * @return 始终返回LOS_OK
 */
UINT32 SerialPipelineInit(VOID)
{
    return LOS_OK;  // 默认初始化成功
}

/**
 * @brief 未使能跟踪控制代理时的串口数据接收
 * @param[in] data 接收缓冲区指针(未使用)
 * @param[in] size 期望接收的数据大小(未使用)
 * @param[in] timeout 超时时间(ms)(未使用)
 * @return 始终返回LOS_OK
 */
UINT32 SerialDataReceive(UINT8 *data, UINT32 size, UINT32 timeout)
{
    (VOID)data;    // 未使用的参数
    (VOID)size;    // 未使用的参数
    (VOID)timeout; // 未使用的参数
    return LOS_OK; // 默认接收成功
}

/**
 * @brief 未使能跟踪控制代理时的串口等待
 * @return 始终返回LOS_OK
 */
UINT32 SerialWait(VOID)
{
    return LOS_OK;  // 默认等待成功
}
#endif

/**
 * @brief 串口数据发送函数
 * @param[in] len 发送数据长度
 * @param[in] data 发送数据缓冲区指针
 * @note 无论是否使能跟踪控制代理，实现相同
 */
VOID SerialDataSend(UINT16 len, UINT8 *data)
{
    UartPuts((CHAR *)data, len, 1);  // 通过UART发送字符串，第三个参数为发送标志(1表示阻塞发送)
}

/**
 * @brief 串口管道操作集
 * @details 实现TracePipelineOps接口，提供串口方式的跟踪数据传输
 */
STATIC const TracePipelineOps g_serialOps = {
    .init = SerialPipelineInit,    // 初始化函数指针
    .dataSend = SerialDataSend,    // 数据发送函数指针
    .dataRecv = SerialDataReceive, // 数据接收函数指针
    .wait = SerialWait,             // 等待函数指针
};

/**
 * @brief 跟踪管道初始化入口函数
 * @return 管道初始化结果
 * @note 注册串口管道操作集并执行初始化
 */
UINT32 OsTracePipelineInit(VOID)
{
    OsTracePipelineReg(&g_serialOps);  // 注册串口管道操作集
    return g_serialOps.init();         // 调用初始化函数并返回结果
}

