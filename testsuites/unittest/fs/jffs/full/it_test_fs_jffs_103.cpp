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
    INT32 dirfd = 0;
    INT32 fd = 0;
    INT32 ret = JFFS_IS_ERROR;
    CHAR pathname0[TEST_STRLEN] = JFFS_PATH_NAME0;
    CHAR pathname1[TEST_STRLEN] = JFFS_PATH_NAME0;
    CHAR pathname2[TEST_STRLEN] = JFFS_PATH_NAME0;
    CHAR pathname3[TEST_STRLEN] = JFFS_PATH_NAME01;
    CHAR filebuf[256] =  "abcdeab89abcedcde01234567fghij9876543210abcdeabc89abcedfghde0123456710abcdeabcde012876543289abcedfghij987654345673210abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123456789abcedfg10abcdeahij9876543289abcedfghlbcde01234567I0lalalalalalalal";
    struct stat info = {0};

    ret = mkdir(pathname0, 0777);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    strcat_s(pathname1, TEST_STRLEN, "/test1");
    ret = mkdir(pathname1, 0777);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    ret = chmod(pathname1, S_IRUSR | S_IRGRP | S_IROTH);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    stat(pathname1, &info);
    ret = info.st_mode & (S_IRUSR | S_IRGRP | S_IROTH);
    ICUNIT_GOTO_NOT_EQUAL(ret, 0, ret, EXIT);

    dirfd = open(pathname0, O_DIRECTORY);
    ICUNIT_GOTO_NOT_EQUAL(dirfd, JFFS_IS_ERROR, dirfd, EXIT);

    /* ENAMETOOLONG */
    ret = fchmodat(dirfd, filebuf, S_IREAD, 0);
    ret = errno;
    ICUNIT_GOTO_EQUAL(ret, ENAMETOOLONG, ret, EXIT);

    /* ENOENT */
    ret = fchmodat(-1, "test1", S_IREAD, 0);
    ret = errno;
    ICUNIT_GOTO_EQUAL(ret, ENOENT, ret, EXIT);

    /* EINVAL */
    ret = fchmodat(dirfd, NULL, S_IREAD, 0);
    ret = errno;
    ICUNIT_GOTO_EQUAL(ret, EINVAL, ret, EXIT);

    /* absolute path */
    ret = fchmodat(0, pathname1, S_IRUSR | S_IWUSR | S_IXUSR, 0);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);
    stat(pathname1, &info);
    ret = info.st_mode & (S_IRUSR | S_IWUSR | S_IXUSR);
    ICUNIT_GOTO_EQUAL(ret, (S_IRUSR | S_IWUSR | S_IXUSR), ret, EXIT);

    /* S_IRUSR | S_IWUSR | S_IXUSR */
    ret = fchmodat(dirfd, "test1", S_IRUSR | S_IWUSR | S_IXUSR, 0);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);
    stat(pathname1, &info);
    ret = info.st_mode & (S_IRUSR | S_IWUSR | S_IXUSR);
    ICUNIT_GOTO_EQUAL(ret, (S_IRUSR | S_IWUSR | S_IXUSR), ret, EXIT);

    /* AT_FDCWD */
    ret = fchmodat(AT_FDCWD, "test/test1", S_IRUSR | S_IWUSR | S_IXUSR, 0);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);
    stat(pathname1, &info);
    ret = info.st_mode & (S_IRUSR | S_IWUSR | S_IXUSR);
    ICUNIT_GOTO_EQUAL(ret, (S_IRUSR | S_IWUSR | S_IXUSR), ret, EXIT);
    ret = close(dirfd);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    /* ENOTDIR */
    dirfd = open(pathname3, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, 0777);
    ICUNIT_GOTO_NOT_EQUAL(fd, JFFS_IS_ERROR, fd, EXIT);
    ret = fchmodat(dirfd, "test1", S_IROTH | S_IWOTH | S_IXOTH, 0);
    ret = errno;
    ICUNIT_GOTO_EQUAL(ret, ENOTDIR, ret, EXIT);
    ret = close(dirfd);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);
    ret = remove(pathname3);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);
    ret = rmdir(pathname1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    dirfd = open(pathname0, O_DIRECTORY);
    ICUNIT_GOTO_NOT_EQUAL(dirfd, JFFS_IS_ERROR, dirfd, EXIT);

    strcat_s(pathname2, TEST_STRLEN, "/test.txt");
    fd = open(pathname2, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, 0777);
    ICUNIT_GOTO_NOT_EQUAL(fd, JFFS_IS_ERROR, fd, EXIT);

    ret = chmod(pathname2, S_IRUSR | S_IRGRP | S_IROTH);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    stat(pathname2, &info);
    ret = info.st_mode & (S_IRUSR | S_IRGRP | S_IROTH);
    ICUNIT_GOTO_EQUAL(ret, (S_IRUSR | S_IRGRP | S_IROTH), ret, EXIT);
    ret = close(fd);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    ret = fchmodat(dirfd, "test.txt", S_IRUSR | S_IWUSR | S_IXUSR, 0);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);
    stat(pathname2, &info);
    ret = info.st_mode & (S_IRUSR | S_IWUSR | S_IXUSR);
    ICUNIT_GOTO_EQUAL(ret, (S_IRUSR | S_IWUSR | S_IXUSR), ret, EXIT);

    ret = close(dirfd);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    ret = remove(pathname2);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    ret = remove(pathname0);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);
    return JFFS_NO_ERROR;
EXIT1:
    if (dirfd > 0) {
        close(dirfd);
    }
    if (fd > 0) {
        close(fd);
    }
EXIT:
    remove(pathname3);
    remove(pathname2);
    remove(pathname1);
    remove(pathname0);
    return JFFS_IS_ERROR;
}

void ItTestFsJffs103(void)
{
    TEST_ADD_CASE("It_Test_Fs_Jffs_103", TestCase, TEST_VFS, TEST_JFFS, TEST_LEVEL0, TEST_FUNCTION);
}
