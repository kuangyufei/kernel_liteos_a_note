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

#include "It_vfs_jffs.h"

static UINT32 TestCase(VOID)
{
    INT32 ret, i, j;
    CHAR pathname[50] = { JFFS_PATH_NAME0 };
    CHAR pathname1[300] = { JFFS_PATH_NAME0 };
    CHAR bufname[10] = "";
    CHAR pathname2[10][300] = {0};

    ret = mkdir(pathname, HIGHEST_AUTHORITY);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    for (i = 0; i < JFFS_SHORT_ARRAY_LENGTH; i++) {
        (void)memset_s(bufname, sizeof(bufname), 0, strlen(bufname));
        snprintf_s(bufname, sizeof(bufname), sizeof(bufname) - 1, "/test_%d", i);
        strcat_s(pathname1, sizeof(pathname1), bufname);
        (void)strcpy_s(pathname2[i], sizeof(pathname2[i]), pathname1);

        ret = mkdir(pathname2[i], HIGHEST_AUTHORITY);
        ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT1);
    }

    for (i = 0; i < JFFS_SHORT_ARRAY_LENGTH - 1; i++) {
        ret = rmdir(pathname2[i]);
        ICUNIT_GOTO_NOT_EQUAL(ret, 0, ret, EXIT1);
    }
    for (i = JFFS_SHORT_ARRAY_LENGTH - 1; i >= 0; i--) {
        ret = rmdir(pathname2[i]);
        ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT1);
    }

    ret = rmdir(JFFS_PATH_NAME0);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT);

    return JFFS_NO_ERROR;
EXIT1:
    for (i = JFFS_SHORT_ARRAY_LENGTH - 1; i >= 0; i--) {
        rmdir(pathname2[i]);
    }
EXIT:
    rmdir(JFFS_PATH_NAME0);
    return JFFS_NO_ERROR;
}

VOID ItFsJffsPressure306(VOID)
{
    TEST_ADD_CASE("ItFsJffsPressure306", TestCase, TEST_VFS, TEST_JFFS, TEST_LEVEL3, TEST_PRESSURE);
}
