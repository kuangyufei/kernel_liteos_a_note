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
#include "It_test_IO.h"
#include <time.h>
#include <locale.h>

static UINT32 testcase(VOID)
{
    time_t currtime;
    struct tm *timer = {nullptr};
    char buffer[80];
    locale_t oldloc = LC_GLOBAL_LOCALE;
    locale_t newloc = nullptr;

    printf("[INFO]newloc=0x%x,oldloc=0x%x\n", (int)newloc, (int)oldloc);
    newloc = duplocale(oldloc);
    printf("[INFO]newloc=0x%x,oldloc=0x%x\n", (int)newloc, (int)oldloc);
    ICUNIT_ASSERT_NOT_EQUAL_NULL(newloc, nullptr, -1);

    time(&currtime);
    timer = localtime(&currtime);

    /* set timer as 'Thu Jan  1 23:48:56 1970'" */
    timer->tm_sec = 32;
    timer->tm_min = 3;
    timer->tm_hour = 1;
    timer->tm_mday = 2;
    timer->tm_mon = 0;
    timer->tm_year = 70;
    timer->tm_wday = 5;
    timer->tm_yday = 1;
    timer->tm_isdst = 0;
    timer->__tm_gmtoff = 0;
    timer->__tm_zone = nullptr;

    setenv("MUSL_LOCPATH", "/storage", 1);
    printf("[INFO]getenv MUSL_LOCPATH=%s\n", getenv("MUSL_LOCPATH"));
    ICUNIT_GOTO_STRING_EQUAL(getenv("MUSL_LOCPATH"), "/storage", getenv("MUSL_LOCPATH"), OUT);

    printf("[INFO]Locale is: %s\n", setlocale(LC_TIME, "en_US.UTF-8"));
    strftime(buffer, 80, "%c", timer);
    printf("[INFO]Date is: %s\n", buffer);
    ICUNIT_GOTO_STRING_EQUAL(buffer, "星期五 一月  2 01:03:32 1970", -1, OUT);

    printf("[INFO]Locale is: %s\n", setlocale(LC_TIME, "zh_CN.UTF-8"));
    strftime(buffer, 80, "%c", timer);
    printf("[INFO]Date is: %s\n", buffer);
    ICUNIT_GOTO_STRING_EQUAL(buffer, "星期五 一月  2 01:03:32 1970", -1, OUT);

    printf("[INFO]Locale is: %s\n", setlocale(LC_TIME, ""));
    strftime(buffer, 80, "%c", timer);
    printf("[INFO]Date is: %s\n", buffer);
    ICUNIT_GOTO_STRING_EQUAL(buffer, "Fri Jan  2 01:03:32 1970", -1, OUT);

    free(newloc);
    return LOS_OK;
OUT:
    free(newloc);
    return LOS_NOK;
}

VOID IO_TEST_DUPLOCALE_001(void)
{
    TEST_ADD_CASE(__FUNCTION__, testcase, TEST_LIB, TEST_LIBC, TEST_LEVEL1, TEST_FUNCTION);
}
