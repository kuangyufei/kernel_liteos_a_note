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

#include "It_los_float.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cpluscplus */
#endif /* __cpluscplus */

#ifndef LOSCFG_ARCH_FPU_DISABLE

static UINT32 g_sz[LOSCFG_KERNEL_CORE_NUM] = {0};
static UINT32 g_cpuidTask1, g_cpuidTask2;
static void TaskF01(UINT32 arg)
{
    UINT32 temp1 = 0xff;
    UINT32 temp2;
    FLOAT a = 123.321;
    FLOAT b = 1234.4321;
    FLOAT c = 999.002345;
    FLOAT d, f, e;
    LOS_AtomicInc(&g_testCount);

    while (temp1--) {
        temp2 = 0xff;
        while (temp2--) {
            d = a * b + c + temp2;
            f = a * c + b + temp2;
            e = a + b * c + temp2;

            // 153230.406250, numbers involved in floating-point operations, without special functions.
            if (d != 153230.406250 + temp2) {
                dprintf("Error:d = %f----temp = 0x%x----\n", d, temp2);
                LOS_AtomicInc(&g_testCount);
                continue;
            // 124432.390625, numbers involved in floating-point operations, without special functions.
            } else if (f != 124432.390625 + temp2) {
                dprintf("Error:f = %f----temp = 0x%x----\n", f, temp2);
                LOS_AtomicInc(&g_testCount);
                continue;
            // 1233323.875000, numbers involved in floating-point operations, without special functions.
            } else if (e != (1233323.875000 + temp2)) {
                dprintf("Error:e = %f----temp = 0x%x----\n", e, temp2);
                LOS_AtomicInc(&g_testCount);
                continue;
            }
            // 153230.406250, numbers involved in floating-point operations, without special functions.
            ICUNIT_GOTO_EQUAL(d, 153230.406250 + temp2, temp2, EXIT);
            // 124432.390625, numbers involved in floating-point operations, without special functions.
            ICUNIT_GOTO_EQUAL(f, 124432.390625 + temp2, temp2, EXIT);
            // 1233323.875000, numbers involved in floating-point operations, without special functions.
            ICUNIT_GOTO_EQUAL(e, 1233323.875000 + temp2, temp2, EXIT);
            // 2, obtains the CPU ID.
            LOS_TaskCpuAffiSet(g_testTaskID01, temp2 % 2 ? g_cpuidTask1 : g_cpuidTask2);
        }
    }

EXIT:
    LOS_TaskDelete(g_testTaskID01);
    return;
}

static void TaskF02(UINT32 arg)
{
    UINT32 temp1 = 0xff;
    UINT32 temp2;
    FLOAT a = 123.321;
    FLOAT b = 1234.4321;
    FLOAT c = 999.002345;
    FLOAT d, f, e;
    LOS_AtomicInc(&g_testCount);

    while (temp1--) {
        temp2 = 0xffff;
        while (temp2--) {
            d = a * b + c + temp2;
            f = a * c + b + temp2;
            e = a + b * c + temp2;

            // 153230.406250, numbers involved in floating-point operations, without special functions.
            if (d != 153230.406250 + temp2) {
                dprintf("Error:d = %f----temp = 0x%x----\n", d, temp2);
                LOS_AtomicInc(&g_testCount);
                continue;
            } else if (f != 124432.390625 + temp2) { // 124432.390625, numbers involved in floating-point operations, without special functions.
                dprintf("Error:f = %f----temp = 0x%x----\n", f, temp2);
                LOS_AtomicInc(&g_testCount);
                continue;
            } else if (e != (1233323.875000 + temp2)) { // 1233323.875000, numbers involved in floating-point operations, without special functions.
                dprintf("Error:e = %f----temp = 0x%x----\n", e, temp2);
                LOS_AtomicInc(&g_testCount);
                continue;
            }
            // 153230.406250, numbers involved in floating-point operations, without special functions.
            ICUNIT_GOTO_EQUAL(d, 153230.406250 + temp2, temp2, EXIT);
            // 124432.390625, numbers involved in floating-point operations, without special functions.
            ICUNIT_GOTO_EQUAL(f, 124432.390625 + temp2, temp2, EXIT);
            // 1233323.875000, numbers involved in floating-point operations, without special functions.
            ICUNIT_GOTO_EQUAL(e, 1233323.875000 + temp2, temp2, EXIT);
            // 2, obtains the CPU ID.
            LOS_TaskCpuAffiSet(g_testTaskID02, temp2 % 2 ? g_cpuidTask2 : g_cpuidTask1);
        }
    }

EXIT:
    LOS_TaskDelete(g_testTaskID02);
    return;
}

static UINT32 Testcase(void)
{
    TSK_INIT_PARAM_S testTask;
    UINT32 ret, currCpuid;

    g_testCount = 0;

    currCpuid = (ArchCurrCpuid() + 1) % (LOSCFG_KERNEL_CORE_NUM);

    g_cpuidTask1 = ArchCurrCpuid();
    g_cpuidTask2 = currCpuid;

    TEST_TASK_PARAM_INIT_AFFI(testTask, "it_smp_float_switch_002_task1", TaskF01, TASK_PRIO_TEST_TASK + 1,
        CPUID_TO_AFFI_MASK(g_cpuidTask1)); // current cpu
    ret = LOS_TaskCreate(&g_testTaskID01, &testTask);
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);

    // 2, It is used to calculate a priority relative to TASK_PRIO_TEST_TASK.
    TEST_TASK_PARAM_INIT_AFFI(testTask, "it_event_028_task2", TaskF02, TASK_PRIO_TEST_TASK + 2,
        CPUID_TO_AFFI_MASK(g_cpuidTask2)); // other cpu
    ret = LOS_TaskCreate(&g_testTaskID02, &testTask);
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);

    LOS_TaskDelay(100); // 100, set delay time.

    ICUNIT_GOTO_EQUAL(g_testCount, 2, g_testCount, EXIT); // 2, assert that g_testCount is equal to this.

EXIT:

    LOS_TaskDelete(g_testTaskID01);
    LOS_TaskDelete(g_testTaskID02);
    return LOS_OK;
}
#endif

void ItSmpLosFloatSwitch002(void)
{
#ifndef LOSCFG_ARCH_FPU_DISABLE
    TEST_ADD_CASE("IT_SMP_LOS_FLOAT_SWITCH_002", Testcase, TEST_LOS, TEST_TASK, TEST_LEVEL0, TEST_FUNCTION);
#endif
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */
