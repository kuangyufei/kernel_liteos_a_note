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
#include "it_test_signal.h"


static const int TAR_STR_LEN = 12;

static UINT32 Testcase(VOID)
{
    int pipeFd[2], ret; // 2, pipe return 2 file descirpter
    fd_set reads;
    ret = pipe(pipeFd);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);
    ret = write(pipeFd[1], "Hello World", TAR_STR_LEN);
    printf("write first status: %d\n", ret);
    ICUNIT_GOTO_EQUAL(ret, TAR_STR_LEN, ret, EXIT);
    FD_ZERO(&reads);
    FD_SET(pipeFd[0], &reads);
    ret = select(pipeFd[0] + 1, &reads, NULL, NULL, NULL);
    ICUNIT_GOTO_EQUAL(ret, 1, ret, EXIT);
    ret = FD_ISSET(pipeFd[0], &reads);
    ICUNIT_GOTO_NOT_EQUAL(ret, 0, ret, EXIT);
    close(pipeFd[0]);
    close(pipeFd[1]);
    printf("-------------------FD_ISSET ok------------\n");
    return LOS_OK;
EXIT:
    close(pipeFd[0]);
    close(pipeFd[1]);
    return LOS_NOK;
}

VOID ItIpcFdIsset001(void)
{
    TEST_ADD_CASE(__FUNCTION__, Testcase, TEST_LIB, TEST_LIBC, TEST_LEVEL1, TEST_FUNCTION);
}
