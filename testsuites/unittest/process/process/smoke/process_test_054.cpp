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
#include "it_test_process.h"

static int TestCase(void)
{
    int ret;
    int pid = 3;
    siginfo_t info = { 0 };

    ret = waitid((idtype_t)3, getpgrp(), &info, WEXITED); // 3, set tid.
    ret = errno;
    ICUNIT_GOTO_EQUAL(ret, EINVAL, ret, EXIT);

    ret = waitid(P_PID, 0, &info, WEXITED);
    ret = errno;
    ICUNIT_GOTO_EQUAL(ret, EINVAL, ret, EXIT);

    ret = waitid(P_PID, 999, &info, WEXITED); // 999, set group.
    ret = errno;
    ICUNIT_GOTO_EQUAL(ret, ECHILD, ret, EXIT);

    ret = waitid(P_PGID, 0, &info, WEXITED);
    ret = errno;
    ICUNIT_GOTO_EQUAL(ret, EINVAL, ret, EXIT);

    ret = waitid(P_ALL, getpgrp(), &info, -1);
    ret = errno;
    ICUNIT_GOTO_EQUAL(ret, EINVAL, ret, EXIT);

    ret = waitid(P_ALL, getpgrp(), &info, 0);
    ret = errno;
    ICUNIT_GOTO_EQUAL(ret, EINVAL, ret, EXIT);

    ret = waitid(P_ALL, getpgrp(), &info, 32); // 32, set exit num.
    ret = errno;
    ICUNIT_GOTO_EQUAL(ret, EINVAL, ret, EXIT);

    ret = waitid(P_ALL, getpgrp(), &info, WSTOPPED);
    ret = errno;
    ICUNIT_GOTO_EQUAL(ret, EOPNOTSUPP, ret, EXIT);

    ret = waitid(P_ALL, getpgrp(), &info, WCONTINUED);
    ret = errno;
    ICUNIT_GOTO_EQUAL(ret, EOPNOTSUPP, ret, EXIT);

    ret = waitid(P_ALL, getpgrp(), &info, WNOWAIT);
    ret = errno;
    ICUNIT_GOTO_EQUAL(ret, EOPNOTSUPP, ret, EXIT);

    ret = waitid(P_ALL, getpgrp(), &info, 21); // 21, set exit num.
    ret = errno;
    ICUNIT_GOTO_EQUAL(ret, EINVAL, ret, EXIT);
    return 0;
EXIT:
    return 1;
}

void ItTestProcess054(void)
{
    TEST_ADD_CASE("IT_POSIX_PROCESS_054", TestCase, TEST_POSIX, TEST_MEM, TEST_LEVEL0, TEST_FUNCTION);
}