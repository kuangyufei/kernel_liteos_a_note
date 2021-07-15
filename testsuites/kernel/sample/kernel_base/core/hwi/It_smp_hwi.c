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

#include "It_smp_hwi.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cpluscplus */
#endif /* __cpluscplus */
VOID ItSuiteSmpHwi(VOID)
{
#ifdef LOSCFG_KERNEL_SMP
    ItSmpLosHwi001();
    ItSmpLosHwi002();
    ItSmpLosHwi003();
    ItSmpLosHwi004();
    ItSmpLosHwi005();
    ItSmpLosHwi006();
    ItSmpLosHwi007();
    ItSmpLosHwi008();
    ItSmpLosHwi009();
    ItSmpLosHwi010();
    ItSmpLosHwi011();
    ItSmpLosHwi012();
    ItSmpLosHwi013();

#ifndef LOSCFG_NO_SHARED_IRQ
    ItSmpLosHwiShare001();
    ItSmpLosHwiShare002();
    ItSmpLosHwiShare003();
    ItSmpLosHwiShare004();
    ItSmpLosHwiShare005();
    ItSmpLosHwiShare006();
    ItSmpLosHwiShare007();
    ItSmpLosHwiShare008();
    ItSmpLosHwiShare009();
    ItSmpLosHwiShare010();
#endif
    ItSmpLosHwiIpi001();
    ItSmpLosHwiIpi002();
    ItSmpLosHwiIpi003();
    ItSmpLosHwiIpi004();
    ItSmpLosHwiIpi005();

    ItSmpLosHwiIpi006();
    ItSmpLosHwiIpi007();
    ItSmpLosHwiIpi008();

    ItSmpLosHwiNest001();
    ItSmpLosHwiNest002();
    ItSmpLosHwiNest003();
    ItSmpLosHwiNest004();
    ItSmpLosHwiNest005();
    ItSmpLosHwiNest006();
    ItSmpLosHwiNest007();
    ItSmpLosHwiNest008();
#endif
#ifdef LOSCFG_KERNEL_SMP
    HalIrqSetAffinity(HWI_NUM_TEST, 1);
    HalIrqSetAffinity(HWI_NUM_TEST1, 1);
    HalIrqSetAffinity(HWI_NUM_TEST2, 1);
    HalIrqSetAffinity(HWI_NUM_TEST3, 1);
#endif
}


#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */
