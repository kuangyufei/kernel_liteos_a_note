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
#include "It_test_misc.h"

static void *g_root = NULL;

static void *Xmalloc(unsigned n)
{
    void *p = NULL;
    if (n <= 0) {
        printf("err: malloc size invalid\n");
        exit(EXIT_FAILURE);
    }
    p = malloc(n);
    if (p) {
        return p;
    }
    fprintf(stderr, "insufficient memory\n");
    exit(EXIT_FAILURE);
}

static int Compare(const void *pa, const void *pb)
{
    if (*(int *)pa < *(int *)pb) {
        return -1;
    }
    if (*(int *)pa > *(int *)pb) {
        return 1;
    }
    return 0;
}

static void Action(const void *nodep, VISIT which, int depth)
{
    int *datap = NULL;

    switch (which) {
        case preorder:
            break;
        case postorder:
            datap = *(int **)nodep;
            break;
        case endorder:
            break;
        case leaf:
            datap = *(int **)nodep;
            break;
    }
}

static UINT32 TestCase(VOID)
{
    int i;
    int *ptr = NULL;
    void *val = NULL;

    srand(time(NULL));
    for (i = 0; i < 12; i++) {
        ptr = (int *)Xmalloc(sizeof(int));
        *ptr = rand() & 0xff;
        val = tsearch((void *)ptr, &g_root, Compare);
        if (val == NULL) {
            exit(EXIT_FAILURE);
        } else if ((*(int **)val) != ptr) {
            free(ptr);
        }
    }

    twalk(g_root, Action);
    tdestroy(g_root, free);

    return 0;
}

VOID ItTestUtil007(VOID)
{
    TEST_ADD_CASE(__FUNCTION__, TestCase, TEST_POSIX, TEST_MEM, TEST_LEVEL0, TEST_FUNCTION);
}
