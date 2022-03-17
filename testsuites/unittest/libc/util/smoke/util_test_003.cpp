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
#include "It_test_misc.h"

#define TEST_BUF_SIZE 20
#define TEST_STR "rw,ro=test"

static UINT32 TestCase(VOID)
{
    enum {
        RO_OPT = 0,
        RW_OPT,
        NAME_OPT
    };

    CHAR *const token[] = {
        [RO_OPT]   = "ro",
        [RW_OPT]   = "rw",
        [NAME_OPT] = "name",
        NULL
    };
    CHAR *buf = (char *)malloc(TEST_BUF_SIZE);
    CHAR *a = TEST_STR;
    CHAR *value = NULL;
    CHAR *subopts = NULL;
    INT32 err = 0;
    INT32 countRight = 0;
    INT32 countErr = 0;

    (void)strcpy_s(buf, TEST_BUF_SIZE, a);
    subopts = buf;

    while (*subopts != '\0' && !err) {
        switch (getsubopt(&subopts, token, &value)) {
            case RO_OPT:
                if (value) {
                    countRight++;
                } else {
                    countErr++;
                }
                break;
            case RW_OPT:
                if (value) {
                    countErr++;
                } else {
                    countRight++;
                }
                break;
            case NAME_OPT:
                if (value) {
                    countErr++;
                } else {
                    countRight++;
                }
                break;
            default:
                err = 1;
                break;
        }
    }

    ICUNIT_GOTO_EQUAL(countRight, 2, countRight, EXIT);
    ICUNIT_GOTO_EQUAL(countErr, 0, countErr, EXIT);

    free(buf);
    return 0;
EXIT:
    free(buf);
    return -1;
}

VOID ItTestUtil003(VOID)
{
    TEST_ADD_CASE(__FUNCTION__, TestCase, TEST_POSIX, TEST_MEM, TEST_LEVEL0, TEST_FUNCTION);
}
