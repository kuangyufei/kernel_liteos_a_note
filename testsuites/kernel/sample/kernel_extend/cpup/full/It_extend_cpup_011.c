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

static UINT32 Testcase(VOID)
{
    UINT32 loop;
    UINT32 ret, cpupUse;
    UINT32 systemProcessNumber = LOS_GetSystemProcessMaximum();
    UINT32 cpupInfoLen;
    CPUP_INFO_S *cpup = NULL;

#ifdef LOSCFG_CPUP_INCLUDE_IRQ
    UINT32 systemIrqNumber = LOS_GetSystemHwiMaximum();
    cpupInfoLen = systemIrqNumber * sizeof(CPUP_INFO_S);
    cpup = (CPUP_INFO_S *)LOS_MemAlloc((VOID *)OS_SYS_MEM_ADDR, cpupInfoLen);
    if (cpup == NULL) {
        PRINTK("%s[%d] malloc failure!\n", __FUNCTION__, __LINE__);
        return OS_ERROR;
    }

    ret = LOS_GetAllIrqCpuUsage(CPUP_LAST_ONE_SECONDS, cpup, cpupInfoLen);
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT1);
    for (loop = 0; loop < systemIrqNumber; loop++) {
        ICUNIT_ASSERT_SINGLE_CPUP_USAGE(cpup[loop].usage, EXIT1);
    }

    ret = LOS_GetAllIrqCpuUsage(CPUP_LAST_TEN_SECONDS, cpup, cpupInfoLen);
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT1);
    for (loop = 0; loop < systemIrqNumber; loop++) {
        ICUNIT_ASSERT_SINGLE_CPUP_USAGE(cpup[loop].usage, EXIT1);
    }

    ret = LOS_GetAllIrqCpuUsage(CPUP_ALL_TIME, cpup, cpupInfoLen);
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT1);
    for (loop = 0; loop < systemIrqNumber; loop++) {
        ICUNIT_ASSERT_SINGLE_CPUP_USAGE(cpup[loop].usage, EXIT1);
    }

    LOS_MemFree((VOID *)OS_SYS_MEM_ADDR, cpup);
    cpup = NULL;
#endif

    cpupInfoLen = systemProcessNumber * sizeof(CPUP_INFO_S);
    cpup = (CPUP_INFO_S *)LOS_MemAlloc((VOID *)OS_SYS_MEM_ADDR, cpupInfoLen);
    if (cpup == NULL) {
        PRINTK("%s[%d] malloc failure!\n", __FUNCTION__, __LINE__);
        return OS_ERROR;
    }

    ret = LOS_GetAllProcessCpuUsage(CPUP_LAST_ONE_SECONDS, cpup, cpupInfoLen);
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT1);
    for (loop = 0; loop < systemProcessNumber; loop++) {
        ICUNIT_ASSERT_PROCESS_CPUP_USAGE(cpup[loop].usage, EXIT1);
    }

    ret = LOS_GetAllProcessCpuUsage(CPUP_LAST_TEN_SECONDS, cpup, cpupInfoLen);
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT1);
    for (loop = 0; loop < systemProcessNumber; loop++) {
        ICUNIT_ASSERT_PROCESS_CPUP_USAGE(cpup[loop].usage, EXIT1);
    }

    ret = LOS_GetAllProcessCpuUsage(CPUP_ALL_TIME, cpup, cpupInfoLen);
    ICUNIT_GOTO_EQUAL(ret, LOS_OK, ret, EXIT1);
    for (loop = 0; loop < systemProcessNumber; loop++) {
        ICUNIT_ASSERT_PROCESS_CPUP_USAGE(cpup[loop].usage, EXIT1);
    }

    LOS_MemFree((VOID *)OS_SYS_MEM_ADDR, cpup);
    cpup = NULL;

EXIT1:
    LOS_MemFree((VOID *)OS_SYS_MEM_ADDR, cpup);
    cpup = NULL;
    return LOS_OK;
}

VOID ItExtendCpup011(VOID)
{
    TEST_ADD_CASE("ItExtendCpup011", Testcase, TEST_EXTEND, TEST_CPUP, TEST_LEVEL2, TEST_FUNCTION);
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */
