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

#include "iCunit.h"
#include "iCunit.inc"
#include "iCunit_config.h"
#include "osTest.h"
#if TEST_RESOURCELEAK_CHECK == 1
#include "los_swtmr_pri.h"
#include "los_sem_pri.h"
#include "los_queue_pri.h"
#include "los_task_pri.h"
#include "los_mux_pri.h"
#endif
#include <stdio.h>

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cpluscplus */
#endif /* __cpluscplus */

#define ARRAY_SIZE 2

#ifdef LOSCFG_KERNEL_SMP
LITE_OS_SEC_BSS SPIN_LOCK_INIT(g_testSuitSpin);
#endif

extern UINT32 g_failResult;
extern UINT32 g_passResult;

extern int atoi(const char *str);
extern int strncmp(const char *s1, const char *s2, size_t n);

char *g_strLayer[] = {"LOS", "POSIX", "LIB", "VFS", "EXTEND",
                      "PARTITION", "CPP", "SHELL", "LINUX", "USB",
#if defined(LOSCFG_3RDPARTY_TEST)
                      "TEST_3RDPARTY",
#endif
                      "DRIVER", "ALL"
                     };

char *g_strModule[] = {"TASK", "MEM", "SEM", "MUX", "EVENT", "QUE", "SWTMR", "HWI", "MP", "ATO", "CPUP", "SCATTER", "RUNSTOP", "TIMER", "MMU",
                       "ROBIN", "LIBC", "WAIT", "VFAT", "JFFS", "RAMFS", "NFS", "PROC", "FS", "UART",
                       "PTHREAD", "COMP", "HWI_HALFBOTTOM", "WORKQ", "WAKELOCK", "TIMES",
                       "LIBM", "SUPPORT", "STL", "MAIL", "MSG", "CP", "SIGNAL", "SCHED", "MTDCHAR", "TIME", "WRITE", "READ", "DYNLOAD", "REGISTER",
                       "UNAME", "ERR", "CMD", "TICKLESS", "TRACE", "UNALIGNACCESS", "EXC", "REQULATOR", "DEVFREQ", "CPUFREQ", "TEST_MISC",
#if defined(LOSCFG_3RDPARTY_TEST)
                       "THTTPD", "BIDIREFC", "CJSON", "CURL", "FFMPEG", "FREETYPE", "INIPARSER", "JSONCPP", "LIBICONV", "LIBJPEG", "LIBPNG", "OPENEXIF", "OPENSSL",
                       "OPUS", "SQLITE", "TINYXML", "XML2", "ZBAR", "HARFBUZZ",
#endif
                       "TEST_DRIVERBASE", "ALL"
                      };
UINT32 g_modelNum = sizeof(g_strModule) / sizeof(g_strModule[0]);

#if TEST_MODULE_CHECK == 1

UINT32 g_failModelResult[sizeof(g_strModule) / sizeof(g_strModule[0])] = {0};
UINT32 g_passModelResult[sizeof(g_strModule) / sizeof(g_strModule[0])] = {0};
UINT32 g_executModelNum[sizeof(g_strModule) / sizeof(g_strModule[0])] = {0};
ICUNIT_CASE_S g_errorCase[50] = {0};

#endif

char *g_strLevel[] = {"LEVEL0", "LEVEL1", "LEVEL2", "LEVEL3", "LEVEL4", "ALL"};
char *g_strType[] = {"FUNCTION", "PRESSURE", "PERFORMANCE", "ALL"};
char *g_strSequence[] = {"SEQUENCE", "RANDOM"};


u_long ICunitRand(void)
{
    static u_long randseed;
    u_long t;
    long x, hi, lo;
    UINT32 high = 0;
    UINT32 low = 0;

    extern VOID LOS_GetCpuCycle(UINT32 * puwCntHi, UINT32 * puwCntLo);
    LOS_GetCpuCycle(&high, &low);
    randseed = ((u_long)(high << 30) + low); // 30, generate a seed.

    /*
     * Compute x[n + 1] = (7^5 * x[n]) mod (2^31 - 1).
     * From "Random number generators: good ones are hard to find",
     * Park and Miller, Communications of the ACM, vol. 31, no. 10,
     * October 1988, p. 1195.
     */
    x = randseed;
    hi = x / 127773;            // 127773, generate a seed.
    lo = x % 127773;            // 127773, generate a seed.
    t = 16807 * lo - 2836 * hi; // 16807, 2836, generate a seed.
    if (t <= 0)
        t += 0x7fffffff;
    randseed = t;
    return (u_long)(t);
}

void ICunitSaveErr(iiUINT32 line, iiUINT32 retCode)
{
#ifdef LOSCFG_KERNEL_SMP
    UINT32 intSave;
    TESTSUIT_LOCK(intSave);
#endif
    g_iCunitErrCode = ((g_iCunitErrCode == 0) && (g_iCunitErrLineNo == 0)) ? (iiUINT32)retCode : g_iCunitErrCode;
    g_iCunitErrLineNo = (g_iCunitErrLineNo == 0) ? line : g_iCunitErrLineNo;
#ifdef LOSCFG_KERNEL_SMP
    TESTSUIT_UNLOCK(intSave);
#endif
}

#undef LOSCFG_TEST_LEVEL
#define LOSCFG_TEST_LEVEL 0
iUINT32 ICunitAddCase(const iCHAR *caseName, CASE_FUNCTION caseFunc, iUINT16 testcaseLayer, iUINT16 testcaseModule,
    iUINT16 testcaseLevel, iUINT16 testcaseType)
{
    iUINT16 idx;

    if (g_iCunitInitSuccess) {
        return (iUINT32)ICUNIT_UNINIT;
    }

    /* Don't run Firstly with Python */
    if (g_iCunitCaseRun == 0) {
        return (iUINT32)ICUNIT_SUCCESS;
    }

#if defined(LITEOS_TESTSUIT_SHELL) || defined(LOSCFG_TEST_MUTIL)
    idx = g_iCunitCaseCnt;
#else
    idx = 0;
#endif
    if (idx == ICUNIT_CASE_SIZE) {
        g_iCunitErrLogAddCase++;
        return (iUINT32)ICUNIT_CASE_FULL;
    }

    g_iCunitCaseArray[idx].pcCaseID = caseName;
    g_iCunitCaseArray[idx].pstCaseFunc = caseFunc;
    g_iCunitCaseArray[idx].testcase_layer = testcaseLayer;
    g_iCunitCaseArray[idx].testcase_module = testcaseModule;
    g_iCunitCaseArray[idx].testcase_level = testcaseLevel;
    g_iCunitCaseArray[idx].testcase_type = testcaseType;

#if defined(LITEOS_TESTSUIT_SHELL) || defined(LOSCFG_TEST_MUTIL)
    g_iCunitCaseCnt++;
#else
    ICunitRunSingle(&g_iCunitCaseArray[idx]);
#endif

#if defined(LOSCFG_TEST_MUTIL)
    ICunitRunSingle(&g_iCunitCaseArray[idx]);
#endif

    return (iUINT32)ICUNIT_SUCCESS;
}
UINT32 g_cunitInitFlag = 0;
iUINT32 ICunitInit(void)
{
#ifdef LOSCFG_KERNEL_SMP
    if (LOS_AtomicCmpXchg32bits(&g_cunitInitFlag, 1, 0)) {
        PRINTK("other core already done iCunitInit\n");
        return (iUINT32)ICUNIT_SUCCESS;
    }
#endif
    g_iCunitInitSuccess = 0x0000;
    g_iCunitCaseCnt = 0x0000;
    g_iCunitCaseFailedCnt = 0;
    g_iCunitErrLogAddCase = 0;
    memset(g_iCunitCaseArray, 0, sizeof(g_iCunitCaseArray));
    g_iCunitCaseRun = 1;

    return (iUINT32)ICUNIT_SUCCESS;
}

iUINT32 ICunitRunSingle(ICUNIT_CASE_S *psubCase)
{
    if ((g_isSpinorInit == FALSE) && (psubCase->testcase_module == TEST_JFFS))
        dprintf("****** Jffs is not support ! ******  \n");
    else
        ICunitRunF(psubCase);

    return (iUINT32)ICUNIT_SUCCESS;
}

iUINT32 ICunitRunF(ICUNIT_CASE_S *psubCase)
{
    iUINT32 caseRet;
    UINT32 curTestTaskID;
    g_iCunitErrLineNo = 0;
    g_iCunitErrCode = 0;

#if TEST_RESOURCELEAK_CHECK == 1
    extern UINT32 LOS_MemTotalUsedGet(VOID * pPool);
    extern HwiHandleForm g_hwiForm[OS_HWI_MAX_NUM];
    extern SWTMR_CTRL_S *g_swtmrCBArray;
    static SWTMR_CTRL_S *swtmr;
    iUINT32 i;
    static UINT32 gAuwMemuse[ARRAY_SIZE];
    static UINT32 gUwTskNum[ARRAY_SIZE];
    static UINT32 gUwSwtNum[ARRAY_SIZE];
    static UINT32 gUwHwiNum[ARRAY_SIZE];
    static UINT32 gUwQueueNum[ARRAY_SIZE];
    LosQueueCB *queueCB = NULL;
    static UINT32 gUwSemNum[ARRAY_SIZE];
    static LosSemCB *semNode;
    UINT32 muxCnt[ARRAY_SIZE];

    memset(gUwSwtNum, 0, sizeof(gUwSwtNum));
    memset(gUwHwiNum, 0, sizeof(gUwHwiNum));
    memset(gUwTskNum, 0, sizeof(gUwTskNum));
    memset(gAuwMemuse, 0, sizeof(gAuwMemuse));
    memset(gUwQueueNum, 0, sizeof(gUwQueueNum));
    memset(gUwSemNum, 0, sizeof(gUwSemNum));

    curTestTaskID = LOS_CurTaskIDGet();

    // task
    for (i = 0; i <= LOSCFG_BASE_CORE_TSK_LIMIT; i++) {
        if (g_taskCBArray[i].taskStatus == OS_TASK_STATUS_UNUSED)
            gUwTskNum[0]++;
    }

    // swtmr
    swtmr = g_swtmrCBArray;
    for (i = 0; i < LOSCFG_BASE_CORE_SWTMR_LIMIT; i++, swtmr++) {
        if (swtmr->ucState == OS_SWTMR_STATUS_UNUSED)
            gUwSwtNum[0]++;
    }

    // hwi
    for (i = 0; i < OS_HWI_MAX_NUM; i++) {
        if ((g_hwiForm[i].pfnHook == (HWI_PROC_FUNC)NULL) && (g_hwiForm[i].uwParam == 0))
            gUwHwiNum[0]++;
    }

    // sem
    for (i = 0; i < LOSCFG_BASE_IPC_SEM_LIMIT; i++) {
        semNode = GET_SEM(i);
        if (semNode->semStat == OS_SEM_UNUSED) {
            gUwSemNum[0]++;
        }
    }

    // queue
    queueCB = g_allQueue;
    for (i = 0; i < LOSCFG_BASE_IPC_QUEUE_LIMIT; i++, queueCB++) {
        if (queueCB->queueState == OS_QUEUE_UNUSED) {
            gUwQueueNum[0]++;
        }
    }

    gAuwMemuse[0] = LOS_MemTotalUsedGet(OS_SYS_MEM_ADDR);

    if (g_performanceStart <= 0) {
        dprintf("# Enter:%s \n", psubCase->pcCaseID);
    }
    caseRet = psubCase->pstCaseFunc();

    if (g_performanceStart <= 0) {
        if ((strcmp(psubCase->pcCaseID, "LLT_PLAT_004") == 0) ||
            (strcmp(psubCase->pcCaseID, "IT_FS_JFFS_MUTIPTHREAD_001") == 0) ||
            (strcmp(psubCase->pcCaseID, "IT_FS_JFFS_PRESSURE_013") == 0) ||
            (strcmp(psubCase->pcCaseID, "IT_FS_JFFS_PRESSURE_052") == 0) ||
            (strcmp(psubCase->pcCaseID, "IT_FS_JFFS_026") == 0) ||
            (strcmp(psubCase->pcCaseID, "IT_PARTITION_JFFS_004") == 0) ||
            (strcmp(psubCase->pcCaseID, "IT_PARTITION_JFFS_009") == 0) ||
            (strcmp(psubCase->pcCaseID, "IT_PARTITION_JFFS_013") == 0) ||
            (strcmp(psubCase->pcCaseID, "IT_PARTITION_JFFS_017") == 0) ||
            (strcmp(psubCase->pcCaseID, "IT_PARTITION_JFFS_018") == 0) ||
            (strcmp(psubCase->pcCaseID, "IT_PARTITION_JFFS_039") == 0) ||
            (strcmp(psubCase->pcCaseID, "IT_FS_FAT_VIRPART_PRESSURE_001") == 0) ||
            (strcmp(psubCase->pcCaseID, "IT_FS_FAT_VIRPART_151") == 0) ||
            (strcmp(psubCase->pcCaseID, "IT_FS_FAT_VIRPART_040") == 0) ||
            (strcmp(psubCase->pcCaseID, "IT_FS_FAT_VIRPART_MULTIPTHREAD_000") == 0) ||
            (strcmp(psubCase->pcCaseID, "IT_FS_FAT_VIRPART_MULTIPTHREAD_033") == 0) ||
            (strcmp(psubCase->pcCaseID, "IT_FS_FAT_VIRPART_MULTIPTHREAD_034") == 0) ||
            (strcmp(psubCase->pcCaseID, "IT_FS_FATVP_363") == 0) ||
            (strcmp(psubCase->pcCaseID, "IT_FS_FAT_VIRPART_012") == 0) ||
            (strcmp(psubCase->pcCaseID, "IT_PARTITION_DISK_001") == 0) ||
            (strcmp(psubCase->pcCaseID, "IT_FS_NFS_PRESSURE_013") == 0) ||
            (strcmp(psubCase->pcCaseID, "IT_PARTITION_Yaffs_019") == 0) ||
            (strcmp(psubCase->pcCaseID, "IT_PARTITION_Yaffs_017") == 0) ||
            (strcmp(psubCase->pcCaseID, "IT_PARTITION_Yaffs_013") == 0) ||
            (strcmp(psubCase->pcCaseID, "IT_PARTITION_Yaffs_009") == 0) ||
            (strcmp(psubCase->pcCaseID, "LLT_VFS_FAT_002") == 0) ||
            (strcmp(psubCase->pcCaseID, "IT_FS_FAT_363") == 0) || (strcmp(psubCase->pcCaseID, "LLT_FS_VFS_004") == 0) ||
            (strcmp(psubCase->pcCaseID, "LLT_FS_RAMFS_003") == 0) ||
            (strcmp(psubCase->pcCaseID, "LLT_FS_RAMFS_001") == 0)) {
            dprintf("  [Case]-%s-%s-%s-%s-%s,unrunning!!!\n", psubCase->pcCaseID, g_strLayer[psubCase->testcase_layer],
                g_strModule[psubCase->testcase_module], g_strLevel[psubCase->testcase_level],
                g_strType[psubCase->testcase_type]);
            goto ENDING;
        }

        gAuwMemuse[1] = LOS_MemTotalUsedGet(OS_SYS_MEM_ADDR);

        // task
        for (i = 0; i <= LOSCFG_BASE_CORE_TSK_LIMIT; i++) {
            if (g_taskCBArray[i].taskStatus == OS_TASK_STATUS_UNUSED)
                gUwTskNum[1]++;
        }

        // swtmr
        swtmr = g_swtmrCBArray;
        for (i = 0; i < LOSCFG_BASE_CORE_SWTMR_LIMIT; i++, swtmr++) {
            if (swtmr->ucState == OS_SWTMR_STATUS_UNUSED)
                gUwSwtNum[1]++;
        }

        // hwi
        for (i = 0; i < OS_HWI_MAX_NUM; i++) {
            if ((g_hwiForm[i].pfnHook == (HWI_PROC_FUNC)NULL) && (g_hwiForm[i].uwParam == 0))
                gUwHwiNum[1]++;
        }

        // sem
        for (i = 0; i < LOSCFG_BASE_IPC_SEM_LIMIT; i++) {
            semNode = GET_SEM(i);
            if (semNode->semStat == OS_SEM_UNUSED) {
                gUwSemNum[1]++;
            }
        }

        // queue
        queueCB = g_allQueue;
        for (i = 0; i < LOSCFG_BASE_IPC_QUEUE_LIMIT; i++, queueCB++) {
            if (queueCB->queueState == OS_QUEUE_UNUSED) {
                gUwQueueNum[1]++;
            }
        }

        if (gUwSemNum[1] != gUwSemNum[0]) {
            dprintf("\n[Case Resource Leak Failed]------------------Sem used:%d  Semleak:%d\n", gUwSemNum[1],
                (gUwSemNum[1] - gUwSemNum[0]));
        }

        if (gUwQueueNum[1] != gUwQueueNum[0]) {
            dprintf("\n[Case Resource Leak Failed]------------------Queue used:%d  Queueleak:%d\n", gUwQueueNum[1],
                (gUwQueueNum[1] - gUwQueueNum[0]));
        }

        if (gUwTskNum[1] != gUwTskNum[0]) {
            dprintf("\n[Case Resource Leak Failed]------------------Task used:%d  Taskleak:%d\n", gUwTskNum[1],
                (gUwTskNum[1] - gUwTskNum[0]));
        }

        if (gUwSwtNum[1] != gUwSwtNum[0]) {
            dprintf("\n[Case Resource Leak Failed]------------------Swtmr used:%d  Swtmrleak:%d\n", gUwSwtNum[1],
                (gUwSwtNum[1] - gUwSwtNum[0]));
        }

        if (gUwHwiNum[1] != gUwHwiNum[0]) {
            dprintf("\n[Case Resource Leak Failed]------------------Hwi used:%d  Hwileak:%d\n", gUwHwiNum[1],
                (gUwHwiNum[1] - gUwHwiNum[0]));
        }

        if (gAuwMemuse[1] != gAuwMemuse[0]) {
            dprintf("\n[Case Resource Leak Failed]------------------Mem used:%d  Memleak:%d\n", gAuwMemuse[1],
                (gAuwMemuse[1] - gAuwMemuse[0]));
        }
    }

#else
    if (g_performanceStart <= 0) {
        curTestTaskID = LOS_CurTaskIDGet();
        dprintf("# T:%d Enter:%s \n", curTestTaskID, psubCase->pcCaseID);
    }
    caseRet = psubCase->pstCaseFunc();
#endif

    psubCase->errLine = g_iCunitErrLineNo;
    psubCase->retCode = (0 == g_iCunitErrLineNo) ? (caseRet) : (g_iCunitErrCode);

#if TEST_MODULE_CHECK == 1
    g_executModelNum[psubCase->testcase_module]++;
#endif
ENDING:
    if (psubCase->errLine == 0 && caseRet == 0) {
        g_passResult++;

#if TEST_MODULE_CHECK == 1
        g_passModelResult[psubCase->testcase_module]++;
#endif

        if (g_performanceStart <= 0) {
            dprintf(" T:%d [Passed]-%s-%s-%s-%s-%s\n", curTestTaskID, psubCase->pcCaseID,
                g_strLayer[psubCase->testcase_layer], g_strModule[psubCase->testcase_module],
                g_strLevel[psubCase->testcase_level], g_strType[psubCase->testcase_type]);
        }
    } else {
#if TEST_MODULE_CHECK == 1
        if (g_failResult < 50) { // 50
            g_errorCase[g_failResult] = *psubCase;
        }
        g_failModelResult[psubCase->testcase_module]++;
#endif

        g_iCunitCaseFailedCnt++;
        g_failResult++;
        dprintf(" T:%d [Failed]-%s-%s-%s-%s-%s-[Errline: %d RetCode:0x%04X%04X]\n", curTestTaskID, psubCase->pcCaseID,
            g_strLayer[psubCase->testcase_layer], g_strModule[psubCase->testcase_module],
            g_strLevel[psubCase->testcase_level], g_strType[psubCase->testcase_type], psubCase->errLine,
            (iUINT16)((psubCase->retCode) >> 16), (iUINT16)(psubCase->retCode)); // 16

#ifdef LOSCFG_SHELL
        dprintf("\n\n ********************************************************** \n");
        OsShellCmdSystemInfo(0, NULL);
        dprintf("\n ********************************************************** \n");
        const CHAR *taskAll = "-a";
        OsShellCmdDumpTask(1, &taskAll);
        dprintf(" ********************************************************** \n\n\n");
#endif
    }

    return (iUINT32)ICUNIT_SUCCESS;
}

INT32 ICunitRunTestArray(const char *tcSequence, const char *tcNum, const char *tcLayer, const char *tcModule,
    const char *tcLevel, const char *tcType)
{
    iUINT32 testcaseNum;
    iUINT32 testcaseLayer = 0xFFFF;
    iUINT32 testcaseModule = 0xFFFF;
    iUINT32 testcaseLevel = 0xFFFF;
    iUINT32 testcaseType = 0xFFFF;
    iUINT32 ilayer;
    iUINT32 imodule;
    iUINT32 ilevel;
    iUINT32 itype;

    testcaseNum = atoi(tcNum);
    if (testcaseNum > 0xFFFF) {
        dprintf("[testcase_Num]-%u is too large \n", testcaseNum);

        testcaseNum = 0xFFFF;
    }

    for (ilayer = 0; ilayer <= TEST_LAYER_ALL; ilayer++) {
        if (strcmp(g_strLayer[ilayer], tcLayer) == 0) {
            testcaseLayer = ilayer;
            dprintf("[testcase_Layer]-%s \n", g_strLayer[ilayer]);

            break;
        }
    }

    if (testcaseLayer == 0xFFFF) {
        dprintf("[testcase_Layer]-%s is invalid \n", tcLayer);

        return (iUINT32)ICUNIT_SUCCESS;
    }

    for (imodule = 0; imodule <= TEST_MODULE_ALL; imodule++) {
        if (strcmp(g_strModule[imodule], tcModule) == 0) {
            testcaseModule = imodule;
            dprintf("[testcase_Module]-%s \n", g_strModule[imodule]);

            break;
        }
    }

    if (testcaseModule == 0xFFFF) {
        dprintf("[testcase_Module]-%s is invalid \n", tcModule);

        return (iUINT32)ICUNIT_SUCCESS;
    }

    for (ilevel = 0; ilevel <= TEST_LEVEL_ALL; ilevel++) {
        if (strcmp(g_strLevel[ilevel], tcLevel) == 0) {
            testcaseLevel = ilevel;
            dprintf("[testcase_Level]-%s \n", g_strLevel[ilevel]);

            break;
        }
    }

    if (testcaseLevel == 0xFFFF) {
        dprintf("[testcase_Level]-%s is invalid \n", tcLevel);

        return (iUINT32)ICUNIT_SUCCESS;
    }

    for (itype = 0; itype <= TEST_TYPE_ALL; itype++) {
        if (strcmp(g_strType[itype], tcType) == 0) {
            testcaseType = itype;
            dprintf("[testcase_Type]-%s \n", g_strType[itype]);

            break;
        }
    }

    if (testcaseType == 0xFFFF) {
        dprintf("[testcase_Type]-%s is invalid \n", tcType);

        return (iUINT32)ICUNIT_SUCCESS;
    }

    if (0 == strncmp(tcSequence, "SEQUENCE", 8)) { // 8, "SEQUENCE" length
        dprintf("[testcase_Sequence]-%s \n", tcSequence);

        ICunitRunTestArraySequence(testcaseNum, testcaseLayer, testcaseModule, testcaseLevel, testcaseType);
    } else if (0 == strncmp(tcSequence, "RANDOM", 6)) { // 6, "RANDOM" length
        dprintf("[testcase_Random]-%s \n", tcSequence);

        ICunitRunTestArrayRandom(testcaseNum, testcaseLayer, testcaseModule, testcaseLevel, testcaseType);
    } else {
        dprintf("[testcase_Sequence]-%s is invalid \n", tcSequence);
    }

    g_failResult = 0;
    g_passResult = 0;

    return (iUINT32)ICUNIT_SUCCESS;
}

iUINT32 ICunitRunTestArraySequence(iUINT32 testcaseNum, iUINT32 testcaseLayer, iUINT32 testcaseModule,
    iUINT32 testcaseLevel, iUINT32 testcaseType)
{
    iUINT32 num;
    iUINT32 idx;
    iUINT32 idx1;
    iUINT32 failedCount;
    iUINT32 successCount;

    idx1 = g_iCunitCaseCnt;

    for (num = 0; num < testcaseNum; num++) {
        failedCount = g_failResult;
        successCount = g_passResult;

        for (idx = 0; idx < idx1; idx++) {
            ICunitRunTestcaseSatisfied(&(g_iCunitCaseArray[idx]), testcaseLayer, testcaseModule, testcaseLevel,
                testcaseType);
        }
    }

    return (iUINT32)ICUNIT_SUCCESS;
}


iUINT32 ICunitRunTestArrayRandom(iUINT32 testcaseNum, iUINT32 testcaseLayer, iUINT32 testcaseModule,
    iUINT32 testcaseLevel, iUINT32 testcaseType)
{
    iUINT32 num;
    iUINT32 idx;
    iUINT32 idx1;
    iUINT32 randIdx;
    ICUNIT_CASE_S subCaseArrayTemp;
    iUINT32 failedCount;
    iUINT32 successCount;

    memset(&subCaseArrayTemp, 0, sizeof(ICUNIT_CASE_S));

    idx1 = g_iCunitCaseCnt;

    for (num = 0; num < testcaseNum; num++) {
        failedCount = g_failResult;
        successCount = g_passResult;

        for (idx = idx1 - 1; idx > 1; idx--) {
            memset(&subCaseArrayTemp, 0, sizeof(ICUNIT_CASE_S));

            randIdx = ICunitRand() % idx;
            subCaseArrayTemp = g_iCunitCaseArray[randIdx];
            g_iCunitCaseArray[randIdx] = g_iCunitCaseArray[idx];
            g_iCunitCaseArray[idx] = subCaseArrayTemp;
            ICunitRunTestcaseSatisfied(&(g_iCunitCaseArray[idx]), testcaseLayer, testcaseModule, testcaseLevel,
                testcaseType);
        }

        ICunitRunTestcaseSatisfied(&(g_iCunitCaseArray[1]), testcaseLayer, testcaseModule, testcaseLevel, testcaseType);
        ICunitRunTestcaseSatisfied(&(g_iCunitCaseArray[0]), testcaseLayer, testcaseModule, testcaseLevel, testcaseType);
    }

    return (iUINT32)ICUNIT_SUCCESS;
}

extern iUINT32 ICunitRunTestcaseOne(ICUNIT_CASE_S *testCase);

iUINT32 ICunitRunTestOne(const char *tcId)
{
    iUINT32 idx;
    iUINT32 idx1;
    ICUNIT_CASE_S subCaseArrayTemp;

    memset(&subCaseArrayTemp, 0, sizeof(ICUNIT_CASE_S));

    idx1 = g_iCunitCaseCnt;

    for (idx = 0; idx < idx1; idx++) {
        memset(&subCaseArrayTemp, 0, sizeof(ICUNIT_CASE_S));
        subCaseArrayTemp = g_iCunitCaseArray[idx];

        if (strcmp(subCaseArrayTemp.pcCaseID, tcId) == 0) {
            ICunitRunTestcaseOne(&g_iCunitCaseArray[idx]);
        }
    }

    return (iUINT32)ICUNIT_SUCCESS;
}

iUINT32 ICunitRunTestcaseSatisfied(ICUNIT_CASE_S *testCase, iUINT32 testcaseLayer, iUINT32 testcaseModule,
    iUINT32 testcaseLevel, iUINT32 testcaseType)
{
    /* reserve interface to extend */
    if (((testCase->testcase_layer == testcaseLayer) || (testcaseLayer == TEST_LAYER_ALL)) &&
        ((testCase->testcase_module == testcaseModule) || (testcaseModule == TEST_MODULE_ALL)) &&
        ((testCase->testcase_level == testcaseLevel) || (testcaseLevel == TEST_LEVEL_ALL)) &&
        ((testCase->testcase_type == testcaseType) || (testcaseType == TEST_TYPE_ALL))) {
        ICunitRunSingle(testCase);
    }

    return (iUINT32)ICUNIT_SUCCESS;
}

iUINT32 ICunitRunTestcaseOne(ICUNIT_CASE_S *testCase)
{
    ICunitRunSingle(testCase);
    return (iUINT32)ICUNIT_SUCCESS;
}


#ifndef TST_DRVPRINT
iUINT32 PtSaveReport(iCHAR *iCunitReportName, iCHAR *name, iUINT32 value)
{
    return ICUNIT_SUCCESS;
}
#endif

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */
