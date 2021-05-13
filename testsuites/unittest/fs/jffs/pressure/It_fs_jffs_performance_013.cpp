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

#include "It_vfs_jffs.h"


static void *PthreadFunc(void *arg)
{
    int fd = -1;
    int ret;
    int num = JFFS_MAX_NUM_TEST;
    char *fileName = { JFFS_PATH_NAME0 };
    char *filebuf = "helloworld";

    while (num-- > 0) {
        fd = open(fileName, O_RDWR | O_CREAT, S_IRUSR);
        if (fd < 0) {
            printf("open %s failed, error: %s\n", fileName, strerror(errno));
        }
        ICUNIT_GOTO_NOT_EQUAL(fd, -1, fd, EXIT);

        ret = write(fd, filebuf, strlen(filebuf));
        if (ret != strlen(filebuf)) {
            printf("write %s failed, ret: %d, error: %s\n", fileName, ret, strerror(errno));
        }
        ICUNIT_GOTO_EQUAL(ret, strlen(filebuf), ret, EXIT1);

        ret = close(fd);
        ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT1);

        ret = unlink(fileName);
        if (ret != 0) {
            printf("remove %s failed, ret: %d, error: %s\n", fileName, ret, strerror(errno));
        }
        ICUNIT_GOTO_EQUAL(ret, 0, ret, EXIT2);
    }
    pthread_exit(0);
EXIT1:
    close(fd);
EXIT2:
    unlink(fileName);
EXIT:
    pthread_exit(0);
}

static UINT32 TestCase(VOID)
{
    pthread_attr_t attr1, attr2;
    pthread_t newTh1, newTh2;
    int ret;
    int num = 100;
    int priority = 14;

    while (num-- > 0) {
        ret = PosixPthreadInit(&attr1, priority);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT);

        ret = pthread_create(&newTh1, &attr1, PthreadFunc, NULL);
        ICUNIT_ASSERT_EQUAL(ret, JFFS_NO_ERROR, ret);

        ret = PosixPthreadInit(&attr2, priority);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

        ret = pthread_create(&newTh2, &attr2, PthreadFunc, NULL);
        ICUNIT_ASSERT_EQUAL(ret, JFFS_NO_ERROR, ret);

        ret = pthread_join(newTh1, NULL);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

        ret = pthread_attr_destroy(&attr1);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);

        ret = pthread_join(newTh2, NULL);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT2);

        ret = pthread_attr_destroy(&attr2);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT2);
    }

    return JFFS_NO_ERROR;
EXIT2:
    pthread_join(newTh2, NULL);
    pthread_attr_destroy(&attr2);
EXIT1:
    pthread_join(newTh1, NULL);
    pthread_attr_destroy(&attr1);
EXIT:
    return JFFS_NO_ERROR;
}


VOID ItFsJffsPerformance013(VOID)
{
    TEST_ADD_CASE("ItFsJffsPerformance013", TestCase, TEST_VFS, TEST_JFFS, TEST_LEVEL3, TEST_PERFORMANCE);
}
