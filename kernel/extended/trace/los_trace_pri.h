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

#ifndef _LOS_TRACE_PRI_H
#define _LOS_TRACE_PRI_H

#include "los_trace.h"
#include "los_task_pri.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */


#ifdef LOSCFG_TRACE_CONTROL_AGENT
#define TRACE_CMD_END_CHAR                  0xD  // 跟踪命令结束符，ASCII码0xD对应回车符
#endif

#define TRACE_ERROR                         PRINT_ERR  // 跟踪模块错误打印宏，复用系统PRINT_ERR接口
#define TRACE_MODE_OFFLINE                  0  // 离线跟踪模式：数据先缓存后导出
#define TRACE_MODE_ONLINE                   1  // 在线跟踪模式：数据实时发送

/* 初始默认跟踪掩码：仅跟踪硬件中断和任务调度事件 */
#define TRACE_DEFAULT_MASK                  (TRACE_HWI_FLAG | TRACE_TASK_FLAG)
#define TRACE_CTL_MAGIC_NUM                 0xDEADBEEF  // 跟踪控制块魔术数，用于合法性校验
#define TRACE_BIGLITTLE_WORD                0x12345678  // 大小端检测字：小端模式存储为0x78563412，大端为0x12345678
#define TRACE_VERSION(MODE)                 (0xFFFFFFFF & (MODE))  // 跟踪版本计算宏，确保结果为32位无符号数
#define TRACE_MASK_COMBINE(c1, c2, c3, c4)  (((c1) << 24) | ((c2) << 16) | ((c3) << 8) | (c4))  // 组合4个字节为32位掩码

#define TRACE_GET_MODE_FLAG(type)           ((type) & 0xFFFFFFF0)  // 获取模式标志（高28位），屏蔽低4位类型信息

#ifdef LOSCFG_KERNEL_SMP
// SMP系统使用自旋锁进行并发控制
extern SPIN_LOCK_S g_traceSpin;
#define TRACE_LOCK(state)                   LOS_SpinLockSave(&g_traceSpin, &(state))  // 获取跟踪自旋锁并保存中断状态
#define TRACE_UNLOCK(state)                 LOS_SpinUnlockRestore(&g_traceSpin, (state))  // 释放跟踪自旋锁并恢复中断状态
#else
// 非SMP系统直接关闭中断进行临界区保护
#define TRACE_LOCK(state)		    (state) = LOS_IntLock()  // 关闭中断并保存中断状态
#define TRACE_UNLOCK(state)     	    LOS_IntRestore(state)  // 恢复中断状态
#endif

// 跟踪数据转储钩子函数类型定义：用于自定义数据输出方式
typedef VOID (*TRACE_DUMP_HOOK)(BOOL toClient);
extern TRACE_DUMP_HOOK g_traceDumpHook;  // 全局跟踪转储钩子函数指针

// 跟踪命令枚举：定义客户端与内核通信的命令码
enum TraceCmd {
    TRACE_CMD_START = 1,          // 启动跟踪命令
    TRACE_CMD_STOP,               // 停止跟踪命令
    TRACE_CMD_SET_EVENT_MASK,     // 设置事件掩码命令
    TRACE_CMD_RECODE_DUMP,        // 转储记录命令
    TRACE_CMD_MAX_CODE,           // 命令码上限（无效命令）
};

/**
 * @ingroup los_trace
 * 存储来自跟踪客户端的命令结构
 */
typedef struct {
    UINT8 cmd;       // 命令码，对应TraceCmd枚举
    UINT8 param1;    // 参数1
    UINT8 param2;    // 参数2
    UINT8 param3;    // 参数3
    UINT8 param4;    // 参数4
    UINT8 param5;    // 参数5
    UINT8 end;       // 命令结束标志，应等于TRACE_CMD_END_CHAR
} TraceClientCmd;

/**
 * @ingroup los_trace
 * 存储事件通知信息的结构
 */
typedef struct {
    UINT32 cmd;     // 命令码（跟踪启动/停止等控制命令）
    UINT32 param;   // 参数（通常为魔术数，用于标识通知消息）
} TraceNotifyFrame;

/**
 * @ingroup los_trace
 * 存储离线跟踪配置信息的结构
 */
typedef struct {
    struct WriteCtrl {
        UINT16 curIndex;            // 当前记录索引
        UINT16 maxRecordCount;      // 最大跟踪项数量
        UINT16 curObjIndex;         // 当前对象索引
        UINT16 maxObjCount;         // 最大对象数量
        ObjData *objBuf;            // 对象信息数据缓冲区指针
        TraceEventFrame *frameBuf;  // 跟踪事件帧缓冲区指针
    } ctrl;  // 写控制结构体
    OfflineHead *head;  // 离线跟踪头部信息指针
} TraceOfflineHeaderInfo;

extern UINT32 OsTraceGetMaskTid(UINT32 taskId);
extern VOID OsTraceSetObj(ObjData *obj, const LosTaskCB *tcb);
extern VOID OsTraceWriteOrSendEvent(const TraceEventFrame *frame);
extern VOID OsTraceObjAdd(UINT32 eventType, UINT32 taskId);
extern BOOL OsTraceIsEnable(VOID);
extern OfflineHead *OsTraceRecordGet(VOID);

#ifdef LOSCFG_RECORDER_MODE_ONLINE
extern VOID OsTraceSendHead(VOID);
extern VOID OsTraceSendObjTable(VOID);
extern VOID OsTraceSendNotify(UINT32 type, UINT32 value);

#define OsTraceNotifyStart() do {                                \
        OsTraceSendNotify(SYS_START, TRACE_CTL_MAGIC_NUM);       \
        OsTraceSendHead();                                       \
        OsTraceSendObjTable();                                   \
    } while (0)

#define OsTraceNotifyStop() do {                                 \
        OsTraceSendNotify(SYS_STOP, TRACE_CTL_MAGIC_NUM);        \
    } while (0)

#define OsTraceReset()
#define OsTraceRecordDump(toClient)
#else
extern UINT32 OsTraceBufInit(UINT32 size);
extern VOID OsTraceReset(VOID);
extern VOID OsTraceRecordDump(BOOL toClient);
#define OsTraceNotifyStart()
#define OsTraceNotifyStop()
#endif

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_TRACE_PRI_H */
