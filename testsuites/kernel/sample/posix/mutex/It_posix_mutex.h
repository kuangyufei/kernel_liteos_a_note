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

#ifndef _IT_POSIX_MUTEX_H
#define _IT_POSIX_MUTEX_H

#include "osTest.h"
#include "pthread.h"
#include "errno.h"
#include "sched.h"
#include "semaphore.h"
#include "unistd.h"
#include "los_task_pri.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

#ifdef LOSCFG_DEBUG_DEADLOCK
#define TEST_MUTEX_INIT              \
    {                                \
        { 0, 0, 0, 0 },              \
        {                            \
            { 0, 0 }, { 0, 0 }, 0, 0 \
        }                            \
    }
#else
#define TEST_MUTEX_INIT    \
    {                      \
        { 0, 0, 0, 0 },    \
        {                  \
            { 0, 0 }, 0, 0 \
        }                  \
    }
#endif
#define MUTEX_TEST_NUM 100

extern int g_retval;
extern int g_value;
extern int g_ctrl;

long sysconf(int name);

#if defined(LOSCFG_TEST_SMOKE)
VOID IT_FS_VNODE_001(void);
VOID IT_FS_NAMECACHE_001(void);

VOID ItPosixMux001(void);
VOID ItPosixMux007(void);
VOID ItPosixMux012(void);
VOID ItPosixMux015(void);
VOID ItPosixMux016(void);
VOID ItPosixMux019(void);
VOID ItPosixMux020(void);
#endif

#if defined(LOSCFG_TEST_FULL)
VOID ItPosixMux002(void);
VOID ItPosixMux003(void);
VOID ItPosixMux004(void);
VOID ItPosixMux005(void);
VOID ItPosixMux006(void);
VOID ItPosixMux008(void);
VOID ItPosixMux009(void);
VOID ItPosixMux010(void);
VOID ItPosixMux011(void);
VOID ItPosixMux013(void);
VOID ItPosixMux014(void);
VOID ItPosixMux017(void);
VOID ItPosixMux018(void);
VOID ItPosixMux021(void);
VOID ItPosixMux022(void);
VOID ItPosixMux023(void);
VOID ItPosixMux024(void);
VOID ItPosixMux025(void);
VOID ItPosixMux026(void);
VOID ItPosixMux027(void);
VOID ItPosixMux028(void);
VOID ItPosixMux029(void);
VOID ItPosixMux032(void);
VOID ItPosixMux033(void);
VOID ItPosixMux034(void);
VOID ItPosixMux035(void);
VOID ItPosixMux036(void);
VOID ItPosixMux037(void);
VOID ItPosixMux038(void);
VOID ItPosixMux039(void);
VOID ItPosixMux040(void);
VOID ItPosixMux041(void);
VOID ItPosixMux042(void);
VOID ItPosixMux043(void);
VOID ItPosixMux044(void);
VOID ItPosixMux045(void);
VOID ItPosixMux046(void);
VOID ItPosixMux047(void);
VOID ItPosixMux048(void);
VOID ItPosixMux049(void);
VOID ItPosixMux050(void);
VOID ItPosixMux054(void);
VOID ItPosixMux055(void);
VOID ItPosixMux056(void);
VOID ItPosixMux057(void);
VOID ItPosixMux058(void);
VOID ItPosixMux059(void);
VOID ItPosixMux060(void);
VOID ItPosixMux061(void);
VOID ItPosixMux062(void);
VOID ItPosixMux063(void);
VOID ItPosixMux064(void);
VOID ItPosixMux065(void);
VOID ItPosixMux066(void);
VOID ItPosixMux067(void);
VOID ItPosixMux068(void);
VOID ItPosixMux069(void);
VOID ItPosixMux070(void);
VOID ItPosixMux071(void);
VOID ItPosixMux072(void);
VOID ItPosixMux073(void);
VOID ItPosixMux074(void);
VOID ItPosixMux075(void);
VOID ItPosixMux076(void);
VOID ItPosixMux077(void);
VOID ItPosixMux078(void);
VOID ItPosixMux079(void);
VOID ItPosixMux080(void);
VOID ItPosixMux081(void);
VOID ItPosixMux082(void);
VOID ItPosixMux084(void);
VOID ItPosixMux085(void);
VOID ItPosixMux086(void);
VOID ItPosixMux087(void);
VOID ItPosixMux089(void);
VOID ItPosixMux090(void);
VOID ItPosixMux091(void);
VOID ItPosixMux092(void);
VOID ItPosixMux093(void);
VOID ItPosixMux094(void);
VOID ItPosixMux095(void);
VOID ItPosixMux097(void);
VOID ItPosixMux098(void);
VOID ItPosixMux099(void);
VOID ItPosixMux101(void);
#endif

#if defined(LOSCFG_TEST_LLT)
VOID LltPosixMux001(VOID);
VOID LltPosixMux002(VOID);
VOID LltPosixMux003(VOID);
VOID LltPosixMux004(VOID);
VOID LltPosixMux005(VOID);
VOID ItPosixMux031(void);
VOID ItPosixMux083(void);
VOID ItPosixMux088(void);
VOID ItPosixMux096(void);
VOID ItPosixMux100(void);
VOID LltPosixMux006(void);
VOID LltPosixMux007(void);
#endif

#if defined(LOSCFG_TEST_PRESSURE)
VOID ItPosixMux051(void);
VOID ItPosixMux052(void);
VOID ItPosixMux053(void);
#endif

VOID ItSuitePosixMutex(void);


#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _IT_POSIX_MUTEX_H */
