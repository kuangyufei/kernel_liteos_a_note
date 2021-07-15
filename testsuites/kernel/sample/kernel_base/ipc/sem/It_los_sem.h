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

#ifndef _IT_LOS_SEM_H
#define _IT_LOS_SEM_H

#include "osTest.h"
#include "los_sem_pri.h"
#include "los_task_pri.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cpluscplus */
#endif /* __cpluscplus */

extern volatile UINT64 g_tickCount[];
extern VOID ItSuiteLosSem(void);
#define LOOP 100

#ifndef spin_lock
#define spin_lock(lock) \
    do {                \
        LOS_TaskLock(); \
    } while (0)
#endif
#ifndef spin_unlock
#define spin_unlock(lock) \
    do {                  \
        LOS_TaskUnlock(); \
    } while (0)
#endif

#if defined(LOSCFG_TEST_SMOKE)
VOID ItLosSem001(void);
VOID ItLosSem002(void);
VOID ItLosSem003(void);
#endif

#if defined(LOSCFG_TEST_FULL)
VOID ItLosSem005(void);
VOID ItLosSem006(void);
VOID ItLosSem009(void);
VOID ItLosSem012(void);
VOID ItLosSem013(void);
VOID ItLosSem014(void);
VOID ItLosSem015(void);
VOID ItLosSem016(void);
VOID ItLosSem017(void);
VOID ItLosSem018(void);
VOID ItLosSem019(void);
VOID ItLosSem020(void);
VOID ItLosSem021(void);
VOID ItLosSem022(void);
VOID ItLosSem023(void);
VOID ItLosSem025(void);
VOID ItLosSem026(void);
VOID ItLosSem027(void);
VOID ItLosSem028(void);
VOID ItLosSem029(void);
VOID ItLosSem034(void);
#endif

#if defined(LOSCFG_TEST_PRESSURE)
VOID ItLosSem035(void);
VOID ItLosSem036(void);
VOID ItLosSem037(void);
VOID ItLosSem038(void);
VOID ItLosSem039(void);
VOID ItLosSem040(void);
VOID ItLosSem041(void);
VOID ItLosSem042(void);
VOID ItLosSem043(void);
VOID ItLosSem044(void);
#endif

#ifdef LOSCFG_KERNEL_SMP
VOID ItSmpLosSem001(VOID);
VOID ItSmpLosSem002(VOID);
VOID ItSmpLosSem003(VOID);
VOID ItSmpLosSem004(VOID);
VOID ItSmpLosSem005(VOID);
VOID ItSmpLosSem006(VOID);
VOID ItSmpLosSem007(VOID);
VOID ItSmpLosSem008(VOID);
VOID ItSmpLosSem009(VOID);
VOID ItSmpLosSem010(VOID);
VOID ItSmpLosSem011(VOID);
VOID ItSmpLosSem012(VOID);
VOID ItSmpLosSem013(VOID);
VOID ItSmpLosSem014(VOID);
VOID ItSmpLosSem015(VOID);
VOID ItSmpLosSem016(VOID);
VOID ItSmpLosSem017(VOID);
VOID ItSmpLosSem018(VOID);
VOID ItSmpLosSem019(VOID);
VOID ItSmpLosSem020(VOID);
VOID ItSmpLosSem021(VOID);
VOID ItSmpLosSem022(VOID);
VOID ItSmpLosSem023(VOID);
VOID ItSmpLosSem024(VOID);
VOID ItSmpLosSem025(VOID);
VOID ItSmpLosSem026(VOID);
VOID ItSmpLosSem027(VOID);
VOID ItSmpLosSem028(VOID);
VOID ItSmpLosSem029(VOID);
VOID ItSmpLosSem030(VOID);
VOID ItSmpLosSem031(VOID);
VOID ItSmpLosSem032(VOID);
VOID ItSmpLosSem033(VOID);
VOID ItSmpLosSem034(VOID);
VOID ItSmpLosSem035(VOID);
VOID ItSmpLosSem036(VOID);
#endif

VOID ItSuiteLosSem(void);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */

#endif /* _IT_LOS_SEM_H */
