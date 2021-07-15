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

#ifndef IT_LOS_CPUP_H
#define IT_LOS_CPUP_H

#include "los_cpup_pri.h"
#include "los_cpup.h"
#include "los_sys.h"
#include "los_task.h"
#include "osTest.h"

#ifdef __cplusplus
#if __cplusplus
    extern "C" {
#endif /* __cpluscplus */
#endif /* __cpluscplus */

#define CPU_USE_MODE1 1
#define CPU_USE_MODE0 0
#define CPU_USE_MIN 0
#define TASK_STATUS_UNDETACHED 0

#define CPUP_TEST_TOLERANT 50
#define CPUP_BACKWARD 2

#define ICUNIT_ASSERT_PROCESS_CPUP_USAGE(usage, lable)               \
    do {                                                             \
        UINT32 ret_ = LOS_NOK;                                       \
        if (((usage) > LOS_CPUP_PRECISION) || ((usage) < CPU_USE_MIN)) { \
            ret_ = LOS_OK;                                           \
        } else {                                                     \
            ret_ = usage;                                            \
        }                                                            \
        ICUNIT_GOTO_EQUAL(ret_, LOS_OK, ret_, lable);                \
    } while (0)

#define ICUNIT_ASSERT_SINGLE_CPUP_USAGE(usage, lable)                            \
    do {                                                                         \
        UINT32 ret_ = LOS_NOK;                                                   \
        if (((usage) > LOS_CPUP_SINGLE_CORE_PRECISION) || ((usage) < CPU_USE_MIN)) { \
            ret_ = LOS_OK;                                                       \
        } else {                                                                 \
            ret_ = usage;                                                        \
        }                                                                        \
        ICUNIT_GOTO_EQUAL(ret_, LOS_OK, ret_, lable);                            \
    } while (0)

extern UINT32 g_cpuTestTaskID;
extern UINT32 g_testTaskID01;
extern UINT32 g_testTaskID02;
extern UINT32 g_testTaskID03;
extern UINT32 g_cpupTestCount;
extern VOID ItSuiteExtendCpup(VOID);
extern UINT32 TimeClkRead(VOID);
extern LITE_OS_SEC_TEXT_MINOR UINT32 OsShellCmdCpup(INT32 argc, CHAR **argv);
extern LITE_OS_SEC_TEXT_MINOR VOID OsTaskCycleEnd(VOID);
extern LITE_OS_SEC_TEXT_MINOR UINT64 OsGetCpuCycle(VOID);

extern UINT32 GetNopCount(void);
#ifdef LOSCFG_CPUP_INCLUDE_IRQ
extern UINT32 TestGetSingleHwiCpup(UINT32 hwi, UINT32 mode);
#endif

#if defined(LOSCFG_TEST_SMOKE)
VOID ItExtendCpup001(VOID);
VOID ItExtendCpup002(VOID);
#endif

#if defined(LOSCFG_TEST_FULL)
VOID ItExtendCpup003(VOID);
VOID ItExtendCpup004(VOID);
VOID ItExtendCpup005(VOID);
VOID ItExtendCpup006(VOID);
VOID ItExtendCpup007(VOID);
VOID ItExtendCpup008(VOID);
VOID ItExtendCpup011(VOID);
VOID ItExtendCpup012(VOID);
#endif

#if defined(LOSCFG_TEST_LLT)
VOID LLT_EXTEND_CPUP_001(VOID);
VOID LLT_EXTEND_CPUP_002(VOID);
VOID LltExtendCpup003(VOID);
VOID LLT_EXTEND_CPUP_006(VOID);
VOID LLT_EXTEND_CPUP_007(VOID);
VOID ItExtendCpup009(VOID);
VOID ItExtendCpup010(VOID);
#endif

#ifdef LOSCFG_KERNEL_SMP
VOID ItSmpExtendCpup001(VOID);
VOID ItSmpExtendCpup002(VOID);
#ifdef LOSCFG_CPUP_INCLUDE_IRQ
VOID ItSmpExtendCpup003(VOID);
VOID ItSmpExtendCpup004(VOID);
#endif
VOID ItSmpExtendCpup005(VOID);
VOID ItSmpExtendCpup006(VOID);
VOID ItSmpExtendCpup007(VOID);
VOID ItSmpExtendCpup008(VOID);
VOID ItSmpExtendCpup009(VOID);
VOID ItSmpExtendCpup010(VOID);
VOID ItSmpExtendCpup011(VOID);
VOID ItSmpExtendCpup012(VOID);
#endif

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */

#endif
