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
    INT32 fd, fd1, fd2, ret;
    CHAR pathname[30] = { JFFS_PATH_NAME0 };
    DIR *dir = NULL;
    struct dirent *ptr;
    INT32 offset, offset1, offset2, offset3, offset4;

    ret = mkdir(pathname, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    strcat(pathname, "/test0");
    ret = mkdir(pathname, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT1);

    JffsStrcat2(pathname, "/file1", sizeof(pathname));
    fd1 = open(pathname, O_NONBLOCK | O_CREAT | O_RDWR, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_NOT_EQUAL(fd, -1, fd, EXIT2);

    JffsStrcat2(pathname, "/file0", sizeof(pathname));
    fd = open(pathname, O_NONBLOCK | O_CREAT | O_RDWR, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_NOT_EQUAL(fd, -1, fd, EXIT3);

    JffsStrcat2(pathname, "/test2", sizeof(pathname));
    ret = mkdir(pathname, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT4);

    JffsStrcat2(pathname, "/file2", sizeof(pathname));
    fd2 = open(pathname, O_NONBLOCK | O_CREAT | O_RDWR, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_NOT_EQUAL(fd, -1, fd, EXIT5);

    JffsStrcat2(pathname, "/test1", sizeof(pathname));
    ret = mkdir(pathname, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT6);

    dir = opendir(JFFS_PATH_NAME0);
    ICUNIT_GOTO_NOT_EQUAL(dir, NULL, dir, EXIT7);

    offset1 = 0;
    ptr = readdir(dir);
    ICUNIT_GOTO_STRING_EQUAL(ptr->d_name, "file0", ptr->d_name, EXIT7);

    offset2 = ptr->d_off;
    ptr = readdir(dir);
    ICUNIT_GOTO_STRING_EQUAL(ptr->d_name, "file1", ptr->d_name, EXIT7);

    offset3 = ptr->d_off;
    ptr = readdir(dir);
    ICUNIT_GOTO_STRING_EQUAL(ptr->d_name, "file2", ptr->d_name, EXIT7);

    offset4 = ptr->d_off;
    ptr = readdir(dir);
    ICUNIT_GOTO_STRING_EQUAL(ptr->d_name, "test0", ptr->d_name, EXIT7);

    seekdir(dir, offset2);
    rewinddir(dir);
    offset = telldir(dir);
    ptr = readdir(dir);
    ICUNIT_GOTO_NOT_EQUAL(ptr, NULL, ptr, EXIT7);
    ICUNIT_GOTO_STRING_EQUAL(ptr->d_name, "file0", ptr->d_name, EXIT7);
    ICUNIT_GOTO_EQUAL(offset, offset1, offset, EXIT7);
    ICUNIT_GOTO_EQUAL(offset, ptr->d_off - 1, offset, EXIT7);

    seekdir(dir, offset1);
    rewinddir(dir);
    offset = telldir(dir);
    ptr = readdir(dir);
    ICUNIT_GOTO_NOT_EQUAL(ptr, NULL, ptr, EXIT7);
    ICUNIT_GOTO_STRING_EQUAL(ptr->d_name, "file0", ptr->d_name, EXIT7);
    ICUNIT_GOTO_EQUAL(offset, offset1, offset, EXIT7);
    ICUNIT_GOTO_EQUAL(offset, ptr->d_off - 1, offset, EXIT7);

    seekdir(dir, offset4);
    rewinddir(dir);
    offset = telldir(dir);
    ptr = readdir(dir);
    ICUNIT_GOTO_NOT_EQUAL(ptr, NULL, ptr, EXIT7);
    ICUNIT_GOTO_STRING_EQUAL(ptr->d_name, "file0", ptr->d_name, EXIT7);
    ICUNIT_GOTO_EQUAL(offset, offset1, offset, EXIT7);
    ICUNIT_GOTO_EQUAL(offset, ptr->d_off - 1, offset, EXIT7);

    seekdir(dir, offset3);
    rewinddir(dir);
    offset = telldir(dir);
    ptr = readdir(dir);
    ICUNIT_GOTO_NOT_EQUAL(ptr, NULL, ptr, EXIT7);
    ICUNIT_GOTO_STRING_EQUAL(ptr->d_name, "file0", ptr->d_name, EXIT7);
    ICUNIT_GOTO_EQUAL(offset, offset1, offset, EXIT7);
    ICUNIT_GOTO_EQUAL(offset, ptr->d_off - 1, offset, EXIT7);

EXIT7:
    ret = closedir(dir);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT7);
EXIT6:
    JffsStrcat2(pathname, "/test1", sizeof(pathname));
    rmdir(pathname);
EXIT5:
    JffsStrcat2(pathname, "/file2", sizeof(pathname));
    close(fd2);
    remove(pathname);
EXIT4:
    JffsStrcat2(pathname, "/test2", sizeof(pathname));
    rmdir(pathname);
EXIT3:
    JffsStrcat2(pathname, "/file0", sizeof(pathname));
    close(fd);
    remove(pathname);
EXIT2:
    JffsStrcat2(pathname, "/file1", sizeof(pathname));
    close(fd1);
    remove(pathname);
EXIT1:
    JffsStrcat2(pathname, "/test0", sizeof(pathname));
    rmdir(pathname);
EXIT:
    rmdir(JFFS_PATH_NAME0);
    return JFFS_NO_ERROR;
}

VOID ItFsJffs005(VOID)
{
    TEST_ADD_CASE("IT_FS_JFFS_005", testcase, TEST_VFS, TEST_JFFS, TEST_LEVEL0, TEST_FUNCTION);
}

