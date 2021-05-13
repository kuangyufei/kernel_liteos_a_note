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
    struct shmid_ds ds = { 0 };
    struct shminfo info = { 0 };

    shmid = shmget(IPC_PRIVATE, PAGE_SIZE, 0777 | IPC_CREAT);
    ICUNIT_ASSERT_NOT_EQUAL(shmid, -1, shmid);

    shm = shmat(shmid, NULL, 0);
    ICUNIT_ASSERT_NOT_EQUAL(shm, INVALID_PTR, shm);

    (void)memset_s(shm, PAGE_SIZE, 0, PAGE_SIZE);

    ret = shmctl(shmid, IPC_STAT, &ds);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);
    ICUNIT_ASSERT_EQUAL(ds.shm_segsz, PAGE_SIZE, ds.shm_segsz);
    ICUNIT_ASSERT_EQUAL(ds.shm_nattch, 1, ds.shm_nattch);
    ICUNIT_ASSERT_EQUAL(ds.shm_cpid, getpid(), ds.shm_cpid);
    ICUNIT_ASSERT_EQUAL(ds.shm_lpid, getpid(), ds.shm_lpid);
    ICUNIT_ASSERT_EQUAL(ds.shm_perm.uid, getuid(), ds.shm_perm.uid);

    ret = shmctl(shmid, SHM_STAT, &ds);
    ICUNIT_ASSERT_EQUAL(ret, 0x10000, ret);

    ds.shm_perm.uid = getuid();
    ds.shm_perm.gid = getgid();
    ds.shm_perm.mode = 0;
    ret = shmctl(shmid, IPC_SET, &ds);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = shmctl(shmid, IPC_INFO, (struct shmid_ds *)&info);
    ICUNIT_ASSERT_EQUAL(ret, 192, ret);
    ICUNIT_ASSERT_EQUAL(info.shmmax, 0x1000000, info.shmmax);
    ICUNIT_ASSERT_EQUAL(info.shmmin, 1, info.shmmin);
    ICUNIT_ASSERT_EQUAL(info.shmmni, 192, info.shmmni);
    ICUNIT_ASSERT_EQUAL(info.shmseg, 128, info.shmseg);
    ICUNIT_ASSERT_EQUAL(info.shmall, 0x1000, info.shmall);

    ret = shmdt(shm);
    ICUNIT_ASSERT_NOT_EQUAL(ret, -1, ret);

    ret = shmctl(shmid, IPC_RMID, NULL);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    return 0;
}

void ItTestShm004(void)
{
    TEST_ADD_CASE("IT_MEM_SHM_004", Testcase, TEST_LOS, TEST_MEM, TEST_LEVEL0, TEST_FUNCTION);
}
