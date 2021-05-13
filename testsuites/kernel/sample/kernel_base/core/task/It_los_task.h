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

#ifndef IT_LOS_TASK_H
#define IT_LOS_TASK_H

#include "osTest.h"
#include "los_base_pri.h"
#include "los_task_pri.h"
#include "los_tick_pri.h"
#include "los_list.h"
#include "los_process_pri.h"
#include "los_percpu_pri.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cpluscplus */
#endif /* __cpluscplus */

#define SELF_DELETED 0
#define SYS_EXIST_SWTMR 1
#define SWTMR_LOOP_NUM 10
#define TEST_HWI_RUNTIME 0x100000
#define TASK_LOOP_NUM 0x100000
#if (YES == LOSCFG_BASE_CORE_SWTMR)
#define TASK_EXISTED_NUM 4
#else
#define TASK_EXISTED_NUM 2
#endif

#define TASK_EXISTED_D_NUM TASK_EXISTED_NUM
#define TASK_NAME_NUM 10
#define IT_TASK_LOOP 20
#define IT_TASK_SMP_LOOP 100

extern EVENT_CB_S g_eventCB;
extern EVENT_CB_S g_eventCB1;
extern EVENT_CB_S g_eventCB2;
extern EVENT_CB_S g_eventCB3;

extern UINT32 g_testTaskID01;
extern UINT32 g_testTaskID02;
extern UINT32 g_testTaskID03;
extern UINT32 g_testTskHandle;
extern UINT32 g_swTmrMaxNum;

extern UINT8 g_mUsIndex;
extern UINT16 g_swTmrID1;
extern UINT16 g_swTmrID2;
extern UINT16 g_swTmrID3;

extern UINT32 g_swtmrCountA;
extern UINT32 g_swtmrCountB;
extern UINT32 g_swtmrCountC;
extern UINT64 g_cpuTickCountA;
extern UINT64 g_cpuTickCountB;
extern UINT32 g_leavingTaskNum;

extern UINT32 g_mAuwTestTaskId[32];

extern volatile UINT64 g_itTimesliceTestCount1;
extern volatile INT32 g_timesliceTestCount;

extern UINT32 OsPriQueueTotalSize(void);
extern void ItSuiteLosTask(void);


#if defined(LOSCFG_TEST_SMP)
void ItSmpLosTask001(void);
void ItSmpLosTask002(void);
void ItSmpLosTask003(void);
void ItSmpLosTask004(void);
void ItSmpLosTask021(void);
void ItSmpLosTask022(void);
void ItSmpLosTask023(void);
void ItSmpLosTask025(void);
void ItSmpLosTask026(void);
void ItSmpLosTask027(void);
void ItSmpLosTask028(void);
void ItSmpLosTask029(void);
void ItSmpLosTask030(void);
void ItSmpLosTask031(void);
void ItSmpLosTask032(void);
void ItSmpLosTask033(void);
void ItSmpLosTask034(void);
void ItSmpLosTask035(void);
void ItSmpLosTask036(void);
void ItSmpLosTask037(void);
void ItSmpLosTask038(void);
void ItSmpLosTask039(void);
void ItSmpLosTask040(void);
void ItSmpLosTask041(void);
void ItSmpLosTask042(void);
void ItSmpLosTask043(void);
void ItSmpLosTask044(void);
void ItSmpLosTask045(void);
void ItSmpLosTask046(void);
void ItSmpLosTask047(void);
void ItSmpLosTask048(void);
void ItSmpLosTask049(void);
void ItSmpLosTask050(void);
void ItSmpLosTask051(void);
void ItSmpLosTask052(void);
void ItSmpLosTask053(void);
void ItSmpLosTask054(void);
void ItSmpLosTask055(void);
void ItSmpLosTask056(void);
void ItSmpLosTask057(void);
void ItSmpLosTask058(void);
void ItSmpLosTask059(void);
void ItSmpLosTask060(void);
void ItSmpLosTask061(void);
void ItSmpLosTask062(void);
void ItSmpLosTask063(void);
void ItSmpLosTask064(void);
void ItSmpLosTask065(void);
void ItSmpLosTask066(void);
void ItSmpLosTask067(void);
void ItSmpLosTask068(void);
void ItSmpLosTask069(void);
void ItSmpLosTask070(void);
void ItSmpLosTask071(void);
void ItSmpLosTask073(void);
void ItSmpLosTask074(void);
void ItSmpLosTask076(void);
void ItSmpLosTask077(void);
void ItSmpLosTask078(void);
void ItSmpLosTask079(void);
void ItSmpLosTask081(void);
void ItSmpLosTask082(void);
void ItSmpLosTask084(void);
void ItSmpLosTask087(void);
void ItSmpLosTask088(void);
void ItSmpLosTask089(void);
void ItSmpLosTask090(void);
void ItSmpLosTask091(void);
void ItSmpLosTask092(void);
void ItSmpLosTask093(void);
void ItSmpLosTask094(void);
void ItSmpLosTask095(void);
void ItSmpLosTask096(void);
void ItSmpLosTask097(void);
void ItSmpLosTask098(void);
void ItSmpLosTask099(void);
void ItSmpLosTask100(void);
void ItSmpLosTask101(void);
void ItSmpLosTask102(void);
void ItSmpLosTask103(void);
void ItSmpLosTask104(void);
void ItSmpLosTask105(void);
void ItSmpLosTask106(void);
void ItSmpLosTask107(void);
void ItSmpLosTask108(void);
void ItSmpLosTask109(void);
void ItSmpLosTask110(void);
void ItSmpLosTask111(void);
void ItSmpLosTask112(void);
void ItSmpLosTask114(void);
void ItSmpLosTask115(void);
void ItSmpLosTask117(void);
void ItSmpLosTask126(void);
void ItSmpLosTask127(void);
void ItSmpLosTask128(void);
void ItSmpLosTask129(void);
void ItSmpLosTask130(void);
void ItSmpLosTask131(void);
void ItSmpLosTask132(void);
void ItSmpLosTask133(void);
void ItSmpLosTask134(void);
void ItSmpLosTask135(void);
void ItSmpLosTask136(void);
void ItSmpLosTask137(void);
void ItSmpLosTask138(void);
void ItSmpLosTask139(void);
void ItSmpLosTask141(void);
void ItSmpLosTask142(void);
void ItSmpLosTask143(void);
void ItSmpLosTask144(void);
void ItSmpLosTask145(void);
void ItSmpLosTask146(void);
void ItSmpLosTask147(void);
void ItSmpLosTask148(void);
void ItSmpLosTask149(void);
void ItSmpLosTask150(void);
void ItSmpLosTask151(void);
void ItSmpLosTask152(void);
void ItSmpLosTask153(void);
void ItSmpLosTask154(void);
void ItSmpLosTask155(void);
void ItSmpLosTask156(void);
void ItSmpLosTask157(void);
void ItSmpLosTask158(void);
void ItSmpLosTask159(void);
void ItSmpLosTask161(void);
#endif

#ifdef LOSCFG_TEST_SMOKE
void ItLosTask081(void);
#endif

#if defined(LOSCFG_TEST_SMOKE)
void ItLosTask045(void);
void ItLosTask046(void);
void ItLosTask049(void);
void ItLosTask081(void);
void ItLosTask089(void);
void ItLosTask097(void);
void ItLosTask099(void);
void ItLosTask101(void);
void ItLosTask105(void);
#endif

#if defined(LOSCFG_TEST_FULL)
void ItLosTask001(void);
void ItLosTask002(void);
void ItLosTask004(void);
void ItLosTask007(void);
void ItLosTask008(void);
void ItLosTask009(void);
void ItLosTask010(void);
void ItLosTask011(void);
void ItLosTask012(void);
void ItLosTask013(void);
void ItLosTask014(void);
void ItLosTask015(void);
void ItLosTask016(void);
void ItLosTask017(void);
void ItLosTask018(void);
void ItLosTask019(void);
void ItLosTask020(void);
void ItLosTask021(void);
void ItLosTask022(void);
void ItLosTask023(void);
void ItLosTask024(void);
void ItLosTask025(void);
void ItLosTask026(void);
void ItLosTask027(void);
void ItLosTask028(void);
void ItLosTask029(void);
void ItLosTask031(void);
void ItLosTask032(void);
void ItLosTask033(void);
void ItLosTask034(void);
void ItLosTask035(void);
void ItLosTask036(void);
void ItLosTask037(void);
void ItLosTask038(void);
void ItLosTask039(void);
void ItLosTask040(void);
void ItLosTask041(void);
void ItLosTask042(void);
void ItLosTask043(void);
void ItLosTask047(void);
void ItLosTask048(void);
void ItLosTask050(void);
void ItLosTask051(void);
void ItLosTask052(void);
void ItLosTask053(void);
void ItLosTask054(void);
void ItLosTask055(void);
void ItLosTask056(void);
void ItLosTask057(void);
void ItLosTask058(void);
void ItLosTask060(void);
void ItLosTask061(void);
void ItLosTask063(void);
void ItLosTask064(void);
void ItLosTask065(void);
void ItLosTask066(void);
void ItLosTask067(void);
void ItLosTask068(void);
void ItLosTask069(void);
void ItLosTask071(void);
void ItLosTask072(void);
void ItLosTask073(void);
void ItLosTask074(void);
void ItLosTask075(void);
void ItLosTask076(void);
void ItLosTask077(void);
void ItLosTask078(void);
void ItLosTask079(void);
void ItLosTask080(void);
void ItLosTask082(void);
void ItLosTask090(void);
void ItLosTask092(void);
void ItLosTask093(void);
void ItLosTask094(void);
void ItLosTask095(void);
void ItLosTask096(void);
void ItLosTask098(void);
void ItLosTask100(void);
void ItLosTask102(void);
void ItLosTask103(void);
void ItLosTask104(void);
void ItLosTask106(void);
void ItLosTask107(void);
void ItLosTask108(void);
void ItLosTask109(void);
void ItLosTask110(void);
void ItLosTask111(void);
void ItLosTask112(void);
void ItLosTask113(void);
void ItLosTask114(void);
void ItLosTask115(void);
void ItLosTask116(void);
void ItLosTask118(void);
void ItLosTask119(void);
void ItLosTask120(void);
void ItLosTask121(void);
void ItLosTask122(void);
void ItLosTask123(void);
void ItLosTask124(void);
void ItLosTask125(void);
void ItLosTask126(void);
void ItLosTask127(void);
void ItLosTask128(void);
void ItLosTask129(void);
void ItLosTask130(void);
void ItLosTask131(void);
void ItLosTask132(void);
void ItLosTask133(void);
void ItLosTask134(void);
void ItLosTask135(void);
void ItLosTask136(void);
void ItLosTask138(void);
void ItLosTaskTimeslice001(void);
void ItLosTaskTimeslice002(void);
void ItLosTaskTimeslice003(void);
void ItLosTaskTimeslice004(void);
#endif

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */

#endif