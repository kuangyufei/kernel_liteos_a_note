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
#ifndef IT_TEST_SIGNAL_H
#define IT_TEST_SIGNAL_H

#include "osTest.h"
#include "sys/resource.h"
#include <fcntl.h>

#define RED_FLAG 6

extern void ItPosixSignal001(void);
extern void ItPosixSignal002(void);
extern void ItPosixSignal003(void);
extern void ItPosixSignal004(void);
extern void ItPosixSignal005(void);
extern void ItPosixSignal006(void);
extern void ItPosixSignal007(void);
extern void ItPosixSignal008(void);
extern void ItPosixSignal009(void);
extern void ItPosixSignal010(void);
extern void ItPosixSignal011(void);
extern void ItPosixSignal012(void);
extern void ItPosixSignal013(void);
extern void ItPosixSignal014(void);
extern void ItPosixSignal015(void);
extern void ItPosixSignal016(void);
extern void ItPosixSignal017(void);
extern void ItPosixSignal018(void);
extern void ItPosixSignal019(void);
extern void ItPosixSignal020(void);
extern void ItPosixSignal021(void);
extern void ItPosixSignal022(void);
extern void ItPosixSignal023(void);
extern void ItPosixSignal024(void);
extern void ItPosixSignal025(void);

extern void ItPosixSignal026(void);
extern void ItPosixSignal027(void);
extern void ItPosixSignal028(void);
extern void ItPosixSignal029(void);
extern void ItPosixSignal030(void);
extern void ItPosixSignal031(void);
extern void ItPosixSignal032(void);
extern void ItPosixSignal033(void);
extern void ItPosixSignal034(void);
extern void ItPosixSignal035(void);
extern void ItPosixSignal036(void);
extern void ItPosixSignal037(void);
extern void ItPosixSignal038(void);
extern void ItPosixSignal039(void);
extern void ItPosixSignal040(void);
extern void ItPosixSignal041(void);
extern void ItPosixSignal042(void);

extern void ItPosixSigset001(void);
extern void ItPosixSigset002(void);
extern void ItPosixPipe001(void);
extern void ItPosixPipe002(void);
extern void ItPosixPipe003(void);
extern void ItPosixPipe004(void);
extern void ItPosixPipe005(void);
extern void ItPosixPipe006(void);
extern void ItPosixMkfifo001(void);
extern void ItPosixMkfifo002(void);
extern void ItIpcFdClr001(void);
extern void ItIpcFdIsset001(void);
extern void ItIpcFdSet001(void);
extern void ItIpcFdZero001(void);
extern void ItIpcMkfifo002(void);
extern void ItIpcMkfifo003(void);
extern void ItIpcPipe002(void);
extern void ItIpcPipe003(void);
extern void ItIpcPipe004(void);
extern void ItIpcPipe005(void);
extern void ItIpcSigaction001(void);
extern void ItIpcSigpause001(void);
extern void ItIpcSigpromask001(void);
extern int kill(pid_t pid, int sig);

#endif
