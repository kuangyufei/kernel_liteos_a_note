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
#include "it_test_shm.h"

#define SHMID_MAX 192

static int Testcase(VOID)
{
    int shmid[SHMID_MAX + 1] = {-1};
    int ret;
    int i;

    shmid[0] = shmget((key_t)0x1234, PAGE_SIZE, 0777 | IPC_CREAT);
    ICUNIT_ASSERT_NOT_EQUAL(shmid[0], -1, shmid[0]);

    ret = shmctl(shmid[0], IPC_RMID, NULL);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    shmid[0] = shmget(IPC_PRIVATE, PAGE_SIZE, 0777 | IPC_CREAT);
    ICUNIT_ASSERT_NOT_EQUAL(shmid[0], -1, shmid[0]);

    ret = shmctl(shmid[0], IPC_RMID, NULL);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    for (i = 0; i < SHMID_MAX; i++) {
        shmid[i] = shmget(IPC_PRIVATE, PAGE_SIZE, 0777 | IPC_CREAT);
        ICUNIT_ASSERT_NOT_EQUAL(shmid[i], -1, shmid[i]);
    }

    shmid[SHMID_MAX] = shmget(IPC_PRIVATE, PAGE_SIZE, 0777 | IPC_CREAT);
    ICUNIT_ASSERT_EQUAL(shmid[SHMID_MAX], -1, shmid[SHMID_MAX]);

    for (i = 0; i < SHMID_MAX; i++) {
        ret = shmctl(shmid[i], IPC_RMID, NULL);
        ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    }

    for (i = 0; i < SHMID_MAX; i++) {
        ret = shmctl(shmid[i], IPC_RMID, NULL);
        ICUNIT_ASSERT_EQUAL(ret, -1, ret);
    }

    return 0;
}

void ItTestShm002(void)
{
    TEST_ADD_CASE("IT_MEM_SHM_002", Testcase, TEST_LOS, TEST_MEM, TEST_LEVEL0, TEST_FUNCTION);
}
