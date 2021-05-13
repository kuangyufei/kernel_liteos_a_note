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

#include "It_los_sem.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cpluscplus */
#endif /* __cpluscplus */

VOID ItSuiteLosSem(void)
{
#if (LOSCFG_KERNEL_SMP == YES)
    /* fixed */
    ItSmpLosSem008();
    ItSmpLosSem022();
    ItSmpLosSem025();

    ItSmpLosSem001();
    ItSmpLosSem002();
    ItSmpLosSem003();
    ItSmpLosSem004();
    ItSmpLosSem005();
    ItSmpLosSem006();
    ItSmpLosSem007();
    ItSmpLosSem009();
    ItSmpLosSem010();
    ItSmpLosSem011();
    ItSmpLosSem012();
    ItSmpLosSem013();
    ItSmpLosSem014();
    ItSmpLosSem015();
    ItSmpLosSem016();
    ItSmpLosSem017();
    ItSmpLosSem018();
    ItSmpLosSem019();
    ItSmpLosSem020();
    ItSmpLosSem021();
    ItSmpLosSem023();
    ItSmpLosSem024();
    ItSmpLosSem026();
    ItSmpLosSem027();
    ItSmpLosSem028();
    ItSmpLosSem029();
    ItSmpLosSem030();
    ItSmpLosSem031();
    ItSmpLosSem032();
    ItSmpLosSem033();
    ItSmpLosSem034();
    ItSmpLosSem035();
    ItSmpLosSem036();
#endif

#if defined(LOSCFG_TEST_SMOKE)
    ItLosSem001();
    ItLosSem003();
    ItLosSem006();
#endif

#if defined(LOSCFG_TEST_FULL)
    ItLosSem002();
    ItLosSem005();
    ItLosSem009();
    ItLosSem012();
    ItLosSem013();
    ItLosSem014();
    ItLosSem015();
    ItLosSem016();
    ItLosSem017();
    ItLosSem019();
    ItLosSem020();
    ItLosSem022();
    ItLosSem023();
    ItLosSem026();
    ItLosSem027();
    ItLosSem028();
    ItLosSem029();
    ItLosSem034();
#endif

#if defined(LOSCFG_TEST_PRESSURE)
#ifndef TESTHI1980IMU
    ItLosSem035();
#endif
    ItLosSem036();
    ItLosSem037();
    ItLosSem038();
#if (LOSCFG_KERNEL_SMP_TASK_SYNC != YES)
    // LOSCFG_KERNEL_SMP_TASK_SYNC is opened ,create task success is depend on created semaphore successed;
    ItLosSem039();
    ItLosSem040();
    ItLosSem041();
    ItLosSem043();
#endif
    ItLosSem042();
    ItLosSem044(); // ID will not be duplicate
#endif

#if defined(LOSCFG_TEST_LLT)
    ItLosSem004();
    ItLosSem007();
    ItLosSem008();
    ItLosSem010();
    ItLosSem011();
    ItLosSem024();
    ItLosSem030();
    ItLosSem032();
#if (LOSCFG_KERNEL_SMP != YES)
    ItLosSem033();
#endif
    LltLosSem001();
#if defined(LOSCFG_SHELL) && defined(LOSCFG_DEBUG_SEMAPHORE)
    ItLosSemDebug001();
#endif
#endif
#if defined(LOSCFG_TEST_MANUAL_SHELL) || defined(LOSCFG_TEST_MANUAL_TEST)
#if defined(LOSCFG_DEBUG_SEMAPHORE)
    ItLosSem045();
    ItLosSem046();
    ItLosSem047();
    ItLosSem048();
    ItLosSem049();
    ItLosSem050();
#endif
#endif

#if (LOSCFG_KERNEL_SMP == YES)
    HalIrqSetAffinity(HWI_NUM_TEST, 1);
#endif
}
#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */
