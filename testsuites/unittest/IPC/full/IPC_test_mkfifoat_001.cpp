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
#include "It_test_IPC.h"
#include <sys/types.h>
#include <dirent.h>

#define PATHNAME_FIFO "./fifo.test"
#define PATHNAME_FIFO_FULL "/dev/fifo.test"

static UINT32 testcase1(VOID)
{
    int ret = 0;
    char *dirname = "/dev";
    DIR *dir = NULL;
    int fdDir = 0;

    errno = 0;
    dir = opendir(dirname);
    fdDir = dirfd(dir);
    TEST_PRINT("[INFO]%s:%d,%s,fdDir=%d,errno=%d,errstr=%s\n", __FILE__, __LINE__, __func__, fdDir, errno, strerror(errno));
    ICUNIT_GOTO_NOT_EQUAL(fdDir, -1, fdDir, OUT);

    errno = 0;
    ret = mkfifoat(fdDir, PATHNAME_FIFO, 0777);
    TEST_PRINT("[INFO]%s:%d,%s,ret=%d,errno=%d,errstr=%s\n", __FILE__, __LINE__, __func__, ret, errno, strerror(errno));
    ICUNIT_GOTO_EQUAL(ret, 0, ret, OUT);
    ICUNIT_GOTO_EQUAL(errno, 0, errno, OUT);

    /* remove the pipe file created in order to ensure to run the testcase again. */
    errno = 0;
    ret = remove(PATHNAME_FIFO_FULL);
    TEST_PRINT("[INFO]%s:%d,%s,ret=%d,errno=%d,errstr=%s\n", __FILE__, __LINE__, __func__, ret, errno, strerror(errno));
    ICUNIT_GOTO_EQUAL(ret, 0, ret, OUT);
    ICUNIT_GOTO_EQUAL(errno, 0, errno, OUT);

    close(fdDir);
    return LOS_OK;
OUT:
    close(fdDir);
    return LOS_NOK;
}

static UINT32 testcase(VOID)
{
    testcase1();

    return LOS_OK;
}

VOID IPC_TEST_MKFIFOAT_001(VOID)
{
    TEST_ADD_CASE(__FUNCTION__, testcase, TEST_LIB, TEST_LIBC, TEST_LEVEL1, TEST_FUNCTION);
}
