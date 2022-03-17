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
    INT32 ret, i;
    INT32 fileCount = 0;
    INT32 fd[JFFS_NAME_LIMITTED_SIZE] = { -1 };
    CHAR pathname1[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };
    CHAR pathname2[JFFS_NAME_LIMITTED_SIZE][JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0, };
    CHAR bufname[JFFS_SHORT_ARRAY_LENGTH] = "";
    DIR *dir = NULL;

    ret = mkdir(pathname1, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    for (i = 0; i < JFFS_NAME_LIMITTED_SIZE; i++) {
        (void)memset_s(bufname, JFFS_SHORT_ARRAY_LENGTH, 0, JFFS_SHORT_ARRAY_LENGTH);
        (void)memset_s(pathname2[i], JFFS_SHORT_ARRAY_LENGTH, 0, JFFS_SHORT_ARRAY_LENGTH);
        snprintf_s(bufname, JFFS_SHORT_ARRAY_LENGTH - 1, JFFS_SHORT_ARRAY_LENGTH, "/test%d", i);
        JffsStrcat2(pathname2[i], bufname, strlen(bufname));

        if (fd[fileCount] == (CONFIG_NFILE_DESCRIPTORS - 1)) {
            fd[i] = open(pathname2[i], O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, HIGHEST_AUTHORITY);
            ICUNIT_GOTO_EQUAL(fd[i], -1, fd[i], EXIT3);
            ICUNIT_GOTO_EQUAL(errno, EMFILE, errno, EXIT3);
            continue;
        }
        fd[i] = open(pathname2[i], O_NONBLOCK | O_CREAT | O_RDWR | O_EXCL, HIGHEST_AUTHORITY);
        ICUNIT_GOTO_NOT_EQUAL(fd[i], -1, fd[i], EXIT3);
        fileCount = i;
    }

    for (i = 0; i < JFFS_NAME_LIMITTED_SIZE; i++) {
        if (i > fileCount) {
            ret = close(fd[i]);
            ICUNIT_GOTO_EQUAL(ret, -1, ret, EXIT3);
            ICUNIT_GOTO_EQUAL(errno, EBADF, errno, EXIT3);
            continue;
        }
        ret = close(fd[i]);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT3);
    }

    for (i = 0; i < JFFS_NAME_LIMITTED_SIZE; i++) {
        if (i > fileCount) {
            ret = remove(pathname2[i]);
            ICUNIT_GOTO_EQUAL(ret, -1, ret, EXIT2);
            ICUNIT_GOTO_EQUAL(errno, ENOENT, errno, EXIT2);
            continue;
        }

        ret = remove(pathname2[i]);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT2);
    }

    dir = opendir(pathname1);
    ICUNIT_GOTO_NOT_EQUAL(dir, NULL, dir, EXIT1);

    ret = closedir(dir);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    ret = rmdir(pathname1);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    return JFFS_NO_ERROR;
EXIT3:
    for (i = 0; i < JFFS_NAME_LIMITTED_SIZE; i++) {
        close(fd[i]);
    }
EXIT2:
    for (i = 0; i < JFFS_NAME_LIMITTED_SIZE; i++) {
        remove(pathname2[i]);
    }
EXIT1:
    closedir(dir);
EXIT:
    remove(pathname1);
    return JFFS_NO_ERROR;
}

VOID ItFsJffs071(VOID)
{
    TEST_ADD_CASE("IT_FS_JFFS_071", Testcase, TEST_VFS, TEST_JFFS, TEST_LEVEL2, TEST_FUNCTION);
}

