/*
 * Copyright (c) 2013-2019 Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020-2022 Huawei Device Co., Ltd. All rights reserved.
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
 * @file    los_trace.c
 * @brief
 * @link kernel-small-debug-trace http://weharmonyos.com/openharmony/zh-cn/device-dev/kernel/kernel-small-debug-trace.html @endlink
   @verbatim
	基本概念
		Trace调测旨在帮助开发者获取内核的运行流程，各个模块、任务的执行顺序，从而可以辅助开发者定位一些时序问题
		或者了解内核的代码运行过程。
	相关宏
		LOSCFG_KERNEL_TRACE				Trace模块的裁剪开关				YES/NO
		LOSCFG_RECORDER_MODE_OFFLINE	Trace工作模式为离线模式				YES/NO
		LOSCFG_RECORDER_MODE_ONLINE		Trace工作模式为在线模式				YES/NO
		LOSCFG_TRACE_CLIENT_INTERACT	使能与Trace IDE （dev tools）的交互，包括数据可视化和流程控制	YES/NO
		LOSCFG_TRACE_FRAME_CORE_MSG		记录CPUID、中断状态、锁任务状态	YES/NO
		LOSCFG_TRACE_FRAME_EVENT_COUNT	记录事件的次序编号					YES/NO
		LOSCFG_TRACE_FRAME_MAX_PARAMS	配置记录事件的最大参数个数				INT
		LOSCFG_TRACE_BUFFER_SIZE		配置Trace的缓冲区大小				INT
	运行机制
		内核提供一套Hook框架，将Hook点预埋在各个模块的主要流程中, 在内核启动初期完成Trace功能的初始化，
		并注册Trace的处理函数到Hook中。

		当系统触发到一个Hook点时，Trace模块会对输入信息进行封装，添加Trace帧头信息，包含事件类型、
		运行的cpuid、运行的任务id、运行的相对时间戳等信息；

		Trace提供2种工作模式，离线模式和在线模式。
			在线模式需要配合IDE使用，实时将trace frame记录发送给IDE，IDE端进行解析并可视化展示。
			离线模式会将trace frame记录到预先申请好的循环buffer中。如果循环buffer记录的frame过多则可能出现翻转，
			会覆盖之前的记录，故保持记录的信息始终是最新的信息。Trace循环buffer的数据可以通过shell命令导出进行详细分析，
			导出信息已按照时间戳信息完成排序。
   @endverbatim 
 * @image html https://gitee.com/weharmonyos/resources/raw/master/80/trace.png  
 * @version 
 * @author  weharmonyos.com | 鸿蒙研究站 | 每天死磕一点点
 * @date    2025-07-17
 */

#include "los_trace_pri.h"
#include "trace_pipeline.h"
#include "los_memory.h"
#include "los_config.h"
#include "securec.h"
#include "trace_cnv.h"
#include "los_init.h"
#include "los_process.h"
#include "los_sched_pri.h"

#ifdef LOSCFG_KERNEL_SMP
#include "los_mp.h"
#endif

#ifdef LOSCFG_SHELL
#include "shcmd.h"
#include "shell.h"
#endif

/* 全局变量定义 */
/* 跟踪事件计数器，记录事件总数，初始值为0 */
LITE_OS_SEC_BSS STATIC UINT32 g_traceEventCount;
/* 跟踪状态枚举变量，初始化为未初始化状态(TRACE_UNINIT) */
LITE_OS_SEC_BSS STATIC volatile enum TraceState g_traceState = TRACE_UNINIT;
/* 跟踪使能标志，初始化为禁用(FALSE)，LITE_OS_SEC_DATA_INIT表示数据段初始化 */
LITE_OS_SEC_DATA_INIT STATIC volatile BOOL g_enableTrace = FALSE;
/* 跟踪事件掩码，用于过滤事件类型，初始值为默认掩码(TRACE_DEFAULT_MASK) */
LITE_OS_SEC_BSS STATIC UINT32 g_traceMask = TRACE_DEFAULT_MASK;

/* 跟踪事件钩子函数指针，用于注册事件处理函数 */
TRACE_EVENT_HOOK g_traceEventHook = NULL;
/* 跟踪转储钩子函数指针，用于注册跟踪数据转储函数 */
TRACE_DUMP_HOOK g_traceDumpHook = NULL;

#ifdef LOSCFG_TRACE_CONTROL_AGENT
/* 跟踪控制代理任务ID，用于标识控制代理任务 */
LITE_OS_SEC_BSS STATIC UINT32 g_traceTaskId;
#endif

/* 事件掩码宏，值为0xFFFFFFF0（十进制4294967280），用于过滤事件类型高4位 */
#define EVENT_MASK            0xFFFFFFF0
/* 最小值宏，返回两个参数中的较小值 */
#define MIN(x, y)             ((x) < (y) ? (x) : (y))

/* 硬件中断过滤钩子函数指针，用于注册自定义中断过滤函数 */
LITE_OS_SEC_BSS STATIC TRACE_HWI_FILTER_HOOK g_traceHwiFilterHook = NULL;

#ifdef LOSCFG_KERNEL_SMP
/* SMP系统下的跟踪自旋锁，用于多核心同步，SPIN_LOCK_INIT宏初始化自旋锁 */
LITE_OS_SEC_BSS SPIN_LOCK_INIT(g_traceSpin);
#endif

/*
 * 静态内联函数：硬件中断过滤
 * 功能：判断是否过滤指定硬件中断
 * 参数：
 *   hwiNum - 中断号
 * 返回值：
 *   BOOL - TRUE表示过滤（不跟踪），FALSE表示不过滤（跟踪）
 * 说明：默认过滤UART中断(NUM_HAL_INTERRUPT_UART)和系统滴答中断(OS_TICK_INT_NUM)，
 *       SMP系统下额外过滤调度IPI中断(LOS_MP_IPI_SCHEDULE)，支持通过钩子函数扩展过滤规则
 */
STATIC_INLINE BOOL OsTraceHwiFilter(UINT32 hwiNum)
{
    BOOL ret = ((hwiNum == NUM_HAL_INTERRUPT_UART) || (hwiNum == OS_TICK_INT_NUM));
#ifdef LOSCFG_KERNEL_SMP
    ret |= (hwiNum == LOS_MP_IPI_SCHEDULE);
#endif
    if (g_traceHwiFilterHook != NULL) {
        ret |= g_traceHwiFilterHook(hwiNum);
    }
    return ret;
}

/*
 * 静态函数：设置跟踪事件帧
 * 功能：填充跟踪事件帧的基本信息，包括任务ID、进程ID、时间戳、事件类型等
 * 参数：
 *   frame - 事件帧指针，用于存储事件信息
 *   eventType - 事件类型
 *   identity - 事件关联标识（如任务ID、中断号等）
 *   params - 参数数组
 *   paramCount - 参数数量
 * 返回值：无
 * 注意事项：
 *   1. 使用TRACE_LOCK/TRACE_UNLOCK宏进行临界区保护
 *   2. 参数数量超过最大限制(LOSCFG_TRACE_FRAME_MAX_PARAMS)时截断
 *   3. 根据配置宏(LOSCFG_TRACE_FRAME_CORE_MSG等)决定是否填充扩展信息
 */
STATIC VOID OsTraceSetFrame(TraceEventFrame *frame, UINT32 eventType, UINTPTR identity, const UINTPTR *params,
    UINT16 paramCount)
{
    INT32 i;
    UINT32 intSave;

    (VOID)memset_s(frame, sizeof(TraceEventFrame), 0, sizeof(TraceEventFrame));

    if (paramCount > LOSCFG_TRACE_FRAME_MAX_PARAMS) {
        paramCount = LOSCFG_TRACE_FRAME_MAX_PARAMS;
    }

    TRACE_LOCK(intSave);
    frame->curTask   = OsTraceGetMaskTid(LOS_CurTaskIDGet()); /* 当前任务ID（经过掩码处理） */
    frame->curPid    = LOS_GetCurrProcessID(); /* 当前进程ID */
    frame->identity  = identity; /* 事件关联标识 */
    frame->curTime   = HalClockGetCycles(); /* 当前时间戳（时钟周期数） */
    frame->eventType = eventType; /* 事件类型 */

#ifdef LOSCFG_TRACE_FRAME_CORE_MSG
    frame->core.cpuid      = ArchCurrCpuid(); /* 当前CPU核心ID */
    frame->core.hwiActive  = OS_INT_ACTIVE ? TRUE : FALSE; /* 中断激活状态 */
    frame->core.taskLockCnt = MIN(OsSchedLockCountGet(), 0xF); /* 任务调度锁计数（4位，最大值0xF即15） */
    frame->core.paramCount = paramCount; /* 参数数量 */
#endif

#ifdef LOS_TRACE_FRAME_LR
    /* 从栈帧获取链接寄存器值并存储到事件帧 */
    LOS_RecordLR(frame->linkReg, LOS_TRACE_LR_RECORD, LOS_TRACE_LR_RECORD, LOS_TRACE_LR_IGNORE);
#endif

#ifdef LOSCFG_TRACE_FRAME_EVENT_COUNT
    frame->eventCount = g_traceEventCount; /* 事件计数器值 */
    g_traceEventCount++; /* 事件计数器自增 */
#endif
    TRACE_UNLOCK(intSave);

    for (i = 0; i < paramCount; i++) {
        frame->params[i] = params[i]; /* 填充参数数组 */
    }
}

/*
 * 函数：设置对象数据
 * 功能：将任务控制块(TCB)信息填充到对象数据结构
 * 参数：
 *   obj - 对象数据指针
 *   tcb - 任务控制块指针
 * 返回值：无
 * 说明：复制任务名称时使用strncpy_s保证安全，若复制失败输出错误信息
 */
VOID OsTraceSetObj(ObjData *obj, const LosTaskCB *tcb)
{
    errno_t ret;
    SchedParam param = { 0 };
    (VOID)memset_s(obj, sizeof(ObjData), 0, sizeof(ObjData));

    obj->id   = OsTraceGetMaskTid(tcb->taskID); /* 任务ID（经过掩码处理） */
    tcb->ops->schedParamGet(tcb, &param);
    obj->prio = param.priority; /* 任务优先级 */

    ret = strncpy_s(obj->name, LOSCFG_TRACE_OBJ_MAX_NAME_SIZE, tcb->taskName, LOSCFG_TRACE_OBJ_MAX_NAME_SIZE - 1);
    if (ret != EOK) {
        TRACE_ERROR("Task name copy failed!\n");
    }
}

/*
 * 函数：跟踪事件钩子
 * 功能：处理跟踪事件，根据事件类型和过滤规则决定是否记录事件
 * 参数：
 *   eventType - 事件类型
 *   identity - 事件关联标识
 *   params - 参数数组
 *   paramCount - 参数数量
 * 返回值：无
 * 说明：
 *   1. 对TASK_CREATE和TASK_PRIOSET事件强制添加对象信息
 *   2. 根据使能标志(g_enableTrace)和事件掩码(g_traceMask)过滤事件
 *   3. 硬件中断事件需通过OsTraceHwiFilter过滤
 *   4. 任务事件需对任务ID进行掩码处理
 */
VOID OsTraceHook(UINT32 eventType, UINTPTR identity, const UINTPTR *params, UINT16 paramCount)
{
    TraceEventFrame frame;
    if ((eventType == TASK_CREATE) || (eventType == TASK_PRIOSET)) {
        OsTraceObjAdd(eventType, identity); /* 处理重要对象信息，不可过滤 */
    }

    if ((g_enableTrace == TRUE) && (eventType & g_traceMask)) {
        UINTPTR id = identity;
        if (TRACE_GET_MODE_FLAG(eventType) == TRACE_HWI_FLAG) {
            if (OsTraceHwiFilter(identity)) {
                return; /* 过滤指定硬件中断事件 */
            }
        } else if (TRACE_GET_MODE_FLAG(eventType) == TRACE_TASK_FLAG) {
            id = OsTraceGetMaskTid(identity); /* 任务ID掩码处理 */
        } else if (eventType == MEM_INFO_REQ) {
            LOS_MEM_POOL_STATUS status;
            LOS_MemInfoGet((VOID *)identity, &status);
            LOS_TRACE(MEM_INFO, identity, status.totalUsedSize, status.totalFreeSize);
            return;
        }

        OsTraceSetFrame(&frame, eventType, id, params, paramCount);
        OsTraceWriteOrSendEvent(&frame); /* 写入或发送事件帧 */
    }
}

/*
 * 函数：检查跟踪是否使能
 * 功能：返回当前跟踪使能状态
 * 参数：无
 * 返回值：
 *   BOOL - TRUE表示跟踪已使能，FALSE表示未使能
 */
BOOL OsTraceIsEnable(VOID)
{
    return g_enableTrace;
}

/*
 * 静态函数：安装跟踪钩子
 * 功能：注册事件钩子和转储钩子函数
 * 参数：无
 * 返回值：无
 * 说明：离线模式下注册OsTraceRecordDump为转储钩子
 */
STATIC VOID OsTraceHookInstall(VOID)
{
    g_traceEventHook = OsTraceHook;
#ifdef LOSCFG_RECORDER_MODE_OFFLINE
    g_traceDumpHook = OsTraceRecordDump;
#endif
}

#ifdef LOSCFG_TRACE_CONTROL_AGENT
/*
 * 静态函数：检查跟踪命令有效性
 * 功能：验证命令格式是否正确
 * 参数：
 *   msg - 命令消息指针
 * 返回值：
 *   BOOL - TRUE表示命令有效，FALSE表示无效
 * 说明：命令需以TRACE_CMD_END_CHAR结尾且命令码小于TRACE_CMD_MAX_CODE
 */
STATIC BOOL OsTraceCmdIsValid(const TraceClientCmd *msg)
{
    return ((msg->end == TRACE_CMD_END_CHAR) && (msg->cmd < TRACE_CMD_MAX_CODE));
}

/*
 * 静态函数：处理跟踪命令
 * 功能：根据命令码执行相应操作
 * 参数：
 *   msg - 命令消息指针
 * 返回值：无
 * 支持命令：
 *   TRACE_CMD_START - 启动跟踪
 *   TRACE_CMD_STOP - 停止跟踪
 *   TRACE_CMD_SET_EVENT_MASK - 设置事件掩码
 *   TRACE_CMD_RECODE_DUMP - 转储跟踪记录
 */
STATIC VOID OsTraceCmdHandle(const TraceClientCmd *msg)
{
    if (!OsTraceCmdIsValid(msg)) {
        return;
    }

    switch (msg->cmd) {
        case TRACE_CMD_START:
            LOS_TraceStart();
            break;
        case TRACE_CMD_STOP:
            LOS_TraceStop();
            break;
        case TRACE_CMD_SET_EVENT_MASK:
            /* 4个UINT8参数组合成UINT32掩码 */
            LOS_TraceEventMaskSet(TRACE_MASK_COMBINE(msg->param1, msg->param2, msg->param3, msg->param4));
            break;
        case TRACE_CMD_RECODE_DUMP:
            LOS_TraceRecordDump(TRUE);
            break;
        default:
            break;
    }
}

/*
 * 函数：跟踪代理任务
 * 功能：循环等待并处理客户端命令
 * 参数：无
 * 返回值：无
 * 说明：无限循环中调用OsTraceDataWait等待数据，接收后调用OsTraceCmdHandle处理
 */
VOID TraceAgent(VOID)
{
    UINT32 ret;
    TraceClientCmd msg;

    while (1) {
        (VOID)memset_s(&msg, sizeof(TraceClientCmd), 0, sizeof(TraceClientCmd));
        ret = OsTraceDataWait();
        if (ret == LOS_OK) {
            OsTraceDataRecv((UINT8 *)&msg, sizeof(TraceClientCmd), 0);
            OsTraceCmdHandle(&msg);
        }
    }
}

/*
 * 静态函数：创建跟踪代理任务
 * 功能：初始化任务参数并创建跟踪代理任务
 * 参数：无
 * 返回值：
 *   UINT32 - 错误码，LOS_OK表示成功
 * 说明：任务优先级为LOSCFG_TRACE_TASK_PRIORITY，栈大小为默认值，SMP系统下绑定到当前CPU核心
 */
STATIC UINT32 OsCreateTraceAgentTask(VOID)
{
    UINT32 ret;
    TSK_INIT_PARAM_S taskInitParam;

    (VOID)memset_s((VOID *)(&taskInitParam), sizeof(TSK_INIT_PARAM_S), 0, sizeof(TSK_INIT_PARAM_S));
    taskInitParam.pfnTaskEntry = (TSK_ENTRY_FUNC)TraceAgent;
    taskInitParam.usTaskPrio = LOSCFG_TRACE_TASK_PRIORITY;
    taskInitParam.pcName = "TraceAgent";
    taskInitParam.uwStackSize = LOSCFG_BASE_CORE_TSK_DEFAULT_STACK_SIZE;
#ifdef LOSCFG_KERNEL_SMP
    taskInitParam.usCpuAffiMask = CPUID_TO_AFFI_MASK(ArchCurrCpuid());
#endif
    ret = LOS_TaskCreate(&g_traceTaskId, &taskInitParam);
    return ret;
}
#endif

/*
 * 静态函数：跟踪模块初始化
 * 功能：初始化跟踪系统，包括管道、代理任务、缓冲区等
 * 参数：无
 * 返回值：
 *   UINT32 - 错误码，LOS_OK表示成功
 * 说明：
 *   1. 仅允许初始化一次，已初始化则返回错误
 *   2. 按条件编译配置初始化相应模块
 *   3. 初始化成功后设置跟踪状态和使能标志
 */
STATIC UINT32 OsTraceInit(VOID)
{
    UINT32 ret;

    if (g_traceState != TRACE_UNINIT) {
        TRACE_ERROR("trace has been initialized already, the current state is :%d\n", g_traceState);
        ret = LOS_ERRNO_TRACE_ERROR_STATUS;
        goto LOS_ERREND;
    }

#ifdef LOSCFG_TRACE_CLIENT_INTERACT
    ret = OsTracePipelineInit();
    if (ret != LOS_OK) {
        goto LOS_ERREND;
    }
#endif

#ifdef LOSCFG_TRACE_CONTROL_AGENT
    ret = OsCreateTraceAgentTask();
    if (ret != LOS_OK) {
        TRACE_ERROR("trace init create agentTask error :0x%x\n", ret);
        goto LOS_ERREND;
    }
#endif

#ifdef LOSCFG_RECORDER_MODE_OFFLINE
    ret = OsTraceBufInit(LOSCFG_TRACE_BUFFER_SIZE);
    if (ret != LOS_OK) {
#ifdef LOSCFG_TRACE_CONTROL_AGENT
        (VOID)LOS_TaskDelete(g_traceTaskId);
#endif
        goto LOS_ERREND;
    }
#endif

    OsTraceHookInstall();
    OsTraceCnvInit();

    g_traceEventCount = 0;

#ifdef LOSCFG_RECORDER_MODE_ONLINE  /* 等待跟踪客户端启动跟踪 */
    g_enableTrace = FALSE;
    g_traceState = TRACE_INITED;
#else
    g_enableTrace = TRUE;
    g_traceState = TRACE_STARTED;
#endif
    return LOS_OK;
LOS_ERREND:
    return ret;
}

/*
 * 函数：启动跟踪
 * 功能：将跟踪状态设置为启动，并使能事件记录
 * 参数：无
 * 返回值：
 *   UINT32 - 错误码，LOS_OK表示成功
 * 说明：
 *   1. 仅在已初始化(TRACE_INITED)状态下可启动
 *   2. 启动后触发MEM_INFO_REQ事件获取内存信息
 */
UINT32 LOS_TraceStart(VOID)
{
    UINT32 intSave;
    UINT32 ret = LOS_OK;

    TRACE_LOCK(intSave);
    if (g_traceState == TRACE_STARTED) {
        goto START_END;
    }

    if (g_traceState == TRACE_UNINIT) {
        TRACE_ERROR("trace not inited, be sure LOS_TraceInit excute success\n");
        ret = LOS_ERRNO_TRACE_ERROR_STATUS;
        goto START_END;
    }

    OsTraceNotifyStart();

    g_enableTrace = TRUE;
    g_traceState = TRACE_STARTED;

    TRACE_UNLOCK(intSave);
    LOS_TRACE(MEM_INFO_REQ, m_aucSysMem0);
    return ret;
START_END:
    TRACE_UNLOCK(intSave);
    return ret;
}

/*
 * 函数：停止跟踪
 * 功能：将跟踪状态设置为停止，并禁用事件记录
 * 参数：无
 * 返回值：无
 * 说明：仅在已启动(TRACE_STARTED)状态下可停止
 */
VOID LOS_TraceStop(VOID)
{
    UINT32 intSave;

    TRACE_LOCK(intSave);
    if (g_traceState != TRACE_STARTED) {
        goto STOP_END;
    }

    g_enableTrace = FALSE;
    g_traceState = TRACE_STOPED;
    OsTraceNotifyStop();
STOP_END:
    TRACE_UNLOCK(intSave);
}

/*
 * 函数：设置跟踪事件掩码
 * 功能：更新事件掩码，用于过滤需要记录的事件类型
 * 参数：
 *   mask - 新的事件掩码
 * 返回值：无
 * 说明：掩码与EVENT_MASK进行与运算，保留低28位有效位
 */
VOID LOS_TraceEventMaskSet(UINT32 mask)
{
    g_traceMask = mask & EVENT_MASK;
}

/*
 * 函数：转储跟踪记录
 * 功能：将跟踪记录输出，仅在跟踪停止后可执行
 * 参数：
 *   toClient - 是否发送到客户端，TRUE表示发送，FALSE表示本地输出
 * 返回值：无
 * 说明：若跟踪未停止，输出错误信息并返回
 */
VOID LOS_TraceRecordDump(BOOL toClient)
{
    if (g_traceState != TRACE_STOPED) {
        TRACE_ERROR("trace dump must after trace stopped , the current state is : %d\n", g_traceState);
        return;
    }
    OsTraceRecordDump(toClient);
}

/*
 * 函数：获取跟踪记录
 * 功能：返回跟踪记录的头部指针
 * 参数：无
 * 返回值：
 *   OfflineHead* - 跟踪记录头部指针
 */
OfflineHead *LOS_TraceRecordGet(VOID)
{
    return OsTraceRecordGet();
}

/*
 * 函数：重置跟踪系统
 * 功能：重置跟踪状态和相关资源
 * 参数：无
 * 返回值：无
 * 说明：若跟踪未初始化，输出错误信息并返回
 */
VOID LOS_TraceReset(VOID)
{
    if (g_traceState == TRACE_UNINIT) {
        TRACE_ERROR("trace not inited, be sure LOS_TraceInit excute success\n");
        return;
    }

    OsTraceReset();
}

/*
 * 函数：注册硬件中断过滤钩子
 * 功能：设置自定义硬件中断过滤函数
 * 参数：
 *   hook - 中断过滤钩子函数
 * 返回值：无
 * 说明：使用TRACE_LOCK/TRACE_UNLOCK宏进行临界区保护
 */
VOID LOS_TraceHwiFilterHookReg(TRACE_HWI_FILTER_HOOK hook)
{
    UINT32 intSave;

    TRACE_LOCK(intSave);
    g_traceHwiFilterHook = hook;
    TRACE_UNLOCK(intSave);
}

#ifdef LOSCFG_SHELL
/*
 * 函数：shell命令-设置跟踪掩码
 * 功能：通过shell命令设置跟踪事件掩码
 * 参数：
 *   argc - 参数数量
 *   argv - 参数数组
 * 返回值：
 *   UINT32 - 错误码，LOS_OK表示成功
 * 说明：支持无参数（恢复默认掩码）或一个参数（指定新掩码）
 */
LITE_OS_SEC_TEXT_MINOR UINT32 OsShellCmdTraceSetMask(INT32 argc, const CHAR **argv)
{
    size_t mask;
    CHAR *endPtr = NULL;

    if (argc >= 2) { /* 2:参数数量 */
        PRINTK("\nUsage: trace_mask or trace_mask ID\n");
        return OS_ERROR;
    }

    if (argc == 0) {
        mask = TRACE_DEFAULT_MASK;
    } else {
        mask = strtoul(argv[0], &endPtr, 0);
    }
    LOS_TraceEventMaskSet((UINT32)mask);
    return LOS_OK;
}

/*
 * 函数：shell命令-转储跟踪记录
 * 功能：通过shell命令触发跟踪记录转储
 * 参数：
 *   argc - 参数数量
 *   argv - 参数数组
 * 返回值：
 *   UINT32 - 错误码，LOS_OK表示成功
 * 说明：支持无参数（本地输出）或一个参数（1发送到客户端，0本地输出）
 */
LITE_OS_SEC_TEXT_MINOR UINT32 OsShellCmdTraceDump(INT32 argc, const CHAR **argv)
{
    BOOL toClient;
    CHAR *endPtr = NULL;

    if (argc >= 2) { /* 2:参数数量 */
        PRINTK("\nUsage: trace_dump or trace_dump [1/0]\n");
        return OS_ERROR;
    }

    if (argc == 0) {
        toClient = FALSE;
    } else {
        toClient = strtoul(argv[0], &endPtr, 0) != 0 ? TRUE : FALSE;
    }
    LOS_TraceRecordDump(toClient);
    return LOS_OK;
}

/* shell命令注册：跟踪相关命令 */
SHELLCMD_ENTRY(tracestart_shellcmd,   CMD_TYPE_EX, "trace_start", 0, (CmdCallBackFunc)LOS_TraceStart);
SHELLCMD_ENTRY(tracestop_shellcmd,    CMD_TYPE_EX, "trace_stop",  0, (CmdCallBackFunc)LOS_TraceStop);
SHELLCMD_ENTRY(tracesetmask_shellcmd, CMD_TYPE_EX, "trace_mask",  1, (CmdCallBackFunc)OsShellCmdTraceSetMask);
SHELLCMD_ENTRY(tracereset_shellcmd,   CMD_TYPE_EX, "trace_reset", 0, (CmdCallBackFunc)LOS_TraceReset);
SHELLCMD_ENTRY(tracedump_shellcmd,    CMD_TYPE_EX, "trace_dump", 1, (CmdCallBackFunc)OsShellCmdTraceDump);
#endif

/* 模块初始化：将OsTraceInit注册为扩展模块初始化函数，初始化级别为LOS_INIT_LEVEL_KMOD_EXTENDED */
LOS_MODULE_INIT(OsTraceInit, LOS_INIT_LEVEL_KMOD_EXTENDED);
