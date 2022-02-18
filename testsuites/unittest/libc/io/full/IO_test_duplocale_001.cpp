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
#include <time.h>
#include <locale.h>
#include <langinfo.h>
#include "It_test_IO.h"

/* Zh_cn.utf-8 File content */
int fileWords[] = {
    0x950412de, 0x0,        0x3,        0x1c,       0x34,       0x5,
    0x4c,       0x0,        0x60,       0x3,        0x61,       0x3,
    0x65,       0x14f,      0x69,       0x9,        0x1b9,      0x6,
    0x1c3,      0x1,        0x3,        0x0,        0x0,        0x2,
    0x69724600, 0x6e614a00, 0x6f725000, 0x7463656a, 0x2d64492d, 0x73726556,
    0x3a6e6f69, 0x6d695620, 0x6d695328, 0x66696c70, 0x20646569, 0x6e696843,
    0x29657365, 0x7065520a, 0x2d74726f, 0x6967734d, 0x75422d64, 0x542d7367,
    0xa203a6f,  0x522d4f50, 0x73697665, 0x2d6e6f69, 0x65746144, 0x3032203a,
    0x302d3630, 0x31322d34, 0x3a343120, 0x302b3030, 0xa303038,  0x7473614c,
    0x6172542d, 0x616c736e, 0x3a726f74, 0x68755920, 0x20676e65, 0xa656958,
    0x676e614c, 0x65676175, 0x6165542d, 0x53203a6d, 0x6c706d69, 0x65696669,
    0x68432064, 0x73656e69, 0x494d0a65, 0x562d454d, 0x69737265, 0x203a6e6f,
    0xa302e31,  0x746e6f43, 0x2d746e65, 0x65707954, 0x6574203a, 0x702f7478,
    0x6e69616c, 0x6863203b, 0x65737261, 0x54553d74, 0xa382d46,  0x746e6f43,
    0x2d746e65, 0x6e617254, 0x72656673, 0x636e452d, 0x6e69646f, 0x38203a67,
    0xa746962,  0x72756c50, 0x462d6c61, 0x736d726f, 0x706e203a, 0x6172756c,
    0x313d736c, 0x6c70203b, 0x6c617275, 0xa3b303d,  0x676e614c, 0x65676175,
    0x687a203a, 0xa4e435f,  0x65472d58, 0x6172656e, 0x3a726f74, 0x656f5020,
    0x20746964, 0xa302e33,  0x9f98e600, 0xe49f9ce6, 0xe40094ba, 0x9ce680b8,
    0x88
};

UINT32 SonFunc(VOID)
{
    int ret;
    struct tm timer_;
    struct tm *timer = &timer_;
    char buffer[80]; /* 80, The number of characters returned by strftime */
    char *retptr = nullptr;

    /* set timer as 'Thu Jan  1 23:48:56 1970'" */
    timer->tm_sec = 32; /* 32, example */
    timer->tm_min = 3;  /* 3, example */
    timer->tm_hour = 1;
    timer->tm_mday = 2;
    timer->tm_mon = 0;
    timer->tm_year = 70;  /* 70, example */
    timer->tm_wday = 5;  /* 5, example */
    timer->tm_yday = 1;
    timer->tm_isdst = 0;
    timer->__tm_gmtoff = 0;
    timer->__tm_zone = nullptr;

    ret = setenv("MUSL_LOCPATH", "/storage", 1);
    ICUNIT_ASSERT_EQUAL(ret, 0, -1);
    retptr = getenv("MUSL_LOCPATH");
    ICUNIT_ASSERT_NOT_EQUAL(retptr, NULL, -1);
    ICUNIT_ASSERT_STRING_EQUAL(retptr, "/storage", -1);

    retptr = setlocale(LC_TIME, "en_US.UTF-8");
    ICUNIT_ASSERT_NOT_EQUAL(retptr, NULL, -1);
    ICUNIT_ASSERT_STRING_EQUAL(retptr, "en_US.UTF-8", -1);

    ret = strftime(buffer, 80, "%c", timer);  /* 80, The maximum number of characters in the string str */
    ICUNIT_ASSERT_NOT_EQUAL(ret, 0, -1);
    ICUNIT_ASSERT_STRING_EQUAL(buffer, "Fri Jan  2 01:03:32 1970", -1);

    retptr = setlocale(LC_TIME, "zh_CN.UTF-8");
    ICUNIT_ASSERT_NOT_EQUAL(retptr, NULL, -1);
    ICUNIT_ASSERT_STRING_EQUAL(retptr, "zh_CN.UTF-8", -1);
   
    ret = strftime(buffer, 80, "%c", timer);  /* 80, The maximum number of characters in the string str */
    ICUNIT_ASSERT_NOT_EQUAL(ret, 0, -1);
    ICUNIT_ASSERT_STRING_EQUAL(buffer, "星期五 一月  2 01:03:32 1970", -1);
    
    retptr = setlocale(LC_TIME, "");
    ICUNIT_ASSERT_NOT_EQUAL(retptr, NULL, -1);
    ICUNIT_ASSERT_STRING_EQUAL(retptr, "C", -1);

    ret = strftime(buffer, 80, "%c", timer);  /* 80, The maximum number of characters in the string str */
    ICUNIT_ASSERT_NOT_EQUAL(ret, 0, -1);
    ICUNIT_ASSERT_STRING_EQUAL(buffer, "Fri Jan  2 01:03:32 1970", -1);

    return 0;
}

static UINT32 testcase(VOID)
{
    int ret, status;
    locale_t oldloc = LC_GLOBAL_LOCALE;
    locale_t newloc = nullptr;

    char *pathList[] = {"/storage/zh_CN.UTF-8"};
    char *streamList[] = {(char *)fileWords};
    int streamLen[] = {sizeof(fileWords) - 2};

    newloc = duplocale(oldloc);
    ICUNIT_ASSERT_NOT_EQUAL(newloc, nullptr, -1);
    free(newloc);

    ret = PrepareFileEnv(pathList, streamList, streamLen, 1);
    if (ret != 0) {
        printf("error: need some env file, but prepare is not ok");
        (VOID)RecoveryFileEnv(pathList, 1);
        return -1;
    }

    pid_t pid = fork();
    if (pid == 0) {
        ret = SonFunc();
        exit(ret);
    }

    ret = waitpid(pid, &status, 0);
    ICUNIT_ASSERT_EQUAL(ret, pid, ret);
    (VOID)RecoveryFileEnv(pathList, 1);
    status = WEXITSTATUS(status);
    ICUNIT_ASSERT_EQUAL(status, 0, status);
    setlocale(LC_ALL, "C");

    return LOS_OK;
}

VOID IO_TEST_DUPLOCALE_001(void)
{
    TEST_ADD_CASE(__FUNCTION__, testcase, TEST_LIB, TEST_LIBC, TEST_LEVEL1, TEST_FUNCTION);
}
