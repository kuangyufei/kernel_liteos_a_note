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

#define LOSCFG_TEST_UNSOLVED 1
volatile UINT64 g_itTimesliceTestCount1 = 0;
volatile INT32 g_timesliceTestCount = 0;

void ItSuiteLosTask(void)
{
    int ret = LOS_SetProcessScheduler(LOS_GetCurrProcessID(), LOS_SCHED_RR, TASK_PRIO_TEST_TASK);
    if (ret != LOS_OK) {
        dprintf("%s set test process schedule failed! %d\n", ret);
    }
    ret = LOS_SetTaskScheduler(LOS_CurTaskIDGet(), LOS_SCHED_RR, TASK_PRIO_TEST_TASK);
    if (ret != LOS_OK) {
        dprintf("%s set test task schedule failed! %d\n", ret);
    }
#if defined(LOSCFG_TEST_SMOKE)
    ItLosTask045();
    ItLosTask046();
    ItLosTask049();
    ItLosTask081();
    ItLosTask089();
    ItLosTask097();
    ItLosTask101();
    ItLosTask105();
    ItLosTask099();
    ItLosTaskTimeslice001();
#ifdef LOSCFG_KERNEL_SMP
    // reserved 20 for smoke test
    ItSmpLosTask001(); /* Task Affinity */
    ItSmpLosTask002(); /* Task Deletion Across Cores */
    ItSmpLosTask003(); /* Task Suspend Across Cores */
    ItSmpLosTask004(); /* Task Created Unbinded */
#endif
#endif

#if defined(LOSCFG_TEST_FULL)
    ItLosTask001();
    ItLosTask002();
    ItLosTask004();
    ItLosTask007();
    ItLosTask008();
    ItLosTask009();
    ItLosTask010();
    ItLosTask011();
    ItLosTask012();
    ItLosTask013();
    ItLosTask014();
    ItLosTask015();
    ItLosTask016();
    ItLosTask017();
    ItLosTask018();
    ItLosTask019();
    ItLosTask020();
    ItLosTask021();
    ItLosTask022();
    ItLosTask023();
    ItLosTask024();
    ItLosTask025();
    ItLosTask026();
    ItLosTask027();
    ItLosTask028();
    ItLosTask029();
    ItLosTask031();
    ItLosTask032();
    ItLosTask033();
    ItLosTask034();
    ItLosTask035();
    ItLosTask036();
    ItLosTask037();
    ItLosTask038();
    ItLosTask039();
#ifndef TESTHI1980IMU
    ItLosTask040();
#endif
    ItLosTask041();
    ItLosTask042();
    ItLosTask043();
    ItLosTask047();
    ItLosTask048();
    ItLosTask050();
    ItLosTask051();
    ItLosTask052();
    ItLosTask053();
    ItLosTask054();
    ItLosTask055();
    ItLosTask056();
    ItLosTask057();
    ItLosTask058();
    ItLosTask060();
    ItLosTask061();
    ItLosTask063();
    ItLosTask064();
    ItLosTask065();
    ItLosTask066();
    ItLosTask067();
    ItLosTask068();
    ItLosTask069();
    ItLosTask071();
    ItLosTask072();
    ItLosTask073();
    ItLosTask074();
#ifndef LOSCFG_KERNEL_SMP
    ItLosTask075();
#endif
    ItLosTask076();
    ItLosTask077();
    ItLosTask078();
    ItLosTask079();
    ItLosTask080(); // LOS_TaskYeild is not allowed in irq
    ItLosTask082();
    ItLosTask090();
    ItLosTask092();
    ItLosTask093();
    ItLosTask094();
    ItLosTask095();
    ItLosTask096();
    ItLosTask098();
    ItLosTask100();
    ItLosTask102();
    ItLosTask103();
    ItLosTask104();
    ItLosTask106();
    ItLosTask107();
    ItLosTask108();
    ItLosTask109();
    ItLosTask110();
    ItLosTask111();
    ItLosTask112();
    ItLosTask113();
    ItLosTask114();
    ItLosTask115();
    ItLosTask116(); // Not used the API LOS_CurTaskPriSet to change the task priority for the software timer
    ItLosTask119();
    ItLosTask120();
    ItLosTask121();
    ItLosTask122();
    ItLosTask123();
    ItLosTask124();
    ItLosTask125();
    ItLosTask126();
    ItLosTask127();
    ItLosTask128();
    ItLosTask129();
    ItLosTask130();
    ItLosTask131();
    ItLosTask132();
    ItLosTask133();
    ItLosTask134();
    ItLosTask135();
    ItLosTask136();
#ifndef LOSCFG_ARCH_FPU_DISABLE
    ItLosTask141(); /* fpu regs check in task switch */
    ItLosTask142(); /* fpu regs check in irq context switch */
#endif
    ItLosTaskTimeslice002();
    ItLosTaskTimeslice003();
    ItLosTaskTimeslice004();
#ifdef LOSCFG_KERNEL_SMP
#ifndef LOSCFG_ARCH_FPU_DISABLE

    ItSmpLosFloatSwitch001();
    ItSmpLosFloatSwitch002();
    ItSmpLosFloatSwitch003();
    ItSmpLosFloatSwitch004();
    ItSmpLosFloatSwitch005();
    ItSmpLosFloatSwitch006();
    ItSmpLosFloatSwitch007();
#endif
    // fixed
    ItSmpLosTask099();
    ItSmpLosTask049();
    ItSmpLosTask130();
    ItSmpLosTask159();
    ItSmpLosTask161();
    ItSmpLosTask137();
    ItSmpLosTask158();
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
    ItSmpLosTask051();
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
    ItSmpLosTask082();
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
    ItSmpLosTask097();
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
#endif

#ifdef LOSCFG_KERNEL_SMP
    HalIrqSetAffinity(HWI_NUM_TEST, 1);
    HalIrqSetAffinity(HWI_NUM_TEST1, 1);
    HalIrqSetAffinity(HWI_NUM_TEST3, 1);
#endif
    ret = LOS_SetProcessScheduler(LOS_GetCurrProcessID(), LOS_SCHED_RR, 20); // 20, set a reasonable priority.
    if (ret != LOS_OK) {
        dprintf("%s set test process schedule failed! %d\n", ret);
    }
    ret = LOS_SetTaskScheduler(LOS_CurTaskIDGet(), LOS_SCHED_RR, TASK_PRIO_TEST);
    if (ret != LOS_OK) {
        dprintf("%s set test task schedule failed! %d\n", ret);
    }
}
#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */
