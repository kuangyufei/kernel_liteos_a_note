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

#ifndef _LOS_PERF_PRI_H
#define _LOS_PERF_PRI_H

#include "los_perf.h"
#include "perf.h"
#include "los_mp.h"
#include "los_task_pri.h"
#include "los_exc_pri.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

/**
 * @brief 事件类型与编码转换方向：事件到编码
 */
#define PERF_EVENT_TO_CODE       0
/**
 * @brief 事件类型与编码转换方向：编码到事件
 */
#define PERF_CODE_TO_EVENT       1
/**
 * @brief 性能数据魔术字，用于数据校验
 * @note 十六进制值0xEFEFEF00，用于标识性能采样数据的起始
 */
#define PERF_DATA_MAGIC_WORD     0xEFEFEF00

/**
 * @brief 在所有CPU上调用性能相关函数
 * @param func 要调用的函数指针
 * @note 使用OsMpFuncCall实现跨CPU函数调用，目标CPU为所有核心
 */
#define SMP_CALL_PERF_FUNC(func)  OsMpFuncCall(OS_MP_CPU_ALL, (SMP_FUNC_CALL)func, NULL)

/**
 * @brief PMU（性能监控单元）状态枚举
 */
enum PmuStatus {
    PERF_PMU_STOPED,               /* PMU已停止 */
    PERF_PMU_STARTED,              /* PMU已启动 */
};

/**
 * @brief 性能采样寄存器信息结构体
 * @details 存储程序计数器和帧指针等关键寄存器值
 */
typedef struct {
    UINTPTR pc;                    /* 程序计数器，当前执行指令地址 */
    UINTPTR fp;                    /* 帧指针，用于栈回溯 */
} PerfRegs;

/**
 * @brief 性能采样调用链结构体
 * @details 存储函数调用链的指令指针信息
 */
typedef struct {
    UINT32  ipNr;                  /* 调用链中有效指令指针数量 */
    IpInfo  ip[PERF_MAX_CALLCHAIN_DEPTH]; /* 调用链指令指针数组，最大深度由PERF_MAX_CALLCHAIN_DEPTH定义 */
} PerfBackTrace;

/**
 * @brief 性能采样数据结构体
 * @details 记录单次性能事件采样的详细信息
 */
typedef struct {
    UINT32        cpuid;           /* CPU核心ID */
    UINT32        taskId;          /* 任务ID */
    UINT32        processId;       /* 进程ID */
    UINT32        eventId;         /* 事件类型ID */
    UINT32        period;          /* 采样周期 */
    UINT64        time;            /* 采样时间戳 */
    IpInfo        pc;              /* 指令指针信息 */
    PerfBackTrace callChain;       /* 调用链信息 */
} PerfSampleData;

/**
 * @brief 性能数据文件头结构体
 * @details 用于标识性能采样数据文件的元信息
 */
typedef struct {
    UINT32      magic;             /* 魔术字，必须为PERF_DATA_MAGIC_WORD */
    UINT32      eventType;         /* 事件类型，对应enum PerfEventType */
    UINT32      len;               /* 采样数据文件长度（字节） */
    UINT32      sampleType;        /* 采样类型，如IP（指令指针）、TID（任务ID）、TIMESTAMP（时间戳）等的组合 */
    UINT32      sectionId;         /* 数据段ID，用于区分不同类型的采样数据 */
} PerfDataHdr;

/**
 * @brief 性能事件计数器结构体
 * @details 记录单个性能事件的计数信息
 */
typedef struct {
    UINT32 counter;                /* 当前计数值 */
    UINT32 eventId;                /* 事件ID */
    UINT32 period;                 /* 采样周期阈值 */
    UINT64 count[LOSCFG_KERNEL_CORE_NUM]; /* 每个CPU核心的事件计数数组 */
} Event;

/**
 * @brief 性能事件集合结构体
 * @details 管理多个性能事件的容器
 */
typedef struct {
    Event per[PERF_MAX_EVENT];     /* 事件数组，最大支持PERF_MAX_EVENT个事件 */
    UINT8 nr;                      /* 当前有效事件数量 */
    UINT8 cntDivided;              /* 计数是否已分频标志 */
} PerfEvent;

/**
 * @brief PMU（性能监控单元）操作结构体
 * @details 定义PMU设备的配置、启动、停止等操作接口
 */
typedef struct {
    enum PerfEventType type;       /* PMU类型 */
    PerfEvent events;              /* 事件集合 */
    UINT32 (*config)(VOID);        /* 配置PMU函数指针 */
    UINT32 (*start)(VOID);         /* 启动PMU函数指针 */
    UINT32 (*stop)(VOID);          /* 停止PMU函数指针 */
    CHAR *(*getName)(Event *event); /* 获取事件名称函数指针 */
} Pmu;

/**
 * @brief 性能采样控制块结构体
 * @details 管理性能采样的全局状态和配置信息
 */
typedef struct {
    /* 时间信息 */
    UINT64                  startTime;       /* 采样开始时间戳 */
    UINT64                  endTime;         /* 采样结束时间戳 */

    /* 采样状态 */
    enum PerfStatus         status;          /* 全局采样状态 */
    enum PmuStatus          pmuStatusPerCpu[LOSCFG_KERNEL_CORE_NUM]; /* 每个CPU核心的PMU状态 */

    /* 配置信息 */
    UINT32                  sampleType;      /* 采样类型 */
    UINT32                  taskIds[PERF_MAX_FILTER_TSKS]; /* 任务过滤ID数组 */
    UINT8                   taskIdsNr;       /* 任务过滤ID数量 */
    UINT32                  processIds[PERF_MAX_FILTER_TSKS]; /* 进程过滤ID数组 */
    UINT8                   processIdsNr;    /* 进程过滤ID数量 */
    UINT8                   needSample;      /* 是否需要采样标志 */
} PerfCB;

/**
 * @brief 架构相关的中断寄存器获取函数（默认实现）
 * @param regs 寄存器信息结构体指针
 * @param curTask 当前任务控制块指针
 * @note 若架构未定义OsPerfArchFetchIrqRegs，则使用此空实现
 */
#ifndef OsPerfArchFetchIrqRegs
STATIC INLINE VOID OsPerfArchFetchIrqRegs(PerfRegs *regs, LosTaskCB *curTask) {}
#endif

/**
 * @brief 获取中断时的寄存器信息
 * @param regs 寄存器信息结构体指针，用于存储获取的寄存器值
 * @note 调用架构相关的OsPerfArchFetchIrqRegs实现，并打印调试信息
 */
STATIC INLINE VOID OsPerfFetchIrqRegs(PerfRegs *regs)
{
    LosTaskCB *curTask = OsCurrTaskGet();  /* 获取当前任务控制块 */
    OsPerfArchFetchIrqRegs(regs, curTask); /* 调用架构相关实现获取寄存器 */
    PRINT_DEBUG("pc = 0x%x, fp = 0x%x\n", regs->pc, regs->fp); /* 打印PC和FP寄存器值用于调试 */
}

/**
 * @brief 架构相关的调用者寄存器获取函数（默认实现）
 * @param regs 寄存器信息结构体指针
 * @note 若架构未定义OsPerfArchFetchCallerRegs，则使用此空实现
 */
#ifndef OsPerfArchFetchCallerRegs
STATIC INLINE VOID OsPerfArchFetchCallerRegs(PerfRegs *regs) {}
#endif

/**
 * @brief 获取调用者的寄存器信息
 * @param regs 寄存器信息结构体指针，用于存储获取的寄存器值
 * @note 调用架构相关的OsPerfArchFetchCallerRegs实现，并打印调试信息
 */
STATIC INLINE VOID OsPerfFetchCallerRegs(PerfRegs *regs)
{
    OsPerfArchFetchCallerRegs(regs);       /* 调用架构相关实现获取寄存器 */
    PRINT_DEBUG("pc = 0x%x, fp = 0x%x\n", regs->pc, regs->fp); /* 打印PC和FP寄存器值用于调试 */
}

extern VOID OsPerfSetIrqRegs(UINTPTR pc, UINTPTR fp);
extern VOID OsPerfUpdateEventCount(Event *event, UINT32 value);
extern VOID OsPerfHandleOverFlow(Event *event, PerfRegs *regs);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_PERF_PRI_H */
