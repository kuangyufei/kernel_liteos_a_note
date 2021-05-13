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
    while (1) {
        WFI;
    };
}

static UINT32 Testcase(void)
{
    TSK_INIT_PARAM_S testTask;
    UINT32 ret, currCpuid;
    UINT32 testid = 0xff;

    /* make sure that created test task is definitely on another core */
    currCpuid = (ArchCurrCpuid() + 1) % LOSCFG_KERNEL_CORE_NUM;

    TEST_TASK_PARAM_INIT_AFFI(testTask, "it_smp_task_003", TaskF01, OS_TASK_PRIORITY_LOWEST - 1,
        CPUID_TO_AFFI_MASK(currCpuid));

    ret = LOS_TaskCreate(&testid, &testTask);
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);

    LOS_TaskDelay(2); // 2, set delay time.
    LOS_TaskSuspend(testid);

    /* delay and check */
    LOS_TaskDelay(10); // 10, set delay time.

    ret = OS_TCB_FROM_TID(testid)->taskStatus;
    ICUNIT_GOTO_EQUAL(ret & OS_TASK_STATUS_SUSPEND, OS_TASK_STATUS_SUSPEND, ret, EXIT);

    LOS_TaskResume(testid);

EXIT:
    LOS_TaskDelay(2); // 2, set delay time.
    LOS_TaskDelete(testid);

    return LOS_OK;
}

void ItSmpLosTask003(void)
{
    TEST_ADD_CASE("ItSmpLosTask003", Testcase, TEST_LOS, TEST_TASK, TEST_LEVEL0, TEST_FUNCTION);
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */
