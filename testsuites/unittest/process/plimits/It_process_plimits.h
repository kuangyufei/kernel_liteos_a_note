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
#ifndef _IT_PROCESS_PLIMITS_H
#define _IT_PROCESS_PLIMITS_H

#include <string>
#include <vector>
#include <iostream>
#include <sstream>
#include <regex>
#include <fcntl.h>
#include <iostream>
#include <cstring>
#include <fstream>
#include <regex>
#include <sstream>
#include <numeric>
#include "osTest.h"

#define MEM_PAGE_SIZE 4096
#define MEM_SLEEP_TIME 5
#define MEM_RESERVED_PAGE 2
#define CHILD_FUNC_ARG           (0x2088)
#define STACK_SIZE               (1024 * 1024)
#define PROCESS_LIMIT_AMOUNT     (64)
#define TEST_BUFFER_SIZE         (512)
#define CPUP10S_INDEX            (11)
#define CPUP1S_INDEX             (12)
#define WAIT_CPUP_STABLE         (7)
#define WAIT_CPUP_STABLE_FOR_100 (17)
#define STATISTIC_TIMES          (11)
#define PERIOD_10_SEC_IN_US      (10 * 1000 * 1000)
#define QUOTA_2_SEC_IN_US        (2 * 1000 * 1000)
#define QUOTA_5_SEC_IN_US        (5 * 1000 * 1000)
#define QUOTA_6_SEC_IN_US        (6 * 1000 * 1000)
#define QUOTA_7_SEC_IN_US        (7 * 1000 * 1000)
#define QUOTA_10_SEC_IN_US       (10 * 1000 * 1000)
#define QUOTA_PERCENT_20         (20)
#define QUOTA_PERCENT_50         (50)
#define QUOTA_PERCENT_60         (60)
#define QUOTA_PERCENT_70         (70)
#define QUOTA_PERCENT_100        (100)
#define HARDWARE_CORE_AMOUNT     (2)
#define TOLERANCE_ERROR          (5)

int WriteFile(const char *filepath, const char *buf);
int RmdirLimiterFile(std::string path);
int RmdirControlFile(std::string path);
int ReadFile(const char *filepath, char *buf);
int GetLine(char *buf, int count, int maxLen, char **array);
int RmdirTest (std::string path);
extern UINT32 LosCurTaskIDGet();

int ForkChilds(int num, int *pidArray);
int CreatePlimitGroup(const char* groupName, char *childPidFiles,
                      unsigned long long periodUs, unsigned long long quotaUs);
int AddPidIntoSchedLimiters(int num, int *pidArray, const char *procspath);
int WaitForCpupStable(int expectedCpupPercent);
double CalcCpupUsage(int childAmount, int *childPidArray, int expectedCpupPercent);
double CheckCpupUsage(double sumAllChildsCpup, int expectedCpupPercent);
int TerminateChildProcess(int *childPidArray, int childAmount, int sig);
double TestCpupInPlimit(int childAmount, const char* groupName,
                        unsigned long long periodUs, unsigned long long quotaUs, int expectedCpupPercent);
double TestCpupWithoutLimit(int childAmount, const char* groupName, int expectedCpupPercent);

#if defined(LOSCFG_USER_TEST_SMOKE)
void ItProcessPlimits001(void);
void ItProcessPlimits002(void);
void ItProcessPlimits003(void);
void ItProcessPlimits004(void);
void ItProcessPlimits005(void);
void ItProcessPlimits006(void);
void ItProcessPlimits007(void);
void ItProcessPlimits008(void);
void ItProcessPlimitsMemory001(void);
void ItProcessPlimitsMemory002(void);
void ItProcessPlimitsPid001(void);
void ItProcessPlimitsPid002(void);
void ItProcessPlimitsPid003(void);
void ItProcessPlimitsPid004(void);
void ItProcessPlimitsPid005(void);
void ItProcessPlimitsPid006(void);
void ItProcessPlimitsSched001(VOID);
void ItProcessPlimitsSched002(VOID);
void ItProcessPlimitsSched003(VOID);
void ItProcessPlimitsSched004(VOID);
void ItProcessPlimitsDevices001(void);
void ItProcessPlimitsDevices002(void);
void ItProcessPlimitsDevices003(void);
void ItProcessPlimitsDevices004(void);
void ItProcessPlimitsDevices005(void);
void ItProcessPlimitsDevices006(void);
void ItProcessPlimitsDevices007(void);
void ItProcessPlimitsDevices008(void);
void ItProcessPlimitsDevices009(void);
void ItProcessPlimitsIpc002(void);
void ItProcessPlimitsIpc003(void);
void ItProcessPlimitsIpc004(void);
void ItProcessPlimitsIpc005(void);
void ItProcessPlimitsIpc006(void);
void ItProcessPlimitsIpc007(void);
void ItProcessPlimitsIpc008(void);
void ItProcessPlimitsIpc009(void);
void ItProcessPlimitsIpc010(void);
void ItProcessPlimitsIpc011(void);
void ItProcessPlimitsIpc012(void);
void ItProcessPlimitsIpc013(void);
#endif
#endif /* _IT_PROCESS_PLIMITS_H */
