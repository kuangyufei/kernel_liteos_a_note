/*
 * Copyright (c) 2013-2019 Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020-2021 Huawei Device Co., Ltd. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this list of
 *    conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list
 *    of conditions and the following disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors may be used
 *    to endorse or promote products derived from this software without specific prior written
 *    permission.
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

#include "It_vfs_jffs.h"

static const int PARAM_NUM = 2;
static int Dup2Test(void)
{
    int ret = 0;
    int number1, number2, sum;
    int savedStdin = -1;
    int savedStdout = -1;

    CHAR pathname[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    (void)strcat_s(pathname, JFFS_STANDARD_NAME_LENGTH, "/input.txt");

    int inputFds = open(pathname, O_CREAT | O_RDWR, S_IRWXU);
    ICUNIT_GOTO_NOT_EQUAL(inputFds, -1, inputFds, EXIT);

    savedStdin = dup(STDIN_FILENO);
    savedStdout = dup(STDOUT_FILENO);

    setbuf(stdout, NULL);
    setbuf(stdin, NULL);

    ret = dup2(inputFds, STDIN_FILENO);
    ICUNIT_GOTO_EQUAL(ret, STDIN_FILENO, ret, EXIT);

    ret = dup2(inputFds, STDOUT_FILENO);
    ICUNIT_GOTO_EQUAL(ret, STDOUT_FILENO, ret, EXIT);

    /* STDIN_FILENO STDOUT_FILENO all be directed */
    printf("1 1\n");
    lseek(inputFds, 0, SEEK_SET);
    ret = scanf_s("%d %d", &number1, &number2);
    ICUNIT_GOTO_EQUAL(ret, PARAM_NUM, ret, EXIT);

    sum = number1 + number2;

EXIT:
    close(inputFds);
    dup2(savedStdin, STDIN_FILENO);
    dup2(savedStdout, STDOUT_FILENO);
    close(savedStdin);
    close(savedStdout);
    unlink(pathname);
    return sum;
}

const int DUP_TEST_RET = 2;

static UINT32 TestCase(VOID)
{
    INT32 ret;
    CHAR pathname1[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };

    ret = mkdir(pathname1, S_IRWXU | S_IRWXG | S_IRWXO);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ret = Dup2Test();
    ICUNIT_GOTO_EQUAL(ret, DUP_TEST_RET, ret, EXIT);

    ret = rmdir(pathname1);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    return JFFS_NO_ERROR;
EXIT:
    remove(pathname1);
    return JFFS_NO_ERROR;
}

VOID ItFsJffs201(VOID)
{
    TEST_ADD_CASE("IT_FS_JFFS_201", TestCase, TEST_VFS, TEST_JFFS, TEST_LEVEL2, TEST_FUNCTION);
}