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

#define MAP_ARRAY_SIZE 9
#define MAP_FLAGS (MAP_ANONYMOUS | MAP_PRIVATE)

static const struct {
    void *addr;
    int ret;
    unsigned int flags;
} g_gMmapTests[MAP_ARRAY_SIZE] = {
    {(void *)0, 0, MAP_FLAGS},
    {(void *)1, -1, MAP_FLAGS | MAP_FIXED},
    {(void *)(PAGE_SIZE - 1), -1, MAP_FLAGS | MAP_FIXED},
    {(void *)PAGE_SIZE, -1, MAP_FLAGS | MAP_FIXED},
    {(void *)-1, 0, MAP_FLAGS},
    {(void *)(-PAGE_SIZE), -1, MAP_FLAGS | MAP_FIXED},
    {(void *)(-1 - PAGE_SIZE), -1, MAP_FLAGS | MAP_FIXED},
    {(void *)(-1 - PAGE_SIZE - 1), -1, MAP_FLAGS | MAP_FIXED},
    {(void *)(0x1000 * PAGE_SIZE), 0, MAP_FLAGS | MAP_FIXED},
};

static int Testcase(void)
{
    void *p = NULL;
    int i;
    int ret;
    int count = sizeof(g_gMmapTests) / sizeof(g_gMmapTests[0]);
    void *array[MAP_ARRAY_SIZE] = {NULL};

    for (i = 0; i < count; i++) {
        p = mmap((void *)g_gMmapTests[i].addr, PAGE_SIZE, PROT_READ | PROT_WRITE | PROT_EXEC, g_gMmapTests[i].flags, -1,
            0);
        ret = (p == MAP_FAILED) ? -1 : 0;
        ICUNIT_ASSERT_EQUAL(g_gMmapTests[i].ret, ret, p);
        array[i] = p;
    }

    for (i = 0; i < count; i++) {
        if (array[i] == MAP_FAILED) {
            continue;
        }
        ret = munmap(array[i], PAGE_SIZE);
        ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    }

    return 0;
}

void ItTestMmap001(void)
{
    TEST_ADD_CASE("IT_MEM_MMAP_001", Testcase, TEST_LOS, TEST_MEM, TEST_LEVEL0, TEST_FUNCTION);
}
