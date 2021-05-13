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

#include "osTest.h"
#include "los_hwi.h"
#include "gic_common.h"
#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cpluscplus */
#endif /* __cpluscplus */

#define HIGHT_PRIO_INTR     100
#define MIDDLE_PRIO_INTR    101
#define LOW_PRIO_INTR       102

#define SPI_HIGHT_PRIO      0
#define SPI_MIDDLE_PRIO     ((GIC_MAX_INTERRUPT_PREEMPTION_LEVEL - 2) << PRIORITY_SHIFT)
#define SPI_LOW_PRIO        MIN_INTERRUPT_PRIORITY

#define INTR_ENTRY          0
#define INTR_EXIT           1

extern UINT32 g_intrHandleEnd;
VOID RecordIntrTrace(INT32 irq, INT32 direction);
VOID ResetIntrTrace(VOID);
UINT32 CheckIntrTrace(UINT32 *expect, UINT32 num);
VOID ItLosHwiNest001(VOID);
VOID ItLosHwiNest002(VOID);
VOID ItLosHwiNest003(VOID);
VOID ItLosHwiNest004(VOID);
VOID ItLosHwiNest005(VOID);
VOID ItLosHwiNest006(VOID);
VOID ItLosHwiNest007(VOID);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */
