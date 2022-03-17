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
    INT32 ret1, ret2;
    INT32 fd = -1;
    CHAR pathname1[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME01 };
    struct mntent *mnt = NULL;
    FILE *fp = NULL;

    ret1 = strcat_s(pathname1, JFFS_STANDARD_NAME_LENGTH, "test12");
    ICUNIT_ASSERT_EQUAL(ret1, EOK, ret1);

    fp = setmntent(pathname1, "w+b");
    ICUNIT_GOTO_NOT_EQUAL(fp, NULL, fp, EXIT);

    mnt = getmntent(fp);
    ICUNIT_GOTO_NOT_EQUAL(mnt, NULL, mnt, EXIT);

    printf("[%s:%d] mnt->mnt_fsname=%s\n", __FUNCTION__, __LINE__, mnt->mnt_fsname);
    printf("[%s:%d] mnt->mnt_dir=%s\n", __FUNCTION__, __LINE__, mnt->mnt_dir);
    printf("[%s:%d] mnt->mnt_type=%s\n", __FUNCTION__, __LINE__, mnt->mnt_type);
    printf("[%s:%d] mnt->mnt_opts=%s\n", __FUNCTION__, __LINE__, mnt->mnt_opts);
    printf("[%s:%d] mnt->mnt_freq=%d\n", __FUNCTION__, __LINE__, mnt->mnt_freq);
    printf("[%s:%d] mnt->mnt_passno=%d\n", __FUNCTION__, __LINE__, mnt->mnt_passno);

    endmntent(fp);
    unlink(pathname1);
    return JFFS_NO_ERROR;
EXIT:
    endmntent(fp);
    unlink(pathname1);
    return JFFS_NO_ERROR;
}

VOID ItJffs006(VOID)
{
    TEST_ADD_CASE(__FUNCTION__, Testcase, TEST_VFS, TEST_JFFS, TEST_LEVEL2, TEST_FUNCTION);
}

