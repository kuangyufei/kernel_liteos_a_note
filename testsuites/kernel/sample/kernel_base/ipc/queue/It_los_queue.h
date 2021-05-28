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


#include "los_base.h"
#include "los_task.h"
#include "los_membox.h"
#include "osTest.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cpluscplus */
#endif /* __cpluscplus */

#define QUEUE_SHORT_BUFFER_LENGTH            12
#define QUEUE_STANDARD_BUFFER_LENGTH        50
#define QUEUE_BASE_NUM                        3
#define QUEUE_BASE_MSGSIZE                8

#define LOOP 100

#ifdef __LP64__
#define PER_ADDED_VALUE     8
#else
#define PER_ADDED_VALUE     4
#endif

extern UINT32 g_testTaskID01;
extern UINT32 g_testTaskID02;
extern UINT32 g_testQueueID01;
extern UINT32 g_testQueueID02;
extern UINT32 g_TestQueueID03;

extern VOID ItSuiteLosQueue(VOID);

#if defined(LOSCFG_TEST_SMOKE)
VOID ItLosQueue001(VOID);
VOID ItLosQueue097(VOID);
#if (LOS_MEM_TLSF == YES)
#else
VOID ItLosQueue100(VOID);
VOID ItLosQueue105(VOID);
#endif
VOID ItLosQueueHead002(VOID);
#endif

#if defined(LOSCFG_TEST_FULL)
VOID ItLosQueue002(VOID);
VOID ItLosQueue003(VOID);
VOID ItLosQueue004(VOID);
VOID ItLosQueue005(VOID);
VOID ItLosQueue006(VOID);
VOID ItLosQueue007(VOID);
VOID ItLosQueue008(VOID);
VOID ItLosQueue009(VOID);
VOID ItLosQueue010(VOID);
VOID ItLosQueue011(VOID);
VOID ItLosQueue012(VOID);
VOID ItLosQueue013(VOID);
VOID ItLosQueue014(VOID);
VOID ItLosQueue015(VOID);
VOID ItLosQueue017(VOID);
VOID ItLosQueue018(VOID);
VOID ItLosQueue019(VOID);
VOID ItLosQueue020(VOID);
VOID ItLosQueue021(VOID);
VOID ItLosQueue022(VOID);
VOID ItLosQueue023(VOID);
VOID ItLosQueue024(VOID);
VOID ItLosQueue025(VOID);
VOID ItLosQueue026(VOID);
VOID ItLosQueue027(VOID);
VOID ItLosQueue028(VOID);
VOID ItLosQueue029(VOID);
VOID ItLosQueue032(VOID);
VOID ItLosQueue033(VOID);
VOID ItLosQueue037(VOID);
VOID ItLosQueue038(VOID);
VOID ItLosQueue040(VOID);
VOID ItLosQueue041(VOID);
VOID ItLosQueue042(VOID);
VOID ItLosQueue043(VOID);
VOID ItLosQueue044(VOID);
VOID ItLosQueue045(VOID);
VOID ItLosQueue046(VOID);
VOID ItLosQueue047(VOID);
VOID ItLosQueue048(VOID);
VOID ItLosQueue049(VOID);
VOID ItLosQueue050(VOID);
VOID ItLosQueue051(VOID);
VOID ItLosQueue052(VOID);
VOID ItLosQueue053(VOID);
VOID ItLosQueue054(VOID);
VOID ItLosQueue055(VOID);
VOID ItLosQueue056(VOID);
VOID ItLosQueue057(VOID);
VOID ItLosQueue058(VOID);
VOID ItLosQueue059(VOID);
VOID ItLosQueue061(VOID);
VOID ItLosQueue062(VOID);
VOID ItLosQueue064(VOID);
VOID ItLosQueue065(VOID);
VOID ItLosQueue066(VOID);
VOID ItLosQueue067(VOID);
VOID ItLosQueue068(VOID);
VOID ItLosQueue069(VOID);
VOID ItLosQueue070(VOID);
VOID ItLosQueue071(VOID);
VOID ItLosQueue072(VOID);
VOID ItLosQueue073(VOID);
VOID ItLosQueue074(VOID);
VOID ItLosQueue075(VOID);
VOID ItLosQueue076(VOID);
VOID ItLosQueue077(VOID);
VOID ItLosQueue078(VOID);
VOID ItLosQueue079(VOID);
VOID ItLosQueue080(VOID);
VOID ItLosQueue081(VOID);
VOID ItLosQueue082(VOID);
VOID ItLosQueue083(VOID);
VOID ItLosQueue084(VOID);
VOID ItLosQueue085(VOID);
VOID ItLosQueue086(VOID);
VOID ItLosQueue087(VOID);
VOID ItLosQueue088(VOID);
VOID ItLosQueue089(VOID);
VOID ItLosQueue090(VOID);
VOID ItLosQueue091(VOID);
VOID ItLosQueue092(VOID);
VOID ItLosQueue093(VOID);
VOID ItLosQueue094(VOID);
VOID ItLosQueue095(VOID);
VOID ItLosQueue096(VOID);
VOID ItLosQueue098(VOID);
VOID ItLosQueue099(VOID);
VOID ItLosQueue101(VOID);
VOID ItLosQueue102(VOID);
VOID ItLosQueue103(VOID);
VOID ItLosQueue104(VOID);
VOID ItLosQueue106(VOID);
VOID ItLosQueue107(VOID);
VOID ItLosQueue108(VOID);
VOID ItLosQueue109(VOID);
VOID ItLosQueue110(VOID);
VOID ItLosQueue111(VOID);
VOID ItLosQueue112(VOID);
VOID ItLosQueue113(VOID);
VOID ItLosQueue114(VOID);
VOID ItLosQueue116(VOID);

VOID ItLosQueueHead003(VOID);
VOID ItLosQueueHead004(VOID);
VOID ItLosQueueHead005(VOID);
VOID ItLosQueueHead006(VOID);
VOID ItLosQueueHead007(VOID);
VOID ItLosQueueHead008(VOID);
VOID ItLosQueueHead009(VOID);
VOID ItLosQueueHead010(VOID);
VOID ItLosQueueHead011(VOID);
VOID ItLosQueueHead012(VOID);
VOID ItLosQueueHead013(VOID);
VOID ItLosQueueHead014(VOID);
VOID ItLosQueueHead015(VOID);
VOID ItLosQueueHead016(VOID);
VOID ItLosQueueHead017(VOID);
VOID ItLosQueueHead018(VOID);
VOID ItLosQueueHead019(VOID);
VOID ItLosQueueHead020(VOID);
VOID ItLosQueueHead021(VOID);
VOID ItLosQueueHead022(VOID);
VOID ItLosQueueHead023(VOID);
VOID ItLosQueueHead024(VOID);
VOID ItLosQueueHead025(VOID);
VOID ItLosQueueHead026(VOID);
VOID ItLosQueueHead027(VOID);
VOID ItLosQueueHead028(VOID);
VOID ItLosQueueHead029(VOID);
VOID ItLosQueueHead030(VOID);
VOID ItLosQueueHead031(VOID);
VOID ItLosQueueHead032(VOID);
VOID ItLosQueueHead036(VOID);
VOID ItLosQueueHead038(VOID);
VOID ItLosQueueHead039(VOID);
VOID ItLosQueueHead040(VOID);
VOID ItLosQueueHead041(VOID);
VOID ItLosQueueHead042(VOID);
#endif

#if defined(LOSCFG_TEST_MANUAL_SHELL) && defined(LOSCFG_DEBUG_QUEUE)
VOID ItLosQueueManual001(VOID);
VOID ItLosQueueManual002(VOID);
VOID ItLosQueueManual003(VOID);
VOID ItLosQueueManual004(VOID);
VOID ItLosQueueManual005(VOID);
VOID ItLosQueueManual006(VOID);
VOID ItLosQueueManual007(VOID);
VOID ItLosQueueManual008(VOID);
VOID ItLosQueueManual009(VOID);
VOID ItLosQueueManual010(VOID);
#endif
#if defined(LOSCFG_TEST_PRESSURE)
VOID ItLosQueue016(VOID);
VOID ItLosQueue030(VOID);
VOID ItLosQueue031(VOID);
VOID ItLosQueue034(VOID);
VOID ItLosQueue035(VOID);
VOID ItLosQueue036(VOID);
VOID ItLosQueue039(VOID);
VOID ItLosQueue060(VOID);
VOID ItLosQueue063(VOID);
VOID ItLosQueue123(VOID);


#endif

#if defined(LOSCFG_TEST_LLT)
VOID ItLosQueueHead001(VOID);
VOID ItLosQueue115(VOID);
VOID ItLosQueue117(VOID);
VOID ItLosQueue118(VOID);
VOID ItLosQueue119(VOID);
VOID ItLosQueue120(VOID);
VOID ItLosQueue121(VOID);
VOID ItLosQueue122(VOID);
#if defined(LOSCFG_SHELL) && defined(LOSCFG_DEBUG_QUEUE)
VOID ItLosQueueDebug001(void);
#endif
#endif

#if (LOSCFG_KERNEL_SMP == YES)
VOID ItSmpLosQueue001(VOID);
VOID ItSmpLosQueue002(VOID);
VOID ItSmpLosQueue003(VOID);
VOID ItSmpLosQueue004(VOID);
VOID ItSmpLosQueue005(VOID);
VOID ItSmpLosQueue006(VOID);
VOID ItSmpLosQueue007(VOID);
VOID ItSmpLosQueue008(VOID);
VOID ItSmpLosQueue009(VOID);
VOID ItSmpLosQueue010(VOID);
VOID ItSmpLosQueue011(VOID);
VOID ItSmpLosQueue012(VOID);
VOID ItSmpLosQueue013(VOID);
VOID ItSmpLosQueue014(VOID);
VOID ItSmpLosQueue015(VOID);
VOID ItSmpLosQueue016(VOID);
VOID ItSmpLosQueue017(VOID);
VOID ItSmpLosQueue018(VOID);
VOID ItSmpLosQueue019(VOID);
VOID ItSmpLosQueue020(VOID);
VOID ItSmpLosQueue021(VOID);
VOID ItSmpLosQueue022(VOID);
VOID ItSmpLosQueue023(VOID);
VOID ItSmpLosQueue024(VOID);
VOID ItSmpLosQueue025(VOID);
VOID ItSmpLosQueue026(VOID);
VOID ItSmpLosQueue027(VOID);
VOID ItSmpLosQueue028(VOID);
VOID ItSmpLosQueue029(VOID);
VOID ItSmpLosQueue030(VOID);
VOID ItSmpLosQueue031(VOID);
VOID ItSmpLosQueue032(VOID);
#endif

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */

