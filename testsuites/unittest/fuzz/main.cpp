/*
 * Copyright (c) 2022-2023 Huawei Device Co., Ltd. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this list of
 *    conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list
 *    of conditions and the following disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors may be used
 *    to endorse or promote products derived from this software without specific prior written
 *    permission.
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

#include <secodeFuzz.h>

extern void TestPosixSpawnFileActionsAddchdirNp(void);
extern void TestPosixSpawnFileActionsAdddup2(void);
extern void TestPosixSpawnFileActionsAddfchdirNp(void);
extern void TestPosixSpawnFileActionsAddopen(void);
extern void TestPosixSpawnFileActionsDestroy(void);
extern void TestPosixSpawnFileActionsInit(void);
extern void TestPosixSpawn(void);
extern void TestPosixSpawnattrDestroy(void);
extern void TestPosixSpawnattrGetflags(void);
extern void TestPosixSpawnattrGetpgroup(void);
extern void TestPosixSpawnattrGetschedparam(void);
extern void TestPosixSpawnattrGetschedpolicy(void);
extern void TestPosixSpawnattrGetsigdefault(void);
extern void TestPosixSpawnattrGetsigmask(void);
extern void TestPosixSpawnattrInit(void);
extern void TestPosixSpawnattrSetflags(void);
extern void TestPosixSpawnattrSetpgroup(void);
extern void TestPosixSpawnattrSetschedparam(void);
extern void TestPosixSpawnattrSetschedpolicy(void);
extern void TestPosixSpawnattrSetsigdefault(void);
extern void TestPosixSpawnattrSetsigmask(void);
extern void TestPosixSpawnp(void);
extern void TestAdjtime(void);
extern void TestFesetenv(void);
extern void TestGetrlimit(void);
extern void TestReadlink(void);
extern void TestReadlinkat(void);
extern void TestSyslog(void);
extern void TestSystem(void);
extern void TestTimes(void);
extern void TestClone(void);
extern void TestEpollCreate(void);
extern void TestEpollCtl(void);
extern void TestEpollWait(void);
extern void TestMlock(void);
extern void TestMlockall(void);
extern void TestPthreadMutexConsistent(void);
extern void TestPthreadMutexGetprioceiling(void);
extern void TestPthreadMutexattrSetprotocol(void);
extern void TestPthreadMutexattrSetrobust(void);
extern void TestPthreadMutexattrSettype(void);
extern void TestPthreadSetconcurrency(void);
extern void TestSetns(void);
extern void TestUnshare(void);
extern void TestSemOpen(void);
extern void TestChroot(void);
extern void TestSethostname(void);

int main()
{
    DT_Set_Report_Path("/storage/");
    TestReadlink();
    TestReadlinkat();
    TestSethostname();
    TestClone();
    TestUnshare();
    TestChroot();
    TestSetns();
    return 0;
}
