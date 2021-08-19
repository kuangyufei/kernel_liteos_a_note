/*
 * Copyright (c) 2013-2019 Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020-2021 Huawei Device Co., Ltd. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this list of
 * conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list
 * of conditions and the following disclaimer in the documentation and/or other materials
 * provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors may be used
 * to endorse or promote products derived from this software without specific prior written
 * permission.
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

#define MOUNT_FILEPATH "/storage/mounts"
#define TESTFILE "/storage/hellomnt"
#define MAXFD 512
#define MINFD 8

static int TestCase0(void)
{
    CHAR pathname1[JFFS_STANDARD_NAME_LENGTH] = MOUNT_FILEPATH;
    struct mntent *mnt = NULL;
    FILE *fp = NULL;
    int ret = -1;
    struct mntent mountsData = {
        .mnt_fsname = "jffs",
        .mnt_dir = "/",
        .mnt_type = "jffs",
        .mnt_opts = nullptr,
        .mnt_freq = 0,
        .mnt_passno = 0,
    };

    fp = setmntent(pathname1, "r+");
    ICUNIT_GOTO_NOT_EQUAL(fp, NULL, fp, EXIT);

    mnt = getmntent(fp);
    ICUNIT_GOTO_NOT_EQUAL(mnt, NULL, mnt, EXIT);

    ret = strcmp(mnt->mnt_fsname, mountsData.mnt_fsname);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    ret = strcmp(mnt->mnt_dir, mountsData.mnt_dir);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    ret = strcmp(mnt->mnt_type, mountsData.mnt_type);
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    ICUNIT_GOTO_EQUAL(mnt->mnt_freq, mountsData.mnt_freq, mnt.mnt_freq, EXIT);
    ICUNIT_GOTO_EQUAL(mnt->mnt_passno, mountsData.mnt_passno, mnt.mnt_passno, EXIT);

    endmntent(fp);
    return JFFS_NO_ERROR;

EXIT:
    endmntent(fp);
    return JFFS_IS_ERROR;
}

static int TestCase1(void)
{
    int ret = -1;
    FILE *fp = NULL;
    int i = 0;
    int fd =-1;
    /* EINVAL */
    fp = setmntent(MOUNT_FILEPATH, "+r");
    ICUNIT_GOTO_EQUAL(fp, NULL, fp, EXIT);
    ICUNIT_GOTO_EQUAL(errno, EINVAL, fp, EXIT);

    /* EISDIR */
    fp = setmntent("./", "r+");
    ICUNIT_GOTO_EQUAL(fp, NULL, fp, EXIT);
    ICUNIT_GOTO_EQUAL(errno, EISDIR, fp, EXIT);
    /* ENOENT */
    fp = setmntent("/storage/testmnt", "r+");
    ICUNIT_GOTO_EQUAL(fp, NULL, fp, EXIT);
    ICUNIT_GOTO_EQUAL(errno, ENOENT, fp, EXIT);
    remove(TESTFILE);

    return JFFS_NO_ERROR;
EXIT:
    remove(TESTFILE);
    return JFFS_IS_ERROR;
}

static int TestCase(void)
{
    int ret = -1;
    ret = TestCase0();
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);
    ret = TestCase1();
    ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

    return JFFS_NO_ERROR;
EXIT:
    return JFFS_IS_ERROR;
}

void ItTestFsJffs004(void)
{
    TEST_ADD_CASE("It_Test_Fs_Jffs_004", TestCase, TEST_VFS, TEST_JFFS, TEST_LEVEL0, TEST_FUNCTION);
}

