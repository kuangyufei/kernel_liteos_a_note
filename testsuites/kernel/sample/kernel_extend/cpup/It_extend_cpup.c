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

UINT32 GetNopCount(void)
{
    return LOS_CyclePerTickGet();
}

#ifdef LOSCFG_CPUP_INCLUDE_IRQ

CPUP_INFO_S g_testPstHwiCpupAll[OS_HWI_MAX_NUM];
CPUP_INFO_S g_testPstHwiCpup10s[OS_HWI_MAX_NUM];
CPUP_INFO_S g_testPstHwiCpup1s[OS_HWI_MAX_NUM];

UINT32 TestGetSingleHwiCpup(UINT32 hwi, UINT32 mode)
{
    UINT32 size;
    UINT32 tempHwi;
    if (hwi > OS_HWI_MAX_NUM) {
        return -1;
    }

    size = sizeof(CPUP_INFO_S) * LOS_GetSystemHwiMaximum();
    switch (mode) {
        case CPUP_LAST_ONE_SECONDS:
            (VOID)memset_s((VOID *)g_testPstHwiCpup1s, size, 0, size);
            (VOID)LOS_GetAllIrqCpuUsage(CPUP_LAST_ONE_SECONDS, g_testPstHwiCpup1s, size);
            tempHwi = g_testPstHwiCpup1s[hwi].usage;
            break;
        case CPUP_LAST_TEN_SECONDS:
            (VOID)memset_s((VOID *)g_testPstHwiCpup10s, size, 0, size);
            (VOID)LOS_GetAllIrqCpuUsage(CPUP_LAST_TEN_SECONDS, g_testPstHwiCpup10s, size);
            tempHwi = g_testPstHwiCpup10s[hwi].usage;
            break;
        case CPUP_ALL_TIME:
            /* fall-through */
        default:
            (VOID)memset_s((VOID *)g_testPstHwiCpupAll, size, 0, size);
            (VOID)LOS_GetAllIrqCpuUsage(CPUP_ALL_TIME, g_testPstHwiCpupAll, size);
            tempHwi = g_testPstHwiCpupAll[hwi].usage;
            break;
    }
    return tempHwi;
}
#endif


VOID ItSuiteExtendCpup(VOID)
{
#if defined(LOSCFG_TEST_SMOKE)
    ItExtendCpup001();
    ItExtendCpup002();
#endif

#if defined(LOSCFG_TEST_FULL)
    ItExtendCpup003();
    ItExtendCpup004();
    ItExtendCpup005();
    ItExtendCpup006();
    ItExtendCpup007();
    ItExtendCpup008();
    ItExtendCpup011();
    ItExtendCpup012();
#endif

#if defined(LOSCFG_TEST_LLT)
    LltExtendCpup003();
    ItExtendCpup009();
    ItExtendCpup010();
#endif

#ifdef LOSCFG_KERNEL_SMP
    ItSmpExtendCpup001();
    ItSmpExtendCpup002();
#ifdef LOSCFG_CPUP_INCLUDE_IRQ
    ItSmpExtendCpup003();
    ItSmpExtendCpup004();
#endif
    ItSmpExtendCpup005();
    ItSmpExtendCpup007();
    ItSmpExtendCpup008();
    ItSmpExtendCpup009();
    ItSmpExtendCpup010();
    ItSmpExtendCpup011();
    ItSmpExtendCpup012();
#endif
}
