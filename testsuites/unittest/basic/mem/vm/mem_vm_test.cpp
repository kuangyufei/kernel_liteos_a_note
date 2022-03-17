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
#include <climits>
#include <gtest/gtest.h>

#include "it_test_vm.h"

using namespace testing::ext;
namespace OHOS {
class MemVmTest : public testing::Test {
public:
    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
};

#if defined(LOSCFG_USER_TEST_SMOKE)
/* *
 * @tc.name: it_test_mmap_001
 * @tc.desc: function for MemVmTest
 * @tc.type: FUNC
 */
HWTEST_F(MemVmTest, ItTestMmap001, TestSize.Level0)
{
    ItTestMmap001();
}
#if 0 // need tmpfs
/* *
 * @tc.name: it_test_mmap_002
 * @tc.desc: function for MemVmTest
 * @tc.type: FUNC
 */
HWTEST_F(MemVmTest, ItTestMmap002, TestSize.Level0)
{
    ItTestMmap002();
}

/* *
 * @tc.name: it_test_mmap_003
 * @tc.desc: function for MemVmTest
 * @tc.type: FUNC
 */
HWTEST_F(MemVmTest, ItTestMmap003, TestSize.Level0)
{
    ItTestMmap003();
}

/* *
 * @tc.name: it_test_mmap_004
 * @tc.desc: function for MemVmTest
 * @tc.type: FUNC
 */
HWTEST_F(MemVmTest, ItTestMmap004, TestSize.Level0)
{
    ItTestMmap004();
}
#endif

/* *
 * @tc.name: it_test_mmap_005
 * @tc.desc: function for MemVmTest
 * @tc.type: FUNC
 */
HWTEST_F(MemVmTest, ItTestMmap005, TestSize.Level0)
{
    ItTestMmap005();
}

/* *
 * @tc.name: it_test_mmap_006
 * @tc.desc: function for MemVmTest
 * @tc.type: FUNC
 */
HWTEST_F(MemVmTest, ItTestMmap006, TestSize.Level0)
{
    ItTestMmap006();
}

/* *
 * @tc.name: it_test_mmap_007
 * @tc.desc: function for MemVmTest
 * @tc.type: FUNC
 */
HWTEST_F(MemVmTest, ItTestMmap007, TestSize.Level0)
{
    ItTestMmap007();
}

/* *
 * @tc.name: it_test_mmap_008
 * @tc.desc: function for MemVmTest
 * @tc.type: FUNC
 */
HWTEST_F(MemVmTest, ItTestMmap008, TestSize.Level0)
{
    ItTestMmap008();
}

/* *
 * @tc.name: it_test_mmap_009
 * @tc.desc: function for MemVmTest
 * @tc.type: FUNC
 */
HWTEST_F(MemVmTest, ItTestMmap009, TestSize.Level0)
{
    ItTestMmap009();
}

/* *
 * @tc.name: it_test_mmap_010
 * @tc.desc: function for MemVmTest
 * @tc.type: FUNC
 */
HWTEST_F(MemVmTest, ItTestMmap010, TestSize.Level0)
{
    ItTestMmap010();
}

/* *
 * @tc.name: it_test_mprotect_001
 * @tc.desc: function for MemVmTest
 * @tc.type: FUNC
 */
HWTEST_F(MemVmTest, ItTestMprotect001, TestSize.Level0)
{
    ItTestMprotect001();
}

#ifndef LOSCFG_USER_TEST_SMP
/* *
 * @tc.name: it_test_oom_001
 * @tc.desc: function for MemVmTest
 * @tc.type: FUNC
 */
HWTEST_F(MemVmTest, ItTestOom001, TestSize.Level0)
{
    ItTestOom001();
}

#endif
/* *
 * @tc.name: it_test_mremap_001
 * @tc.desc: function for MemVmTest
 * @tc.type: FUNC
 */
HWTEST_F(MemVmTest, ItTestMremap001, TestSize.Level0)
{
    ItTestMremap001();
}

/* *
 * @tc.name: it_test_user_copy_001
 * @tc.desc: function for MemVmTest
 * @tc.type: FUNC
 */
HWTEST_F(MemVmTest, ItTestUserCopy001, TestSize.Level0)
{
    ItTestUserCopy001();
}

/* *
 * @tc.name: open_wmemstream_test_001
 * @tc.desc: function for open_wmemstream
 * @tc.type: FUNC
 */
HWTEST_F(MemVmTest, open_wmemstream_test_001, TestSize.Level0)
{
    open_wmemstream_test_001();
}
#endif
} // namespace OHOS
