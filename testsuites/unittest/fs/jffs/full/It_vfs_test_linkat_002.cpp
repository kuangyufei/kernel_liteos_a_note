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

static UINT32 testcase(VOID)
{
    INT32 fd = -1;
    INT32 fd1 = -1;
    INT32 len, ret;
    struct stat statBuf = {0};
    CHAR filebuf[JFFS_STANDARD_NAME_LENGTH] = "1234567890abcde&";
    CHAR readbuf[JFFS_STANDARD_NAME_LENGTH] = {0};
    CHAR pathname1[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    CHAR pathname2[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME01 };
    CHAR pathname3[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME02 };
    CHAR pathname4[JFFS_STANDARD_NAME_LENGTH] = "originfile";
    CHAR pathname5[JFFS_STANDARD_NAME_LENGTH] = "symlinkfile";
    CHAR pathname6[JFFS_STANDARD_NAME_LENGTH] = "linkfile";
    CHAR pathname7[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    CHAR pathname8[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME01 };
    CHAR pathname9[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME02 };
    INT32 dirFd0 = -1;
    INT32 dirFd1 = -1;
    INT32 dirFd2 = -1;
    DIR *dir0 = NULL;
    DIR *dir1 = NULL;
    DIR *dir2 = NULL;

    /* get dirfd0 */
    ret = mkdir(pathname1, 0777);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    dir0 = opendir(pathname1);
    ICUNIT_GOTO_NOT_EQUAL(dir0, NULL, dir0, EXIT1);

    dirFd0 = dirfd(dir0);
    ICUNIT_GOTO_NOT_EQUAL(dirFd0, JFFS_IS_ERROR, dirFd0, EXIT2);

    /* get dirfd1 */
    ret = mkdir(pathname2, 0777);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT2);

    dir1 = opendir(pathname2);
    ICUNIT_GOTO_NOT_EQUAL(dir1, NULL, dir1, EXIT3);

    dirFd1 = dirfd(dir1);
    ICUNIT_GOTO_NOT_EQUAL(dirFd1, JFFS_IS_ERROR, dirFd1, EXIT4);

    /* get dirfd2 */
    ret = mkdir(pathname3, 0777);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT4);

    dir2 = opendir(pathname3);
    ICUNIT_GOTO_NOT_EQUAL(dir2, NULL, dir2, EXIT5);

    dirFd2 = dirfd(dir2);
    ICUNIT_GOTO_NOT_EQUAL(dirFd2, JFFS_IS_ERROR, dirFd2, EXIT6);

    /* creat original file */
    fd = openat(dirFd0, pathname4, O_NONBLOCK | O_CREAT | O_RDWR, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_NOT_EQUAL(fd, JFFS_IS_ERROR, fd, EXIT6);

    len = write(fd, filebuf, strlen(filebuf));
    ICUNIT_GOTO_EQUAL(len, strlen(filebuf), len, EXIT8);

    ret = close(fd);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT7);

    /* creat a symbol link to the original file */
    strcat(pathname7, "/");
    strcat(pathname7, pathname4);
    ret = symlinkat(pathname7, dirFd1, pathname5);
    ICUNIT_GOTO_NOT_EQUAL(ret, JFFS_IS_ERROR, ret, EXIT7);

    len = readlinkat(dirFd1, pathname5, readbuf, sizeof(readbuf));
    ICUNIT_GOTO_EQUAL(len, strlen(pathname7), len, EXIT9);
    ICUNIT_GOTO_STRING_EQUAL(readbuf, pathname7, readbuf, EXIT9);

    /* creat a hard link to the symlink file */
    ret = linkat(dirFd1, pathname5, dirFd2, pathname6, 0);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT9);

    strcat(pathname9, "/");
    strcat(pathname9, pathname6);
    ret = stat(pathname9, &statBuf);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT10);
    ICUNIT_GOTO_EQUAL(statBuf.st_mode & S_IFMT, S_IFLNK, statBuf.st_mode & S_IFMT, EXIT10);
    ICUNIT_GOTO_EQUAL(statBuf.st_size, strlen(pathname7), statBuf.st_size, EXIT10);

    len = readlink(pathname9, readbuf, sizeof(readbuf));
    ICUNIT_GOTO_EQUAL(len, JFFS_IS_ERROR, len, EXIT10);
    ICUNIT_GOTO_EQUAL(errno, EINVAL, errno, EXIT10);

    ret = unlink(pathname9);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT10);

    /* creat a hard link to the original file by linking to the symlink file */
    ret = linkat(dirFd1, pathname5, dirFd2, pathname6, AT_SYMLINK_FOLLOW);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT9);

    memset(&statBuf, 0, sizeof(struct stat));
    ret = stat(pathname9, &statBuf);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT10);
    ICUNIT_GOTO_EQUAL(statBuf.st_mode & S_IFMT, S_IFREG, statBuf.st_mode & S_IFMT, EXIT10);

    fd1 = openat(dirFd2, pathname6, O_NONBLOCK | O_RDWR, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_NOT_EQUAL(fd, JFFS_IS_ERROR, fd, EXIT10);

    memset(readbuf, 0, JFFS_STANDARD_NAME_LENGTH);
    len = read(fd1, readbuf, strlen(filebuf));
    ICUNIT_GOTO_EQUAL(len, strlen(filebuf), len, EXIT11);
    ICUNIT_GOTO_STRING_EQUAL(readbuf, filebuf, readbuf, EXIT11);

    ret = close(fd1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT11);

    ret = unlinkat(dirFd2, pathname6, 0);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT10);

    ret = unlinkat(dirFd1, pathname5, 0);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT9);

    ret = unlinkat(dirFd0, pathname4, 0);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT7);

    ret = closedir(dir2);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT6);

    ret = rmdir(pathname3);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT5);

    ret = closedir(dir1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT4);

    ret = rmdir(pathname2);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT3);

    ret = closedir(dir0);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT2);

    ret = rmdir(pathname1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    return JFFS_NO_ERROR;

EXIT11:
    close(fd1);
EXIT10:
    unlinkat(dirFd2, pathname6, 0);
EXIT9:
    unlinkat(dirFd1, pathname5, 0);
    goto EXIT7;
EXIT8:
    close(fd);
EXIT7:
    unlinkat(dirFd0, pathname4, 0);
EXIT6:
    closedir(dir2);
EXIT5:
    rmdir(pathname3);
EXIT4:
    closedir(dir1);
EXIT3:
    rmdir(pathname2);
EXIT2:
    closedir(dir0);
EXIT1:
    rmdir(pathname1);
EXIT:
    return JFFS_NO_ERROR;
}

VOID ItFsTestLinkat002(VOID)
{
    TEST_ADD_CASE("IT_FS_TEST_LINKAT_002", testcase, TEST_VFS, TEST_JFFS, TEST_LEVEL0, TEST_FUNCTION);
}
