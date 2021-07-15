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


#include "It_los_queue.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cpluscplus */
#endif /* __cpluscplus */

VOID ItSuiteLosQueue(VOID)
{
#ifdef LOSCFG_KERNEL_SMP
    ItSmpLosQueue001();
    ItSmpLosQueue002();
    ItSmpLosQueue003();
    ItSmpLosQueue004();
    ItSmpLosQueue005();
    ItSmpLosQueue006();
    ItSmpLosQueue007();
    ItSmpLosQueue008();
    ItSmpLosQueue009();
    ItSmpLosQueue010();
    ItSmpLosQueue011();
    ItSmpLosQueue012();
    ItSmpLosQueue013();
    ItSmpLosQueue014();
    ItSmpLosQueue015();
    ItSmpLosQueue016();
    ItSmpLosQueue017();
    ItSmpLosQueue018();
    ItSmpLosQueue019();
    ItSmpLosQueue020();
    ItSmpLosQueue021();
    ItSmpLosQueue022();
    ItSmpLosQueue023();
    ItSmpLosQueue024();
    ItSmpLosQueue025();
    ItSmpLosQueue026();
    ItSmpLosQueue027();
    ItSmpLosQueue029();
    ItSmpLosQueue031();
    ItSmpLosQueue032();
#endif
#if defined(LOSCFG_TEST_SMOKE)
    ItLosQueue001();
    ItLosQueue097();
#if (LOS_MEM_TLSF == YES)
#else
    ItLosQueue100();
    ItLosQueue105();
#endif
    ItLosQueueHead002();
#endif

#if defined(LOSCFG_TEST_FULL)
    ItLosQueue002();
    ItLosQueue003();
    ItLosQueue005();
    ItLosQueue006();
    ItLosQueue007();
    ItLosQueue008();
    ItLosQueue009();
    ItLosQueue010();
    ItLosQueue011();
    ItLosQueue012();
    ItLosQueue013();
    ItLosQueue014();
    ItLosQueue015();
    ItLosQueue017();
    ItLosQueue018();
    ItLosQueue019();
    ItLosQueue020();
#ifndef LOSCFG_KERNEL_SMP
    ItLosQueue021();
#endif
    ItLosQueue022();
    ItLosQueue023();
    ItLosQueue024();
    ItLosQueue025();
    ItLosQueue026();
    ItLosQueue027();
    ItLosQueue028();
    ItLosQueue029();
    ItLosQueue032();
    ItLosQueue033();
    ItLosQueue037();
    ItLosQueue038();
    ItLosQueue040();
    ItLosQueue041();
    ItLosQueue042();
    ItLosQueue043();
    ItLosQueue044();
    ItLosQueue045();
    ItLosQueue046();
    ItLosQueue047();
    ItLosQueue048();
    ItLosQueue049();
    ItLosQueue050();
    ItLosQueue051();
    ItLosQueue052();
    ItLosQueue053();
    ItLosQueue054();
    ItLosQueue055();
    ItLosQueue056();
    ItLosQueue057();
    ItLosQueue058();
    ItLosQueue059();
    ItLosQueue061();
    ItLosQueue062();
    ItLosQueue064();
    ItLosQueue065();
    ItLosQueue066();
    ItLosQueue067();
    ItLosQueue068();
    ItLosQueue069();
    ItLosQueue070();
    ItLosQueue071();
    ItLosQueue072();
    ItLosQueue073();
    ItLosQueue074();
    ItLosQueue075();
    ItLosQueue076();
    ItLosQueue077();
    ItLosQueue078();
    ItLosQueue079();
    ItLosQueue080();
    ItLosQueue081();
    ItLosQueue082();
    ItLosQueue083();
    ItLosQueue084();
    ItLosQueue085();
    ItLosQueue086();
    ItLosQueue087();
    ItLosQueue088();
    ItLosQueue089();
    ItLosQueue091();
    ItLosQueue092();
    ItLosQueue093();
    ItLosQueue094();
    ItLosQueue095();
    ItLosQueue096();
    ItLosQueue098();

#if (LOS_MEM_TLSF == YES)
#else
    ItLosQueue099();
    ItLosQueue101();
    ItLosQueue102();
    ItLosQueue103();
    ItLosQueue104();
    ItLosQueue106();
    ItLosQueue107();
    ItLosQueue108();
    ItLosQueue109();
    ItLosQueue110();
    ItLosQueue111();
    ItLosQueue112();
    ItLosQueue113();
    ItLosQueue114();
    ItLosQueue116();
#endif

    ItLosQueueHead003();
    ItLosQueueHead004();
    ItLosQueueHead005();
    ItLosQueueHead006();
    ItLosQueueHead007();
    ItLosQueueHead008();
    ItLosQueueHead009();
    ItLosQueueHead010();
    ItLosQueueHead011();
    ItLosQueueHead012();
    ItLosQueueHead013();
    ItLosQueueHead014();
    ItLosQueueHead015();
    ItLosQueueHead016();
    ItLosQueueHead017();
    ItLosQueueHead018();
    ItLosQueueHead019();
    ItLosQueueHead020();
    ItLosQueueHead021();
    ItLosQueueHead022();
    ItLosQueueHead023();
    ItLosQueueHead024();
    ItLosQueueHead025();
    ItLosQueueHead026();
    ItLosQueueHead027();
    ItLosQueueHead028();
    ItLosQueueHead029();
    ItLosQueueHead030();
    ItLosQueueHead031();
    ItLosQueueHead032();
    ItLosQueueHead038();
    ItLosQueueHead039();
    ItLosQueueHead040();
    ItLosQueueHead041();
    ItLosQueueHead042();
#endif

#ifdef LOSCFG_KERNEL_SMP
    HalIrqSetAffinity(HWI_NUM_TEST, 1);
#endif
}
#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */

