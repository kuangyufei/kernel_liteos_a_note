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

#ifndef _IT_LOS_MUX_H
#define _IT_LOS_MUX_H

#include "osTest.h"
#include "los_mux_pri.h"
#include "los_config.h"
#include "los_bitmap.h"
#include "los_sem_pri.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cpluscplus */
#endif /* __cpluscplus */

extern LosMux g_mutexTest1;
extern LosMux g_mutexTest2;
extern LosMux g_mutexTest3;
extern VOID ItSuiteLosMux(void);

#define LOOP 100

#define LOS_ERRNO_MUX_INVALID EINVAL
#define LOS_ERRNO_MUX_PTR_NULL EINVAL
#define LOS_ERRNO_MUX_PENDED EBUSY
#define LOS_ERRNO_MUX_UNAVAILABLE EBUSY
#define LOS_ERRNO_MUX_TIMEOUT ETIMEDOUT
#define LOS_ERRNO_MUX_ALL_BUSY EAGAIN
#define LOS_ERRNO_MUX_PEND_IN_LOCK EDEADLK
#define LOS_ERRNO_MUX_PEND_INTERR EINTR
u_long TRand(void);
void ShowMuxId(UINT32 *p, int len);
void RandSZ(UINT32 *sz, int len);
void WaitAllTasksFinish(UINT32 *szTaskID, int len);
bool CheckAllTasksStatus(UINT32 *szTaskID, int len, const char *supposedStatus);
UINT8 *T_osShellConvertTskStatus(UINT16 taskStatus);
#if defined(LOSCFG_TEST_SMOKE)
VOID ItLosMux001(void);
VOID ItLosMux002(void);
VOID ItLosMux003(void);
VOID ItLosMux004(void);
#endif

#if defined(LOSCFG_TEST_FULL)
VOID ItLosMux006(void);
VOID ItLosMux007(void);
VOID ItLosMux008(void);
VOID ItLosMux009(void);
VOID ItLosMux010(void);
VOID ItLosMux011(void);
VOID ItLosMux012(void);
VOID ItLosMux013(void);
VOID ItLosMux015(void);
VOID ItLosMux016(void);
VOID ItLosMux017(void);
VOID ItLosMux018(void);
VOID ItLosMux020(void);
VOID ItLosMux021(void);
VOID ItLosMux025(void);
VOID ItLosMux026(void);
VOID ItLosMux027(void);
VOID ItLosMux028(void);
VOID ItLosMux029(void);
VOID ItLosMux031(void);
VOID ItLosMux035(void);
VOID ItLosMux036(void);
VOID ItLosMux037(void);
VOID ItLosMux038(void);
VOID ItLosMux039(void);
VOID ItLosMux040(void);
VOID ItLosMux041(void);
VOID ItLosMux042(void);
VOID ItLosMux043(void);
#endif

#if defined(LOSCFG_TEST_SMP)
VOID ItSmpLosMux001(void);
VOID ItSmpLosMux002(void);
VOID ItSmpLosMux003(void);
VOID ItSmpLosMux004(void);
VOID ItSmpLosMux005(void);
VOID ItSmpLosMux006(void);
VOID ItSmpLosMux007(void);
VOID ItSmpLosMux2001(void);
VOID ItSmpLosMux2002(void);
VOID ItSmpLosMux2003(void);
VOID ItSmpLosMux2004(void);
VOID ItSmpLosMux2005(void);
VOID ItSmpLosMux2006(void);
VOID ItSmpLosMux2007(void);
VOID ItSmpLosMux2008(void);
VOID ItSmpLosMux2009(void);
VOID ItSmpLosMux2010(void);
VOID ItSmpLosMux2011(void);
VOID ItSmpLosMux2012(void);
VOID ItSmpLosMux2013(void);
VOID ItSmpLosMux2014(void);
VOID ItSmpLosMux2015(void);
VOID ItSmpLosMux2016(void);
VOID ItSmpLosMux2017(void);
VOID ItSmpLosMux2018(void);
VOID ItSmpLosMux2019(void);
VOID ItSmpLosMux2020(void);
VOID ItSmpLosMux2021(void);
VOID ItSmpLosMux2022(void);
VOID ItSmpLosMux2023(void);
VOID ItSmpLosMux2024(void);
VOID ItSmpLosMux2025(void);
VOID ItSmpLosMux2026(void);
VOID ItSmpLosMux2027(void);
VOID ItSmpLosMux2028(void);
VOID ItSmpLosMux2029(void);
#endif

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */

#endif /* _IT_LOS_MUX_H */