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

#include "It_fs_jffs.h"

#define FILEPATH "./test.txt"

static int TestCase(void)
{
    FILE *stream = nullptr;
    wchar_t *buf = nullptr;
    size_t len = -1;
    off_t eob = -1;
    int ret = -1;

    stream = open_wmemstream(&buf, &len);
    ICUNIT_ASSERT_NOT_EQUAL(stream, NULL, JFFS_IS_ERROR);

    fprintf(stream, "hello my world");

    ret = fflush(stream);
    ICUNIT_ASSERT_NOT_EQUAL(ret, JFFS_IS_ERROR, ret);
    ICUNIT_ASSERT_EQUAL(buf, "hello my world", -1);
    eob = ftello(stream);
    ICUNIT_ASSERT_NOT_EQUAL(eob, JFFS_IS_ERROR, eob);

    ret = fseeko(stream, 0, SEEK_SET);
    ICUNIT_ASSERT_NOT_EQUAL(ret, JFFS_IS_ERROR, ret);

    fprintf(stream, "good-bye");
    ret = fseeko(stream, eob, SEEK_SET);
    ICUNIT_ASSERT_NOT_EQUAL(ret, JFFS_IS_ERROR, ret);

    ret = fclose(stream);
    ICUNIT_ASSERT_NOT_EQUAL(ret, JFFS_IS_ERROR, ret);

    ICUNIT_ASSERT_EQUAL(buf, "good-bye", -1);
    free(buf);
}

void ItTestFsJffs202(void)
{
    TEST_ADD_CASE("It_Test_Fs_Jffs_202", TestCase, TEST_VFS, TEST_JFFS, TEST_LEVEL0, TEST_FUNCTION);
}
