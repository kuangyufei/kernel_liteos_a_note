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


static const int NAME_BUF_SIZE = 50;
static const int TAR_STR_LEN = 12;

static UINT32 Testcase(VOID)
{
    int tarFd, ret, spid;
    char buffer[20]; // 20, target buffer size
    char pathname[NAME_BUF_SIZE];
    char *filename = "/mkfifotest2";
    strncpy(pathname, "/dev", NAME_BUF_SIZE);
    strcat(pathname, filename);
    ret = mkfifo(pathname, 0777); // 0777, file athurioty
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT1);
    spid = fork();
    ICUNIT_GOTO_NOT_EQUAL(ret, -1, ret, EXIT1);
    if (spid == 0) {
        sleep(1);
        tarFd = open(pathname, O_WRONLY, 0777); // 0777, file athurioty
        if (tarFd == -1) {
            exit(12); // 12, the value of son process unexpect exit, convenient to debug
        }
        printf("son tarfd = %d\n", tarFd);
        ret = write(tarFd, "hello world", TAR_STR_LEN);
        if (ret != TAR_STR_LEN) {
            exit(11); // 11, the value of son process unexpect exit, convenient to debug
        }
        printf("write first status: %d\n", ret);
        close(tarFd);
        exit(RED_FLAG);
    }

    tarFd = open(pathname, O_RDONLY, 0777); // 0777, file athurioty
    printf("son tarfd = %d\n", tarFd);
    ICUNIT_GOTO_NOT_EQUAL(tarFd, -1, tarFd, EXIT);
    ret = read(tarFd, buffer, TAR_STR_LEN);
    ICUNIT_GOTO_EQUAL(ret, TAR_STR_LEN, ret, EXIT);
    ret = strcmp(buffer, "hello world");
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);
    printf("read fifo success: %s\n", buffer);
    wait(&ret);
    ICUNIT_GOTO_EQUAL(WEXITSTATUS(ret), RED_FLAG, WEXITSTATUS(ret), EXIT);

    close(tarFd);
    remove(pathname);
    printf("mkfifo ok\n");
    return LOS_OK;
EXIT:
    close(tarFd);
EXIT1:
    remove(pathname);
    return LOS_NOK;
}

VOID ItIpcMkfifo002(void)
{
    TEST_ADD_CASE(__FUNCTION__, Testcase, TEST_LIB, TEST_LIBC, TEST_LEVEL1, TEST_FUNCTION);
}
