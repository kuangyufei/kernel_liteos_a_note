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
#include <unistd.h>

#define FILE_PATH    "/storage/testlockf.txt"

/* F_TLOCK、F_ULOCK */
static int TestCase0(void)
{
    int fd = -1;
    int ret;

    fd = open(FILE_PATH, O_WRONLY, JFFS_FILE_MODE);
    ICUNIT_GOTO_NOT_EQUAL(fd, JFFS_IS_ERROR, fd, EXIT);

    /* lock 0-3 Regions */
    ret = lseek(fd, 0, SEEK_SET);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    ret = lockf(fd, F_TLOCK, 3);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    /* unlock 1 Regions */
    ret = lseek(fd, 0, SEEK_SET);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    ret = lseek(fd, 1, SEEK_CUR);
    ICUNIT_GOTO_EQUAL(ret, 1, ret, EXIT);

    ret = lockf(fd, F_ULOCK, 1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    close(fd);
    return 0;

EXIT:
    close(fd);
    return -1;
}

/* F_TEST */
static int TestCase1(void)
{
    int fd = -1;
    int ret;

    fd = open(FILE_PATH, O_WRONLY, JFFS_FILE_MODE);
    ICUNIT_GOTO_NOT_EQUAL(fd, JFFS_IS_ERROR, fd, EXIT);

    /* lock 0-3 Regions */
    ret = lseek(fd, 0, SEEK_SET);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    ret = lockf(fd, F_LOCK, 3);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    /* try lock 1 Region */
    ret = lseek(fd, 0, SEEK_SET);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    ret = lseek(fd, 1, SEEK_CUR);
    ICUNIT_GOTO_EQUAL(ret, 1, ret, EXIT);

    ret = lockf(fd, F_TEST, 1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    /* try lock 3 Region */
    ret = lseek(fd, 0, SEEK_SET);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    ret = lseek(fd, 3, SEEK_CUR);
    ICUNIT_GOTO_EQUAL(ret, 3, ret, EXIT);

    ret = lockf(fd, F_TEST, 1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    close(fd);
    return 0;

EXIT:
    close(fd);
    return -1;
}

/* F_LOCK */
static int TestCase2(void)
{
    int fd = -1;
    int ret;

    fd = open(FILE_PATH, O_WRONLY, JFFS_FILE_MODE);
    ICUNIT_GOTO_NOT_EQUAL(fd, JFFS_IS_ERROR, fd, EXIT);

    /* lock 0-3 Regions */
    ret = lseek(fd, 0, SEEK_SET);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    ret = lockf(fd, F_LOCK, 3);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    /* lock 2-5 Region */
    ret = lseek(fd, 0, SEEK_SET);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    ret = lseek(fd, 2, SEEK_CUR);
    ICUNIT_GOTO_EQUAL(ret, 2, ret, EXIT);

    ret = lockf(fd, F_LOCK, 3);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    close(fd);
    return 0;

EXIT:
    close(fd);
    return -1;
}

static int TestCase(void)
{
    int fd = -1;
    int ret;

    fd = open(FILE_PATH, O_WRONLY | O_CREAT, JFFS_FILE_MODE);
    ICUNIT_GOTO_NOT_EQUAL(fd, JFFS_IS_ERROR, fd, EXIT);

    ret = TestCase0();
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    ret = TestCase1();
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    ret = TestCase2();
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

EXIT:
    close(fd);
    unlink(FILE_PATH);
}

void ItTestFsJffs008(void)
{
    TEST_ADD_CASE("It_Test_Fs_Jffs_008", TestCase, TEST_VFS, TEST_JFFS, TEST_LEVEL0, TEST_FUNCTION);
}

