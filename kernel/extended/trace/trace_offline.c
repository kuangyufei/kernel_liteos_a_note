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
 * @date    2025-07-17
 */

#include "los_trace_pri.h"
#include "trace_pipeline.h"
/**
 * @def BITS_NUM_FOR_TASK_ID
 * @brief 任务ID掩码位数
 *
 * 定义任务ID在掩码中的占位位数，用于将任务ID与其他标识信息进行位组合
 * 当前配置为16位，支持最大任务ID为65535（2^16-1）
 */
#define BITS_NUM_FOR_TASK_ID 16

/**
 * @var g_traceRecoder
 * @brief 离线跟踪记录器全局实例
 *
 * 存储离线跟踪的头部信息、控制参数及缓冲区指针，使用LITE_OS_SEC_BSS属性
 * 确保在系统启动时被初始化为零
 */
LITE_OS_SEC_BSS STATIC TraceOfflineHeaderInfo g_traceRecoder;

/**
 * @var g_tidMask
 * @brief 任务ID掩码数组
 *
 * 记录每个任务ID的掩码值，数组大小由最大任务数LOSCFG_BASE_CORE_TSK_LIMIT决定
 * 用于OsTraceGetMaskTid函数生成带掩码的任务ID
 */
LITE_OS_SEC_BSS STATIC UINT32 g_tidMask[LOSCFG_BASE_CORE_TSK_LIMIT] = {0};

/**
 * @brief 获取带掩码的任务ID
 * 
 * @param tid 原始任务ID
 * @return UINT32 带掩码的任务ID（高16位为掩码值，低16位为原始任务ID）
 * @note 当任务ID超过LOSCFG_BASE_CORE_TSK_LIMIT时，掩码部分为0
 */
UINT32 OsTraceGetMaskTid(UINT32 tid)
{
    /* 任务ID小于最大限制时，将掩码左移16位后与原始ID组合 */
    return tid | ((tid < LOSCFG_BASE_CORE_TSK_LIMIT) ? g_tidMask[tid] << BITS_NUM_FOR_TASK_ID : 0); /* tid < 65535 */
}

/**
 * @brief 初始化离线跟踪缓冲区
 * 
 * @param size 缓冲区总大小（字节）
 * @return UINT32 操作结果
 *         - LOS_OK: 初始化成功
 *         - LOS_ERRNO_TRACE_BUF_TOO_SMALL: 缓冲区大小不足
 *         - LOS_ERRNO_TRACE_NO_MEMORY: 内存分配失败
 * 
 * 计算头部所需空间，分配并初始化跟踪缓冲区，设置头部信息和控制参数
 * 缓冲区布局：[OfflineHead][ObjData数组][TraceEventFrame数组]
 */
UINT32 OsTraceBufInit(UINT32 size)
{
    UINT32 headSize;        /* 头部总大小（OfflineHead + ObjData数组） */
    VOID *buf = NULL;       /* 分配的缓冲区指针 */
    /* 计算头部大小：固定头部 + 最大对象数 * 单个对象大小 */
    headSize = sizeof(OfflineHead) + sizeof(ObjData) * LOSCFG_TRACE_OBJ_MAX_NUM;
    /* 检查缓冲区是否足够容纳头部 */
    if (size <= headSize) {
        TRACE_ERROR("trace buf size not enough than 0x%x\n", headSize);
        return LOS_ERRNO_TRACE_BUF_TOO_SMALL;
    }

    /* 从系统内存池分配缓冲区 */
    buf = LOS_MemAlloc(m_aucSysMem0, size);
    if (buf == NULL) {
        return LOS_ERRNO_TRACE_NO_MEMORY;
    }

    /* 初始化整个缓冲区为0 */
    (VOID)memset_s(buf, size, 0, size);
    /* 设置头部指针并初始化基础信息 */
    g_traceRecoder.head = (OfflineHead *)buf;
    g_traceRecoder.head->baseInfo.bigLittleEndian = TRACE_BIGLITTLE_WORD; /* 字节序标识 */
    g_traceRecoder.head->baseInfo.version         = TRACE_VERSION(TRACE_MODE_OFFLINE); /* 离线模式版本号 */
    g_traceRecoder.head->baseInfo.clockFreq       = OS_SYS_CLOCK; /* 系统时钟频率（Hz） */
    g_traceRecoder.head->objSize                  = sizeof(ObjData); /* 单个对象数据大小 */
    g_traceRecoder.head->frameSize                = sizeof(TraceEventFrame); /* 单个事件帧大小 */
    g_traceRecoder.head->objOffset                = sizeof(OfflineHead); /* 对象数据区偏移量 */
    g_traceRecoder.head->frameOffset              = headSize; /* 事件帧区偏移量 */
    g_traceRecoder.head->totalLen                 = size; /* 缓冲区总长度 */

    /* 初始化控制参数 */
    g_traceRecoder.ctrl.curIndex       = 0; /* 当前事件帧索引 */
    g_traceRecoder.ctrl.curObjIndex    = 0; /* 当前对象索引 */
    g_traceRecoder.ctrl.maxObjCount    = LOSCFG_TRACE_OBJ_MAX_NUM; /* 最大对象数 */
    g_traceRecoder.ctrl.maxRecordCount = (size - headSize) / sizeof(TraceEventFrame); /* 最大事件帧数 */
    /* 计算对象缓冲区和事件帧缓冲区指针 */
    g_traceRecoder.ctrl.objBuf         = (ObjData *)((UINTPTR)buf + g_traceRecoder.head->objOffset);
    g_traceRecoder.ctrl.frameBuf       = (TraceEventFrame *)((UINTPTR)buf + g_traceRecoder.head->frameOffset);

    return LOS_OK;
}

/**
 * @brief 添加任务对象信息到跟踪缓冲区
 * 
 * @param eventType 事件类型（未使用，预留参数）
 * @param taskId 任务ID
 * 
 * 在临界区中添加任务控制块信息到对象缓冲区，使用循环缓冲区机制
 * 自动更新任务ID掩码计数并处理缓冲区溢出（循环覆盖）
 */
VOID OsTraceObjAdd(UINT32 eventType, UINT32 taskId)
{
    UINT32 intSave;         /* 中断状态保存变量 */
    UINT32 index;           /* 当前对象索引 */
    ObjData *obj = NULL;    /* 对象数据指针 */

    TRACE_LOCK(intSave);    /* 进入临界区，禁止中断 */
    /* add obj begin */
    index = g_traceRecoder.ctrl.curObjIndex;
    /* 当配置最大对象数为0时不执行操作 */
    if (index >= LOSCFG_TRACE_OBJ_MAX_NUM) {
        TRACE_UNLOCK(intSave); /* 退出临界区 */
        return;
    }
    obj = &g_traceRecoder.ctrl.objBuf[index];

    /* 更新任务ID掩码计数（仅当任务ID有效时） */
    if (taskId < LOSCFG_BASE_CORE_TSK_LIMIT) {
        g_tidMask[taskId]++;
    }

    /* 将任务控制块信息填充到对象数据结构 */
    OsTraceSetObj(obj, OS_TCB_FROM_TID(taskId));

    /* 更新当前对象索引，达到最大值时循环到起始位置 */
    g_traceRecoder.ctrl.curObjIndex++;
    if (g_traceRecoder.ctrl.curObjIndex >= g_traceRecoder.ctrl.maxObjCount) {
        g_traceRecoder.ctrl.curObjIndex = 0; /* turn around */
    }
    /* add obj end */
    TRACE_UNLOCK(intSave);  /* 退出临界区，恢复中断 */
}

/**
 * @brief 写入或发送跟踪事件帧（离线模式实现）
 * 
 * @param frame 跟踪事件帧指针
 * 
 * 在临界区中将事件帧数据写入循环缓冲区，确保多线程安全
 * 当缓冲区满时自动覆盖最旧数据
 */
VOID OsTraceWriteOrSendEvent(const TraceEventFrame *frame)
{
    UINT16 index;           /* 当前事件帧索引 */
    UINT32 intSave;         /* 中断状态保存变量 */

    TRACE_LOCK(intSave);    /* 进入临界区，禁止中断 */
    index = g_traceRecoder.ctrl.curIndex;
    /* 复制事件帧数据到缓冲区 */
    (VOID)memcpy_s(&g_traceRecoder.ctrl.frameBuf[index], sizeof(TraceEventFrame), frame, sizeof(TraceEventFrame));

    /* 更新当前事件帧索引，达到最大值时循环到起始位置 */
    g_traceRecoder.ctrl.curIndex++;
    if (g_traceRecoder.ctrl.curIndex >= g_traceRecoder.ctrl.maxRecordCount) {
        g_traceRecoder.ctrl.curIndex = 0;
    }
    TRACE_UNLOCK(intSave);  /* 退出临界区，恢复中断 */
}

/**
 * @brief 重置离线跟踪缓冲区
 * 
 * 清除事件帧缓冲区数据并重置当前索引，保留对象数据和头部信息
 * 用于跟踪数据的周期性重置或重新开始跟踪
 */
VOID OsTraceReset(VOID)
{
    UINT32 intSave;         /* 中断状态保存变量 */
    UINT32 bufLen;          /* 事件缓冲区长度 */

    TRACE_LOCK(intSave);    /* 进入临界区，禁止中断 */
    /* 计算事件缓冲区总长度 */
    bufLen = sizeof(TraceEventFrame) * g_traceRecoder.ctrl.maxRecordCount;
    /* 清除事件缓冲区数据 */
    (VOID)memset_s(g_traceRecoder.ctrl.frameBuf, bufLen, 0, bufLen);
    g_traceRecoder.ctrl.curIndex = 0; /* 重置当前事件索引 */
    TRACE_UNLOCK(intSave);  /* 退出临界区，恢复中断 */
}

/**
 * @brief 显示对象信息表（静态内部函数）
 * 
 * 打印当前跟踪缓冲区中的所有任务对象信息，包括索引、任务ID、优先级和名称
 * 当最大对象数为0时不执行任何操作
 */
STATIC VOID OsTraceInfoObj(VOID)
{
    UINT32 i;
    ObjData *obj = &g_traceRecoder.ctrl.objBuf[0];

    if (g_traceRecoder.ctrl.maxObjCount > 0) {
        PRINTK("CurObjIndex = %u\n", g_traceRecoder.ctrl.curObjIndex);
        PRINTK("Index   TaskID   TaskPrio   TaskName \n");
        /* 遍历所有对象缓冲区条目 */
        for (i = 0; i < g_traceRecoder.ctrl.maxObjCount; i++, obj++) {
            PRINTK("%-7u 0x%-6x %-10u %s\n", i, obj->id, obj->prio, obj->name);
        }
        PRINTK("\n");
    }
}

/**
 * @brief 打印事件数据标题行（静态内部函数）
 * 
 * 根据编译配置打印事件数据表格的标题行，包含基础列和条件编译列
 * 支持CPU ID、硬件中断状态、任务锁计数、事件计数和回溯信息等扩展字段
 */
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
    /* 为每个回溯记录项预留列 */
    for (i = 0; i < LOS_TRACE_LR_RECORD; i++) {
        PRINTK("           ");
    }
#endif
    /* 如果配置了最大参数数，则打印参数列 */
    if (LOSCFG_TRACE_FRAME_MAX_PARAMS > 0) {
        PRINTK("params    ");
    }
    PRINTK("\n");
}

/**
 * @brief 打印事件数据内容（静态内部函数）
 * 
 * 遍历事件帧缓冲区，打印所有事件记录的详细信息，根据编译配置输出不同字段
 * SMP系统下自动调整任务锁计数（减去跟踪锁本身占用的计数）
 */
STATIC VOID OsTraceInfoEventData(VOID)
{
    UINT32 i, j;
    TraceEventFrame *frame = &g_traceRecoder.ctrl.frameBuf[0];

    /* 遍历所有事件帧记录 */
    for (i = 0; i < g_traceRecoder.ctrl.maxRecordCount; i++, frame++) {
        /* 打印基础事件信息：索引、时间戳、事件类型、进程ID、任务ID、标识 */
        PRINTK("%-7u 0x%-15llx 0x%-12x 0x%-7x 0x%-7x 0x%-11x ", i, frame->curTime, frame->eventType,
            frame->curPid, frame->curTask, frame->identity);
#ifdef LOSCFG_TRACE_FRAME_CORE_MSG
        UINT32 taskLockCnt = frame->core.taskLockCnt;
#ifdef LOSCFG_KERNEL_SMP
        /*
         * 对于SMP系统，TRACE_LOCK会请求taskLock，导致计数器增加1
         * 此处修正锁计数，减去跟踪机制本身引入的锁计数
         */
        taskLockCnt -= 1;
#endif
        /* 打印CPU ID、硬件中断状态和修正后的任务锁计数 */
        PRINTK("%-11u %-11u %-11u", frame->core.cpuid, frame->core.hwiActive, taskLockCnt);
#endif
#ifdef LOSCFG_TRACE_FRAME_EVENT_COUNT
        /* 打印事件计数器 */
        PRINTK("%-11u", frame->eventCount);
#endif
#ifdef LOS_TRACE_FRAME_LR
        /* 打印回溯地址（链接寄存器值） */
        for (j = 0; j < LOS_TRACE_LR_RECORD; j++) {
            PRINTK("0x%-11x", frame->linkReg[j]);
        }
#endif
        /* 打印事件参数 */
        for (j = 0; j < LOSCFG_TRACE_FRAME_MAX_PARAMS; j++) {
            PRINTK("0x%-11x", frame->params[j]);
        }
        PRINTK("\n");
    }
}

/**
 * @brief 显示完整跟踪信息（静态内部函数）
 * 
 * 按固定格式打印跟踪系统的完整信息，包括时钟频率、对象表和事件数据
 * 是离线跟踪数据的主要展示接口
 */
STATIC VOID OsTraceInfoDisplay(VOID)
{
    OfflineHead *head = g_traceRecoder.head;

    PRINTK("*******TraceInfo begin*******\n");
    PRINTK("clockFreq = %u\n", head->baseInfo.clockFreq); /* 打印系统时钟频率 */

    OsTraceInfoObj();          /* 打印对象信息表 */
    OsTraceInfoEventTitle();   /* 打印事件标题行 */
    OsTraceInfoEventData();    /* 打印事件详细数据 */

    PRINTK("*******TraceInfo end*******\n");
}

#ifdef LOSCFG_TRACE_CLIENT_INTERACT
/**
 * @brief 发送跟踪信息到客户端（静态内部函数）
 * 
 * 通过OsTraceDataSend接口将完整跟踪数据发送到外部客户端，包括头部、对象表和事件帧
 * 仅在启用LOSCFG_TRACE_CLIENT_INTERACT配置时可用
 */
STATIC VOID OsTraceSendInfo(VOID)
{
    UINT32 i;
    ObjData *obj = NULL;
    TraceEventFrame *frame = NULL;

    /* 发送头部信息 */
    OsTraceDataSend(HEAD, sizeof(OfflineHead), (UINT8 *)g_traceRecoder.head);

    /* 发送所有对象数据 */
    obj = &g_traceRecoder.ctrl.objBuf[0];
    for (i = 0; i < g_traceRecoder.ctrl.maxObjCount; i++) {
        OsTraceDataSend(OBJ, sizeof(ObjData), (UINT8 *)(obj + i));
    }

    /* 发送所有事件帧数据 */
    frame = &g_traceRecoder.ctrl.frameBuf[0];
    for (i = 0; i < g_traceRecoder.ctrl.maxRecordCount; i++) {
        OsTraceDataSend(EVENT, sizeof(TraceEventFrame), (UINT8 *)(frame + i));
    }
}
#endif

/**
 * @brief 转储离线跟踪记录
 * 
 * @param toClient 是否发送到客户端
 *                 - TRUE: 通过OsTraceSendInfo发送到客户端（需启用LOSCFG_TRACE_CLIENT_INTERACT）
 *                 - FALSE: 在本地通过OsTraceInfoDisplay打印
 */
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

/**
 * @brief 获取离线跟踪头部指针
 * 
 * @return OfflineHead* 离线跟踪头部信息指针，包含缓冲区布局和基础配置
 * @note 用于外部模块访问跟踪系统的元数据
 */
OfflineHead *OsTraceRecordGet(VOID)
{
    return g_traceRecoder.head;
}