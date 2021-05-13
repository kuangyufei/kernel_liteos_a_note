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
#include "It_posix_mem.h"

/* *
 * @test IT_POSIX_MEMALIGN_001
 * -@tspec posix_memalign API test
 * -@ttitle The alignment argument was not a power of two, or was not a multiple of sizeof(void *)
 * -@tprecon dynamic memory function open
 * -@tbrief
 * -#alignment == 0,2,3,7,15,31,63
 * -@texpect
 * -#return EINVAL
 * -#return EINVAL
 * -@tprior 1
 * -@tauto TRUE
 * -@tremark
 */
static UINT32 Testcase(VOID)
{
    int ret;
    size_t size = 0x100;
    void *p = nullptr;

    ret = posix_memalign(&p, 0, size);
    ICUNIT_ASSERT_EQUAL(ret, EINVAL, ret);
    ICUNIT_ASSERT_EQUAL(p, nullptr, p);

    ret = posix_memalign(&p, 2, size); // 2, alignment
    ICUNIT_ASSERT_EQUAL(ret, EINVAL, ret);
    ICUNIT_ASSERT_EQUAL(p, nullptr, p);

    for (UINT32 n = 2; n < 7; n++) { // 2, 7, alignment
        ret = posix_memalign(&p, ((1 << n) - 1), size);
        ICUNIT_ASSERT_EQUAL(ret, EINVAL, ret);
        ICUNIT_ASSERT_EQUAL(p, nullptr, p);
    }

    return 0;
}


VOID ItPosixMem001(void)
{
    TEST_ADD_CASE("IT_POSIX_MEM_001", Testcase, TEST_POSIX, TEST_MEM, TEST_LEVEL0, TEST_FUNCTION);
}
