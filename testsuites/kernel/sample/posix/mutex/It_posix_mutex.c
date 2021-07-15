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

#include "It_posix_mutex.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cpluscplus */
#endif /* __cpluscplus */

int g_retval;
int g_returned;
int g_canceled;
int g_value;
int g_ctrl;


VOID ItSuitePosixMutex(void)
{
#if defined(LOSCFG_TEST_SMOKE)
    ItPosixMux001();
    ItPosixMux007();
    ItPosixMux012();
    ItPosixMux015();
    ItPosixMux016();
    ItPosixMux019();
    ItPosixMux020();
#endif

#if defined(LOSCFG_TEST_FULL)
    ItPosixMux002();
    ItPosixMux003();
    ItPosixMux004();
    ItPosixMux005();
    ItPosixMux006();
    ItPosixMux008();
    ItPosixMux009();
    ItPosixMux010();
    ItPosixMux011();
    ItPosixMux013();
    ItPosixMux014();
    ItPosixMux017();
    ItPosixMux018();
    ItPosixMux021();
    ItPosixMux022();
    ItPosixMux023();
    ItPosixMux024();
    ItPosixMux025();
    ItPosixMux026();
    ItPosixMux027();
    ItPosixMux028();
    ItPosixMux029();
    ItPosixMux032();
    ItPosixMux033();
    ItPosixMux034();
    ItPosixMux035();
    ItPosixMux036();
    ItPosixMux037();
    ItPosixMux038();
    ItPosixMux039();
    ItPosixMux040();
    ItPosixMux041();
    ItPosixMux042();
    ItPosixMux043();
    ItPosixMux044();
    ItPosixMux045();
    ItPosixMux046();
    ItPosixMux047();
    ItPosixMux048();
    ItPosixMux049();
    ItPosixMux050();
    ItPosixMux054();
    ItPosixMux055();
    ItPosixMux056();
    ItPosixMux057();
    ItPosixMux058();
    ItPosixMux059();
    ItPosixMux060();
    ItPosixMux061();
    ItPosixMux062();
    ItPosixMux063();
    ItPosixMux064();
    ItPosixMux065();
    ItPosixMux066();
    ItPosixMux067();
    ItPosixMux068();
    ItPosixMux069();
    ItPosixMux070();
    ItPosixMux071();
    ItPosixMux072();
    ItPosixMux073();
    ItPosixMux074();
#ifndef LOSCFG_KERNEL_SMP
    ItPosixMux075();
#endif
    ItPosixMux076();
    ItPosixMux077();
    ItPosixMux078();
    ItPosixMux079();
    ItPosixMux080();
    ItPosixMux081();
    ItPosixMux082();
    ItPosixMux084();
    ItPosixMux085();
    ItPosixMux086();
    ItPosixMux087();
    ItPosixMux089();
    ItPosixMux090();
    ItPosixMux091();
    ItPosixMux092();
    ItPosixMux093();
    ItPosixMux094();
    ItPosixMux095();
    ItPosixMux097();
    ItPosixMux098();
    ItPosixMux099();
    ItPosixMux101();
#endif
}


#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cpluscplus */
#endif /* __cpluscplus */
