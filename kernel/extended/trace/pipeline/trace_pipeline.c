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

#include "trace_pipeline.h"
#include "trace_tlv.h"
#include "los_trace_pri.h"
/**
 * @brief SMP系统下的管道锁定义
 * @details 根据是否支持SMP系统，定义不同的管道锁操作宏
 */
#ifdef LOSCFG_KERNEL_SMP
LITE_OS_SEC_BSS SPIN_LOCK_INIT(g_pipeSpin);  // SMP系统使用自旋锁
#define PIPE_LOCK(state)                   LOS_SpinLockSave(&g_pipeSpin, &(state))  // 获取管道自旋锁并保存中断状态
#define PIPE_UNLOCK(state)                 LOS_SpinUnlockRestore(&g_pipeSpin, (state))  // 恢复管道自旋锁并还原中断状态
#else
#define PIPE_LOCK(state) 		   (state) = LOS_IntLock()  // 非SMP系统直接关闭中断
#define PIPE_UNLOCK(state)		   LOS_IntRestore(state)  // 非SMP系统恢复中断
#endif

/**
 * @brief 通知类型TLV表
 * @details 定义NOTIFY类型消息的TLV编码规则
 */
STATIC TlvTable g_traceTlvTblNotify[] = {
    { CMD,    LOS_OFF_SET_OF(TraceNotifyFrame, cmd),   sizeof(UINT32) },  // 命令字段，偏移量为TraceNotifyFrame结构体中cmd成员的位置，大小4字节
    { PARAMS, LOS_OFF_SET_OF(TraceNotifyFrame, param), sizeof(UINT32) },  // 参数字段，偏移量为TraceNotifyFrame结构体中param成员的位置，大小4字节
    { TRACE_TLV_TYPE_NULL, 0, 0 },  // TLV表结束标志
};

/**
 * @brief 头部信息TLV表
 * @details 定义HEAD类型消息的TLV编码规则
 */
STATIC TlvTable g_traceTlvTblHead[] = {
    { ENDIAN,     LOS_OFF_SET_OF(TraceBaseHeaderInfo, bigLittleEndian), sizeof(UINT32) },  // 大小端标识字段，偏移量为TraceBaseHeaderInfo结构体中bigLittleEndian成员的位置，大小4字节
    { VERSION,    LOS_OFF_SET_OF(TraceBaseHeaderInfo, version),         sizeof(UINT32) },  // 版本信息字段，偏移量为TraceBaseHeaderInfo结构体中version成员的位置，大小4字节
    { CLOCK_FREQ, LOS_OFF_SET_OF(TraceBaseHeaderInfo, clockFreq),       sizeof(UINT32) },  // 时钟频率字段，偏移量为TraceBaseHeaderInfo结构体中clockFreq成员的位置，大小4字节
    { TRACE_TLV_TYPE_NULL, 0, 0 },  // TLV表结束标志
};

/**
 * @brief 对象信息TLV表
 * @details 定义OBJ类型消息的TLV编码规则
 */
STATIC TlvTable g_traceTlvTblObj[] = {
    { ADDR, LOS_OFF_SET_OF(ObjData, id),   sizeof(UINT32) },  // 地址字段，偏移量为ObjData结构体中id成员的位置，大小4字节
    { PRIO, LOS_OFF_SET_OF(ObjData, prio), sizeof(UINT32) },  // 优先级字段，偏移量为ObjData结构体中prio成员的位置，大小4字节
    { NAME, LOS_OFF_SET_OF(ObjData, name), sizeof(CHAR) * LOSCFG_TRACE_OBJ_MAX_NAME_SIZE },  // 名称字段，偏移量为ObjData结构体中name成员的位置，大小为配置的最大名称长度
    { TRACE_TLV_TYPE_NULL, 0, 0 },  // TLV表结束标志
};

/**
 * @brief 事件信息TLV表
 * @details 定义EVENT类型消息的TLV编码规则
 */
STATIC TlvTable g_traceTlvTblEvent[] = {
#ifdef LOSCFG_TRACE_FRAME_CORE_MSG
    { CORE,         LOS_OFF_SET_OF(TraceEventFrame, core),       sizeof(UINT32) },  // 核心编号字段，偏移量为TraceEventFrame结构体中core成员的位置，大小4字节
#endif
    { EVENT_CODE,   LOS_OFF_SET_OF(TraceEventFrame, eventType),  sizeof(UINT32) },  // 事件代码字段，偏移量为TraceEventFrame结构体中eventType成员的位置，大小4字节
    { CUR_TIME,     LOS_OFF_SET_OF(TraceEventFrame, curTime),    sizeof(UINT64) },  // 当前时间字段，偏移量为TraceEventFrame结构体中curTime成员的位置，大小8字节

#ifdef LOSCFG_TRACE_FRAME_EVENT_COUNT
    { EVENT_COUNT,  LOS_OFF_SET_OF(TraceEventFrame, eventCount), sizeof(UINT32) },  // 事件计数字段，偏移量为TraceEventFrame结构体中eventCount成员的位置，大小4字节
#endif
    { CUR_TASK,     LOS_OFF_SET_OF(TraceEventFrame, curTask),    sizeof(UINT32) },  // 当前任务字段，偏移量为TraceEventFrame结构体中curTask成员的位置，大小4字节
    { IDENTITY,     LOS_OFF_SET_OF(TraceEventFrame, identity),   sizeof(UINTPTR) },  // 身份标识字段，偏移量为TraceEventFrame结构体中identity成员的位置，大小为指针宽度
    { EVENT_PARAMS, LOS_OFF_SET_OF(TraceEventFrame, params),     sizeof(UINTPTR) * LOSCFG_TRACE_FRAME_MAX_PARAMS },  // 事件参数数组，偏移量为TraceEventFrame结构体中params成员的位置，大小为参数数量×指针宽度
    { CUR_PID,      LOS_OFF_SET_OF(TraceEventFrame, curPid),     sizeof(UINT32) },  // 当前进程ID字段，偏移量为TraceEventFrame结构体中curPid成员的位置，大小4字节
#ifdef LOS_TRACE_FRAME_LR
    { EVENT_LR,     LOS_OFF_SET_OF(TraceEventFrame, linkReg),    sizeof(UINTPTR) * LOS_TRACE_LR_RECORD },  // 链接寄存器值数组，偏移量为TraceEventFrame结构体中linkReg成员的位置，大小为记录数量×指针宽度
#endif
    { TRACE_TLV_TYPE_NULL, 0, 0 },  // TLV表结束标志
};

/**
 * @brief TLV表数组
 * @details 按消息类型索引的TLV表数组，顺序与TraceMsgType枚举对应
 */
STATIC TlvTable *g_traceTlvTbl[] = {
    g_traceTlvTblNotify,  // NOTIFY类型消息的TLV表
    g_traceTlvTblHead,    // HEAD类型消息的TLV表
    g_traceTlvTblObj,     // OBJ类型消息的TLV表
    g_traceTlvTblEvent    // EVENT类型消息的TLV表
};

/**
 * @brief 默认管道初始化函数
 * @return 始终返回LOS_OK
 */
STATIC UINT32 DefaultPipelineInit(VOID)
{
    return LOS_OK;  // 默认初始化成功
}

/**
 * @brief 默认数据发送函数
 * @param[in] len 数据长度
 * @param[in] data 数据缓冲区指针
 * @note 默认实现为空操作
 */
STATIC VOID DefaultDataSend(UINT16 len, UINT8 *data)
{
    (VOID)len;   // 未使用的参数
    (VOID)data;  // 未使用的参数
}

/**
 * @brief 默认数据接收函数
 * @param[in] data 接收缓冲区指针
 * @param[in] size 缓冲区大小
 * @param[in] timeout 超时时间(ms)
 * @return 始终返回LOS_OK
 * @note 默认实现为空操作
 */
STATIC UINT32 DefaultDataReceive(UINT8 *data, UINT32 size, UINT32 timeout)
{
    (VOID)data;    // 未使用的参数
    (VOID)size;    // 未使用的参数
    (VOID)timeout; // 未使用的参数
    return LOS_OK; // 默认接收成功
}

/**
 * @brief 默认等待函数
 * @return 始终返回LOS_OK
 * @note 默认实现为空操作
 */
STATIC UINT32 DefaultWait(VOID)
{
    return LOS_OK;  // 默认等待成功
}

/**
 * @brief 默认跟踪管道操作集
 * @details 定义默认的跟踪管道操作函数指针
 */
STATIC TracePipelineOps g_defaultOps = {
    .init = DefaultPipelineInit,    // 初始化函数指针
    .dataSend = DefaultDataSend,    // 数据发送函数指针
    .dataRecv = DefaultDataReceive, // 数据接收函数指针
    .wait = DefaultWait,             // 等待函数指针
};

STATIC const TracePipelineOps *g_tracePipelineOps = &g_defaultOps;  // 跟踪管道操作集指针，初始化为默认操作集

/**
 * @brief 注册跟踪管道操作集
 * @param[in] ops 指向TracePipelineOps结构体的指针
 * @note 此函数用于替换默认的跟踪管道操作集
 */
VOID OsTracePipelineReg(const TracePipelineOps *ops)
{
    g_tracePipelineOps = ops;  // 更新跟踪管道操作集指针
}

/**
 * @brief 发送跟踪数据
 * @param[in] type 消息类型，取值范围见TraceMsgType枚举
 * @param[in] len 原始数据长度
 * @param[in] data 原始数据缓冲区指针
 * @note 函数会先进行TLV编码，再通过注册的管道发送数据
 */
VOID OsTraceDataSend(UINT8 type, UINT16 len, UINT8 *data)
{
    UINT32 intSave;                          // 用于保存中断状态
    UINT8 outBuf[LOSCFG_TRACE_TLV_BUF_SIZE] = {0};  // TLV编码输出缓冲区

    if ((type >= TRACE_MSG_MAX) || (len > LOSCFG_TRACE_TLV_BUF_SIZE)) {  // 参数合法性检查：消息类型越界或数据长度超过缓冲区大小
        return;  // 非法参数，直接返回
    }

    len = OsTraceDataEncode(type, g_traceTlvTbl[type], data, &outBuf[0], sizeof(outBuf));  // 执行TLV编码，返回编码后长度

    PIPE_LOCK(intSave);  // 获取管道锁，保护数据发送过程
    g_tracePipelineOps->dataSend(len, &outBuf[0]);  // 调用注册的发送函数发送编码后的数据
    PIPE_UNLOCK(intSave);  // 释放管道锁
}

/**
 * @brief 接收跟踪数据
 * @param[in] data 接收缓冲区指针
 * @param[in] size 缓冲区大小
 * @param[in] timeout 超时时间
 * @return 实际接收的数据长度
 * @note 调用注册的管道接收函数
 */
UINT32 OsTraceDataRecv(UINT8 *data, UINT32 size, UINT32 timeout)
{
    return g_tracePipelineOps->dataRecv(data, size, timeout);  // 调用注册的接收函数
}

/**
 * @brief 等待跟踪操作完成
 * @return 操作结果，LOS_OK表示成功
 * @note 调用注册的等待函数
 */
UINT32 OsTraceDataWait(VOID)
{
    return g_tracePipelineOps->wait();  // 调用注册的等待函数
}
