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
#include "It_fs_jffs.h"

static FILE *g_filep;
#define NUM 64
#define COUNT 10

void *DoChild()
{
    int fd = 0;
    int ret, count;
    char buf[NUM];

    flockfile(g_filep);

    if (fseek(g_filep, 0L, SEEK_SET) == -1) {
        perror("fseek()");
    }
    ret = fread(buf, NUM, 1, g_filep);

    count = atoi(buf);
    ++count;
    sprintf_s(buf, NUM, "%d", count);
    if (fseek(g_filep, 0L, SEEK_SET) == -1) {
        perror("fseek()");
    }
    ret = fwrite(buf, strlen(buf), 1, g_filep);

    funlockfile(g_filep);

    return NULL;
}

static int TestCase(void)
{
    INT32 ret = JFFS_IS_ERROR;
    INT32 fd = 0;
    INT32 count = 0;
    pthread_t tid[COUNT];
    CHAR pathname1[JFFS_STANDARD_NAME_LENGTH] = JFFS_PATH_NAME01;
    CHAR pathname2[JFFS_STANDARD_NAME_LENGTH] = JFFS_PATH_NAME01;
    CHAR buf[COUNT];

    ret = mkdir(pathname1, 0777);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    strcat_s(pathname2, JFFS_STANDARD_NAME_LENGTH, "/test12345.txt");
    g_filep = fopen(pathname2, "w+");
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    for (count = 0; count < COUNT; count++) {
        ret = pthread_create(tid + count, NULL, DoChild, NULL);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);
    }

    for (count = 0; count < COUNT; count++) {
        ret = pthread_join(tid[count], NULL);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);
    }

    ret = fclose(g_filep);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    fd = open(pathname2, O_RDONLY);
    ICUNIT_GOTO_NOT_EQUAL(fd, JFFS_IS_ERROR, fd, EXIT1);

    ret = read(fd, buf, COUNT);
    ICUNIT_GOTO_NOT_EQUAL(ret, JFFS_IS_ERROR, ret, EXIT1);

    ret = atoi(buf);
    ICUNIT_GOTO_EQUAL(ret, COUNT, ret, EXIT1);

    ret = close(fd);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    ret = unlink(pathname2);
    g_filep = nullptr;
    ICUNIT_GOTO_NOT_EQUAL(ret, JFFS_IS_ERROR, ret, EXIT);

    ret = rmdir(JFFS_PATH_NAME01);
    ICUNIT_GOTO_NOT_EQUAL(ret, JFFS_IS_ERROR, ret, EXIT);

    return JFFS_NO_ERROR;

EXIT1:
    if (fd > 0) {
        close(fd);
    }
    unlink(pathname2);
    g_filep = nullptr;
EXIT:
    rmdir(JFFS_PATH_NAME01);
    return JFFS_IS_ERROR;
}

void ItTestFsJffs113(void)
{
    TEST_ADD_CASE("It_Test_Fs_Jffs_113", TestCase, TEST_VFS, TEST_JFFS, TEST_LEVEL0, TEST_FUNCTION);
}
