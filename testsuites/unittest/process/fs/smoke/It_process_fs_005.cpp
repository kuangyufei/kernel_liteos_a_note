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

#include <cstdio>
#include "It_process_fs_test.h"

static std::string gen_proc_pid_cpup_path(int pid)
{
    std::ostringstream buf;
    buf << "/proc/" << pid << "/cpup";
    return buf.str();
}

static void operPidCpup(std::string strFile)
{
    const int PUP_LEN = 1024;
    char szStatBuf[PUP_LEN];
    FILE *fp = fopen(strFile.c_str(), "rb");
    ASSERT_NE(fp, nullptr);

    (void)memset_s(szStatBuf, PUP_LEN, 0, PUP_LEN);
    int ret = fread(szStatBuf, 1, PUP_LEN, fp);
    PrintTest("cat %s\n", strFile.c_str());

    PrintTest("%s\n", szStatBuf);
    ASSERT_EQ(ret, strlen(szStatBuf));

    char *res = strstr(szStatBuf, "TotalRunningTime");
    ASSERT_NE(res, nullptr);

    (void)fclose(fp);
}

void ItProcessFs005(void)
{
    auto path = gen_proc_pid_cpup_path(getpid());
    operPidCpup(path);
}
