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
    INT32 fd[10] = { -1 };
    INT32 ret;
    CHAR pathname[JFFS_NAME_LIMITTED_SIZE] = { JFFS_PATH_NAME0 };
    DIR *dir = NULL;
    struct dirent *ptr = NULL;
    INT32 offset;

    ret = mkdir(pathname, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    dir = opendir(pathname);
    ICUNIT_GOTO_NOT_EQUAL(dir, NULL, dir, EXIT0);

    ret = closedir(dir);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT0);

    strcat_s(pathname, JFFS_NAME_LIMITTED_SIZE, "/0testwe12rttyututututqweqqfsdfsdfsdf.exe");
    ret = mkdir(pathname, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT1);

    JffsStrcat2(pathname, "/0testwe12rttyututututqweqqfsdfsdfsdffsdf1234567890.TXT", JFFS_NAME_LIMITTED_SIZE);
    ret = mkdir(pathname, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT2);

    JffsStrcat2(pathname,
        "/0testwe12rttyututututqweqqfsdfsdfsdffsdf12345678900testwe12rttyututututqweqqfsdfsdfsdffsdf1234567890.123",
        JFFS_NAME_LIMITTED_SIZE);
    ret = mkdir(pathname, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT3);

    JffsStrcat2(pathname, "/12345678901234567890123456789012345678901234567890.123", JFFS_NAME_LIMITTED_SIZE);
    ret = mkdir(pathname, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT4);

    JffsStrcat2(pathname, "/ABCDEFGHJKLMNOPQRSTUVWXYZabcdefghjklmnopqrstuvwxyz.123", JFFS_NAME_LIMITTED_SIZE);
    ret = mkdir(pathname, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT5);

    JffsStrcat2(pathname, "/����ϵͳ������ȱ�δ�δ�.123", JFFS_NAME_LIMITTED_SIZE);
    ret = mkdir(pathname, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT6);

    JffsStrcat2(pathname, "/����ϵͳ������ȱ�δ�δ�123ttt.www", JFFS_NAME_LIMITTED_SIZE);
    ret = mkdir(pathname, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT7);

    JffsStrcat2(pathname, "/����ϵͳ������ȱ�δ�δ�123ttt.www", JFFS_NAME_LIMITTED_SIZE);
    fd[0] = open(pathname, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, HIGHEST_AUTHORITY); // 0 means first fd
    ICUNIT_GOTO_EQUAL(fd[0], -1, fd[0], EXIT7); // 0 means first fd

    JffsStrcat2(pathname, "/0testwe12rttyututututqweqqfsdfsdfsdf.exe", JFFS_NAME_LIMITTED_SIZE);
    fd[0] = open(pathname, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, HIGHEST_AUTHORITY); // 0 means first fd
    ICUNIT_GOTO_EQUAL(fd[0], -1, fd[0], EXIT7); // 0 means first fd

    JffsStrcat2(pathname, "/0testwe12rttyututututqweqqfsdfsdfsdf.txt", JFFS_NAME_LIMITTED_SIZE);
    fd[0] = open(pathname, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, HIGHEST_AUTHORITY); // 0 means first fd
    ICUNIT_GOTO_NOT_EQUAL(fd[0], -1, fd[0], EXIT8); // 0 means first fd

    JffsStrcat2(pathname, "/0testwe12rttyututututqweqqfsdfsdfsdffsdf1234567890.WWW", JFFS_NAME_LIMITTED_SIZE);
    fd[1] = open(pathname, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, HIGHEST_AUTHORITY); // 1 means second fd
    ICUNIT_GOTO_NOT_EQUAL(fd[1], -1, fd[1], EXIT9); // 1 means second fd

    JffsStrcat2(pathname,
        "/0testwe12rttyututututqweqqfsdfsdfsdffsdf12345678900testwe12rttyututututqweqqfsdfsdfsdffsdf1234567890.AAA",
        JFFS_NAME_LIMITTED_SIZE);
    fd[2] = open(pathname, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, HIGHEST_AUTHORITY); // 2 means third fd
    ICUNIT_GOTO_NOT_EQUAL(fd[2], -1, fd[2], EXIT10); // 2 means third fd

    JffsStrcat2(pathname, "/12345678901234567890123456789012345678901234567890.456", JFFS_NAME_LIMITTED_SIZE);
    fd[3] = open(pathname, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, HIGHEST_AUTHORITY); // 3 means four fd
    ICUNIT_GOTO_NOT_EQUAL(fd[3], -1, fd[3], EXIT11); // 3 means four fd

    JffsStrcat2(pathname, "/ABCDEFGHJKLMNOPQRSTUVWXYZabcdefghjklmnopqrstuvwxyz.456", JFFS_NAME_LIMITTED_SIZE);
    fd[4] = open(pathname, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, HIGHEST_AUTHORITY); // 4 means fifth fd
    ICUNIT_GOTO_NOT_EQUAL(fd[4], -1, fd[4], EXIT12); // 4 means fifth fd

    JffsStrcat2(pathname, "/����ϵͳ������ȱ�δ�δ�.123.456", JFFS_NAME_LIMITTED_SIZE);
    fd[5] = open(pathname, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, HIGHEST_AUTHORITY); // 5 means six fd
    ICUNIT_GOTO_NOT_EQUAL(fd[5], -1, fd[5], EXIT13); // 5 means six fd

    JffsStrcat2(pathname, "/����ϵͳ������ȱ�δ�δ�123ttt.www", JFFS_NAME_LIMITTED_SIZE);
    fd[6] = open(pathname, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, HIGHEST_AUTHORITY); // 6 means seven fd
    ICUNIT_GOTO_EQUAL(fd[6], -1, fd[6], EXIT13); // 6 means seven fd

    JffsStrcat2(pathname, "/����ϵͳ������ȱ�δ�δ�123ttt.AAA", JFFS_NAME_LIMITTED_SIZE);
    fd[6] = open(pathname, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, HIGHEST_AUTHORITY); // 6 means seven fd
    ICUNIT_GOTO_NOT_EQUAL(fd[6], -1, fd[6], EXIT14); // 6 means seven fd

    JffsStrcat2(pathname, "/����ϵͳ������ȱ�δ�δ�123ttt.AAA", JFFS_NAME_LIMITTED_SIZE);
    fd[7] = open(pathname, O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, HIGHEST_AUTHORITY); // 7 means eight fd
    ICUNIT_GOTO_EQUAL(fd[7], -1, fd[7], EXIT14); // 7 means eight fd

    dir = opendir(JFFS_PATH_NAME0);
    ICUNIT_GOTO_NOT_EQUAL(dir, NULL, dir, EXIT15);

    ptr = readdir(dir);
    ICUNIT_GOTO_NOT_EQUAL(ptr, NULL, ptr, EXIT15);

    ptr = readdir(dir);
    ICUNIT_GOTO_NOT_EQUAL(ptr, NULL, ptr, EXIT15);

    ptr = readdir(dir);
    ICUNIT_GOTO_NOT_EQUAL(ptr, NULL, ptr, EXIT15);

    ptr = readdir(dir);
    ICUNIT_GOTO_NOT_EQUAL(ptr, NULL, ptr, EXIT15);

    ptr = readdir(dir);
    ICUNIT_GOTO_NOT_EQUAL(ptr, NULL, ptr, EXIT15);

    ptr = readdir(dir);
    ICUNIT_GOTO_NOT_EQUAL(ptr, NULL, ptr, EXIT15);

    ptr = readdir(dir);
    ICUNIT_GOTO_NOT_EQUAL(ptr, NULL, ptr, EXIT15);

    ptr = readdir(dir);
    ICUNIT_GOTO_NOT_EQUAL(ptr, NULL, ptr, EXIT15);

    ptr = readdir(dir);
    ICUNIT_GOTO_NOT_EQUAL(ptr, NULL, ptr, EXIT15);

    ptr = readdir(dir);
    ICUNIT_GOTO_NOT_EQUAL(ptr, NULL, ptr, EXIT15);

    ptr = readdir(dir);
    ICUNIT_GOTO_NOT_EQUAL(ptr, NULL, ptr, EXIT15);

    ptr = readdir(dir);
    ICUNIT_GOTO_NOT_EQUAL(ptr, NULL, ptr, EXIT15);

    ptr = readdir(dir);
    ICUNIT_GOTO_NOT_EQUAL(ptr, NULL, ptr, EXIT15);

    ptr = readdir(dir);
    ICUNIT_GOTO_NOT_EQUAL(ptr, NULL, ptr, EXIT15);

    ptr = readdir(dir);
    ICUNIT_GOTO_EQUAL(ptr, NULL, ptr, EXIT15);

    ret = closedir(dir);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT15);
    LosTaskDelay(5); // 5 means timedelay len

EXIT14:
    JffsStrcat2(pathname, "/����ϵͳ������ȱ�δ�δ�123ttt.AAA", JFFS_NAME_LIMITTED_SIZE);
    ret = close(fd[6]); // 6 means seven fd
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT14_1);
EXIT14_1:
    ret = unlink(pathname);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT13);

EXIT13:
    JffsStrcat2(pathname, "/����ϵͳ������ȱ�δ�δ�.123.456", JFFS_NAME_LIMITTED_SIZE);
    ret = close(fd[5]); // 5 means six fd
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT13_1);
EXIT13_1:
    ret = unlink(pathname);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT12);
EXIT12:
    JffsStrcat2(pathname, "/ABCDEFGHJKLMNOPQRSTUVWXYZabcdefghjklmnopqrstuvwxyz.456", JFFS_NAME_LIMITTED_SIZE);
    ret = close(fd[4]); // 4 means fifth fd
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT12_1);
EXIT12_1:
    ret = unlink(pathname);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT11);
EXIT11:
    JffsStrcat2(pathname, "/12345678901234567890123456789012345678901234567890.456", JFFS_NAME_LIMITTED_SIZE);
    ret = close(fd[3]); // 3 means four fd
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT11_1);
EXIT11_1:
    ret = unlink(pathname);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT10);
EXIT10:
    JffsStrcat2(pathname,
        "/0testwe12rttyututututqweqqfsdfsdfsdffsdf12345678900testwe12rttyututututqweqqfsdfsdfsdffsdf1234567890.AAA",
        JFFS_NAME_LIMITTED_SIZE);
    ret = close(fd[2]); // 2 means third fd
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT10_1);
EXIT10_1:
    ret = unlink(pathname);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT9);
EXIT9:
    JffsStrcat2(pathname, "/0testwe12rttyututututqweqqfsdfsdfsdffsdf1234567890.WWW", JFFS_NAME_LIMITTED_SIZE);
    ret = close(fd[1]); // 1 means second fd
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT9_1);
EXIT9_1:
    ret = unlink(pathname);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT8);

EXIT8:
    JffsStrcat2(pathname, "/0testwe12rttyututututqweqqfsdfsdfsdf.txt", JFFS_NAME_LIMITTED_SIZE);
    ret = close(fd[0]); // 0 means first fd
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT8_1);
EXIT8_1:
    ret = unlink(pathname);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT7);

EXIT7:
    JffsStrcat2(pathname, "/����ϵͳ������ȱ�δ�δ�123ttt.www", JFFS_NAME_LIMITTED_SIZE);
    rmdir(pathname);

EXIT6:
    JffsStrcat2(pathname, "/����ϵͳ������ȱ�δ�δ�.123", JFFS_NAME_LIMITTED_SIZE);
    rmdir(pathname);
EXIT5:
    JffsStrcat2(pathname, "/ABCDEFGHJKLMNOPQRSTUVWXYZabcdefghjklmnopqrstuvwxyz.123", JFFS_NAME_LIMITTED_SIZE);
    rmdir(pathname);
EXIT4:
    JffsStrcat2(pathname, "/12345678901234567890123456789012345678901234567890.123", JFFS_NAME_LIMITTED_SIZE);
    rmdir(pathname);
EXIT3:
    JffsStrcat2(pathname,
        "/0testwe12rttyututututqweqqfsdfsdfsdffsdf12345678900testwe12rttyututututqweqqfsdfsdfsdffsdf1234567890.123",
        JFFS_NAME_LIMITTED_SIZE);
    rmdir(pathname);

EXIT2:
    JffsStrcat2(pathname, "/0testwe12rttyututututqweqqfsdfsdfsdffsdf1234567890.TXT", JFFS_NAME_LIMITTED_SIZE);
    rmdir(pathname);

EXIT1:
    JffsStrcat2(pathname, "/0testwe12rttyututututqweqqfsdfsdfsdf.exe", JFFS_NAME_LIMITTED_SIZE);
    rmdir(pathname);
EXIT:
    rmdir(JFFS_PATH_NAME0);
    return JFFS_NO_ERROR;
EXIT15:
    closedir(dir);
    goto EXIT14;
    return JFFS_NO_ERROR;
EXIT0:
    closedir(dir);
    goto EXIT;
}

VOID ItFsJffs051(VOID)
{
    TEST_ADD_CASE("IT_FS_JFFS_051", Testcase, TEST_VFS, TEST_JFFS, TEST_LEVEL2, TEST_FUNCTION);
}

