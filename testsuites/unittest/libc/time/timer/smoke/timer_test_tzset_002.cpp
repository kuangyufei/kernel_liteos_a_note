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
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <osTest.h>
#include "lt_timer_test.h"
#include <stdlib.h>

static UINT32 testcase(VOID)
{
    time_t td;
    int ret = 0;

    errno = 0;
    ret = putenv((char *) "TZ=GMT-100");
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    tzset();

    errno = 0;
    ret = time(&td);
    TEST_PRINT("[INFO]%s:%d,%s,Current time = %s\n",  __FILE__, __LINE__, __func__, asctime(localtime(&td)));
    ICUNIT_ASSERT_NOT_EQUAL(ret, -1, ret);
    TEST_PRINT("[INFO]%s:%d,%s,ret=%d,errno=%d,errstr=%s\n", __FILE__, __LINE__, __func__, ret, errno, strerror(errno));
    ICUNIT_GOTO_EQUAL(errno, 0, errno, OUT);

    return LOS_OK;
OUT:
    return LOS_NOK;
}

void TIME_TEST_TZSET_002(void)
{
    TEST_ADD_CASE(__FUNCTION__, testcase, TEST_POSIX, TEST_SWTMR, TEST_LEVEL0, TEST_FUNCTION);
}
