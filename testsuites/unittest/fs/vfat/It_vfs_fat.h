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

#ifndef IT_VFS_FAT_H
#define IT_VFS_FAT_H

#include "osTest.h"
#include "sys/uio.h"
#include "utime.h"
#include <sys/types.h>
#include <fcntl.h>
#include <time.h>
#include <pthread.h>
#include <signal.h>
#include <semaphore.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/mount.h>
#include <limits.h>
#include <sys/prctl.h>

#ifndef _GNU_SOURCE
struct timeval {
    time_t tv_sec;
    suseconds_t tv_usec;
};
#endif
#define F_RDO 0x01 /* Read only */
#define F_HID 0x02 /* Hidden */
#define F_SYS 0x04 /* System */
#define F_ARC 0x20 /* Archive */
#define NAME_MAX 255

extern int Chattr(const char *path, mode_t mode); //
using off_t = off64_t;

#define SD_MOUNT_DIR "/vs/sd"
#define FAT_MOUNT_DIR SD_MOUNT_DIR
#define FAT_MAIN_DIR SD_MOUNT_DIR
#define FAT_PATH_NAME "/vs/sd/test"
#define FAT_PATH_NAME0 "/vs/sd/test0"
#define FAT_PATH_NAME1 "/vs/sd/test1"
#define FAT_PATH_NAME2 "/vs/sd/test2"
#define FAT_PATH_NAME00 "/vs/sd/test0/test00"
#define FAT_PATH_NAME11 "/vs/sd/test1/test11"
#define FAT_PATH_NAME22 "/vs/sd/test2/test22"
#define FAT_PATH_NAME_0 "/vs/sd/test/test0"
#define FAT_PATH_NAME_01 "/vs/sd/test/test0/test1"
#define FAT_DEV_PATH "/dev/mmcblk0"
#define FAT_DEV_PATH1 "/dev/mmcblk0p0"
#define FAT_FILESYS_TYPE "vfat"
#define FAT_CHINESE_NAME1 "ÔçÉÏºÃ"

#define FAT_FALLOCATE_KEEP_SIZE 0x1 // FALLOC_FL_KEEP_SIZE
#define FAT_FALLOCATE_NO_KEEP_SIZE 0

#define FAT_NAME_LIMITTED_SIZE 300
#define FAT_FILE_LIMITTED_NUM 200
#define FAT_STANDARD_NAME_LENGTH 50
#define FAT_SHORT_ARRAY_LENGTH 10
#define FAT_LONG_ARRAY_LENGTH 100
#define MAX_DEF_BUF_NUM 21
#define FIX_DATA_LEN 524288
#define MAX_BUFFER_LEN FIX_DATA_LEN // 9000000  //the mem is not enough
#define CAL_THRESHOLD_VALUE 0x1900000
#define FAT_WR_CAP_SIZE_TEST 0x1900000 // the size is 25MB for each speed calculation
#define MAX_THREAD_NUM 10
#define FAT_PTHREAD_PRIORITY_TEST1 4
#define CONFIG_NFILE_DESCRIPTORS 512

#define PRINTF_OUT dprintf
#define PRINTF_HELP()                                                                           \
    do {                                                                                        \
        PRINTF_OUT(" usage: testfile [rand/fix/quit] [file size] [file num] [write/fwrite]\n"); \
    } while (0);

#define FILE_SIZE (500 * BYTES_PER_MBYTES)
#define FAT_PERFORMANCE_FILE_SIZE (500 * BYTES_PER_MBYTES)
#define INTERFACE_TYPE 0    // 0:use fwrite and fread for test
#define FAT_WR_TYPE_TEST1 0 // 0:use fwrite and fread for test
#define FAT_WR_TYPE_TEST2 1 // 0:use fwrite and fread for test
#define FAT_FILE_FIX_WRITE_SIZE 5 * BYTES_PER_MBYTES

/* These are same as the value in the fs/vfat/include/ff.h */
#define FAT_FILE_SYSTEMTYPE_FAT 0x01   // FM_FAT
#define FAT_FILE_SYSTEMTYPE_FAT32 0x02 // FM_FAT32
#define FAT_FILE_SYSTEMTYPE_EXFAT 0x04 // FM_EXFAT
#define FAT_FILE_SYSTEMTYPE_ANY 0x07   // FM_ANY
#define FAT_FILE_SYSTEMTYPE_SFD 0x08   // FM_SFD

/* For the different test scenes:FAT32 and EXFAT */
#define FAT_FILE_SYSTEMTYPE_AUTO g_fatFilesystem

#define FAT_MAX_NUM_TEST 1000
#define FAT_MOUNT_CYCLES_TEST 10000
#define FAT_PRESSURE_CYCLES 10
#define FAT_MAXIMUM_OPERATIONS 10
#define FAT_MAXIMUM_SIZES 10
#define FAT_FILEMUM_NUM 100
#define FAT_MAX_THREADS 3
#define FAT_NO_ERROR 0
#define FAT_IS_ERROR -1
#define FAT_TO_NULL NULL
#define FAT_LONG_FILESIZE 5
#define FAT_CREATFILE_NUM 5
#define FAT_MIDDLE_CYCLES 10
#define FAT_MAX_CYCLES 100

#define BYTES_PER_KBYTES 1024
#define BYTES_PER_MBYTES (1024 * 1024)
#define US_PER_SEC 1000000

#endif

extern INT32 g_fatFilesMax;
extern INT32 g_fatFlag;
extern INT32 g_fatFlagF01;
extern INT32 g_fatFlagF02;
extern INT32 g_fatFd;
extern FILE *g_fatFfd;

extern DIR *g_fatDir;

extern INT32 g_fatFd11[FAT_MAXIMUM_SIZES];
extern INT32 g_fatFd12[FAT_MAXIMUM_SIZES][FAT_MAXIMUM_SIZES];

extern CHAR g_fatPathname1[FAT_STANDARD_NAME_LENGTH];
extern CHAR g_fatPathname2[FAT_STANDARD_NAME_LENGTH];
extern CHAR g_fatPathname3[FAT_STANDARD_NAME_LENGTH];

extern CHAR g_fatPathname6[FAT_NAME_LIMITTED_SIZE];
extern CHAR g_fatPathname7[FAT_NAME_LIMITTED_SIZE];
extern CHAR g_fatPathname8[FAT_NAME_LIMITTED_SIZE];

extern CHAR g_fatPathname11[FAT_MAXIMUM_SIZES][FAT_NAME_LIMITTED_SIZE];
extern CHAR g_fatPathname12[FAT_MAXIMUM_SIZES][FAT_NAME_LIMITTED_SIZE];
extern CHAR g_fatPathname13[FAT_MAXIMUM_SIZES][FAT_NAME_LIMITTED_SIZE];

extern UINT32 g_fatMuxHandle1;
extern UINT32 g_fatMuxHandle2;

extern pthread_mutex_t g_vfatGlobalLock1;
extern pthread_mutex_t g_vfatGlobalLock2;

VOID FatStrcat2(char *pathname, char *str, int len);
INT32 FatScandirFree(struct dirent **namelist, int n);
VOID FatStatPrintf(struct stat sb);
VOID FatStatfsPrintf(struct statfs buf);

INT32 FixWrite(CHAR *path, INT64 file_size, INT32 write_size, INT32 interface_type);
INT32 FixRead(CHAR *path, INT64 file_size, INT32 read_size, INT32 interface_type);
INT32 RandWrite(CHAR *path, INT64 file_size, INT32 interface_type);
INT32 RandRead(CHAR *path, INT64 file_size, INT32 interface_type);

extern INT32 g_grandSize[MAX_DEF_BUF_NUM];
extern struct iovec g_fatIov[FAT_SHORT_ARRAY_LENGTH];

extern UINT32 g_testTaskId01;
extern UINT32 g_testTaskId02;
extern UINT32 g_fatFilesystem;

extern const INT32 CAL_THRESHOLD;
extern INT32 FatDeleteFile(int fd, char *pathname);

#if defined(LOSCFG_USER_TEST_SMOKE)
VOID ItFsFat026(VOID);
#endif

#if defined(LOSCFG_USER_TEST_FULL)
VOID ItFsFat066(VOID);
VOID ItFsFat068(VOID);
VOID ItFsFat072(VOID);
VOID ItFsFat073(VOID);
VOID ItFsFat074(VOID);
VOID ItFsFat496(VOID);
VOID ItFsFat497(VOID);
VOID ItFsFat498(VOID);
VOID ItFsFat499(VOID);
VOID ItFsFat500(VOID);
VOID ItFsFat501(VOID);
VOID ItFsFat502(VOID);
VOID ItFsFat503(VOID);
VOID ItFsFat504(VOID);
VOID ItFsFat506(VOID);
VOID ItFsFat507(VOID);
VOID ItFsFat508(VOID);
VOID ItFsFat509(VOID);
VOID ItFsFat510(VOID);
VOID ItFsFat511(VOID);
VOID ItFsFat512(VOID);
VOID ItFsFat513(VOID);
VOID ItFsFat514(VOID);
VOID ItFsFat515(VOID);
VOID ItFsFat516(VOID);
VOID ItFsFat517(VOID);
VOID ItFsFat518(VOID);
VOID ItFsFat519(VOID);
VOID ItFsFat662(VOID);
VOID ItFsFat663(VOID);
VOID ItFsFat664(VOID);
VOID ItFsFat665(VOID);
VOID ItFsFat666(VOID);
VOID ItFsFat667(VOID);
VOID ItFsFat668(VOID);
VOID ItFsFat669(VOID);
VOID ItFsFat670(VOID);
VOID ItFsFat671(VOID);
VOID ItFsFat672(VOID);
VOID ItFsFat673(VOID);
VOID ItFsFat674(VOID);
VOID ItFsFat675(VOID);
VOID ItFsFat676(VOID);
VOID ItFsFat677(VOID);
VOID ItFsFat678(VOID);
VOID ItFsFat679(VOID);
VOID ItFsFat680(VOID);
VOID ItFsFat681(VOID);
VOID ItFsFat682(VOID);
VOID ItFsFat683(VOID);
VOID ItFsFat684(VOID);
VOID ItFsFat685(VOID);
VOID ItFsFat686(VOID);
VOID ItFsFat687(VOID);
VOID ItFsFat692(VOID);
VOID ItFsFat693(VOID);
VOID ItFsFat694(VOID);
VOID ItFsFat870(VOID);
VOID ItFsFat872(VOID);
VOID ItFsFat873(VOID);
VOID ItFsFat874(VOID);
VOID ItFsFat875(VOID);
VOID ItFsFat902(VOID);
VOID ItFsFat903(VOID);
VOID ItFsFat904(VOID);
VOID ItFsFat909(VOID);

#endif

#if defined(LOSCFG_USER_TEST_PRESSURE)
VOID ItFsFatMutipthread003(VOID);
VOID ItFsFatMutipthread004(VOID);
VOID ItFsFatMutipthread005(VOID);
VOID ItFsFatMutipthread006(VOID);
VOID ItFsFatMutipthread008(VOID);
VOID ItFsFatMutipthread009(VOID);
VOID ItFsFatMutipthread010(VOID);
VOID ItFsFatMutipthread012(VOID);
VOID ItFsFatMutipthread014(VOID);
VOID ItFsFatMutipthread016(VOID);
VOID ItFsFatMutipthread017(VOID);
VOID ItFsFatMutipthread018(VOID);
VOID ItFsFatMutipthread019(VOID);
VOID ItFsFatMutipthread020(VOID);
VOID ItFsFatMutipthread021(VOID);
VOID ItFsFatMutipthread022(VOID);
VOID ItFsFatMutipthread023(VOID);
VOID ItFsFatMutipthread024(VOID);
VOID ItFsFatMutipthread027(VOID);
VOID ItFsFatMutipthread029(VOID);
VOID ItFsFatMutipthread030(VOID);
VOID ItFsFatMutipthread032(VOID);
VOID ItFsFatMutipthread033(VOID);
VOID ItFsFatMutipthread035(VOID);
VOID ItFsFatMutipthread036(VOID);
VOID ItFsFatMutipthread038(VOID);
VOID ItFsFatMutipthread039(VOID);
VOID ItFsFatMutipthread040(VOID);
VOID ItFsFatMutipthread041(VOID);
VOID ItFsFatMutipthread042(VOID);
VOID ItFsFatMutipthread043(VOID);
VOID ItFsFatMutipthread044(VOID);
VOID ItFsFatMutipthread045(VOID);
VOID ItFsFatMutipthread046(VOID);
VOID ItFsFatMutipthread047(VOID);
VOID ItFsFatMutipthread048(VOID);
VOID ItFsFatMutipthread049(VOID);
VOID ItFsFatMutipthread050(VOID);

VOID ItFsFatPressure029(VOID);
VOID ItFsFatPressure030(VOID);
VOID ItFsFatPressure031(VOID);
VOID ItFsFatPressure038(VOID);
VOID ItFsFatPressure040(VOID);
VOID ItFsFatPressure041(VOID);
VOID ItFsFatPressure042(VOID);

VOID ItFsFatPressure301(VOID);
VOID ItFsFatPressure302(VOID);
VOID ItFsFatPressure303(VOID);
VOID ItFsFatPressure305(VOID);
VOID ItFsFatPressure306(VOID);
VOID ItFsFatPressure309(VOID);

VOID ItFsFatPerformance013(VOID);
VOID ItFsFatPerformance014(VOID);
VOID ItFsFatPerformance015(VOID);
VOID ItFsFatPerformance016(VOID);
VOID ItFsFatPerformance001(VOID);
VOID ItFsFatPerformance002(VOID);
VOID ItFsFatPerformance003(VOID);
VOID ItFsFatPerformance004(VOID);
VOID ItFsFatPerformance005(VOID);
VOID ItFsFatPerformance006(VOID);
VOID ItFsFatPerformance007(VOID);
VOID ItFsFatPerformance008(VOID);

#endif
