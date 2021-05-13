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

static INT32 DisplayInfo(const char *fpath, const struct stat *sb, int tflag, struct FTW *ftwbuf)
{
    printf("%-3s %2d ",
           (tflag == FTW_D) ?   "d"   : (tflag == FTW_DNR) ? "dnr" :
           (tflag == FTW_DP) ?  "dp"  : (tflag == FTW_F) ?   "f" :
           (tflag == FTW_NS) ?  "ns"  : (tflag == FTW_SL) ?  "sl" :
           (tflag == FTW_SLN) ? "sln" : "???",
           ftwbuf->level);

    if (tflag == FTW_NS) {
        printf("-------");
    } else {
        printf("%7jd", (intmax_t)sb->st_size);
    }

    printf("   %-40s %d %s\n", fpath, ftwbuf->base, fpath + ftwbuf->base);

    return 0;
}

static UINT32 Testcase(VOID)
{
    INT32 ret;
    INT32 flags = 0;
    CHAR *pathnamedir = { JFFS_MAIN_DIR0 };
    CHAR pathname01[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME01 };
    CHAR pathname11[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME11 };
    CHAR pathname02[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME02 };

    ret = mkdir(pathname01, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    ret = mkdir(pathname11, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT2);

    ret = mkdir(pathname02, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT3);

    ret = nftw(pathnamedir, DisplayInfo, 20, flags); // 20 means max fd
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT3);

    ret = rmdir(pathname02);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT3);

    ret = rmdir(pathname11);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT2);

    ret = rmdir(pathname01);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

    return JFFS_NO_ERROR;

EXIT3:
    rmdir(pathname02);
EXIT2:
    rmdir(pathname11);
EXIT1:
    rmdir(pathname01);
    return JFFS_IS_ERROR;
}

VOID ItJffs017(VOID)
{
    TEST_ADD_CASE(__FUNCTION__, Testcase, TEST_VFS, TEST_JFFS, TEST_LEVEL2, TEST_FUNCTION);
}

