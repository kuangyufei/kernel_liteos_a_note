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

#ifndef _TRACE_PIPELINE_H
#define _TRACE_PIPELINE_H

#include "los_typedef.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */
/**
 * @brief 跟踪管道操作函数集
 * @details 定义了跟踪功能的初始化、数据发送、数据接收和等待操作的函数指针
 */
typedef struct {
    UINT32 (*init)(VOID);                          // 初始化跟踪管道，返回0表示成功，非0表示失败
    VOID (*dataSend)(UINT16 len, UINT8 *data);     // 发送跟踪数据，len为数据长度，data为数据缓冲区指针
    UINT32 (*dataRecv)(UINT8 *data, UINT32 size, UINT32 timeout);  // 接收跟踪数据，data为接收缓冲区，size为缓冲区大小，timeout为超时时间(ms)，返回实际接收长度
    UINT32 (*wait)(VOID);                          // 等待跟踪操作完成，返回0表示成功
} TracePipelineOps;

/* used as tlv's tag */
/**
 * @brief TLV消息类型枚举
 * @details 定义了跟踪系统中使用的TLV消息类型标签
 */
enum TraceMsgType {
    NOTIFY,             // 通知类型消息
    HEAD,               // 头部信息类型消息
    OBJ,                // 对象信息类型消息
    EVENT,              // 事件类型消息
    TRACE_MSG_MAX,      // 跟踪消息类型总数
};

/**
 * @brief 通知子类型枚举
 * @details 定义NOTIFY类型消息的具体子类型
 */
enum TraceNotifySubType {
    CMD = 0x1,          // 命令类型通知 (十进制1)
    PARAMS,             // 参数类型通知
};

/**
 * @brief 头部子类型枚举
 * @details 定义HEAD类型消息的具体子类型
 */
enum TraceHeadSubType {
    ENDIAN = 0x1,       // 大小端标识 (十进制1)
    VERSION,            // 版本信息
    OBJ_SIZE,           // 对象大小
    OBJ_COUNT,          // 对象数量
    CUR_INDEX,          // 当前索引
    MAX_RECODE,         // 最大记录数
    CUR_OBJ_INDEX,      // 当前对象索引
    CLOCK_FREQ,         // 时钟频率
};

/**
 * @brief 对象子类型枚举
 * @details 定义OBJ类型消息的具体子类型
 */
enum TraceObjSubType {
    ADDR = 0x1,         // 地址信息 (十进制1)
    PRIO,               // 优先级信息
    NAME,               // 名称信息
};

/**
 * @brief 事件子类型枚举
 * @details 定义EVENT类型消息的具体子类型
 */
enum TraceEvtSubType {
    CORE = 0x1,         // 核心编号 (十进制1)
    EVENT_CODE,         // 事件代码
    CUR_TIME,           // 当前时间
    EVENT_COUNT,        // 事件计数
    CUR_TASK,           // 当前任务
    IDENTITY,           // 身份标识
    EVENT_PARAMS,       // 事件参数
    CUR_PID,            // 当前进程ID
    EVENT_LR,           // 事件链接寄存器值
};

extern VOID OsTracePipelineReg(const TracePipelineOps *ops);
extern UINT32 OsTracePipelineInit(VOID);

extern VOID OsTraceDataSend(UINT8 type, UINT16 len, UINT8 *data);
extern UINT32 OsTraceDataRecv(UINT8 *data, UINT32 size, UINT32 timeout);
extern UINT32 OsTraceDataWait(VOID);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _TRACE_PIPELINE_H */
