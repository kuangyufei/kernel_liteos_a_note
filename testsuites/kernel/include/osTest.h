/*
 * Copyright (c) 2013-2019 Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020-2021 Huawei Device Co., Ltd. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this list of
 * conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list
 * of conditions and the following disclaimer in the documentation and/or other materials
 * provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors may be used
 * to endorse or promote products derived from this software without specific prior written
 * permission.
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

#ifndef _OSTEST_H
#define _OSTEST_H

#ifndef SWTMR_TEST
#define SWTMR_TEST
#endif

#include "test_menuconfig.h"
#include "stdarg.h"
#include "los_config.h"
#include "iCunit.h"
#include "stdio.h"
#include "stdlib.h"
#include "limits.h"
#include "string.h"
#include "los_base.h"
#include "los_config.h"
#include "los_typedef.h"
#include "los_hwi.h"
#include "los_vm_map.h"
#include "los_task.h"
#include "los_sched_pri.h"
#include "los_task_pri.h"
#include "los_sys_pri.h"
#include "los_sem_pri.h"
#include "los_event.h"
#include "los_memory.h"
#include "los_queue.h"
#include "los_swtmr.h"
#include "los_mux.h"
#include "los_queue_pri.h"
#include "los_atomic.h"
#if !defined(TEST1980) && !defined(TESTISP)
#include "console.h"
#endif

#ifndef LOSCFG_AARCH64
#ifdef LOSCFG_LIB_LIBC
#include "time.h"
#endif
#include "target_config.h"
#endif
#include "los_process_pri.h"
#include "pthread.h"

#ifdef LOSCFG_PLATFORM_HI3516DV300
#define TEST3516DV300
#elif LOSCFG_PLATFORM_HI3518EV300
#define TEST3518EV300
#endif

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cpluscplus */
#endif /* __cpluscplus */

#define TEST_TASK_PARAM_INIT(testTask, task_name, entry, prio)     \
    do {                                                           \
        memset(&testTask, 0, sizeof(TSK_INIT_PARAM_S));            \
        testTask.pfnTaskEntry = (TSK_ENTRY_FUNC)entry;             \
        testTask.uwStackSize = LOS_TASK_MIN_STACK_SIZE;            \
        testTask.pcName = task_name;                               \
        testTask.usTaskPrio = prio;                                \
        testTask.uwResved = LOS_TASK_STATUS_DETACHED;              \
    } while (0);

#if (LOSCFG_KERNEL_SMP == YES)
#define TEST_TASK_PARAM_INIT_AFFI(testTask, task_name, entry, prio, affi) \
    TEST_TASK_PARAM_INIT(testTask, task_name, entry, prio)                \
    testTask.usCpuAffiMask = affi;
#else
#define TEST_TASK_PARAM_INIT_AFFI(stTestTask, task_name, entry, prio, affi) \
    TEST_TASK_PARAM_INIT(stTestTask, task_name, entry, prio)
#endif
#define JFFS_BASE_MTD_ADDR 0x100000
#define JFFS_BASE_MTD_LEN 0x600000


#define TASK_PRIO_TEST 25
#define TASK_PRIO_TEST_TASK 4
#define TASK_PRIO_TEST_SWTMR 4
#ifdef LOSCFG_AARCH64
#define TASK_STACK_SIZE_TEST (LOS_TASK_MIN_STACK_SIZE * 3)
#else
#define TASK_STACK_SIZE_TEST LOS_TASK_MIN_STACK_SIZE
#endif
#define LOS_MS_PER_TICK (OS_SYS_MS_PER_SECOND / LOSCFG_BASE_CORE_TICK_PER_SECOND)

#define HWI_NUM_INTVALID OS_HWI_MAX_NUM
#define writel(g_value, address) WRITE_UINT32(g_value, address)
#ifdef TESTPBXA9
extern int vsnprintf(char *str, size_t n, const char *fmt, va_list ap);
#endif

#if defined(LOSCFG_TEST_POSIX)
extern UINT32 PosixPthreadInit(pthread_attr_t *attr, int pri);
extern UINT32 PosixPthreadDestroy(pthread_attr_t *attr, pthread_t thread);
#endif

extern UINT32 TaskCountGetTest(VOID);
extern UINT32 SemCountGetTest(VOID);
extern UINT32 QueueCountGetTest(VOID);
extern UINT32 SwtmrCountGetTest(VOID);
extern void hal_interrupt_set_affinity(uint32_t irq, uint32_t cpuMask);

#define TASK_EXISTED_NUM (TaskCountGetTest())
#define QUEUE_EXISTED_NUM (QueueCountGetTest())
#define SWTMR_EXISTED_NUM (SwtmrCountGetTest())
#define SEM_EXISTED_NUM (SemCountGetTest())

extern void TestTestHwiDelete(unsigned int irq, void *devId);
extern void TestHwiTrigger(unsigned int irq);
extern void TestExtraTaskDelay(UINT32 tick);
extern UINT64 TestTickCountGet(void);
extern UINT64 TestTickCountByCurrCpuid(void);
extern void TestBusyTaskDelay(UINT32 tick);
extern void *malloc(size_t size);
extern void TestDumpCpuid(void);
extern u_long TRandom(void);

#define TEST_HwiDelete(ID) TestTestHwiDelete(ID, NULL)
#define TEST_HwiClear(ID) HalIrqMask(ID)
#define TEST_HwiTriggerDelay LOS_TaskDelay(200 * LOSCFG_BASE_CORE_TICK_PER_SECOND / 1000)
#define TEST_HwiCreate(ID, prio, mode, Func, arg) LOS_HwiCreate(ID, prio, mode, Func, arg)


#define HWI_NUM_INT0 0
#define HWI_NUM_INT1 1
#define HWI_NUM_INT2 2
#define HWI_NUM_INT3 3
#define HWI_NUM_INT4 4
#define HWI_NUM_INT5 5
#define HWI_NUM_INT6 6
#define HWI_NUM_INT7 7
#define HWI_NUM_INT11 11
#define HWI_NUM_INT12 12
#define HWI_NUM_INT13 13
#define HWI_NUM_INT14 14
#define HWI_NUM_INT15 15
#define HWI_NUM_INT16 16
#define HWI_NUM_INT17 17
#define HWI_NUM_INT18 18
#define HWI_NUM_INT19 19
#define HWI_NUM_INT21 21
#define HWI_NUM_INT22 22
#define HWI_NUM_INT23 23
#define HWI_NUM_INT24 24
#define HWI_NUM_INT25 25
#define HWI_NUM_INT26 26
#define HWI_NUM_INT27 27
#define HWI_NUM_INT28 28
#define HWI_NUM_INT30 30
#define HWI_NUM_INT31 31
#define HWI_NUM_INT32 32
#define HWI_NUM_INT33 33
#define HWI_NUM_INT34 34
#define HWI_NUM_INT35 35
#define HWI_NUM_INT42 42
#define HWI_NUM_INT45 45
#define HWI_NUM_INT46 46
#define HWI_NUM_INT50 50
#define HWI_NUM_INT55 55
#define HWI_NUM_INT56 56
#define HWI_NUM_INT57 57
#define HWI_NUM_INT58 58
#define HWI_NUM_INT59 59
#define HWI_NUM_INT60 60
#define HWI_NUM_INT61 61
#define HWI_NUM_INT63 63
#define HWI_NUM_INT62 62
#define HWI_NUM_INT68 68
#define HWI_NUM_INT69 69

#define HWI_NUM_INT95 95
#define HWI_NUM_INT114 114
#define HWI_NUM_INT169 169

#if defined TESTPBXA9
#define HWI_NUM_TEST HWI_NUM_INT56
#define HWI_NUM_TEST1 HWI_NUM_INT57
#define HWI_NUM_TEST0 HWI_NUM_INT58
#define HWI_NUM_TEST2 HWI_NUM_INT59
#define HWI_NUM_TEST3 HWI_NUM_INT60
#elif defined TEST3518EV300
#define HWI_NUM_TEST0 HWI_NUM_INT58
#define HWI_NUM_TEST HWI_NUM_INT59
#define HWI_NUM_TEST1 HWI_NUM_INT60
#define HWI_NUM_TEST2 HWI_NUM_INT61
#define HWI_NUM_TEST3 HWI_NUM_INT68
#elif defined TEST3516DV300
#define HWI_NUM_TEST HWI_NUM_INT56
#define HWI_NUM_TEST1 HWI_NUM_INT57
#define HWI_NUM_TEST0 HWI_NUM_INT58
#define HWI_NUM_TEST2 HWI_NUM_INT59
#define HWI_NUM_TEST3 HWI_NUM_INT60
#endif

#define TEST_TASKDELAY_1TICK 1
#define TEST_TASKDELAY_2TICK 2
#define TEST_TASKDELAY_4TICK 4
#define TEST_TASKDELAY_10TICK 10
#define TEST_TASKDELAY_20TICK 20
#define TEST_TASKDELAY_50TICK 50

#ifdef TEST3731
#define TestTimer2ValueGet(temp) READ_UINT32(temp, TIMER1_REG_BASE + TIMER_VALUE)
#elif defined TEST3559
#define TestTimer2ValueGet(temp) READ_UINT32(temp, TIMER3_REG_BASE + TIMER_VALUE)
#else
#define TestTimer2ValueGet(temp) READ_UINT32(temp, TIMER2_REG_BASE + TIMER_VALUE)
#endif

#define REALTIME(time) (UINT32)((UINT64)(0xffffffff - time) * 1000 / OS_SYS_CLOCK)           /* accuracy:ms */
#define HW_TMI(time) (UINT32)((UINT64)(0xffffffff - time) * 1000 / (OS_SYS_CLOCK / 1000000)) /* accuracy:ns */

#define uart_printf_func dprintf

#ifndef VFS_STAT_PRINTF
#define VFS_STAT_PRINTF 0
#endif

#ifndef VFS_STATFS_PRINTF
#define VFS_STATFS_PRINTF 0
#endif

#define OPEN_FILE_MAX 20

#define HUAWEI_ENV_NFS 0

#ifndef TEST_RESOURCELEAK_CHECK
#define TEST_RESOURCELEAK_CHECK YES
#endif

#ifndef TEST_MODULE_CHECK
#define TEST_MODULE_CHECK YES
#endif

#define OS_PROCESS_STATUS_PEND OS_PROCESS_STATUS_PENDING
#define OS_TASK_STATUS_SUSPEND OS_TASK_STATUS_SUSPENDED
#define OS_TASK_STATUS_PEND OS_TASK_STATUS_PENDING

extern UINT32 volatile g_testCount;
extern UINT32 g_testCount1;
extern UINT32 g_testCount2;
extern UINT32 g_testCount3;
extern UINT32 g_flowcheck;
extern UINT32 g_failResult;
extern UINT32 g_passResult;
extern UINT32 g_testTskHandle;
extern UINT32 g_testTaskID01;
extern UINT32 g_testTaskID02;
extern UINT32 g_testTaskID03;
extern UINT32 g_testTaskID04;
extern UINT32 g_hwiNum1;
extern UINT32 g_hwiNum2;
extern UINT32 g_semID;
extern UINT32 g_semID2;
extern LosMux g_mutexkernelTest;
extern UINT32 g_cpupTestCount;
extern UINT16 g_swTmrID;
extern UINT32 g_semID;
extern UINT32 g_testQueueID01;
extern UINT32 g_testQueueID02;
extern UINT32 g_testQueueID03;
extern UINT32 g_testTskHandle;
extern UINT32 g_leavingTaskNum;
extern UINT32 g_semID3[];
extern EVENT_CB_S g_eventCB;
extern EVENT_CB_S g_event;
extern UINT32 g_testPeriod;
extern BOOL g_isAddArray;
extern BOOL g_isSdInit;
extern BOOL g_isSpinorInit;
extern UINT32 g_getTickConsume;
extern UINT32 g_waitTestCount;
extern INT32 g_libFileSystem;
extern UINT32 LosMuxCreate(LosMux *mutex);
extern INT32 g_performanceStart;

extern void msleep(unsigned int msecs);
extern unsigned int sleep(unsigned int seconds);
extern int usleep(unsigned useconds);

#define OS_TASK_STATUS_DETACHED OS_TASK_FLAG_DETACHED
extern UINT32 LOS_MemTotalUsedGet(VOID *pool);
extern VOID ptestTickConsume(VOID);
extern UINT32 TEST_TskDelete(UINT32 taskID);
extern UINT32 TEST_SemDelete(UINT32 semHandle);

extern VOID ItSuiteLosQueue(VOID);
extern VOID ItSuiteLosSwtmr(VOID);
extern VOID ItSuiteLosTask(VOID);
extern VOID ItSuiteLosEvent(VOID);

extern VOID ItSuiteLosMux(VOID);
extern VOID ItSuiteLosRwlock(VOID);
extern VOID ItSuiteLosSem(VOID);
extern VOID ItSuiteSmpHwi(VOID);
extern VOID ItSuiteHwiNesting(VOID);

extern VOID ItSuiteExtendCpup(VOID);

extern VOID ItSuitePosixMutex(VOID);
extern VOID ItSuitePosixPthread(VOID);

extern VOID TestRunShell(VOID);

extern UINT32 OsTestInit(VOID);

extern void TEST_DT_COMMON(void);
extern VOID dprintf(const char *fmt, ...);

#define BIG_FD 512
typedef struct testrunParam {
    CHAR testcase_sequence[16];
    CHAR testcase_num[16];
    CHAR testcase_layer[32];
    CHAR testcase_module[32];
    CHAR testcase_level[16];
    CHAR testcase_type[16];
    CHAR testcase_id[128];
} TEST_RUN_PARAM;

#define LOSCFG_BASE_CORE_TSK_CONFIG LOSCFG_BASE_CORE_TSK_LIMIT
#define LOSCFG_BASE_IPC_SEM_CONFIG LOSCFG_BASE_IPC_SEM_LIMIT
#define LOSCFG_BASE_IPC_QUEUE_CONFIG LOSCFG_BASE_IPC_QUEUE_LIMIT
#define LOSCFG_BASE_CORE_SWTMR_CONFIG LOSCFG_BASE_CORE_SWTMR_LIMIT

#define HAL_READ_UINT8(addr, data) READ_UINT8(data, addr)
#define HAL_WRITE_UINT8(addr, data) WRITE_UINT8(data, addr)
#define HAL_READ_UINT32(addr, data) READ_UINT32(data, addr)
#define HAL_WRITE_UINT32(addr, data) WRITE_UINT32(data, addr)

extern void InitRebootHook(void);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */

#endif /* _OSTEST_H */
