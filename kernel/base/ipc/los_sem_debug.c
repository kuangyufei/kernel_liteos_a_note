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

#include "los_sem_debug_pri.h"
#include "stdlib.h"
#include "los_typedef.h"
#include "los_task_pri.h"
#include "los_ipcdebug_pri.h"
#ifdef LOSCFG_SHELL
#include "shcmd.h"
#endif /* LOSCFG_SHELL */


#define OS_ALL_SEM_MASK 0xffffffff

#if defined(LOSCFG_DEBUG_SEMAPHORE) || defined(LOSCFG_SHELL_CMD_DEBUG)
STATIC VOID OsSemPendedTaskNamePrint(LosSemCB *semNode)
{
    LosTaskCB *tskCB = NULL;
    CHAR *nameArr[LOSCFG_BASE_CORE_TSK_LIMIT] = {0};
    UINT32 i, intSave;
    UINT32 num = 0;

    SCHEDULER_LOCK(intSave);
    if ((semNode->semStat == OS_SEM_UNUSED) || (LOS_ListEmpty(&semNode->semList))) {
        SCHEDULER_UNLOCK(intSave);
        return;
    }

    LOS_DL_LIST_FOR_EACH_ENTRY(tskCB, &semNode->semList, LosTaskCB, pendList) {
        nameArr[num++] = tskCB->taskName;
        if (num == LOSCFG_BASE_CORE_TSK_LIMIT) {
            break;
        }
    }
    SCHEDULER_UNLOCK(intSave);

    PRINTK("Pended task list : ");
    for (i = 0; i < num; i++) {
        if (i == 0) {
            PRINTK("\n%s", nameArr[i]);
        } else {
            PRINTK(", %s", nameArr[i]);
        }
    }
    PRINTK("\n");
}
#endif

#ifdef LOSCFG_DEBUG_SEMAPHORE

typedef struct {
    UINT16  origSemCount;   /* Number of orignal available semaphores *///原始可用信号量数
    UINT64  lastAccessTime; /* The last operation time */				//最后操作时间
    TSK_ENTRY_FUNC creator; /* The task entry who created this sem */	//由哪个task的入口函数创建了这个任务
} SemDebugCB;
STATIC SemDebugCB *g_semDebugArray = NULL;//默认1024个SemDebugCB debug信号量池

STATIC BOOL SemCompareValue(const IpcSortParam *sortParam, UINT32 left, UINT32 right)
{
    return (*((UINT64 *)(VOID *)SORT_ELEM_ADDR(sortParam, left)) >
            *((UINT64 *)(VOID *)SORT_ELEM_ADDR(sortParam, right)));
}

UINT32 OsSemDbgInit(VOID)
{
    UINT32 size = LOSCFG_BASE_IPC_SEM_LIMIT * sizeof(SemDebugCB);
    /* system resident memory, don't free */
    g_semDebugArray = (SemDebugCB *)LOS_MemAlloc(m_aucSysMem1, size);
    if (g_semDebugArray == NULL) {
        PRINT_ERR("%s: malloc failed!\n", __FUNCTION__);
        return LOS_NOK;
    }
    (VOID)memset_s(g_semDebugArray, size, 0, size);
    return LOS_OK;
}
///更新最后访问时间
VOID OsSemDbgTimeUpdate(UINT32 semID)
{
    SemDebugCB *semDebug = &g_semDebugArray[GET_SEM_INDEX(semID)];
    semDebug->lastAccessTime = LOS_TickCountGet();//获取tick总数
    return;
}
///更新信号量
VOID OsSemDbgUpdate(UINT32 semID, TSK_ENTRY_FUNC creator, UINT16 count)
{
    SemDebugCB *semDebug = &g_semDebugArray[GET_SEM_INDEX(semID)];
    semDebug->creator = creator;	//改为由参数入口函数创建了这个任务
    semDebug->lastAccessTime = LOS_TickCountGet();//获取tick总数
    semDebug->origSemCount = count;//原始信号量改变
    return;
}
///按信号量访问时间排序
STATIC VOID OsSemSort(UINT32 *semIndexArray, UINT32 usedCount)
{
    UINT32 i, intSave;
    LosSemCB *semCB = NULL;
    LosSemCB semNode = {0};
    SemDebugCB semDebug = {0};
    IpcSortParam semSortParam;
    semSortParam.buf = (CHAR *)g_semDebugArray;
    semSortParam.ipcDebugCBSize = sizeof(SemDebugCB);
    semSortParam.ipcDebugCBCnt = LOSCFG_BASE_IPC_SEM_LIMIT;
    semSortParam.sortElemOff = LOS_OFF_SET_OF(SemDebugCB, lastAccessTime);

    /* It will Print out ALL the Used Semaphore List. */
    PRINTK("Used Semaphore List: \n");
    PRINTK("\r\n   SemID    Count    OriginalCount   Creator(TaskEntry)    LastAccessTime\n");
    PRINTK("   ------   ------   -------------   ------------------    --------------   \n");

    SCHEDULER_LOCK(intSave);
    OsArraySortByTime(semIndexArray, 0, usedCount - 1, &semSortParam, SemCompareValue);
    SCHEDULER_UNLOCK(intSave);
    for (i = 0; i < usedCount; i++) {
        semCB = GET_SEM(semIndexArray[i]);
        SCHEDULER_LOCK(intSave);
        (VOID)memcpy_s(&semNode, sizeof(LosSemCB), semCB, sizeof(LosSemCB));
        (VOID)memcpy_s(&semDebug, sizeof(SemDebugCB), &g_semDebugArray[semIndexArray[i]], sizeof(SemDebugCB));
        SCHEDULER_UNLOCK(intSave);
        if ((semNode.semStat != OS_SEM_USED) || (semDebug.creator == NULL)) {
            continue;
        }
        PRINTK("   0x%-07x0x%-07u0x%-14u%-22p0x%llx\n", semNode.semID, semDebug.origSemCount,
               semNode.semCount, semDebug.creator, semDebug.lastAccessTime);
        if (!LOS_ListEmpty(&semNode.semList)) {
            OsSemPendedTaskNamePrint(semCB);
        }
    }
}

/**
 * @brief  获取所有使用中的信号量完整调试信息
 * @details 遍历信号量控制块数组，统计并收集所有活跃信号量的索引信息，
 *          为信号量调试命令提供数据支持
 * @return UINT32 操作结果
 *         - LOS_OK: 成功
 *         - LOS_NOK: 内存分配失败
 */
UINT32 OsSemInfoGetFullData(VOID)
{
    UINT32 usedSemCnt = 0;          /* 使用中的信号量计数 */
    LosSemCB *semNode = NULL;       /* 信号量控制块指针 */
    SemDebugCB *semDebug = NULL;    /* 信号量调试信息结构体指针 */
    UINT32 i;                       /* 循环索引 */
    UINT32 *semIndexArray = NULL;   /* 信号量索引数组指针 */
    UINT32 count, intSave;          /* count: 数组填充计数; intSave: 中断状态保存值 */

    SCHEDULER_LOCK(intSave);        /* 关闭调度器，防止并发访问冲突 */
    /* 获取使用中的信号量数量 */
    for (i = 0; i < LOSCFG_BASE_IPC_SEM_LIMIT; i++) {
        semNode = GET_SEM(i);       /* 获取索引为i的信号量控制块 */
        semDebug = &g_semDebugArray[i]; /* 获取对应调试信息结构体 */
        /* 信号量处于使用状态且创建者信息有效 */
        if ((semNode->semStat == OS_SEM_USED) && (semDebug->creator != NULL)) {
            usedSemCnt++;           /* 增加使用计数 */
        }
    }
    SCHEDULER_UNLOCK(intSave);      /* 恢复调度器 */

    if (usedSemCnt > 0) {           /* 存在使用中的信号量时处理 */
        /* 分配存储信号量索引的数组内存 */
        semIndexArray = (UINT32 *)LOS_MemAlloc((VOID *)OS_SYS_MEM_ADDR, usedSemCnt * sizeof(UINT32));
        if (semIndexArray == NULL) { /* 内存分配失败检查 */
            PRINTK("LOS_MemAlloc failed in %s \n", __func__);
            return LOS_NOK;
        }

        /* 填充信号量索引数组 */
        count = 0;

        SCHEDULER_LOCK(intSave);    /* 关闭调度器，防止并发修改 */
        for (i = 0; i < LOSCFG_BASE_IPC_SEM_LIMIT; i++) {
            semNode = GET_SEM(i);
            semDebug = &g_semDebugArray[i];
            /* 跳过未使用或调试信息无效的信号量 */
            if ((semNode->semStat != OS_SEM_USED) || (semDebug->creator == NULL)) {
                continue;
            }
            *(semIndexArray + count) = i; /* 保存有效信号量索引 */
            count++;
            /* 已收集足够数量的信号量索引，提前退出循环 */
            if (count >= usedSemCnt) {
                break;
            }
        }
        SCHEDULER_UNLOCK(intSave);  /* 恢复调度器 */
        OsSemSort(semIndexArray, count); /* 对信号量索引进行排序 */

        /* 释放索引数组内存 */
        (VOID)LOS_MemFree((VOID *)OS_SYS_MEM_ADDR, semIndexArray);
    }
    return LOS_OK;
}
#endif /* LOSCFG_DEBUG_SEMAPHORE */

#ifdef LOSCFG_SHELL_CMD_DEBUG
/**
 * @brief  输出信号量详细信息
 * @param  semID 信号量ID，OS_ALL_SEM_MASK表示所有信号量
 * @return UINT32 操作结果，始终返回LOS_OK
 */
STATIC UINT32 OsSemInfoOutput(size_t semID)
{
    UINT32 loop, semCnt, intSave;   /* loop:循环变量; semCnt:信号量计数; intSave:中断状态 */
    LosSemCB *semCB = NULL;         /* 信号量控制块指针 */
    LosSemCB semNode = {0};         /* 本地信号量控制块副本，避免临界区过长 */

    if (semID == OS_ALL_SEM_MASK) { /* 处理查询所有信号量的情况 */
        for (loop = 0, semCnt = 0; loop < LOSCFG_BASE_IPC_SEM_LIMIT; loop++) {
            semCB = GET_SEM(loop);
            SCHEDULER_LOCK(intSave);
            if (semCB->semStat == OS_SEM_USED) { /* 信号量处于使用状态 */
                /* 复制信号量信息到本地变量，减少临界区长度 */
                (VOID)memcpy_s(&semNode, sizeof(LosSemCB), semCB, sizeof(LosSemCB));
                SCHEDULER_UNLOCK(intSave);
                semCnt++;
                /* 打印信号量信息表头 */
                PRINTK("\r\n   SemID       Count\n   ----------  -----\n");
                PRINTK("   0x%08x  %u\n", semNode.semID, semNode.semCount);
                continue;
            }
            SCHEDULER_UNLOCK(intSave);
        }
        PRINTK("   SemUsingNum    :  %u\n\n", semCnt); /* 打印信号量总数 */
        return LOS_OK;
    } else {                        /* 处理查询特定信号量的情况 */
        /* 检查信号量ID有效性 */
        if (GET_SEM_INDEX(semID) >= LOSCFG_BASE_IPC_SEM_LIMIT) {
            PRINTK("\nInvalid semaphore id!\n");
            return LOS_OK;
        }

        semCB = GET_SEM(semID);
        SCHEDULER_LOCK(intSave);
        /* 复制信号量信息到本地变量 */
        (VOID)memcpy_s(&semNode, sizeof(LosSemCB), semCB, sizeof(LosSemCB));
        SCHEDULER_UNLOCK(intSave);
        /* 检查信号量是否处于使用状态 */
        if ((semNode.semID != semID) || (semNode.semStat != OS_SEM_USED)) {
            PRINTK("\nThe semaphore is not in use!\n");
            return LOS_OK;
        }

        /* 打印特定信号量详细信息 */
        PRINTK("\r\n   SemID       Count\n   ----------  -----\n");
        PRINTK("   0x%08x      0x%u\n", semNode.semID, semNode.semCount);

        /* 检查是否有任务等待该信号量 */
        if (LOS_ListEmpty(&semNode.semList)) {
            PRINTK("No task is pended on this semaphore!\n");
            return LOS_OK;
        } else {
            OsSemPendedTaskNamePrint(semCB); /* 打印等待任务信息 */
        }
    }
    return LOS_OK;
}

/**
 * @brief  信号量信息查询shell命令实现
 * @param  argc 命令参数个数
 * @param  argv 命令参数数组
 * @return UINT32 操作结果
 *         - LOS_OK: 成功
 *         - OS_ERROR: 参数错误
 */
LITE_OS_SEC_TEXT_MINOR UINT32 OsShellCmdSemInfoGet(UINT32 argc, const CHAR **argv)
{
    size_t semID;                   /* 信号量ID */
    CHAR *endPtr = NULL;            /* strtoul函数的结束指针 */
    UINT32 ret;                     /* 函数返回值 */

    if (argc > 1) {                 /* 参数数量检查 */
#ifdef LOSCFG_DEBUG_SEMAPHORE
        PRINTK("\nUsage: sem [fulldata|ID]\n"); /* 带调试功能的用法提示 */
#else
        PRINTK("\nUsage: sem [ID]\n");          /* 普通用法提示 */
#endif
        return OS_ERROR;
    }

    if (argc == 0) {
        semID = OS_ALL_SEM_MASK;    /* 无参数时查询所有信号量 */
    } else {
#ifdef LOSCFG_DEBUG_SEMAPHORE
        /* 处理fulldata参数，获取完整调试信息 */
        if (strcmp(argv[0], "fulldata") == 0) {
            ret = OsSemInfoGetFullData();
            return ret;
        }
#endif
        /* 将参数转换为信号量ID */
        semID = strtoul(argv[0], &endPtr, 0);
        /* 检查参数有效性 */
        if ((endPtr == NULL) || (*endPtr != 0)) {
            PRINTK("\nsem ID can't access %s.\n", argv[0]);
            return 0;
        }
    }

    ret = OsSemInfoOutput(semID);   /* 输出信号量信息 */
    return ret;
}

SHELLCMD_ENTRY(sem_shellcmd, CMD_TYPE_EX, "sem", 1, (CmdCallBackFunc)OsShellCmdSemInfoGet);//采用shell命令静态注册方式
#endif

