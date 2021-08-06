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


#include "It_posix_queue.h"
CHAR *g_mqueueMsessage[MQUEUE_SHORT_ARRAY_LENGTH] = {"0123456789", "1234567890", "2345678901", "3456789012", "4567890123", "5678901234", "6789012345", "7890123456", "lalalalala", "hahahahaha"};
CHAR g_gqname[MQUEUE_STANDARD_NAME_LENGTH];
CHAR g_mqueueName[LOSCFG_BASE_IPC_QUEUE_CONFIG + 1][MQUEUE_STANDARD_NAME_LENGTH];
mqd_t g_mqueueId[LOSCFG_BASE_IPC_QUEUE_CONFIG + 1];
SEM_HANDLE_T g_mqueueSem;
mqd_t g_messageQId;
mqd_t g_gqueue;

int LOS_HwiCreate(int hwiNum, int hwiPrio, int hwiMode, HWI_PROC_FUNC hwiHandler, int *irqParam)
{
    return 0;
}

int LosSemCreate(int num, const SEM_HANDLE_T *hdl)
{
    return 0;
}

int LosSemDelete(SEM_HANDLE_T num)
{
    return 0;
}

int SemPost(SEM_HANDLE_T num)
{
    return 0;
}

int LosSemPost(SEM_HANDLE_T num)
{
    return 0;
}

int HalIrqMask(int num)
{
    return 0;
}

int SemPend(SEM_HANDLE_T hdl, int num)
{
    return 0;
}

int LosSemPend(SEM_HANDLE_T hdl, int num)
{
    return 0;
}

UINT32 QueueCountGetTest(VOID)
{
    return 0;
}

void TEST_HwiTrigger(unsigned int irq)
{
    return;
}

UINT64 JiffiesToTick(unsigned long j)
{
    return 0;
}

unsigned long MsecsToJiffies(const unsigned int m)
{
    return 0;
}

using namespace testing::ext;
namespace OHOS {
class PosixMqueueTest : public testing::Test {
public:
    static void SetUpTestCase(void) {}
    static void TearDownTestCase(void) {}
};

#if defined(LOSCFG_USER_TEST_SMOKE)
/* *
 * @tc.name: IT_POSIX_QUEUE_001
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue001, TestSize.Level0)
{
    ItPosixQueue001();
}

/* *
 * @tc.name: IT_POSIX_QUEUE_003
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue003, TestSize.Level0)
{
    ItPosixQueue003();
}

/* *
 * @tc.name: IT_POSIX_QUEUE_053
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue053, TestSize.Level0)
{
    ItPosixQueue053();
}

/* *
 * @tc.name: IT_POSIX_QUEUE_028
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue028, TestSize.Level0)
{
    ItPosixQueue028();
}

/* *
 * @tc.name: IT_POSIX_QUEUE_062
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue062, TestSize.Level0)
{
    ItPosixQueue062();
}

#endif

#if defined(LOSCFG_USER_TEST_FULL)
/**
 * @tc.name: IT_POSIX_QUEUE_002
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue002, TestSize.Level0)
{
    ItPosixQueue002();
}

/**
 * @tc.name: IT_POSIX_QUEUE_005
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue005, TestSize.Level0)
{
    ItPosixQueue005();
}

/**
 * @tc.name: IT_POSIX_QUEUE_008
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue008, TestSize.Level0)
{
    ItPosixQueue008();
}

/**
 * @tc.name: IT_POSIX_QUEUE_011
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue011, TestSize.Level0)
{
    ItPosixQueue011();
}

/**
 * @tc.name: IT_POSIX_QUEUE_013
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue013, TestSize.Level0)
{
    ItPosixQueue013();
}

/**
 * @tc.name: IT_POSIX_QUEUE_014
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue014, TestSize.Level0)
{
    ItPosixQueue014();
}

/**
 * @tc.name: IT_POSIX_QUEUE_015
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue015, TestSize.Level0)
{
    ItPosixQueue015();
}

/**
 * @tc.name: IT_POSIX_QUEUE_016
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue016, TestSize.Level0)
{
    ItPosixQueue016();
}

#ifndef LOSCFG_KERNEL_SMP
/**
 * @tc.name: IT_POSIX_QUEUE_113
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue113, TestSize.Level0)
{
    ItPosixQueue113();
}
#endif
/**
 * @tc.name: IT_POSIX_QUEUE_018
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue018, TestSize.Level0)
{
    ItPosixQueue018();
}

/**
 * @tc.name: IT_POSIX_QUEUE_019
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue019, TestSize.Level0)
{
    ItPosixQueue019();
}

/**
 * @tc.name: IT_POSIX_QUEUE_020
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue020, TestSize.Level0)
{
    ItPosixQueue020();
}

/**
 * @tc.name: IT_POSIX_QUEUE_021
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue021, TestSize.Level0)
{
    ItPosixQueue021();
}

/**
 * @tc.name: IT_POSIX_QUEUE_025
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue025, TestSize.Level0)
{
    ItPosixQueue025();
}

/**
 * @tc.name: IT_POSIX_QUEUE_026
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue026, TestSize.Level0)
{
    ItPosixQueue026();
}

/**
 * @tc.name: IT_POSIX_QUEUE_027
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue027, TestSize.Level0)
{
    ItPosixQueue027();
}

/**
 * @tc.name: IT_POSIX_QUEUE_030
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue030, TestSize.Level0)
{
    ItPosixQueue030();
}

/**
 * @tc.name: IT_POSIX_QUEUE_031
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue031, TestSize.Level0)
{
    ItPosixQueue031();
}

/**
 * @tc.name: IT_POSIX_QUEUE_032
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue032, TestSize.Level0)
{
    ItPosixQueue032();
}

/**
 * @tc.name: IT_POSIX_QUEUE_033
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue033, TestSize.Level0)
{
    ItPosixQueue033();
}

/**
 * @tc.name: IT_POSIX_QUEUE_036
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue036, TestSize.Level0)
{
    ItPosixQueue036();
}

/**
 * @tc.name: IT_POSIX_QUEUE_038
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue038, TestSize.Level0)
{
    ItPosixQueue038();
}

/**
 * @tc.name: IT_POSIX_QUEUE_040
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue040, TestSize.Level0)
{
    ItPosixQueue040();
}

#ifndef LOSCFG_USER_TEST_SMP
/**
 * @tc.name: IT_POSIX_QUEUE_041
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue041, TestSize.Level0)
{
    ItPosixQueue041();
}
#endif

/**
 * @tc.name: IT_POSIX_QUEUE_042
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue042, TestSize.Level0)
{
    ItPosixQueue042();
}

/**
 * @tc.name: IT_POSIX_QUEUE_044
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue044, TestSize.Level0)
{
    ItPosixQueue044();
}

/**
 * @tc.name: IT_POSIX_QUEUE_046
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue046, TestSize.Level0)
{
    ItPosixQueue046();
}

/**
 * @tc.name: IT_POSIX_QUEUE_047
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue047, TestSize.Level0)
{
    ItPosixQueue047();
}

/**
 * @tc.name: IT_POSIX_QUEUE_048
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue048, TestSize.Level0)
{
    ItPosixQueue048();
}

/**
 * @tc.name: IT_POSIX_QUEUE_049
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue049, TestSize.Level0)
{
    ItPosixQueue049();
}

/**
 * @tc.name: IT_POSIX_QUEUE_050
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue050, TestSize.Level0)
{
    ItPosixQueue050();
}

/**
 * @tc.name: IT_POSIX_QUEUE_052
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue052, TestSize.Level0)
{
    ItPosixQueue052();
}

/**
 * @tc.name: IT_POSIX_QUEUE_054
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue054, TestSize.Level0)
{
    ItPosixQueue054();
}

/**
 * @tc.name: IT_POSIX_QUEUE_055
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue055, TestSize.Level0)
{
    ItPosixQueue055();
}

/**
 * @tc.name: IT_POSIX_QUEUE_056
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue056, TestSize.Level0)
{
    ItPosixQueue056();
}

/**
 * @tc.name: IT_POSIX_QUEUE_057
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue057, TestSize.Level0)
{
    ItPosixQueue057();
}

/**
 * @tc.name: IT_POSIX_QUEUE_058
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue058, TestSize.Level0)
{
    ItPosixQueue058();
}

/**
 * @tc.name: IT_POSIX_QUEUE_060
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue060, TestSize.Level0)
{
    ItPosixQueue060();
}

/**
 * @tc.name: IT_POSIX_QUEUE_061
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue061, TestSize.Level0)
{
    ItPosixQueue061();
}

/**
 * @tc.name: IT_POSIX_QUEUE_063
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue063, TestSize.Level0)
{
    ItPosixQueue063();
}

/**
 * @tc.name: IT_POSIX_QUEUE_064
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue064, TestSize.Level0)
{
    ItPosixQueue064();
}

/**
 * @tc.name: IT_POSIX_QUEUE_065
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue065, TestSize.Level0)
{
    ItPosixQueue065();
}

/**
 * @tc.name: IT_POSIX_QUEUE_066
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue066, TestSize.Level0)
{
    ItPosixQueue066();
}

/**
 * @tc.name: IT_POSIX_QUEUE_067
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue067, TestSize.Level0)
{
    ItPosixQueue067();
}

/**
 * @tc.name: IT_POSIX_QUEUE_069
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue069, TestSize.Level0)
{
    ItPosixQueue069();
}

/**
 * @tc.name: IT_POSIX_QUEUE_070
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue070, TestSize.Level0)
{
    ItPosixQueue070();
}

/**
 * @tc.name: IT_POSIX_QUEUE_071
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue071, TestSize.Level0)
{
    ItPosixQueue071();
}

/**
 * @tc.name: IT_POSIX_QUEUE_072
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue072, TestSize.Level0)
{
    ItPosixQueue072();
}

/**
 * @tc.name: IT_POSIX_QUEUE_073
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue073, TestSize.Level0)
{
    ItPosixQueue073();
}

/**
 * @tc.name: IT_POSIX_QUEUE_074
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue074, TestSize.Level0)
{
    ItPosixQueue074();
}

/**
 * @tc.name: IT_POSIX_QUEUE_075
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue075, TestSize.Level0)
{
    ItPosixQueue075();
}

/**
 * @tc.name: IT_POSIX_QUEUE_080
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue080, TestSize.Level0)
{
    ItPosixQueue080();
}

/**
 * @tc.name: IT_POSIX_QUEUE_081
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue081, TestSize.Level0)
{
    ItPosixQueue081();
}

/**
 * @tc.name: IT_POSIX_QUEUE_082
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue082, TestSize.Level0)
{
    ItPosixQueue082();
}

/**
 * @tc.name: IT_POSIX_QUEUE_083
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue083, TestSize.Level0)
{
    ItPosixQueue083();
}

/**
 * @tc.name: IT_POSIX_QUEUE_084
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue084, TestSize.Level0)
{
    ItPosixQueue084();
}

/**
 * @tc.name: IT_POSIX_QUEUE_085
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue085, TestSize.Level0)
{
    ItPosixQueue085();
}

/**
 * @tc.name: IT_POSIX_QUEUE_086
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue086, TestSize.Level0)
{
    ItPosixQueue086();
}

/**
 * @tc.name: IT_POSIX_QUEUE_087
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue087, TestSize.Level0)
{
    ItPosixQueue087();
}

/**
 * @tc.name: IT_POSIX_QUEUE_088
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue088, TestSize.Level0)
{
    ItPosixQueue088();
}

/**
 * @tc.name: IT_POSIX_QUEUE_089
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue089, TestSize.Level0)
{
    ItPosixQueue089();
}


#ifndef TEST3559A_M7
/**
 * @tc.name: IT_POSIX_QUEUE_090
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue090, TestSize.Level0)
{
    ItPosixQueue090();
}

#endif
/**
 * @tc.name: IT_POSIX_QUEUE_091
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue091, TestSize.Level0)
{
    ItPosixQueue091();
}

/**
 * @tc.name: IT_POSIX_QUEUE_093
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue093, TestSize.Level0)
{
    ItPosixQueue093();
}

/**
 * @tc.name: IT_POSIX_QUEUE_094
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue094, TestSize.Level0)
{
    ItPosixQueue094();
}

/**
 * @tc.name: IT_POSIX_QUEUE_095
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue095, TestSize.Level0)
{
    ItPosixQueue095();
}

/**
 * @tc.name: IT_POSIX_QUEUE_096
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue096, TestSize.Level0)
{
    ItPosixQueue096();
}

/**
 * @tc.name: IT_POSIX_QUEUE_097
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue097, TestSize.Level0)
{
    ItPosixQueue097();
}

/**
 * @tc.name: IT_POSIX_QUEUE_098
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue098, TestSize.Level0)
{
    ItPosixQueue098();
}

/**
 * @tc.name: IT_POSIX_QUEUE_100
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue100, TestSize.Level0)
{
    ItPosixQueue100();
}

/**
 * @tc.name: IT_POSIX_QUEUE_101
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue101, TestSize.Level0)
{
    ItPosixQueue101();
}

/**
 * @tc.name: IT_POSIX_QUEUE_102
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue102, TestSize.Level0)
{
    ItPosixQueue102();
}

/**
 * @tc.name: IT_POSIX_QUEUE_103
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue103, TestSize.Level0)
{
    ItPosixQueue103();
}

/**
 * @tc.name: IT_POSIX_QUEUE_104
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue104, TestSize.Level0)
{
    ItPosixQueue104();
}

/**
 * @tc.name: IT_POSIX_QUEUE_106
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue106, TestSize.Level0)
{
    ItPosixQueue106();
}

/**
 * @tc.name: IT_POSIX_QUEUE_108
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue108, TestSize.Level0)
{
    ItPosixQueue108();
}

/**
 * @tc.name: IT_POSIX_QUEUE_109
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue109, TestSize.Level0)
{
    ItPosixQueue109();
}

/**
 * @tc.name: IT_POSIX_QUEUE_110
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue110, TestSize.Level0)
{
    ItPosixQueue110();
}


/**
 * @tc.name: IT_POSIX_QUEUE_127
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue127, TestSize.Level0)
{
    ItPosixQueue127();
}

/**
 * @tc.name: IT_POSIX_QUEUE_128
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue128, TestSize.Level0)
{
    ItPosixQueue128();
}

/**
 * @tc.name: IT_POSIX_QUEUE_129
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue129, TestSize.Level0)
{
    ItPosixQueue129();
}

/**
 * @tc.name: IT_POSIX_QUEUE_130
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue130, TestSize.Level0)
{
    ItPosixQueue130();
}

/* *
 * @tc.name: IT_POSIX_QUEUE_144
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue144, TestSize.Level0)
{
    ItPosixQueue144();
}

/**
 * @tc.name: IT_POSIX_QUEUE_147
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue147, TestSize.Level0)
{
    ItPosixQueue147();
}

/**
 * @tc.name: IT_POSIX_QUEUE_148
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue148, TestSize.Level0)
{
    ItPosixQueue148();
}

/**
 * @tc.name: IT_POSIX_QUEUE_149
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue149, TestSize.Level0)
{
    ItPosixQueue149();
}

/**
 * @tc.name: IT_POSIX_QUEUE_150
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue150, TestSize.Level0)
{
    ItPosixQueue150();
}

/**
 * @tc.name: IT_POSIX_QUEUE_151
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue151, TestSize.Level0)
{
    ItPosixQueue151();
}

/**
 * @tc.name: IT_POSIX_QUEUE_152
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue152, TestSize.Level0)
{
    ItPosixQueue152();
}

/**
 * @tc.name: IT_POSIX_QUEUE_153
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue153, TestSize.Level0)
{
    ItPosixQueue153();
}

/**
 * @tc.name: IT_POSIX_QUEUE_154
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue154, TestSize.Level0)
{
    ItPosixQueue154();
}

/**
 * @tc.name: IT_POSIX_QUEUE_155
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue155, TestSize.Level0)
{
    ItPosixQueue155();
}

/**
 * @tc.name: IT_POSIX_QUEUE_156
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue156, TestSize.Level0)
{
    ItPosixQueue156();
}

/**
 * @tc.name: IT_POSIX_QUEUE_164
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue164, TestSize.Level0)
{
    ItPosixQueue164();
}

/**
 * @tc.name: IT_POSIX_QUEUE_165
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue165, TestSize.Level0)
{
    ItPosixQueue165();
}

/**
 * @tc.name: IT_POSIX_QUEUE_166
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue166, TestSize.Level0)
{
    ItPosixQueue166();
}

/**
 * @tc.name: IT_POSIX_QUEUE_168
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue168, TestSize.Level0)
{
    ItPosixQueue168();
}

/**
 * @tc.name: IT_POSIX_QUEUE_169
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue169, TestSize.Level0)
{
    ItPosixQueue169();
}

/**
 * @tc.name: IT_POSIX_QUEUE_173
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue173, TestSize.Level0)
{
    ItPosixQueue173();
}

/**
 * @tc.name: IT_POSIX_QUEUE_175
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue175, TestSize.Level0)
{
    ItPosixQueue175();
}

/**
 * @tc.name: IT_POSIX_QUEUE_176
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue176, TestSize.Level0)
{
    ItPosixQueue176();
}

/**
 * @tc.name: IT_POSIX_QUEUE_187
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue187, TestSize.Level0)
{
    ItPosixQueue187();
}


/**
 * @tc.name: IT_POSIX_QUEUE_200
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue200, TestSize.Level0)
{
    ItPosixQueue200();
}

/**
 * @tc.name: IT_POSIX_QUEUE_201
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue201, TestSize.Level0)
{
    ItPosixQueue201();
}

/**
 * @tc.name: IT_POSIX_QUEUE_202
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue202, TestSize.Level0)
{
    ItPosixQueue202();
}

/**
 * @tc.name: IT_POSIX_QUEUE_203
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue203, TestSize.Level0)
{
    ItPosixQueue203();
}

#if (LOSCFG_USER_TEST_SMP == YES)
/**
 * @tc.name: IT_POSIX_QUEUE_204
 * @tc.desc: function for PosixMqueueTest
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue204, TestSize.Level0)
{
    ItPosixQueue204();
}

#endif

/**
 * @tc.name: IT_POSIX_QUEUE_205
 * @tc.desc: function for mq_notify:Set sigev_notify to SIGEV_NONE
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue205, TestSize.Level0)
{
    ItPosixQueue205();
}

/**
 * @tc.name: IT_POSIX_QUEUE_206
 * @tc.desc: function for mq_notify:The function returns a failure and the error code is verified.
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue206, TestSize.Level0)
{
    ItPosixQueue206();
}

/**
 * @tc.name: IT_POSIX_QUEUE_207
 * @tc.desc: function for mq_notify:Set sigev_notify to SIGEV_NONE
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue207, TestSize.Level0)
{
    ItPosixQueue207();
}

/**
 * @tc.name: IT_POSIX_QUEUE_208
 * @tc.desc: function for mq_notify:The message queue is not empty.
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 */
HWTEST_F(PosixMqueueTest, ItPosixQueue208, TestSize.Level0)
{
    ItPosixQueue208();
}

/**
 * @tc.name: IT_POSIX_QUEUE_209
 * @tc.desc: function for mq_notify:The message queue has waiters.
 * @tc.type: FUNC
 * @tc.require: AR000EEMQ9
 **/
HWTEST_F(PosixMqueueTest, ItPosixQueue209, TestSize.Level0)
{
    ItPosixQueue209();
}

#endif
} // namespace OHOS
