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

static void TaskF01(void)
{
    LOS_AtomicInc(&g_testCount);
}

static void HwiF01(void)
{
    UINT32 ret;

    TSK_INIT_PARAM_S task1 = { 0 };

    TEST_HwiClear(HWI_NUM_TEST);

    TEST_TASK_PARAM_INIT(task1, "it_smp_task_143", (TSK_ENTRY_FUNC)TaskF01, TASK_PRIO_TEST_TASK);

    ICUNIT_ASSERT_EQUAL_VOID(g_testCount, 0, g_testCount);

    LOS_AtomicInc(&g_testCount);

    task1.usCpuAffiMask = CPUID_TO_AFFI_MASK(ArchCurrCpuid());
    ret = LOS_TaskCreate(&g_testTaskID01, &task1);
    ICUNIT_ASSERT_EQUAL_VOID(ret, LOS_ERRNO_TSK_YIELD_IN_INT, ret);
}

static UINT32 Testcase(void)
{
    UINT32 ret;

    TSK_INIT_PARAM_S task1 = { 0 };

    g_testCount = 0;

    LOS_TaskLock();

    ret = LOS_HwiCreate(HWI_NUM_TEST, 1, 0, HwiF01, 0);
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);
    HalIrqSetAffinity(HWI_NUM_TEST, CPUID_TO_AFFI_MASK(ArchCurrCpuid())); // current cpu

    TestHwiTrigger(HWI_NUM_TEST);

    TEST_HwiDelete(HWI_NUM_TEST);

    ICUNIT_ASSERT_EQUAL(g_testCount, 1, g_testCount);

    LOS_TaskUnlock();

    /* wait for TaskF01 is finished */
    TestAssertBusyTaskDelay(100, 2); // 100, Set the timeout of runtime; 2, test runing count

    ICUNIT_ASSERT_EQUAL(g_testCount, 1, g_testCount);

    return LOS_OK;

EXIT:
    LOS_TaskDelete(g_testTaskID01);

    return LOS_NOK;
}

void ItSmpLosTask143(void)
{
    TEST_ADD_CASE("ItSmpLosTask143", Testcase, TEST_LOS, TEST_TASK, TEST_LEVEL1, TEST_FUNCTION);
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */
