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
#include "It_test_util.h"

static struct option g_longOptions[] = {
    {"test1", 0, NULL, 'a'},
    {"test2", 0, NULL, 'b'},
    {"test3", 0, NULL, 'c'},
    {0, 0, 0, 0},
};

#define ARGC_NUM 5

static UINT32 TestCase(VOID)
{
    INT32 ret, i;
    INT32 argc = ARGC_NUM;
    CHAR *argv[] = {"test", "-b", "-a", "-c", NULL};
    CHAR *ptr = NULL;
    CHAR * const shortOptions = "abc";

    for (i = 0; (ret = getopt_long_only(argc, argv, shortOptions, g_longOptions, NULL)) != -1; i++) {
        switch (ret) {
            case 'a':
                ICUNIT_GOTO_EQUAL(i, 1, i, EXIT);
                break;
            case 'b':
                ICUNIT_GOTO_EQUAL(i, 0, i, EXIT);
                break;
            case 'c':
                ICUNIT_GOTO_EQUAL(i, 2, i, EXIT);
                break;
            default:
                break;
        }
    }

    return 0;
EXIT:
    return -1;
}

VOID ItTestUtil002(VOID)
{
    TEST_ADD_CASE(__FUNCTION__, TestCase, TEST_POSIX, TEST_MEM, TEST_LEVEL0, TEST_FUNCTION);
}
