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

static uid_t g_testUID1 = 111;
static uid_t g_testUID2 = 222;
static char *g_testDIR = "TEST_D";

/* umask */
static int TestUmask(const char *path)
{
    mode_t mode;
    int ret;
    struct stat buf = { 0 };
    char filename[128] = {0};

    printf("%s, %d\n", __FUNCTION__, __LINE__);

    mode = umask(0044); // umask: 0044
    ICUNIT_ASSERT_EQUAL(mode, 0022, mode); // mode: 0022
    mode = umask(0022); // umask: 0022
    ICUNIT_ASSERT_EQUAL(mode, 0044, mode); // mode: 0044
    sprintf(filename, "%s/%s", path, g_testDIR);
    ret = mkdir(filename, HIGHEST_AUTHORITY);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ret = stat(filename, &buf);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ICUNIT_ASSERT_EQUAL(buf.st_mode, 040755, buf.st_mode);
    rmdir(filename);
    return 0;
}

/* open */
static int TestOpen(const char *path)
{
    int ret;
    char filename[64] = {0};
    char filename1[64] = {0};
    char filename2[64] = {0};
    char filename3[64] = {0};

    ret = setuid(g_testUID1);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ret = setgid(g_testUID1);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    // uid 111, gid 111,home dir 0 0 757

    sprintf(filename, "%s/%s", path, g_testDIR);
    ret = mkdir(filename, 0753); // mode: 0753
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    sprintf(filename1, "%s/%s", filename, "f1");
    ret = open(filename1, O_CREAT | O_WRONLY, 0755); // mode: 0755
    ICUNIT_ASSERT_NOT_EQUAL(ret, -1, ret);
    close(ret);

    ret = setuid(g_testUID2);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ret = setgid(g_testUID1);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    // uid 222, gid 111,home dir 111 111 753
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    sprintf(filename2, "%s/%s", filename, "f2");
    ret = open(filename2, O_CREAT | O_WRONLY, 0755); // mode: 0755
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);
    ret = open(filename1, O_WRONLY);
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);
    ICUNIT_ASSERT_EQUAL(EACCES, errno, errno);

    ret = setgid(g_testUID2);
    // uid 222, gid 222,home dir 111 111 753
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    sprintf(filename3, "%s/%s", filename, "f3");
    ret = open(filename3, O_CREAT | O_WRONLY, 0755); // mode: 0755
    printf("%s, %d ret %d\n", __FUNCTION__, __LINE__, ret);
    ICUNIT_ASSERT_NOT_EQUAL(ret, -1, ret);
    close(ret);
    ret = unlink(filename3);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    // file mode 755
    ret = open(filename1, O_WRONLY);
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);
    ret = open(filename1, O_RDONLY);
    ICUNIT_ASSERT_NOT_EQUAL(ret, -1, ret);
    close(ret);

    ret = setuid(g_testUID1);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ret = setgid(g_testUID1);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ret = unlink(filename1);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ret = setuid(0);
    ret = setgid(0);
    ret = rmdir(filename);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    return 0;
}

/* unlink */
static int TestUnlink(const char *path)
{
    int ret;
    char filename[64] = {0};
    char filename1[64] = {0};

    printf("%s, %d\n", __FUNCTION__, __LINE__);
    // uid 0, gid 0, home dir 0 0 757
    sprintf(filename, "%s/%s", path, g_testDIR);
    ret = mkdir(filename, 0757); // mode 757
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    sprintf(filename1, "%s/%s", filename, "f1");

    ret = open(filename1, O_CREAT | O_WRONLY, 0755); // mode: 0755
    ICUNIT_ASSERT_NOT_EQUAL(ret, -1, ret);
    close(ret);

    ret = setuid(g_testUID2);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    // uid 222, gid 0
    ret = unlink(filename1);
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);
    ret = setgid(g_testUID2);
    // uid 222, gid 222
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = unlink(filename1);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = rmdir(filename);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ret = setuid(0);
    ret = setgid(0);

    return 0;
}

/* mkdir */
/* rmdir */
static int TestMkdir(const char *path)
{
    int ret;
    char filename[64] = {0};

    printf("%s, %d\n", __FUNCTION__, __LINE__);
    // uid 0, gid 0, home dir 0 0 757
    sprintf(filename, "%s/%s", path, g_testDIR);
    ret = mkdir(filename, 0735); // mode 735
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = setuid(g_testUID2);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ret = rmdir(filename);
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);

    ret = setgid(g_testUID2);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ret = rmdir(filename);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = setuid(0);
    ret = setgid(0);

    return 0;
}

/* chmod */
static int TestChmod(const char *path)
{
    int ret;
    struct stat buf = { 0 };
    char filename[64] = {0};
    char filename1[64] = {0};

    printf("%s, %d\n", __FUNCTION__, __LINE__);
    // uid 0, gid 0, home dir 0 0 757
    sprintf(filename, "%s/%s", path, g_testDIR);
    ret = mkdir(filename, 0757); // mode 757
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    sprintf(filename1, "%s/%s", filename, "f1");

    setuid(g_testUID2);
    ret = chmod(filename, 0111); // mode: 0111
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);
    setuid(0);
    ret = chmod(filename, 0111); // mode: 0111
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = stat(filename, &buf);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ICUNIT_ASSERT_EQUAL(buf.st_mode, 040111, buf.st_mode); // mode: 040111

    ret = rmdir(filename);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ret = setuid(0);
    ret = setgid(0);

    return 0;
}

/* stat chdir */
static int TestStatChdir(const char *path)
{
    int ret;
    struct stat buf = { 0 };
    char filename[64] = {0};
    char filename1[64] = {0};

    printf("%s, %d\n", __FUNCTION__, __LINE__);
    // uid 0, gid 0, home dir 0 0 757
    sprintf(filename, "%s/%s", path, g_testDIR);
    ret = mkdir(filename, 0700); // mode: 0700
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    sprintf(filename1, "%s/%s", filename, "f1");

    ret = mkdir(filename1, 0755); // mode: 0755
    ICUNIT_ASSERT_NOT_EQUAL(ret, -1, ret);

    ret = setuid(g_testUID2);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    // uid 222, gid 0
    ret = chdir(filename);
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);
    ICUNIT_ASSERT_EQUAL(EACCES, errno, errno);
    ret = stat(filename1, &buf);
    printf("%s, %d ret %d errno %d\n", __FUNCTION__, __LINE__, ret, errno);
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);

    ret = setuid(0);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    // uid 222, gid 0
    ret = chdir(filename);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ret = stat(filename1, &buf);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = rmdir(filename1);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = chdir(JFFS_MAIN_DIR0);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = rmdir(filename);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ret = setuid(0);
    ret = setgid(0);

    return 0;
}

/* rename */
static int TestRename(const char *path)
{
    int ret;
    char filename[64] = {0};
    char filename1[64] = {0};
    char filename2[64] = {0};

    ret = setuid(g_testUID1);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ret = setgid(g_testUID1);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    // uid 111, gid 111,home dir 0 0 757

    sprintf(filename, "%s/%s", path, g_testDIR);
    ret = mkdir(filename, 0711); // mode: 0711
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    sprintf(filename1, "%s/%s", filename, "f1");
    ret = mkdir(filename1, 0755); // mode: 0755
    ICUNIT_ASSERT_NOT_EQUAL(ret, -1, ret);
    close(ret);

    sprintf(filename2, "%s/%s", filename, "f2");
    ret = rename(filename1, filename2);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = setuid(g_testUID2);
    // uid 222, gid 222,home dir 111 111 753
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    sprintf(filename2, "%s/%s", filename, "f2");
    ret = rename(filename2, filename1);
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);

    ret = setuid(g_testUID1);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ret = setgid(g_testUID1);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ret = rmdir(filename2);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ret = setuid(0);
    ret = setgid(0);
    ret = rmdir(filename);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    return 0;
}


/* execve */
/* access */
static int TestAccess(const char *path)
{
    int ret;
    char filename[64] = {0};

    printf("%s, %d\n", __FUNCTION__, __LINE__);
    // uid 0, gid 0, home dir 0 0 757
    sprintf(filename, "%s/%s", path, g_testDIR);
    ret = mkdir(filename, 0757); // mode 757
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = access(filename, F_OK);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ret = access(filename, R_OK);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ret = chmod(filename, 0311); // mode: 0311
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ret = access(filename, R_OK);
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);
    ret = access(filename, W_OK);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ret = chmod(filename, 0111); // mode: 0111
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ret = access(filename, W_OK);
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);
    ret = access(filename, X_OK);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ret = chmod(filename, 0011); // mode: 0011
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ret = access(filename, X_OK);
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);

    ret = rmdir(filename);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ret = setuid(0);
    ret = setgid(0);

    return 0;
}

static int SetReadAndSearch()
{
    int capNum = 2;
    struct __user_cap_header_struct capheader;
    struct __user_cap_data_struct capdata[capNum];
    int ret;

    memset(&capheader, 0, sizeof(struct __user_cap_header_struct));
    memset(capdata, 0, capNum * sizeof(struct __user_cap_data_struct));
    capdata[0].permitted = 0xffffffff;
    capdata[1].permitted = 0xffffffff;
    capheader.version = _LINUX_CAPABILITY_VERSION_3;
    capdata[CAP_TO_INDEX(CAP_SETPCAP)].effective |= CAP_TO_MASK(CAP_SETPCAP);
    capdata[CAP_TO_INDEX(CAP_SETUID)].effective |= CAP_TO_MASK(CAP_SETUID);
    capdata[CAP_TO_INDEX(CAP_SETGID)].effective |= CAP_TO_MASK(CAP_SETGID);
    capdata[CAP_TO_INDEX(CAP_CHOWN)].effective |= CAP_TO_MASK(CAP_CHOWN);
    capdata[CAP_TO_INDEX(CAP_DAC_READ_SEARCH)].effective |= CAP_TO_MASK(CAP_DAC_READ_SEARCH);
    ret = capset(&capheader, &capdata[0]);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    return 0;
}

static int TestCapReadSearch()
{
    int ret;
    char filenameParent[64] = {0};
    char filenameChild[64] = {0};

    ret = setuid(0);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ret = setgid(0);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = mkdir("/storage/test_jffs2", 0757); // mode 0757
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    sprintf(filenameParent, "%s/%s", "/storage/test_jffs2", "testParent");
    sprintf(filenameChild, "%s/%s", filenameParent, "testChild");
    ret = mkdir(filenameParent, 0222); // mode 0222
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    SetReadAndSearch();
    ret = mkdir(filenameChild, 0777); // mode 0777
    ICUNIT_ASSERT_NOT_EQUAL(ret, 0, ret);

    ret = rmdir(filenameParent);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = rmdir("/storage/test_jffs2");
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    return 0;
}

/* execve */
/* access */

static int TestJffsDac(const char *path)
{
    int ret = TestUmask(path);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    umask(0);
    ret = TestOpen(path);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ret = TestUnlink(path);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ret = TestMkdir(path);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ret = TestChmod(path);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ret = TestStatChdir(path);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ret = TestRename(path);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ret = TestAccess(path);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    umask(0022); // umask: 0022
    return 0;
}

static int ChildFunc(VOID)
{
    int capNum = 2;
    struct __user_cap_header_struct capheader;
    struct __user_cap_data_struct capdata[capNum];
    int ret;

    memset(&capheader, 0, sizeof(struct __user_cap_header_struct));
    memset(capdata, 0, capNum * sizeof(struct __user_cap_data_struct));
    capdata[0].permitted = 0xffffffff;
    capdata[1].permitted = 0xffffffff;
    capheader.version = _LINUX_CAPABILITY_VERSION_3;
    capdata[CAP_TO_INDEX(CAP_SETPCAP)].effective |= CAP_TO_MASK(CAP_SETPCAP);
    capdata[CAP_TO_INDEX(CAP_SETUID)].effective |= CAP_TO_MASK(CAP_SETUID);
    capdata[CAP_TO_INDEX(CAP_SETGID)].effective |= CAP_TO_MASK(CAP_SETGID);
    capdata[CAP_TO_INDEX(CAP_CHOWN)].effective |= CAP_TO_MASK(CAP_CHOWN);
    ret = capset(&capheader, &capdata[0]);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ret = setuid(0);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ret = setgid(0);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    /* jffs2 & rootfs test case */
    ret = mkdir("/storage/test_jffs", 0757); // mode 757
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    chmod("/storage/test_jffs", 0757); // mode 757
    ret = TestJffsDac("/storage/test_jffs");
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    rmdir("/storage/test_jffs");

    ret = setuid(2); // uid: 2
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ret = setgid(2); // gid: 2
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    return 0;
}

static int testcase(VOID)
{
    int ret;
    int status = 0;
    pid_t pid = fork();
    ICUNIT_GOTO_WITHIN_EQUAL(pid, 0, 100000, pid, EXIT); // pid must in range 0 - 100000
    if (pid == 0) {
        ret = ChildFunc();
        printf("err line :%d error code: %d\n", 0, 0);
        exit(0);
    }

    ret = waitpid(pid, &status, 0);
    ICUNIT_GOTO_EQUAL(ret, pid, ret, EXIT);

    pid = fork();
    ICUNIT_GOTO_WITHIN_EQUAL(pid, 0, 100000, pid, EXIT); // pid must in range 0 - 100000
    if (pid == 0) {
        ret = TestCapReadSearch();
        printf("err line :%d error code: %d\n", 0, 0);
        exit(0);
    }

    ret = waitpid(pid, &status, 0);
    ICUNIT_GOTO_EQUAL(ret, pid, ret, EXIT);
    status = WEXITSTATUS(status);
    ICUNIT_GOTO_EQUAL(status, 0, status, EXIT);

    return 0;

EXIT:
    return 1;
}


void ItTestDac001(void)
{
    TEST_ADD_CASE("IT_SEC_DAC_001", testcase, TEST_VFS, TEST_JFFS, TEST_LEVEL0, TEST_FUNCTION);
}
