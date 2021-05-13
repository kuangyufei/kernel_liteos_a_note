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

static int TestCase(void)
{
    FILE *fp = NULL;
    char *buf = "hello tmpfile !";
    char rbuf[20] = {0};
    int i = 0;
    int ret = -1;

    fp = tmpfile();
    ICUNIT_GOTO_NOT_EQUAL(fp, NULL, fp, EXIT);

    ret = fwrite(buf, strlen(buf), 1, fp);
    ICUNIT_GOTO_EQUAL(ret, 1, ret, EXIT);

    ret = fseek(fp, 0, SEEK_SET);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    ret = fread(rbuf, strlen(buf), 1, fp);
    ICUNIT_GOTO_EQUAL(ret, 1, ret, EXIT);

    ret = memcmp(buf, rbuf, strlen(buf));
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    fclose(fp);
    return JFFS_NO_ERROR;

EXIT:
    fclose(fp);
    return JFFS_IS_ERROR;
}

void ItTestFsJffs005(void)
{
    TEST_ADD_CASE("It_Test_Fs_Jffs_005", TestCase, TEST_VFS, TEST_JFFS, TEST_LEVEL0, TEST_FUNCTION);
}

