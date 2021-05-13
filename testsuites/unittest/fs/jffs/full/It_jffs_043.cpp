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
    DIR *dirp = NULL;
    INT32 ret;
    CHAR *pathnamedir = (CHAR *)JFFS_MAIN_DIR0;
    CHAR pathname01[JFFS_STANDARD_NAME_LENGTH] = { JFFS_MAIN_DIR0 };
    struct dirent *dp1 = (struct dirent *)malloc(sizeof(struct dirent));
    struct dirent *dp2 = (struct dirent *)malloc(sizeof(struct dirent));
    struct dirent *dp2Bak = dp2;
    DIR adir = { 0 };
    DIR *dir1 = &adir;

    strcat_s(pathname01, JFFS_STANDARD_NAME_LENGTH, "/test1");
    ret = mkdir(pathname01, HIGHEST_AUTHORITY);
    if (ret != 0) {
        if (errno != EEXIST) {
            ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);
        }
    }

    dirp = opendir(pathnamedir);
    ICUNIT_GOTO_NOT_EQUAL(dirp, NULL, dirp, EXIT2);

    while (1) {
        ret = readdir_r(dir1, dp1, &dp2);
        printf("ret=%d\n", ret);
        ICUNIT_GOTO_EQUAL(errno, EBADF, errno, EXIT2);
        break;
    }

    ret = rmdir(pathname01);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT2);

    closedir(dirp);
    free(dp1);
    free(dp2Bak);
    return JFFS_NO_ERROR;
EXIT2:
    rmdir(pathname01);
    if (dirp) closedir(dirp);
    free(dp1);
    free(dp2Bak);
    return JFFS_IS_ERROR;
EXIT1:
    rmdir(pathname01);
    return JFFS_IS_ERROR;
}

VOID ItJffs043(VOID)
{
    TEST_ADD_CASE(__FUNCTION__, Testcase, TEST_VFS, TEST_JFFS, TEST_LEVEL2, TEST_FUNCTION);
}

