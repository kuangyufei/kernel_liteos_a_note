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
    int pipeFd[2], ret, spid; // 2, pipe return 2 file descripter
    char buffer[20];          // 20, target buffer size
    ret = pipe(pipeFd);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT1);
    spid = fork();
    ICUNIT_GOTO_NOT_EQUAL(spid, -1, spid, EXIT1);
    if (spid == 0) {
        close(pipeFd[0]);
        ret = write(pipeFd[1], "hello world", TAR_STR_LEN);
        printf("write first status: %d\n", ret);
        if (ret != TAR_STR_LEN) {
            exit(11); // 11, the value of son process unexpect exit, convenient to debug
        }
        close(pipeFd[1]);
        exit(RED_FLAG);
    }
    close(pipeFd[1]);
    sleep(2); // 2, sleep 2 second
    ret = read(pipeFd[0], buffer, TAR_STR_LEN);
    ICUNIT_GOTO_EQUAL(ret, TAR_STR_LEN, ret, EXIT);
    ret = strcmp(buffer, "hello world");
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);
    printf("read pipe success: %s\n", buffer);
    wait(&ret);
    printf("son return status: %d\n", WEXITSTATUS(ret));
    ICUNIT_GOTO_EQUAL(WEXITSTATUS(ret), RED_FLAG, WEXITSTATUS(ret), EXIT);
    printf("pipe ok\n");
    close(pipeFd[0]);
    return LOS_OK;
EXIT:
    close(pipeFd[0]);
EXIT1:
    return LOS_NOK;
}

VOID ItIpcPipe003(void)
{
    TEST_ADD_CASE(__FUNCTION__, Testcase, TEST_LIB, TEST_LIBC, TEST_LEVEL1, TEST_FUNCTION);
}
