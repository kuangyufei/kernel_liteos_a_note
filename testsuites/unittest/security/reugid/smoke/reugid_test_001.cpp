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
#include "pthread.h"
#include "linux/capability.h"
#include "it_test_reugid.h"
#include <signal.h>
#include <sys/types.h>
#include <time.h>

static int Child1(int *list, int listSize)
{
    int getList[500];
    int ruid = 0;
    int euid = 0;
    int suid = 100;
    int rgid = 0;
    int egid = 0;
    int sgid = 100;
    int ret;

    rgid = getgid();
    ICUNIT_ASSERT_EQUAL(rgid, 300, rgid);
    egid = getegid();
    ICUNIT_ASSERT_EQUAL(egid, 300, egid);

    ret = getresgid((gid_t *)&ruid, (gid_t *)&euid, (gid_t *)&suid);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ICUNIT_ASSERT_EQUAL(ruid, 300, ruid);
    ICUNIT_ASSERT_EQUAL(euid, 300, euid);
    ICUNIT_ASSERT_EQUAL(suid, 300, suid);

    ruid = getuid();
    ICUNIT_ASSERT_EQUAL(ruid, 300, ruid);
    euid = geteuid();
    ICUNIT_ASSERT_EQUAL(euid, 300, euid);

    ret = getresuid((gid_t *)&ruid, (gid_t *)&euid, (gid_t *)&suid);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ICUNIT_ASSERT_EQUAL(ruid, 300, ruid);
    ICUNIT_ASSERT_EQUAL(euid, 300, euid);
    ICUNIT_ASSERT_EQUAL(suid, 300, suid);

    int size = getgroups(0, (gid_t *)getList);
    ICUNIT_ASSERT_EQUAL(size, listSize, size);

    size = getgroups(size, (gid_t *)getList);
    ICUNIT_ASSERT_EQUAL(size, listSize, size);
    for (int i = 0; i < size - 1; i++) {
        ICUNIT_ASSERT_EQUAL(getList[i], list[i], getList[i]);
    }

    getList[size - 1] = 500;
    ret = setgroups(0, (gid_t *)getList);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = getgroups(0, NULL);
    ICUNIT_ASSERT_EQUAL(ret, 1, ret);

    ret = getgroups(ret, (gid_t *)getList);
    ICUNIT_ASSERT_EQUAL(ret, 1, ret);
    ICUNIT_ASSERT_EQUAL(getList[0], getgid(), getList[0]);

    exit(255);
}

static int Child(void)
{
    int size;
    int ret;
    int list[500];
    int getList[500];
    int ruid = 0;
    int euid = 0;
    int suid = 100;
    int rgid = 0;
    int egid = 0;
    int sgid = 100;

    ret = setgid(3000);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ret = getgid();
    ICUNIT_ASSERT_EQUAL(ret, 3000, ret);
    ret = getegid();
    ICUNIT_ASSERT_EQUAL(ret, 3000, ret);

    ret = setegid(3000);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ret = getgid();
    ICUNIT_ASSERT_EQUAL(ret, 3000, ret);
    ret = getegid();
    ICUNIT_ASSERT_EQUAL(ret, 3000, ret);

    ret = setgid(-1);
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);
    ICUNIT_ASSERT_EQUAL(EINVAL, errno, errno);
    ret = setegid(-2);
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);
    ICUNIT_ASSERT_EQUAL(EINVAL, errno, errno);

    list[0] = 1000;
    list[1] = 2000;
    list[2] = 3000;
    list[3] = 4000;
    list[4] = 5000;
    ret = setgroups(5, (gid_t *)list);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    size = getgroups(0, (gid_t *)getList);
    ICUNIT_ASSERT_EQUAL(size, 5, size);

    size = getgroups(size, (gid_t *)getList);
    ICUNIT_ASSERT_EQUAL(size, 5, size);

    for (int i = 0; i < size; i++) {
        ICUNIT_ASSERT_EQUAL(getList[i], list[i], getList[i]);
    }

    list[0] = 1000;
    list[1] = 2000;
    list[2] = 6000;
    list[3] = 4000;
    list[4] = 5000;
    list[5] = -1;
    ret = setgroups(6, (gid_t *)list);
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);
    ICUNIT_ASSERT_EQUAL(EINVAL, errno, errno);

    list[0] = 1000;
    list[1] = 2000;
    list[2] = 6000;
    list[3] = 4000;
    list[4] = 5000;
    list[5] = 7000;
    ret = setgroups(6, (gid_t *)list);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    size = getgroups(0, (gid_t *)getList);
    ICUNIT_ASSERT_EQUAL(size, 7, size);

    size = getgroups(0, NULL);
    ICUNIT_ASSERT_EQUAL(size, 7, size);

    size = getgroups(size, (gid_t *)getList);
    ICUNIT_ASSERT_EQUAL(size, 7, size);
    for (int i = 0; i < size - 1; i++) {
        ICUNIT_ASSERT_EQUAL(getList[i], list[i], getList[i]);
    }

    ICUNIT_ASSERT_EQUAL(getList[size - 1], getgid(), getList[size - 1]);

    ret = seteuid(8000);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = geteuid();
    ICUNIT_ASSERT_EQUAL(ret, 8000, ret);

    ret = setuid(2000);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ret = geteuid();
    ICUNIT_ASSERT_EQUAL(ret, 2000, ret);
    ret = getuid();
    ICUNIT_ASSERT_EQUAL(ret, 2000, ret);

    ret = setuid(-1);
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);
    ICUNIT_ASSERT_EQUAL(EINVAL, errno, errno);
    ret = seteuid(-2);
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);
    ICUNIT_ASSERT_EQUAL(EINVAL, errno, errno);

    ret = setregid(5000, 300);
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);
    ICUNIT_ASSERT_EQUAL(EPERM, errno, errno);

    ret = setregid(5000, 5000);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ret = getgid();
    ICUNIT_ASSERT_EQUAL(ret, 5000, ret);
    egid = getegid();
    ICUNIT_ASSERT_EQUAL(egid, 5000, egid);

    ret = setregid(5000, -1);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ret = getgid();
    ICUNIT_ASSERT_EQUAL(ret, 5000, ret);
    egid = getegid();
    ICUNIT_ASSERT_EQUAL(egid, 5000, egid);

    ret = setregid(3000, -2);
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);
    ICUNIT_ASSERT_EQUAL(EINVAL, errno, errno);

    ret = setregid(-5, -2);
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);
    ICUNIT_ASSERT_EQUAL(EINVAL, errno, errno);

    ret = setregid(3000, -1);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ret = getgid();
    ICUNIT_ASSERT_EQUAL(ret, 3000, ret);
    egid = getegid();
    ICUNIT_ASSERT_EQUAL(egid, 3000, egid);

    ret = setreuid(1000, 3000);
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);
    ICUNIT_ASSERT_EQUAL(EPERM, errno, errno);

    ret = setreuid(1000, -2);
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);
    ICUNIT_ASSERT_EQUAL(EINVAL, errno, errno);

    ret = setreuid(-2, -2);
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);
    ICUNIT_ASSERT_EQUAL(EINVAL, errno, errno);

    ret = setreuid(-1, 3000);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ruid = getuid();
    ICUNIT_ASSERT_EQUAL(ruid, 3000, ruid);
    euid = geteuid();
    ICUNIT_ASSERT_EQUAL(euid, 3000, euid);

    ret = setreuid(1000, -1);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ruid = getuid();
    ICUNIT_ASSERT_EQUAL(ruid, 1000, ruid);
    euid = geteuid();
    ICUNIT_ASSERT_EQUAL(euid, 1000, euid);

    ret = setresuid(100, 100, 100);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ret = getresuid((uid_t *)&ruid, (uid_t *)&euid, (uid_t *)&suid);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ICUNIT_ASSERT_EQUAL(ruid, 100, ruid);
    ICUNIT_ASSERT_EQUAL(euid, 100, euid);
    ICUNIT_ASSERT_EQUAL(suid, 100, suid);

    ret = setresuid(200, 100, 100);
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);
    ICUNIT_ASSERT_EQUAL(EPERM, errno, errno);

    ret = setresuid(100, 100, 200);
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);
    ICUNIT_ASSERT_EQUAL(EPERM, errno, errno);

    ret = setresuid(100, 200, 200);
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);
    ICUNIT_ASSERT_EQUAL(EPERM, errno, errno);

    ret = setresuid(-1, 200, 200);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ret = getresuid((uid_t *)&ruid, (uid_t *)&euid, (uid_t *)&suid);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ICUNIT_ASSERT_EQUAL(ruid, 200, ruid);
    ICUNIT_ASSERT_EQUAL(euid, 200, euid);
    ICUNIT_ASSERT_EQUAL(suid, 200, suid);

    ret = setresuid(-1, -1, 300);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ret = getresuid((uid_t *)&ruid, (uid_t *)&euid, (uid_t *)&suid);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ICUNIT_ASSERT_EQUAL(ruid, 300, ruid);
    ICUNIT_ASSERT_EQUAL(euid, 300, euid);
    ICUNIT_ASSERT_EQUAL(suid, 300, suid);

    ret = setresuid(-1, 200, 300);
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);
    ret = setresuid(-1, -2, 200);
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);
    ICUNIT_ASSERT_EQUAL(EINVAL, errno, errno);

    ret = setresuid(-2, 200, 200);
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);
    ICUNIT_ASSERT_EQUAL(EINVAL, errno, errno);

    ret = setresuid(200, -2, 200);
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);
    ICUNIT_ASSERT_EQUAL(EINVAL, errno, errno);

    ret = setresuid(200, 200, -3);
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);
    ICUNIT_ASSERT_EQUAL(EINVAL, errno, errno);

    ret = setresgid(100, 100, 100);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ret = getresgid((gid_t *)&rgid, (gid_t *)&egid, (gid_t *)&sgid);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ICUNIT_ASSERT_EQUAL(rgid, 100, rgid);
    ICUNIT_ASSERT_EQUAL(egid, 100, egid);
    ICUNIT_ASSERT_EQUAL(sgid, 100, sgid);

    ret = setresgid(200, 100, 100);
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);
    ICUNIT_ASSERT_EQUAL(EPERM, errno, errno);

    ret = setresgid(100, 100, 200);
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);
    ICUNIT_ASSERT_EQUAL(EPERM, errno, errno);

    ret = setresgid(100, 200, 200);
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);
    ICUNIT_ASSERT_EQUAL(EPERM, errno, errno);

    ret = setresgid(-2, 100, 200);
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);
    ICUNIT_ASSERT_EQUAL(EINVAL, errno, errno);
    ret = setresgid(100, -2, 200);
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);
    ICUNIT_ASSERT_EQUAL(EINVAL, errno, errno);
    ret = setresgid(100, 100, -2);
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);
    ICUNIT_ASSERT_EQUAL(EINVAL, errno, errno);
    ret = setresgid(100, -1, -2);
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);
    ICUNIT_ASSERT_EQUAL(EINVAL, errno, errno);
    ret = setresgid(-1, 200, 200);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ret = getresgid((gid_t *)&rgid, (gid_t *)&egid, (gid_t *)&sgid);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ICUNIT_ASSERT_EQUAL(rgid, 200, rgid);
    ICUNIT_ASSERT_EQUAL(egid, 200, egid);
    ICUNIT_ASSERT_EQUAL(sgid, 200, sgid);

    ret = setresgid(-1, -1, 300);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ret = getresgid((gid_t *)&rgid, (gid_t *)&egid, (gid_t *)&sgid);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ICUNIT_ASSERT_EQUAL(rgid, 300, rgid);
    ICUNIT_ASSERT_EQUAL(egid, 300, egid);
    ICUNIT_ASSERT_EQUAL(sgid, 300, sgid);

    ret = setresgid(-1, 200, 300);
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);
    ICUNIT_ASSERT_EQUAL(EPERM, errno, errno);

    size = getgroups(0, (gid_t *)getList);
    size = getgroups(size, (gid_t *)getList);
    pid_t pid = fork();
    if (pid == 0) {
        Child1(getList, size);
        exit(__LINE__);
    }

    int status = 0;
    ret = waitpid(pid, &status, 0);
    ICUNIT_GOTO_EQUAL(ret, pid, ret, EXIT);
    status = WEXITSTATUS(status);
    ICUNIT_GOTO_EQUAL(status, 255, status, EXIT);

    ret = setgroups(0, NULL);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = getgroups(0, (gid_t *)getList);
    ICUNIT_ASSERT_EQUAL(ret, 1, ret);

    ret = getgroups(1, (gid_t *)getList);
    ICUNIT_ASSERT_EQUAL(ret, 1, ret);
    ICUNIT_ASSERT_EQUAL(getList[0], getgid(), getList[0]);

    ret = setreuid(-1, -1);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ret = setregid(-1, -1);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ret = setresuid(-1, -1, -1);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ret = setresuid(-1, -1, -1);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ret = setegid(-1);
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);
    ICUNIT_ASSERT_EQUAL(EINVAL, errno, errno);
    ret = seteuid(-1);
    ICUNIT_ASSERT_EQUAL(ret, -1, ret);
    ICUNIT_ASSERT_EQUAL(EINVAL, errno, errno);

    exit(255);

EXIT:
    return 1;
}

static int TestCase(VOID)
{
    int ret;
    int status = 0;
    pid_t pid = fork();
    ICUNIT_GOTO_WITHIN_EQUAL(pid, 0, 100000, pid, EXIT);
    if (pid == 0) {
        ret = Child();
        exit(__LINE__);
    }

    ret = waitpid(pid, &status, 0);
    ICUNIT_GOTO_EQUAL(ret, pid, ret, EXIT);
    status = WEXITSTATUS(status);
    ICUNIT_GOTO_EQUAL(status, 255, status, EXIT);

    return 0;

EXIT:
    return 1;
}

void ItTestReugid001(void)
{
    TEST_ADD_CASE("IT_SEC_UGID_001", TestCase, TEST_POSIX, TEST_SEC, TEST_LEVEL0, TEST_FUNCTION);
}
