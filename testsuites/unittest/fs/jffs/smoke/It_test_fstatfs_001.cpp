/*
 * Copyright (c) 2019-2021, Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020-2021, Huawei Device Co., Ltd. All rights reserved.
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
#include <stdlib.h>
#include "fcntl.h"
#include "sys/vfs.h"

static UINT32 testcase1(VOID)
{
    int fd;
    struct statfs buf;
    int ret;
    errno = 0;

    fd = open("/lib/libc.so", O_RDONLY);
    ret = fstatfs(fd, &buf);
    ICUNIT_GOTO_EQUAL(ret, 0, ret, OUT);
    TEST_PRINT("[INFO]The \"/lib/libc.so\" 's,buf->f_type=0x%x\n", buf.f_type);
    TEST_PRINT("[INFO]Check the file's filesystem type:./musl/kernel/include/sys/statfs.h:#define JFFS2_SUPER_MAGIC 0x72b6\n");
    TEST_PRINT("[INFO]%s:%d,%s,ret=%d,errno=%d,errstr=%s\n", __FILE__, __LINE__, __func__, ret, errno, strerror(errno));
    ICUNIT_ASSERT_EQUAL(buf.f_type, 0x72b6, -1);
    ICUNIT_GOTO_EQUAL(buf.f_type, 0x72b6, -1, OUT);

    return LOS_OK;
OUT:
    return LOS_NOK;
}

static UINT32 testcase(VOID)
{
    testcase1();

    return LOS_OK;
}

VOID IO_TEST_FSTATFS_001(void)
{
    TEST_ADD_CASE(__FUNCTION__, testcase, TEST_LIB, TEST_LIBC, TEST_LEVEL1, TEST_FUNCTION);
}
