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

static void HwiF01(void)
{
    UINT32 ret;

    TEST_HwiClear(HWI_NUM_TEST);

    LOS_AtomicInc(&g_testCount);

    ret = OS_TCB_FROM_TID(g_testTaskID01)->taskStatus;
    ICUNIT_ASSERT_EQUAL_VOID(ret, (OS_TASK_STATUS_READY | OS_TASK_STATUS_DETACHED), ret);

    ret = LOS_TaskYield();
    ICUNIT_ASSERT_EQUAL_VOID(ret, LOS_ERRNO_TSK_YIELD_IN_INT, ret);
}

static void TaskF01(void)
{
    LOS_AtomicInc(&g_testCount);

    while (1) {
    }
}

static UINT32 Testcase(void)
{
    UINT32 ret;
    TSK_INIT_PARAM_S task1 = { 0 };
    g_testCount = 0;

    TEST_TASK_PARAM_INIT(task1, "it_smp_task_131", (TSK_ENTRY_FUNC)TaskF01, TASK_PRIO_TEST_TASK);

    task1.usCpuAffiMask = CPUID_TO_AFFI_MASK(ArchCurrCpuid());
    ret = LOS_TaskCreate(&g_testTaskID01, &task1);
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);

    ret = OS_TCB_FROM_TID(g_testTaskID01)->taskStatus;
    ICUNIT_ASSERT_NOT_EQUAL((ret & OS_TASK_STATUS_READY), 0, ret);

    ret = LOS_HwiCreate(HWI_NUM_TEST, 1, 0, HwiF01, 0);
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);
    HalIrqSetAffinity(HWI_NUM_TEST, CPUID_TO_AFFI_MASK(ArchCurrCpuid())); // current cpu

    TestHwiTrigger(HWI_NUM_TEST);

    TEST_HwiDelete(HWI_NUM_TEST);

    TestBusyTaskDelay(2 * LOSCFG_BASE_CORE_TIMESLICE_TIMEOUT); // 2, set delay time

    ret = LOS_TaskDelete(g_testTaskID01);
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);

    ICUNIT_ASSERT_EQUAL(g_testCount, 2, g_testCount); // 2, assert that g_testCount is equal to this.

    return LOS_OK;
}

void ItSmpLosTask131(void)
{
    TEST_ADD_CASE("ItSmpLosTask131", Testcase, TEST_LOS, TEST_TASK, TEST_LEVEL1, TEST_FUNCTION);
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */
