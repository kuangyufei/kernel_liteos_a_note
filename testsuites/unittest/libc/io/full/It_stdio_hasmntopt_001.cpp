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
#include "It_test_IO.h"

static UINT32 testcase(VOID)
{
    struct mntent* mnt = nullptr;
    struct mntent* mnt_new = nullptr;
    FILE *fp = nullptr;
    FILE *fp2 = nullptr;
    char *argv[2] = {nullptr};
    argv[0] = "/etc/fstab";
    argv[1] = "errors";
    char *opt = argv[1];
    char *ret = nullptr;

    char fileWords[] = "/dev/disk/by-uuid/c4992556-a86e-45e8-ba5f-190b16a9073x /usr1 ext3 errors=remount-ro,nofail 0 1";
    char *pathList[] = {"/etc/fstab"};
    char *streamList[] = {(char *)fileWords};
    int streamLen[] = {sizeof(fileWords)};

    int flag = PrepareFileEnv(pathList, streamList, streamLen, 1);
    if (flag != 0) {
        printf("error: need some env file, but prepare is not ok");
        (VOID)RecoveryFileEnv(pathList, 1);
        return -1;
    }

    mnt_new = (struct mntent *)malloc(sizeof(struct mntent));
    mnt_new->mnt_fsname = "UUID=c4992556-a86e-45e8-ba5f-190b16a9073x";
    mnt_new->mnt_dir = "/usr1";
    mnt_new->mnt_type = "ext3";
    mnt_new->mnt_opts = "errors=remount-ro,nofail";
    mnt_new->mnt_freq = 0;
    mnt_new->mnt_passno = 1;

    fp = setmntent("/etc/fstab", "r");
    if (!fp) {
        printf("fp=0x%x\n", fp);
        free(mnt_new);
        return LOS_NOK;
    }

    mnt = getmntent(fp);
    if (mnt && !(feof(fp) || ferror(fp))) {
        ret =  hasmntopt(mnt, opt);
        printf("hasmntopt=%s\n", ret);
        ICUNIT_ASSERT_NOT_EQUAL_NULL(ret, NULL, -1);
        mnt = getmntent(fp);
    }

    if (fp != NULL) {
        endmntent(fp);
    }

    /* test the addmntent below */
    fp = setmntent(argv[0], "a");
    fp2 = fopen("/lib/libc.so", "r");
    if (fp2 != NULL) {
        printf("aha I found you are OHOS!!!\n");
        if (addmntent(fp, mnt_new)) {
            printf("a new mnt is added to %s\n", argv[0]);
        }
    }

    if (fp != NULL) {
        endmntent(fp);
    }
    if (fp2 != NULL) {
        fclose(fp2);
    }
    (VOID)RecoveryFileEnv(pathList, 1);
    return LOS_OK;
}

VOID IT_STDIO_HASMNTOPT_001(void)
{
    TEST_ADD_CASE(__FUNCTION__, testcase, TEST_LIB, TEST_LIBC, TEST_LEVEL1, TEST_FUNCTION);
}
