/*!
 * @file    trace_online.c
 * @brief 在线模式需要配合IDE使用，实时将trace frame记录发送给IDE，IDE端进行解析并可视化展示
 * @link
   @verbatim
   Trace调测旨在帮助开发者获取内核的运行流程，各个模块、任务的执行顺序，从而可以辅助开发者定位一些时序问题或者了解内核的代码运行过程。
   
   内核提供一套Hook框架，将Hook点预埋在各个模块的主要流程中, 在内核启动初期完成Trace功能的初始化，并注册Trace的处理函数到Hook中。

   当系统触发到一个Hook点时，Trace模块会对输入信息进行封装，添加Trace帧头信息，包含事件类型、运行的cpuid、运行的任务id、运行的相对时间戳等信息；

   Trace提供2种工作模式，离线模式和在线模式。
   本文件为 在线模式
   @endverbatim
 * @version 
 * @author  weharmonyos.com | 鸿蒙研究站 | 每天死磕一点点
 * @date    2021-11-22
 */
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

#include "los_trace_pri.h"
#include "trace_pipeline.h"

UINT32 OsTraceGetMaskTid(UINT32 taskId)
{
    return taskId;
}
/// 发送头信息
VOID OsTraceSendHead(VOID)
{
    TraceBaseHeaderInfo head = {
        .bigLittleEndian = TRACE_BIGLITTLE_WORD,
        .version         = TRACE_VERSION(TRACE_MODE_ONLINE),
        .clockFreq       = OS_SYS_CLOCK,
    };

    OsTraceDataSend(HEAD, sizeof(TraceBaseHeaderInfo), (UINT8 *)&head);
}
/// 发送通知类信息(启动,停止 trace 等通知)
VOID OsTraceSendNotify(UINT32 type, UINT32 value)
{
    TraceNotifyFrame frame = {
        .cmd   = type,
        .param = value,
    };

    OsTraceDataSend(NOTIFY, sizeof(TraceNotifyFrame), (UINT8 *)&frame);
}
/// 向串口发送一个类型为对象的trace
STATIC VOID OsTraceSendObj(const LosTaskCB *tcb)
{
    ObjData obj;

    OsTraceSetObj(&obj, tcb);
    OsTraceDataSend(OBJ, sizeof(ObjData), (UINT8 *)&obj);//发送任务信息到串口
}
///发送所有任务对象至串口
VOID OsTraceSendObjTable(VOID)
{
    UINT32 loop;
    LosTaskCB *tcb = NULL;

    for (loop = 0; loop < g_taskMaxNum; ++loop) {
        tcb = g_taskCBArray + loop;
        if (tcb->taskStatus & OS_TASK_STATUS_UNUSED) {//过滤掉已使用任务
            continue;
        }
        OsTraceSendObj(tcb);//向串口发送数据
    }
}
/// 添加一个对象(任务) trace
VOID OsTraceObjAdd(UINT32 eventType, UINT32 taskId)
{
    if (OsTraceIsEnable()) {
        OsTraceSendObj(OS_TCB_FROM_TID(taskId));
    }
}
/// 在线模式下发送数据给 IDE, Trace模块会对输入信息进行封装，添加Trace帧头信息，包含事件类型、运行的cpuid、运行的任务id、运行的相对时间戳等信息
VOID OsTraceWriteOrSendEvent(const TraceEventFrame *frame)
{
    OsTraceDataSend(EVENT, sizeof(TraceEventFrame), (UINT8 *)frame);// 编码TLV并向串口发送
}

OfflineHead *OsTraceRecordGet(VOID)
{
    return NULL;
}
