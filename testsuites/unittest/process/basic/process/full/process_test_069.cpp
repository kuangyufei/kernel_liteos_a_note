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
#include <spawn.h>

static const int NUMMAX = 16;

static int TestCase(void)
{
    char *envp[] = {"ABC=asddfg", NULL};
    char *argv1[] = {"envp", NULL};
    pid_t pid;
    int status = 1;
    char temp[NUMMAX] = {0};
    int ret;
    int fd;
    
    ret = posix_spawnp(&pid, "/storage/test_spawn", NULL, NULL, argv1, envp);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ret = waitpid(pid, &status, 0);
    ICUNIT_GOTO_EQUAL(ret, pid, ret, EXIT);

    status = WEXITSTATUS(status);
    ICUNIT_GOTO_EQUAL(status, 0, status, EXIT);

    fd = open("/storage/testspawnattr.txt", O_RDWR | O_CREAT, 0644); // 0644, open config
    ret = read(fd, temp, NUMMAX);
    ICUNIT_GOTO_EQUAL(ret, NUMMAX, ret, EXIT1);

    ret = strncmp(temp, "ABC=asddfg", strlen(temp));
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT1);

    close(fd);
    unlink("/storage/testspawnattr.txt");
    return 0;
EXIT1:
    close(fd);
    unlink("/storage/testspawnattr.txt");
EXIT:
    return 1;
}

void ItTestProcess069(void)
{
    TEST_ADD_CASE("IT_POSIX_PROCESS_069", TestCase, TEST_POSIX, TEST_MEM, TEST_LEVEL0, TEST_FUNCTION);
}