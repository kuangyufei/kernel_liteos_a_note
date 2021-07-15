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

#ifndef IT_LOS_EVENT_H
#define IT_LOS_EVENT_H

#include "osTest.h"
#include "los_base_pri.h"
#include "los_task_pri.h"
#include "los_tick_pri.h"

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
#define TASK_LOOP_NUM 0x100000

#define TASK_NAME_NUM 10
#define IT_TASK_LOOP 20

extern UINT32 g_testTaskID01;
extern UINT32 g_testTaskID02;
extern UINT32 g_testTaskID03;
extern UINT32 g_testQueueID01;
extern UINT32 g_TestTskHandle;
extern UINT32 g_swTmrMaxNum;

extern UINT16 usSwTmrID1;
extern UINT16 usSwTmrID2;
extern UINT16 usSwTmrID3;

extern UINT32 SwtmrCountA;
extern UINT32 SwtmrCountB;
extern UINT32 SwtmrCountC;
extern UINT64 CpuTickCountA;
extern UINT64 CpuTickCountB;

typedef struct tagHwTmrTCR {
    UINT16 rsvd1 : 4; // 3:0   Reserved
    UINT16 TSS : 1;   // 4     Timer Start/Stop
    UINT16 TRB : 1;   // 5     Timer reload
    UINT16 rsvd2 : 4; // 9:6   Reserved
    UINT16 SOFT : 1;  // 10    Emulation modes
    UINT16 FREE : 1;  // 11
    UINT16 rsvd3 : 2; // 12:13 Reserved
    UINT16 TIE : 1;   // 14    Output enable
    UINT16 TIF : 1;   // 15    Interrupt flag
} HWTMR_TCR;

typedef struct tagHwTmr {
    volatile UINT32 vuwTIM;
    volatile UINT32 vuwPRD;
    volatile HWTMR_TCR stTCR;
    volatile UINT16 vusReserved;
    volatile UINT16 vusTPR;
    volatile UINT16 vusTPRH;
} OS_HWTMR_S;

extern OS_HWTMR_S *g_pstHwTmrMap;
extern VOID LOS_GetCpuTick(UINT32 *puwCntHi, UINT32 *puwCntLo);
extern VOID ItSuiteLosEvent(VOID);

#if defined(LOSCFG_TEST_SMOKE)
extern VOID ItLosEvent031(VOID);
extern VOID ItLosEvent035(VOID);
extern VOID ItLosEvent036(VOID);
extern VOID ItLosEvent041(VOID);
#endif

#if defined(LOSCFG_TEST_FULL)
extern VOID ItLosEvent001(VOID);
extern VOID ItLosEvent002(VOID);
extern VOID ItLosEvent003(VOID);
extern VOID ItLosEvent004(VOID);
extern VOID ItLosEvent005(VOID);
extern VOID ItLosEvent006(VOID);
extern VOID ItLosEvent007(VOID);
extern VOID ItLosEvent008(VOID);
extern VOID ItLosEvent009(VOID);
extern VOID ItLosEvent010(VOID);
extern VOID ItLosEvent011(VOID);
extern VOID ItLosEvent012(VOID);
extern VOID ItLosEvent013(VOID);
extern VOID ItLosEvent014(VOID);
extern VOID ItLosEvent015(VOID);
extern VOID ItLosEvent016(VOID);
extern VOID ItLosEvent018(VOID);
extern VOID ItLosEvent019(VOID);
extern VOID ItLosEvent020(VOID);
extern VOID ItLosEvent021(VOID);
extern VOID ItLosEvent022(VOID);
extern VOID ItLosEvent023(VOID);
extern VOID ItLosEvent024(VOID);
extern VOID ItLosEvent025(VOID);
extern VOID ItLosEvent026(VOID);
extern VOID ItLosEvent027(VOID);
extern VOID ItLosEvent029(VOID);
extern VOID ItLosEvent030(VOID);
extern VOID ItLosEvent032(VOID);
extern VOID ItLosEvent033(VOID);
extern VOID ItLosEvent037(VOID);
extern VOID ItLosEvent038(VOID);
extern VOID ItLosEvent039(VOID);
extern VOID ItLosEvent040(VOID);
extern VOID ItLosEvent042(VOID);
extern VOID ItLosEvent043(VOID);
#endif

#if defined(LOSCFG_TEST_PRESSURE)
extern VOID ItLosEvent028(VOID);
#endif
#if defined(LOSCFG_TEST_LLT)
extern VOID LltLosEvent001(VOID);
extern VOID ItLosEvent017(VOID);
extern VOID ItLosEvent034(VOID);
#endif

#ifdef LOSCFG_KERNEL_SMP
VOID ItSmpLosEvent001(VOID);
VOID ItSmpLosEvent002(VOID);
VOID ItSmpLosEvent003(VOID);
VOID ItSmpLosEvent004(VOID);
VOID ItSmpLosEvent005(VOID);
VOID ItSmpLosEvent006(VOID);
VOID ItSmpLosEvent007(VOID);
VOID ItSmpLosEvent008(VOID);
VOID ItSmpLosEvent009(VOID);
VOID ItSmpLosEvent010(VOID);
VOID ItSmpLosEvent011(VOID);
VOID ItSmpLosEvent012(VOID);
VOID ItSmpLosEvent013(VOID);
VOID ItSmpLosEvent014(VOID);
VOID ItSmpLosEvent015(VOID);
VOID ItSmpLosEvent016(VOID);
VOID ItSmpLosEvent017(VOID);
VOID ItSmpLosEvent018(VOID);
VOID ItSmpLosEvent019(VOID);
VOID ItSmpLosEvent020(VOID);
VOID ItSmpLosEvent021(VOID);
VOID ItSmpLosEvent022(VOID);
VOID ItSmpLosEvent023(VOID);
VOID ItSmpLosEvent024(VOID);
VOID ItSmpLosEvent025(VOID);
VOID ItSmpLosEvent026(VOID);
VOID ItSmpLosEvent027(VOID);
VOID ItSmpLosEvent028(VOID);
VOID ItSmpLosEvent029(VOID);
VOID ItSmpLosEvent030(VOID);
VOID ItSmpLosEvent031(VOID);
VOID ItSmpLosEvent032(VOID);
VOID ItSmpLosEvent033(VOID);
VOID ItSmpLosEvent034(VOID);
VOID ItSmpLosEvent035(VOID);
VOID ItSmpLosEvent036(VOID);
VOID ItSmpLosEvent037(VOID);
#endif

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */
#endif /* IT_LOS_EVENT_H */
