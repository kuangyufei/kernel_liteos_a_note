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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

static int TestMkfifo()
{
    char buffer[80];
    int fd;
    int retValue;
    pid_t pid;

    unlink("/dev/fifo");

    retValue = mkfifo("/dev/fifo", 0777); // 0777, mkfifo config.
    printf("mkfifo begin,retValue=%d\n", retValue);

    char sentence[] = "Hello World";

    pid = fork();
    if (pid < 0) {
        printf("Fork error\n");
        return -1;
    } else if (pid == 0) {
        fd = open("/dev/fifo", O_WRONLY);
        write(fd, sentence, sizeof(sentence) + 1);
        close(fd);
        exit(0);
    } else {
        fd = open("/dev/fifo", O_RDONLY);
        read(fd, buffer, sizeof(buffer));
        printf("Receive data: %s\n", buffer);
        close(fd);
    }
    return 0;
}


void ItPosixMkfifo001(void)
{
    TEST_ADD_CASE(__FUNCTION__, TestMkfifo, TEST_POSIX, TEST_MEM, TEST_LEVEL0, TEST_FUNCTION);
}
