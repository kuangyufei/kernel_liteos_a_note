/*
 * Copyright (c) 2013-2019 Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020-2022 Huawei Device Co., Ltd. All rights reserved.
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

#define LOSCFG_TEST_UNSOLVED 1
volatile UINT64 g_itTimesliceTestCount1 = 0;
volatile INT32 g_timesliceTestCount = 0;

void ItSuiteLosTask(void)
{
#if defined(LOSCFG_TEST_SMOKE)
    ItLosTask045();
    ItLosTask046();
    ItLosTask089();
    ItLosTask097();
    ItLosTask101();
    ItLosTask099();
    ItLosTaskTimeslice001();
#ifdef LOSCFG_KERNEL_SMP
    ItSmpLosTask001(); /* Task Affinity */
    ItSmpLosTask002(); /* Task Deletion Across Cores */
    ItSmpLosTask003(); /* Task Suspend Across Cores */
    ItSmpLosTask004(); /* Task Created Unbinded */
#endif
#endif

#if defined(LOSCFG_TEST_FULL)
    // fixed
    ItSmpLosTask099();
    ItSmpLosTask049();
    ItSmpLosTask130();
    ItSmpLosTask159();
    ItSmpLosTask161();
    ItSmpLosTask137();
    ItSmpLosTask021();
    ItSmpLosTask022();
    ItSmpLosTask023();
    ItSmpLosTask025();
    ItSmpLosTask026();
    ItSmpLosTask027();
    ItSmpLosTask028();
    ItSmpLosTask029();
    ItSmpLosTask030();
    ItSmpLosTask032();
    ItSmpLosTask033();
    ItSmpLosTask034();
    ItSmpLosTask035();
    ItSmpLosTask036();
    ItSmpLosTask037();
    ItSmpLosTask040();
    ItSmpLosTask042();
    ItSmpLosTask043();
    ItSmpLosTask044();
    ItSmpLosTask046();
    ItSmpLosTask047();
    ItSmpLosTask048();
    ItSmpLosTask050();
    ItSmpLosTask052();
    ItSmpLosTask053();
    ItSmpLosTask054();
    ItSmpLosTask055();
    ItSmpLosTask056();
    ItSmpLosTask057();
    ItSmpLosTask058();
    ItSmpLosTask059();
    ItSmpLosTask060();
    ItSmpLosTask061();
    ItSmpLosTask062();
    ItSmpLosTask063();
    ItSmpLosTask064();
    ItSmpLosTask065();
    ItSmpLosTask066();
    ItSmpLosTask067();
    ItSmpLosTask068();
    ItSmpLosTask069();
    ItSmpLosTask070();
    ItSmpLosTask071();
    ItSmpLosTask073();
    ItSmpLosTask074();
    ItSmpLosTask076();
    ItSmpLosTask077();
    ItSmpLosTask078();
    ItSmpLosTask079();
    ItSmpLosTask081();
    ItSmpLosTask084();
    ItSmpLosTask087();
    ItSmpLosTask088();
    ItSmpLosTask089();
    ItSmpLosTask090();
    ItSmpLosTask091();
    ItSmpLosTask092();
    ItSmpLosTask093();
    ItSmpLosTask094();
    ItSmpLosTask095();
    ItSmpLosTask096();
    ItSmpLosTask098();
    ItSmpLosTask100();
    ItSmpLosTask101();
    ItSmpLosTask102();
    ItSmpLosTask103();
    ItSmpLosTask105();
    ItSmpLosTask106();
    ItSmpLosTask107();
    ItSmpLosTask108();
    ItSmpLosTask109();
    ItSmpLosTask110();
    ItSmpLosTask112();
    ItSmpLosTask114();
    ItSmpLosTask115();
#ifndef LOSCFG_KERNEL_SMP_TASK_SYNC
    ItSmpLosTask117();
#endif
    ItSmpLosTask126();
    ItSmpLosTask127();
    ItSmpLosTask128();
    ItSmpLosTask129();
    ItSmpLosTask131();
    ItSmpLosTask132();
    ItSmpLosTask133();
    ItSmpLosTask134();
    ItSmpLosTask135();
    ItSmpLosTask136();
    ItSmpLosTask138();
    ItSmpLosTask139();
    ItSmpLosTask141();
    ItSmpLosTask142();
    ItSmpLosTask143();
    ItSmpLosTask144();
    ItSmpLosTask145();
    ItSmpLosTask146();
    ItSmpLosTask147();
    ItSmpLosTask148();
    ItSmpLosTask149();
    ItSmpLosTask150();
    ItSmpLosTask151();
    ItSmpLosTask152();
    ItSmpLosTask153();
    ItSmpLosTask154();
    ItSmpLosTask155();
    ItSmpLosTask156();
    ItSmpLosTask157();
#endif
}
#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */
