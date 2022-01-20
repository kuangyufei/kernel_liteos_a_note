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
#include "pthread.h"

#define TEXT_SZ 8
static int g_threadCount = 0;

struct shared_use_st {
    int written;
    char text[TEXT_SZ];
};

VOID *ShmReadFunc(VOID *ptr)
{
    void *shm = nullptr;
    struct shared_use_st *shared = nullptr;
    int shmid;
    int ret;

    shmid = shmget((key_t)1234, sizeof(struct shared_use_st), 0666 | IPC_CREAT);
    ICUNIT_ASSERT_NOT_EQUAL_NULL_VOID(shmid, -1, shmid);

    shm = shmat(shmid, 0, 0);
    ICUNIT_ASSERT_NOT_EQUAL_NULL_VOID(shm, INVALID_PTR, shm);

    printf("Memory attached at %p\n", shm);

    shared = (struct shared_use_st *)shm;
    while (1) {
        if (shared->written == 1) {
            printf("You wrote: %s\n", shared->text);
            sleep(1);

            shared->written = 0;
            if (strncmp(shared->text, "end", 3) == 0) {
                break;
            }
        } else {
            sleep(1);
        }
    }

    ret = shmdt(shm);
    ICUNIT_ASSERT_EQUAL_NULL_VOID(ret, 0, ret);

    ret = shmctl(shmid, IPC_RMID, 0);
    ICUNIT_ASSERT_EQUAL_NULL_VOID(ret, 0, ret);

    g_threadCount++;
    return nullptr;
}

VOID *ShmWriteFunc(VOID *ptr)
{
    void *shm = nullptr;
    struct shared_use_st *shared = nullptr;
    char buffer[BUFSIZ + 1] = {'\0'};
    int shmid;
    int ret;
    static int count = 0;

    shmid = shmget((key_t)1234, sizeof(struct shared_use_st), 0666 | IPC_CREAT);
    ICUNIT_ASSERT_NOT_EQUAL_NULL_VOID(shmid, -1, shmid);

    shm = shmat(shmid, (void *)0, 0);
    ICUNIT_ASSERT_NOT_EQUAL_NULL_VOID(shm, INVALID_PTR, shm);

    printf("Memory attched at %p\n", shm);

    shared = (struct shared_use_st *)shm;
    while (1) {
        while (shared->written == 1) {
            sleep(1);
            printf("%s %d, Waiting...\n", __FUNCTION__, __LINE__);
        }

        printf("Enter some text: ");
        if (count == 0) {
            (void)snprintf_s(buffer, BUFSIZ, BUFSIZ + 1, "test");
        } else {
            (void)snprintf_s(buffer, BUFSIZ, BUFSIZ + 1, "end");
        }
        count++;
        (void)strncpy_s(shared->text, TEXT_SZ, buffer, TEXT_SZ - 1);

        shared->written = 1;
        if (strncmp(buffer, "end", 3) == 0) {
            break;
        }
    }

    ret = shmdt(shm);
    ICUNIT_ASSERT_EQUAL_NULL_VOID(ret, 0, ret);

    sleep(1);
    g_threadCount++;
    return nullptr;
}


static int Testcase(VOID)
{
    pthread_t newPthread[2];
    int curThreadPri, curThreadPolicy;
    pthread_attr_t a = { 0 };
    struct sched_param param = { 0 };
    int ret;
    int i, j;

    g_threadCount = 0;

    ret = pthread_getschedparam(pthread_self(), &curThreadPolicy, &param);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    curThreadPri = param.sched_priority;
    ret = pthread_attr_init(&a);
    param.sched_priority = curThreadPri;
    pthread_attr_setschedparam(&a, &param);

    ret = pthread_create(&newPthread[0], &a, ShmReadFunc, NULL);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = pthread_create(&newPthread[1], &a, ShmWriteFunc, NULL);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = pthread_join(newPthread[0], nullptr);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = pthread_join(newPthread[1], nullptr);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ICUNIT_ASSERT_EQUAL(g_threadCount, 2, g_threadCount);
    
    return 0;
}

void ItTestShm001(void)
{
    TEST_ADD_CASE("IT_MEM_SHM_001", Testcase, TEST_LOS, TEST_MEM, TEST_LEVEL0, TEST_FUNCTION);
}
