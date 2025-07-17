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
 * @date    2025-07-17
 */

#include "los_trace_pri.h"
#include "trace_pipeline.h"
/**
 * @brief 获取跟踪任务ID掩码
 * 
 * @param taskId 任务ID
 * @return UINT32 返回任务ID（当前实现直接返回输入ID，未进行掩码处理）
 * @note 该函数为预留接口，未来可扩展支持任务组掩码或过滤功能
 */
UINT32 OsTraceGetMaskTid(UINT32 taskId)
{
    return taskId;
}

/**
 * @brief 发送跟踪系统基准头信息
 * 
 * 初始化并发送跟踪系统的基础头部信息，包括字节序标识、版本号和系统时钟频率，
 * 用于跟踪数据的解析和时间戳计算
 */
VOID OsTraceSendHead(VOID)
{
    TraceBaseHeaderInfo head = {
        .bigLittleEndian = TRACE_BIGLITTLE_WORD,  /* 字节序标识（0x1234表示大端，0x3412表示小端） */
        .version         = TRACE_VERSION(TRACE_MODE_ONLINE),  /* 跟踪版本号，包含在线跟踪模式标识 */
        .clockFreq       = OS_SYS_CLOCK,  /* 系统时钟频率（单位：Hz），用于时间戳换算 */
    };

    /* 发送头部信息，数据类型为HEAD，长度为TraceBaseHeaderInfo结构体大小 */
    OsTraceDataSend(HEAD, sizeof(TraceBaseHeaderInfo), (UINT8 *)&head);
}

/**
 * @brief 发送跟踪通知帧
 * 
 * @param type 通知类型（如系统启动、配置变更等预定义事件类型）
 * @param value 通知参数，具体含义随通知类型变化
 * 
 * 用于发送系统级别的控制通知或状态变更信息，不包含具体跟踪事件数据
 */
VOID OsTraceSendNotify(UINT32 type, UINT32 value)
{
    TraceNotifyFrame frame = {
        .cmd   = type,   /* 通知命令类型 */
        .param = value,  /* 通知参数值 */
    };

    /* 发送通知帧，数据类型为NOTIFY，长度为TraceNotifyFrame结构体大小 */
    OsTraceDataSend(NOTIFY, sizeof(TraceNotifyFrame), (UINT8 *)&frame);
}

/**
 * @brief 发送任务对象信息（静态内部函数）
 * 
 * @param tcb 任务控制块指针，指向需要发送信息的任务
 * 
 * 将任务控制块信息转换为跟踪系统通用的对象格式并发送，用于任务创建/销毁事件跟踪
 */
STATIC VOID OsTraceSendObj(const LosTaskCB *tcb)
{
    ObjData obj;  /* 对象数据结构体，用于标准化存储各类内核对象信息 */

    /* 将任务控制块信息填充到对象数据结构体 */
    OsTraceSetObj(&obj, tcb);
    /* 发送对象数据，数据类型为OBJ，长度为ObjData结构体大小 */
    OsTraceDataSend(OBJ, sizeof(ObjData), (UINT8 *)&obj);
}

/**
 * @brief 发送所有活动任务对象表
 * 
 * 遍历系统中所有任务控制块，筛选出非未使用状态的任务，批量发送其对象信息，
 * 通常在跟踪系统启动时调用，用于初始化全局任务视图
 */
VOID OsTraceSendObjTable(VOID)
{
    UINT32 loop;
    LosTaskCB *tcb = NULL;

    /* 遍历任务控制块数组（最大任务数由g_taskMaxNum定义） */
    for (loop = 0; loop < g_taskMaxNum; ++loop) {
        tcb = g_taskCBArray + loop;  /* 获取当前索引的任务控制块指针 */
        /* 跳过未使用的任务控制块（OS_TASK_STATUS_UNUSED表示任务槽位空闲） */
        if (tcb->taskStatus & OS_TASK_STATUS_UNUSED) {
            continue;
        }
        /* 发送活动任务的对象信息 */
        OsTraceSendObj(tcb);
    }
}

/**
 * @brief 添加任务对象到跟踪系统
 * 
 * @param eventType 事件类型（未使用，预留参数）
 * @param taskId 任务ID
 * 
 * 当跟踪系统启用时，发送指定任务的对象信息，通常在任务创建时调用
 */
VOID OsTraceObjAdd(UINT32 eventType, UINT32 taskId)
{
    /* 仅在跟踪系统启用时发送对象信息 */
    if (OsTraceIsEnable()) {
        /* 通过任务ID获取任务控制块并发送对象信息 */
        OsTraceSendObj(OS_TCB_FROM_TID(taskId));
    }
}

/**
 * @brief 写入或发送跟踪事件帧
 * 
 * @param frame 跟踪事件帧指针，包含事件类型、时间戳和具体参数
 * 
 * 在线跟踪模式下直接发送事件数据，离线模式下可能写入本地缓冲区（当前仅实现在线发送）
 */
VOID OsTraceWriteOrSendEvent(const TraceEventFrame *frame)
{
    /* 发送事件数据，数据类型为EVENT，长度为TraceEventFrame结构体大小 */
    OsTraceDataSend(EVENT, sizeof(TraceEventFrame), (UINT8 *)frame);
}

/**
 * @brief 获取离线跟踪记录头部（在线模式未实现）
 * 
 * @return OfflineHead* 总是返回NULL，表示在线模式不支持离线记录功能
 * @note 该函数为离线跟踪预留接口，在线模式下无实际功能
 */
OfflineHead *OsTraceRecordGet(VOID)
{
    return NULL;
}