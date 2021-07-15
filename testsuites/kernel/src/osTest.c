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

#include "osTest.h"
#include "securec.h"
#ifdef LOSCFG_KERNEL_CPPSUPPORT
#include "los_cppsupport.h"
#endif
#if defined(TEST3518E) || defined(TEST3516A) || defined(TEST3516EV200)
#include "eth_drv.h"
#endif
#if !defined(LOSCFG_AARCH64) && !defined(TESTISP)
#include "console.h"
#else
#include "los_config.h"
#endif
#include "los_sem_pri.h"
#include "los_mux_pri.h"
#include "los_queue_pri.h"
#include "los_swtmr_pri.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cpluscplus */
#endif /* __cpluscplus */
UINT32 g_shellTestQueueID;

UINT32 g_testTskHandle;
UINT32 volatile g_testCount;
UINT32 g_flowcheck = 0;
UINT32 g_failResult = 0;
UINT32 g_passResult = 0;

#ifdef TESTPBXA9
int snprintf(char *str, unsigned int len, const char *fmt, ...) {}
#endif

#ifdef TEST1980
UINT32 g_testhwiFlag;
UINT32 g_testCpuMask;
#endif

SPIN_LOCK_S g_testSpin;
UINT32 g_testCount1;
UINT32 g_testCount2;
UINT32 g_testCount3;
UINT32 g_testTaskID01;
UINT32 g_testTaskID02;
UINT32 g_testTaskID03;
UINT32 g_testTaskID04;
UINT32 g_hwiNum1;
UINT32 g_hwiNum2;
UINT32 g_semID;
UINT32 g_semID2;
LosMux g_mutexkernelTest;
UINT32 g_cpupTestCount;
UINT32 g_waitTestCount;
UINT32 g_testPeriod;

UINT16 g_swTmrID;
UINT32 g_testQueueID01;
UINT32 g_testQueueID02;
UINT32 g_testQueueID03;
UINT32 g_leavingTaskNum;
UINT32 g_getTickConsume = 0;
EVENT_CB_S g_eventCB;
EVENT_CB_S g_event;
UINT32 g_testCircleCount = 0;
INT32 g_performanceStart = -1;

#define MOUNT_PATH NFS_MOUNT_DIR
UINT32 g_fatFilesystem;
UINT8 g_mIndex;
UINT32 g_semID3[LOSCFG_BASE_IPC_SEM_CONFIG + 1];
LOS_MEM_POOL_STATUS g_sysMemStatus = { 0 };

#if TEST_MODULE_CHECK == YES

extern UINT32 g_failModelResult[];
extern UINT32 g_passModelResult[];
extern UINT32 g_executModelNum[];
extern ICUNIT_CASE_S g_errorCase[];
#endif
extern char *g_strLayer[];
extern char *g_strLevel[];
extern char *g_strType[];

extern char *g_strModule[];
extern UINT32 g_modelNum;

#ifdef LOSCFG_TEST_FS_FAT
#define TEST_FAT32 0x02
#define TEST_EXFAT 0x04
#endif

BOOL g_isSpinorInit = FALSE;
BOOL g_isSdInit = FALSE;
BOOL g_isUartDevInit = FALSE;
BOOL g_isTcpipInit = FALSE;
BOOL g_isInitSerial = FALSE;
UINT32 g_vfsCyclesCount = 0;
INT32 g_serialInitFlag = -1;
BOOL g_isAddArray = TRUE;
BOOL g_isUsbInit = FALSE;
BOOL g_isIpcGmacInit = FALSE;

BOOL g_isDriversRandomInit = FALSE;

BOOL g_isHisiEthSetPhyModeInit = FALSE;

BOOL g_isVfsInit = FALSE;

u_long TRandom(VOID)
{
    u_long t;
    long x, hi, lo;
    UINT64 cpuCycle;
    UINT32 high, low;
    extern VOID LOS_GetCpuCycle(UINT32 * puwCntHi, UINT32 * puwCntLo);
    LOS_GetCpuCycle(&high, &low);
    cpuCycle = (((UINT64)high << 30) + low); // 30, generate a seed.
    x = cpuCycle;
    hi = x / 127773; // 127773, generate a seed.
    lo = x % 127773; // 127773, generate a seed.
    t = 16807 * lo - 2836 * hi; // 16807, 2836, generate a seed.
    if (t <= 0)
        t += 0x7fffffff;
    return (u_long)(t);
}

UINT32 LosMuxCreate(LosMux *mutex)
{
    return LOS_MuxInit(mutex, NULL);
}

UINT32 TaskCountGetTest(VOID)
{
    UINT32 loop;
    UINT32 taskCnt = 0;
    UINT32 intSave;
    LosTaskCB *taskCB = (LosTaskCB *)NULL;

    intSave = LOS_IntLock();
    for (loop = 0; loop < g_taskMaxNum; loop++) {
        taskCB = (((LosTaskCB *)g_taskCBArray) + loop);
        if (taskCB->taskStatus & OS_TASK_STATUS_UNUSED) {
            continue;
        }
        taskCnt++;
    }
    (VOID)LOS_IntRestore(intSave);
    return taskCnt;
}

UINT32 SemCountGetTest(VOID)
{
    UINT32 loop;
    UINT32 semCnt = 0;
    UINT32 intSave;
    LosSemCB *semNode = (LosSemCB *)NULL;

    intSave = LOS_IntLock();
    for (loop = 0; loop < LOSCFG_BASE_IPC_SEM_CONFIG; loop++) {
        semNode = GET_SEM(loop);
        if (semNode->semStat == OS_SEM_USED) {
            semCnt++;
        }
    }
    (VOID)LOS_IntRestore(intSave);
    return semCnt;
}

UINT32 QueueCountGetTest(VOID)
{
    UINT32 loop;
    UINT32 queueCnt = 0;
    UINT32 intSave;
    LosQueueCB *queueCB = (LosQueueCB *)NULL;

    intSave = LOS_IntLock();
    queueCB = g_allQueue;
    for (loop = 0; loop < LOSCFG_BASE_IPC_QUEUE_CONFIG; loop++, queueCB++) {
        if (queueCB->queueState == OS_QUEUE_INUSED) {
            queueCnt++;
        }
    }
    (VOID)LOS_IntRestore(intSave);
    return queueCnt;
}

UINT32 SwtmrCountGetTest(VOID)
{
    UINT32 loop;
    UINT32 swTmrCnt = 0;
    UINT32 intSave;
    SWTMR_CTRL_S *swTmrCB = (SWTMR_CTRL_S *)NULL;

    intSave = LOS_IntLock();
    swTmrCB = g_swtmrCBArray;
    for (loop = 0; loop < LOSCFG_BASE_CORE_SWTMR_CONFIG; loop++, swTmrCB++) {
        if (swTmrCB->ucState != OS_SWTMR_STATUS_UNUSED) {
            swTmrCnt++;
        }
    }
    (VOID)LOS_IntRestore(intSave);
    return swTmrCnt;
}
#ifdef TEST1980
VOID TestHwiTrigger(unsigned int irq)
{
    int i;
    HalIrqSendIpi(g_testCpuMask, irq);
    for (i = 0; i < 0xff; i++) {
    }
}
#else
void TestHwiTrigger(unsigned int irq)
{
    extern void Dsb(void);
    extern void HalIrqPending(unsigned int vector);
    extern VOID HalIrqUnmask(unsigned int vector);

    HalIrqUnmask(irq);
    HalIrqPending(irq);
    Dsb();
    Dsb();
    Dsb();
}
#endif

VOID TestExtraTaskDelay(UINT32 tick)
{
#ifdef LOSCFG_KERNEL_SMP
    // trigger task schedule may occor on another core
    // needs adding delay and checking status later
    LOS_TaskDelay(tick);
#else
    // do nothing
#endif
}

UINT64 TestTickCountGet(VOID)
{
    /* not use LOS_TickCountGet for now,
       cause every timer is not match with others.
       use cpu0 timer instead. */
    return LOS_TickCountGet();
}

UINT64 TestTickCountByCurrCpuid(VOID)
{
    return LOS_TickCountGet();
}

/*
 * different from calling LOS_TaskDelay,
 * this func will not yeild this task to another one.
 */
VOID TestBusyTaskDelay(UINT32 tick)
{
    UINT64 runtime;

    runtime = TestTickCountByCurrCpuid() + tick;
    while (1) {
        if (runtime <= TestTickCountByCurrCpuid()) {
            break;
        }
        WFI;
    }
}

VOID TestAssertBusyTaskDelay(UINT32 timeout, UINT32 flag)
{
    UINT64 runtime;

    runtime = TestTickCountGet() + timeout;
    while (1) {
        if ((runtime <= TestTickCountGet()) || (g_testCount == flag)) {
            break;
        }
        WFI;
    }
}

VOID TestTestHwiDelete(unsigned int irq, VOID *devId)
{
    HwiIrqParam stuwIrqPara;

    if (OS_INT_ACTIVE)
        return;

    stuwIrqPara.swIrq = irq;
    stuwIrqPara.pDevId = devId;

    (VOID)LOS_HwiDelete(irq, &stuwIrqPara);
    return;
}

VOID TestDumpCpuid(VOID)
{
#ifdef LOSCFG_KERNEL_SMP
    PRINT_DEBUG("%d,cpuid = %d\n", LOS_CurTaskIDGet(), ArchCurrCpuid());
#endif
    return;
}

#if defined(LOSCFG_COMPAT_POSIX)
UINT32 PosixPthreadInit(pthread_attr_t *attr, int pri)
{
    UINT32 ret;
    struct sched_param sp;

    ret = pthread_attr_init(attr);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, NOK);

    attr->inheritsched = PTHREAD_EXPLICIT_SCHED;

    sp.sched_priority = pri;
    ret = pthread_attr_setschedparam(attr, &sp);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, NOK);

    ret = pthread_attr_setschedpolicy(attr, SCHED_RR);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, NOK);

    return LOS_OK;
NOK:
    return LOS_NOK;
}

UINT32 PosixPthreadDestroy(pthread_attr_t *attr, pthread_t thread)
{
    UINT32 ret;

    ret = pthread_join(thread, NULL);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, NOK);

    ret = pthread_attr_destroy(attr);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, NOK);

    return LOS_OK;
NOK:
    return LOS_NOK;
}
#endif


VOID TestKernelBaseCore(VOID)
{
#if defined(LOSCFG_TEST_KERNEL_BASE_CORE)
    ItSuiteLosTask();
    ItSuiteLosSwtmr();
    ItSuiteSmpHwi();
    ItSuiteHwiNesting();
#endif
}

VOID TestKernelBaseIpc(VOID)
{
#if defined(LOSCFG_TEST_KERNEL_BASE_IPC)
    ItSuiteLosEvent();
    ItSuiteLosMux();
    ItSuiteLosSem();
    ItSuiteLosQueue();
#endif
}

VOID TestKernelBase(VOID)
{

#if defined(LOSCFG_TEST_KERNEL_BASE)
    TestKernelBaseCore();
    TestKernelBaseIpc();
#endif
}

VOID TestPosixMutex(VOID)
{
#if defined(LOSCFG_TEST_POSIX_MUTEX)
    ItSuitePosixMutex();
#endif
}

VOID TestPosixPthread(VOID)
{
#if defined(LOSCFG_TEST_POSIX_PTHREAD)
    ItSuitePosixPthread();
#endif
}

VOID TestPosix(VOID)
{
#if defined(LOSCFG_TEST_POSIX)
    TestPosixMutex();
    TestPosixPthread();
#endif
}

VOID TestKernelExtendCpup(VOID)
{
#if defined(LOSCFG_TEST_KERNEL_EXTEND_CPUP)
    ItSuiteExtendCpup();
#endif
}

VOID TestKernelExtend(VOID)
{
#if defined(LOSCFG_TEST_KERNEL_EXTEND)
    TestKernelExtendCpup();
#endif
}

VOID TestReset(VOID)
{
#if defined(TEST3559A) || defined(TEST3559A_M7) || defined(TEST3516EV200) || defined(LOSCFG_LLTREPORT) || \
    defined(LITEOS_3RDPARTY_THTTPD_TEST) || defined(TESTISP)
#else
#ifdef LOSCFG_SHELL
    extern VOID cmd_reset(VOID);
    cmd_reset();
#endif
#endif
}

VOID TestTaskEntry(UINT32 param1, UINT32 param2, UINT32 param3, UINT32 param4)
{
    UINT32 i;

    INT32 ret = LOS_SetProcessScheduler(LOS_GetCurrProcessID(), LOS_SCHED_RR, 20); // 20, set a reasonable priority.
    if (ret != LOS_OK) {
        dprintf("%s set test process schedule failed! %d\n", ret);
    }

    ret = LOS_SetTaskScheduler(LOS_CurTaskIDGet(), LOS_SCHED_RR, TASK_PRIO_TEST);
    if (ret != LOS_OK) {
        dprintf("%s set test task schedule failed! %d\n", ret);
    }

    g_testCircleCount = 0;
    dprintf("\t\n --- Test start--- \n");

#if (TEST_LESSER_MEM == YES)
    UINT32 memusedfirst = 0x600000; // 6M for fs or 3M for kernel
    LOS_MEM_POOL_STATUS status = { 0 };
    LOS_MemInfoGet(OS_SYS_MEM_ADDR, &status);
    LOS_MemAlloc(OS_SYS_MEM_ADDR, status.uwTotalFreeSize - memusedfirst);
#endif

#if defined(LOSCFG_TEST)
    for (i = 0; i < 1; i++) {
        g_testCircleCount++;
        ICunitInit();

        TestKernelExtend();
        TestKernelBase();
        TestPosix();

#if (TEST_MODULE_CHECK == YES) && defined(LOSCFG_TEST)
        for (int i = 0; i < g_modelNum - 1; i++) {
            if (g_executModelNum[i] != 0) {
                dprintf("\nExecuted Model: %s, Executed Model_Num: %d ,failed_count: %d , sucess_count :%d",
                    g_strModule[i], g_executModelNum[i], g_failModelResult[i], g_passModelResult[i]);
            }
            for (int j = 0; j < g_failResult && j < 50; j++) { // 50
                if (i == g_errorCase[j].testcase_module) {
                    LOS_Msleep(200); // 200, delay.
                    dprintf(" \n [Failed]-%s-%s-%s-%s-%s-[Errline: %d RetCode:0x%04X%04X]", g_errorCase[j].pcCaseID,
                        g_strLayer[g_errorCase[j].testcase_layer], g_strModule[g_errorCase[j].testcase_module],
                        g_strLevel[g_errorCase[j].testcase_level], g_strType[g_errorCase[j].testcase_type],
                        g_errorCase[j].errLine, (iUINT16)((g_errorCase[j].retCode) >> 16), // 16
                        (iUINT16)(g_errorCase[j].retCode));
                }
            }
        }
        dprintf("\nNot Executed Model: ");
        for (int i = 0; i < g_modelNum - 1; i++) {
            if (g_executModelNum[i] == 0) {
                LOS_Msleep(40); // 40, delay.
                dprintf("%s    ", g_strModule[i]);
            }
        }
#endif
        dprintf("\n\ntest_count: %d,failed count: %d, success count: %d\n", g_testCircleCount, g_failResult,
            g_passResult);
    }
    LOS_Msleep(200); // 200, delay.
#endif
    dprintf("\t\n --- Test End--- \n");
}

void TestSystemInit(void)
{
    INT32 pid;
    LosProcessCB *testProcess = NULL;

    InitRebootHook();

    pid = LOS_Fork(0, "IT_TST_INI", (TSK_ENTRY_FUNC)TestTaskEntry, 0x30000);
    if (pid < 0) {
        return;
    }

    testProcess = OS_PCB_FROM_PID(pid);
    g_testTskHandle = testProcess->threadGroupID;
#ifdef LOSCFG_KERNEL_SMP
    ((LosTaskCB *)OS_TCB_FROM_TID(g_testTskHandle))->cpuAffiMask = CPUID_TO_AFFI_MASK(ArchCurrCpuid());
#endif
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */
