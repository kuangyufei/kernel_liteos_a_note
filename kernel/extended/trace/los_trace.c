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
 * @date    2021-11-20
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
#include "los_memory.h"
#include "los_config.h"
#include "securec.h"
#include "trace_cnv.h"
#include "los_init.h"
#include "los_process.h"

#ifdef LOSCFG_KERNEL_SMP
#include "los_mp.h"
#endif

#ifdef LOSCFG_SHELL
#include "shcmd.h"
#include "shell.h"
#endif

LITE_OS_SEC_BSS STATIC UINT32 g_traceEventCount;
LITE_OS_SEC_BSS STATIC volatile enum TraceState g_traceState = TRACE_UNINIT;
LITE_OS_SEC_DATA_INIT STATIC volatile BOOL g_enableTrace = FALSE; ///< trace开关
LITE_OS_SEC_BSS STATIC UINT32 g_traceMask = TRACE_DEFAULT_MASK;	///< 全局变量设置事件掩码，仅记录某些模块的事件

TRACE_EVENT_HOOK g_traceEventHook = NULL;	///< 事件钩子函数
TRACE_DUMP_HOOK g_traceDumpHook = NULL;	///< 输出缓冲区数据

#ifdef LOSCFG_TRACE_CONTROL_AGENT
LITE_OS_SEC_BSS STATIC UINT32 g_traceTaskId;
#endif

#define EVENT_MASK            0xFFFFFFF0
#define MIN(x, y)             ((x) < (y) ? (x) : (y))

LITE_OS_SEC_BSS STATIC TRACE_HWI_FILTER_HOOK g_traceHwiFilterHook = NULL; ///< 用于跟踪硬中断过滤的钩子函数

#ifdef LOSCFG_KERNEL_SMP
LITE_OS_SEC_BSS SPIN_LOCK_INIT(g_traceSpin);
#endif

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
    frame->curTask   = OsTraceGetMaskTid(LOS_CurTaskIDGet());
    frame->curPid    = LOS_GetCurrProcessID();
    frame->identity  = identity;
    frame->curTime   = HalClockGetCycles();
    frame->eventType = eventType;

#ifdef LOSCFG_TRACE_FRAME_CORE_MSG
    frame->core.cpuId      = ArchCurrCpuid();
    frame->core.hwiActive  = OS_INT_ACTIVE ? TRUE : FALSE;
    frame->core.taskLockCnt = MIN(OsPercpuGet()->taskLockCnt, 0xF); /* taskLockCnt is 4 bits, max value = 0xF */
    frame->core.paramCount = paramCount;
#endif

#ifdef LOS_TRACE_FRAME_LR
    /* Get the linkreg from stack fp and storage to frame */
    LOS_RecordLR(frame->linkReg, LOS_TRACE_LR_RECORD, LOS_TRACE_LR_RECORD, LOS_TRACE_LR_IGNORE);
#endif

#ifdef LOSCFG_TRACE_FRAME_EVENT_COUNT
    frame->eventCount = g_traceEventCount;
    g_traceEventCount++;
#endif
    TRACE_UNLOCK(intSave);

    for (i = 0; i < paramCount; i++) {
        frame->params[i] = params[i];
    }
}

VOID OsTraceSetObj(ObjData *obj, const LosTaskCB *tcb)
{
    errno_t ret;
    (VOID)memset_s(obj, sizeof(ObjData), 0, sizeof(ObjData));

    obj->id   = OsTraceGetMaskTid(tcb->taskID);
    obj->prio = tcb->priority;

    ret = strncpy_s(obj->name, LOSCFG_TRACE_OBJ_MAX_NAME_SIZE, tcb->taskName, LOSCFG_TRACE_OBJ_MAX_NAME_SIZE - 1);
    if (ret != EOK) {
        TRACE_ERROR("Task name copy failed!\n");
    }
}

/*!
 * @brief OsTraceHook	
 * 事件统一处理函数
 * @param eventType	
 * @param identity	
 * @param paramCount	
 * @param params	
 * @return	
 *
 * @see
 */
VOID OsTraceHook(UINT32 eventType, UINTPTR identity, const UINTPTR *params, UINT16 paramCount)
{
    TraceEventFrame frame;//离线和在线模式下, trace数据的保存和传送以帧为单位
    if ((eventType == TASK_CREATE) || (eventType == TASK_PRIOSET)) {//创建任务和设置任务优先级
        OsTraceObjAdd(eventType, identity); /* handle important obj info, these can not be filtered */
    }

    if ((g_enableTrace == TRUE) && (eventType & g_traceMask)) {//使能跟踪模块且事件未屏蔽
        UINTPTR id = identity;
        if (TRACE_GET_MODE_FLAG(eventType) == TRACE_HWI_FLAG) {//关于硬中断的事件
            if (OsTraceHwiFilter(identity)) {//检查中断号是否过滤掉了,注意:中断控制器本身是可以屏蔽中断号的
                return;
            }
        } else if (TRACE_GET_MODE_FLAG(eventType) == TRACE_TASK_FLAG) {//关于任务的事件
            id = OsTraceGetMaskTid(identity);//获取任务ID
        } else if (eventType == MEM_INFO_REQ) {//内存信息事件
            LOS_MEM_POOL_STATUS status;
            LOS_MemInfoGet((VOID *)identity, &status);//获取内存各项信息
            LOS_TRACE(MEM_INFO, identity, status.totalUsedSize, status.totalFreeSize);//打印信息
            return;
        }

        OsTraceSetFrame(&frame, eventType, id, params, paramCount);//创建帧数据
        OsTraceWriteOrSendEvent(&frame);//保存(离线模式下)或者发送(在线模式下)帧数据
    }
}

BOOL OsTraceIsEnable(VOID)
{
    return g_enableTrace;
}
/// 初始化事件处理函数
STATIC VOID OsTraceHookInstall(VOID)
{
    g_traceEventHook = OsTraceHook;
#ifdef LOSCFG_RECORDER_MODE_OFFLINE
    g_traceDumpHook = OsTraceRecordDump;
#endif
}

#ifdef LOSCFG_TRACE_CONTROL_AGENT
STATIC BOOL OsTraceCmdIsValid(const TraceClientCmd *msg)
{
    return ((msg->end == TRACE_CMD_END_CHAR) && (msg->cmd < TRACE_CMD_MAX_CODE));
}

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
            /* 4 params(UINT8) composition the mask(UINT32) */
            LOS_TraceEventMaskSet(TRACE_MASK_COMBINE(msg->param1, msg->param2, msg->param3, msg->param4));
            break;
        case TRACE_CMD_RECODE_DUMP:
            LOS_TraceRecordDump(TRUE);
            break;
        default:
            break;
    }
}
///< trace任务的入口函数
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

/*!
 * @brief OsCreateTraceAgentTask 创建trace任务	
 * 
 * @return	
 *
 * @see
 */
STATIC UINT32 OsCreateTraceAgentTask(VOID)
{
    UINT32 ret;
    TSK_INIT_PARAM_S taskInitParam;

    (VOID)memset_s((VOID *)(&taskInitParam), sizeof(TSK_INIT_PARAM_S), 0, sizeof(TSK_INIT_PARAM_S));
    taskInitParam.pfnTaskEntry = (TSK_ENTRY_FUNC)TraceAgent;	//任务入口函数
    taskInitParam.usTaskPrio = LOSCFG_TRACE_TASK_PRIORITY;	//任务优先级 2
    taskInitParam.pcName = "TraceAgent";	//任务名称
    taskInitParam.uwStackSize = LOSCFG_BASE_CORE_TSK_DEFAULT_STACK_SIZE; //内核栈大小 16K
#ifdef LOSCFG_KERNEL_SMP
    taskInitParam.usCpuAffiMask = CPUID_TO_AFFI_MASK(ArchCurrCpuid());//指定为当前CPU执行
#endif
    ret = LOS_TaskCreate(&g_traceTaskId, &taskInitParam);//创建任务并产生调度
    return ret;
}
#endif
/// 跟踪模块初始化
STATIC UINT32 OsTraceInit(VOID)
{
    UINT32 ret;

    if (g_traceState != TRACE_UNINIT) {
        TRACE_ERROR("trace has been initialized already, the current state is :%d\n", g_traceState);
        ret = LOS_ERRNO_TRACE_ERROR_STATUS;
        goto LOS_ERREND;
    }

#ifdef LOSCFG_TRACE_CLIENT_INTERACT //使能与Trace IDE （dev tools）的交互，包括数据可视化和流程控制
    ret = OsTracePipelineInit();//在线模式(管道模式)的初始化
    if (ret != LOS_OK) {
        goto LOS_ERREND;
    }
#endif

#ifdef LOSCFG_TRACE_CONTROL_AGENT //trace任务代理开关,所谓代理是创建专门的任务来处理 trace
    ret = OsCreateTraceAgentTask();
    if (ret != LOS_OK) {
        TRACE_ERROR("trace init create agentTask error :0x%x\n", ret);
        goto LOS_ERREND;
    }
#endif

#ifdef LOSCFG_RECORDER_MODE_OFFLINE //trace离线模式开关
//离线模式会将trace frame记录到预先申请好的循环buffer中。如果循环buffer记录的frame过多则可能出现翻转，
//会覆盖之前的记录，故保持记录的信息始终是最新的信息。
    ret = OsTraceBufInit(LOSCFG_TRACE_BUFFER_SIZE); //离线模式下buf 大小,这个大小决定了装多少 ObjData 和 TraceEventFrame
    if (ret != LOS_OK) {
#ifdef LOSCFG_TRACE_CONTROL_AGENT
        (VOID)LOS_TaskDelete(g_traceTaskId);
#endif
        goto LOS_ERREND;
    }
#endif

    OsTraceHookInstall();//安装HOOK框架
    OsTraceCnvInit();//将事件处理函数注册到HOOK框架

    g_traceEventCount = 0;

#ifdef LOSCFG_RECORDER_MODE_ONLINE  /* Wait trace client to start trace */
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
/// 启动Trace
UINT32 LOS_TraceStart(VOID)
{
    UINT32 intSave;
    UINT32 ret = LOS_OK;

    TRACE_LOCK(intSave);
    if (g_traceState == TRACE_STARTED) {
        goto START_END;
    }

    if (g_traceState == TRACE_UNINIT) {//必须初始化好
        TRACE_ERROR("trace not inited, be sure LOS_TraceInit excute success\n");
        ret = LOS_ERRNO_TRACE_ERROR_STATUS;
        goto START_END;
    }

    OsTraceNotifyStart();//通知系统开始

    g_enableTrace = TRUE; //使能trace功能
    g_traceState = TRACE_STARTED;//设置状态,已开始

    TRACE_UNLOCK(intSave);
    LOS_TRACE(MEM_INFO_REQ, m_aucSysMem0);//输出日志
    return ret;
START_END:
    TRACE_UNLOCK(intSave);
    return ret;
}
/// 停止Trace(跟踪)
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
/// 设置事件掩码，仅记录某些模块的事件
VOID LOS_TraceEventMaskSet(UINT32 mask)
{
    g_traceMask = mask & EVENT_MASK;
}
/// 输出Trace缓冲区数据
VOID LOS_TraceRecordDump(BOOL toClient)
{
    if (g_traceState != TRACE_STOPED) {
        TRACE_ERROR("trace dump must after trace stopped , the current state is : %d\n", g_traceState);
        return;
    }
    OsTraceRecordDump(toClient);
}
/// 获取Trace缓冲区的首地址
OfflineHead *LOS_TraceRecordGet(VOID)
{
    return OsTraceRecordGet();
}
/// 清除Trace缓冲区中的事件
VOID LOS_TraceReset(VOID)
{
    if (g_traceState == TRACE_UNINIT) {
        TRACE_ERROR("trace not inited, be sure LOS_TraceInit excute success\n");
        return;
    }

    OsTraceReset();
}
/// 注册过滤特定中断号事件的钩子函数
VOID LOS_TraceHwiFilterHookReg(TRACE_HWI_FILTER_HOOK hook)
{
    UINT32 intSave;

    TRACE_LOCK(intSave);
    g_traceHwiFilterHook = hook;// 注册全局钩子函数
    TRACE_UNLOCK(intSave);
}

#ifdef LOSCFG_SHELL
/// 通过shell命令 设置事件掩码，仅记录某些模块的事件
LITE_OS_SEC_TEXT_MINOR UINT32 OsShellCmdTraceSetMask(INT32 argc, const CHAR **argv)
{
    size_t mask;
    CHAR *endPtr = NULL;

    if (argc >= 2) { /* 2:Just as number of parameters */
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

LITE_OS_SEC_TEXT_MINOR UINT32 OsShellCmdTraceDump(INT32 argc, const CHAR **argv)
{
    BOOL toClient;
    CHAR *endPtr = NULL;

    if (argc >= 2) { /* 2:Just as number of parameters */
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

SHELLCMD_ENTRY(tracestart_shellcmd,   CMD_TYPE_EX, "trace_start", 0, (CmdCallBackFunc)LOS_TraceStart);//通过shell 启动trace
SHELLCMD_ENTRY(tracestop_shellcmd,    CMD_TYPE_EX, "trace_stop",  0, (CmdCallBackFunc)LOS_TraceStop);
SHELLCMD_ENTRY(tracesetmask_shellcmd, CMD_TYPE_EX, "trace_mask",  1, (CmdCallBackFunc)OsShellCmdTraceSetMask);//设置事件掩码，仅记录某些模块的事件
SHELLCMD_ENTRY(tracereset_shellcmd,   CMD_TYPE_EX, "trace_reset", 0, (CmdCallBackFunc)LOS_TraceReset);
SHELLCMD_ENTRY(tracedump_shellcmd,    CMD_TYPE_EX, "trace_dump", 1, (CmdCallBackFunc)OsShellCmdTraceDump);
#endif

LOS_MODULE_INIT(OsTraceInit, LOS_INIT_LEVEL_KMOD_EXTENDED);
