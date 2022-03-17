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

#include "It_los_swtmr.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cpluscplus */
#endif /* __cpluscplus */

EVENT_CB_S g_eventCB1, g_eventCB2, g_eventCB3;

VOID ItSuiteLosSwtmr(VOID)
{
#if defined(LOSCFG_TEST_SMOKE)
    ItLosSwtmr053();
    ItLosSwtmr058();
#endif
#ifdef LOSCFG_KERNEL_SMP
    ItSmpLosSwtmr001(); /* Concurrent Multi-core */
    ItSmpLosSwtmr002(); /* Stop Across Cores */
    ItSmpLosSwtmr003();
    ItSmpLosSwtmr004();
    ItSmpLosSwtmr005();
    ItSmpLosSwtmr006();
    ItSmpLosSwtmr007();
    ItSmpLosSwtmr008();
    ItSmpLosSwtmr009();
    ItSmpLosSwtmr010();
    ItSmpLosSwtmr011();
    ItSmpLosSwtmr012();
    ItSmpLosSwtmr013();
    ItSmpLosSwtmr014();
    ItSmpLosSwtmr015();
    ItSmpLosSwtmr016();
    ItSmpLosSwtmr017();
    ItSmpLosSwtmr018();
    ItSmpLosSwtmr019();
    ItSmpLosSwtmr020();
    ItSmpLosSwtmr021();
    ItSmpLosSwtmr023();
    ItSmpLosSwtmr024();
    ItSmpLosSwtmr026();
    ItSmpLosSwtmr027();
    ItSmpLosSwtmr028();
    ItSmpLosSwtmr029();
    ItSmpLosSwtmr030();
    ItSmpLosSwtmr031();
    ItSmpLosSwtmr032();
    ItSmpLosSwtmr034();
    ItSmpLosSwtmr035();
#endif

#if defined(LOSCFG_TEST_FULL)
    ItLosSwtmr001();
    ItLosSwtmr002();
    ItLosSwtmr003();
    ItLosSwtmr005();
    ItLosSwtmr006();
    ItLosSwtmr007();
    ItLosSwtmr008();
    ItLosSwtmr009();
    ItLosSwtmr010();
    ItLosSwtmr011();
    ItLosSwtmr012();
    ItLosSwtmr013();
    ItLosSwtmr014();
    ItLosSwtmr015();
    ItLosSwtmr016();
    ItLosSwtmr017();
    ItLosSwtmr018();
    ItLosSwtmr019();
    ItLosSwtmr020();
    ItLosSwtmr021();
    ItLosSwtmr030();
    ItLosSwtmr036();
    ItLosSwtmr037();
    ItLosSwtmr042();
    ItLosSwtmr044();
    ItLosSwtmr045();
    ItLosSwtmr046();
    ItLosSwtmr047();
    ItLosSwtmr048();
    ItLosSwtmr049();
    ItLosSwtmr050();
    ItLosSwtmr051();
    ItLosSwtmr052();
    ItLosSwtmr054();
    ItLosSwtmr055();
    ItLosSwtmr056();
    ItLosSwtmr057();
    ItLosSwtmr059();
    ItLosSwtmr061();
    ItLosSwtmr062();
    ItLosSwtmr063();
    ItLosSwtmr066();
    ItLosSwtmr067();
    ItLosSwtmr068();
    ItLosSwtmr069();
    ItLosSwtmr070();
    ItLosSwtmr071();
    ItLosSwtmr075();
    ItLosSwtmr076();
    ItLosSwtmr077();
    ItLosSwtmr078();
#endif

#if defined(LOSCFG_TEST_PRESSURE)
    ItLosSwtmr004();
    ItLosSwtmr024();
    ItLosSwtmr025();
    ItLosSwtmr026();
    ItLosSwtmr027();
    ItLosSwtmr028();
    ItLosSwtmr029();
    ItLosSwtmr031();
    ItLosSwtmr032();
    ItLosSwtmr034();
    ItLosSwtmr035();
    ItLosSwtmr038();
#endif

#if defined(LOSCFG_TEST_LLT)
    ItLosSwtmr073();
#if !defined(TESTPBXA9) && !defined(TESTVIRTA53) && !defined(TEST3516DV300) && !defined(TESTHI1980IMU)
    ItLosSwtmr072(); // SwtmrTimeGet
    ItLosSwtmr074(); // across cores
    ItLosSwtmr079(); // sched_clock_swtmr
#endif
    ItLosSwtmr080();
    ItLosSwtmr023();
#endif
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */

