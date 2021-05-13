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

static UINT32 Testcase(VOID)
{
    INT32 fd = -1;
    INT32 fd1 = -1;
    INT32 fd2 = -1;
    INT32 fd3 = -1;
    INT32 ret, scandirCount;
    CHAR *pret = NULL;
    CHAR pathname[50] = { JFFS_PATH_NAME0 };
    CHAR buf[50] = "";
    DIR *dir = NULL;
    struct dirent *ptr = NULL;
    struct stat statfile;
    struct statfs statfsfile;
    struct dirent **namelist = NULL;

    ret = mkdir(pathname, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ret = chdir(pathname);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    pret = getcwd(buf, 20); // 20 means path name len
    ICUNIT_GOTO_NOT_EQUAL(pret, NULL, pret, EXIT);
    ICUNIT_GOTO_STRING_EQUAL(buf, pathname, buf, EXIT);

    strcat_s(pathname, sizeof(pathname), "/0test");
    ret = mkdir(pathname, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT1);

    JffsStrcat2(pathname, "/0file", sizeof(pathname));
    fd = open(pathname, O_NONBLOCK | O_CREAT | O_RDWR, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_NOT_EQUAL(fd, -1, fd, EXIT2);

    JffsStrcat2(pathname, "/2test", sizeof(pathname));
    ret = mkdir(pathname, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT3);

    strcat_s(pathname, sizeof(pathname), "/1file");
    fd1 = open(pathname, O_NONBLOCK | O_CREAT | O_RDWR, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_NOT_EQUAL(fd1, -1, fd1, EXIT4);

    JffsStrcat2(pathname, "/1test", sizeof(pathname));
    ret = mkdir(pathname, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT5);

    JffsStrcat2(pathname, "/2test", sizeof(pathname));
    ret = chdir(pathname);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT5);

    pret = getcwd(buf, 20); // 20 means path name len
    ICUNIT_GOTO_NOT_EQUAL(pret, NULL, pret, EXIT5);
    ICUNIT_GOTO_STRING_EQUAL(buf, pathname, buf, EXIT5);

    fd2 = open("2file", O_NONBLOCK | O_CREAT | O_RDWR, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_NOT_EQUAL(fd2, -1, fd2, EXIT6);

    fd3 = open("2file", O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(fd3, -1, fd3, EXIT6);

    ret = mkdir("3dir", HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT7);

    ret = mkdir("3dir", HIGHEST_AUTHORITY);
    ICUNIT_GOTO_NOT_EQUAL(ret, 0, ret, EXIT7);

    dir = opendir("3dir");
    ICUNIT_GOTO_NOT_EQUAL(dir, NULL, dir, EXIT8);

    scandirCount = scandir(".", &namelist, 0, alphasort);
    ICUNIT_GOTO_EQUAL(scandirCount, 3, scandirCount, EXIT8); // 3 means scan file num
    ICUNIT_GOTO_STRING_EQUAL(namelist[0]->d_name, "1file", namelist[0]->d_name, EXIT9); // 0 means first file

    ret = rename("2file", "file2");
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT10);

    ret = stat("file2", &statfile);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT10);
    ICUNIT_GOTO_EQUAL(statfile.st_size, 0, statfile.st_size, EXIT10);

    ret = statfs("file2", &statfsfile);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT10);

    ret = access("file2", R_OK);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT10);

    ret = close(fd2);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT10);

    ret = unlink("file2");
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT10);

    ret = chdir("3dir");
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT9);
    strcat_s(pathname, sizeof(pathname), "/3dir");
    pret = getcwd(buf, 30); // 30 means path name len
    ICUNIT_GOTO_NOT_EQUAL(pret, NULL, pret, EXIT9);
    ICUNIT_GOTO_STRING_EQUAL(buf, pathname, buf, EXIT9);

    ret = chdir("../");
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT9);
    JffsStrcat2(pathname, "/2test", sizeof(pathname));
    pret = getcwd(buf, 20); // 20 means path name len
    ICUNIT_GOTO_NOT_EQUAL(pret, NULL, pret, EXIT9);
    ICUNIT_GOTO_STRING_EQUAL(buf, pathname, buf, EXIT9);

EXIT10:
    JffsStrcat2(pathname, "/2test/file2", sizeof(pathname));
    close(fd2);
    remove(pathname);
EXIT9:
    JffsScandirFree(namelist, scandirCount);
EXIT8:
    ret = closedir(dir);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT8);
EXIT7:
    JffsStrcat2(pathname, "/2test/3dir", sizeof(pathname));
    rmdir(pathname);
EXIT6:
    JffsStrcat2(pathname, "/2test/2file", sizeof(pathname));
    close(fd2);
    close(fd3);
    remove(pathname);
EXIT5:
    JffsStrcat2(pathname, "/1test", sizeof(pathname));
    rmdir(pathname);
EXIT4:
    JffsStrcat2(pathname, "/2test/1file", sizeof(pathname));
    close(fd1);
    remove(pathname);
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


VOID ItFsJffs007(VOID)
{
    TEST_ADD_CASE("IT_FS_JFFS_007", Testcase, TEST_VFS, TEST_JFFS, TEST_LEVEL2, TEST_FUNCTION);
}

