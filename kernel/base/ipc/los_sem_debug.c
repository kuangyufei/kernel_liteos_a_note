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

/*
 * 信号量调试相关宏定义
 * OS_ALL_SEM_MASK: 所有信号量掩码，用于表示需要操作或查询所有信号量
 */
#define OS_ALL_SEM_MASK 0xffffffff

/*
 * 根据配置项LOSCFG_DEBUG_SEMAPHORE或LOSCFG_SHELL_CMD_DEBUG决定是否编译信号量挂起任务名称打印功能
 * 当开启信号量调试或shell命令调试功能时，编译此段代码
 */
#if defined(LOSCFG_DEBUG_SEMAPHORE) || defined(LOSCFG_SHELL_CMD_DEBUG)
/*
 * 功能：打印信号量挂起任务名称列表
 * 参数：
 *   semNode - 信号量控制块指针，指向需要查询的信号量
 * 返回值：无
 * 说明：遍历信号量的挂起任务链表，收集并打印所有挂起任务的名称
 */
STATIC VOID OsSemPendedTaskNamePrint(LosSemCB *semNode)
{
    LosTaskCB *tskCB = NULL;                     /* 任务控制块指针，用于遍历挂起任务 */
    CHAR *nameArr[LOSCFG_BASE_CORE_TSK_LIMIT] = {0}; /* 存储挂起任务名称的数组，大小受最大任务数限制 */
    UINT32 i, intSave;                          /* i:循环变量；intSave:中断状态保存变量 */
    UINT32 num = 0;                             /* 挂起任务数量计数器 */

    SCHEDULER_LOCK(intSave);                    /* 关闭调度器，防止任务调度影响链表遍历 */
    /* 检查信号量状态是否为未使用或挂起任务链表是否为空，是则解锁调度器并返回 */
    if ((semNode->semStat == OS_SEM_UNUSED) || (LOS_ListEmpty(&semNode->semList))) {
        SCHEDULER_UNLOCK(intSave);              /* 恢复调度器运行 */
        return;
    }

    /* 遍历信号量的挂起任务链表，收集任务名称到数组中 */
    LOS_DL_LIST_FOR_EACH_ENTRY(tskCB, &semNode->semList, LosTaskCB, pendList) {
        nameArr[num++] = tskCB->taskName;       /* 获取任务名称并存储到数组 */
        if (num == LOSCFG_BASE_CORE_TSK_LIMIT) {/* 达到最大任务数限制，停止收集 */
            break;
        }
    }
    SCHEDULER_UNLOCK(intSave);                  /* 恢复调度器运行 */

    PRINTK("Pended task list : ");              /* 打印挂起任务列表标题 */
    /* 遍历任务名称数组，打印所有收集到的任务名称 */
    for (i = 0; i < num; i++) {
        if (i == 0) {
            PRINTK("\n%s", nameArr[i]);        /* 第一个任务名称前换行 */
        } else {
            PRINTK(", %s", nameArr[i]);       /* 后续任务名称前加逗号分隔 */
        }
    }
    PRINTK("\n");                              /* 列表结束换行 */
}
#endif

/*
 * 根据配置项LOSCFG_DEBUG_SEMAPHORE决定是否编译信号量调试功能模块
 * 当开启信号量调试功能时，编译此段代码
 */
#ifdef LOSCFG_DEBUG_SEMAPHORE

/*
 * 信号量调试控制块结构体
 * 用于记录信号量的调试相关信息，辅助信号量的调试和问题定位
 */
typedef struct {
    UINT16  origSemCount;   /* 原始可用信号量数量，记录信号量创建时的初始计数值 */
    UINT64  lastAccessTime; /* 最后操作时间，记录信号量最近一次被操作的时间戳 */
    TSK_ENTRY_FUNC creator; /* 创建者任务入口函数，指向创建此信号量的任务入口 */
} SemDebugCB;

STATIC SemDebugCB *g_semDebugArray = NULL;       /* 信号量调试控制块数组，存储所有信号量的调试信息 */

/*
 * 功能：比较两个信号量调试控制块的最后访问时间
 * 参数：
 *   sortParam - 排序参数结构体指针，包含排序相关信息
 *   left - 左侧比较元素索引
 *   right - 右侧比较元素索引
 * 返回值：BOOL类型，若左侧元素时间戳大于右侧则返回TRUE，否则返回FALSE
 * 说明：用于信号量的排序操作，实现按最后访问时间降序排列
 */
STATIC BOOL SemCompareValue(const IpcSortParam *sortParam, UINT32 left, UINT32 right)
{
    /* 获取左右两个元素的最后访问时间并比较大小 */
    return (*((UINT64 *)(VOID *)SORT_ELEM_ADDR(sortParam, left)) >
            *((UINT64 *)(VOID *)SORT_ELEM_ADDR(sortParam, right)));
}

/*
 * 功能：初始化信号量调试控制块数组
 * 参数：无
 * 返回值：UINT32类型，初始化成功返回LOS_OK，否则返回LOS_NOK
 * 说明：分配系统常驻内存用于存储信号量调试信息，初始化数组内存为0
 */
UINT32 OsSemDbgInit(VOID)
{
    UINT32 size = LOSCFG_BASE_IPC_SEM_LIMIT * sizeof(SemDebugCB); /* 计算调试控制块数组所需内存大小 */
    /* system resident memory, don't free - 系统常驻内存，无需释放 */
    /* 从系统内存区域m_aucSysMem1分配内存 */
    g_semDebugArray = (SemDebugCB *)LOS_MemAlloc(m_aucSysMem1, size);
    if (g_semDebugArray == NULL) {                               /* 检查内存分配是否成功 */
        PRINT_ERR("%s: malloc failed!\n", __FUNCTION__);          /* 打印内存分配失败错误信息 */
        return LOS_NOK;                                          /* 返回初始化失败 */
    }
    (VOID)memset_s(g_semDebugArray, size, 0, size);              /* 将调试控制块数组内存初始化为0 */
    return LOS_OK;                                               /* 返回初始化成功 */
}

/*
 * 功能：更新信号量的最后访问时间
 * 参数：
 *   semID - 信号量ID
 * 返回值：无
 * 说明：根据信号量ID获取对应的调试控制块，更新其最后访问时间为当前系统滴答计数
 */
VOID OsSemDbgTimeUpdate(UINT32 semID)
{
    SemDebugCB *semDebug = &g_semDebugArray[GET_SEM_INDEX(semID)]; /* 根据信号量ID获取调试控制块指针 */
    semDebug->lastAccessTime = LOS_TickCountGet();                 /* 更新最后访问时间为当前滴答计数 */
    return;
}

/*
 * 功能：更新信号量的调试信息
 * 参数：
 *   semID - 信号量ID
 *   creator - 创建者任务入口函数
 *   count - 信号量初始计数值
 * 返回值：无
 * 说明：初始化或更新信号量调试控制块的创建者、最后访问时间和原始计数值信息
 */
VOID OsSemDbgUpdate(UINT32 semID, TSK_ENTRY_FUNC creator, UINT16 count)
{
    SemDebugCB *semDebug = &g_semDebugArray[GET_SEM_INDEX(semID)]; /* 根据信号量ID获取调试控制块指针 */
    semDebug->creator = creator;                                   /* 设置创建者任务入口函数 */
    semDebug->lastAccessTime = LOS_TickCountGet();                 /* 更新最后访问时间为当前滴答计数 */
    semDebug->origSemCount = count;                                /* 设置原始信号量计数值 */
    return;
}

/*
 * 功能：对信号量进行排序并打印使用列表
 * 参数：
 *   semIndexArray - 信号量索引数组
 *   usedCount - 使用中的信号量数量
 * 返回值：无
 * 说明：按最后访问时间对信号量进行排序，打印信号量ID、计数值、创建者和最后访问时间等信息，并列出挂起任务
 */
STATIC VOID OsSemSort(UINT32 *semIndexArray, UINT32 usedCount)
{
    UINT32 i, intSave;                          /* i:循环变量；intSave:中断状态保存变量 */
    LosSemCB *semCB = NULL;                     /* 信号量控制块指针 */
    LosSemCB semNode = {0};                     /* 临时信号量控制块，用于安全拷贝 */
    SemDebugCB semDebug = {0};                  /* 临时信号量调试控制块，用于安全拷贝 */
    IpcSortParam semSortParam;                  /* 排序参数结构体 */
    /* 初始化排序参数：调试数组地址、控制块大小、数量及排序元素偏移量 */
    semSortParam.buf = (CHAR *)g_semDebugArray;
    semSortParam.ipcDebugCBSize = sizeof(SemDebugCB);
    semSortParam.ipcDebugCBCnt = LOSCFG_BASE_IPC_SEM_LIMIT;
    semSortParam.sortElemOff = LOS_OFF_SET_OF(SemDebugCB, lastAccessTime);

    /* It will Print out ALL the Used Semaphore List. - 打印所有使用中的信号量列表 */
    PRINTK("Used Semaphore List: \n");          /* 打印列表标题 */
    /* 打印表头：信号量ID、原始计数值、当前计数值、创建者任务入口、最后访问时间 */
    PRINTK("\r\n   SemID    Count    OriginalCount   Creator(TaskEntry)    LastAccessTime\n");
    PRINTK("   ------   ------   -------------   ------------------    --------------   \n");

    SCHEDULER_LOCK(intSave);                    /* 关闭调度器，防止排序过程中数据被修改 */
    /* 调用数组排序函数，按最后访问时间降序排列 */
    OsArraySortByTime(semIndexArray, 0, usedCount - 1, &semSortParam, SemCompareValue);
    SCHEDULER_UNLOCK(intSave);                  /* 恢复调度器运行 */

    /* 遍历排序后的信号量索引数组，打印每个信号量信息 */
    for (i = 0; i < usedCount; i++) {
        semCB = GET_SEM(semIndexArray[i]);      /* 根据索引获取信号量控制块指针 */
        SCHEDULER_LOCK(intSave);                /* 关闭调度器，安全拷贝数据 */
        (VOID)memcpy_s(&semNode, sizeof(LosSemCB), semCB, sizeof(LosSemCB)); /* 拷贝信号量控制块数据 */
        /* 拷贝信号量调试控制块数据 */
        (VOID)memcpy_s(&semDebug, sizeof(SemDebugCB), &g_semDebugArray[semIndexArray[i]], sizeof(SemDebugCB));
        SCHEDULER_UNLOCK(intSave);              /* 恢复调度器运行 */

        /* 过滤掉未使用或创建者为空的信号量 */
        if ((semNode.semStat != OS_SEM_USED) || (semDebug.creator == NULL)) {
            continue;
        }

        /* 打印信号量详细信息：ID、原始计数值、当前计数值、创建者入口、最后访问时间 */
        PRINTK("   0x%-07x0x%-07u0x%-14u%-22p0x%llx\n", semNode.semID, semDebug.origSemCount,
               semNode.semCount, semDebug.creator, semDebug.lastAccessTime);
        /* 如果信号量有挂起任务，则打印挂起任务列表 */
        if (!LOS_ListEmpty(&semNode.semList)) {
            OsSemPendedTaskNamePrint(semCB);    /* 调用挂起任务名称打印函数 */
        }
    }
}

/*
 * 功能：获取所有使用中的信号量完整调试信息并排序输出
 * 参数：无
 * 返回值：UINT32类型，操作成功返回LOS_OK，内存分配失败返回LOS_NOK
 * 说明：遍历所有信号量，统计使用数量，分配索引数组，排序后调用OsSemSort打印详细信息
 */
UINT32 OsSemInfoGetFullData(VOID)
{
    UINT32 usedSemCnt = 0;                      /* 使用中的信号量计数 */
    LosSemCB *semNode = NULL;                   /* 信号量控制块指针，用于遍历信号量数组 */
    SemDebugCB *semDebug = NULL;                /* 信号量调试控制块指针，用于获取调试信息 */
    UINT32 i;                                   /* 循环变量 */
    UINT32 *semIndexArray = NULL;               /* 信号量索引数组，存储使用中信号量的索引 */
    UINT32 count, intSave;                      /* count:索引数组填充计数；intSave:中断状态保存变量 */

    SCHEDULER_LOCK(intSave);                    /* 关闭调度器，防止遍历过程中信号量状态变化 */
    /* Get the used semaphore count. - 统计使用中的信号量数量 */
    for (i = 0; i < LOSCFG_BASE_IPC_SEM_LIMIT; i++) {
        semNode = GET_SEM(i);                   /* 获取当前索引的信号量控制块 */
        semDebug = &g_semDebugArray[i];         /* 获取对应调试控制块 */
        /* 信号量状态为使用中且创建者有效时，计数加1 */
        if ((semNode->semStat == OS_SEM_USED) && (semDebug->creator != NULL)) {
            usedSemCnt++;
        }
    }
    SCHEDULER_UNLOCK(intSave);                  /* 恢复调度器运行 */

    if (usedSemCnt > 0) {                       /* 存在使用中的信号量时处理 */
        /* 分配信号量索引数组内存，大小为使用数量×UINT32 */
        semIndexArray = (UINT32 *)LOS_MemAlloc((VOID *)OS_SYS_MEM_ADDR, usedSemCnt * sizeof(UINT32));
        if (semIndexArray == NULL) {            /* 检查内存分配是否成功 */
            PRINTK("LOS_MemAlloc failed in %s \n", __func__); /* 打印内存分配失败信息 */
            return LOS_NOK;                     /* 返回分配失败 */
        }

        /* Fill the semIndexArray with the real index. - 填充使用中信号量的索引到数组 */
        count = 0;                              /* 索引数组计数器清零 */

        SCHEDULER_LOCK(intSave);                /* 关闭调度器，防止信号量状态变化 */
        for (i = 0; i < LOSCFG_BASE_IPC_SEM_LIMIT; i++) {
            semNode = GET_SEM(i);               /* 获取当前索引的信号量控制块 */
            semDebug = &g_semDebugArray[i];     /* 获取对应调试控制块 */
            /* 跳过未使用或创建者无效的信号量 */
            if ((semNode->semStat != OS_SEM_USED) || (semDebug->creator == NULL)) {
                continue;
            }
            *(semIndexArray + count) = i;       /* 将有效信号量索引存入数组 */
            count++;                            /* 计数器递增 */
            /* if the count is touched usedSemCnt break. - 达到使用数量时退出循环 */
            if (count >= usedSemCnt) {
                break;
            }
        }
        SCHEDULER_UNLOCK(intSave);              /* 恢复调度器运行 */
        OsSemSort(semIndexArray, count);        /* 调用排序函数，按最后访问时间排序并打印 */

        /* free the index array. - 释放索引数组内存 */
        (VOID)LOS_MemFree((VOID *)OS_SYS_MEM_ADDR, semIndexArray);
    }
    return LOS_OK;                              /* 返回操作成功 */
}
#endif /* LOSCFG_DEBUG_SEMAPHORE */             /* 结束LOSCFG_DEBUG_SEMAPHORE条件编译块 */

/*
 * 根据配置项LOSCFG_SHELL_CMD_DEBUG决定是否编译Shell命令调试功能
 * 当开启Shell命令调试功能时，编译此段代码
 */
#ifdef LOSCFG_SHELL_CMD_DEBUG
/*
 * 功能：根据信号量ID输出信号量信息
 * 参数：
 *   semID - 信号量ID，OS_ALL_SEM_MASK表示所有信号量
 * 返回值：UINT32类型，操作成功返回LOS_OK
 * 说明：根据semID输出单个信号量详细信息或所有信号量简要信息，包括挂起任务列表
 */
STATIC UINT32 OsSemInfoOutput(size_t semID)
{
    UINT32 loop, semCnt, intSave;               /* loop:循环变量；semCnt:信号量计数；intSave:中断状态保存变量 */
    LosSemCB *semCB = NULL;                     /* 信号量控制块指针 */
    LosSemCB semNode = {0};                     /* 临时信号量控制块，用于安全拷贝数据 */

    if (semID == OS_ALL_SEM_MASK) {             /* 判断是否需要输出所有信号量信息 */
        for (loop = 0, semCnt = 0; loop < LOSCFG_BASE_IPC_SEM_LIMIT; loop++) {
            semCB = GET_SEM(loop);              /* 获取当前索引的信号量控制块 */
            SCHEDULER_LOCK(intSave);            /* 关闭调度器，安全访问信号量数据 */
            if (semCB->semStat == OS_SEM_USED) {/* 信号量状态为使用中时处理 */
                /* 拷贝信号量控制块数据到临时变量 */
                (VOID)memcpy_s(&semNode, sizeof(LosSemCB), semCB, sizeof(LosSemCB));
                SCHEDULER_UNLOCK(intSave);      /* 恢复调度器运行 */
                semCnt++;                       /* 信号量计数器递增 */
                /* 打印信号量信息表头 */
                PRINTK("\r\n   SemID       Count\n   ----------  -----\n");
                PRINTK("   0x%08x  %u\n", semNode.semID, semNode.semCount); /* 打印信号量ID和计数值 */
                continue;
            }
            SCHEDULER_UNLOCK(intSave);          /* 恢复调度器运行 */
        }
        PRINTK("   SemUsingNum    :  %u\n\n", semCnt); /* 打印使用中的信号量总数 */
        return LOS_OK;
    } else {
        /* 检查信号量索引是否超出最大限制 */
        if (GET_SEM_INDEX(semID) >= LOSCFG_BASE_IPC_SEM_LIMIT) {
            PRINTK("\nInvalid semaphore id!\n"); /* 打印无效信号量ID错误信息 */
            return LOS_OK;
        }

        semCB = GET_SEM(semID);                 /* 根据ID获取信号量控制块 */
        SCHEDULER_LOCK(intSave);                /* 关闭调度器，安全拷贝数据 */
        (VOID)memcpy_s(&semNode, sizeof(LosSemCB), semCB, sizeof(LosSemCB)); /* 拷贝信号量数据 */
        SCHEDULER_UNLOCK(intSave);              /* 恢复调度器运行 */
        /* 检查信号量ID匹配且状态为使用中 */
        if ((semNode.semID != semID) || (semNode.semStat != OS_SEM_USED)) {
            PRINTK("\nThe semaphore is not in use!\n"); /* 打印信号量未使用信息 */
            return LOS_OK;
        }

        /* 打印单个信号量详细信息表头 */
        PRINTK("\r\n   SemID       Count\n   ----------  -----\n");
        PRINTK("   0x%08x      0x%u\n", semNode.semID, semNode.semCount); /* 打印信号量ID和计数值（十六进制） */

        if (LOS_ListEmpty(&semNode.semList)) {  /* 检查信号量挂起任务链表是否为空 */
            PRINTK("No task is pended on this semaphore!\n"); /* 打印无挂起任务信息 */
            return LOS_OK;
        } else {
            OsSemPendedTaskNamePrint(semCB);    /* 调用挂起任务名称打印函数 */
        }
    }
    return LOS_OK;                              /* 返回操作成功 */
}

/*
 * 功能：Shell命令处理函数，用于解析并执行信号量信息查询命令
 * 参数：
 *   argc - 命令参数个数
 *   argv - 命令参数数组
 * 返回值：UINT32类型，命令执行结果，成功返回LOS_OK，参数错误返回OS_ERROR
 * 说明：支持"sem"、"sem ID"和"sem fulldata"（调试模式）三种命令格式，调用相应函数输出信息
 */
LITE_OS_SEC_TEXT_MINOR UINT32 OsShellCmdSemInfoGet(UINT32 argc, const CHAR **argv)
{
    size_t semID;                               /* 信号量ID，用于存储解析后的参数 */
    CHAR *endPtr = NULL;                        /* 字符串转换结束指针，用于验证参数有效性 */
    UINT32 ret;                                 /* 函数返回值临时存储 */

    if (argc > 1) {                             /* 参数个数大于1时，打印命令用法并返回错误 */
#ifdef LOSCFG_DEBUG_SEMAPHORE                   /* 调试模式下支持fulldata参数 */
        PRINTK("\nUsage: sem [fulldata|ID]\n"); /* 打印调试模式命令用法 */
#else
        PRINTK("\nUsage: sem [ID]\n");          /* 非调试模式命令用法 */
#endif
        return OS_ERROR;                        /* 返回参数错误 */
    }

    if (argc == 0) {                            /* 无参数时，查询所有信号量 */
        semID = OS_ALL_SEM_MASK;
    } else {
#ifdef LOSCFG_DEBUG_SEMAPHORE                   /* 调试模式下处理fulldata参数 */
        if (strcmp(argv[0], "fulldata") == 0) { /* 参数为"fulldata"时获取完整调试信息 */
            ret = OsSemInfoGetFullData();
            return ret;
        }
#endif
        /* 将参数转换为信号量ID（支持十进制/十六进制） */
        semID = strtoul(argv[0], &endPtr, 0);
        /* 检查参数转换是否成功（endPtr指向字符串结束且非空指针） */
        if ((endPtr == NULL) || (*endPtr != 0)) {
            PRINTK("\nsem ID can't access %s.\n", argv[0]); /* 打印参数无效错误 */
            return 0;
        }
    }

    ret = OsSemInfoOutput(semID);               /* 调用信息输出函数，根据semID输出信息 */
    return ret;
}

/*
 * Shell命令注册宏 - 将sem命令注册到系统
 * 参数说明：
 *   sem_shellcmd - 命令结构体名称
 *   CMD_TYPE_EX - 命令类型（扩展命令）
 *   "sem" - 命令名称
 *   1 - 命令权限（普通用户）
 *   (CmdCallBackFunc)OsShellCmdSemInfoGet - 命令回调函数
 */
SHELLCMD_ENTRY(sem_shellcmd, CMD_TYPE_EX, "sem", 1, (CmdCallBackFunc)OsShellCmdSemInfoGet);
#endif

