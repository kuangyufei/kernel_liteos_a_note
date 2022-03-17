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
#include "stdio.h"
#include <climits>
#include <gtest/gtest.h>
#include "it_test_process.h"

void Wait(const char *ptr, int scount)
{
    int count = 0xffffffff;
    while (scount > 0) {
        while (count > 0) {
            printf("\r");
            count--;
        }
        count = 0xffffffff;
        scount--;
        if (ptr) {
            printf("%s\n", ptr);
        }
    }
}

int GetCpuCount(void)
{
    cpu_set_t cpuset;

    CPU_ZERO(&cpuset);
    int temp = sched_getaffinity(getpid(), sizeof(cpu_set_t), &cpuset);
    if (temp != 0) {
        printf("%s %d Error : %d\n", __FUNCTION__, __LINE__, temp);
    }

    return CPU_COUNT(&cpuset);
}

using namespace testing::ext;
namespace OHOS {
class ProcessProcessTest : public testing::Test {
public:
    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
};

#if defined(LOSCFG_USER_TEST_SMOKE)
/* *
 * @tc.name: it_test_process_001
 * @tc.desc: function for ProcessProcessTest
 * @tc.type: FUNC
 */
HWTEST_F(ProcessProcessTest, ItTestProcess001, TestSize.Level0)
{
    ItTestProcess001();
}

/* *
 * @tc.name: it_test_process_002
 * @tc.desc: function for ProcessProcessTest
 * @tc.type: FUNC
 */
HWTEST_F(ProcessProcessTest, ItTestProcess002, TestSize.Level0)
{
    ItTestProcess002();
}

/* *
 * @tc.name: it_test_process_004
 * @tc.desc: function for ProcessProcessTest
 * @tc.type: FUNC
 */
HWTEST_F(ProcessProcessTest, ItTestProcess004, TestSize.Level0)
{
    ItTestProcess004();
}

/* *
 * @tc.name: it_test_process_005
 * @tc.desc: function for ProcessProcessTest
 * @tc.type: FUNC
 */
HWTEST_F(ProcessProcessTest, ItTestProcess005, TestSize.Level0)
{
    ItTestProcess005();
}

/* *
 * @tc.name: it_test_process_006
 * @tc.desc: function for ProcessProcessTest
 * @tc.type: FUNC
 */
HWTEST_F(ProcessProcessTest, ItTestProcess006, TestSize.Level0)
{
    ItTestProcess006();
}

/* *
 * @tc.name: it_test_process_008
 * @tc.desc: function for ProcessProcessTest
 * @tc.type: FUNC
 */
HWTEST_F(ProcessProcessTest, ItTestProcess008, TestSize.Level0)
{
    ItTestProcess008();
}

/* *
 * @tc.name: it_test_process_010
 * @tc.desc: function for ProcessProcessTest
 * @tc.type: FUNC
 */
HWTEST_F(ProcessProcessTest, ItTestProcess010, TestSize.Level0)
{
    ItTestProcess010();
}

/* *
 * @tc.name: it_test_process_009
 * @tc.desc: function for ProcessProcessTest
 * @tc.type: FUNC
 */
HWTEST_F(ProcessProcessTest, ItTestProcess009, TestSize.Level0)
{
    ItTestProcess009();
}

/* *
 * @tc.name: it_test_process_011
 * @tc.desc: function for ProcessProcessTest
 * @tc.type: FUNC
 */
HWTEST_F(ProcessProcessTest, ItTestProcess011, TestSize.Level0)
{
    ItTestProcess011();
}

/* *
 * @tc.name: it_test_process_012
 * @tc.desc: function for ProcessProcessTest
 * @tc.type: FUNC
 */
HWTEST_F(ProcessProcessTest, ItTestProcess012, TestSize.Level0)
{
    ItTestProcess012();
}

/* *
 * @tc.name: it_test_process_013
 * @tc.desc: function for ProcessProcessTest
 * @tc.type: FUNC
 */
HWTEST_F(ProcessProcessTest, ItTestProcess013, TestSize.Level0)
{
    ItTestProcess013();
}

/* *
 * @tc.name: it_test_process_014
 * @tc.desc: function for ProcessProcessTest
 * @tc.type: FUNC
 */
HWTEST_F(ProcessProcessTest, ItTestProcess014, TestSize.Level0)
{
    ItTestProcess014();
}

/* *
 * @tc.name: it_test_process_015
 * @tc.desc: function for ProcessProcessTest
 * @tc.type: FUNC
 */
HWTEST_F(ProcessProcessTest, ItTestProcess015, TestSize.Level0)
{
    ItTestProcess015();
}

/* *
 * @tc.name: it_test_process_016
 * @tc.desc: function for ProcessProcessTest
 * @tc.type: FUNC
 */
HWTEST_F(ProcessProcessTest, ItTestProcess016, TestSize.Level0)
{
    ItTestProcess016();
}

/* *
 * @tc.name: it_test_process_017
 * @tc.desc: function for ProcessProcessTest
 * @tc.type: FUNC
 */
HWTEST_F(ProcessProcessTest, ItTestProcess017, TestSize.Level0)
{
    ItTestProcess017();
}

/* *
 * @tc.name: it_test_process_018
 * @tc.desc: function for ProcessProcessTest
 * @tc.type: FUNC
 */
HWTEST_F(ProcessProcessTest, ItTestProcess018, TestSize.Level0)
{
    ItTestProcess018();
}

/* *
 * @tc.name: it_test_process_019
 * @tc.desc: function for ProcessProcessTest
 * @tc.type: FUNC
 */
HWTEST_F(ProcessProcessTest, ItTestProcess019, TestSize.Level0)
{
    ItTestProcess019();
}

/* *
 * @tc.name: it_test_process_020
 * @tc.desc: function for ProcessProcessTest
 * @tc.type: FUNC
 */
HWTEST_F(ProcessProcessTest, ItTestProcess020, TestSize.Level0)
{
    ItTestProcess020();
}

/* *
 * @tc.name: it_test_process_021
 * @tc.desc: function for ProcessProcessTest
 * @tc.type: FUNC
 */
HWTEST_F(ProcessProcessTest, ItTestProcess021, TestSize.Level0)
{
    ItTestProcess021();
}

/* *
 * @tc.name: it_test_process_022
 * @tc.desc: function for ProcessProcessTest
 * @tc.type: FUNC
 */
HWTEST_F(ProcessProcessTest, ItTestProcess022, TestSize.Level0)
{
    ItTestProcess022();
}

/* *
 * @tc.name: it_test_process_023
 * @tc.desc: function for ProcessProcessTest
 * @tc.type: FUNC
 */
HWTEST_F(ProcessProcessTest, ItTestProcess023, TestSize.Level0)
{
    ItTestProcess023();
}

/* *
 * @tc.name: it_test_process_024
 * @tc.desc: function for ProcessProcessTest
 * @tc.type: FUNC
 */
HWTEST_F(ProcessProcessTest, ItTestProcess024, TestSize.Level0)
{
    ItTestProcess024();
}

/* *
 * @tc.name: it_test_process_025
 * @tc.desc: function for ProcessProcessTest
 * @tc.type: FUNC
 */
HWTEST_F(ProcessProcessTest, ItTestProcess025, TestSize.Level0)
{
    ItTestProcess025();
}

/* *
 * @tc.name: it_test_process_026
 * @tc.desc: function for ProcessProcessTest
 * @tc.type: FUNC
 */
HWTEST_F(ProcessProcessTest, ItTestProcess026, TestSize.Level0)
{
    ItTestProcess026();
}

/* *
 * @tc.name: it_test_process_027
 * @tc.desc: function for ProcessProcessTest
 * @tc.type: FUNC
 */
HWTEST_F(ProcessProcessTest, ItTestProcess027, TestSize.Level0)
{
    ItTestProcess027();
}

/* *
 * @tc.name: it_test_process_029
 * @tc.desc: function for ProcessProcessTest
 * @tc.type: FUNC
 */
HWTEST_F(ProcessProcessTest, ItTestProcess029, TestSize.Level0)
{
    ItTestProcess029();
}

/* *
 * @tc.name: it_test_process_030
 * @tc.desc: function for ProcessProcessTest
 * @tc.type: FUNC
 */
HWTEST_F(ProcessProcessTest, ItTestProcess030, TestSize.Level0)
{
    ItTestProcess030();
}

/* *
 * @tc.name: it_test_process_038
 * @tc.desc: function for ProcessProcessTest
 * @tc.type: FUNC
 */
HWTEST_F(ProcessProcessTest, ItTestProcess038, TestSize.Level0)
{
    ItTestProcess038();
}

/* *
 * @tc.name: it_test_process_039
 * @tc.desc: function for ProcessProcessTest
 * @tc.type: FUNC
 */
HWTEST_F(ProcessProcessTest, ItTestProcess039, TestSize.Level0)
{
    ItTestProcess039();
}

/* *
 * @tc.name: it_test_process_043
 * @tc.desc: function for ProcessProcessTest
 * @tc.type: FUNC
 */
HWTEST_F(ProcessProcessTest, ItTestProcess043, TestSize.Level0)
{
    ItTestProcess043();
}

/* *
 * @tc.name: it_test_process_044
 * @tc.desc: function for ProcessProcessTest
 * @tc.type: FUNC
 */
HWTEST_F(ProcessProcessTest, ItTestProcess044, TestSize.Level0)
{
    ItTestProcess044();
}

/* *
 * @tc.name: it_test_process_045
 * @tc.desc: function for ProcessProcessTest
 * @tc.type: FUNC
 */
HWTEST_F(ProcessProcessTest, ItTestProcess045, TestSize.Level0)
{
    ItTestProcess045();
}

/* *
 * @tc.name: it_test_process_046
 * @tc.desc: function for ProcessProcessTest
 * @tc.type: FUNC
 */
HWTEST_F(ProcessProcessTest, ItTestProcess046, TestSize.Level0)
{
    ItTestProcess046();
}

/* *
 * @tc.name: it_test_process_047
 * @tc.desc: function for ProcessProcessTest
 * @tc.type: FUNC
 */
HWTEST_F(ProcessProcessTest, ItTestProcess047, TestSize.Level0)
{
    ItTestProcess047();
}

/* *
 * @tc.name: it_test_process_048
 * @tc.desc: function for ProcessProcessTest
 * @tc.type: FUNC
 */
HWTEST_F(ProcessProcessTest, ItTestProcess048, TestSize.Level0)
{
    ItTestProcess048();
}

/* *
 * @tc.name: it_test_process_054
 * @tc.desc: function for waitid: The waitid parameter is incorrect and the error code is verified.
 * @tc.type: FUNC
 */
HWTEST_F(ProcessProcessTest, ItTestProcess054, TestSize.Level0)
{
    ItTestProcess054();
}

/* *
 * @tc.name: it_test_process_061
 * @tc.desc: function for killpg: The killpg parameter is incorrect and the error code is verified.
 * @tc.type: FUNC
 */
HWTEST_F(ProcessProcessTest, ItTestProcess061, TestSize.Level0)
{
    ItTestProcess061();
}

#ifdef LOSCFG_USER_TEST_SMP
/* *
 * @tc.name: it_test_process_smp_001
 * @tc.desc: function for ProcessProcessTest
 * @tc.type: FUNC
 */
HWTEST_F(ProcessProcessTest, ItTestProcessSmp001, TestSize.Level0)
{
    ItTestProcessSmp001();
}

/* *
 * @tc.name: it_test_process_smp_002
 * @tc.desc: function for ProcessProcessTest
 * @tc.type: FUNC
 */
HWTEST_F(ProcessProcessTest, ItTestProcessSmp002, TestSize.Level0)
{
    ItTestProcessSmp002();
}

/* *
 * @tc.name: it_test_process_smp_003
 * @tc.desc: function for ProcessProcessTest
 * @tc.type: FUNC
 */
HWTEST_F(ProcessProcessTest, ItTestProcessSmp003, TestSize.Level0)
{
    ItTestProcessSmp003();
}

/* *
 * @tc.name: it_test_process_smp_004
 * @tc.desc: function for ProcessProcessTest
 * @tc.type: FUNC
 */
HWTEST_F(ProcessProcessTest, ItTestProcessSmp004, TestSize.Level0)
{
    ItTestProcessSmp004();
}

/* *
 * @tc.name: it_test_process_smp_005
 * @tc.desc: function for ProcessProcessTest
 * @tc.type: FUNC
 */
HWTEST_F(ProcessProcessTest, ItTestProcessSmp005, TestSize.Level0)
{
    ItTestProcessSmp005();
}

/* *
 * @tc.name: it_test_process_smp_006
 * @tc.desc: function for ProcessProcessTest
 * @tc.type: FUNC
 */
HWTEST_F(ProcessProcessTest, ItTestProcessSmp006, TestSize.Level0)
{
    ItTestProcessSmp006();
}

/* *
 * @tc.name: it_test_process_smp_007
 * @tc.desc: function for ProcessProcessTest
 * @tc.type: FUNC
 */
HWTEST_F(ProcessProcessTest, ItTestProcessSmp007, TestSize.Level0)
{
    ItTestProcessSmp007();
}

/* *
 * @tc.name: it_test_process_smp_008
 * @tc.desc: function for ProcessProcessTest
 * @tc.type: FUNC
 */
HWTEST_F(ProcessProcessTest, ItTestProcessSmp008, TestSize.Level0)
{
    ItTestProcessSmp008();
}
#endif
#endif

#if defined(LOSCFG_USER_TEST_FULL)
/* *
 * @tc.name: it_test_process_007
 * @tc.desc: function for ProcessProcessTest
 * @tc.type: FUNC
 */
HWTEST_F(ProcessProcessTest, ItTestProcess007, TestSize.Level0)
{
    ItTestProcess007();
}

/* *
 * @tc.name: it_test_process_031
 * @tc.desc: function for ProcessProcessTest
 * @tc.type: FUNC
 */
HWTEST_F(ProcessProcessTest, ItTestProcess031, TestSize.Level0)
{
    ItTestProcess031();
}

/* *
 * @tc.name: it_test_process_032
 * @tc.desc: function for ProcessProcessTest
 * @tc.type: FUNC
 */
HWTEST_F(ProcessProcessTest, ItTestProcess032, TestSize.Level0)
{
    ItTestProcess032();
}

/* *
 * @tc.name: it_test_process_033
 * @tc.desc: function for ProcessProcessTest
 * @tc.type: FUNC
 */
HWTEST_F(ProcessProcessTest, ItTestProcess033, TestSize.Level0)
{
    ItTestProcess033();
}

/* *
 * @tc.name: it_test_process_034
 * @tc.desc: function for ProcessProcessTest
 * @tc.type: FUNC
 */
HWTEST_F(ProcessProcessTest, ItTestProcess034, TestSize.Level0)
{
    ItTestProcess034();
}

/* *
 * @tc.name: it_test_process_035
 * @tc.desc: function for ProcessProcessTest
 * @tc.type: FUNC
 */
HWTEST_F(ProcessProcessTest, ItTestProcess035, TestSize.Level0)
{
    ItTestProcess035();
}

/* *
 * @tc.name: it_test_process_036
 * @tc.desc: function for ProcessProcessTest
 * @tc.type: FUNC
 */
HWTEST_F(ProcessProcessTest, ItTestProcess036, TestSize.Level0)
{
    ItTestProcess036();
}

/* *
 * @tc.name: it_test_process_037
 * @tc.desc: function for ProcessProcessTest
 * @tc.type: FUNC
 */
HWTEST_F(ProcessProcessTest, ItTestProcess037, TestSize.Level0)
{
    ItTestProcess037();
}

/* *
 * @tc.name: it_test_process_040
 * @tc.desc: function for ProcessProcessTest
 * @tc.type: FUNC
 */
HWTEST_F(ProcessProcessTest, ItTestProcess040, TestSize.Level0)
{
    ItTestProcess040();
}

/* *
 * @tc.name: it_test_process_041
 * @tc.desc: function for ProcessProcessTest
 * @tc.type: FUNC
 */
HWTEST_F(ProcessProcessTest, ItTestProcess041, TestSize.Level0)
{
    ItTestProcess041();
}

/* *
 * @tc.name: it_test_process_042
 * @tc.desc: function for ProcessProcessTest
 * @tc.type: FUNC
 */
HWTEST_F(ProcessProcessTest, ItTestProcess042, TestSize.Level0)
{
    ItTestProcess042();
}

/* *
 * @tc.name: it_test_process_053
 * @tc.desc: function for killpg:Sends a signal to the process group,
 * Other processes in the process group can receive the signal.
 * @tc.type: FUNC
 */
HWTEST_F(ProcessProcessTest, ItTestProcess053, TestSize.Level0)
{
    ItTestProcess053();
}

/* *
 * @tc.name: it_test_process_055
 * @tc.desc: function for waitid:To test the function of transferring different parameters of the waitid.
 * @tc.type: FUNC
 */
HWTEST_F(ProcessProcessTest, ItTestProcess055, TestSize.Level0)
{
    ItTestProcess055();
}

/* *
 * @tc.name: it_test_process_062
 * @tc.desc: function for killpg:Fork two processes. The killpg sends a signal to the current process group.
 * The other two processes can receive the signal.
 * @tc.type: FUNC
 */
HWTEST_F(ProcessProcessTest, ItTestProcess062, TestSize.Level0)
{
    ItTestProcess062();
}
#endif
} // namespace OHOS
