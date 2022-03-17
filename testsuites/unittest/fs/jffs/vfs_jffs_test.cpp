/*
 * Copyright (c) 2013-2019 Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020-2021 Huawei Device Co., Ltd. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this list of
 *    conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list
 *    of conditions and the following disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors may be used
 *    to endorse or promote products derived from this software without specific prior written
 *    permission.
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

#if 1
struct iovec g_jffsIov[10];

INT32 g_jffsFd = 0;
DIR *g_jffsDir = nullptr;
INT32 g_jffsFilesMax = 10;
INT32 g_jffsFlag = 0;
INT32 g_jffsFlagF01 = 0;
INT32 g_jffsFlagF02 = 0;
INT32 g_TestCnt = 0;

INT32 g_jffsFd11[JFFS_MAXIMUM_SIZES];
INT32 g_jffsFd12[JFFS_MAXIMUM_SIZES][JFFS_MAXIMUM_SIZES];

CHAR g_jffsPathname1[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME1 };
CHAR g_jffsPathname2[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME1 };
CHAR g_jffsPathname3[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME1 };
CHAR g_jffsPathname4[JFFS_STANDARD_NAME_LENGTH] = { JFFS_PATH_NAME0 };

CHAR g_jffsPathname6[JFFS_NAME_LIMITTED_SIZE] = { JFFS_PATH_NAME1 };
CHAR g_jffsPathname7[JFFS_NAME_LIMITTED_SIZE] = { JFFS_PATH_NAME1 };
CHAR g_jffsPathname8[JFFS_NAME_LIMITTED_SIZE] = { JFFS_PATH_NAME1 };

CHAR g_jffsPathname11[JFFS_MAXIMUM_SIZES][JFFS_NAME_LIMITTED_SIZE] = { 0, };
CHAR g_jffsPathname12[JFFS_MAXIMUM_SIZES][JFFS_NAME_LIMITTED_SIZE] = { 0, };
CHAR g_jffsPathname13[JFFS_MAXIMUM_SIZES][JFFS_NAME_LIMITTED_SIZE] = { 0, };
#endif
INT32 g_jffsGrandSize[JFFS_MAX_DEF_BUF_NUM] = { 29, 30290, 3435, 235, 12345, 80,
    9845, 564, 34862, 123, 267890, 36, 6788, 86, 234567, 1232, 514, 50, 678, 9864, 333333 };

pthread_mutex_t g_jffs2GlobalLock1;
pthread_mutex_t g_jffs2GlobalLock2;

#if 1
INT32 JffsMultiWrite(CHAR *path, INT64 fileSize, INT32 writeSize, int oflags, INT32 interfaceType)
{
    INT32 ret, fd;
    INT64 total = 0;
    INT64 totalSize = 0;

    CHAR *writeBuf;
    FILE *file;

    writeBuf = (CHAR *)malloc(writeSize);
    ICUNIT_GOTO_NOT_EQUAL(writeBuf, NULL, writeBuf, EXIT);

    if (interfaceType == 1) {
        fd = open(path, oflags, S_IRUSR | S_IWUSR);
        if (fd < 0) {
            goto EXIT1;
        }
    } else {
        file = fopen(path, "w+b");
        if (file == nullptr) {
            goto EXIT2;
        }
    }

    while (1) {
        if (interfaceType == 1) {
            ret = write(fd, writeBuf, writeSize);
            if (ret <= 0) {
                if (errno == ENOSPC) {
                    dprintf("No space !! %s,\n", strerror(errno));
                    goto EXIT1;
                }
                dprintf("jffs_multi_write fail,path = %s,ret=:%d ,errno=:%d!\n", path, ret, errno);
                goto EXIT1;
            }
        } else {
            ret = fwrite(writeBuf, 1, writeSize, file);
            if (ret <= 0 || ret != writeSize) {
                if (errno == ENOSPC) {
                    dprintf("No space !! %s,\n", strerror(errno));
                }
                dprintf("jffs_multi_write error! %s ,path=:%s, ret = %d,\n", strerror(errno), path, ret);

                goto EXIT2;
            }
        }
        total += ret;
        totalSize += ret;

        if (total >= JFFS_WR_CAP_SIZE_TEST) {
            if (interfaceType == 1) {
            } else {
            }
            total = 0;
        }

        if (fileSize <= totalSize) {
            break;
        }
    }

    if (interfaceType == 1) {
        ret = close(fd);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);
    } else {
        ret = fclose(file);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);
    }

    free(writeBuf);

    return JFFS_NO_ERROR;
EXIT2:
    fclose(file);
    goto EXIT;
EXIT1:
    close(fd);
EXIT:
    free(writeBuf);
    return JFFS_IS_ERROR;
}


INT32 JffsMultiRead(CHAR *path, INT64 fileSize, INT32 readSize, int oflags, INT32 interfaceType)
{
    INT32 fd, ret;
    INT64 total = 0;
    INT64 totalSize = 0;
    FILE *file;
    CHAR *readBuf;

    readBuf = (CHAR *)malloc(readSize);
    ICUNIT_ASSERT_NOT_EQUAL(readBuf, NULL, readBuf);

    if (interfaceType == 1) {
        fd = open(path, oflags, S_IRUSR | S_IWUSR);
        if (fd < 0) {
            goto EXIT1;
        }
    } else {
        file = fopen(path, "rb");
        if (file == nullptr) {
            goto EXIT2;
        }
    }

    while (1) {
        if (interfaceType == 1) {
            ret = read(fd, readBuf, readSize);
            if (ret < 0) {
                dprintf("ret = %d,%s read fail!-->(X),\n", ret, path);
                goto EXIT1;
            }
            if (!ret) {
                dprintf("warning: read ret = 0,\n");
                goto EXIT1;
            }
        } else {
            ret = fread(readBuf, 1, readSize, file);
            if (ret <= 0) {
                if (feof(file) == 1) {
                    dprintf("yyy feof of %s,\n", path);
                } else {
                    dprintf("fread error!,\n");
                    goto EXIT2;
                }
            }
        }
        total += ret;
        totalSize += ret;

        if (total >= JFFS_WR_CAP_SIZE_TEST) {
            total = 0;
        }

        if (fileSize <= totalSize) {
            break;
        }
    }

    if (interfaceType == 1) {
        ret = close(fd);
        if (ret < 0) {
            dprintf("fail to close  %s!\n", strerror(errno));
        }
    } else {
        ret = fclose(file);
        if (ret < 0) {
            dprintf("fail to fclose  %s!\n", strerror(errno));
        }
    }

    free(readBuf);
    return JFFS_NO_ERROR;
EXIT2:
    fclose(file);
EXIT1:
    close(fd);
    free(readBuf);
    return JFFS_IS_ERROR;
}
#endif

INT32 JffsFixWrite(CHAR *path, INT64 fileSize, INT32 writeSize, INT32 interfaceType)
{
    INT32 ret, i, fd;
    INT64 total = 0;
    CHAR filebuf[256] = "abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123"
                        "456789abcedfghij9876543210abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123456789abcedfgh"
                        "ij9876543210abcdeabcde0123456789abcedfghij9876543210lalalalalalalal";
    INT64 perTime;
    INT64 totalSize = 0;
    INT64 totalTime = 0;
    INT32 cycleCount = 0;
    INT32 taskId;
    struct timeval testTime1;
    struct timeval testTime2;
    DOUBLE testSpeed;
    CHAR *pid;
    CHAR *writeBuf;
    FILE *file;

    taskId = strlen(path);
    pid = path + taskId - 1;
    taskId = atoi(pid);

    writeBuf = (CHAR *)malloc(writeSize);
    ICUNIT_GOTO_NOT_EQUAL(writeBuf, NULL, writeBuf, EXIT);
    memset(writeBuf, 0, writeSize);

    for (i = 0; i < writeSize / 256; i++) { // 256 means the maxsize of write len
        strcat(writeBuf, filebuf);
    }

    gettimeofday(&testTime1, 0);
    if (interfaceType == 1) {
        fd = open(path, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
        if (fd < 0) {
            printf("Task_%d fail to open %s,\n", taskId, path);
            goto EXIT1;
        }
    } else {
        file = fopen(path, "w+b");
        if (file == nullptr) {
            printf("Task_%d fail to fopen %s,\n", taskId, path);
            goto EXIT2;
        }
    }
    gettimeofday(&testTime2, 0);
    perTime = (testTime2.tv_sec - testTime1.tv_sec) * USECS_PER_SEC + (testTime2.tv_usec - testTime1.tv_usec);
    printf("fix_Write TaskID:%3d,open %s ,task %lld ms ,\n", taskId, path, perTime / MSECS_PER_SEC);

    gettimeofday(&testTime1, 0);

    while (1) {
        if (interfaceType == 1) {
            ret = write(fd, writeBuf, writeSize);
            if (ret <= 0) {
                if (errno == ENOSPC) {
                    printf("No space !! %s,\n", strerror(errno));
                    goto EXIT1;
                }
                printf("fix_write fail,path = %s,ret=:%d ,errno=:%d!\n", path, ret, errno);
                goto EXIT1;
            }
        } else {
            ret = fwrite(writeBuf, 1, writeSize, file);
            if (ret <= 0 || ret != writeSize) {
                if (errno == ENOSPC) {
                    printf("No space !! %s,\n", strerror(errno));
                }
                printf("fix_fwrite error! %s ,path=:%s, ret = %d,\n", strerror(errno), path, ret);

                goto EXIT2;
            }
        }
        total += ret;
        totalSize += ret;
        if (total >= JFFS_WR_CAP_SIZE_TEST) {
            gettimeofday(&testTime2, 0);

            perTime = (testTime2.tv_sec - testTime1.tv_sec) * USECS_PER_SEC + (testTime2.tv_usec - testTime1.tv_usec);

            totalTime += perTime;

            testSpeed = total * 1.0;
            testSpeed = testSpeed * USECS_PER_SEC / perTime;
            testSpeed = testSpeed / BYTES_PER_MBYTE;

            if (interfaceType == 1) {
                printf("fix_Write TaskID:%3d,%d time write, write %lldbytes, cost %lld usecs, speed: %0.3lfMB/s,\n",
                    taskId, cycleCount++, total, perTime, testSpeed);
            } else {
                printf("fix_fwrite TaskID:%3d,%d time write, write %lldbytes, cost %lld usecs, speed: %0.3lfMB/s,\n",
                    taskId, cycleCount++, total, perTime, testSpeed);
            }
            total = 0;
            gettimeofday(&testTime1, 0);
        }

        if (fileSize <= totalSize) {
            break;
        }
    }

    gettimeofday(&testTime2, 0);
    perTime = (testTime2.tv_sec - testTime1.tv_sec) * USECS_PER_SEC + (testTime2.tv_usec - testTime1.tv_usec);

    totalTime += perTime;

    testSpeed = totalSize * 1.0;
    testSpeed = testSpeed * USECS_PER_SEC / totalTime;
    testSpeed = testSpeed / BYTES_PER_MBYTE;

    printf("\nfix_Write TaskID:%3d,total write=%lld , time=%lld,arv speed =%0.3lfMB/s ,\n", taskId, totalSize,
        totalTime, testSpeed);
    gettimeofday(&testTime1, 0);
    if (interfaceType == 1) {
        ret = close(fd);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);
    } else {
        ret = fclose(file);
        ICUNIT_GOTO_EQUAL(ret, JFFS_NO_ERROR, ret, EXIT1);
    }

    gettimeofday(&testTime2, 0);
    perTime = (testTime2.tv_sec - testTime1.tv_sec) * USECS_PER_SEC + (testTime2.tv_usec - testTime1.tv_usec);

    printf("fix_Write TaskID:%3d,sucess to fclose the %s ,task:%d ms,\n", taskId, path, (perTime / USECS_PER_SEC) * MSECS_PER_SEC);

    free(writeBuf);

    return JFFS_NO_ERROR;
EXIT2:
    fclose(file);
    goto EXIT;
EXIT1:
    close(fd);
EXIT:
    free(writeBuf);
    return JFFS_NO_ERROR;
}

INT32 JffsFixRead(CHAR *path, INT64 fileSize, INT32 readSize, INT32 interfaceType)
{
    INT32 fd, taskId, ret;
    INT32 cycleCount = 0;
    INT64 total = 0;
    INT64 totalSize = 0;
    INT64 perTime;
    INT64 totalTime = 0;
    FILE *file;
    CHAR *readBuf;
    CHAR *pid;
    DOUBLE testSpeed;
    struct timeval testTime1;
    struct timeval testTime2;

    taskId = strlen(path);
    pid = path + taskId - 1;
    taskId = atoi(pid);

    readBuf = (CHAR *)malloc(readSize);
    ICUNIT_ASSERT_NOT_EQUAL(readBuf, NULL, readBuf);

    gettimeofday(&testTime1, 0);

    if (interfaceType == 1) {
        fd = open(path, O_RDWR, S_IRUSR | S_IWUSR);
        if (fd < 0) {
            printf("Task_%d fail to open %s,\n", taskId, path);
            goto EXIT1;
        }
    } else {
        file = fopen(path, "rb");
        if (file == nullptr) {
            printf("Task_%d fail to fopen %s,\n", taskId, path);
            goto EXIT2;
        }
    }

    gettimeofday(&testTime2, 0);

    perTime = (testTime2.tv_sec - testTime1.tv_sec) * USECS_PER_SEC + (testTime2.tv_usec - testTime1.tv_usec);

    printf("fix_Read TaskID:%3d,open %s , task:%lld ms,\n", taskId, path, perTime / MSECS_PER_SEC);

    gettimeofday(&testTime1, 0);

    while (1) {
        if (interfaceType == 1) {
            ret = read(fd, readBuf, readSize);
            if (ret < 0) {
                printf("ret = %d,%s read fail!-->(X),\n", ret, path);
                goto EXIT1;
            }
            if (!ret) {
                printf("warning: read ret = 0,\n");
                goto EXIT1;
            }
        } else {
            ret = fread(readBuf, 1, readSize, file);
            if (ret <= 0) {
                if (feof(file) == 1) {
                    printf("feof of %s,\n", path);
                } else {
                    printf("fread error!,\n");
                    goto EXIT2;
                }
            }
        }
        total += ret;
        totalSize += ret;

        if (total >= JFFS_WR_CAP_SIZE_TEST) {
            gettimeofday(&testTime2, 0);

            perTime = (testTime2.tv_sec - testTime1.tv_sec) * USECS_PER_SEC + (testTime2.tv_usec - testTime1.tv_usec);

            totalTime += perTime;

            testSpeed = total * 1.0;
            testSpeed = testSpeed * USECS_PER_SEC / perTime;
            testSpeed = testSpeed / BYTES_PER_MBYTE;

            printf("fix_Read TaskID:%3d,times %d, read %lldbytes, cost %lld usecs, speed: %0.3lfMB/s,\n", taskId,
                cycleCount++, total, perTime, testSpeed);

            total = 0;
            gettimeofday(&testTime1, 0);
        }

        if (fileSize <= totalSize) {
            break;
        }
    }

    gettimeofday(&testTime2, 0);
    perTime = (testTime2.tv_sec - testTime1.tv_sec) * USECS_PER_SEC + (testTime2.tv_usec - testTime1.tv_usec);

    totalTime += perTime;

    testSpeed = totalSize * 1.0;
    testSpeed = testSpeed * USECS_PER_SEC / totalTime;
    testSpeed = testSpeed / BYTES_PER_MBYTE;

    printf("\nfix_Read TaskID:%3d,total read=%lld , time=%lld,arv speed =%0.3lfMB/s ,\n", taskId, totalSize,
        totalTime, testSpeed);

    gettimeofday(&testTime1, 0);
    if (interfaceType == 1) {
        ret = close(fd);
        if (ret < 0) {
            printf("fail to close  %s!\n", strerror(errno));
        }
    } else {
        ret = fclose(file);
        if (ret < 0) {
            dprintf("fail to fclose  %s!\n", strerror(errno));
        }
    }
    gettimeofday(&testTime2, 0);
    perTime = (testTime2.tv_sec - testTime1.tv_sec) * USECS_PER_SEC + (testTime2.tv_usec - testTime1.tv_usec);

    dprintf("fix_Read TaskID:%3d, fclose %s!,task:%lld ms,\n", taskId, path, perTime / MSECS_PER_SEC);

    ret = remove(path);
    if (ret < 0) {
        dprintf("fail to remove  %s!\n", strerror(errno));
    }

    dprintf("Read TaskID:%3d,sucess to fread the %s,\n", taskId, path);
    free(readBuf);
    return 0;
EXIT2:
    fclose(file);
EXIT1:
    close(fd);
    free(readBuf);
    return JFFS_NO_ERROR;
}

INT32 JffsRandWrite(CHAR *path, INT64 fileSize, INT32 interfaceType)
{
    INT32 ret, i, fd;
    INT32 cycleCount = 0;
    INT32 taskId;
    CHAR filebuf[256] = "abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123"
                        "456789abcedfghij9876543210abcdeabcde0123456789abcedfghij9876543210abcdeabcde0123456789abcedfgh"
                        "ij9876543210abcdeabcde0123456789abcedfghij9876543210lalalalalalalal";
    INT64 total = 0;
    INT64 totalSize = 0;
    INT64 perTime;
    INT64 totalTime = 0;
    CHAR *writeBuf;
    CHAR *pid;
    struct timeval testTime1;
    struct timeval testTime2;
    DOUBLE testSpeed;
    FILE *file;

    taskId = strlen(path);
    pid = path + taskId - 1;
    taskId = atoi(pid);

    writeBuf = (CHAR *)malloc(JFFS_PRESSURE_W_R_SIZE);
    ICUNIT_GOTO_NOT_EQUAL(writeBuf, NULL, writeBuf, EXIT);
    memset(writeBuf, 0, JFFS_PRESSURE_W_R_SIZE);

    for (i = 0; i < JFFS_PRESSURE_W_R_SIZE / 256; i++) { // 256 means the maxsize of write len
        strcat(writeBuf, filebuf);
    }

    gettimeofday(&testTime1, 0);
    if (interfaceType == 1) {
        fd = open(path, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
        if (fd < 0) {
            dprintf("Task_%d fail to open %s,\n", taskId, path);
            goto EXIT1;
        }
    } else {
        file = fopen(path, "w+b");
        if (file == nullptr) {
            dprintf("Task_%d fail to fopen %s,\n", taskId, path);
            goto EXIT2;
        }
    }

    gettimeofday(&testTime2, 0);
    perTime = (testTime2.tv_sec - testTime1.tv_sec) * USECS_PER_SEC + (testTime2.tv_usec - testTime1.tv_usec);
    dprintf("rand_write TaskID:%3d,open %s , cost:%lld ms,\n", taskId, path, perTime / MSECS_PER_SEC);

    gettimeofday(&testTime1, 0);

    i = 0;

    while (1) {
        if (interfaceType == 1) {
            ret = write(fd, writeBuf, g_jffsGrandSize[i]);
            if (ret <= 0) {
                dprintf("ret = %d,%s write fail!-->(X),\n", ret, path);
                if (errno == ENOSPC) {
                    dprintf("No space !! %s,\n", strerror(errno));
                    goto EXIT1;
                }
                goto EXIT1;
            }
        } else {
            ret = fwrite(writeBuf, 1, g_jffsGrandSize[i], file);
            if (ret <= 0 || ret != g_jffsGrandSize[i]) {
                if (errno == ENOSPC) {
                    dprintf("No space !! %s,\n", strerror(errno));
                }
                dprintf("rand_Write TaskID:%3d,fwrite error! %s , ret = %d,\n", taskId, strerror(errno), ret);
                goto EXIT2;
            }
        }

        total += ret;
        totalSize += ret;
        if (total >= JFFS_WR_CAP_SIZE_TEST) {
            gettimeofday(&testTime2, 0);

            perTime = (testTime2.tv_sec - testTime1.tv_sec) * USECS_PER_SEC + (testTime2.tv_usec - testTime1.tv_usec);

            totalTime += perTime;
            testSpeed = total * 1.0;
            testSpeed = testSpeed * USECS_PER_SEC / perTime;
            testSpeed = testSpeed / BYTES_PER_MBYTE;

            dprintf("rand_Write TaskID:%3d,%d time write, write %lldbytes, cost %lld usecs, speed: %0.3lfMB/s,\n",
                taskId, cycleCount++, total, perTime, testSpeed);

            total = 0;
            gettimeofday(&testTime1, 0);
        }

        if (fileSize <= totalSize) {
            break;
        }
        if (++i >= JFFS_MAX_DEF_BUF_NUM)
            i = 0;
    }

    gettimeofday(&testTime2, 0);
    perTime = (testTime2.tv_sec - testTime1.tv_sec) * USECS_PER_SEC + (testTime2.tv_usec - testTime1.tv_usec);

    totalTime += perTime;

    testSpeed = totalSize * 1.0;
    testSpeed = testSpeed * USECS_PER_SEC / totalTime;
    testSpeed = testSpeed / BYTES_PER_MBYTE;
    dprintf("--------------------------------\n");
    dprintf("rand_Write TaskID:%3d,total write=%lld , time=%lld,arv speed =%0.3lfMB/s ,\n", taskId, totalSize,
        totalTime, testSpeed);

    gettimeofday(&testTime1, 0);
    if (interfaceType == 1) {
        ret = close(fd);
        if (ret < 0) {
            dprintf("rand_Write TaskID:%3d,fail to close  %s!\n", taskId, strerror(errno));
            goto EXIT1;
        }
    } else {
        ret = fclose(file);
        if (ret < 0) {
            dprintf("rand_Write TaskID:%3d,fail to fclose  %s!\n", taskId, strerror(errno));
            goto EXIT2;
        }
    }

    gettimeofday(&testTime2, 0);
    perTime = (testTime2.tv_sec - testTime1.tv_sec) * USECS_PER_SEC + (testTime2.tv_usec - testTime1.tv_usec);

    dprintf("rand_Write TaskID:%3d,sucess to fclose the %s ,task %lld,\n", taskId, path, perTime / MSECS_PER_SEC);

    free(writeBuf);

    return JFFS_NO_ERROR;
EXIT2:
    fclose(file);
    goto EXIT;
EXIT1:
    close(fd);
EXIT:
    free(writeBuf);
    return JFFS_NO_ERROR;
}


INT32 JffsRandRead(CHAR *path, INT64 fileSize, INT32 interfaceType)
{
    INT32 ret, fd, i;
    INT32 cycleCount = 0;
    INT32 taskId;
    INT64 total = 0;
    INT64 totalSize = 0;
    INT64 perTime;
    INT64 totalTime = 0;
    struct timeval testTime1;
    struct timeval testTime2;
    DOUBLE testSpeed;
    CHAR *pid;
    CHAR *readBuf;
    FILE *file;

    taskId = strlen(path);
    pid = path + taskId - 1;
    taskId = atoi(pid);

    readBuf = (CHAR *)malloc(JFFS_PRESSURE_W_R_SIZE);
    ICUNIT_GOTO_NOT_EQUAL(readBuf, NULL, readBuf, EXIT);

    gettimeofday(&testTime1, 0);
    if (interfaceType == 1) {
        fd = open(path, O_RDWR, S_IRUSR | S_IWUSR);
        if (-1 == fd) {
            printf("fail to open %s\n", path);
            goto EXIT1;
        }
    } else {
        file = fopen(path, "rb");
        if (file == nullptr) {
            printf("fail to fopen %s\n", path);
            goto EXIT2;
        }
    }

    gettimeofday(&testTime2, 0);
    perTime = (testTime2.tv_sec - testTime1.tv_sec) * USECS_PER_SEC + (testTime2.tv_usec - testTime1.tv_usec);
    printf("rand_read, open %s , task:%lld ms,\n", path, perTime / MSECS_PER_SEC);

    i = 0;
    gettimeofday(&testTime1, 0);

    while (1) {
        if (interfaceType == 1) {
            ret = read(fd, readBuf, g_jffsGrandSize[i]);
            if (ret < 0) {
                printf("ret = %d,%s read fail!-->(X)\n", ret, path);
                goto EXIT1;
            }
            if (!ret) {
                printf("warning: read ret = 0,\n");
            }
        } else {
            ret = fread(readBuf, 1, g_jffsGrandSize[i], file);
            if (ret <= 0) {
                if (feof(file) == 1) {
                    printf("feof of %s\n", path);
                } else {
                    printf("fread error!\n");
                    goto EXIT2;
                }
            }
        }
        total += ret;
        totalSize += ret;

        if (total >= JFFS_WR_CAP_SIZE_TEST) {
            gettimeofday(&testTime2, 0);

            perTime = (testTime2.tv_sec - testTime1.tv_sec) * USECS_PER_SEC + (testTime2.tv_usec - testTime1.tv_usec);

            totalTime += perTime;

            testSpeed = total * 1.0;
            testSpeed = testSpeed * USECS_PER_SEC / perTime;
            testSpeed = testSpeed / BYTES_PER_MBYTE;

            dprintf("rand_Read TaskID:%3d, times %d, read %lldbytes, cost %lld usecs, speed: %0.3lfMB/s,\n", taskId,
                cycleCount++, total, perTime, testSpeed);

            total = 0;
            gettimeofday(&testTime1, 0);
        }

        if (fileSize <= totalSize) {
            break;
        }
        if (++i >= JFFS_MAX_DEF_BUF_NUM)
            i = 0;
    }

    gettimeofday(&testTime2, 0);
    perTime = (testTime2.tv_sec - testTime1.tv_sec) * USECS_PER_SEC + (testTime2.tv_usec - testTime1.tv_usec);

    totalTime += perTime;

    testSpeed = totalSize * 1.0;
    testSpeed = testSpeed * USECS_PER_SEC / totalTime;
    testSpeed = testSpeed / BYTES_PER_MBYTE;
    printf("--------------------------------\n");
    printf("rand_Read TaskID:%3d ,total read=%lld , time=%lld,arv speed =%0.3lfMB/s ,\n", taskId, totalSize,
        totalTime, testSpeed);

    gettimeofday(&testTime1, 0);
    if (interfaceType == 1) {
        ret = close(fd);
        if (ret < 0) {
            printf("fail to close  %s!\n", strerror(errno));
        }
    } else {
        ret = fclose(file);
        if (ret < 0) {
            printf("fail to fclose  %s!\n", strerror(errno));
        }
    }
    gettimeofday(&testTime2, 0);
    perTime = (testTime2.tv_sec - testTime1.tv_sec) * USECS_PER_SEC + (testTime2.tv_usec - testTime1.tv_usec);

    printf(" rand_Read TaskID:%3d,fclose %s!,task:%lld ms,\n", taskId, path, perTime / MSECS_PER_SEC);

    ret = remove(path);
    if (ret < 0) {
        printf("fail to fclose  %s!\n", strerror(errno));
    }

    printf("rand_Read TaskID:%3d,sucess to fread the %s,\n", taskId, path);

    free(readBuf);
    return JFFS_NO_ERROR;

EXIT2:
    fclose(file);
    goto EXIT1;
EXIT1:
    close(fd);
EXIT:
    free(readBuf);
    return JFFS_NO_ERROR;
}

INT32 JffsDeleteDirs(char *str, int n)
{
    struct dirent *ptr;
    int ret;
    DIR *dir;
    int i = 0;

    ret = chdir(str);
    ICUNIT_ASSERT_EQUAL(ret, JFFS_NO_ERROR, ret);

    dir = opendir(str);
    ICUNIT_ASSERT_NOT_EQUAL(dir, NULL, dir);

    rewinddir(dir);
    while ((ptr = readdir(dir)) != nullptr) {
        if ((strcmp(ptr->d_name, "..") == 0) || (strcmp(ptr->d_name, ".") == 0))
            continue;

        if (i == n) {
            break;
        }
        i++;

        ret = rmdir(ptr->d_name);
        ICUNIT_ASSERT_EQUAL(ret, JFFS_NO_ERROR, ret);
    }

    ret = closedir(dir);
    ICUNIT_ASSERT_EQUAL(ret, JFFS_NO_ERROR, ret);

    ret = rmdir(str);
    ICUNIT_ASSERT_EQUAL(ret, JFFS_NO_ERROR, ret);

    return JFFS_NO_ERROR;
}
INT32 JffsDeletefile(int fd, char *pathname)
{
    int ret;

    ret = close(fd);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    ret = unlink(pathname);
    ICUNIT_ASSERT_EQUAL(ret, 0, ret);

    return JFFS_NO_ERROR;
}

INT32 JffsStrcat2(char *pathname, char *str, int len)
{
    memset(pathname, 0, len);
    strcpy(pathname, JFFS_PATH_NAME0);
    strcat(pathname, str);

    return 0;
}
INT32 JffsScandirFree(struct dirent **namelist, int n)
{
    if (n < 0 || namelist == nullptr) {
        return -1;
    } else if (n == 0) {
        free(namelist);
        return 0;
    }
    while (n--) {
        free(namelist[n]);
    }
    free(namelist);

    return 0;
}

INT32 JffsStatPrintf(struct stat sb)
{
#if VFS_STAT_PRINTF == 1

    dprintf("File type:                ");

    switch (sb.st_mode & S_IFMT) {
        case S_IFBLK:
            dprintf("block device\n");
            break;
        case S_IFCHR:
            dprintf("character device\n");
            break;
        case S_IFDIR:
            dprintf("directory\n");
            break;
        case S_IFIFO:
            dprintf("FIFO/pipe\n");
            break;
        case S_IFLNK:
            dprintf("symlink\n");
            break;
        case S_IFREG:
            dprintf("regular file\n");
            break;
        case S_IFSOCK:
            dprintf("socket\n");
            break;
        default:
            dprintf("unknown?\n");
            break;
    }

    switch (sb.st_mode & S_IRWXU) {
        case S_IRUSR:
            dprintf("Owners have read permission\n");
            break;
        case S_IWUSR:
            dprintf("Owners have write permission\n");
            break;
        case S_IXUSR:
            dprintf("Owners have execute permissions\n");
            break;
        default:
            break;
    }

    switch (sb.st_mode & S_IRWXG) {
        case S_IRGRP:
            dprintf("Group has read permission\n");
            break;
        case S_IWGRP:
            dprintf("Group has write permission\n");
            break;
        case S_IXGRP:
            dprintf("Group has execute permission\n");
            break;
        default:
            break;
    }

    switch (sb.st_mode & S_IRWXO) {
        case S_IROTH:
            dprintf("Other have read permission\n");
            break;
        case S_IWOTH:
            dprintf("Other has write permission\n");
            break;
        case S_IXOTH:
            dprintf("Other has execute permission\n");
            break;
        default:
            break;
    }

    dprintf("I-node number:            %ld\n", (long)sb.st_ino);
    dprintf("Mode:                     %lo (octal)\n", (unsigned long)sb.st_mode);
    dprintf("Link count:               %ld\n", (long)sb.st_nlink);
    dprintf("Ownership:                UID=%ld   GID=%ld\n", (long)sb.st_uid, (long)sb.st_gid);

    dprintf("Preferred I/O block size: %ld bytes\n", (long)sb.st_blksize);
    dprintf("File size:                %lld bytes\n", (long long)sb.st_size);
    dprintf("Blocks allocated:         %lld\n", (long long)sb.st_blocks);

    dprintf("Last status change:       %s", ctime(&sb.st_ctime));
    dprintf("Last file access:         %s", ctime(&sb.st_atime));
    dprintf("Last file modification:   %s\n", ctime(&sb.st_mtime));

#endif
    return 0;
}

INT32 JffsStatfsPrintf(struct statfs buf)
{
#if VFS_STATFS_PRINTF == 1
    dprintf("type of file system :            0x%x\n", buf.f_type);
    dprintf("optimal transfer block size :            %ld\n", (long)buf.f_bsize);
    dprintf("total data blocks in file system :            %ld\n", (long)buf.f_blocks);
    dprintf("free blocks in fs :            %ld\n", (long)buf.f_bfree);
    dprintf("free blocks available to unprivileged user :            %ld\n", (long)buf.f_bavail);
    dprintf("total file nodes in file system :            %ld\n", (long)buf.f_files);

    dprintf("free file nodes in fs :        %ld\n", (long)buf.f_ffree);
    dprintf("file system id :            %d\n", buf.f_fsid.__val[0]);
    dprintf("maximum length of filenames :            0x%x\n", buf.f_namelen);
    dprintf("fragment size:            %d\n\n", buf.f_frsize);
#endif
    return 0;
}


using namespace testing::ext;
namespace OHOS {
#if defined(LOSCFG_USER_TEST_PRESSURE)
pthread_mutexattr_t mutex;
#endif
class VfsJffsTest : public testing::Test {
public:
    static void SetUpTestCase(void)
    {
#if defined(LOSCFG_USER_TEST_PRESSURE)
        pthread_mutexattr_settype(&mutex, PTHREAD_MUTEX_NORMAL);
        pthread_mutex_init(&g_jffs2GlobalLock1, &mutex);
        pthread_mutex_init(&g_jffs2GlobalLock2, &mutex);
#endif
    }
    static void TearDownTestCase(void) {}
};
#if defined(LOSCFG_USER_TEST_FULL)
/* *
 * @tc.name: IO_TEST_FACCESSAT_001
 * @tc.desc: normal tests for faccessat
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, IO_TEST_FACCESSAT_001, TestSize.Level0)
{
    IO_TEST_FACCESSAT_001();
}

/* *
 * @tc.name: IO_TEST_FACCESSAT_002
 * @tc.desc: innormal tests for faccessat
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, IO_TEST_FACCESSAT_002, TestSize.Level0)
{
    IO_TEST_FACCESSAT_002();
}

/* *
 * @tc.name: IO_TEST_FSTATFS_001
 * @tc.desc: normal tests for fstatfs
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, IO_TEST_FSTATFS_001, TestSize.Level0)
{
    IO_TEST_FSTATFS_001();
}

/* *
 * @tc.name: IO_TEST_FSTATFS_002
 * @tc.desc: innormal tests for fstatfs
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, IO_TEST_FSTATFS_002, TestSize.Level0)
{
    IO_TEST_FSTATFS_002();
}

/* *
 * @tc.name: IO_TEST_FSTATAT_001
 * @tc.desc: normal tests for fstatat
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, IO_TEST_FSTATAT_001, TestSize.Level0)
{
    IO_TEST_FSTATAT_001();
}

/* *
 * @tc.name: IO_TEST_FSTATAT_002
 * @tc.desc: innormal tests for fstatat
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, IO_TEST_FSTATAT_002, TestSize.Level0)
{
    IO_TEST_FSTATAT_002();
}

/* *
 * @tc.name: ItTestFsJffs001
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItTestFsJffs001, TestSize.Level0)
{
    ItTestFsJffs001();
}

/* *
 * @tc.name: ItTestFsJffs002
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItTestFsJffs002, TestSize.Level0)
{
    ItTestFsJffs002();
}

/* *
 * @tc.name: ItTestFsJffs003
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItTestFsJffs003, TestSize.Level0)
{
    ItTestFsJffs003();
}

/* *
 * @tc.name: ItTestFsJffs004
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItTestFsJffs004, TestSize.Level0)
{
    ItTestFsJffs004();
}

/* *
 * @tc.name: ItTestFsJffs100
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItTestFsJffs100, TestSize.Level0)
{
    ItTestFsJffs100();
}

/* *
 * @tc.name: ItTestFsJffs101
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItTestFsJffs101, TestSize.Level0)
{
    ItTestFsJffs101();
}

/* *
 * @tc.name: ItTestFsJffs102
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItTestFsJffs102, TestSize.Level0)
{
    ItTestFsJffs102();
}

/* *
 * @tc.name: ItTestFsJffs103
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItTestFsJffs103, TestSize.Level0)
{
    ItTestFsJffs103();
}

/* *
 * @tc.name: ItTestFsJffs106
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItTestFsJffs106, TestSize.Level0)
{
    ItTestFsJffs106();
}

/* *
 * @tc.name: ItTestFsJffs112
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItTestFsJffs112, TestSize.Level0)
{
    ItTestFsJffs112();
}

/* *
 * @tc.name: ItTestFsJffs113
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItTestFsJffs113, TestSize.Level0)
{
    ItTestFsJffs113();
}

/* *
 * @tc.name: IT_JFFS_002
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItJffs002, TestSize.Level0)
{
    ItJffs002();
}

/* *
 * @tc.name: IT_JFFS_004
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItJffs004, TestSize.Level0)
{
    ItJffs004();
}

/* *
 * @tc.name: IT_JFFS_007
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItJffs007, TestSize.Level0)
{
    ItJffs007();
}

/* *
 * @tc.name: IT_JFFS_009
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItJffs009, TestSize.Level0)
{
    ItJffs009();
}

/* *
 * @tc.name: IT_JFFS_010
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItJffs010, TestSize.Level0)
{
    ItJffs010();
}

/* *
 * @tc.name: IT_JFFS_011
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItJffs011, TestSize.Level0)
{
    ItJffs011();
}

/* *
 * @tc.name: IT_JFFS_012
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItJffs012, TestSize.Level0)
{
    ItJffs012();
}

/* *
 * @tc.name: IT_JFFS_013
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItJffs013, TestSize.Level0)
{
    ItJffs013();
}

/* *
 * @tc.name: IT_JFFS_015
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItJffs015, TestSize.Level0)
{
    ItJffs015();
}

/* *
 * @tc.name: IT_JFFS_017
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItJffs017, TestSize.Level0)
{
    ItJffs017();
}

/* *
 * @tc.name: IT_JFFS_018
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItJffs018, TestSize.Level0)
{
    ItJffs018();
}

/* *
 * @tc.name: IT_JFFS_019
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItJffs019, TestSize.Level0)
{
    ItJffs019();
}

/* *
 * @tc.name: IT_JFFS_022
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItJffs022, TestSize.Level0)
{
    ItJffs022();
}

/* *
 * @tc.name: IT_JFFS_025
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItJffs025, TestSize.Level0)
{
    ItJffs025();
}

/* *
 * @tc.name: IT_JFFS_026
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItJffs026, TestSize.Level0)
{
    ItJffs026();
}

/* *
 * @tc.name: IT_JFFS_027
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItJffs027, TestSize.Level0)
{
    ItJffs027();
}

/* *
 * @tc.name: IT_JFFS_030
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItJffs030, TestSize.Level0)
{
    ItJffs030();
}

/* *
 * @tc.name: IT_JFFS_032
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItJffs032, TestSize.Level0)
{
    ItJffs032();
}

/* *
 * @tc.name: IT_JFFS_033
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItJffs033, TestSize.Level0)
{
    ItJffs033();
}

/* *
 * @tc.name: IT_JFFS_034
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItJffs034, TestSize.Level0)
{
    ItJffs034();
}

/* *
 * @tc.name: IT_JFFS_035
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItJffs035, TestSize.Level0)
{
    ItJffs035();
}

/* *
 * @tc.name: IT_JFFS_036
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItJffs036, TestSize.Level0)
{
    ItJffs036();
}

/* *
 * @tc.name: IT_JFFS_037
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItJffs037, TestSize.Level0)
{
    ItJffs037();
}

/* *
 * @tc.name: IT_JFFS_038
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItJffs038, TestSize.Level0)
{
    ItJffs038();
}

/* *
 * @tc.name: IT_JFFS_040
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItJffs040, TestSize.Level0)
{
    ItJffs040();
}

/* *
 * @tc.name: IT_JFFS_041
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItJffs041, TestSize.Level0)
{
    ItJffs041();
}

/* *
 * @tc.name: IT_JFFS_042
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItJffs042, TestSize.Level0)
{
    ItJffs042();
}

/* *
 * @tc.name: IT_JFFS_021
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItJffs021, TestSize.Level0)
{
    ItJffs021();
}

/* *
 * @tc.name: IT_JFFS_043
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItJffs043, TestSize.Level0)
{
    ItJffs043();
}

/* *
 * @tc.name: IT_JFFS_044
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItJffs044, TestSize.Level0)
{
    ItJffs044();
}


/* *
 * @tc.name: ItFsJffs004
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs004, TestSize.Level0)
{
    ItFsJffs004();
}

/* *
 * @tc.name: ItFsJffs006
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs006, TestSize.Level0)
{
    ItFsJffs006();
}

/* *
 * @tc.name: ItFsJffs008
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs008, TestSize.Level0)
{
    ItFsJffs008();
}

/* *
 * @tc.name: ItFsJffs009
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs009, TestSize.Level0)
{
    ItFsJffs009();
}

/* *
 * @tc.name: ItFsJffs011
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs011, TestSize.Level0)
{
    ItFsJffs011();
}

/* *
 * @tc.name: ItFsJffs012
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs012, TestSize.Level0)
{
    ItFsJffs012();
}

/* *
 * @tc.name: ItFsJffs013
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs013, TestSize.Level0)
{
    ItFsJffs013();
}

/* *
 * @tc.name: ItFsJffs014
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs014, TestSize.Level0)
{
    ItFsJffs014();
}

/* *
 * @tc.name: ItFsJffs015
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs015, TestSize.Level0)
{
    ItFsJffs015();
}

/* *
 * @tc.name: ItFsJffs016
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs016, TestSize.Level0)
{
    ItFsJffs016();
}

/* *
 * @tc.name: ItFsJffs017
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs017, TestSize.Level0)
{
    ItFsJffs017();
}

/* *
 * @tc.name: ItFsJffs018
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs018, TestSize.Level0)
{
    ItFsJffs018();
}

/* *
 * @tc.name: ItFsJffs019
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs019, TestSize.Level0)
{
    ItFsJffs019();
}

/* *
 * @tc.name: ItFsJffs020
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs020, TestSize.Level0)
{
    ItFsJffs020();
}

/* *
 * @tc.name: ItFsJffs023
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs023, TestSize.Level0)
{
    ItFsJffs023();
}

/* *
 * @tc.name: ItFsJffs024
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs024, TestSize.Level0)
{
    ItFsJffs024();
}

/* *
 * @tc.name: ItFsJffs025
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs025, TestSize.Level0)
{
    ItFsJffs025();
}

/* *
 * @tc.name: ItFsJffs029
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs029, TestSize.Level0)
{
    ItFsJffs029();
}

/* *
 * @tc.name: ItFsJffs030
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs030, TestSize.Level0)
{
    ItFsJffs030();
}

/* *
 * @tc.name: ItFsJffs031
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs031, TestSize.Level0)
{
    ItFsJffs031();
}

/* *
 * @tc.name: ItFsJffs032
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs032, TestSize.Level0)
{
    ItFsJffs032();
}

/* *
 * @tc.name: ItFsJffs033
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs033, TestSize.Level0)
{
    ItFsJffs033();
}

/* *
 * @tc.name: ItFsJffs037
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs037, TestSize.Level0)
{
    ItFsJffs037();
}

/* *
 * @tc.name: ItFsJffs038
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs038, TestSize.Level0)
{
    ItFsJffs038();
}

/* *
 * @tc.name: ItFsJffs039
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs039, TestSize.Level0)
{
    ItFsJffs039();
}

/* *
 * @tc.name: ItFsJffs040
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs040, TestSize.Level0)
{
    ItFsJffs040();
}

/* *
 * @tc.name: ItFsJffs041
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs041, TestSize.Level0)
{
    ItFsJffs041();
}

/* *
 * @tc.name: ItFsJffs042
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs042, TestSize.Level0)
{
    ItFsJffs042();
}

/* *
 * @tc.name: ItFsJffs043
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs043, TestSize.Level0)
{
    ItFsJffs043();
}

/* *
 * @tc.name: ItFsJffs044
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs044, TestSize.Level0)
{
    ItFsJffs044();
}

/* *
 * @tc.name: ItFsJffs045
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs045, TestSize.Level0)
{
    ItFsJffs045();
}

/* *
 * @tc.name: ItFsJffs046
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs046, TestSize.Level0)
{
    ItFsJffs046();
}

/* *
 * @tc.name: ItFsJffs048
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs048, TestSize.Level0)
{
    ItFsJffs048();
}

/* *
 * @tc.name: ItFsJffs049
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs049, TestSize.Level0)
{
    ItFsJffs049();
}

/* *
 * @tc.name: ItFsJffs050
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs050, TestSize.Level0)
{
    ItFsJffs050();
}

/* *
 * @tc.name: ItFsJffs053
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs053, TestSize.Level0)
{
    ItFsJffs053();
}

/* *
 * @tc.name: ItFsJffs055
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs055, TestSize.Level0)
{
    ItFsJffs055();
}

/* *
 * @tc.name: ItFsJffs056
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs056, TestSize.Level0)
{
    ItFsJffs056();
}

/* *
 * @tc.name: ItFsJffs057
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs057, TestSize.Level0)
{
    ItFsJffs057();
}

/* *
 * @tc.name: ItFsJffs059
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs059, TestSize.Level0)
{
    ItFsJffs059();
}

/* *
 * @tc.name: ItFsJffs060
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs060, TestSize.Level0)
{
    ItFsJffs060();
}

/* *
 * @tc.name: ItFsJffs061
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs061, TestSize.Level0)
{
    ItFsJffs061();
}

/* *
 * @tc.name: ItFsJffs062
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs062, TestSize.Level0)
{
    ItFsJffs062();
}

/* *
 * @tc.name: ItFsJffs063
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs063, TestSize.Level0)
{
    ItFsJffs063();
}

/* *
 * @tc.name: ItFsJffs064
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs064, TestSize.Level0)
{
    ItFsJffs064();
}

/* *
 * @tc.name: ItFsJffs066
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs066, TestSize.Level0)
{
    ItFsJffs066();
}

/* *
 * @tc.name: ItFsJffs068
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs068, TestSize.Level0)
{
    ItFsJffs068();
}

/* *
 * @tc.name: ItFsJffs069
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs069, TestSize.Level0)
{
    ItFsJffs069();
}

/* *
 * @tc.name: ItFsJffs070
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs070, TestSize.Level0)
{
    ItFsJffs070();
}

/* *
 * @tc.name: ItFsJffs077
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs077, TestSize.Level0)
{
    ItFsJffs077();
}

/* *
 * @tc.name: ItFsJffs078
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs078, TestSize.Level0)
{
    ItFsJffs078();
}

/* *
 * @tc.name: ItFsJffs079
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs079, TestSize.Level0)
{
    ItFsJffs079();
}

/* *
 * @tc.name: ItFsJffs081
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs081, TestSize.Level0)
{
    ItFsJffs081();
}

/* *
 * @tc.name: ItFsJffs082
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs082, TestSize.Level0)
{
    ItFsJffs082();
}

/* *
 * @tc.name: ItFsJffs083
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs083, TestSize.Level0)
{
    ItFsJffs083();
}

/* *
 * @tc.name: ItFsJffs084
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs084, TestSize.Level0)
{
    ItFsJffs084();
}

/* *
 * @tc.name: ItFsJffs085
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs085, TestSize.Level0)
{
    ItFsJffs085();
}

/* *
 * @tc.name: ItFsJffs088
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs088, TestSize.Level0)
{
    ItFsJffs088();
}

/* *
 * @tc.name: ItFsJffs090
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs090, TestSize.Level0)
{
    ItFsJffs090();
}

/* *
 * @tc.name: ItFsJffs093
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs093, TestSize.Level0)
{
    ItFsJffs093();
}

/* *
 * @tc.name: ItFsJffs096
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs096, TestSize.Level0)
{
    ItFsJffs096();
}

/* *
 * @tc.name: ItFsJffs099
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs099, TestSize.Level0)
{
    ItFsJffs099();
}

/* *
 * @tc.name: ItFsJffs100
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs100, TestSize.Level0)
{
    ItFsJffs100();
}

/* *
 * @tc.name: ItFsJffs104
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs104, TestSize.Level0)
{
    ItFsJffs104();
}

/* *
 * @tc.name: ItFsJffs116
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs116, TestSize.Level0)
{
    ItFsJffs116();
}

/* *
 * @tc.name: ItFsJffs117
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs117, TestSize.Level0)
{
    ItFsJffs117();
}

/* *
 * @tc.name: ItFsJffs118
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs118, TestSize.Level0)
{
    ItFsJffs118();
}

/* *
 * @tc.name: ItFsJffs119
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs119, TestSize.Level0)
{
    ItFsJffs119();
}

/* *
 * @tc.name: ItFsJffs120
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs120, TestSize.Level0)
{
    ItFsJffs120();
}

/* *
 * @tc.name: ItFsJffs121
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs121, TestSize.Level0)
{
    ItFsJffs121();
}

/* *
 * @tc.name: ItFsJffs123
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs123, TestSize.Level0)
{
    ItFsJffs123();
}

/* *
 * @tc.name: ItFsJffs126
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs126, TestSize.Level0)
{
    ItFsJffs126();
}

/* *
 * @tc.name: ItFsJffs127
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs127, TestSize.Level0)
{
    ItFsJffs127();
}

/* *
 * @tc.name: ItFsJffs128
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs128, TestSize.Level0)
{
    ItFsJffs128();
}

/* *
 * @tc.name: ItFsJffs129
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs129, TestSize.Level0)
{
    ItFsJffs129();
}

/* *
 * @tc.name: ItFsJffs130
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs130, TestSize.Level0)
{
    ItFsJffs130();
}

/* *
 * @tc.name: ItFsJffs131
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs131, TestSize.Level0)
{
    ItFsJffs131();
}

/* *
 * @tc.name: ItFsJffs132
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs132, TestSize.Level0)
{
    ItFsJffs132();
}

/* *
 * @tc.name: ItFsJffs133
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs133, TestSize.Level0)
{
    ItFsJffs133();
}

/* *
 * @tc.name: ItFsJffs134
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs134, TestSize.Level0)
{
    ItFsJffs134();
}

/* *
 * @tc.name: ItFsJffs135
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs135, TestSize.Level0)
{
    ItFsJffs135();
}

/* *
 * @tc.name: ItFsJffs136
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs136, TestSize.Level0)
{
    ItFsJffs136();
}

/* *
 * @tc.name: ItFsJffs137
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs137, TestSize.Level0)
{
    ItFsJffs137();
}

/* *
 * @tc.name: ItFsJffs138
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs138, TestSize.Level0)
{
    ItFsJffs138();
}

/* *
 * @tc.name: ItFsJffs139
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs139, TestSize.Level0)
{
    ItFsJffs139();
}

/* *
 * @tc.name: ItFsJffs140
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs140, TestSize.Level0)
{
    ItFsJffs140();
}

/* *
 * @tc.name: ItFsJffs141
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs141, TestSize.Level0)
{
    ItFsJffs141();
}

/* *
 * @tc.name: ItFsJffs142
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs142, TestSize.Level0)
{
    ItFsJffs142();
}

/* *
 * @tc.name: ItFsJffs143
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs143, TestSize.Level0)
{
    ItFsJffs143();
}

/* *
 * @tc.name: ItFsJffs144
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs144, TestSize.Level0)
{
    ItFsJffs144();
}

/* *
 * @tc.name: ItFsJffs145
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs145, TestSize.Level0)
{
    ItFsJffs145();
}

/* *
 * @tc.name: ItFsJffs146
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs146, TestSize.Level0)
{
    ItFsJffs146();
}

/* *
 * @tc.name: ItFsJffs147
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs147, TestSize.Level0)
{
    ItFsJffs147();
}

/* *
 * @tc.name: ItFsJffs148
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs148, TestSize.Level0)
{
    ItFsJffs148();
}

/* *
 * @tc.name: ItFsJffs149
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs149, TestSize.Level0)
{
    ItFsJffs149();
}

/* *
 * @tc.name: ItFsJffs150
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs150, TestSize.Level0)
{
    ItFsJffs150();
}

/* *
 * @tc.name: ItFsJffs151
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs151, TestSize.Level0)
{
    ItFsJffs151();
}

/* *
 * @tc.name: ItFsJffs152
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs152, TestSize.Level0)
{
    ItFsJffs152();
}

/* *
 * @tc.name: ItFsJffs153
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs153, TestSize.Level0)
{
    ItFsJffs153();
}

/* *
 * @tc.name: ItFsJffs154
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs154, TestSize.Level0)
{
    ItFsJffs154();
}

/* *
 * @tc.name: ItFsJffs155
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs155, TestSize.Level0)
{
    ItFsJffs155();
}

/* *
 * @tc.name: ItFsJffs156
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs156, TestSize.Level0)
{
    ItFsJffs156();
}

/* *
 * @tc.name: ItFsJffs157
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs157, TestSize.Level0)
{
    ItFsJffs157();
}

/* *
 * @tc.name: ItFsJffs158
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs158, TestSize.Level0)
{
    ItFsJffs158();
}

/* *
 * @tc.name: ItFsJffs159
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs159, TestSize.Level0)
{
    ItFsJffs159();
}

/* *
 * @tc.name: ItFsJffs160
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs160, TestSize.Level0)
{
    ItFsJffs160();
}

/* *
 * @tc.name: ItFsJffs161
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs161, TestSize.Level0)
{
    ItFsJffs161();
}

/* *
 * @tc.name: ItFsJffs162
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs162, TestSize.Level0)
{
    ItFsJffs162();
}

/* *
 * @tc.name: ItFsJffs163
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs163, TestSize.Level0)
{
    ItFsJffs163();
}

/* *
 * @tc.name: ItFsJffs164
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs164, TestSize.Level0)
{
    ItFsJffs164();
}

/* *
 * @tc.name: ItFsJffs165
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs165, TestSize.Level0)
{
    ItFsJffs165();
}

/* *
 * @tc.name: ItFsJffs166
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs166, TestSize.Level0)
{
    ItFsJffs166();
}

/* *
 * @tc.name: ItFsJffs167
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs167, TestSize.Level0)
{
    ItFsJffs167();
}

/* *
 * @tc.name: ItFsJffs168
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs168, TestSize.Level0)
{
    ItFsJffs168();
}

/* *
 * @tc.name: ItFsJffs169
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs169, TestSize.Level0)
{
    ItFsJffs169();
}

/* *
 * @tc.name: ItFsJffs170
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs170, TestSize.Level0)
{
    ItFsJffs170();
}

/* *
 * @tc.name: ItFsJffs171
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs171, TestSize.Level0)
{
    ItFsJffs171();
}

/* *
 * @tc.name: ItFsJffs172
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs172, TestSize.Level0)
{
    ItFsJffs172();
}

/* *
 * @tc.name: ItFsJffs173
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs173, TestSize.Level0)
{
    ItFsJffs173();
}

/* *
 * @tc.name: ItFsJffs175
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs175, TestSize.Level0)
{
    ItFsJffs175();
}

/* *
 * @tc.name: ItFsJffs176
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs176, TestSize.Level0)
{
    ItFsJffs176();
}

/* *
 * @tc.name: ItFsJffs177
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs177, TestSize.Level0)
{
    ItFsJffs177();
}

/* *
 * @tc.name: ItFsJffs178
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs178, TestSize.Level0)
{
    ItFsJffs178();
}

/* *
 * @tc.name: ItFsJffs179
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs179, TestSize.Level0)
{
    ItFsJffs179();
}

/* *
 * @tc.name: ItFsJffs180
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs180, TestSize.Level0)
{
    ItFsJffs180();
}

/* *
 * @tc.name: ItFsJffs182
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs182, TestSize.Level0)
{
    ItFsJffs182();
}

/* *
 * @tc.name: ItFsJffs183
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs183, TestSize.Level0)
{
    ItFsJffs183();
}

/* *
 * @tc.name: ItFsJffs184
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs184, TestSize.Level0)
{
    ItFsJffs184();
}

/* *
 * @tc.name: ItFsJffs185
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs185, TestSize.Level0)
{
    ItFsJffs185();
}

/* *
 * @tc.name: ItFsJffs187
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs187, TestSize.Level0)
{
    ItFsJffs187();
}

/* *
 * @tc.name: ItFsJffs188
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs188, TestSize.Level0)
{
    ItFsJffs188();
}

/* *
 * @tc.name: ItFsJffs189
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs189, TestSize.Level0)
{
    ItFsJffs189();
}

/* *
 * @tc.name: ItFsJffs190
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs190, TestSize.Level0)
{
    ItFsJffs190();
}

/* *
 * @tc.name: ItFsJffs191
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs191, TestSize.Level0)
{
    ItFsJffs191();
}

/* *
 * @tc.name: ItFsJffs192
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs192, TestSize.Level0)
{
    ItFsJffs192();
}

/* *
 * @tc.name: ItFsJffs193
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs193, TestSize.Level0)
{
    ItFsJffs193();
}

/* *
 * @tc.name: ItFsJffs194
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs194, TestSize.Level0)
{
    ItFsJffs194();
}

/* *
 * @tc.name: ItFsJffs195
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs195, TestSize.Level0)
{
    ItFsJffs195();
}

/* *
 * @tc.name: ItFsJffs196
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs196, TestSize.Level0)
{
    ItFsJffs196();
}

/* *
 * @tc.name: ItFsJffs197
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs197, TestSize.Level0)
{
    ItFsJffs197();
}

/* *
 * @tc.name: ItFsJffs198
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs198, TestSize.Level0)
{
    ItFsJffs198();
}

/* *
 * @tc.name: ItFsJffs199
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs199, TestSize.Level0)
{
    ItFsJffs199();
}

/* *
 * @tc.name: ItFsJffs200
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs200, TestSize.Level0)
{
    ItFsJffs200();
}

/* *
 * @tc.name: ItFsJffs202
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs202, TestSize.Level0)
{
    ItFsJffs202();
}

/* *
 * @tc.name: ItFsJffs203
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs203, TestSize.Level0)
{
    ItFsJffs203();
}

/* *
 * @tc.name: ItFsJffs204
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs204, TestSize.Level0)
{
    ItFsJffs204();
}

/* *
 * @tc.name: ItFsJffs205
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs205, TestSize.Level0)
{
    ItFsJffs205();
}

/* *
 * @tc.name: ItFsJffs206
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs206, TestSize.Level0)
{
    ItFsJffs206();
}

/* *
 * @tc.name: ItFsJffs207
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs207, TestSize.Level0)
{
    ItFsJffs207();
}

/* *
 * @tc.name: ItFsJffs208
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs208, TestSize.Level0)
{
    ItFsJffs208();
}

/* *
 * @tc.name: ItFsJffs209
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs209, TestSize.Level0)
{
    ItFsJffs209();
}

/* *
 * @tc.name: ItFsJffs210
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs210, TestSize.Level0)
{
    ItFsJffs210();
}

/* *
 * @tc.name: ItFsJffs211
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs211, TestSize.Level0)
{
    ItFsJffs211();
}

/* *
 * @tc.name: ItFsJffs212
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs212, TestSize.Level0)
{
    ItFsJffs212();
}

/* *
 * @tc.name: ItFsJffs213
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs213, TestSize.Level0)
{
    ItFsJffs213();
}

/* *
 * @tc.name: ItFsJffs214
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs214, TestSize.Level0)
{
    ItFsJffs214();
}

/* *
 * @tc.name: ItFsJffs215
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs215, TestSize.Level0)
{
    ItFsJffs215();
}

/* *
 * @tc.name: ItFsJffs216
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs216, TestSize.Level0)
{
    ItFsJffs216();
}

/* *
 * @tc.name: ItFsJffs217
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs217, TestSize.Level0)
{
    ItFsJffs217();
}

/* *
 * @tc.name: ItFsJffs218
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs218, TestSize.Level0)
{
    ItFsJffs218();
}

/* *
 * @tc.name: ItFsJffs219
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs219, TestSize.Level0)
{
    ItFsJffs219();
}

/* *
 * @tc.name: ItFsJffs220
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs220, TestSize.Level0)
{
    ItFsJffs220();
}

/* *
 * @tc.name: ItFsJffs221
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs221, TestSize.Level0)
{
    ItFsJffs221();
}

/* *
 * @tc.name: ItFsJffs222
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs222, TestSize.Level0)
{
    ItFsJffs222();
}

/* *
 * @tc.name: ItFsJffs223
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs223, TestSize.Level0)
{
    ItFsJffs223();
}

/* *
 * @tc.name: ItFsJffs224
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs224, TestSize.Level0)
{
    ItFsJffs224();
}

/* *
 * @tc.name: ItFsJffs225
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs225, TestSize.Level0)
{
    ItFsJffs225();
}

/* *
 * @tc.name: ItFsJffs226
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs226, TestSize.Level0)
{
    ItFsJffs226();
}

/* *
 * @tc.name: ItFsJffs227
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs227, TestSize.Level0)
{
    ItFsJffs227();
}

/* *
 * @tc.name: ItFsJffs228
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs228, TestSize.Level0)
{
    ItFsJffs228();
}

/* *
 * @tc.name: ItFsJffs229
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs229, TestSize.Level0)
{
    ItFsJffs229();
}

/* *
 * @tc.name: ItFsJffs230
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs230, TestSize.Level0)
{
    ItFsJffs230();
}

/* *
 * @tc.name: ItFsJffs231
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs231, TestSize.Level0)
{
    ItFsJffs231();
}

/* *
 * @tc.name: ItFsJffs232
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs232, TestSize.Level0)
{
    ItFsJffs232();
}

/* *
 * @tc.name: ItFsJffs233
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs233, TestSize.Level0)
{
    ItFsJffs233();
}

/* *
 * @tc.name: ItFsJffs234
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs234, TestSize.Level0)
{
    ItFsJffs234();
}

/* *
 * @tc.name: ItFsJffs235
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs235, TestSize.Level0)
{
    ItFsJffs235();
}

/* *
 * @tc.name: ItFsJffs236
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs236, TestSize.Level0)
{
    ItFsJffs236();
}

/* *
 * @tc.name: ItFsJffs237
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs237, TestSize.Level0)
{
    ItFsJffs237();
}

/* *
 * @tc.name: ItFsJffs238
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs238, TestSize.Level0)
{
    ItFsJffs238();
}

/* *
 * @tc.name: ItFsJffs239
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs239, TestSize.Level0)
{
    ItFsJffs239();
}

/* *
 * @tc.name: ItFsJffs240
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs240, TestSize.Level0)
{
    ItFsJffs240();
}

/* *
 * @tc.name: ItFsJffs241
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs241, TestSize.Level0)
{
    ItFsJffs241();
}

/* *
 * @tc.name: ItFsJffs242
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs242, TestSize.Level0)
{
    ItFsJffs242();
}

/* *
 * @tc.name: ItFsJffs243
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs243, TestSize.Level0)
{
    ItFsJffs243();
}

/* *
 * @tc.name: ItFsJffs244
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs244, TestSize.Level0)
{
    ItFsJffs244();
}

/* *
 * @tc.name: ItFsJffs245
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs245, TestSize.Level0)
{
    ItFsJffs245();
}

/* *
 * @tc.name: ItFsJffs246
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs246, TestSize.Level0)
{
    ItFsJffs246();
}

/* *
 * @tc.name: ItFsJffs247
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs247, TestSize.Level0)
{
    ItFsJffs247();
}

/* *
 * @tc.name: ItFsJffs248
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs248, TestSize.Level0)
{
    ItFsJffs248();
}

/* *
 * @tc.name: ItFsJffs249
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs249, TestSize.Level0)
{
    ItFsJffs249();
}

/* *
 * @tc.name: ItFsJffs250
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs250, TestSize.Level0)
{
    ItFsJffs250();
}

/* *
 * @tc.name: ItFsJffs251
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs251, TestSize.Level0)
{
    ItFsJffs251();
}

/* *
 * @tc.name: ItFsJffs252
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs252, TestSize.Level0)
{
    ItFsJffs252();
}

/* *
 * @tc.name: ItFsJffs253
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs253, TestSize.Level0)
{
    ItFsJffs253();
}

/* *
 * @tc.name: ItFsJffs254
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs254, TestSize.Level0)
{
    ItFsJffs254();
}

/* *
 * @tc.name: ItFsJffs255
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs255, TestSize.Level0)
{
    ItFsJffs255();
}

/* *
 * @tc.name: ItFsJffs256
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs256, TestSize.Level0)
{
    ItFsJffs256();
}

/* *
 * @tc.name: ItFsJffs257
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs257, TestSize.Level0)
{
    ItFsJffs257();
}

/* *
 * @tc.name: ItFsJffs258
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs258, TestSize.Level0)
{
    ItFsJffs258();
}

/* *
 * @tc.name: ItFsJffs259
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs259, TestSize.Level0)
{
    ItFsJffs259();
}

/* *
 * @tc.name: ItFsJffs260
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs260, TestSize.Level0)
{
    ItFsJffs260();
}

/* *
 * @tc.name: ItFsJffs261
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs261, TestSize.Level0)
{
    ItFsJffs261();
}

/* *
 * @tc.name: ItFsJffs262
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs262, TestSize.Level0)
{
    ItFsJffs262();
}

/* *
 * @tc.name: ItFsJffs263
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs263, TestSize.Level0)
{
    ItFsJffs263();
}

/* *
 * @tc.name: ItFsJffs264
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs264, TestSize.Level0)
{
    ItFsJffs264();
}

/* *
 * @tc.name: ItFsJffs265
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs265, TestSize.Level0)
{
    ItFsJffs265();
}

/* *
 * @tc.name: ItFsJffs266
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs266, TestSize.Level0)
{
    ItFsJffs266();
}

/* *
 * @tc.name: ItFsJffs267
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs267, TestSize.Level0)
{
    ItFsJffs267();
}

/* *
 * @tc.name: ItFsJffs268
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs268, TestSize.Level0)
{
    ItFsJffs268();
}

/* *
 * @tc.name: ItFsJffs269
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs269, TestSize.Level0)
{
    ItFsJffs269();
}

/* *
 * @tc.name: ItFsJffs270
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs270, TestSize.Level0)
{
    ItFsJffs270();
}

/* *
 * @tc.name: ItFsJffs271
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs271, TestSize.Level0)
{
    ItFsJffs271();
}

/* *
 * @tc.name: ItFsJffs272
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs272, TestSize.Level0)
{
    ItFsJffs272();
}

/* *
 * @tc.name: ItFsJffs273
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs273, TestSize.Level0)
{
    ItFsJffs273();
}

/* *
 * @tc.name: ItFsJffs274
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs274, TestSize.Level0)
{
    ItFsJffs274();
}

/* *
 * @tc.name: ItFsJffs275
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs275, TestSize.Level0)
{
    ItFsJffs275();
}

/* *
 * @tc.name: ItFsJffs276
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs276, TestSize.Level0)
{
    ItFsJffs276();
}

/* *
 * @tc.name: ItFsJffs277
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs277, TestSize.Level0)
{
    ItFsJffs277();
}

/* *
 * @tc.name: ItFsJffs278
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs278, TestSize.Level0)
{
    ItFsJffs278();
}

/* *
 * @tc.name: ItFsJffs279
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs279, TestSize.Level0)
{
    ItFsJffs279();
}

/* *
 * @tc.name: ItFsJffs280
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs280, TestSize.Level0)
{
    ItFsJffs280();
}

/* *
 * @tc.name: ItFsJffs281
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs281, TestSize.Level0)
{
    ItFsJffs281();
}

/* *
 * @tc.name: ItFsJffs282
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs282, TestSize.Level0)
{
    ItFsJffs282();
}

/* *
 * @tc.name: ItFsJffs283
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs283, TestSize.Level0)
{
    ItFsJffs283();
}

/* *
 * @tc.name: ItFsJffs284
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs284, TestSize.Level0)
{
    ItFsJffs284();
}

/* *
 * @tc.name: ItFsJffs285
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs285, TestSize.Level0)
{
    ItFsJffs285();
}

/* *
 * @tc.name: ItFsJffs288
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs288, TestSize.Level0)
{
    ItFsJffs288();
}

/* *
 * @tc.name: ItFsJffs289
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs289, TestSize.Level0)
{
    ItFsJffs289();
}

/* *
 * @tc.name: ItFsJffs290
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs290, TestSize.Level0)
{
    ItFsJffs290();
}

/* *
 * @tc.name: ItFsJffs291
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs291, TestSize.Level0)
{
    ItFsJffs291();
}

/* *
 * @tc.name: ItFsJffs292
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs292, TestSize.Level0)
{
    ItFsJffs292();
}

/* *
 * @tc.name: ItFsJffs293
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs293, TestSize.Level0)
{
    ItFsJffs293();
}

/* *
 * @tc.name: ItFsJffs294
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs294, TestSize.Level0)
{
    ItFsJffs294();
}

/* *
 * @tc.name: ItFsJffs295
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs295, TestSize.Level0)
{
    ItFsJffs295();
}

/* *
 * @tc.name: ItFsJffs296
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs296, TestSize.Level0)
{
    ItFsJffs296();
}

/* *
 * @tc.name: ItFsJffs297
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs297, TestSize.Level0)
{
    ItFsJffs297();
}

/* *
 * @tc.name: ItFsJffs298
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs298, TestSize.Level0)
{
    ItFsJffs298();
}

/* *
 * @tc.name: ItFsJffs299
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs299, TestSize.Level0)
{
    ItFsJffs299();
}

/* *
 * @tc.name: ItFsJffs300
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs300, TestSize.Level0)
{
    ItFsJffs300();
}

/* *
 * @tc.name: ItFsJffs301
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs301, TestSize.Level0)
{
    ItFsJffs301();
}

/* *
 * @tc.name: ItFsJffs302
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs302, TestSize.Level0)
{
    ItFsJffs302();
}

/* *
 * @tc.name: ItFsJffs303
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs303, TestSize.Level0)
{
    ItFsJffs303();
}

/* *
 * @tc.name: ItFsJffs304
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs304, TestSize.Level0)
{
    ItFsJffs304();
}

/* *
 * @tc.name: ItFsJffs305
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs305, TestSize.Level0)
{
    ItFsJffs305();
}

/* *
 * @tc.name: ItFsJffs306
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs306, TestSize.Level0)
{
    ItFsJffs306();
}

/* *
 * @tc.name: ItFsJffs307
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs307, TestSize.Level0)
{
    ItFsJffs307();
}

/* *
 * @tc.name: ItFsJffs308
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs308, TestSize.Level0)
{
    ItFsJffs308();
}

/* *
 * @tc.name: ItFsJffs309
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs309, TestSize.Level0)
{
    ItFsJffs309();
}

/* *
 * @tc.name: ItFsJffs310
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs310, TestSize.Level0)
{
    ItFsJffs310();
}

/* *
 * @tc.name: ItFsJffs311
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs311, TestSize.Level0)
{
    ItFsJffs311();
}

/* *
 * @tc.name: ItFsJffs312
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs312, TestSize.Level0)
{
    ItFsJffs312();
}

/* *
 * @tc.name: ItFsJffs313
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs313, TestSize.Level0)
{
    ItFsJffs313();
}

/* *
 * @tc.name: ItFsJffs314
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs314, TestSize.Level0)
{
    ItFsJffs314();
}

/* *
 * @tc.name: ItFsJffs315
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs315, TestSize.Level0)
{
    ItFsJffs315();
}

/* *
 * @tc.name: ItFsJffs316
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs316, TestSize.Level0)
{
    ItFsJffs316();
}

/* *
 * @tc.name: ItFsJffs317
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs317, TestSize.Level0)
{
    ItFsJffs317();
}

/* *
 * @tc.name: ItFsJffs318
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs318, TestSize.Level0)
{
    ItFsJffs318();
}

/* *
 * @tc.name: ItFsJffs319
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs319, TestSize.Level0)
{
    ItFsJffs319();
}

/* *
 * @tc.name: ItFsJffs320
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs320, TestSize.Level0)
{
    ItFsJffs320();
}

/* *
 * @tc.name: ItFsJffs321
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs321, TestSize.Level0)
{
    ItFsJffs321();
}

/* *
 * @tc.name: ItFsJffs322
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs322, TestSize.Level0)
{
    ItFsJffs322();
}

/* *
 * @tc.name: ItFsJffs323
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs323, TestSize.Level0)
{
    ItFsJffs323();
}

/* *
 * @tc.name: ItFsJffs324
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs324, TestSize.Level0)
{
    ItFsJffs324();
}

/* *
 * @tc.name: ItFsJffs325
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs325, TestSize.Level0)
{
    ItFsJffs325();
}

/* *
 * @tc.name: ItFsJffs326
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs326, TestSize.Level0)
{
    ItFsJffs326();
}

/* *
 * @tc.name: ItFsJffs327
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs327, TestSize.Level0)
{
    ItFsJffs327();
}

/* *
 * @tc.name: ItFsJffs328
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs328, TestSize.Level0)
{
    ItFsJffs328();
}

/* *
 * @tc.name: ItFsJffs329
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs329, TestSize.Level0)
{
    ItFsJffs329();
}

/* *
 * @tc.name: ItFsJffs330
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs330, TestSize.Level0)
{
    ItFsJffs330();
}

/* *
 * @tc.name: ItFsJffs331
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs331, TestSize.Level0)
{
    ItFsJffs331();
}

/* *
 * @tc.name: ItFsJffs332
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs332, TestSize.Level0)
{
    ItFsJffs332();
}

/* *
 * @tc.name: ItFsJffs333
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs333, TestSize.Level0)
{
    ItFsJffs333();
}

/* *
 * @tc.name: ItFsJffs334
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs334, TestSize.Level0)
{
    ItFsJffs334();
}

/* *
 * @tc.name: ItFsJffs335
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs335, TestSize.Level0)
{
    ItFsJffs335();
}

/* *
 * @tc.name: ItFsJffs336
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs336, TestSize.Level0)
{
    ItFsJffs336();
}

/* *
 * @tc.name: ItFsJffs337
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs337, TestSize.Level0)
{
    ItFsJffs337();
}

/* *
 * @tc.name: ItFsJffs338
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs338, TestSize.Level0)
{
    ItFsJffs338();
}

/* *
 * @tc.name: ItFsJffs339
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs339, TestSize.Level0)
{
    ItFsJffs339();
}

/* *
 * @tc.name: ItFsJffs340
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs340, TestSize.Level0)
{
    ItFsJffs340();
}

/* *
 * @tc.name: ItFsJffs342
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs342, TestSize.Level0)
{
    ItFsJffs342();
}

/* *
 * @tc.name: ItFsJffs343
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs343, TestSize.Level0)
{
    ItFsJffs343();
}

/* *
 * @tc.name: ItFsJffs344
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs344, TestSize.Level0)
{
    ItFsJffs344();
}

/* *
 * @tc.name: ItFsJffs346
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs346, TestSize.Level0)
{
    ItFsJffs346();
}

/* *
 * @tc.name: ItFsJffs352
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs352, TestSize.Level0)
{
    ItFsJffs352();
}

/* *
 * @tc.name: ItFsJffs353
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs353, TestSize.Level0)
{
    ItFsJffs353();
}

/* *
 * @tc.name: ItFsJffs354
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs354, TestSize.Level0)
{
    ItFsJffs354();
}

/* *
 * @tc.name: ItFsJffs355
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs355, TestSize.Level0)
{
    ItFsJffs355();
}

/* *
 * @tc.name: ItFsJffs356
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs356, TestSize.Level0)
{
    ItFsJffs356();
}

/* *
 * @tc.name: ItFsJffs357
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs357, TestSize.Level0)
{
    ItFsJffs357();
}

/* *
 * @tc.name: ItFsJffs358
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs358, TestSize.Level0)
{
    ItFsJffs358();
}

/* *
 * @tc.name: ItFsJffs359
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs359, TestSize.Level0)
{
    ItFsJffs359();
}

/* *
 * @tc.name: ItFsJffs360
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs360, TestSize.Level0)
{
    ItFsJffs360();
}

/* *
 * @tc.name: ItFsJffs361
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs361, TestSize.Level0)
{
    ItFsJffs361();
}

/* *
 * @tc.name: ItFsJffs362
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs362, TestSize.Level0)
{
    ItFsJffs362();
}

/* *
 * @tc.name: ItFsJffs364
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs364, TestSize.Level0)
{
    ItFsJffs364();
}

/* *
 * @tc.name: ItFsJffs365
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs365, TestSize.Level0)
{
    ItFsJffs365();
}

/* *
 * @tc.name: ItFsJffs366
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs366, TestSize.Level0)
{
    ItFsJffs366();
}

/* *
 * @tc.name: ItFsJffs367
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs367, TestSize.Level0)
{
    ItFsJffs367();
}

/* *
 * @tc.name: ItFsJffs368
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs368, TestSize.Level0)
{
    ItFsJffs368();
}

/* *
 * @tc.name: ItFsJffs369
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs369, TestSize.Level0)
{
    ItFsJffs369();
}

/* *
 * @tc.name: ItFsJffs370
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs370, TestSize.Level0)
{
    ItFsJffs370();
}

/* *
 * @tc.name: ItFsJffs371
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs371, TestSize.Level0)
{
    ItFsJffs371();
}

/* *
 * @tc.name: ItFsJffs372
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs372, TestSize.Level0)
{
    ItFsJffs372();
}

/* *
 * @tc.name: ItFsJffs373
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs373, TestSize.Level0)
{
    ItFsJffs373();
}

/* *
 * @tc.name: ItFsJffs374
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs374, TestSize.Level0)
{
    ItFsJffs374();
}

/* *
 * @tc.name: ItFsJffs375
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs375, TestSize.Level0)
{
    ItFsJffs375();
}

/* *
 * @tc.name: ItFsJffs376
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs376, TestSize.Level0)
{
    ItFsJffs376();
}

/* *
 * @tc.name: ItFsJffs377
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs377, TestSize.Level0)
{
    ItFsJffs377();
}

/* *
 * @tc.name: ItFsJffs378
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs378, TestSize.Level0)
{
    ItFsJffs378();
}

/* *
 * @tc.name: ItFsJffs379
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs379, TestSize.Level0)
{
    ItFsJffs379();
}

/* *
 * @tc.name: ItFsJffs380
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs380, TestSize.Level0)
{
    ItFsJffs380();
}

/* *
 * @tc.name: ItFsJffs381
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs381, TestSize.Level0)
{
    ItFsJffs381();
}

/* *
 * @tc.name: ItFsJffs382
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs382, TestSize.Level0)
{
    ItFsJffs382();
}

/* *
 * @tc.name: ItFsJffs383
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs383, TestSize.Level0)
{
    ItFsJffs383();
}

/* *
 * @tc.name: ItFsJffs384
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs384, TestSize.Level0)
{
    ItFsJffs384();
}

/* *
 * @tc.name: ItFsJffs385
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs385, TestSize.Level0)
{
    ItFsJffs385();
}

/* *
 * @tc.name: ItFsJffs386
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs386, TestSize.Level0)
{
    ItFsJffs386();
}

/* *
 * @tc.name: ItFsJffs387
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs387, TestSize.Level0)
{
    ItFsJffs387();
}

/* *
 * @tc.name: ItFsJffs388
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs388, TestSize.Level0)
{
    ItFsJffs388();
}

/* *
 * @tc.name: ItFsJffs389
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs389, TestSize.Level0)
{
    ItFsJffs389();
}

/* *
 * @tc.name: ItFsJffs390
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs390, TestSize.Level0)
{
    ItFsJffs390();
}

/* *
 * @tc.name: ItFsJffs391
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs391, TestSize.Level0)
{
    ItFsJffs391();
}

/* *
 * @tc.name: ItFsJffs392
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs392, TestSize.Level0)
{
    ItFsJffs392();
}

/* *
 * @tc.name: ItFsJffs393
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs393, TestSize.Level0)
{
    ItFsJffs393();
}

/* *
 * @tc.name: ItFsJffs394
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs394, TestSize.Level0)
{
    ItFsJffs394();
}

/* *
 * @tc.name: ItFsJffs395
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs395, TestSize.Level0)
{
    ItFsJffs395();
}

/* *
 * @tc.name: ItFsJffs396
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs396, TestSize.Level0)
{
    ItFsJffs396();
}

/* *
 * @tc.name: ItFsJffs397
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs397, TestSize.Level0)
{
    ItFsJffs397();
}

/* *
 * @tc.name: ItFsJffs398
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs398, TestSize.Level0)
{
    ItFsJffs398();
}

/* *
 * @tc.name: ItFsJffs399
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs399, TestSize.Level0)
{
    ItFsJffs399();
}

/* *
 * @tc.name: ItFsJffs400
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs400, TestSize.Level0)
{
    ItFsJffs400();
}

/* *
 * @tc.name: ItFsJffs401
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs401, TestSize.Level0)
{
    ItFsJffs401();
}

/* *
 * @tc.name: ItFsJffs402
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs402, TestSize.Level0)
{
    ItFsJffs402();
}

/* *
 * @tc.name: ItFsJffs403
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs403, TestSize.Level0)
{
    ItFsJffs403();
}

/* *
 * @tc.name: ItFsJffs404
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs404, TestSize.Level0)
{
    ItFsJffs404();
}

/* *
 * @tc.name: ItFsJffs405
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs405, TestSize.Level0)
{
    ItFsJffs405();
}

/* *
 * @tc.name: ItFsJffs406
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs406, TestSize.Level0)
{
    ItFsJffs406();
}

/* *
 * @tc.name: ItFsJffs407
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs407, TestSize.Level0)
{
    ItFsJffs407();
}

/* *
 * @tc.name: ItFsJffs408
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs408, TestSize.Level0)
{
    ItFsJffs408();
}

/* *
 * @tc.name: ItFsJffs409
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs409, TestSize.Level0)
{
    ItFsJffs409();
}

/* *
 * @tc.name: ItFsJffs410
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs410, TestSize.Level0)
{
    ItFsJffs410();
}

/* *
 * @tc.name: ItFsJffs411
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs411, TestSize.Level0)
{
    ItFsJffs411();
}

/* *
 * @tc.name: ItFsJffs412
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs412, TestSize.Level0)
{
    ItFsJffs412();
}

/* *
 * @tc.name: ItFsJffs413
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs413, TestSize.Level0)
{
    ItFsJffs413();
}

/* *
 * @tc.name: ItFsJffs414
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs414, TestSize.Level0)
{
    ItFsJffs414();
}

/* *
 * @tc.name: ItFsJffs415
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs415, TestSize.Level0)
{
    ItFsJffs415();
}

/* *
 * @tc.name: ItFsJffs416
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs416, TestSize.Level0)
{
    ItFsJffs416();
}

/* *
 * @tc.name: ItFsJffs417
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs417, TestSize.Level0)
{
    ItFsJffs417();
}

/* *
 * @tc.name: ItFsJffs418
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs418, TestSize.Level0)
{
    ItFsJffs418();
}

/* *
 * @tc.name: ItFsJffs419
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs419, TestSize.Level0)
{
    ItFsJffs419();
}

/* *
 * @tc.name: ItFsJffs420
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs420, TestSize.Level0)
{
    ItFsJffs420();
}

/* *
 * @tc.name: ItFsJffs421
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs421, TestSize.Level0)
{
    ItFsJffs421();
}

/* *
 * @tc.name: ItFsJffs422
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs422, TestSize.Level0)
{
    ItFsJffs422();
}

/* *
 * @tc.name: ItFsJffs423
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs423, TestSize.Level0)
{
    ItFsJffs423();
}

/* *
 * @tc.name: ItFsJffs424
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs424, TestSize.Level0)
{
    ItFsJffs424();
}

/* *
 * @tc.name: ItFsJffs425
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs425, TestSize.Level0)
{
    ItFsJffs425();
}

/* *
 * @tc.name: ItFsJffs426
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs426, TestSize.Level0)
{
    ItFsJffs426();
}

/* *
 * @tc.name: ItFsJffs427
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs427, TestSize.Level0)
{
    ItFsJffs427();
}

/* *
 * @tc.name: ItFsJffs428
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs428, TestSize.Level0)
{
    ItFsJffs428();
}

/* *
 * @tc.name: ItFsJffs429
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs429, TestSize.Level0)
{
    ItFsJffs429();
}

/* *
 * @tc.name: ItFsJffs430
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs430, TestSize.Level0)
{
    ItFsJffs430();
}

/* *
 * @tc.name: ItFsJffs431
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs431, TestSize.Level0)
{
    ItFsJffs431();
}

/* *
 * @tc.name: ItFsJffs432
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs432, TestSize.Level0)
{
    ItFsJffs432();
}

/* *
 * @tc.name: ItFsJffs433
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs433, TestSize.Level0)
{
    ItFsJffs433();
}

/* *
 * @tc.name: ItFsJffs434
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs434, TestSize.Level0)
{
    ItFsJffs434();
}

/* *
 * @tc.name: ItFsJffs435
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs435, TestSize.Level0)
{
    ItFsJffs435();
}

/* *
 * @tc.name: ItFsJffs454
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs454, TestSize.Level0)
{
    ItFsJffs454();
}

/* *
 * @tc.name: ItFsJffs455
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs455, TestSize.Level0)
{
    ItFsJffs455();
}

/* *
 * @tc.name: ItFsJffs456
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs456, TestSize.Level0)
{
    ItFsJffs456();
}

/* *
 * @tc.name: ItFsJffs457
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs457, TestSize.Level0)
{
    ItFsJffs457();
}

/* *
 * @tc.name: ItFsJffs458
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs458, TestSize.Level0)
{
    ItFsJffs458();
}

/* *
 * @tc.name: ItFsJffs459
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs459, TestSize.Level0)
{
    ItFsJffs459();
}

/* *
 * @tc.name: ItFsJffs460
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs460, TestSize.Level0)
{
    ItFsJffs460();
}

/* *
 * @tc.name: ItFsJffs461
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs461, TestSize.Level0)
{
    ItFsJffs461();
}

/* *
 * @tc.name: ItFsJffs462
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs462, TestSize.Level0)
{
    ItFsJffs462();
}

/* *
 * @tc.name: ItFsJffs487
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs487, TestSize.Level0)
{
    ItFsJffs487();
}

/* *
 * @tc.name: ItFsJffs488
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs488, TestSize.Level0)
{
    ItFsJffs488();
}

/* *
 * @tc.name: ItFsJffs489
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs489, TestSize.Level0)
{
    ItFsJffs489();
}

/* *
 * @tc.name: ItFsJffs490
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs490, TestSize.Level0)
{
    ItFsJffs490();
}

/* *
 * @tc.name: ItFsJffs491
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs491, TestSize.Level0)
{
    ItFsJffs491();
}

/* *
 * @tc.name: ItFsJffs492
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs492, TestSize.Level0)
{
    ItFsJffs492();
}

/* *
 * @tc.name: ItFsJffs493
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs493, TestSize.Level0)
{
    ItFsJffs493();
}

/* *
 * @tc.name: ItFsJffs494
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs494, TestSize.Level0)
{
    ItFsJffs494();
}

/* *
 * @tc.name: ItFsJffs496
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs496, TestSize.Level0)
{
    ItFsJffs496();
}

/* *
 * @tc.name: ItFsJffs497
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs497, TestSize.Level0)
{
    ItFsJffs497();
}

/* *
 * @tc.name: ItFsJffs498
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs498, TestSize.Level0)
{
    ItFsJffs498();
}

/* *
 * @tc.name: ItFsJffs499
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs499, TestSize.Level0)
{
    ItFsJffs499();
}

/* *
 * @tc.name: ItFsJffs500
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs500, TestSize.Level0)
{
    ItFsJffs500();
}

/* *
 * @tc.name: ItFsJffs501
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs501, TestSize.Level0)
{
    ItFsJffs501();
}

/* *
 * @tc.name: ItFsJffs502
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs502, TestSize.Level0)
{
    ItFsJffs502();
}

/* *
 * @tc.name: ItFsJffs503
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs503, TestSize.Level0)
{
    ItFsJffs503();
}

/* *
 * @tc.name: ItFsJffs504
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs504, TestSize.Level0)
{
    ItFsJffs504();
}

/* *
 * @tc.name: ItFsJffs505
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs505, TestSize.Level0)
{
    ItFsJffs505();
}

/* *
 * @tc.name: ItFsJffs506
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs506, TestSize.Level0)
{
    ItFsJffs506();
}

/* *
 * @tc.name: ItFsJffs507
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs507, TestSize.Level0)
{
    ItFsJffs507();
}

/* *
 * @tc.name: ItFsJffs508
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs508, TestSize.Level0)
{
    ItFsJffs508();
}

/* *
 * @tc.name: ItFsJffs509
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs509, TestSize.Level0)
{
    ItFsJffs509();
}

/* *
 * @tc.name: ItFsJffs510
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs510, TestSize.Level0)
{
    ItFsJffs510();
}

/* *
 * @tc.name: ItFsJffs511
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs511, TestSize.Level0)
{
    ItFsJffs511();
}

/* *
 * @tc.name: ItFsJffs512
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs512, TestSize.Level0)
{
    ItFsJffs512();
}

/* *
 * @tc.name: ItFsJffs513
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs513, TestSize.Level0)
{
    ItFsJffs513();
}

/* *
 * @tc.name: ItFsJffs514
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs514, TestSize.Level0)
{
    ItFsJffs514();
}

/* *
 * @tc.name: ItFsJffs515
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs515, TestSize.Level0)
{
    ItFsJffs515();
}

/* *
 * @tc.name: ItFsJffs516
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs516, TestSize.Level0)
{
    ItFsJffs516();
}

/* *
 * @tc.name: ItFsJffs517
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs517, TestSize.Level0)
{
    ItFsJffs517();
}

/* *
 * @tc.name: ItFsJffs518
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs518, TestSize.Level0)
{
    ItFsJffs518();
}

/* *
 * @tc.name: ItFsJffs519
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs519, TestSize.Level0)
{
    ItFsJffs519();
}

/* *
 * @tc.name: ItFsJffs520
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs520, TestSize.Level0)
{
    ItFsJffs520();
}

/* *
 * @tc.name: ItFsJffs521
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs521, TestSize.Level0)
{
    ItFsJffs521();
}

/* *
 * @tc.name: ItFsJffs522
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs522, TestSize.Level0)
{
    ItFsJffs522();
}

/* *
 * @tc.name: ItFsJffs523
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs523, TestSize.Level0)
{
    ItFsJffs523();
}

/* *
 * @tc.name: ItFsJffs524
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs524, TestSize.Level0)
{
    ItFsJffs524();
}

/* *
 * @tc.name: ItFsJffs526
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs526, TestSize.Level0)
{
    ItFsJffs526();
}

/* *
 * @tc.name: ItFsJffs528
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs528, TestSize.Level0)
{
    ItFsJffs528();
}

/* *
 * @tc.name: ItFsJffs529
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs529, TestSize.Level0)
{
    ItFsJffs529();
}

/* *
 * @tc.name: ItFsJffs530
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs530, TestSize.Level0)
{
    ItFsJffs530();
}

/* *
 * @tc.name: ItFsJffs531
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs531, TestSize.Level0)
{
    ItFsJffs531();
}

/* *
 * @tc.name: ItFsJffs532
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs532, TestSize.Level0)
{
    ItFsJffs532();
}

/* *
 * @tc.name: ItFsJffs533
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs533, TestSize.Level0)
{
    ItFsJffs533();
}

/* *
 * @tc.name: ItFsJffs534
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs534, TestSize.Level0)
{
    ItFsJffs534();
}

/* *
 * @tc.name: ItFsJffs541
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs541, TestSize.Level0)
{
    ItFsJffs541();
}

/* *
 * @tc.name: ItFsJffs542
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs542, TestSize.Level0)
{
    ItFsJffs542();
}

/* *
 * @tc.name: ItFsJffs543
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs543, TestSize.Level0)
{
    ItFsJffs543();
}

/* *
 * @tc.name: ItFsJffs544
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs544, TestSize.Level0)
{
    ItFsJffs544();
}

/* *
 * @tc.name: ItFsJffs545
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs545, TestSize.Level0)
{
    ItFsJffs545();
}

/* *
 * @tc.name: ItFsJffs546
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs546, TestSize.Level0)
{
    ItFsJffs546();
}

/* *
 * @tc.name: ItFsJffs547
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs547, TestSize.Level0)
{
    ItFsJffs547();
}

/* *
 * @tc.name: ItFsJffs548
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs548, TestSize.Level0)
{
    ItFsJffs548();
}

/* *
 * @tc.name: ItFsJffs549
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs549, TestSize.Level0)
{
    ItFsJffs549();
}

/* *
 * @tc.name: ItFsJffs550
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs550, TestSize.Level0)
{
    ItFsJffs550();
}

/* *
 * @tc.name: ItFsJffs551
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs551, TestSize.Level0)
{
    ItFsJffs551();
}

/* *
 * @tc.name: ItFsJffs552
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs552, TestSize.Level0)
{
    ItFsJffs552();
}

/* *
 * @tc.name: ItFsJffs553
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs553, TestSize.Level0)
{
    ItFsJffs553();
}

/* *
 * @tc.name: ItFsJffs554
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs554, TestSize.Level0)
{
    ItFsJffs554();
}

/* *
 * @tc.name: ItFsJffs555
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs555, TestSize.Level0)
{
    ItFsJffs555();
}

/* *
 * @tc.name: ItFsJffs556
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs556, TestSize.Level0)
{
    ItFsJffs556();
}

/* *
 * @tc.name: ItFsJffs557
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs557, TestSize.Level0)
{
    ItFsJffs557();
}

/* *
 * @tc.name: ItFsJffs560
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs560, TestSize.Level0)
{
    ItFsJffs560();
}

/* *
 * @tc.name: ItFsJffs562
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs562, TestSize.Level0)
{
    ItFsJffs562();
}

/* *
 * @tc.name: ItFsJffs563
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs563, TestSize.Level0)
{
    ItFsJffs563();
}

/* *
 * @tc.name: ItFsJffs564
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs564, TestSize.Level0)
{
    ItFsJffs564();
}

/* *
 * @tc.name: ItFsJffs565
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs565, TestSize.Level0)
{
    ItFsJffs565();
}

/* *
 * @tc.name: ItFsJffs566
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs566, TestSize.Level0)
{
    ItFsJffs566();
}

/* *
 * @tc.name: ItFsJffs567
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs567, TestSize.Level0)
{
    ItFsJffs567();
}

/* *
 * @tc.name: ItFsJffs568
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs568, TestSize.Level0)
{
    ItFsJffs568();
}

/* *
 * @tc.name: ItFsJffs569
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs569, TestSize.Level0)
{
    ItFsJffs569(); // the second param in stat64 is NULL
}

/* *
 * @tc.name: ItFsJffs570
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs570, TestSize.Level0)
{
    ItFsJffs570(); // the second param in stat64 is NULL
}

/* *
 * @tc.name: ItFsJffs571
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs571, TestSize.Level0)
{
    ItFsJffs571();
}

/* *
 * @tc.name: ItFsJffs572
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs572, TestSize.Level0)
{
    ItFsJffs572(); // the second param in fstat64 is NULL
}

/* *
 * @tc.name: ItFsJffs573
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs573, TestSize.Level0)
{
    ItFsJffs573(); // the second param in fstat64 is NULL
}

/* *
 * @tc.name: ItFsJffs574
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs574, TestSize.Level0)
{
    ItFsJffs574();
}

/* *
 * @tc.name: ItFsJffs124
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs124, TestSize.Level0)
{
    ItFsJffs124();
}

/* *
 * @tc.name: ItFsJffs125
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs125, TestSize.Level0)
{
    ItFsJffs125();
}

/* *
 * @tc.name: ItFsJffs583
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs583, TestSize.Level0)
{
    ItFsJffs583();
}

/* *
 * @tc.name: ItFsJffs584
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs584, TestSize.Level0)
{
    ItFsJffs584();
}

/* *
 * @tc.name: ItFsJffs586
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs586, TestSize.Level0)
{
    ItFsJffs586();
}

/* *
 * @tc.name: ItFsJffs589
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs589, TestSize.Level0)
{
    ItFsJffs589();
}

/* *
 * @tc.name: ItFsJffs591
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs591, TestSize.Level0)
{
    ItFsJffs591();
}

/* *
 * @tc.name: ItFsJffs592
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs592, TestSize.Level0)
{
    ItFsJffs592();
}

/* *
 * @tc.name: ItFsJffs593
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs593, TestSize.Level0)
{
    ItFsJffs593();
}

/* *
 * @tc.name: ItFsJffs594
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs594, TestSize.Level0)
{
    ItFsJffs594();
}

/* *
 * @tc.name: ItFsJffs595
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs595, TestSize.Level0)
{
    ItFsJffs595();
}

/* *
 * @tc.name: ItFsJffs596
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs596, TestSize.Level0)
{
    ItFsJffs596();
}

/* *
 * @tc.name: ItFsJffs603
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs603, TestSize.Level0)
{
    ItFsJffs603();
}

/* *
 * @tc.name: ItFsJffs636
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs636, TestSize.Level0)
{
    ItFsJffs636();
}

/* *
 * @tc.name: ItFsJffs643
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs643, TestSize.Level0)
{
    ItFsJffs643();
}

/* *
 * @tc.name: ItFsJffs644
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs644, TestSize.Level0)
{
    ItFsJffs644();
}

/* *
 * @tc.name: ItFsJffs645
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs645, TestSize.Level0)
{
    ItFsJffs645();
}

/* *
 * @tc.name: ItFsJffs646
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs646, TestSize.Level0)
{
    ItFsJffs646();
}

/* *
 * @tc.name: ItFsJffs648
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs648, TestSize.Level0)
{
    ItFsJffs648();
}

/* *
 * @tc.name: ItFsJffs649
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs649, TestSize.Level0)
{
    ItFsJffs649();
}

/* *
 * @tc.name: ItFsJffs650
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs650, TestSize.Level0)
{
    ItFsJffs650();
}

/* *
 * @tc.name: ItFsJffs651
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs651, TestSize.Level0)
{
    ItFsJffs651();
}

/* *
 * @tc.name: ItFsJffs652
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs652, TestSize.Level0)
{
    ItFsJffs652();
}

/* *
 * @tc.name: ItFsJffs653
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs653, TestSize.Level0)
{
    ItFsJffs653();
}

/* *
 * @tc.name: ItFsJffs654
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs654, TestSize.Level0)
{
    ItFsJffs654();
}

/* *
 * @tc.name: ItFsJffs655
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs655, TestSize.Level0)
{
    ItFsJffs655();
}

/* *
 * @tc.name: ItFsJffs656
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs656, TestSize.Level0)
{
    ItFsJffs656();
}

/* *
 * @tc.name: ItFsJffs663
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs663, TestSize.Level0)
{
    ItFsJffs663();
}

/* *
 * @tc.name: ItFsJffs664
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs664, TestSize.Level0)
{
    ItFsJffs664();
}

/* *
 * @tc.name: ItFsJffs665
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs665, TestSize.Level0)
{
    ItFsJffs665();
}

/* *
 * @tc.name: ItFsJffs666
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs666, TestSize.Level0)
{
    ItFsJffs666();
}

/* *
 * @tc.name: ItFsJffs668
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs668, TestSize.Level0)
{
    ItFsJffs668();
}

/* *
 * @tc.name: ItFsJffs669
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs669, TestSize.Level0)
{
    ItFsJffs669();
}

/* *
 * @tc.name: ItFsJffs670
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs670, TestSize.Level0)
{
    ItFsJffs670();
}

/* *
 * @tc.name: ItFsJffs671
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs671, TestSize.Level0)
{
    ItFsJffs671();
}

/* *
 * @tc.name: ItFsJffs672
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs672, TestSize.Level0)
{
    ItFsJffs672();
}

/* *
 * @tc.name: ItFsJffs673
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs673, TestSize.Level0)
{
    ItFsJffs673();
}

/* *
 * @tc.name: ItFsJffs674
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs674, TestSize.Level0)
{
    ItFsJffs674();
}

/* *
 * @tc.name: ItFsJffs675
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs675, TestSize.Level0)
{
    ItFsJffs675();
}

/* *
 * @tc.name: ItFsJffs676
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs676, TestSize.Level0)
{
    ItFsJffs676();
}

/* *
 * @tc.name: ItFsJffs690
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs690, TestSize.Level0)
{
    ItFsJffs690();
}

/* *
 * @tc.name: ItFsJffs694
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs694, TestSize.Level0)
{
    ItFsJffs694();
}

/* *
 * @tc.name: ItFsJffs696
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs696, TestSize.Level0)
{
    ItFsJffs696();
}

/* *
 * @tc.name: ItFsJffs697
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs697, TestSize.Level0)
{
    ItFsJffs697();
}

/* *
 * @tc.name: ItFsJffs700
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs700, TestSize.Level0)
{
    ItFsJffs700();
}

/* *
 * @tc.name: ItFsJffs701
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs701, TestSize.Level0)
{
    ItFsJffs701();
}

/* *
 * @tc.name: ItFsJffs807
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs807, TestSize.Level0)
{
    ItFsJffs807(); // jffs dir1 rename dir2,dir2 have more than two files,rename failed
}

/* *
 * @tc.name: ItFsJffs808
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs808, TestSize.Level0)
{
    ItFsJffs808();
}

HWTEST_F(VfsJffsTest, ItFsTestLink001, TestSize.Level0)
{
    ItFsTestLink001();
}

HWTEST_F(VfsJffsTest, ItFsTestLink002, TestSize.Level0)
{
    ItFsTestLink002();
}

HWTEST_F(VfsJffsTest, ItFsTestLink003, TestSize.Level0)
{
    ItFsTestLink003();
}

HWTEST_F(VfsJffsTest, ItFsTestLinkat001, TestSize.Level0)
{
    ItFsTestLinkat001();
}

HWTEST_F(VfsJffsTest, ItFsTestLinkat002, TestSize.Level0)
{
    ItFsTestLinkat002();
}

HWTEST_F(VfsJffsTest, ItFsTestLinkat003, TestSize.Level0)
{
    ItFsTestLinkat003();
}

HWTEST_F(VfsJffsTest, ItFsTestReadlink001, TestSize.Level0)
{
    ItFsTestReadlink001();
}

HWTEST_F(VfsJffsTest, ItFsTestSymlink001, TestSize.Level0)
{
    ItFsTestSymlink001();
}

HWTEST_F(VfsJffsTest, ItFsTestSymlink002, TestSize.Level0)
{
    ItFsTestSymlink002();
}

HWTEST_F(VfsJffsTest, ItFsTestSymlink003, TestSize.Level0)
{
    ItFsTestSymlink003();
}

HWTEST_F(VfsJffsTest, ItFsTestSymlinkat001, TestSize.Level0)
{
    ItFsTestSymlinkat001();
}

HWTEST_F(VfsJffsTest, ItFsTestMountRdonly001, TestSize.Level0)
{
    ItFsTestMountRdonly001();
}

HWTEST_F(VfsJffsTest, ItFsTestMountRdonly002, TestSize.Level0)
{
    ItFsTestMountRdonly002();
}

HWTEST_F(VfsJffsTest, ItFsTestMountRdonly003, TestSize.Level0)
{
    ItFsTestMountRdonly003();
}

#endif
#if defined(LOSCFG_USER_TEST_SMOKE)
/* *
 * @tc.name: It_Test_Dac_001
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItTestDac001, TestSize.Level0)
{
    ItTestDac001();
}

/* *
 * @tc.name: ItFsJffs001
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs001, TestSize.Level0)
{
    ItFsJffs001();
}

/* *
 * @tc.name: ItFsJffs002
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs002, TestSize.Level0)
{
    ItFsJffs002();
}

/* *
 * @tc.name: ItFsJffs003
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs003, TestSize.Level0)
{
    ItFsJffs003();
}

/* *
 * @tc.name: ItFsJffs005
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs005, TestSize.Level0)
{
    ItFsJffs005();
}

/* *
 * @tc.name: ItFsJffs021
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs021, TestSize.Level0)
{
    ItFsJffs021();
}

/* *
 * @tc.name: ItFsJffs022
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs022, TestSize.Level0)
{
    ItFsJffs022();
}

/* *
 * @tc.name: ItFsJffs026
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs026, TestSize.Level0)
{
    ItFsJffs026();
}

/* *
 * @tc.name: ItFsJffs027
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs027, TestSize.Level0)
{
    ItFsJffs027();
}

/* *
 * @tc.name: ItFsJffs095
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs095, TestSize.Level0)
{
    ItFsJffs095();
}

/* *
 * @tc.name: ItFsJffs535
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffs535, TestSize.Level0)
{
    ItFsJffs535();
}

#endif

#if defined(LOSCFG_USER_TEST_PRESSURE)
/* *
 * @tc.name: ItFsJffsPRESSURE_001
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsPressure001, TestSize.Level0)
{
    ItFsJffsPressure001();
}

/* *
 * @tc.name: ItFsJffsPRESSURE_002
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsPressure002, TestSize.Level0)
{
    ItFsJffsPressure002();
}

/* *
 * @tc.name: ItFsJffsPRESSURE_003
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsPressure003, TestSize.Level0)
{
    ItFsJffsPressure003();
}

/* *
 * @tc.name: ItFsJffsPRESSURE_004
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsPressure004, TestSize.Level0)
{
    ItFsJffsPressure004();
}

/* *
 * @tc.name: ItFsJffsPRESSURE_005
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsPressure005, TestSize.Level0)
{
    ItFsJffsPressure005();
}

/* *
 * @tc.name: ItFsJffsPRESSURE_006
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsPressure006, TestSize.Level0)
{
    ItFsJffsPressure006();
}

/* *
 * @tc.name: ItFsJffsPRESSURE_007
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsPressure007, TestSize.Level0)
{
    ItFsJffsPressure007();
}

/* *
 * @tc.name: ItFsJffsPRESSURE_008
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsPressure008, TestSize.Level0)
{
    ItFsJffsPressure008();
}

/* *
 * @tc.name: ItFsJffsPRESSURE_009
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsPressure009, TestSize.Level0)
{
    ItFsJffsPressure009();
}

/* *
 * @tc.name: ItFsJffsPRESSURE_010
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsPressure010, TestSize.Level0)
{
    ItFsJffsPressure010();
}

/* *
 * @tc.name: ItFsJffsPRESSURE_011
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsPressure011, TestSize.Level0)
{
    ItFsJffsPressure011();
}

/* *
 * @tc.name: ItFsJffsPRESSURE_012
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsPressure012, TestSize.Level0)
{
    ItFsJffsPressure012();
}

/* *
 * @tc.name: ItFsJffsPRESSURE_014
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsPressure014, TestSize.Level0)
{
    ItFsJffsPressure014();
}

/* *
 * @tc.name: ItFsJffsPRESSURE_015
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsPressure015, TestSize.Level0)
{
    ItFsJffsPressure015();
}

/* *
 * @tc.name: ItFsJffsPRESSURE_016
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsPressure016, TestSize.Level0)
{
    ItFsJffsPressure016();
}

/* *
 * @tc.name: ItFsJffsPRESSURE_017
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsPressure017, TestSize.Level0)
{
    ItFsJffsPressure017();
}

/* *
 * @tc.name: ItFsJffsPRESSURE_018
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsPressure018, TestSize.Level0)
{
    ItFsJffsPressure018();
}

/* *
 * @tc.name: ItFsJffsPRESSURE_019
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsPressure019, TestSize.Level0)
{
    ItFsJffsPressure019();
}

/* *
 * @tc.name: ItFsJffsPRESSURE_020
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsPressure020, TestSize.Level0)
{
    ItFsJffsPressure020();
}

/* *
 * @tc.name: ItFsJffsPRESSURE_021
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsPressure021, TestSize.Level0)
{
    ItFsJffsPressure021();
}

/* *
 * @tc.name: ItFsJffsPRESSURE_022
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsPressure022, TestSize.Level0)
{
    ItFsJffsPressure022();
}

/* *
 * @tc.name: ItFsJffsPRESSURE_023
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsPressure023, TestSize.Level0)
{
    ItFsJffsPressure023();
}

/* *
 * @tc.name: ItFsJffsPRESSURE_025
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsPressure025, TestSize.Level0)
{
    ItFsJffsPressure025();
}

/* *
 * @tc.name: ItFsJffsPRESSURE_026
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsPressure026, TestSize.Level0)
{
    ItFsJffsPressure026();
}

/* *
 * @tc.name: ItFsJffsPRESSURE_027
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsPressure027, TestSize.Level0)
{
    ItFsJffsPressure027();
}

/* *
 * @tc.name: ItFsJffsPRESSURE_028
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsPressure028, TestSize.Level0)
{
    ItFsJffsPressure028();
}

/* *
 * @tc.name: ItFsJffsPRESSURE_030
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsPressure030, TestSize.Level0)
{
    ItFsJffsPressure030();
}

/* *
 * @tc.name: ItFsJffsPRESSURE_031
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsPressure031, TestSize.Level0)
{
    ItFsJffsPressure031();
}

/* *
 * @tc.name: ItFsJffsPRESSURE_032
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsPressure032, TestSize.Level0)
{
    ItFsJffsPressure032();
}

/* *
 * @tc.name: ItFsJffsPRESSURE_033
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsPressure033, TestSize.Level0)
{
    ItFsJffsPressure033();
}

/* *
 * @tc.name: ItFsJffsPRESSURE_034
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsPressure034, TestSize.Level0)
{
    ItFsJffsPressure034();
}

/* *
 * @tc.name: ItFsJffsPRESSURE_035
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsPressure035, TestSize.Level0)
{
    ItFsJffsPressure035();
}

/* *
 * @tc.name: ItFsJffsPRESSURE_036
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsPressure036, TestSize.Level0)
{
    ItFsJffsPressure036();
}

/* *
 * @tc.name: ItFsJffsPRESSURE_037
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsPressure037, TestSize.Level0)
{
    ItFsJffsPressure037();
}

/* *
 * @tc.name: ItFsJffsPRESSURE_038
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsPressure038, TestSize.Level0)
{
    ItFsJffsPressure038();
}

/* *
 * @tc.name: ItFsJffsPRESSURE_039
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsPressure039, TestSize.Level0)
{
    ItFsJffsPressure039();
}

/* *
 * @tc.name: ItFsJffsPRESSURE_040
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsPressure040, TestSize.Level0)
{
    ItFsJffsPressure040();
}

/* *
 * @tc.name: ItFsJffsPRESSURE_041
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsPressure041, TestSize.Level0)
{
    ItFsJffsPressure041();
}

/* *
 * @tc.name: ItFsJffsPRESSURE_042
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsPressure042, TestSize.Level0)
{
    ItFsJffsPressure042();
}

/* *
 * @tc.name: ItFsJffsPRESSURE_043
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsPressure043, TestSize.Level0)
{
    ItFsJffsPressure043();
}

/* *
 * @tc.name: ItFsJffsPRESSURE_044
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsPressure044, TestSize.Level0)
{
    ItFsJffsPressure044();
}

/* *
 * @tc.name: ItFsJffsPRESSURE_045
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsPressure045, TestSize.Level0)
{
    ItFsJffsPressure045();
}

/* *
 * @tc.name: ItFsJffsPRESSURE_046
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsPressure046, TestSize.Level0)
{
    ItFsJffsPressure046();
}

/* *
 * @tc.name: ItFsJffsPRESSURE_047
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsPressure047, TestSize.Level0)
{
    ItFsJffsPressure047();
}

/* *
 * @tc.name: ItFsJffsPRESSURE_048
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsPressure048, TestSize.Level0)
{
    ItFsJffsPressure048(); // Time-consuming
}

/* *
 * @tc.name: ItFsJffsPRESSURE_049
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsPressure049, TestSize.Level0)
{
    ItFsJffsPressure049(); // Time-consuming
}

/* *
 * @tc.name: ItFsJffsPRESSURE_051
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsPressure051, TestSize.Level0)
{
    ItFsJffsPressure051();
}

/* *
 * @tc.name: ItFsJffsPRESSURE_052
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsPressure052, TestSize.Level0)
{
    ItFsJffsPressure052();
}

/* *
 * @tc.name: ItFsJffsPRESSURE_053
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsPressure053, TestSize.Level0)
{
    ItFsJffsPressure053();
}

/* *
 * @tc.name: ItFsJffsPRESSURE_301
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsPressure301, TestSize.Level0)
{
    ItFsJffsPressure301();
}

/* *
 * @tc.name: ItFsJffsPRESSURE_302
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsPressure302, TestSize.Level0)
{
    ItFsJffsPressure302();
}

/* *
 * @tc.name: ItFsJffsPRESSURE_303
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsPressure303, TestSize.Level0)
{
    ItFsJffsPressure303();
}

/* *
 * @tc.name: ItFsJffsPRESSURE_304
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsPressure304, TestSize.Level0)
{
    ItFsJffsPressure304();
}

/* *
 * @tc.name: ItFsJffsPRESSURE_305
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsPressure305, TestSize.Level0)
{
    ItFsJffsPressure305();
}

/* *
 * @tc.name: ItFsJffsPRESSURE_306
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsPressure306, TestSize.Level0)
{
    ItFsJffsPressure306();
}

/* *
 * @tc.name: ItFsJffsPRESSURE_307
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsPressure307, TestSize.Level0)
{
    ItFsJffsPressure307();
}

/* *
 * @tc.name: ItFsJffsPRESSURE_308
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsPressure308, TestSize.Level0)
{
    ItFsJffsPressure308();
}

/* *
 * @tc.name: ItFsJffsPRESSURE_309
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsPressure309, TestSize.Level0)
{
    ItFsJffsPressure309();
}

/* *
 * @tc.name: ItFsJffsPRESSURE_310
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsPressure310, TestSize.Level0)
{
    ItFsJffsPressure310();
}

/* *
 * @tc.name: ItFsJffsPRESSURE_311
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsPressure311, TestSize.Level0)
{
    ItFsJffsPressure311();
}

/* *
 * @tc.name: ItFsJffsPRESSURE_312
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsPressure312, TestSize.Level0)
{
    ItFsJffsPressure312();
}

/* *
 * @tc.name: ItFsJffsPRESSURE_313
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsPressure313, TestSize.Level0)
{
    ItFsJffsPressure313();
}

/* *
 * @tc.name: ItFsJffsPRESSURE_314
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsPressure314, TestSize.Level0)
{
    ItFsJffsPressure314();
}

/* *
 * @tc.name: ItFsJffsPRESSURE_315
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsPressure315, TestSize.Level0)
{
    ItFsJffsPressure315();
}


/* *
 * @tc.name: ItFsJffsMULTIPTHREAD_001
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsMultipthread001, TestSize.Level0)
{
    ItFsJffsMultipthread001();
}

/* *
 * @tc.name: ItFsJffsMULTIPTHREAD_002
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsMultipthread002, TestSize.Level0)
{
    ItFsJffsMultipthread002();
}

/* *
 * @tc.name: ItFsJffsMULTIPTHREAD_003
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsMultipthread003, TestSize.Level0)
{
    ItFsJffsMultipthread003();
}

/* *
 * @tc.name: ItFsJffsMULTIPTHREAD_004
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsMultipthread004, TestSize.Level0)
{
    ItFsJffsMultipthread004();
}

/* *
 * @tc.name: ItFsJffsMULTIPTHREAD_005
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsMultipthread005, TestSize.Level0)
{
    ItFsJffsMultipthread005();
}

/* *
 * @tc.name: ItFsJffsMULTIPTHREAD_006
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsMultipthread006, TestSize.Level0)
{
    ItFsJffsMultipthread006();
}

/* *
 * @tc.name: ItFsJffsMULTIPTHREAD_007
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsMultipthread007, TestSize.Level0)
{
    ItFsJffsMultipthread007();
}

/* *
 * @tc.name: ItFsJffsMULTIPTHREAD_008
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsMultipthread008, TestSize.Level0)
{
    ItFsJffsMultipthread008();
}

/* *
 * @tc.name: ItFsJffsMULTIPTHREAD_009
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsMultipthread009, TestSize.Level0)
{
    ItFsJffsMultipthread009();
}

/* *
 * @tc.name: ItFsJffsMULTIPTHREAD_010
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsMultipthread010, TestSize.Level0)
{
    ItFsJffsMultipthread010();
}

/* *
 * @tc.name: ItFsJffsMULTIPTHREAD_011
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsMultipthread011, TestSize.Level0)
{
    ItFsJffsMultipthread011();
}

/* *
 * @tc.name: ItFsJffsMULTIPTHREAD_012
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsMultipthread012, TestSize.Level0)
{
    ItFsJffsMultipthread012();
}

/* *
 * @tc.name: ItFsJffsMULTIPTHREAD_013
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsMultipthread013, TestSize.Level0)
{
    ItFsJffsMultipthread013();
}

/* *
 * @tc.name: ItFsJffsMULTIPTHREAD_014
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsMultipthread014, TestSize.Level0)
{
    ItFsJffsMultipthread014();
}

/* *
 * @tc.name: ItFsJffsMULTIPTHREAD_015
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsMultipthread015, TestSize.Level0)
{
    ItFsJffsMultipthread015();
}

/* *
 * @tc.name: ItFsJffsMULTIPTHREAD_016
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsMultipthread016, TestSize.Level0)
{
    ItFsJffsMultipthread016();
}

/* *
 * @tc.name: ItFsJffsMULTIPTHREAD_017
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsMultipthread017, TestSize.Level0)
{
    ItFsJffsMultipthread017();
}

/* *
 * @tc.name: ItFsJffsMULTIPTHREAD_018
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsMultipthread018, TestSize.Level0)
{
    ItFsJffsMultipthread018();
}

/* *
 * @tc.name: ItFsJffsMULTIPTHREAD_019
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsMultipthread019, TestSize.Level0)
{
    ItFsJffsMultipthread019();
}

/* *
 * @tc.name: ItFsJffsMULTIPTHREAD_020
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsMultipthread020, TestSize.Level0)
{
    ItFsJffsMultipthread020();
}

/* *
 * @tc.name: ItFsJffsMULTIPTHREAD_021
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsMultipthread021, TestSize.Level0)
{
    ItFsJffsMultipthread021();
}

/* *
 * @tc.name: ItFsJffsMULTIPTHREAD_022
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsMultipthread022, TestSize.Level0)
{
    ItFsJffsMultipthread022();
}

/* *
 * @tc.name: ItFsJffsMULTIPTHREAD_023
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsMultipthread023, TestSize.Level0)
{
    ItFsJffsMultipthread023();
}

/* *
 * @tc.name: ItFsJffsMULTIPTHREAD_024
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsMultipthread024, TestSize.Level0)
{
    ItFsJffsMultipthread024();
}

/* *
 * @tc.name: ItFsJffsMULTIPTHREAD_025
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsMultipthread025, TestSize.Level0)
{
    ItFsJffsMultipthread025();
}

/* *
 * @tc.name: ItFsJffsMULTIPTHREAD_026
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsMultipthread026, TestSize.Level0)
{
    ItFsJffsMultipthread026();
}

/* *
 * @tc.name: ItFsJffsMULTIPTHREAD_027
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsMultipthread027, TestSize.Level0)
{
    ItFsJffsMultipthread027();
}

/* *
 * @tc.name: ItFsJffsMULTIPTHREAD_028
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsMultipthread028, TestSize.Level0)
{
    ItFsJffsMultipthread028();
}

/* *
 * @tc.name: ItFsJffsMULTIPTHREAD_029
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsMultipthread029, TestSize.Level0)
{
    ItFsJffsMultipthread029();
}

/* *
 * @tc.name: ItFsJffsMULTIPTHREAD_030
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsMultipthread030, TestSize.Level0)
{
    ItFsJffsMultipthread030();
}

/* *
 * @tc.name: ItFsJffsMULTIPTHREAD_031
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsMultipthread031, TestSize.Level0)
{
    ItFsJffsMultipthread031();
}

/* *
 * @tc.name: ItFsJffsMULTIPTHREAD_032
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsMultipthread032, TestSize.Level0)
{
    ItFsJffsMultipthread032();
}

/* *
 * @tc.name: ItFsJffsMULTIPTHREAD_033
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsMultipthread033, TestSize.Level0)
{
    ItFsJffsMultipthread033();
}

/* *
 * @tc.name: ItFsJffsMULTIPTHREAD_034
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsMultipthread034, TestSize.Level0)
{
    ItFsJffsMultipthread034();
}

/* *
 * @tc.name: ItFsJffsMULTIPTHREAD_035
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsMultipthread035, TestSize.Level0)
{
    ItFsJffsMultipthread035();
}

/* *
 * @tc.name: ItFsJffsMULTIPTHREAD_036
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsMultipthread036, TestSize.Level0)
{
    ItFsJffsMultipthread036();
}

/* *
 * @tc.name: ItFsJffsMULTIPTHREAD_037
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsMultipthread037, TestSize.Level0)
{
    ItFsJffsMultipthread037();
}

/* *
 * @tc.name: ItFsJffsMULTIPTHREAD_038
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsMultipthread038, TestSize.Level0)
{
    ItFsJffsMultipthread038();
}

/* *
 * @tc.name: ItFsJffsMULTIPTHREAD_039
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsMultipthread039, TestSize.Level0)
{
    ItFsJffsMultipthread039();
}

/* *
 * @tc.name: ItFsJffsMULTIPTHREAD_040
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsMultipthread040, TestSize.Level0)
{
    ItFsJffsMultipthread040();
}

/* *
 * @tc.name: ItFsJffsMULTIPTHREAD_041
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsMultipthread041, TestSize.Level0)
{
    ItFsJffsMultipthread041();
}

/* *
 * @tc.name: ItFsJffsMULTIPTHREAD_042
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsMultipthread042, TestSize.Level0)
{
    ItFsJffsMultipthread042();
}

/* *
 * @tc.name: ItFsJffsMULTIPTHREAD_043
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsMultipthread043, TestSize.Level0)
{
    ItFsJffsMultipthread043();
}

/* *
 * @tc.name: ItFsJffsMULTIPTHREAD_044
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsMultipthread044, TestSize.Level0)
{
    ItFsJffsMultipthread044();
}

/* *
 * @tc.name: ItFsJffsMULTIPTHREAD_045
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsMultipthread045, TestSize.Level0)
{
    ItFsJffsMultipthread045();
}

/* *
 * @tc.name: ItFsJffsMULTIPTHREAD_046
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsMultipthread046, TestSize.Level0)
{
    ItFsJffsMultipthread046();
}

/* *
 * @tc.name: ItFsJffsMULTIPTHREAD_047
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsMultipthread047, TestSize.Level0)
{
    ItFsJffsMultipthread047();
}

/* *
 * @tc.name: ItFsJffsMULTIPTHREAD_048
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsMultipthread048, TestSize.Level0)
{
    ItFsJffsMultipthread048();
}

/* *
 * @tc.name: ItFsJffsMULTIPTHREAD_049
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsMultipthread049, TestSize.Level0)
{
    ItFsJffsMultipthread049();
}

/* *
 * @tc.name: ItFsJffsMULTIPTHREAD_050
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsMultipthread050, TestSize.Level0)
{
    ItFsJffsMultipthread050();
}

/* *
 * @tc.name: ItFsJffsMULTIPTHREAD_051
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsMultipthread051, TestSize.Level0)
{
    ItFsJffsMultipthread051();
}

/* *
 * @tc.name: ItFsJffsMULTIPTHREAD_052
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsMultipthread052, TestSize.Level0)
{
    ItFsJffsMultipthread052();
}

/* *
 * @tc.name: ItFsJffsMULTIPTHREAD_053
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsMultipthread053, TestSize.Level0)
{
    ItFsJffsMultipthread053();
}

/* *
 * @tc.name: ItFsJffsMULTIPTHREAD_054
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsMultipthread054, TestSize.Level0)
{
    ItFsJffsMultipthread054();
}

/* *
 * @tc.name: ItFsJffsMULTIPTHREAD_055
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsMultipthread055, TestSize.Level0)
{
    ItFsJffsMultipthread055();
}

/* *
 * @tc.name: ItFsJffsMULTIPTHREAD_056
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsMultipthread056, TestSize.Level0)
{
    ItFsJffsMultipthread056();
}

/* *
 * @tc.name: ItFsJffsMULTIPTHREAD_057
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsMultipthread057, TestSize.Level0)
{
    ItFsJffsMultipthread057();
}

/* *
 * @tc.name: ItFsJffsMULTIPTHREAD_058
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsMultipthread058, TestSize.Level0)
{
    ItFsJffsMultipthread058();
}

/* *
 * @tc.name: ItFsJffsMULTIPTHREAD_059
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsMultipthread059, TestSize.Level0)
{
    ItFsJffsMultipthread059();
}

/* *
 * @tc.name: ItFsJffsMULTIPTHREAD_060
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsMultipthread060, TestSize.Level0)
{
    ItFsJffsMultipthread060();
}

/* *
 * @tc.name: ItFsJffsMULTIPTHREAD_061
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsMultipthread061, TestSize.Level0)
{
    ItFsJffsMultipthread061();
}

/* *
 * @tc.name: ItFsJffsMULTIPTHREAD_062
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsMultipthread062, TestSize.Level0)
{
    ItFsJffsMultipthread062();
}

/* *
 * @tc.name: ItFsJffsMULTIPTHREAD_063
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsMultipthread063, TestSize.Level0)
{
    ItFsJffsMultipthread063();
}


/* *
 * @tc.name: ItFsJffsPERFORMANCE_013
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsPerformance013, TestSize.Level0)
{
    ItFsJffsPerformance013();
}

/* *
 * @tc.name: ItFsJffsPERFORMANCE_001
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsPerformance001, TestSize.Level0)
{
    ItFsJffsPerformance001();
}

/* *
 * @tc.name: ItFsJffsPERFORMANCE_002
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsPerformance002, TestSize.Level0)
{
    ItFsJffsPerformance002();
}

/* *
 * @tc.name: ItFsJffsPERFORMANCE_003
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsPerformance003, TestSize.Level0)
{
    ItFsJffsPerformance003();
}

/* *
 * @tc.name: ItFsJffsPERFORMANCE_004
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsPerformance004, TestSize.Level0)
{
    ItFsJffsPerformance004();
}

/* *
 * @tc.name: ItFsJffsPERFORMANCE_005
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsPerformance005, TestSize.Level0)
{
    ItFsJffsPerformance005();
}

/* *
 * @tc.name: ItFsJffsPERFORMANCE_006
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsPerformance006, TestSize.Level0)
{
    ItFsJffsPerformance006();
}

/* *
 * @tc.name: ItFsJffsPERFORMANCE_008
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsPerformance008, TestSize.Level0)
{
    ItFsJffsPerformance008();
}

/* *
 * @tc.name: ItFsJffsPERFORMANCE_009
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsPerformance009, TestSize.Level0)
{
    ItFsJffsPerformance009();
}

/* *
 * @tc.name: ItFsJffsPERFORMANCE_010
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsPerformance010, TestSize.Level0)
{
    ItFsJffsPerformance010();
}

/* *
 * @tc.name: ItFsJffsPERFORMANCE_011
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsPerformance011, TestSize.Level0)
{
    ItFsJffsPerformance011();
}

/* *
 * @tc.name: ItFsJffsPERFORMANCE_012
 * @tc.desc: function for VfsJffsTest
 * @tc.type: FUNC
 */
HWTEST_F(VfsJffsTest, ItFsJffsPerformance012, TestSize.Level0)
{
    ItFsJffsPerformance012();
}

#endif
}
