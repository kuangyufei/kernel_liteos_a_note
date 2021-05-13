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

#ifndef LOS_SMP_HWI_H
#define LOS_SMP_HWI_H

#include "los_hwi.h"
#include "osTest.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cpluscplus */
#endif /* __cpluscplus */

#define LOOP 100

#if defined(LOSCFG_TEST_SMP)
VOID ItSmpLosHwi001(VOID);
VOID ItSmpLosHwi002(VOID);
VOID ItSmpLosHwi003(VOID);
VOID ItSmpLosHwi004(VOID);
VOID ItSmpLosHwi005(VOID);
VOID ItSmpLosHwi006(VOID);
VOID ItSmpLosHwi007(VOID);
VOID ItSmpLosHwi008(VOID);
VOID ItSmpLosHwi009(VOID);
VOID ItSmpLosHwi010(VOID);
VOID ItSmpLosHwi011(VOID);
VOID ItSmpLosHwi012(VOID);
VOID ItSmpLosHwi013(VOID);

#ifndef LOSCFG_NO_SHARED_IRQ
VOID ItSmpLosHwiShare001(VOID);
VOID ItSmpLosHwiShare002(VOID);
VOID ItSmpLosHwiShare003(VOID);
VOID ItSmpLosHwiShare004(VOID);
VOID ItSmpLosHwiShare005(VOID);
VOID ItSmpLosHwiShare006(VOID);
VOID ItSmpLosHwiShare007(VOID);
VOID ItSmpLosHwiShare008(VOID);
VOID ItSmpLosHwiShare009(VOID);
VOID ItSmpLosHwiShare010(VOID);
#endif

VOID ItSmpLosHwiIpi001(VOID);
VOID ItSmpLosHwiIpi002(VOID);
VOID ItSmpLosHwiIpi003(VOID);
VOID ItSmpLosHwiIpi004(VOID);
VOID ItSmpLosHwiIpi005(VOID);
VOID ItSmpLosHwiIpi006(VOID);
VOID ItSmpLosHwiIpi007(VOID);
VOID ItSmpLosHwiIpi008(VOID);

VOID ItSmpLosHwiNest001(VOID);
VOID ItSmpLosHwiNest002(VOID);
VOID ItSmpLosHwiNest003(VOID);
VOID ItSmpLosHwiNest004(VOID);
VOID ItSmpLosHwiNest005(VOID);
VOID ItSmpLosHwiNest006(VOID);
VOID ItSmpLosHwiNest007(VOID);
VOID ItSmpLosHwiNest008(VOID);
#endif

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */
#endif /* LOS_SMP_HWI_H */