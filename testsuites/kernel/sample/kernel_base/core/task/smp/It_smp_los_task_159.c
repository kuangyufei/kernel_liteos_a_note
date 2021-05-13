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

static EVENT_CB_S g_eventCb01;

static void TaskF01(void)
{
    UINT32 ret;

    ICUNIT_ASSERT_EQUAL_VOID(g_testCount, 0, g_testCount);

    LOS_AtomicInc(&g_testCount);

    ret = LOS_TaskSuspend(g_testTaskID01);
    ICUNIT_ASSERT_EQUAL_VOID(ret, LOS_OK, ret);

    LOS_AtomicInc(&g_testCount);
}

static void TaskF02(void)
{
    UINT32 ret;
    int i;

    ICUNIT_ASSERT_EQUAL_VOID(g_testCount, 1, g_testCount);

    LOS_AtomicInc(&g_testCount);

    for (i = 0; i < TRandom() % 200; i++) { // 200, Number of cycles.
    }
    /* It is possible that TaskF01 is finished. */
    LOS_TaskDelete(g_testTaskID01);

    LOS_AtomicInc(&g_testCount);

    LOS_EventWrite(&g_eventCb01, 0x1);
}

static UINT32 Testcase(void)
{
    UINT32 ret;
    TSK_INIT_PARAM_S task1 = { 0 };
    int i, j;
    LOS_EventInit(&g_eventCb01);

    for (i = 0; i < IT_TASK_SMP_LOOP; i++) {
        g_testCount = 0;
        TEST_TASK_PARAM_INIT_AFFI(task1, "it_smp_task_159_1", (TSK_ENTRY_FUNC)TaskF01, TASK_PRIO_TEST_TASK - 1, 0);

        task1.usCpuAffiMask = CPUID_TO_AFFI_MASK(ArchCurrCpuid());
        ret = LOS_TaskCreate(&g_testTaskID01, &task1);
        ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);

        TEST_TASK_PARAM_INIT_AFFI(task1, "it_smp_task_159_2", (TSK_ENTRY_FUNC)TaskF02, TASK_PRIO_TEST_TASK - 1, 0);

        task1.usCpuAffiMask = CPUID_TO_AFFI_MASK((ArchCurrCpuid() + 1) % (LOSCFG_KERNEL_CORE_NUM));
        ret = LOS_TaskCreate(&g_testTaskID02, &task1);
        ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);

        /* wait for task02 start */
        TestAssertBusyTaskDelay(100, 2); // 100, Set the timeout of runtime; 2, test runing count

        for (j = 0; j < TRandom() % 200; j++) { // 200, Number of cycles.
        }
        /* It is possible that delete TaskF01 first. */
        ret = LOS_TaskResume(g_testTaskID01);
        if (ret != LOS_ERRNO_TSK_NOT_CREATED && ret != LOS_OK) {
            goto EXIT;
        }

        /* possible value of g_testCount are 2, 3, 4 */
        if (g_testCount != 2 && g_testCount != 3 && g_testCount != 4) {
            goto EXIT;
        }

        ret = LOS_EventRead(&g_eventCb01, 0x11, LOS_WAITMODE_OR | LOS_WAITMODE_CLR, LOS_WAIT_FOREVER);
        ICUNIT_GOTO_EQUAL(ret, 0x1, ret, EXIT);

        TestBusyTaskDelay(2); // 2, set delay time

        ret = OS_TCB_FROM_TID(g_testTaskID01)->taskStatus;
        ICUNIT_GOTO_EQUAL(!(ret & OS_TASK_STATUS_UNUSED), 0, ret, EXIT);

        ret = OS_TCB_FROM_TID(g_testTaskID02)->taskStatus;
        ICUNIT_GOTO_EQUAL(!(ret & OS_TASK_STATUS_UNUSED), 0, ret, EXIT);
    }

    return LOS_OK;

EXIT:
    LOS_TaskDelete(g_testTaskID01);
    LOS_TaskDelete(g_testTaskID02);

    return LOS_NOK;
}

void ItSmpLosTask159(void)
{
    TEST_ADD_CASE("ItSmpLosTask159", Testcase, TEST_LOS, TEST_TASK, TEST_LEVEL0, TEST_FUNCTION);
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */
