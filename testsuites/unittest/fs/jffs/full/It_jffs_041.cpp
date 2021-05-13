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
typedef struct __dirstream {
    off_t tell;
    int fd;
    int bufPos;
    int bufEnd;
    volatile int lock[1];
    void *dirk;
    void *resv;
    /* Any changes to this struct must preserve the property:
     * offsetof(struct __dirent, buf) % sizeof(off_t) == 0 */
    char buf[2048];
} DIR;

static UINT32 Testcase(VOID)
{
    INT32 ret, scandirCount;
    INT32 fd = -1;
    CHAR pathname[50] = { JFFS_PATH_NAME0 };
    DIR *dir = NULL;
    DIR adir = { 0 };
    DIR *dir1 = &adir;
    struct dirent *ptr = NULL;
    off_t offset, offset1, offset2, offset3, offset4;
    struct dirent **namelist = NULL;

    errno = 0;
    scandirCount = scandir(JFFS_PATH_NAME0, &namelist, 0, NULL);
    ICUNIT_GOTO_EQUAL(scandirCount, -1, scandirCount, EXIT);
    ICUNIT_GOTO_EQUAL(errno, ENOENT, errno, EXIT);

    ret = mkdir(pathname, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    strcat_s(pathname, sizeof(pathname), "/0test");
    ret = mkdir(pathname, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT1);

    JffsStrcat2(pathname, "/0file", sizeof(pathname));
    fd = open(pathname, O_NONBLOCK | O_CREAT | O_RDWR, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_NOT_EQUAL(fd, -1, fd, EXIT2);

    JffsStrcat2(pathname, "/2test", sizeof(pathname));
    ret = mkdir(pathname, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT3);

    JffsStrcat2(pathname, "/1test", sizeof(pathname));
    ret = mkdir(pathname, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT4);

    dir = opendir(JFFS_PATH_NAME0);
    ICUNIT_GOTO_NOT_EQUAL(dir, NULL, dir, EXIT5);

    ret = closedir(dir);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT5);

    offset1 = telldir(dir1);
    ptr = readdir(dir1);
    ICUNIT_GOTO_EQUAL(ptr, NULL, ptr, EXIT4);
    ICUNIT_GOTO_EQUAL(errno, EBADF, errno, EXIT4);

    offset2 = telldir(dir1);
    ptr = readdir(dir1);
    ICUNIT_GOTO_EQUAL(ptr, NULL, ptr, EXIT4);
    ICUNIT_GOTO_EQUAL(errno, EBADF, errno, EXIT4);

    offset3 = telldir(dir1);
    ptr = readdir(dir1);
    ICUNIT_GOTO_EQUAL(ptr, NULL, ptr, EXIT4);
    ICUNIT_GOTO_EQUAL(errno, EBADF, errno, EXIT4);

    offset4 = telldir(dir1);
    ptr = readdir(dir1);
    ICUNIT_GOTO_EQUAL(ptr, NULL, ptr, EXIT4);
    ICUNIT_GOTO_EQUAL(errno, EBADF, errno, EXIT4);

    dir = opendir(JFFS_PATH_NAME0);
    ICUNIT_GOTO_NOT_EQUAL(dir, NULL, dir, EXIT5);

    ptr = readdir(dir);
    offset1 = ptr->d_off;

    ptr = readdir(dir);
    offset2 = ptr->d_off;

    ptr = readdir(dir);
    offset3 = ptr->d_off;

    ptr = readdir(dir);
    offset4 = ptr->d_off;

    seekdir(dir, offset2);
    offset = telldir(dir);
    ICUNIT_GOTO_EQUAL(offset, offset2, offset, EXIT5);
    ptr = readdir(dir);
    ICUNIT_GOTO_NOT_EQUAL(ptr, NULL, ptr, EXIT5);
    ICUNIT_GOTO_STRING_EQUAL(ptr->d_name, "1test", ptr->d_name, EXIT5);
    ICUNIT_GOTO_EQUAL(offset, ptr->d_off - 1, offset, EXIT5);

    seekdir(dir, offset1);
    offset = telldir(dir);
    ICUNIT_GOTO_EQUAL(offset, offset1, offset, EXIT5);
    ptr = readdir(dir);
    ICUNIT_GOTO_NOT_EQUAL(ptr, NULL, ptr, EXIT5);
    ICUNIT_GOTO_STRING_EQUAL(ptr->d_name, "0file", ptr->d_name, EXIT5);
    ICUNIT_GOTO_EQUAL(offset, ptr->d_off - 1, offset, EXIT5);

    seekdir(dir, offset4);
    offset = telldir(dir);
    ICUNIT_GOTO_EQUAL(offset, offset4, offset, EXIT5);
    ptr = readdir(dir);
    ICUNIT_GOTO_EQUAL(ptr, NULL, ptr, EXIT5);

    seekdir(dir, offset3);
    offset = telldir(dir);
    ICUNIT_GOTO_EQUAL(offset, offset3, offset, EXIT5);
    ptr = readdir(dir);
    ICUNIT_GOTO_NOT_EQUAL(ptr, NULL, ptr, EXIT5);
    ICUNIT_GOTO_STRING_EQUAL(ptr->d_name, "0test", ptr->d_name, EXIT5);
    ICUNIT_GOTO_EQUAL(offset, ptr->d_off - 1, offset, EXIT5);

    scandirCount = scandir(JFFS_PATH_NAME0, &namelist, 0, alphasort);
    ICUNIT_GOTO_EQUAL(scandirCount, 4, scandirCount, EXIT5); // 4 means total scaned file
    ICUNIT_GOTO_STRING_EQUAL(namelist[0]->d_name, "0file", namelist[0]->d_name, EXIT); // 0 means first file
    ICUNIT_GOTO_STRING_EQUAL(namelist[1]->d_name, "0test", namelist[1]->d_name, EXIT); // 1 means second file
    ICUNIT_GOTO_STRING_EQUAL(namelist[2]->d_name, "1test", namelist[2]->d_name, EXIT); // 2 means third file
    ICUNIT_GOTO_STRING_EQUAL(namelist[3]->d_name, "2test", namelist[3]->d_name, EXIT); // 3 means four file

    seekdir(dir, namelist[3]->d_off); // 3 means four file
    offset = telldir(dir);
    ICUNIT_GOTO_NOT_EQUAL(offset, -1, offset, EXIT5);
    ICUNIT_GOTO_EQUAL(offset1, namelist[3]->d_off, offset1, EXIT5); // 3 means four file
    ICUNIT_GOTO_EQUAL(offset, namelist[3]->d_off, offset, EXIT5); // 3 means four file
    ptr = readdir(dir);
    ICUNIT_GOTO_NOT_EQUAL(ptr, NULL, ptr, EXIT5);
    ICUNIT_GOTO_STRING_EQUAL(ptr->d_name, "0file", ptr->d_name, EXIT5);

    seekdir(dir, namelist[2]->d_off); // 2 means third file
    offset = telldir(dir);
    ICUNIT_GOTO_NOT_EQUAL(offset, -1, offset, EXIT5);
    ICUNIT_GOTO_EQUAL(offset3, namelist[2]->d_off, offset3, EXIT5); // 2 means third file
    ICUNIT_GOTO_EQUAL(offset, namelist[2]->d_off, offset, EXIT5); // 2 means third file
    ptr = readdir(dir);
    ICUNIT_GOTO_NOT_EQUAL(ptr, NULL, ptr, EXIT5);
    ICUNIT_GOTO_STRING_EQUAL(ptr->d_name, "0test", ptr->d_name, EXIT5);

    JffsScandirFree(namelist, scandirCount);

    scandirCount = 0;

    scandirCount = scandir(JFFS_PATH_NAME0, &namelist, 0, NULL);
    ICUNIT_GOTO_EQUAL(scandirCount, 4, scandirCount, EXIT5); // 4 means total scaned file

    seekdir(dir, namelist[3]->d_off); // 3 means four file
    ICUNIT_GOTO_EQUAL(offset4, namelist[3]->d_off, offset4, EXIT5); // 3 means four file
    ptr = readdir(dir);
    ICUNIT_GOTO_EQUAL(ptr, NULL, ptr, EXIT5);

    seekdir(dir, namelist[0]->d_off); // 0 means first file
    ICUNIT_GOTO_EQUAL(offset1, namelist[0]->d_off, offset1, EXIT5); // 0 means first file
    ptr = readdir(dir);
    ICUNIT_GOTO_NOT_EQUAL(ptr, NULL, ptr, EXIT);
    ICUNIT_GOTO_STRING_EQUAL(ptr->d_name, "0file", ptr->d_name, EXIT5);

EXIT6:
    JffsScandirFree(namelist, scandirCount);
EXIT5:
    ret = closedir(dir);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT5);
EXIT4:
    JffsStrcat2(pathname, "/1test", sizeof(pathname));
    rmdir(pathname);
EXIT3:
    JffsStrcat2(pathname, "/2test", sizeof(pathname));
    rmdir(pathname);
EXIT2:
    JffsStrcat2(pathname, "/0file", sizeof(pathname));
    close(fd);
    remove(pathname);
EXIT1:
    JffsStrcat2(pathname, "/0test", sizeof(pathname));
    rmdir(pathname);
EXIT:
    rmdir(JFFS_PATH_NAME0);
    return JFFS_NO_ERROR;
}

VOID ItJffs041(VOID)
{
    TEST_ADD_CASE(__FUNCTION__, Testcase, TEST_VFS, TEST_JFFS, TEST_LEVEL2, TEST_FUNCTION);
}

