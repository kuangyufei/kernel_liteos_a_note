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

#include "It_extend_cpup.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cpluscplus */
#endif /* __cpluscplus */

#define ICUNIT_ASSERT_CPUP_EQUAL(cpupUse, val)                                   \
    do {                                                                         \
        if ((cpupUse) > LOS_CPUP_SINGLE_CORE_PRECISION || (cpupUse) < CPU_USE_MIN) { \
            ICUNIT_ASSERT_EQUAL(cpupUse, val, cpupUse);                          \
        }                                                                        \
    } while (0)

static INT32 TaskF01(VOID)
{
    INT32 testCount = 1000; // 1000, set to init testcount.
    UINT32 cpupUse;
    LOS_Mdelay(5000); // 5000, set delay time.

    const CHAR *taskAll = "-a";
    OsShellCmdDumpTask(1, &taskAll);

    while (testCount > 0) {
        cpupUse = LOS_HistoryProcessCpuUsage(LOS_GetCurrProcessID(), CPUP_LAST_TEN_SECONDS);
        ICUNIT_ASSERT_CPUP_EQUAL(cpupUse, OS_INVALID_VALUE);
        cpupUse = LOS_HistoryProcessCpuUsage(LOS_GetCurrProcessID(), CPUP_LAST_ONE_SECONDS);
        ICUNIT_ASSERT_CPUP_EQUAL(cpupUse, OS_INVALID_VALUE);

        cpupUse = LOS_HistorySysCpuUsage(CPU_USE_MODE1);
        if ((cpupUse > LOS_CPUP_PRECISION) || (cpupUse < CPU_USE_MIN)) {
            ICUNIT_ASSERT_EQUAL(cpupUse, OS_INVALID_VALUE, cpupUse);
        }

        testCount--;
    }
    cpupUse = LOS_HistoryProcessCpuUsage(LOS_GetCurrProcessID(), CPUP_ALL_TIME);
    ICUNIT_ASSERT_CPUP_EQUAL(cpupUse, OS_INVALID_VALUE);

    LOS_CpupReset();
    LOS_Mdelay(5000); // 5000, set delay time.

    testCount = 1000;  // 1000, set to init testcount.
    while (testCount > 0) {
        cpupUse = LOS_HistoryProcessCpuUsage(LOS_GetCurrProcessID(), CPUP_LAST_TEN_SECONDS);
        ICUNIT_ASSERT_CPUP_EQUAL(cpupUse, OS_INVALID_VALUE);
        cpupUse = LOS_HistoryProcessCpuUsage(LOS_GetCurrProcessID(), CPUP_LAST_ONE_SECONDS);
        ICUNIT_ASSERT_CPUP_EQUAL(cpupUse, OS_INVALID_VALUE);

        cpupUse = LOS_HistorySysCpuUsage(CPU_USE_MODE1);
        if ((cpupUse > LOS_CPUP_PRECISION) || (cpupUse < CPU_USE_MIN)) {
            ICUNIT_ASSERT_EQUAL(cpupUse, OS_INVALID_VALUE, cpupUse);
        }
        testCount--;
    }

    cpupUse = LOS_HistoryProcessCpuUsage(LOS_GetCurrProcessID(), CPUP_ALL_TIME);
    ICUNIT_ASSERT_CPUP_EQUAL(cpupUse, OS_INVALID_VALUE);

    OsShellCmdDumpTask(1, &taskAll);

EXIT:
    return 0;
}

static UINT32 Testcase(VOID)
{
    INT32 pid = LOS_Fork(0, "TestCpupProcess", TaskF01, TASK_STACK_SIZE_TEST);
    if (pid < 0) {
        ICUNIT_ASSERT_EQUAL(1, LOS_OK, 1);
    }

    LOS_Wait(pid, 0, 0, 0);

    return LOS_OK;
}

VOID ItExtendCpup012(VOID)
{
    TEST_ADD_CASE("ItExtendCpup012", Testcase, TEST_EXTEND, TEST_CPUP, TEST_LEVEL2, TEST_FUNCTION);
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */
