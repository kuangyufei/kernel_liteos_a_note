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

static int Testcase(VOID)
{
    int shmid;
    int ret;
    void *shm = NULL;
    void *vaddrPageAlign = NULL;
    void *vaddr = NULL;

    shmid = shmget(IPC_PRIVATE, PAGE_SIZE, 0777 | IPC_CREAT);
    ICUNIT_ASSERT_NOT_EQUAL(shmid, -1, shmid);

    shm = shmat(shmid, NULL, 0);
    ICUNIT_ASSERT_NOT_EQUAL(shm, INVALID_PTR, shm);

    (void)memset_s(shm, PAGE_SIZE, 0, PAGE_SIZE);

    ret = shmdt(shm);
    ICUNIT_ASSERT_NOT_EQUAL(ret, -1, ret);

    vaddrPageAlign = mmap(NULL, PAGE_SIZE * 2, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    ICUNIT_ASSERT_NOT_EQUAL(vaddrPageAlign, NULL, vaddrPageAlign);

    ret = munmap(vaddrPageAlign, PAGE_SIZE * 2);
    ICUNIT_ASSERT_NOT_EQUAL(ret, -1, ret);

    shm = shmat(shmid, vaddrPageAlign, 0);
    ICUNIT_ASSERT_EQUAL(shm, vaddrPageAlign, shm);

    (void)memset_s(shm, PAGE_SIZE, 0, PAGE_SIZE);

    ret = shmdt(shm);
    ICUNIT_ASSERT_NOT_EQUAL(ret, -1, ret);

    vaddr = (void *)((uintptr_t)vaddrPageAlign + 0x10);
    shm = shmat(shmid, vaddr, SHM_REMAP);
    ICUNIT_ASSERT_NOT_EQUAL(ret, -1, ret);

    shm = shmat(shmid, vaddr, 0);
    ICUNIT_ASSERT_NOT_EQUAL(ret, -1, ret);

    ret = shmctl(shmid, IPC_RMID, NULL);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    return 0;
}

void ItTestShm003(void)
{
    TEST_ADD_CASE("IT_MEM_SHM_003", Testcase, TEST_LOS, TEST_MEM, TEST_LEVEL0, TEST_FUNCTION);
}
