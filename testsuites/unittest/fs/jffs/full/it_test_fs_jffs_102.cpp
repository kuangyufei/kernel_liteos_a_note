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

#define TEST_STRLEN 30

static int TestCase(void)
{
    INT32 fd = 0;
    INT32 ret = JFFS_IS_ERROR;
    CHAR pathname1[TEST_STRLEN] = JFFS_PATH_NAME0;
    struct stat info = {0};

    fd = open(pathname1, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, 0777);
    ICUNIT_GOTO_NOT_EQUAL(fd, -1, fd, EXIT1);

    /* EBADF */
    ret = fchmod(-1, S_IREAD);
    ret = errno;
    ICUNIT_GOTO_EQUAL(ret, EBADF, ret, EXIT1);

    /* S_IRWXU */
    ret = fchmod(fd, S_IRWXU);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);
    stat(pathname1, &info);
    ret = info.st_mode & S_IRWXU;
    ICUNIT_GOTO_NOT_EQUAL(ret, 0, ret, EXIT);

    /* S_IRUSR */
    ret = fchmod(fd, S_IRUSR);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);
    stat(pathname1, &info);
    ret = info.st_mode & S_IRUSR;
    ICUNIT_GOTO_NOT_EQUAL(ret, 0, ret, EXIT);

    /* S_IWUSR */
    ret = fchmod(fd, S_IWUSR);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);
    stat(pathname1, &info);
    ret = info.st_mode & S_IWUSR;
    ICUNIT_GOTO_NOT_EQUAL(ret, 0, ret, EXIT);

    /* S_IXUSR */
    ret = fchmod(fd, S_IXUSR);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);
    stat(pathname1, &info);
    ret = info.st_mode & S_IXUSR;
    ICUNIT_GOTO_NOT_EQUAL(ret, 0, ret, EXIT);

    /* S_IRWXG */
    ret = fchmod(fd, S_IRWXG);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);
    stat(pathname1, &info);
    ret = info.st_mode & S_IRWXG;
    ICUNIT_GOTO_NOT_EQUAL(ret, 0, ret, EXIT);

    /* SIRGRP */
    ret = fchmod(fd, S_IRGRP);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);
    stat(pathname1, &info);
    ret = info.st_mode & S_IRGRP;
    ICUNIT_GOTO_NOT_EQUAL(ret, 0, ret, EXIT);

    /* S_IXGRP */
    ret = fchmod(fd, S_IXGRP);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);
    stat(pathname1, &info);
    ret = info.st_mode & S_IXGRP;
    ICUNIT_GOTO_NOT_EQUAL(ret, 0, ret, EXIT);

    /* S_IRWXO */
    ret = fchmod(fd, S_IRWXO);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);
    stat(pathname1, &info);
    ret =  info.st_mode &S_IRWXO;
    ICUNIT_GOTO_NOT_EQUAL(ret, 0, ret, EXIT);

    /* S_IROTH */
    ret = fchmod(fd, S_IROTH);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);
    stat(pathname1, &info);
    ret = info.st_mode & S_IROTH;
    ICUNIT_GOTO_NOT_EQUAL(ret, 0, ret, EXIT);

    /* S_IXOTH */
    ret = fchmod(fd, S_IXOTH);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);
    stat(pathname1, &info);
    ret = info.st_mode & S_IXOTH;
    ICUNIT_GOTO_NOT_EQUAL(ret, 0, ret, EXIT);
    ret = close(fd);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    ret = unlink(pathname1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    return JFFS_NO_ERROR;
EXIT1:
    if (fd > 0) {
        close(fd);
    }
    unlink(pathname1);
EXIT:
    rmdir(pathname1);
    return JFFS_IS_ERROR;
}

void ItTestFsJffs102(void)
{
    TEST_ADD_CASE("It_Test_Fs_Jffs_102", TestCase, TEST_VFS, TEST_JFFS, TEST_LEVEL0, TEST_FUNCTION);
}
