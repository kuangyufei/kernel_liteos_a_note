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

#ifndef TESTHI1980IMU
#ifndef LOSCFG_ARCH_FPU_DISABLE

static UINT32 g_fpsrc01;
static UINT32 g_fpsrc02;
static UINT32 g_fpsrc03;

static void ItHwi()
{
    ICUNIT_ASSERT_EQUAL_VOID(g_testCount, 1, g_testCount);
    float d1 = -100.0f;
    UINT32 n1 = 0;
    __asm__("vldr s15, %0" ::"m"(d1));
    __asm__("vcmp.f32 s15, #0");

    __asm__ __volatile__("vmrs %0, fpscr\n" : "=r"(n1)::); // n1  is fpscr
    g_fpsrc03 = n1;
    dprintf("after change fpscr = %x\n", n1);

    g_testCount += 1;
    return;
}
static UINT32 Testcase(void)
{
    UINT32 ret;
    HWI_PRIOR_T hwiPrio = 3;
    HWI_MODE_T mode = 0;
    HWI_ARG_T arg = 0;

    g_testCount = 0;
    g_fpsrc01 = 0;
    g_fpsrc02 = 0;
    g_fpsrc03 = 0;
    ret = LOS_HwiCreate(HWI_NUM_TEST3, hwiPrio, mode, (HWI_PROC_FUNC)ItHwi, arg);
    ICUNIT_ASSERT_EQUAL(ret, LOS_OK, ret);

#ifdef LOSCFG_KERNEL_SMP
    HalIrqSetAffinity(HWI_NUM_TEST3, ArchCurrCpuid() + 1); // curret cpu
#endif
    float d1 = 100.0f;
    UINT32 n1 = 0;
    __asm__("vldr s15, %0" ::"m"(d1));
    __asm__("vcmp.f32 s15, #0");

    __asm__ __volatile__("vmrs %0, fpscr\n" : "=r"(n1)::); // n1  is fpscr
    g_fpsrc01 = n1;
    dprintf("before schual fpscr = %x\n", n1);

    ICUNIT_ASSERT_EQUAL(g_testCount, 0, g_testCount);
    g_testCount++;
    TestHwiTrigger(HWI_NUM_TEST3);
    ICUNIT_ASSERT_EQUAL(g_testCount, 2, g_testCount); // 2, assert that g_testCount is equal to this.
    __asm__ __volatile__("vmrs %0, fpscr\n" : "=r"(n1)::);
    g_fpsrc02 = n1;
    dprintf("after resume fpscr = %x\n", n1);
    g_testCount++;
    ICUNIT_ASSERT_EQUAL(g_testCount, 3, g_testCount); // 3, assert that g_testCount is equal to this.

    TEST_HwiDelete(HWI_NUM_TEST3);
    ICUNIT_ASSERT_EQUAL(g_fpsrc01, g_fpsrc02, g_fpsrc01);
    ICUNIT_ASSERT_NOT_EQUAL(g_fpsrc01, g_fpsrc03, g_fpsrc03);
    return LOS_OK;
}
#endif

void ItLosTask142(void)
{
#ifndef LOSCFG_ARCH_FPU_DISABLE
    TEST_ADD_CASE("ItLosTask142", Testcase, TEST_LOS, TEST_TASK, TEST_LEVEL2, TEST_FUNCTION);
#endif
    return;
}
#endif

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */
