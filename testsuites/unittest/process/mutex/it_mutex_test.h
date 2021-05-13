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
#ifndef _IT_TEST_MUTEX_H
#define _IT_TEST_MUTEX_H
#include "osTest.h"
#include "sys/time.h"
#include <sys/types.h>

#define SLEEP_AND_YIELD(tick) usleep((tick)*10 * 1000)

extern int Gettid(void);
extern void ItTestPthreadMutex001(void);
extern void ItTestPthreadMutex002(void);
extern void ItTestPthreadMutex003(void);
extern void ItTestPthreadMutex004(void);
extern void ItTestPthreadMutex005(void);
extern void ItTestPthreadMutex006(void);
extern void ItTestPthreadMutex007(void);
extern void ItTestPthreadMutex008(void);
extern void ItTestPthreadMutex009(void);
extern void ItTestPthreadMutex010(void);
extern void ItTestPthreadMutex011(void);
extern void ItTestPthreadMutex012(void);
extern void ItTestPthreadMutex013(void);
extern void ItTestPthreadMutex014(void);
extern void ItTestPthreadMutex015(void);
extern void ItTestPthreadMutex016(void);
extern void ItTestPthreadMutex017(void);
extern void ItTestPthreadMutex018(void);
extern void ItTestPthreadMutex019(void);
extern void ItTestPthreadMutex020(void);
extern void ItTestPthreadMutex021(void);
extern void ItTestPthreadMutex022(void);
extern void ItTestPthreadMutex023(void);
extern void ItTestPthreadMutex024(void);
extern void ItTestPthreadMutex025(void);

#endif
