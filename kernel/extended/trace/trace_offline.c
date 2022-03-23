/*!
 * @file    trace_offline.c
 * @brief
 * @link
   @verbatim
   基本概念
   	Trace调测旨在帮助开发者获取内核的运行流程，各个模块、任务的执行顺序，从而可以辅助开发者定位一些时序问题或者了解内核的代码运行过程。

   运行机制
   	内核提供一套Hook框架，将Hook点预埋在各个模块的主要流程中, 在内核启动初期完成Trace功能的初始化，并注册Trace的处理函数到Hook中。
   当系统触发到一个Hook点时，Trace模块会对输入信息进行封装，添加Trace帧头信息，包含事件类型、运行的cpuid、运行的任务id、运行的相对时间戳等信息；

   Trace提供2种工作模式，离线模式和在线模式。此处为离线模式下的实现
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

#define BITS_NUM_FOR_TASK_ID 16

LITE_OS_SEC_BSS STATIC TraceOfflineHeaderInfo g_traceRecoder; ///< 离线模式下的记录方式
LITE_OS_SEC_BSS STATIC UINT32 g_tidMask[LOSCFG_BASE_CORE_TSK_LIMIT] = {0};///< 记录屏蔽任务情况

UINT32 OsTraceGetMaskTid(UINT32 tid)
{
    return tid | ((tid < LOSCFG_BASE_CORE_TSK_LIMIT) ? g_tidMask[tid] << BITS_NUM_FOR_TASK_ID : 0); /* tid < 65535 */
}
/// trace离线模式初始化
UINT32 OsTraceBufInit(UINT32 size)
{
    UINT32 headSize;
    VOID *buf = NULL;
    headSize = sizeof(OfflineHead) + sizeof(ObjData) * LOSCFG_TRACE_OBJ_MAX_NUM;
    if (size <= headSize) {
        TRACE_ERROR("trace buf size not enough than 0x%x\n", headSize);
        return LOS_ERRNO_TRACE_BUF_TOO_SMALL;
    }


    buf = LOS_MemAlloc(m_aucSysMem0, size);//在内核堆空间中申请内存
    if (buf == NULL) {
        return LOS_ERRNO_TRACE_NO_MEMORY;
    }

    (VOID)memset_s(buf, size, 0, size);
    g_traceRecoder.head = (OfflineHead *)buf;
    g_traceRecoder.head->baseInfo.bigLittleEndian = TRACE_BIGLITTLE_WORD;//大小端
    g_traceRecoder.head->baseInfo.version         = TRACE_VERSION(TRACE_MODE_OFFLINE);//离线模式
    g_traceRecoder.head->baseInfo.clockFreq       = OS_SYS_CLOCK;//CPU时钟频率
    g_traceRecoder.head->objSize                  = sizeof(ObjData);//对象大小
    g_traceRecoder.head->frameSize                = sizeof(TraceEventFrame);//帧大小
    g_traceRecoder.head->objOffset                = sizeof(OfflineHead);//对象开始位置
    g_traceRecoder.head->frameOffset              = headSize;//帧开始位置
    g_traceRecoder.head->totalLen                 = size;//头部大小

    g_traceRecoder.ctrl.curIndex       = 0;	//当前帧位置
    g_traceRecoder.ctrl.curObjIndex    = 0;	//当前对象位置
    g_traceRecoder.ctrl.maxObjCount    = LOSCFG_TRACE_OBJ_MAX_NUM;//最大
    g_traceRecoder.ctrl.maxRecordCount = (size - headSize) / sizeof(TraceEventFrame);//最大记录数
    g_traceRecoder.ctrl.objBuf         = (ObjData *)((UINTPTR)buf + g_traceRecoder.head->objOffset);
    g_traceRecoder.ctrl.frameBuf       = (TraceEventFrame *)((UINTPTR)buf + g_traceRecoder.head->frameOffset);

    return LOS_OK;
}
/// 添加一个任务
VOID OsTraceObjAdd(UINT32 eventType, UINT32 taskId)
{
    UINT32 intSave;
    UINT32 index;
    ObjData *obj = NULL;

    TRACE_LOCK(intSave);
    /* add obj begin */
    index = g_traceRecoder.ctrl.curObjIndex;
    if (index >= LOSCFG_TRACE_OBJ_MAX_NUM) { /* do nothing when config LOSCFG_TRACE_OBJ_MAX_NUM = 0 */
        TRACE_UNLOCK(intSave);
        return;
    }
    obj = &g_traceRecoder.ctrl.objBuf[index];

    if (taskId < LOSCFG_BASE_CORE_TSK_LIMIT) {
        g_tidMask[taskId]++;
    }

    OsTraceSetObj(obj, OS_TCB_FROM_TID(taskId));

    g_traceRecoder.ctrl.curObjIndex++;
    if (g_traceRecoder.ctrl.curObjIndex >= g_traceRecoder.ctrl.maxObjCount) {
        g_traceRecoder.ctrl.curObjIndex = 0; /* turn around */
    }
    /* add obj end */
    TRACE_UNLOCK(intSave);
}
/// 离线模式下保存帧数据 @note_thinking 此处未封装好,会懵逼,文件名中体现了对离线模式的保存或对在线模式的发送这样真的好吗? .
VOID OsTraceWriteOrSendEvent(const TraceEventFrame *frame)
{
    UINT16 index;
    UINT32 intSave;

    TRACE_LOCK(intSave);
    index = g_traceRecoder.ctrl.curIndex;//获取当前位置,离线模式会将trace frame记录到预先申请好的循环buffer中
    (VOID)memcpy_s(&g_traceRecoder.ctrl.frameBuf[index], sizeof(TraceEventFrame), frame, sizeof(TraceEventFrame));//保存帧内容至frameBuf中

    g_traceRecoder.ctrl.curIndex++;//当前位置向前滚
    if (g_traceRecoder.ctrl.curIndex >= g_traceRecoder.ctrl.maxRecordCount) {//循环写入,这里可以看出,如果循环buffer记录的frame过多则可能出现翻转，
        g_traceRecoder.ctrl.curIndex = 0;//会覆盖之前的记录，保持记录的信息始终是最新的信息。
    }
    TRACE_UNLOCK(intSave);
}
/// 重置循环buf
VOID OsTraceReset(VOID)
{
    UINT32 intSave;
    UINT32 bufLen;

    TRACE_LOCK(intSave);
    bufLen = sizeof(TraceEventFrame) * g_traceRecoder.ctrl.maxRecordCount;
    (VOID)memset_s(g_traceRecoder.ctrl.frameBuf, bufLen, 0, bufLen);
    g_traceRecoder.ctrl.curIndex = 0;
    TRACE_UNLOCK(intSave);
}

STATIC VOID OsTraceInfoObj(VOID)
{
    UINT32 i;
    ObjData *obj = &g_traceRecoder.ctrl.objBuf[0];

    if (g_traceRecoder.ctrl.maxObjCount > 0) {
        PRINTK("CurObjIndex = %u\n", g_traceRecoder.ctrl.curObjIndex);
        PRINTK("Index   TaskID   TaskPrio   TaskName \n");
        for (i = 0; i < g_traceRecoder.ctrl.maxObjCount; i++, obj++) {
            PRINTK("%-7u 0x%-6x %-10u %s\n", i, obj->id, obj->prio, obj->name);
        }
        PRINTK("\n");
    }
}

STATIC VOID OsTraceInfoEventTitle(VOID)
{
    PRINTK("CurEvtIndex = %u\n", g_traceRecoder.ctrl.curIndex);

    PRINTK("Index   Time(cycles)      EventType      CurPid   CurTask   Identity      ");
#ifdef LOSCFG_TRACE_FRAME_CORE_MSG
    PRINTK("cpuid    hwiActive    taskLockCnt    ");
#endif
#ifdef LOSCFG_TRACE_FRAME_EVENT_COUNT
    PRINTK("eventCount    ");
#endif
#ifdef LOS_TRACE_FRAME_LR
    UINT32 i;
    PRINTK("backtrace ");
    for (i = 0; i < LOS_TRACE_LR_RECORD; i++) {
        PRINTK("           ");
    }
#endif
    if (LOSCFG_TRACE_FRAME_MAX_PARAMS > 0) {
        PRINTK("params    ");
    }
    PRINTK("\n");
}

STATIC VOID OsTraceInfoEventData(VOID)
{
    UINT32 i, j;
    TraceEventFrame *frame = &g_traceRecoder.ctrl.frameBuf[0];

    for (i = 0; i < g_traceRecoder.ctrl.maxRecordCount; i++, frame++) {
        PRINTK("%-7u 0x%-15llx 0x%-12x 0x%-7x 0x%-7x 0x%-11x ", i, frame->curTime, frame->eventType,
            frame->curPid, frame->curTask, frame->identity);
#ifdef LOSCFG_TRACE_FRAME_CORE_MSG
        UINT32 taskLockCnt = frame->core.taskLockCnt;
#ifdef LOSCFG_KERNEL_SMP
        /*
         * For smp systems, TRACE_LOCK will requst taskLock, and this counter
         * will increase by 1 in that case.
         */
        taskLockCnt -= 1;
#endif
        PRINTK("%-11u %-11u %-11u", frame->core.cpuid, frame->core.hwiActive, taskLockCnt);
#endif
#ifdef LOSCFG_TRACE_FRAME_EVENT_COUNT
        PRINTK("%-11u", frame->eventCount);
#endif
#ifdef LOS_TRACE_FRAME_LR
        for (j = 0; j < LOS_TRACE_LR_RECORD; j++) {
            PRINTK("0x%-11x", frame->linkReg[j]);
        }
#endif
        for (j = 0; j < LOSCFG_TRACE_FRAME_MAX_PARAMS; j++) {
            PRINTK("0x%-11x", frame->params[j]);
        }
        PRINTK("\n");
    }
}

STATIC VOID OsTraceInfoDisplay(VOID)
{
    OfflineHead *head = g_traceRecoder.head;

    PRINTK("*******TraceInfo begin*******\n");
    PRINTK("clockFreq = %u\n", head->baseInfo.clockFreq);

    OsTraceInfoObj();

    OsTraceInfoEventTitle();
    OsTraceInfoEventData();

    PRINTK("*******TraceInfo end*******\n");
}

#ifdef LOSCFG_TRACE_CLIENT_INTERACT
STATIC VOID OsTraceSendInfo(VOID)
{
    UINT32 i;
    ObjData *obj = NULL;
    TraceEventFrame *frame = NULL;

    OsTraceDataSend(HEAD, sizeof(OfflineHead), (UINT8 *)g_traceRecoder.head);

    obj = &g_traceRecoder.ctrl.objBuf[0];
    for (i = 0; i < g_traceRecoder.ctrl.maxObjCount; i++) {
        OsTraceDataSend(OBJ, sizeof(ObjData), (UINT8 *)(obj + i));
    }

    frame = &g_traceRecoder.ctrl.frameBuf[0];
    for (i = 0; i < g_traceRecoder.ctrl.maxRecordCount; i++) {
        OsTraceDataSend(EVENT, sizeof(TraceEventFrame), (UINT8 *)(frame + i));
    }
}
#endif

VOID OsTraceRecordDump(BOOL toClient)
{
    if (!toClient) {
        OsTraceInfoDisplay();
        return;
    }

#ifdef LOSCFG_TRACE_CLIENT_INTERACT
    OsTraceSendInfo();
#endif
}

OfflineHead *OsTraceRecordGet(VOID)
{
    return g_traceRecoder.head;
}
