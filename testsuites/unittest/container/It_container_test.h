/*
 * Copyright (c) 2023-2023 Huawei Device Co., Ltd. All rights reserved.
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
#ifndef _IT_CONTAINER_TEST_H
#define _IT_CONTAINER_TEST_H

#include <string>
#include <vector>
#include <iostream>
#include <sstream>
#include <regex>
#include <sys/syscall.h>
#include <sys/capability.h>
#include "osTest.h"

const int EXIT_CODE_ERRNO_1 = 1;
const int EXIT_CODE_ERRNO_2 = 2;
const int EXIT_CODE_ERRNO_3 = 3;
const int EXIT_CODE_ERRNO_4 = 4;
const int EXIT_CODE_ERRNO_5 = 5;
const int EXIT_CODE_ERRNO_6 = 6;
const int EXIT_CODE_ERRNO_7 = 7;
const int EXIT_CODE_ERRNO_8 = 8;
const int EXIT_CODE_ERRNO_9 = 9;
const int EXIT_CODE_ERRNO_10 = 10;
const int EXIT_CODE_ERRNO_11 = 11;
const int EXIT_CODE_ERRNO_12 = 12;
const int EXIT_CODE_ERRNO_13 = 13;
const int EXIT_CODE_ERRNO_14 = 14;
const int EXIT_CODE_ERRNO_15 = 15;
const int EXIT_CODE_ERRNO_16 = 16;
const int EXIT_CODE_ERRNO_255 = 255;
const int CONTAINER_FIRST_PID = 1;
const int CONTAINER_SECOND_PID = 2;
const int CONTAINER_THIRD_PID = 3;


extern const char *USERDATA_DIR_NAME;
extern const char *ACCESS_FILE_NAME;
extern const char *USERDATA_DEV_NAME;
extern const char *FS_TYPE;

extern const int BIT_ON_RETURN_VALUE;
extern const int STACK_SIZE;
extern const int CHILD_FUNC_ARG;

int ChildFunction(void *args);

pid_t CloneWrapper(int (*func)(void *), int flag, void *args);

std::string GenContainerLinkPath(int pid, const std::string& containerType);

extern std::string ReadlinkContainer(int pid, const std::string& containerType);

#if defined(LOSCFG_USER_TEST_SMOKE)
void ItContainer001(void);
#if defined(LOSCFG_USER_TEST_PID_CONTAINER)
void ItPidContainer023(void);
#endif
#if defined(LOSCFG_USER_TEST_UTS_CONTAINER)
void ItUtsContainer001(void);
void ItUtsContainer002(void);
#endif
#endif

#if defined(LOSCFG_USER_TEST_FULL)
#if defined(LOSCFG_USER_TEST_PID_CONTAINER)
void ItPidContainer001(void);
void ItPidContainer002(void);
void ItPidContainer003(void);
void ItPidContainer004(void);
void ItPidContainer006(void);
void ItPidContainer007(void);
void ItPidContainer008(void);
void ItPidContainer009(void);
void ItPidContainer010(void);
void ItPidContainer011(void);
void ItPidContainer012(void);
void ItPidContainer013(void);
void ItPidContainer014(void);
void ItPidContainer015(void);
void ItPidContainer016(void);
void ItPidContainer017(void);
void ItPidContainer018(void);
void ItPidContainer019(void);
void ItPidContainer020(void);
void ItPidContainer021(void);
void ItPidContainer022(void);
void ItPidContainer024(void);
#endif
#if defined(LOSCFG_USER_TEST_UTS_CONTAINER)
void ItUtsContainer003(void);
#endif
#endif

#endif /* _IT_CONTAINER_TEST_H */
