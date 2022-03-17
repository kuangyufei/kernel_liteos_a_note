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
#define __USE_GUN
#include "it_test_exc.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

static char *g_args[] = {
    "hic et nunc",
    "-l",
    "/dev/shm",
    nullptr
};

static int TestCase(void)
{
    struct stat st = {0};
    void *p = nullptr;
    int fd = 0;
    int shmFd = 0;
    int rc = 0;
    int ret = 0;

    shmFd = shm_open("wurstverschwendung", O_RDWR | O_CREAT, 0777);
    if (shmFd == -1) {
        perror("shm_open");
        return -1;
    }

    rc = stat("/bin/shell", &st);
    if (rc == -1) {
        perror("stat");
        return -1;
    }

    printf("shm_fd=%d,st.st_size=%lld\n", shmFd, st.st_size);
    rc = ftruncate(shmFd, st.st_size);
    if (rc == -1) {
        perror("ftruncate");
        return -1;
    }

    p = mmap(nullptr, st.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, shmFd, 0);
    if (p == MAP_FAILED) {
        perror("mmap");
        return -1;
    }

    fd = open("/bin/shell", O_RDONLY, 0);
    printf("fd=%d\n", fd);
    if (fd == -1) {
        perror("openls");
        munmap(p, st.st_size);
        return -1;
    }

    rc = read(fd, p, st.st_size);
    if (rc == -1) {
        perror("read");
        munmap(p, st.st_size);
        close(shmFd);
        return -1;
    }
    if (rc != st.st_size) {
        fputs("Strange situation!\n", stderr);
        munmap(p, st.st_size);
        close(shmFd);
        return -1;
    }

    munmap(p, st.st_size);
    close(shmFd);

    shmFd = shm_open("wurstverschwendung", O_RDONLY, 0);
    ret = fexecve(shmFd, g_args, environ);
    perror("fexecve");
    ICUNIT_ASSERT_NOT_EQUAL(ret, -1, ret);

    return LOS_OK;
}

void ItTestFexecve001(void)
{
    TEST_ADD_CASE(__FUNCTION__, TestCase, TEST_POSIX, TEST_MEM, TEST_LEVEL0, TEST_FUNCTION);
}
