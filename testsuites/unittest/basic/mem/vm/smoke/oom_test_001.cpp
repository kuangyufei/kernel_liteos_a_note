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
#include "it_test_vm.h"

#define MAX_MEM_SIZE 0x6000000
/* test page fault oom */
static int Testcase(void)
{
    int ret;
    int status;
    int pid;
    unsigned int *ptr = NULL;

    ret = fork();
    if (ret == 0) {
        ptr = (unsigned int *)mmap(0, MAX_MEM_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
        ICUNIT_ASSERT_NOT_EQUAL(ptr, MAP_FAILED, ptr);
        /* expect oom */
        for (int i = 0; i < MAX_MEM_SIZE / PAGE_SIZE; i++) {
            *(ptr + i * PAGE_SIZE / sizeof(unsigned int)) = 0;
        }

        /* if we go here, phy mem is enough, should increase the MAX_MEM_SIZE and rerun */
        ICUNIT_ASSERT_EQUAL(1, 0, 1);
    } else {
        pid = ret;
        ret = waitpid(pid, &status, 0);
        status = WIFSIGNALED(status);
        ICUNIT_ASSERT_EQUAL(ret, pid, ret);
        ICUNIT_ASSERT_EQUAL(status, 1, status);
    }

    return 0;
}

void ItTestOom001(void)
{
    TEST_ADD_CASE("IT_MEM_OOM_001", Testcase, TEST_LOS, TEST_MEM, TEST_LEVEL0, TEST_FUNCTION);
}
