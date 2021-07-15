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

#include "los_task_pri.h"
#include "It_los_mux.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cpluscplus */
#endif /* __cpluscplus */


LosMux g_mutexTest1;
LosMux g_mutexTest2;
LosMux g_mutexTest3;


u_long TRand()
{
    return TRandom();
}
void ShowMuxId(UINT32 *p, int len)
{
    int i;
    for (i = 0; i < len; i++) {
        dprintf("%d , ", p[i]);
    }
    dprintf("\n");
}
void RandSZ(UINT32 *sz, int len)
{
    int idx;
    for (idx = len - 1; idx > 1; idx--) { /* get the random index */
        int randIdx = TRand() % idx;
        UINT32 tempMuxID = sz[randIdx];
        sz[randIdx] = sz[idx];
        sz[idx] = tempMuxID;
    }
    return;
}
VOID ItSuiteLosMux(void)
{
#if defined(LOSCFG_TEST_SMOKE)
    ItLosMux001();
    ItLosMux002();
    ItLosMux003();
    ItLosMux004();
#endif
#if defined(LOSCFG_TEST_FULL)
    ItLosMux006();
    ItLosMux007();
    ItLosMux008();
    ItLosMux009();
    ItLosMux010();
    ItLosMux011();
    ItLosMux012();
    ItLosMux013();
    ItLosMux015();
    ItLosMux016();
    ItLosMux017();
    ItLosMux018();
    ItLosMux020();
    ItLosMux021();
    ItLosMux025();
    ItLosMux026();
    ItLosMux027();
    ItLosMux028();
    ItLosMux029();
    ItLosMux031();
    ItLosMux035();
    ItLosMux036();
    ItLosMux037();
    ItLosMux038();
    ItLosMux039();
    ItLosMux040();
    ItLosMux041();
    ItLosMux042();
    ItLosMux043();
#endif

#ifdef LOSCFG_KERNEL_SMP
    ItSmpLosMux001();
    ItSmpLosMux002();
    ItSmpLosMux003();
    ItSmpLosMux004();
    ItSmpLosMux005();
    ItSmpLosMux006();
    ItSmpLosMux007();
    ItSmpLosMux2001();
    ItSmpLosMux2002();
    ItSmpLosMux2003();
    ItSmpLosMux2004();
    ItSmpLosMux2005();
    ItSmpLosMux2006();
    ItSmpLosMux2007();
    ItSmpLosMux2008();
    ItSmpLosMux2009();
    ItSmpLosMux2010();
    ItSmpLosMux2011();
    ItSmpLosMux2012();
    ItSmpLosMux2013();
    ItSmpLosMux2014();
    ItSmpLosMux2015();
    ItSmpLosMux2016();
    ItSmpLosMux2017();
    ItSmpLosMux2018();
    ItSmpLosMux2021();
    ItSmpLosMux2022();
    ItSmpLosMux2024();
    ItSmpLosMux2025();
    ItSmpLosMux2026();
    ItSmpLosMux2027();
    ItSmpLosMux2028();
    ItSmpLosMux2029();
#endif

#ifdef LOSCFG_KERNEL_SMP
    HalIrqSetAffinity(HWI_NUM_TEST, 1);
    HalIrqSetAffinity(HWI_NUM_TEST1, 1);
#endif
}


#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */
