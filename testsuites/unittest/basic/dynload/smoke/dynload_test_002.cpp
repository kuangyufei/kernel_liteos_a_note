/*
 * Copyright (c) 2013-2019 Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020-2023 Huawei Device Co., Ltd. All rights reserved.
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
#include "it_test_dynload.h"
#include <stdio.h>

#define LIBCSO_REAL_PATH "/lib/libc.so"
#define LIBCSO_RELATIVE_PATH "../../lib/libc.so"
#define SYMBOL_TO_FIND "printf"
#define SYMBOL_TO_MATCH (void *)printf

static int Testcase(void)
{
    int ret;
    char *workingPath = "/usr/bin";
    void *handle = nullptr;
    int (*func)(int);
    char curPath[1024] = { 0 }; /* 1024, buffer size */

    handle = getcwd(curPath, sizeof(curPath));
    ICUNIT_ASSERT_NOT_EQUAL(handle, NULL, handle);

    ret = chdir(workingPath);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    handle = dlopen(NULL, RTLD_NOW | RTLD_LOCAL);
    ICUNIT_GOTO_NOT_EQUAL(handle, NULL, 1, EXIT1);  /* 1, return value */

    ret = dlclose(handle);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    handle = dlopen(LIBCSO_REAL_PATH, RTLD_NOW);
    ICUNIT_GOTO_NOT_EQUAL(handle, NULL, 1, EXIT1);  /* 1, return value */

    func = reinterpret_cast<int (*)(int)>(dlsym(handle, SYMBOL_TO_FIND));
    ICUNIT_GOTO_NOT_EQUAL(func, NULL, func, EXIT);
    ICUNIT_GOTO_EQUAL(func, SYMBOL_TO_MATCH, func, EXIT);

    ret = dlclose(handle);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    handle = dlopen(LIBCSO_RELATIVE_PATH, RTLD_NOW);
    ICUNIT_GOTO_NOT_EQUAL(handle, NULL, 1, EXIT1);  /* 1, return value */

    func = reinterpret_cast<int (*)(int)>(dlsym(handle, SYMBOL_TO_FIND));
    ICUNIT_GOTO_NOT_EQUAL(func, NULL, func, EXIT);

    ret = dlclose(handle);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ret = chdir(curPath);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT1);

    return 0;

EXIT:
    dlclose(handle);

EXIT1:
    ret = chdir(curPath);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    return 1;   /* 1, return value */
}

void ItTestDynload002(void)
{
    TEST_ADD_CASE("IT_TEST_DYNLOAD_002", Testcase, TEST_EXTEND, TEST_DYNLOAD, TEST_LEVEL2, TEST_FUNCTION);
}
