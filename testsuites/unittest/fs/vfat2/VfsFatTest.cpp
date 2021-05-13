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

#include "It_vfs_fat.h"

#define MS_PER_SEC 1000

struct iovec g_fatIov[FAT_SHORT_ARRAY_LENGTH];

const INT32 CAL_THRESHOLD = CAL_THRESHOLD_VALUE;
INT32 g_grandSize[MAX_DEF_BUF_NUM] = {
    29, 30290, 3435, 235, 12345, 80, 9845,
    564, 34862, 123, 267890, 36, 6788, 86,
    234567, 1232, 514, 50, 678, 9864, 333333
};

INT32 g_fatFilesMax = 10;
INT32 g_fatFlag = 0;
INT32 g_fatFlagF01 = 0;
INT32 g_fatFlagF02 = 0;
INT32 g_fatFd = 0;
DIR *g_fatDir = nullptr;
UINT32 g_fatMuxHandle1 = 0;
UINT32 g_fatMuxHandle2 = 0;
pthread_mutex_t g_vfatGlobalLock1;
pthread_mutex_t g_vfatGlobalLock2;

INT32 g_fatFd11[FAT_MAXIMUM_SIZES];
INT32 g_fatFd12[FAT_MAXIMUM_SIZES][FAT_MAXIMUM_SIZES];

CHAR g_fatPathname1[FAT_STANDARD_NAME_LENGTH] = FAT_PATH_NAME;
CHAR g_fatPathname2[FAT_STANDARD_NAME_LENGTH] = FAT_PATH_NAME;
CHAR g_fatPathname3[FAT_STANDARD_NAME_LENGTH] = FAT_PATH_NAME;

CHAR g_fatPathname6[FAT_NAME_LIMITTED_SIZE] = FAT_PATH_NAME;
CHAR g_fatPathname7[FAT_NAME_LIMITTED_SIZE] = FAT_PATH_NAME;
CHAR g_fatPathname8[FAT_NAME_LIMITTED_SIZE] = FAT_PATH_NAME;

CHAR g_fatPathname11[FAT_MAXIMUM_SIZES][FAT_NAME_LIMITTED_SIZE] = { 0 };
CHAR g_fatPathname12[FAT_MAXIMUM_SIZES][FAT_NAME_LIMITTED_SIZE] = { 0 };
CHAR g_fatPathname13[FAT_MAXIMUM_SIZES][FAT_NAME_LIMITTED_SIZE] = { 0 };

FILE *g_fatFfd;

INT32 FatDeleteFile(int fd, char *pathname)
{
    int ret;

    ret = close(fd);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);

    ret = unlink(pathname);
    ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT);

    return FAT_NO_ERROR;
EXIT:
    return FAT_NO_ERROR;
}

INT32 FixWrite(CHAR *path, INT64 file_size, INT32 write_size, INT32 interface_type)
{
    INT32 ret;
    INT32 fd = -1;
    INT64 total = 0;
    INT64 perTime;
    INT64 totalSize = 0;
    INT64 totalTime = 0;
    INT32 cycleCount = 0;
    INT32 taskId;
    struct timeval testTime1;
    struct timeval testTime2;
    DOUBLE testSpeed;
    CHAR *pid = NULL;
    CHAR *writeBuf = NULL;
    FILE *file = NULL;

    taskId = strlen(path);
    pid = path + taskId - 1;
    taskId = atoi(pid);

    writeBuf = (CHAR *)malloc(write_size);
    ICUNIT_GOTO_NOT_EQUAL(writeBuf, nullptr, writeBuf, EXIT);

    gettimeofday(&testTime1, 0);
    if (interface_type == 1) {
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
    perTime = (testTime2.tv_sec - testTime1.tv_sec) * US_PER_SEC + (testTime2.tv_usec - testTime1.tv_usec);
    printf("fix_Write TaskID:%3d,open %s ,task %lld ms ,\n", taskId, path, perTime / MS_PER_SEC);

    gettimeofday(&testTime1, 0);

    while (1) {
        if (interface_type == 1) {
            ret = write(fd, writeBuf, write_size);
            if (ret <= 0) {
                if (errno == ENOSPC) {
                    printf("No space !! %s,\n", strerror(errno));
                    goto EXIT1;
                }
                printf("fix_write fail,path = %s,ret=:%d ,errno=:%d!\n", path, ret, errno);
                goto EXIT1;
            }
        } else {
            ret = fwrite(writeBuf, 1, write_size, file);
            if (ret <= 0 || ret != write_size) {
                if (errno == ENOSPC) {
                    printf("No space !! %s,\n", strerror(errno));
                }
                printf("fix_fwrite error! %s ,path=:%s, ret = %d,\n", strerror(errno), path, ret);

                goto EXIT2;
            }
        }
        total += ret;
        totalSize += ret;
        if (total >= CAL_THRESHOLD) {
            gettimeofday(&testTime2, 0);

            perTime = (testTime2.tv_sec - testTime1.tv_sec) * US_PER_SEC + (testTime2.tv_usec - testTime1.tv_usec);

            totalTime += perTime;

            testSpeed = total * 1.0;
            testSpeed = testSpeed * US_PER_SEC / perTime;
            testSpeed = testSpeed / BYTES_PER_KBYTES / BYTES_PER_KBYTES;

            if (interface_type == 1) {
                printf("fix_Write TaskID:%3d,%d time write, write %lldbytes, cost %lld usecs, speed: %0.3lfMB/s,\n",
                    taskId, cycleCount++, total, perTime, testSpeed);
            } else {
                printf("fix_fwrite TaskID:%3d,%d time write, write %lldbytes, cost %lld usecs, speed: %0.3lfMB/s,\n",
                    taskId, cycleCount++, total, perTime, testSpeed);
            }
            total = 0;
            gettimeofday(&testTime1, 0);
        }

        if (file_size <= totalSize) {
            break;
        }
    }

    gettimeofday(&testTime2, 0);
    perTime = (testTime2.tv_sec - testTime1.tv_sec) * US_PER_SEC + (testTime2.tv_usec - testTime1.tv_usec);

    totalTime += perTime;

    testSpeed = totalSize * 1.0;
    testSpeed = testSpeed * US_PER_SEC / totalTime;
    testSpeed = testSpeed / BYTES_PER_KBYTES / BYTES_PER_KBYTES;

    printf("\nfix_Write TaskID:%3d,total write=%lld , time=%lld,arv speed =%0.3lfMB/s ,\n", taskId, totalSize,
        totalTime, testSpeed);
    gettimeofday(&testTime1, 0);
    if (interface_type == 1) {
        ret = close(fd);
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);
    } else {
        ret = fclose(file);
        ICUNIT_GOTO_EQUAL(ret, FAT_NO_ERROR, ret, EXIT1);
    }

    gettimeofday(&testTime2, 0);
    perTime = (testTime2.tv_sec - testTime1.tv_sec) * US_PER_SEC + (testTime2.tv_usec - testTime1.tv_usec);

    printf("fix_Write TaskID:%3d,sucess to fclose the %s ,task:%lld ms,\n", taskId, path, perTime / MS_PER_SEC);

    free(writeBuf);

    return FAT_NO_ERROR;
EXIT2:
    fclose(file);
    goto EXIT;
EXIT1:
    close(fd);
EXIT:
    free(writeBuf);
    return FAT_NO_ERROR;
}

INT32 FixRead(CHAR *path, INT64 file_size, INT32 read_size, INT32 interface_type)
{
    INT32 fd, taskId, ret;
    INT32 cycleCount = 0;
    INT64 total = 0;
    INT64 totalSize = 0;
    INT64 perTime;
    INT64 totalTime = 0;
    FILE *file = NULL;
    CHAR *readBuf = NULL;
    CHAR *pid = NULL;
    DOUBLE testSpeed;
    struct timeval testTime1;
    struct timeval testTime2;

    taskId = strlen(path);
    pid = path + taskId - 1;
    taskId = atoi(pid);

    readBuf = (CHAR *)malloc(read_size);
    ICUNIT_ASSERT_NOT_EQUAL(readBuf, nullptr, readBuf);

    gettimeofday(&testTime1, 0);

    if (interface_type == 1) {
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

    perTime = (testTime2.tv_sec - testTime1.tv_sec) * US_PER_SEC + (testTime2.tv_usec - testTime1.tv_usec);

    printf("fix_Read TaskID:%3d,open %s , task:%lld ms,\n", taskId, path, perTime / MS_PER_SEC);

    gettimeofday(&testTime1, 0);

    while (1) {
        if (interface_type == 1) {
            ret = read(fd, readBuf, read_size);
            if (ret < 0) {
                printf("ret = %d,%s read fail!-->(X),\n", ret, path);
                goto EXIT1;
            }
            if (!ret) {
                printf("warning: read ret = 0,\n");
                goto EXIT1;
            }
        } else {
            ret = fread(readBuf, 1, read_size, file);
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

        if (total >= CAL_THRESHOLD) {
            gettimeofday(&testTime2, 0);

            perTime = (testTime2.tv_sec - testTime1.tv_sec) * US_PER_SEC + (testTime2.tv_usec - testTime1.tv_usec);

            totalTime += perTime;

            testSpeed = total * 1.0;
            testSpeed = testSpeed * US_PER_SEC / perTime;
            testSpeed = testSpeed / BYTES_PER_KBYTES / BYTES_PER_KBYTES;

            printf("fix_Read TaskID:%3d,times %d, read %lldbytes, cost %lld usecs, speed: %0.3lfMB/s,\n", taskId,
                cycleCount++, total, perTime, testSpeed);

            total = 0;
            gettimeofday(&testTime1, 0);
        }

        if (file_size <= totalSize) {
            break;
        }
    }

    gettimeofday(&testTime2, 0);
    perTime = (testTime2.tv_sec - testTime1.tv_sec) * US_PER_SEC + (testTime2.tv_usec - testTime1.tv_usec);

    totalTime += perTime;

    testSpeed = totalSize * 1.0;
    testSpeed = testSpeed * US_PER_SEC / totalTime;
    testSpeed = testSpeed / BYTES_PER_KBYTES / BYTES_PER_KBYTES;

    printf("\nfix_Read TaskID:%3d,total read=%lld , time=%lld,arv speed =%0.3lfMB/s ,\n", taskId, totalSize, totalTime,
        testSpeed);

    gettimeofday(&testTime1, 0);
    if (interface_type == 1) {
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
    perTime = (testTime2.tv_sec - testTime1.tv_sec) * US_PER_SEC + (testTime2.tv_usec - testTime1.tv_usec);

    printf("fix_Read TaskID:%3d, fclose %s!,task:%lld ms,\n", taskId, path, perTime / MS_PER_SEC);

    ret = remove(path);
    if (ret < 0) {
        printf("fail to remove  %s!\n", strerror(errno));
    }

    printf("Read TaskID:%3d,sucess to fread the %s,\n", taskId, path);
    free(readBuf);
    return 0;
EXIT2:
    fclose(file);
EXIT1:
    close(fd);

    free(readBuf);
    return FAT_NO_ERROR;
}

INT32 RandWrite(CHAR *path, INT64 file_size, INT32 interface_type)
{
    INT32 ret, i, fd;
    INT32 cycleCount = 0;
    INT32 taskId;
    INT64 total = 0;
    INT64 totalSize = 0;
    INT64 perTime;
    INT64 totalTime = 0;
    CHAR *writeBuf = NULL;
    CHAR *pid = NULL;
    struct timeval testTime1;
    struct timeval testTime2;
    DOUBLE testSpeed;
    FILE *file = NULL;

    taskId = strlen(path);
    pid = path + taskId - 1;
    taskId = atoi(pid);

    writeBuf = (CHAR *)malloc(MAX_BUFFER_LEN);
    ICUNIT_GOTO_NOT_EQUAL(writeBuf, nullptr, writeBuf, EXIT);

    gettimeofday(&testTime1, 0);
    if (interface_type == 1) {
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
    perTime = (testTime2.tv_sec - testTime1.tv_sec) * US_PER_SEC + (testTime2.tv_usec - testTime1.tv_usec);
    printf("RandWrite TaskID:%3d,open %s , cost:%lld ms,\n", taskId, path, perTime / MS_PER_SEC);

    gettimeofday(&testTime1, 0);

    i = 0;

    while (1) {
        if (interface_type == 1) {
            ret = write(fd, writeBuf, g_grandSize[i]);
            if (ret <= 0) {
                printf("ret = %d,%s write fail!-->(X),\n", ret, path);
                if (errno == ENOSPC) {
                    printf("No space !! %s,\n", strerror(errno));
                    goto EXIT1;
                }
                goto EXIT1;
            }
        } else {
            ret = fwrite(writeBuf, 1, g_grandSize[i], file);
            if (ret <= 0 || ret != g_grandSize[i]) {
                if (errno == ENOSPC) {
                    printf("No space !! %s,\n", strerror(errno));
                }
                printf("rand_Write TaskID:%3d,fwrite error! %s , ret = %d,\n", taskId, strerror(errno), ret);
                goto EXIT2;
            }
        }

        total += ret;
        totalSize += ret;
        if (total >= CAL_THRESHOLD) {
            gettimeofday(&testTime2, 0);

            perTime = (testTime2.tv_sec - testTime1.tv_sec) * US_PER_SEC + (testTime2.tv_usec - testTime1.tv_usec);

            totalTime += perTime;
            testSpeed = total * 1.0;
            testSpeed = testSpeed * US_PER_SEC / perTime;
            testSpeed = testSpeed / BYTES_PER_KBYTES / BYTES_PER_KBYTES;

            printf("rand_Write TaskID:%3d,%d time write, write %lldbytes, cost %lld usecs, speed: %0.3lfMB/s,\n",
                taskId, cycleCount++, total, perTime, testSpeed);

            total = 0;
            gettimeofday(&testTime1, 0);
        }

        if (file_size <= totalSize) {
            break;
        }
        if (++i >= MAX_DEF_BUF_NUM)
            i = 0;
    }

    gettimeofday(&testTime2, 0);
    perTime = (testTime2.tv_sec - testTime1.tv_sec) * US_PER_SEC + (testTime2.tv_usec - testTime1.tv_usec);

    totalTime += perTime;

    testSpeed = totalSize * 1.0;
    testSpeed = testSpeed * US_PER_SEC / totalTime;
    testSpeed = testSpeed / BYTES_PER_KBYTES / BYTES_PER_KBYTES;
    printf("--------------------------------\n");
    printf("rand_Write TaskID:%3d,total write=%lld , time=%lld,arv speed =%0.3lfMB/s ,\n", taskId, totalSize, totalTime,
        testSpeed);

    gettimeofday(&testTime1, 0);
    if (interface_type == 1) {
        ret = close(fd);
        if (ret < 0) {
            printf("rand_Write TaskID:%3d,fail to close  %s!\n", taskId, strerror(errno));
            goto EXIT1;
        }
    } else {
        ret = fclose(file);
        if (ret < 0) {
            printf("rand_Write TaskID:%3d,fail to fclose  %s!\n", taskId, strerror(errno));
            goto EXIT2;
        }
    }

    gettimeofday(&testTime2, 0);
    perTime = (testTime2.tv_sec - testTime1.tv_sec) * US_PER_SEC + (testTime2.tv_usec - testTime1.tv_usec);

    printf("rand_Write TaskID:%3d,sucess to fclose the %s ,task %lld,\n", taskId, path, perTime / MS_PER_SEC);

    free(writeBuf);

    return FAT_NO_ERROR;
EXIT2:
    fclose(file);
    goto EXIT;
EXIT1:
    close(fd);
EXIT:
    free(writeBuf);
    return FAT_NO_ERROR;
}

INT32 RandRead(CHAR *path, INT64 file_size, INT32 interface_type)
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
    CHAR *pid = NULL;
    CHAR *readBuf = NULL;
    FILE *file = NULL;

    taskId = strlen(path);
    pid = path + taskId - 1;
    taskId = atoi(pid);

    readBuf = (CHAR *)malloc(MAX_BUFFER_LEN);
    ICUNIT_GOTO_NOT_EQUAL(readBuf, nullptr, readBuf, EXIT);

    gettimeofday(&testTime1, 0);
    if (interface_type == 1) {
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
    perTime = (testTime2.tv_sec - testTime1.tv_sec) * US_PER_SEC + (testTime2.tv_usec - testTime1.tv_usec);
    printf("RandRead, open %s , task:%lld ms,\n", path, perTime / MS_PER_SEC);

    i = 0;
    gettimeofday(&testTime1, 0);

    while (1) {
        if (interface_type == 1) {
            ret = read(fd, readBuf, g_grandSize[i]);
            if (ret < 0) {
                printf("ret = %d,%s read fail!-->(X)\n", ret, path);
                goto EXIT1;
            }
            if (!ret) {
                printf("warning: read ret = 0,\n");
            }
        } else {
            ret = fread(readBuf, 1, g_grandSize[i], file);
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

        if (total >= CAL_THRESHOLD) {
            gettimeofday(&testTime2, 0);

            perTime = (testTime2.tv_sec - testTime1.tv_sec) * US_PER_SEC + (testTime2.tv_usec - testTime1.tv_usec);

            totalTime += perTime;

            testSpeed = total * 1.0;
            testSpeed = testSpeed * US_PER_SEC / perTime;
            testSpeed = testSpeed / BYTES_PER_KBYTES / BYTES_PER_KBYTES;

            printf("rand_Read TaskID:%3d, times %d, read %lldbytes, cost %lld usecs, speed: %0.3lfMB/s,\n", taskId,
                cycleCount++, total, perTime, testSpeed);

            total = 0;
            gettimeofday(&testTime1, 0);
        }

        if (file_size <= totalSize) {
            break;
        }
        if (++i >= MAX_DEF_BUF_NUM)
            i = 0;
    }

    gettimeofday(&testTime2, 0);
    perTime = (testTime2.tv_sec - testTime1.tv_sec) * US_PER_SEC + (testTime2.tv_usec - testTime1.tv_usec);

    totalTime += perTime;

    testSpeed = totalSize * 1.0;
    testSpeed = testSpeed * US_PER_SEC / totalTime;
    testSpeed = testSpeed / BYTES_PER_KBYTES / BYTES_PER_KBYTES;
    printf("--------------------------------\n");
    printf("rand_Read TaskID:%3d ,total read=%lld , time=%lld,arv speed =%0.3lfMB/s ,\n", taskId, totalSize, totalTime,
        testSpeed);

    gettimeofday(&testTime1, 0);
    if (interface_type == 1) {
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
    perTime = (testTime2.tv_sec - testTime1.tv_sec) * US_PER_SEC + (testTime2.tv_usec - testTime1.tv_usec);

    printf(" rand_Read TaskID:%3d,fclose %s!,task:%lld ms,\n", taskId, path, perTime / MS_PER_SEC);

    ret = remove(path);
    if (ret < 0) {
        printf("fail to fclose  %s!\n", strerror(errno));
    }

    printf("rand_Read TaskID:%3d,sucess to fread the %s,\n", taskId, path);

    free(readBuf);
    return FAT_NO_ERROR;

EXIT2:
    fclose(file);
    goto EXIT1;
EXIT1:
    close(fd);
EXIT:
    free(readBuf);
    return FAT_NO_ERROR;
}

VOID FatStrcat2(char *pathname, char *str, int len)
{
    (void)memset_s(pathname, len, 0, len);
    (void)strcpy_s(pathname, len, FAT_PATH_NAME);
    (void)strcat_s(pathname, len, str);
}

INT32 FatScandirFree(struct dirent **namelist, int n)
{
    if (n < 0 || namelist == nullptr) {
        return -1;
    } else if (n == 0) {
        free(namelist);
        namelist = nullptr;
        return 0;
    }
    while (n--) {
        free(namelist[n]);
    }
    free(namelist);
    namelist = nullptr;

    return 0;
}

VOID FatStatPrintf(struct stat sb)
{
#if VFS_STAT_PRINTF == 1

    printf("File type:                ");

    switch (sb.st_mode & S_IFMT) {
        case S_IFBLK:
            printf("block device\n");
            break;
        case S_IFCHR:
            printf("character device\n");
            break;
        case S_IFDIR:
            printf("directory\n");
            break;
        case S_IFIFO:
            printf("FIFO/pipe\n");
            break;
        case S_IFLNK:
            printf("symlink\n");
            break;
        case S_IFREG:
            printf("regular file\n");
            break;
        case S_IFSOCK:
            printf("socket\n");
            break;
        default:
            printf("unknown?\n");
            break;
    }

    switch (sb.st_mode & S_IRWXU) {
        case S_IRUSR:
            printf("Owners have read permission\n");
            break;
        case S_IWUSR:
            printf("Owners have write permission\n");
            break;
        case S_IXUSR:
            printf("Owners have execute permissions\n");
            break;
        default:
            break;
    }

    switch (sb.st_mode & S_IRWXG) {
        case S_IRGRP:
            printf("Group has read permission\n");
            break;
        case S_IWGRP:
            printf("Group has write permission\n");
            break;
        case S_IXGRP:
            printf("Group has execute permission\n");
            break;
        default:
            break;
    }

    switch (sb.st_mode & S_IRWXO) {
        case S_IROTH:
            printf("Other have read permission\n");
            break;
        case S_IWOTH:
            printf("Other has write permission\n");
            break;
        case S_IXOTH:
            printf("Other has execute permission\n");
            break;
        default:
            break;
    }

    printf("I-node number:            %ld\n", (long)sb.st_ino);
    printf("Mode:                     %lo (octal)\n", (unsigned long)sb.st_mode);
    printf("Link count:               %ld\n", (long)sb.st_nlink);
    printf("Ownership:                UID=%ld   GID=%ld\n", (long)sb.st_uid, (long)sb.st_gid);

    printf("Preferred I/O block size: %ld bytes\n", (long)sb.st_blksize);
    printf("File size:                %lld bytes\n", (long long)sb.st_size);
    printf("Blocks allocated:         %lld\n", (long long)sb.st_blocks);

    printf("Last status change:       %s", ctime(&sb.st_ctime));
    printf("Last file access:         %s", ctime(&sb.st_atime));
    printf("Last file modification:   %s\n\n", ctime(&sb.st_mtime));

#endif
    return;
}

VOID FatStatfsPrintf(struct statfs buf)
{
#if VFS_STATFS_PRINTF == 1
    printf("type of file system :            0x%x\n", buf.f_type);
    printf("optimal transfer block size :            %ld\n", (long)buf.f_bsize);
    printf("total data blocks in file system :            %ld\n", (long)buf.f_blocks);
    printf("free blocks in fs :            %ld\n", (long)buf.f_bfree);
    printf("free blocks available to unprivileged user :            %ld\n", (long)buf.f_bavail);
    printf("total file nodes in file system :            %ld\n", (long)buf.f_files);

    printf("free file nodes in fs :        %ld\n", (long)buf.f_ffree);
    printf("file system id :            %d\n", buf.f_fsid.__val[0]);
    printf("maximum length of filenames :            0x%x\n", buf.f_namelen);
    printf("fragment size:            %d\n\n", buf.f_frsize);

#endif

    return;
}

using namespace testing::ext;
namespace OHOS {
class VfsFatTest : public testing::Test {
public:
    static void SetUpTestCase(void)
    {
        INT32 ret = 0;
        sleep(3); // 3s
        if (ret != 0)
            perror("format sd card");
        return;
        ret = mkdir("/vs/", S_IRWXU | S_IRWXG | S_IRWXO);
        if (ret != 0) {
            perror("mkdir mount dir");
            return;

            ret = mkdir(FAT_MOUNT_DIR, S_IRWXU | S_IRWXG | S_IRWXO);
            if (ret != 0) {
                perror("mkdir mount dir");
                return;

                ret = mount(FAT_DEV_PATH, FAT_MOUNT_DIR, FAT_FILESYS_TYPE, 0, nullptr);
                if (ret != 0) {
                    perror("mount sd card");
                    return;
                }
            }
        }
    }
    static void TearDownTestCase(void)
    {
        umount(FAT_MOUNT_DIR);
    }
};

#if defined(LOSCFG_USER_TEST_FULL)
HWTEST_F(VfsFatTest, ItFsFat066, TestSize.Level0)
{
    ItFsFat066();
}

HWTEST_F(VfsFatTest, ItFsFat068, TestSize.Level0)
{
    ItFsFat068();
}

HWTEST_F(VfsFatTest, ItFsFat072, TestSize.Level0)
{
    ItFsFat072();
}

HWTEST_F(VfsFatTest, ItFsFat073, TestSize.Level0)
{
    ItFsFat073();
}

HWTEST_F(VfsFatTest, ItFsFat074, TestSize.Level0)
{
    ItFsFat074();
}

HWTEST_F(VfsFatTest, ItFsFat496, TestSize.Level0)
{
    ItFsFat496();
}

HWTEST_F(VfsFatTest, ItFsFat497, TestSize.Level0)
{
    ItFsFat497();
}

HWTEST_F(VfsFatTest, ItFsFat498, TestSize.Level0)
{
    ItFsFat498();
}

HWTEST_F(VfsFatTest, ItFsFat499, TestSize.Level0)
{
    ItFsFat499();
}

HWTEST_F(VfsFatTest, ItFsFat500, TestSize.Level0)
{
    ItFsFat500();
}

HWTEST_F(VfsFatTest, ItFsFat501, TestSize.Level0)
{
    ItFsFat501();
}

HWTEST_F(VfsFatTest, ItFsFat502, TestSize.Level0)
{
    ItFsFat502();
}

HWTEST_F(VfsFatTest, ItFsFat503, TestSize.Level0)
{
    ItFsFat503();
}

HWTEST_F(VfsFatTest, ItFsFat504, TestSize.Level0)
{
    ItFsFat504();
}

HWTEST_F(VfsFatTest, ItFsFat506, TestSize.Level0)
{
    ItFsFat506();
}

HWTEST_F(VfsFatTest, ItFsFat507, TestSize.Level0)
{
    ItFsFat507();
}

HWTEST_F(VfsFatTest, ItFsFat508, TestSize.Level0)
{
    ItFsFat508();
}

HWTEST_F(VfsFatTest, ItFsFat509, TestSize.Level0)
{
    ItFsFat509();
}

HWTEST_F(VfsFatTest, ItFsFat510, TestSize.Level0)
{
    ItFsFat510();
}

HWTEST_F(VfsFatTest, ItFsFat511, TestSize.Level0)
{
    ItFsFat511();
}

HWTEST_F(VfsFatTest, ItFsFat512, TestSize.Level0)
{
    ItFsFat512();
}

HWTEST_F(VfsFatTest, ItFsFat513, TestSize.Level0)
{
    ItFsFat513();
}

HWTEST_F(VfsFatTest, ItFsFat514, TestSize.Level0)
{
    ItFsFat514();
}

HWTEST_F(VfsFatTest, ItFsFat515, TestSize.Level0)
{
    ItFsFat515();
}

HWTEST_F(VfsFatTest, ItFsFat516, TestSize.Level0)
{
    ItFsFat516();
}

HWTEST_F(VfsFatTest, ItFsFat517, TestSize.Level0)
{
    ItFsFat517();
}

HWTEST_F(VfsFatTest, ItFsFat518, TestSize.Level0)
{
    ItFsFat518();
}

HWTEST_F(VfsFatTest, ItFsFat519, TestSize.Level0)
{
    ItFsFat519();
}

HWTEST_F(VfsFatTest, ItFsFat662, TestSize.Level0)
{
    ItFsFat662();
}

HWTEST_F(VfsFatTest, ItFsFat663, TestSize.Level0)
{
    ItFsFat663();
}

HWTEST_F(VfsFatTest, ItFsFat664, TestSize.Level0)
{
    ItFsFat664();
}

HWTEST_F(VfsFatTest, ItFsFat665, TestSize.Level0)
{
    ItFsFat665();
}

HWTEST_F(VfsFatTest, ItFsFat666, TestSize.Level0)
{
    ItFsFat666();
}

HWTEST_F(VfsFatTest, ItFsFat667, TestSize.Level0)
{
    ItFsFat667();
}

HWTEST_F(VfsFatTest, ItFsFat668, TestSize.Level0)
{
    ItFsFat668();
}

HWTEST_F(VfsFatTest, ItFsFat669, TestSize.Level0)
{
    ItFsFat669();
}

HWTEST_F(VfsFatTest, ItFsFat670, TestSize.Level0)
{
    ItFsFat670();
}

HWTEST_F(VfsFatTest, ItFsFat671, TestSize.Level0)
{
    ItFsFat671();
}

HWTEST_F(VfsFatTest, ItFsFat672, TestSize.Level0)
{
    ItFsFat672();
}

HWTEST_F(VfsFatTest, ItFsFat673, TestSize.Level0)
{
    ItFsFat673();
}

HWTEST_F(VfsFatTest, ItFsFat674, TestSize.Level0)
{
    ItFsFat674();
}

HWTEST_F(VfsFatTest, ItFsFat675, TestSize.Level0)
{
    ItFsFat675();
}

HWTEST_F(VfsFatTest, ItFsFat676, TestSize.Level0)
{
    ItFsFat676();
}

HWTEST_F(VfsFatTest, ItFsFat677, TestSize.Level0)
{
    ItFsFat677();
}

HWTEST_F(VfsFatTest, ItFsFat678, TestSize.Level0)
{
    ItFsFat678();
}

HWTEST_F(VfsFatTest, ItFsFat679, TestSize.Level0)
{
    ItFsFat679();
}

HWTEST_F(VfsFatTest, ItFsFat680, TestSize.Level0)
{
    ItFsFat680();
}

HWTEST_F(VfsFatTest, ItFsFat681, TestSize.Level0)
{
    ItFsFat681();
}

HWTEST_F(VfsFatTest, ItFsFat682, TestSize.Level0)
{
    ItFsFat682();
}

HWTEST_F(VfsFatTest, ItFsFat683, TestSize.Level0)
{
    ItFsFat683();
}

HWTEST_F(VfsFatTest, ItFsFat684, TestSize.Level0)
{
    ItFsFat684();
}

HWTEST_F(VfsFatTest, ItFsFat685, TestSize.Level0)
{
    ItFsFat685();
}

HWTEST_F(VfsFatTest, ItFsFat686, TestSize.Level0)
{
    ItFsFat686();
}

HWTEST_F(VfsFatTest, ItFsFat687, TestSize.Level0)
{
    ItFsFat687();
}

HWTEST_F(VfsFatTest, ItFsFat692, TestSize.Level0)
{
    ItFsFat692();
}

HWTEST_F(VfsFatTest, ItFsFat693, TestSize.Level0)
{
    ItFsFat693();
}

HWTEST_F(VfsFatTest, ItFsFat694, TestSize.Level0)
{
    ItFsFat694();
}

HWTEST_F(VfsFatTest, ItFsFat870, TestSize.Level0)
{
    ItFsFat870();
}

HWTEST_F(VfsFatTest, ItFsFat872, TestSize.Level0)
{
    ItFsFat872();
}

HWTEST_F(VfsFatTest, ItFsFat873, TestSize.Level0)
{
    ItFsFat873();
}

HWTEST_F(VfsFatTest, ItFsFat874, TestSize.Level0)
{
    ItFsFat874();
}

HWTEST_F(VfsFatTest, ItFsFat875, TestSize.Level0)
{
    ItFsFat875();
}

HWTEST_F(VfsFatTest, ItFsFat902, TestSize.Level0)
{
    ItFsFat902();
}

HWTEST_F(VfsFatTest, ItFsFat903, TestSize.Level0)
{
    ItFsFat903();
}

HWTEST_F(VfsFatTest, ItFsFat904, TestSize.Level0)
{
    ItFsFat904();
}

HWTEST_F(VfsFatTest, ItFsFat909, TestSize.Level0)
{
    ItFsFat909(); //  plug and unplug during writing or reading
}

#endif
#if defined(LOSCFG_USER_TEST_PRESSURE)
HWTEST_F(VfsFatTest, ItFsFatPressure040, TestSize.Level0)
{
    ItFsFatPressure040();
}

HWTEST_F(VfsFatTest, ItFsFatMutipthread046, TestSize.Level0)
{
    ItFsFatMutipthread046();
}

HWTEST_F(VfsFatTest, ItFsFatMutipthread047, TestSize.Level0)
{
    ItFsFatMutipthread047();
}

HWTEST_F(VfsFatTest, ItFsFatPressure030, TestSize.Level0)
{
    ItFsFatPressure030(); //  time too long
}

#if (FAT_FILE_SYSTEMTYPE_AUTO == FAT_FILE_SYSTEMTYPE_FAT32)
HWTEST_F(VfsFatTest, ItFsFatPressure031, TestSize.Level0)
{
    ItFsFatPressure031();
}
#endif

HWTEST_F(VfsFatTest, ItFsFatPressure038, TestSize.Level0)
{
    ItFsFatPressure038();
}

HWTEST_F(VfsFatTest, ItFsFatPressure041, TestSize.Level0)
{
    ItFsFatPressure041();
}

HWTEST_F(VfsFatTest, ItFsFatPressure042, TestSize.Level0)
{
    ItFsFatPressure042();
}

HWTEST_F(VfsFatTest, ItFsFatPressure301, TestSize.Level0)
{
    ItFsFatPressure301();
}

HWTEST_F(VfsFatTest, ItFsFatPressure302, TestSize.Level0)
{
    ItFsFatPressure302();
}

HWTEST_F(VfsFatTest, ItFsFatPressure303, TestSize.Level0)
{
    ItFsFatPressure303();
}

HWTEST_F(VfsFatTest, ItFsFatPressure305, TestSize.Level0)
{
    ItFsFatPressure305();
}

HWTEST_F(VfsFatTest, ItFsFatPressure306, TestSize.Level0)
{
    ItFsFatPressure306();
}

HWTEST_F(VfsFatTest, ItFsFatPressure309, TestSize.Level0)
{
    ItFsFatPressure309();
}

HWTEST_F(VfsFatTest, ItFsFatMutipthread003, TestSize.Level0)
{
    ItFsFatMutipthread003();
}

HWTEST_F(VfsFatTest, ItFsFatMutipthread004, TestSize.Level0)
{
    ItFsFatMutipthread004();
}

HWTEST_F(VfsFatTest, ItFsFatMutipthread005, TestSize.Level0)
{
    ItFsFatMutipthread005();
}

HWTEST_F(VfsFatTest, ItFsFatMutipthread006, TestSize.Level0)
{
    ItFsFatMutipthread006();
}

HWTEST_F(VfsFatTest, ItFsFatMutipthread008, TestSize.Level0)
{
    ItFsFatMutipthread008();
}

HWTEST_F(VfsFatTest, ItFsFatMutipthread009, TestSize.Level0)
{
    ItFsFatMutipthread009();
}

HWTEST_F(VfsFatTest, ItFsFatMutipthread010, TestSize.Level0)
{
    ItFsFatMutipthread010();
}

HWTEST_F(VfsFatTest, ItFsFatMutipthread012, TestSize.Level0)
{
    ItFsFatMutipthread012();
}

HWTEST_F(VfsFatTest, ItFsFatMutipthread014, TestSize.Level0)
{
    ItFsFatMutipthread014();
}

HWTEST_F(VfsFatTest, ItFsFatMutipthread016, TestSize.Level0)
{
    ItFsFatMutipthread016();
}

HWTEST_F(VfsFatTest, ItFsFatMutipthread017, TestSize.Level0)
{
    ItFsFatMutipthread017();
}

HWTEST_F(VfsFatTest, ItFsFatMutipthread018, TestSize.Level0)
{
    ItFsFatMutipthread018();
}

HWTEST_F(VfsFatTest, ItFsFatMutipthread019, TestSize.Level0)
{
    ItFsFatMutipthread019();
}

HWTEST_F(VfsFatTest, ItFsFatMutipthread020, TestSize.Level0)
{
    ItFsFatMutipthread020();
}

HWTEST_F(VfsFatTest, ItFsFatMutipthread021, TestSize.Level0)
{
    ItFsFatMutipthread021();
}

HWTEST_F(VfsFatTest, ItFsFatMutipthread022, TestSize.Level0)
{
    ItFsFatMutipthread022();
}

HWTEST_F(VfsFatTest, ItFsFatMutipthread023, TestSize.Level0)
{
    ItFsFatMutipthread023();
}

HWTEST_F(VfsFatTest, ItFsFatMutipthread024, TestSize.Level0)
{
    ItFsFatMutipthread024();
}

HWTEST_F(VfsFatTest, ItFsFatMutipthread027, TestSize.Level0)
{
    ItFsFatMutipthread027();
}

HWTEST_F(VfsFatTest, ItFsFatMutipthread029, TestSize.Level0)
{
    ItFsFatMutipthread029();
}

HWTEST_F(VfsFatTest, ItFsFatMutipthread030, TestSize.Level0)
{
    ItFsFatMutipthread030();
}

HWTEST_F(VfsFatTest, ItFsFatMutipthread032, TestSize.Level0)
{
    ItFsFatMutipthread032();
}

HWTEST_F(VfsFatTest, ItFsFatMutipthread033, TestSize.Level0)
{
    ItFsFatMutipthread033();
}

HWTEST_F(VfsFatTest, ItFsFatMutipthread035, TestSize.Level0)
{
    ItFsFatMutipthread035();
}

HWTEST_F(VfsFatTest, ItFsFatMutipthread036, TestSize.Level0)
{
    ItFsFatMutipthread036();
}

HWTEST_F(VfsFatTest, ItFsFatMutipthread038, TestSize.Level0)
{
    ItFsFatMutipthread038();
}

HWTEST_F(VfsFatTest, ItFsFatMutipthread039, TestSize.Level0)
{
    ItFsFatMutipthread039();
}

HWTEST_F(VfsFatTest, ItFsFatMutipthread040, TestSize.Level0)
{
    ItFsFatMutipthread040();
}

HWTEST_F(VfsFatTest, ItFsFatMutipthread041, TestSize.Level0)
{
    ItFsFatMutipthread041();
}

HWTEST_F(VfsFatTest, ItFsFatMutipthread042, TestSize.Level0)
{
    ItFsFatMutipthread042();
}

HWTEST_F(VfsFatTest, ItFsFatMutipthread043, TestSize.Level0)
{
    ItFsFatMutipthread043();
}

HWTEST_F(VfsFatTest, ItFsFatMutipthread044, TestSize.Level0)
{
    ItFsFatMutipthread044();
}

HWTEST_F(VfsFatTest, ItFsFatMutipthread045, TestSize.Level0)
{
    ItFsFatMutipthread045();
}

HWTEST_F(VfsFatTest, ItFsFatMutipthread048, TestSize.Level0)
{
    ItFsFatMutipthread048();
}

HWTEST_F(VfsFatTest, ItFsFatMutipthread049, TestSize.Level0)
{
    ItFsFatMutipthread049();
}

HWTEST_F(VfsFatTest, ItFsFatMutipthread050, TestSize.Level0)
{
    ItFsFatMutipthread050();
}

HWTEST_F(VfsFatTest, ItFsFatPerformance013, TestSize.Level0)
{
    ItFsFatPerformance013();
}

HWTEST_F(VfsFatTest, ItFsFatPerformance014, TestSize.Level0)
{
    ItFsFatPerformance014();
}

HWTEST_F(VfsFatTest, ItFsFatPerformance015, TestSize.Level0)
{
    ItFsFatPerformance015();
}

HWTEST_F(VfsFatTest, ItFsFatPerformance016, TestSize.Level0)
{
    ItFsFatPerformance016(); //  Multi-threaded takes time to delete directory files When the fourth
}

HWTEST_F(VfsFatTest, ItFsFatPerformance001, TestSize.Level0)
{
    ItFsFatPerformance001(); //  rand fwrite and fread for one pthread
}

HWTEST_F(VfsFatTest, ItFsFatPerformance006, TestSize.Level0)
{
    ItFsFatPerformance006(); //  rand fwrite and fread for three pthread
}

HWTEST_F(VfsFatTest, ItFsFatPerformance002, TestSize.Level0)
{
    ItFsFatPerformance002(); //  fix fwrite and fread for one pthread
}

HWTEST_F(VfsFatTest, ItFsFatPerformance005, TestSize.Level0)
{
    ItFsFatPerformance005(); //  fix fwrite and fread for three pthread
}

HWTEST_F(VfsFatTest, ItFsFatPerformance003, TestSize.Level0)
{
    ItFsFatPerformance003(); //  rand write and read for one pthread
}

HWTEST_F(VfsFatTest, ItFsFatPerformance004, TestSize.Level0)
{
    ItFsFatPerformance004(); //  fix write and read for one pthread
}

HWTEST_F(VfsFatTest, ItFsFatPerformance007, TestSize.Level0)
{
    ItFsFatPerformance007(); //  rand write and read for three pthread
}

HWTEST_F(VfsFatTest, ItFsFatPerformance008, TestSize.Level0)
{
    ItFsFatPerformance008(); //  fix write and read for three pthread
}

#endif
#if defined(LOSCFG_USER_TEST_SMOKE)
HWTEST_F(VfsFatTest, ItFsFat026, TestSize.Level0)
{
    ItFsFat026(); //  statfs Unsupport
}
#endif
} // namespace OHOS
