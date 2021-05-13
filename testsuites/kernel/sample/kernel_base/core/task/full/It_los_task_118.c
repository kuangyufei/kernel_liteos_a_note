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

#include "It_los_task.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cpluscplus */
#endif /* __cpluscplus */

static UINT32 g_sumSize;
static void *g_ptr1 = NULL;
static void *g_ptr2 = NULL;
extern UINT32 OsTaskMemUsage(UINT32 taskID);
static void HwiF01(void)
{
    UINT32 size = 2;
    UINT32 ret;
    UINT32 sumSize;
    g_ptr1 = LOS_MemAlloc((void *)OS_SYS_MEM_ADDR, size);
    ICUNIT_GOTO_NOT_EQUAL(g_ptr1, NULL, g_ptr1, EXIT);
    sumSize = OsTaskMemUsage(g_testTaskID01);
    ICUNIT_GOTO_EQUAL(sumSize, g_sumSize, sumSize, EXIT);
    LOS_MemFree((void *)OS_SYS_MEM_ADDR, g_ptr1);
    sumSize = OsTaskMemUsage(g_testTaskID01);
    ICUNIT_GOTO_EQUAL(sumSize, g_sumSize, sumSize, EXIT);
EXIT:
    TEST_HwiClear(HWI_NUM_TEST);
}

static void TaskF02(void)
{
    UINT32 ret;
    UINT32 sumSize;

    LOS_MemFree((void *)OS_SYS_MEM_ADDR, g_ptr2);
    sumSize = OsTaskMemUsage(g_testTaskID02);
    ICUNIT_GOTO_EQUAL(sumSize, 0, sumSize, EXIT);
    sumSize = OsTaskMemUsage(g_testTaskID01);
    ICUNIT_GOTO_EQUAL(sumSize, 0, sumSize, EXIT);

    return;
EXIT:
    LOS_TaskDelete(g_testTaskID02);
    return;
}

static void TaskF01(void)
{
    UINT32 ret;
    UINT32 sumSize1, sumSize2;
    UINT32 size = 2;
    HWI_PRIOR_T hwiPrio = 3;
    HWI_MODE_T mode = 0;
    HWI_ARG_T arg = 0;

    ret = LOS_HwiCreate(HWI_NUM_TEST, hwiPrio, mode, (HWI_PROC_FUNC)HwiF01, arg);
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);
#if (LOSCFG_KERNEL_SMP == YES)
    HalIrqSetAffinity(HWI_NUM_TEST, CPUID_TO_AFFI_MASK(ArchCurrCpuid()));
#endif

    g_sumSize = OsTaskMemUsage(g_testTaskID01);

    TestHwiTrigger(HWI_NUM_TEST);

    TEST_HwiDelete(HWI_NUM_TEST);

    g_ptr2 = LOS_MemAlloc((void *)OS_SYS_MEM_ADDR, size);
    ICUNIT_GOTO_NOT_EQUAL(g_ptr2, NULL, g_ptr2, EXIT);
    sumSize1 = OsTaskMemUsage(g_testTaskID01);
    ICUNIT_GOTO_NOT_EQUAL(sumSize1, 0, sumSize1, EXIT);
    LOS_TaskDelay(10); // 10, set delay time.

    size = 0x200;
    g_ptr1 = LOS_MemAlloc((void *)OS_SYS_MEM_ADDR, size);
    ICUNIT_GOTO_NOT_EQUAL(g_ptr1, NULL, g_ptr1, EXIT);
    sumSize1 = OsTaskMemUsage(g_testTaskID01);
    ICUNIT_GOTO_NOT_EQUAL(sumSize1, 0, sumSize1, EXIT);

    size = 0x2;
    g_ptr2 = LOS_MemAlloc((void *)OS_SYS_MEM_ADDR, size);
    ICUNIT_GOTO_NOT_EQUAL(g_ptr2, NULL, g_ptr2, EXIT);
    sumSize2 = OsTaskMemUsage(g_testTaskID01);
    ICUNIT_GOTO_NOT_EQUAL(sumSize2, sumSize1, sumSize2, EXIT);

    LOS_MemFree((void *)OS_SYS_MEM_ADDR, g_ptr2);
    sumSize2 = OsTaskMemUsage(g_testTaskID01);
    ICUNIT_GOTO_EQUAL(sumSize2, sumSize1, sumSize2, EXIT);

    LOS_MemFree((void *)OS_SYS_MEM_ADDR, g_ptr1);
    sumSize2 = OsTaskMemUsage(g_testTaskID01);
    ICUNIT_GOTO_EQUAL(sumSize2, 0, sumSize2, EXIT);

EXIT:
    LOS_TaskDelete(g_testTaskID01);
    return;
}

static UINT32 Testcase(void)
{
    UINT32 ret;
    TSK_INIT_PARAM_S task1 = { 0 };
    task1.pfnTaskEntry = (TSK_ENTRY_FUNC)TaskF01;
    task1.uwStackSize = TASK_STACK_SIZE_TEST;
    task1.pcName = "Tsk118A";
    // 2, It is used to calculate a priority relative to TASK_PRIO_TEST_TASK.
    task1.usTaskPrio = TASK_PRIO_TEST_TASK - 2;
    task1.uwResved = LOS_TASK_STATUS_DETACHED;
#if (LOSCFG_KERNEL_SMP == YES)
    task1.usCpuAffiMask = CPUID_TO_AFFI_MASK(ArchCurrCpuid());
#endif
    ret = LOS_TaskCreate(&g_testTaskID01, &task1);
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);

    memset(&task1, 0, sizeof(TSK_INIT_PARAM_S));
    task1.pfnTaskEntry = (TSK_ENTRY_FUNC)TaskF02;
    task1.uwStackSize = TASK_STACK_SIZE_TEST;
    task1.pcName = "Tsk118B";
    // 2, It is used to calculate a priority relative to TASK_PRIO_TEST_TASK.
    task1.usTaskPrio = TASK_PRIO_TEST_TASK - 2;
    task1.uwResved = LOS_TASK_STATUS_DETACHED;
#if (LOSCFG_KERNEL_SMP == YES)
    task1.usCpuAffiMask = CPUID_TO_AFFI_MASK(ArchCurrCpuid());
#endif
    ret = LOS_TaskCreate(&g_testTaskID02, &task1);
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);

    LOS_TaskDelay(10); // 10, set delay time.
    return LOS_OK;

EXIT:
    LOS_TaskDelete(g_testTaskID01);
    LOS_TaskDelete(g_testTaskID02);

    return LOS_OK;
}

void ItLosTask118(void) // IT_Layer_ModuleORFeature_No
{
    TEST_ADD_CASE("ItLosTask118", Testcase, TEST_LOS, TEST_TASK, TEST_LEVEL2, TEST_FUNCTION);
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */
