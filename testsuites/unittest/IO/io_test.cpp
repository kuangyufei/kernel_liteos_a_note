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

#include "It_test_IO.h"

char *g_ioTestPath = "/storage";

using namespace testing::ext;
namespace OHOS {
class IoTest : public testing::Test {
public:
    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
};

#if defined(LOSCFG_USER_TEST_SMOKE)
/* *
 * @tc.name: IT_TEST_IO_005
 * @tc.desc: function for IoTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(IoTest, ItTestIo005, TestSize.Level0)
{
    ItTestIo005();
}

#ifdef LOSCFG_USER_TEST_FS_JFFS
/* *
 * @tc.name: IT_TEST_IO_008
 * @tc.desc: function for IoTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(IoTest, ItTestIo008, TestSize.Level0)
{
    ItTestIo008();
}
#endif


/* *
 * @tc.name: IT_TEST_IO_010
 * @tc.desc: function for IoTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(IoTest, ItTestIo010, TestSize.Level0)
{
    ItTestIo010();
}

/* *
 * @tc.name: IT_TEST_IO_013
 * @tc.desc: function for IoTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(IoTest, ItTestIo013, TestSize.Level0)
{
    ItTestIo013();
}
#endif

#if defined(LOSCFG_USER_TEST_FULL)
/* *
 * @tc.name: IO_TEST_PSELECT_001
 * @tc.desc: function for IoTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(IoTest, IO_TEST_PSELECT_001, TestSize.Level0)
{
    IO_TEST_PSELECT_001();
}

/* *
 * @tc.name: IO_TEST_PSELECT_002
 * @tc.desc: function for IoTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(IoTest, IO_TEST_PSELECT_002, TestSize.Level0)
{
    IO_TEST_PSELECT_002();
}

/* *
 * @tc.name: IO_TEST_PPOLL_001
 * @tc.desc: function for IoTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(IoTest, IO_TEST_PPOLL_001, TestSize.Level0)
{
    IO_TEST_PPOLL_001();
}

/* *
 * @tc.name: IO_TEST_PPOLL_002
 * @tc.desc: function for IoTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(IoTest, IO_TEST_PPOLL_002, TestSize.Level0)
{
    IO_TEST_PPOLL_002();
}

/* *
 * @tc.name: IT_STDLIB_POLL_002
 * @tc.desc: function for IoTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(IoTest, ItStdlibPoll002, TestSize.Level0)
{
    ItStdlibPoll002();
}

/* *
 * @tc.name: IT_STDLIB_POLL_003
 * @tc.desc: function for IoTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(IoTest, ItStdlibPoll003, TestSize.Level0)
{
    ItStdlibPoll003();
}

/* *
 * @tc.name: IT_STDIO_PUTWC_001
 * @tc.desc: function for IoTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(IoTest, ItStdioPutwc001, TestSize.Level0)
{
    ItStdioPutwc001();
}

/* *
 * @tc.name: IT_STDIO_READV_001
 * @tc.desc: function for IoTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(IoTest, ItStdioReadv001, TestSize.Level0)
{
    ItStdioReadv001();
}


/* *
 * @tc.name: IT_STDIO_RINDEX_001
 * @tc.desc: function for IoTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(IoTest, ItStdioRindex001, TestSize.Level0)
{
    ItStdioRindex001();
}

/* *
 * @tc.name: IT_STDIO_SETLOGMASK_001
 * @tc.desc: function for IoTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(IoTest, ItStdioSetlogmask001, TestSize.Level0)
{
    ItStdioSetlogmask001();
}

/* *
 * @tc.name: IT_STDLIB_GCVT_001
 * @tc.desc: function for IoTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(IoTest, ItStdlibGcvt001, TestSize.Level0)
{
    ItStdlibGcvt001();
}

/* *
 * @tc.name: IT_LOCALE_LOCALECONV_001
 * @tc.desc: function for IoTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(IoTest, ItLocaleLocaleconv001, TestSize.Level0)
{
    ItLocaleLocaleconv001();
}

/* *
 * @tc.name: IT_STDIO_FPUTWS_001
 * @tc.desc: function for IoTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(IoTest, ItStdioFputws001, TestSize.Level0)
{
    ItStdioFputws001();
}

/* *
 * @tc.name: IT_STDIO_FWPRINTF_001
 * @tc.desc: function for IoTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(IoTest, ItStdioFwprintf001, TestSize.Level0)
{
    ItStdioFwprintf001();
}

/* *
 * @tc.name: IT_STDIO_GETC_UNLOCKED_001
 * @tc.desc: function for IoTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(IoTest, ItStdioGetcUnlocked001, TestSize.Level0)
{
    ItStdioGetcUnlocked001();
}

/* *
 * @tc.name: IT_STDIO_MBLEN_001
 * @tc.desc: function for IoTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(IoTest, ItStdioMblen001, TestSize.Level0)
{
    ItStdioMblen001();
}

/* *
 * @tc.name: IT_STDIO_MBRLEN_001
 * @tc.desc: function for IoTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(IoTest, ItStdioMbrlen001, TestSize.Level0)
{
    ItStdioMbrlen001();
}
#endif
} // namespace OHOS
