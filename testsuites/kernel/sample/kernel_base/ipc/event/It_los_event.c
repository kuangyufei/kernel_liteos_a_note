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

#include "It_los_event.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cpluscplus */
#endif /* __cpluscplus */

VOID ItSuiteLosEvent(VOID)
{
#if (LOSCFG_KERNEL_SMP == YES)
    ItSmpLosEvent001();
    ItSmpLosEvent002();
    ItSmpLosEvent003();
    ItSmpLosEvent004();
    ItSmpLosEvent005();
    ItSmpLosEvent006();
    ItSmpLosEvent007();
    ItSmpLosEvent008();
    ItSmpLosEvent009();
    ItSmpLosEvent010();
    ItSmpLosEvent011();
    ItSmpLosEvent012();
    ItSmpLosEvent013();
    ItSmpLosEvent014();
    ItSmpLosEvent015();
    ItSmpLosEvent016();
    ItSmpLosEvent017();
    ItSmpLosEvent018();
    ItSmpLosEvent019();
    ItSmpLosEvent020();
    ItSmpLosEvent021();
    ItSmpLosEvent022();
    ItSmpLosEvent023();
    ItSmpLosEvent024();
    ItSmpLosEvent025();
    ItSmpLosEvent027();

    ItSmpLosEvent029();
    ItSmpLosEvent030();
    ItSmpLosEvent031();
    ItSmpLosEvent032();
    ItSmpLosEvent034();
    ItSmpLosEvent037();
#endif

#if defined(LOSCFG_TEST_SMOKE)
    ItLosEvent031(); // 0 destroy
    ItLosEvent035(); // 0  init
    ItLosEvent036(); //  0  clear destroy write read
    ItLosEvent041(); // 0 read
#endif

#if defined(LOSCFG_TEST_FULL)
    ItLosEvent001();
    ItLosEvent002();
    ItLosEvent003();
    ItLosEvent004();
    ItLosEvent005(); // write  clear  destroy
    ItLosEvent006();
    ItLosEvent007();
    ItLosEvent008();
    ItLosEvent009(); // read
    ItLosEvent010();
    ItLosEvent011();
    ItLosEvent012();
    ItLosEvent013();
    ItLosEvent014();
    ItLosEvent015();
    ItLosEvent016();
    ItLosEvent018();
    ItLosEvent019();
    ItLosEvent020();
    ItLosEvent021();
    ItLosEvent022();
    ItLosEvent023();
    ItLosEvent024();
    ItLosEvent025();
#if (LOSCFG_KERNEL_SMP != YES)
    ItLosEvent026();
#endif
    ItLosEvent027();
    ItLosEvent029();
    ItLosEvent030();
    ItLosEvent032();
    ItLosEvent033(); // /
    ItLosEvent037();
    ItLosEvent038();
#if (LOSCFG_KERNEL_SMP != YES)
    ItLosEvent039();
#endif
    ItLosEvent040();
    ItLosEvent042();
    ItLosEvent043();
#endif

#if defined(LOSCFG_TEST_PRESSURE)
    ItLosEvent028();
#endif

#if defined(LOSCFG_TEST_LLT)
    LltLosEvent001();
    ItLosEvent017();
    ItLosEvent034();
#endif
#if (LOSCFG_KERNEL_SMP == YES)
    HalIrqSetAffinity(HWI_NUM_TEST, 1);
#endif
}
#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus   */
#endif /* __cpluscplus   */
