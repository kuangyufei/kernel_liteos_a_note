/*
 * Copyright (c) 2013-2019 Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020-2021 Huawei Device Co., Ltd. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this list of
 *    conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list
 *    of conditions and the following disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors may be used
 *    to endorse or promote products derived from this software without specific prior written
 *    permission.
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

#include "It_vfs_proc.h"

static UINT32 TestCase(VOID)
{
    INT32 fd, ret, len;
    CHAR readbuf[PROC_NAME_LIMITTED_SIZE] = "";

    fd = open(UPTIME_DIR_PATH, O_RDONLY);
    ICUNIT_GOTO_NOT_EQUAL(fd, -1, fd, EXIT1);

    ret = memset_s(readbuf, PROC_NAME_LIMITTED_SIZE, 0, PROC_NAME_LIMITTED_SIZE);
    ICUNIT_GOTO_EQUAL(ret, PROC_NO_ERROR, ret, EXIT1);
    len = read(fd, readbuf, PROC_NAME_LIMITTED_SIZE - 1);
    ICUNIT_GOTO_NOT_EQUAL(len, -1, len, EXIT1);

    ret = close(fd);
    ICUNIT_GOTO_EQUAL(ret, PROC_NO_ERROR, fd, EXIT1);

    return PROC_NO_ERROR;

EXIT1:
    close(fd);

    return PROC_NO_ERROR;
}

VOID ItFsProc002(VOID)
{
    TEST_ADD_CASE("ItFsProc002", TestCase, TEST_VFS, TEST_PROC, TEST_LEVEL2, TEST_FUNCTION);
}
