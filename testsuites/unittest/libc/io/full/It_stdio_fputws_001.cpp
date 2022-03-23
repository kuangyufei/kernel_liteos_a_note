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
#include "It_test_IO.h"


static UINT32 Testcase(VOID)
{
    wchar_t *srcStr = L"hello";
    wchar_t tarStr[20] = {0}; // 20, target buffer size
    int nRet;
    unsigned int nPos;
    int nType;
    char pathname[50]; // 50, path name buffer size
    FILE *testFile;
    (void)strncpy_s(pathname, 50, g_ioTestPath, 50); // 50, path name buffer size
    char *filename = "/crtfputwstest1";
    strcat(pathname, filename);
    for (nType = 0; nType < 6; nType++) { // 6, test loop num
        testFile = fopen(pathname, "a");

        ICUNIT_GOTO_NOT_EQUAL(testFile, NULL, testFile, EXIT);
        fseek(testFile, 0, SEEK_END);
        nPos = (unsigned int)ftell(testFile);

        nRet = fputws(srcStr, testFile);

        ICUNIT_GOTO_NOT_EQUAL(nRet, -1, nRet, EXIT);
        if ((nPos + 5) != (unsigned int)ftell(testFile)) { // 5, expect position offset
            ICUNIT_GOTO_EQUAL(1, 0, 1, EXIT);
        }
        fclose(testFile);

        testFile = fopen(pathname, "r");
        ICUNIT_GOTO_NOT_EQUAL(testFile, NULL, testFile, EXIT);
        wchar_t *p = fgetws(tarStr, 6, testFile); // 6, "hello" length and '\0'
        ICUNIT_GOTO_EQUAL(p, tarStr, p, EXIT);
        nRet = wcscmp(srcStr, tarStr);
        ICUNIT_GOTO_EQUAL(nRet, 0, nRet, EXIT);
        fclose(testFile);
    }

    remove(pathname);
    return LOS_OK;
EXIT:
    if (testFile != NULL) {
        fclose(testFile);
        remove(pathname);
    }
    return LOS_NOK;
}

VOID ItStdioFputws001(void)
{
    TEST_ADD_CASE(__FUNCTION__, Testcase, TEST_LIB, TEST_LIBC, TEST_LEVEL1, TEST_FUNCTION);
}
