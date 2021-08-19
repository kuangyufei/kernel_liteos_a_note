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
#include "It_vfs_jffs.h"

#define DIRPATH1  "/storage/test1"
#define DIRPATH2  "/storage/test2"
#define TEST_MAXPATHLEN  4098

static int TestCase0(void)
{
    INT32 ret = -1;
    INT32 dirFd = -1;
    INT32 fd = -1;
    INT32 len = 0;
    CHAR pathname1[JFFS_STANDARD_NAME_LENGTH] = DIRPATH1;
    CHAR pathname2[JFFS_STANDARD_NAME_LENGTH] = DIRPATH2;
    DIR *dir = NULL;

    ret = mkdir(pathname1, JFFS_FILE_MODE);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    ret = renameat(AT_FDCWD, DIRPATH1, AT_FDCWD, DIRPATH2);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT2);

    dir = opendir(pathname2);
    ICUNIT_GOTO_NOT_EQUAL(dir, NULL, dir, EXIT1);

    dirFd = dirfd(dir);
    ICUNIT_GOTO_NOT_EQUAL(dirFd, JFFS_IS_ERROR, dirFd, EXIT1);

    ret = strcat_s(pathname2, JFFS_STANDARD_NAME_LENGTH, "/test1.txt");
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    fd = open(pathname2, O_EXCL | O_CREAT | O_RDWR, JFFS_FILE_MODE);
    ICUNIT_GOTO_NOT_EQUAL(fd, JFFS_IS_ERROR, fd, EXIT1);

    len = write(fd, "01234567890", 11);

    ret = close(fd);
    ICUNIT_GOTO_NOT_EQUAL(ret, JFFS_IS_ERROR, ret, EXIT1);

    ret = renameat(dirFd, "test1.txt", dirFd, "test2.txt");
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT2);

    ret = unlink("/storage/test2/test2.txt");
    ICUNIT_GOTO_NOT_EQUAL(ret, JFFS_IS_ERROR, ret, EXIT2);

    ret = closedir(dir);
    ICUNIT_GOTO_NOT_EQUAL(ret, JFFS_IS_ERROR, ret, EXIT);

    ret = rmdir(DIRPATH2);
    ICUNIT_GOTO_NOT_EQUAL(ret, JFFS_IS_ERROR, ret, EXIT);

    return JFFS_NO_ERROR;

EXIT1:
    close(fd);
EXIT2:
    unlink(pathname2);
    closedir(dir);
EXIT:
    rmdir(DIRPATH2);
    return JFFS_NO_ERROR;
}

static int TestCase1(void)
{
    int ret = -1;
    char pathName[TEST_MAXPATHLEN] = {0};
    int errFd = -1;
    /* ENAMETOOLONG */
    ret = memset_s(pathName, TEST_MAXPATHLEN, 1, TEST_MAXPATHLEN);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);
    ret = renameat(AT_FDCWD, pathName, AT_FDCWD, pathName);
    ICUNIT_GOTO_EQUAL(ret, JFFS_IS_ERROR, ret, EXIT);
    ICUNIT_GOTO_EQUAL(errno, ENAMETOOLONG, ret, EXIT);
    /* EINVAL */
    ret = renameat(AT_FDCWD, "\0", AT_FDCWD, "\0");
    ICUNIT_GOTO_EQUAL(ret, JFFS_IS_ERROR, ret, EXIT);
    ICUNIT_GOTO_EQUAL(errno, EINVAL, errno, EXIT);
    /* ENOENT */
    ret = renameat(errFd, "/storage/test.txt", errFd, "/storage/test1.txt");
    ICUNIT_GOTO_EQUAL(ret, JFFS_IS_ERROR, ret, EXIT);
    ICUNIT_GOTO_EQUAL(errno, ENOENT, ret, EXIT);

    return JFFS_NO_ERROR;

EXIT:
    return JFFS_IS_ERROR;
}

static int TestCase(void)
{
    int ret = -1;
    ret = TestCase0();
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);
    ret = TestCase1();
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    return JFFS_NO_ERROR;

EXIT:
    return JFFS_IS_ERROR;
}

void ItTestFsJffs003(void)
{
    TEST_ADD_CASE("It_Test_Fs_Jffs_003", TestCase, TEST_VFS, TEST_JFFS, TEST_LEVEL0, TEST_FUNCTION);
}

