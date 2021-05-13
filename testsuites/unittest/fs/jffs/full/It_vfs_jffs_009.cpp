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

static INT32 ScandirF01(const struct dirent *ent)
{
    // read out a file while the name begined with "test"
    return (strncmp(ent->d_name, "test", 4) == 0);
}
static UINT32 Testcase(VOID)
{
    INT32 fd = -1;
    INT32 fd1 = -1;
    INT32 ret, scandirCount;
    CHAR *pret = NULL;
    CHAR pathname[50] = { JFFS_PATH_NAME0 };
    DIR *dir = NULL;
    struct dirent *ptr = NULL;
    struct dirent **namelist = NULL;
    INT32 offset, offset1, offset2, offset3, offset4;

    ret = mkdir(pathname, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    strcat_s(pathname, sizeof(pathname), "/test");
    ret = mkdir(pathname, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT1);

    JffsStrcat2(pathname, "/testfile", 50); // 50 means name length
    fd = open(pathname, O_NONBLOCK | O_CREAT | O_RDWR, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_NOT_EQUAL(fd, -1, fd, EXIT2);

    JffsStrcat2(pathname, "/0test1", 50); // 50 means name length
    ret = mkdir(pathname, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT3);

    strcat_s(pathname, sizeof(pathname), "/test1file");
    fd1 = open(pathname, O_NONBLOCK | O_CREAT | O_RDWR, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_NOT_EQUAL(fd, -1, fd, EXIT4);

    JffsStrcat2(pathname, "/test2", 50); // 50 means name length
    ret = mkdir(pathname, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT5);

    dir = opendir(JFFS_PATH_NAME0);
    ICUNIT_GOTO_NOT_EQUAL(dir, NULL, dir, EXIT6);

    offset = telldir(dir);
    ptr = readdir(dir);
    offset1 = ptr->d_off;
    ICUNIT_GOTO_EQUAL(offset, offset1 - 1, offset, EXIT6);

    offset = telldir(dir);
    ptr = readdir(dir);
    offset2 = ptr->d_off;
    ICUNIT_GOTO_EQUAL(offset, offset2 - 1, offset, EXIT6);

    offset = telldir(dir);
    ptr = readdir(dir);
    offset3 = ptr->d_off;
    ICUNIT_GOTO_EQUAL(offset, offset3 - 1, offset, EXIT6);

    offset = telldir(dir);
    ptr = readdir(dir);
    offset4 = ptr->d_off;
    ICUNIT_GOTO_EQUAL(offset, offset4 - 1, offset, EXIT6);

    scandirCount = scandir(JFFS_PATH_NAME0, &namelist, ScandirF01, alphasort);
    ICUNIT_GOTO_EQUAL(scandirCount, 3, scandirCount, EXIT6); // 3 means total scaned file
    ICUNIT_GOTO_STRING_EQUAL(namelist[0]->d_name, "test", namelist[0]->d_name, EXIT7); // 0 means first file
    ICUNIT_GOTO_STRING_EQUAL(namelist[1]->d_name, "test2", namelist[1]->d_name, EXIT7); // 1 means second file
    ICUNIT_GOTO_STRING_EQUAL(namelist[2]->d_name, "testfile", namelist[2]->d_name, EXIT7); // 2 means third file
    ICUNIT_GOTO_EQUAL(namelist[0]->d_type, DT_DIR, namelist[0]->d_name, EXIT7); // 0 means first file

EXIT7:
    JffsScandirFree(namelist, scandirCount);
EXIT6:
    ret = closedir(dir);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT6);
EXIT5:
    JffsStrcat2(pathname, "/test2", sizeof(pathname));
    rmdir(pathname);
EXIT4:
    JffsStrcat2(pathname, "/0test1/test1file", sizeof(pathname));
    close(fd1);
    remove(pathname);
EXIT3:
    JffsStrcat2(pathname, "/0test1", sizeof(pathname));
    rmdir(pathname);
EXIT2:
    JffsStrcat2(pathname, "/testfile", sizeof(pathname));
    close(fd);
    remove(pathname);
EXIT1:
    JffsStrcat2(pathname, "/test", sizeof(pathname));
    rmdir(pathname);
EXIT:
    rmdir(JFFS_PATH_NAME0);
    return JFFS_NO_ERROR;
}

VOID ItFsJffs009(VOID)
{
    TEST_ADD_CASE("IT_FS_JFFS_009", Testcase, TEST_VFS, TEST_JFFS, TEST_LEVEL2, TEST_FUNCTION);
}

