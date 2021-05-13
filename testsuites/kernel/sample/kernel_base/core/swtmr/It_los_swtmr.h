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

#ifndef IT_LOS_SWTMR_H
#define IT_LOS_SWTMR_H

#include "los_swtmr.h"
#include "osTest.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cpluscplus */
#endif /* __cpluscplus */

#define LOOP 100

#define SELF_DELETED 0
#define SYS_EXIST_SWTMR 1
#define SWTMR_LOOP_NUM 10
#define TEST_HWI_RUNTIME 0x100000

extern EVENT_CB_S g_eventCB1;
extern EVENT_CB_S g_eventCB2;
extern EVENT_CB_S g_eventCB3;

extern LITE_OS_SEC_BSS SWTMR_CTRL_S *m_pstSwtmrFreeList;

static UINT32 g_swTmrMaxNum;

static UINT16 g_swTmrID1;
static UINT16 g_swTmrID2;
static UINT16 g_swTmrID3;

static UINT32 g_swtmrCountA;
static UINT32 g_swtmrCountB;
static UINT32 g_swtmrCountC;
static UINT64 g_cpuTickCountA;
static UINT64 g_cpuTickCountB;

extern UINT32 OsSwTmrGetNextTimeout(VOID);
extern UINT32 OsSwtmrTimeGet(SWTMR_CTRL_S *swtmr);

extern VOID LOS_GetCpuTick(UINT32 *puwCntHi, UINT32 *puwCntLo);
extern VOID ItSuiteLosSwtmr(VOID);

#if defined(LOSCFG_KERNEL_SMP)
VOID ItSmpLosSwtmr001(VOID);
VOID ItSmpLosSwtmr002(VOID);
VOID ItSmpLosSwtmr003(VOID);
VOID ItSmpLosSwtmr004(VOID);
VOID ItSmpLosSwtmr005(VOID);
VOID ItSmpLosSwtmr006(VOID);
VOID ItSmpLosSwtmr007(VOID);
VOID ItSmpLosSwtmr008(VOID);
VOID ItSmpLosSwtmr009(VOID);
VOID ItSmpLosSwtmr010(VOID);
VOID ItSmpLosSwtmr011(VOID);
VOID ItSmpLosSwtmr012(VOID);
VOID ItSmpLosSwtmr013(VOID);
VOID ItSmpLosSwtmr014(VOID);
VOID ItSmpLosSwtmr015(VOID);
VOID ItSmpLosSwtmr016(VOID);
VOID ItSmpLosSwtmr017(VOID);
VOID ItSmpLosSwtmr018(VOID);
VOID ItSmpLosSwtmr019(VOID);
VOID ItSmpLosSwtmr020(VOID);
VOID ItSmpLosSwtmr021(VOID);
VOID ItSmpLosSwtmr022(VOID);
VOID ItSmpLosSwtmr023(VOID);
VOID ItSmpLosSwtmr024(VOID);
VOID ItSmpLosSwtmr025(VOID);
VOID ItSmpLosSwtmr026(VOID);
VOID ItSmpLosSwtmr027(VOID);
VOID ItSmpLosSwtmr028(VOID);
VOID ItSmpLosSwtmr029(VOID);
VOID ItSmpLosSwtmr030(VOID);
VOID ItSmpLosSwtmr031(VOID);
VOID ItSmpLosSwtmr032(VOID);
VOID ItSmpLosSwtmr033(VOID);
VOID ItSmpLosSwtmr034(VOID);
VOID ItSmpLosSwtmr035(VOID);
#endif

#if defined(LOSCFG_TEST_SMOKE)
VOID ItLosSwtmr053(VOID);
VOID ItLosSwtmr058(VOID);
#endif

#if defined(LOSCFG_TEST_FULL)
VOID ItLosSwtmr001(VOID);
VOID ItLosSwtmr002(VOID);
VOID ItLosSwtmr003(VOID);
VOID ItLosSwtmr005(VOID);
VOID ItLosSwtmr006(VOID);
VOID ItLosSwtmr007(VOID);
VOID ItLosSwtmr008(VOID);
VOID ItLosSwtmr009(VOID);
VOID ItLosSwtmr010(VOID);
VOID ItLosSwtmr011(VOID);
VOID ItLosSwtmr012(VOID);
VOID ItLosSwtmr013(VOID);
VOID ItLosSwtmr014(VOID);
VOID ItLosSwtmr015(VOID);
VOID ItLosSwtmr016(VOID);
VOID ItLosSwtmr017(VOID);
VOID ItLosSwtmr018(VOID);
VOID ItLosSwtmr019(VOID);
VOID ItLosSwtmr020(VOID);
VOID ItLosSwtmr021(VOID);
VOID ItLosSwtmr022(VOID);
VOID ItLosSwtmr030(VOID);
VOID ItLosSwtmr033(VOID);
VOID ItLosSwtmr036(VOID);
VOID ItLosSwtmr037(VOID);
VOID ItLosSwtmr038(VOID);
VOID ItLosSwtmr039(VOID);
VOID ItLosSwtmr040(VOID);
VOID ItLosSwtmr041(VOID);
VOID ItLosSwtmr042(VOID);
VOID ItLosSwtmr043(VOID);
VOID ItLosSwtmr044(VOID);
VOID ItLosSwtmr045(VOID);
VOID ItLosSwtmr046(VOID);
VOID ItLosSwtmr047(VOID);
VOID ItLosSwtmr048(VOID);
VOID ItLosSwtmr049(VOID);
VOID ItLosSwtmr050(VOID);
VOID ItLosSwtmr051(VOID);
VOID ItLosSwtmr052(VOID);
VOID ItLosSwtmr054(VOID);
VOID ItLosSwtmr055(VOID);
VOID ItLosSwtmr056(VOID);
VOID ItLosSwtmr057(VOID);
VOID ItLosSwtmr059(VOID);
VOID ItLosSwtmr060(VOID);
VOID ItLosSwtmr061(VOID);
VOID ItLosSwtmr062(VOID);
VOID ItLosSwtmr063(VOID);
VOID ItLosSwtmr064(VOID);
VOID ItLosSwtmr065(VOID);
VOID ItLosSwtmr066(VOID);
VOID ItLosSwtmr067(VOID);
VOID ItLosSwtmr068(VOID);
VOID ItLosSwtmr069(VOID);
VOID ItLosSwtmr070(VOID);
VOID ItLosSwtmr071(VOID);
VOID ItLosSwtmr075(VOID);
VOID ItLosSwtmr076(VOID);
VOID ItLosSwtmr077(VOID);
VOID ItLosSwtmr078(VOID);
#endif

#if defined(LOSCFG_TEST_PRESSURE)
VOID ItLosSwtmr004(VOID);
VOID ItLosSwtmr024(VOID);
VOID ItLosSwtmr025(VOID);
VOID ItLosSwtmr026(VOID);
VOID ItLosSwtmr027(VOID);
VOID ItLosSwtmr028(VOID);
VOID ItLosSwtmr029(VOID);
VOID ItLosSwtmr031(VOID);
VOID ItLosSwtmr032(VOID);
VOID ItLosSwtmr034(VOID);
VOID ItLosSwtmr035(VOID);
VOID ItLosSwtmr081(VOID);
VOID ItLosSwtmr082(VOID);
VOID ItLosSwtmr083(VOID);
#endif

#if defined(LOSCFG_TEST_LLT)
VOID ItLosSwtmr072(VOID);
VOID ItLosSwtmr073(VOID);
VOID ItLosSwtmr074(VOID);
VOID ItLosSwtmr079(VOID);
VOID ItLosSwtmr080(VOID);
VOID ItLosSwtmr023(VOID);
#endif

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */
#endif /* IT_LOS_SWTMR_H */