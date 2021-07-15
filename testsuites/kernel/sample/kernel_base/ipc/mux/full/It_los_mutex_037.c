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
#include "los_config.h"
#include "los_bitmap.h"
#include "It_los_mux.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

#define OS_MUX_NUM 1000
LosMux *g_testMux = NULL;

static VOID TaskFe4Func(VOID)
{
    UINT32 ret;

    g_testCount++;
    for (INT32 i = 0; i < OS_MUX_NUM; i++) {
        ret = LOS_MuxLock(&g_testMux[i], LOS_WAIT_FOREVER);
        ICUNIT_ASSERT_EQUAL_VOID(ret, LOS_OK, ret);
    }

    g_testCount++;
    return;
}

static VOID TaskMisc10Func(VOID)
{
    UINT32 ret;
    TSK_INIT_PARAM_S task1, task2, task3;

    g_testCount++;
    for (INT32 i = 0; i < OS_MUX_NUM; i++) {
        ret = LOS_MuxLock(&g_testMux[i], LOS_WAIT_FOREVER);
        ICUNIT_ASSERT_EQUAL_VOID(ret, LOS_OK, ret);
    }

    g_testCount++;
    PRINTK("taskMisc_10_func LOCK : 0x%x\n", &g_testMux[0]);
    return;
}


static UINT32 Testcase(VOID)
{
    UINT32 ret;
    TSK_INIT_PARAM_S task;
    g_testCount = 0;

    g_testMux = LOS_MemAlloc(m_aucSysMem0, OS_MUX_NUM * sizeof(LosMux));
    ICUNIT_ASSERT_NOT_EQUAL(g_testMux, NULL, g_testMux);

    for (INT32 i = 0; i < OS_MUX_NUM; i++) {
        ret = LosMuxCreate(&g_testMux[i]);
        ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);
    }

    g_testCount = 0;

    task.pfnTaskEntry = (TSK_ENTRY_FUNC)TaskMisc10Func;
    task.usTaskPrio = 10; // 10, set reasonable priority.
    task.pcName = "TaskMisc_10";
    task.uwStackSize = LOSCFG_BASE_CORE_TSK_DEFAULT_STACK_SIZE;
    task.uwResved = LOS_TASK_STATUS_DETACHED;
#ifdef LOSCFG_KERNEL_SMP
    task.usCpuAffiMask = CPUID_TO_AFFI_MASK(ArchCurrCpuid());
#endif

    ret = LOS_TaskCreate(&g_testTaskID01, &task);
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);

    task.pfnTaskEntry = (TSK_ENTRY_FUNC)TaskFe4Func;
    task.usTaskPrio = 4; // 4, set reasonable priority.
    task.pcName = "TaskFe_4";
    task.uwStackSize = LOSCFG_BASE_CORE_TSK_DEFAULT_STACK_SIZE;
    task.uwResved = LOS_TASK_STATUS_DETACHED;
#ifdef LOSCFG_KERNEL_SMP
    task.usCpuAffiMask = CPUID_TO_AFFI_MASK(ArchCurrCpuid());
#endif

    ret = LOS_TaskCreate(&g_testTaskID03, &task);
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);

    for (INT32 i = 0; i < OS_MUX_NUM; i++) {
        ret = LOS_MuxLock(&g_testMux[i], LOS_WAIT_FOREVER);
        ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT);
    }

    ICUNIT_GOTO_EQUAL(g_testCount, 4, g_testCount, EXIT); // 4, here assert the result.

EXIT:
    for (UINT32 i = 0; i < OS_MUX_NUM; i++) {
        LOS_MuxUnlock(&g_testMux[i]);
        LOS_MuxDestroy(&g_testMux[i]);
    }

    LOS_TaskDelete(g_testTaskID01);
    LOS_TaskDelete(g_testTaskID03);
    LOS_MemFree(m_aucSysMem0, g_testMux);

    return LOS_OK;
}

VOID ItLosMux037(void)
{
    TEST_ADD_CASE("ItLosMux037", Testcase, TEST_LOS, TEST_MUX, TEST_LEVEL2, TEST_FUNCTION);
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */
