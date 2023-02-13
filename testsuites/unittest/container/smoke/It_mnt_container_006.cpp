/*
 * Copyright (c) 2023-2023 Huawei Device Co., Ltd. All rights reserved.
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
#include "It_container_test.h"
#include "sys/mount.h"

static int ChildFunc(void *arg)
{
    int ret;
    int value = *((int *)arg);
    if (value != CHILD_FUNC_ARG) {
        return EXIT_CODE_ERRNO_1;
    }

    ret = access(ACCESS_FILE_NAME, F_OK);
    if (ret != 0) {
        return EXIT_CODE_ERRNO_2;
    }

    ret = unshare(CLONE_NEWNS);
    if (ret != 0) {
        return EXIT_CODE_ERRNO_3;
    }

    ret = access(ACCESS_FILE_NAME, F_OK);
    if (ret != 0) {
        return EXIT_CODE_ERRNO_4;
    }

    ret = umount(USERDATA_DIR_NAME);
    if (ret != 0) {
        return EXIT_CODE_ERRNO_5;
    }

    ret = access(ACCESS_FILE_NAME, F_OK);
    if (ret == 0) {
        return EXIT_CODE_ERRNO_6;
    }

    return 0;
}

/*
 * mount container unshare test: mount in parent, child clone without NEW_NS,
 * unshare in child with NEW_NS, umount in child.
 */
void ItMntContainer006(void)
{
    int ret;
    int status = 0;
    int childReturn = 0;
    int arg = CHILD_FUNC_ARG;
    char *stack = nullptr;
    char *stackTop = nullptr;

    stack = (char *)mmap(nullptr, STACK_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0);
    ASSERT_NE(stack, MAP_FAILED);

    stackTop = stack + STACK_SIZE;
    ret = access(ACCESS_FILE_NAME, F_OK);
    ASSERT_EQ(ret, 0);

    auto pid = clone(ChildFunc, stackTop, 0, &arg);
    ASSERT_NE(pid, -1);

    ret = waitpid(pid, &status, 0);
    ASSERT_EQ(ret, pid);

    ret = WIFEXITED(status);
    ASSERT_NE(ret, 0);

    ret = WEXITSTATUS(status);
    ASSERT_EQ(ret, 0);

    ret = access(ACCESS_FILE_NAME, F_OK);
    ASSERT_EQ(ret, 0);
}
