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
#include <stdio.h>
#include <sys/prctl.h>
#include "sys/capability.h"
#include "sys/ioctl.h"
#include "sys/select.h"
#include "sys/stat.h"
#include "sys/statfs.h"
#include "sys/types.h"
#include "sys/uio.h"
#include "syslog.h"

#include "fcntl.h"
#include "stdlib.h"
#include "time.h"
#include "unistd.h"
#include "utime.h"

#include "dirent.h"
#include "ftw.h"
#include "libgen.h"
#include "limits.h"
#include "los_typedef.h"
#include "osTest.h"
#include "pthread.h"
#include "sched.h"
#include "semaphore.h"
#include "signal.h"
#include "wchar.h"

constexpr int F_RDO = 0x01; /* Read only */
constexpr int F_HID = 0x02; /* Hidden */
constexpr int F_SYS = 0x04; /* System */
constexpr int F_ARC = 0x20; /* Archive */
constexpr int CONFIG_NFILE_DESCRIPTORS = 512;
constexpr int LOS_WAIT_FOREVER = 0xFFFFFFFF;
#if 0
constexpr int DT_UNKNOWN = 0;
constexpr int DT_FIFO = 1;
constexpr int DT_CHR = 2;
constexpr int DT_DIR = 4;
constexpr int DT_BLK = 6;
constexpr int DT_REG = 8;
constexpr int DT_LNK = 10;
constexpr int DT_SOCK = 12;
constexpr int DT_WHT = 14;
constexpr int MS_RDONLY = 1;
#endif

#if 0
constexpr const char* JFFS_MAIN_DIR0 = "/storage";
constexpr const char* JFFS_MOUNT_DIR0 = "/storage";
constexpr const char* JFFS_MAIN_DIR1 = "/storage1";
constexpr const char* JFFS_PATH_NAME0 = "/storage/test";
constexpr const char* JFFS_PATH_NAME01 = "/storage/test1";
constexpr const char* JFFS_PATH_NAME02 = "/storage/test2";
constexpr const char* JFFS_PATH_NAME00 = "/storage/test/test00";
constexpr const char* JFFS_PATH_NAME11 = "/storage/test1/test11";
constexpr const char* JFFS_PATH_NAME22 = "/storage/test2/test22";
constexpr const char* JFFS_PATH_NAME_0 = "/storage/test/test0";
constexpr const char* JFFS_PATH_NAME_01 = "/storage/test/test0/test1";
constexpr const char* JFFS_PATH_NAME1 = "/storage1/test";
constexpr const char* JFFS_DEV_PATH0 = "/dev/spinorblk2";
constexpr const char* JFFS_DEV_PATH1 = "/dev/spinorblk1";
constexpr const char* JFFS_FILESYS_TYPE = "jffs2";
constexpr const char* JFFS_CHINESE_NAME1 = "���Ϻ�";
constexpr const char* JFFS_BASE_DIR = "/";
#endif

#define JFFS_MAIN_DIR0 "/storage"
#define JFFS_MOUNT_DIR0 "/storage"
#define JFFS_MAIN_DIR1 "/storage1"
#define JFFS_PATH_NAME0 "/storage/test"
#define JFFS_PATH_NAME01 "/storage/test1"
#define JFFS_PATH_NAME02 "/storage/test2"
#define JFFS_PATH_NAME03 "/storage/test3"
#define JFFS_PATH_NAME04 "/storage/test4"
#define JFFS_PATH_NAME00 "/storage/test/test00"
#define JFFS_PATH_NAME11 "/storage/test1/test11"
#define JFFS_PATH_NAME22 "/storage/test2/test22"
#define JFFS_PATH_NAME_0 "/storage/test/test0"
#define JFFS_PATH_NAME_01 "/storage/test/test0/test1"
#define JFFS_PATH_NAME1 "/storage1/test"
#define JFFS_DEV_PATH0 "/dev/spinorblk2"
#define JFFS_DEV_PATH1 "/dev/spinorblk1"
#define JFFS_FILESYS_TYPE "jffs2"
#define JFFS_CHINESE_NAME1 "���Ϻ�"
#define JFFS_BASE_DIR "/"

constexpr int MSECS_PER_SEC = 1000;
constexpr int USECS_PER_SEC = 1000000;
constexpr int BYTES_PER_KBYTE = 1024;
constexpr int BYTES_PER_MBYTE = (1024 * 1024);
constexpr int BYTES_PER_GBYTES = (1024 * 1024 * 1024);

constexpr int FILE_BUF_SIZE = 260;
constexpr int HIGHEST_AUTHORITY = 0777;
constexpr int JFFS_MAX_NUM_TEST = 1000;
constexpr int JFFS_NAME_LIMITTED_SIZE = 300;
constexpr int JFFS_STANDARD_NAME_LENGTH = 50;

constexpr int JFFS_LONG_ARRAY_LENGTH = 100;
constexpr int JFFS_MIDDLE_ARRAY_LENGTH = 50;
constexpr int JFFS_SHORT_ARRAY_LENGTH = 10;
constexpr int JFFS_WR_CAP_SIZE_TEST = 0x100000; // the size is 1MB for each speed calculation
constexpr int JFFS_MAX_DEF_BUF_NUM = 21;

constexpr int JFFS_FILE_LIMITTED_NUM = 200;

constexpr int JFFS_FILE_SIZE_TEST = 100 * 1024 * 1024; // *1024
constexpr int JFFS_PERFORMANCE_W_R_SIZE = 5 * 1024 * 1024;
constexpr int JFFS_FILE_PER_WRITE_SIZE = 5 * 1024 * 1024;
constexpr int JFFS_PRESSURE_W_R_SIZE1 = 1 * 1024 * 1024;
constexpr int JFFS_PRESSURE_W_R_SIZE2 = 5 * 1024 * 1024;
constexpr int JFFS_PRESSURE_W_R_SIZE3 = 10 * 1024 * 1024;
constexpr int JFFS_PRESSURE_W_R_SIZE4 = 50 * 1024 * 1024;
constexpr int JFFS_PRESSURE_W_R_SIZE5 = 100 * 1024 * 1024;
constexpr int JFFS_PRESSURE_W_R_SIZE = JFFS_PRESSURE_W_R_SIZE3;
constexpr int JFFS_THREAD_NUM_TEST = 3;

constexpr int SLEEP_TIME = 10;
constexpr int TASK_PRIO_TEST2 = TASK_PRIO_TEST - 2;

constexpr int JFFS_MIDDLE_CYCLES = 3;
constexpr int JFFS_MIDDLE_NUM = 6;
constexpr int JFFS_MAX_CYCLES = 5;
constexpr int JFFS_CREATFILE_NUM = 5;
constexpr int JFFS_WR_TYPE_TEST1 = 0; // 0:use fwrite and fread for test
constexpr int JFFS_WR_TYPE_TEST2 = 1; // 1:use write and read for test
constexpr int JFFS_WR_TYPE_TEST = JFFS_WR_TYPE_TEST2;

constexpr int JFFS_PRESSURE_CYCLES = 10;
constexpr int JFFS_MAXIMUM_OPERATIONS = 10;
constexpr int JFFS_MAXIMUM_SIZES = 10;
constexpr int JFFS_MAX_THREADS = 3;
#define JFFS_NO_ERROR 0
#define JFFS_IS_ERROR -1
#define JFFS_TO_NULL NULL

constexpr int JFFS_UTIME_SUPPORT = -1; // 0 means utime is supported,-1 means utime is not supported
constexpr int JFFS_BASE_ADDR = 0x1900000;
constexpr int JFFS_MTD_SIZE = 0x200000;
constexpr int JFFS_SECOND_MDT = 1;

constexpr int JFFS2NUM_JFFS2_GC_THREAD_PRIORITY = 10;
constexpr int JFFS2NUM_JFFS2_GC_THREAD_PRIORITY1 = JFFS2NUM_JFFS2_GC_THREAD_PRIORITY - 1;
constexpr int JFFS2NUM_JFFS2_GC_THREAD_PRIORITY2 = JFFS2NUM_JFFS2_GC_THREAD_PRIORITY - 2;
constexpr int JFFS2NUM_JFFS2_GC_THREAD_PRIORITY3 = JFFS2NUM_JFFS2_GC_THREAD_PRIORITY - 3;
constexpr int JFFS2NUM_JFFS2_GC_THREAD_PRIORITY4 = JFFS2NUM_JFFS2_GC_THREAD_PRIORITY - 4;
constexpr int JFFS2NUM_JFFS2_GC_THREAD_PRIORITY5 = JFFS2NUM_JFFS2_GC_THREAD_PRIORITY - 5;


constexpr int JFFS_PRESSURE_MTD_SIZE = 0x6A00000; /* The size is 106MB */
#if defined TEST3516A || defined TEST3516EV200
constexpr int JFFS_PRESSURE_ADDRESS_BEGIN = 0xe00000;
constexpr int JFFS_PRESSURE_ADDRESS_END = 0X7f00000;
#elif defined TEST3518E || defined TEST3516CV300
constexpr int JFFS_PRESSURE_ADDRESS_BEGIN = 0xe00000;
constexpr int JFFS_PRESSURE_ADDRESS_END = 0X7f00000;
#endif

extern INT32 g_jffsFilesMax;
extern INT32 g_jffsFlag;
extern INT32 g_jffsFlagF01;
extern INT32 g_jffsFlagF02;
extern INT32 g_jffsFd;
extern DIR *g_jffsDir;
extern BOOL isCpuAffiMaskTest;
extern INT32 g_TestCnt;

extern INT32 g_jffsFd11[JFFS_MAXIMUM_SIZES];
extern INT32 g_jffsFd12[JFFS_MAXIMUM_SIZES][JFFS_MAXIMUM_SIZES];

extern CHAR g_jffsPathname1[JFFS_STANDARD_NAME_LENGTH];
extern CHAR g_jffsPathname2[JFFS_STANDARD_NAME_LENGTH];
extern CHAR g_jffsPathname3[JFFS_STANDARD_NAME_LENGTH];
extern CHAR g_jffsPathname4[JFFS_STANDARD_NAME_LENGTH];

extern CHAR g_jffsPathname6[JFFS_NAME_LIMITTED_SIZE];
extern CHAR g_jffsPathname7[JFFS_NAME_LIMITTED_SIZE];
extern CHAR g_jffsPathname8[JFFS_NAME_LIMITTED_SIZE];

extern CHAR g_jffsPathname11[JFFS_MAXIMUM_SIZES][JFFS_NAME_LIMITTED_SIZE];
extern CHAR g_jffsPathname12[JFFS_MAXIMUM_SIZES][JFFS_NAME_LIMITTED_SIZE];
extern CHAR g_jffsPathname13[JFFS_MAXIMUM_SIZES][JFFS_NAME_LIMITTED_SIZE];

extern struct iovec g_jffsIov[10];

extern pthread_mutex_t g_jffs2GlobalLock1;
extern pthread_mutex_t g_jffs2GlobalLock2;

INT32 JffsFixWrite(CHAR *path, INT64 fileSize, INT32 writeSize, INT32 interfaceType);
INT32 JffsFixRead(CHAR *path, INT64 fileSize, INT32 readSize, INT32 interfaceType);
INT32 JffsMultiWrite(CHAR *path, INT64 fileSize, INT32 writeSize, int oflags, INT32 interfaceType);
INT32 JffsMultiRead(CHAR *path, INT64 fileSize, INT32 readSize, int oflags, INT32 interfaceType);
INT32 JffsRandWrite(CHAR *path, INT64 fileSize, INT32 interfaceType);
INT32 JffsRandRead(CHAR *path, INT64 fileSize, INT32 interfaceType);

INT32 JffsDeleteDirs(char *str, int n);
INT32 JffsDeletefile(int fd, char *pathname);
INT32 JffsStrcat2(char *pathname, char *str, int len);
INT32 JffsScandirFree(struct dirent **namelist, int n);
INT32 JffsStatPrintf(struct stat sb);
INT32 JffsStatfsPrintf(struct statfs buf);
INT32 JffsStat64Printf(struct stat64 sb);

extern int open64(const char *__path, int __oflag, ...);
extern int fcntl64(int fd, int cmd, ...);

extern int osShellCmdPartitionShow(int argc, char **argv);

extern int alphasort(const struct dirent **a, const struct dirent **b);
extern int mount(const char *source, const char *target, const char *filesystemtype, unsigned long mountflags,
    const void *data);
extern int umount(const char *target);
extern bool IS_MOUNTPT(const char *dev);
extern int chattr(const char *path, mode_t mode);

extern VOID ItSuite_Vfs_Jffs(VOID);

#if defined(LOSCFG_USER_TEST_SMOKE)
VOID ItTestDac001(VOID);
VOID ItFsJffs001(VOID);
VOID ItFsJffs002(VOID);
VOID ItFsJffs003(VOID);
VOID ItFsJffs005(VOID);
VOID ItFsJffs021(VOID);
VOID ItFsJffs022(VOID);
VOID ItFsJffs026(VOID);
VOID ItFsJffs027(VOID);
VOID ItFsJffs034(VOID);
VOID ItFsJffs035(VOID);
VOID ItFsJffs094(VOID);
VOID ItFsJffs095(VOID);
VOID ItFsJffs103(VOID);
VOID ItFsJffs535(VOID);
#endif

#if defined(LOSCFG_USER_TEST_FULL)
VOID ItJffs001(VOID);
VOID ItJffs002(VOID);
VOID ItJffs003(VOID);
VOID ItJffs004(VOID);
VOID ItJffs005(VOID);
VOID ItJffs006(VOID);
VOID ItJffs007(VOID);
VOID ItJffs008(VOID);
VOID ItJffs009(VOID);
VOID ItJffs010(VOID);
VOID ItJffs011(VOID);
VOID ItJffs012(VOID);
VOID ItJffs013(VOID);
VOID ItJffs014(VOID);
VOID ItJffs015(VOID);
VOID ItJffs016(VOID);
VOID ItJffs017(VOID);
VOID ItJffs018(VOID);
VOID ItJffs019(VOID);
VOID ItJffs020(VOID);
VOID ItJffs021(VOID);
VOID ItJffs022(VOID);
VOID ItJffs023(VOID);
VOID ItJffs024(VOID);
VOID ItJffs025(VOID);
VOID ItJffs026(VOID);
VOID ItJffs027(VOID);
VOID ItJffs028(VOID);
VOID ItJffs029(VOID);
VOID ItJffs030(VOID);
VOID ItJffs031(VOID);
VOID ItJffs032(VOID);
VOID ItJffs033(VOID);
VOID ItJffs034(VOID);
VOID ItJffs035(VOID);
VOID ItJffs036(VOID);
VOID ItJffs037(VOID);
VOID ItJffs038(VOID);
VOID ItJffs039(VOID);
VOID ItJffs040(VOID);
VOID ItJffs041(VOID);
VOID ItJffs042(VOID);
VOID ItJffs043(VOID);
VOID ItJffs044(VOID);

VOID ItFsJffs004(VOID);
VOID ItFsJffs006(VOID);
VOID ItFsJffs007(VOID);
VOID ItFsJffs008(VOID);
VOID ItFsJffs009(VOID);
VOID ItFsJffs010(VOID);
VOID ItFsJffs011(VOID);
VOID ItFsJffs012(VOID);
VOID ItFsJffs013(VOID);
VOID ItFsJffs014(VOID);
VOID ItFsJffs015(VOID);
VOID ItFsJffs016(VOID);
VOID ItFsJffs017(VOID);
VOID ItFsJffs018(VOID);
VOID ItFsJffs019(VOID);
VOID ItFsJffs020(VOID);
VOID ItFsJffs023(VOID);
VOID ItFsJffs024(VOID);
VOID ItFsJffs025(VOID);
VOID ItFsJffs028(VOID);
VOID ItFsJffs029(VOID);
VOID ItFsJffs030(VOID);
VOID ItFsJffs031(VOID);
VOID ItFsJffs032(VOID);
VOID ItFsJffs033(VOID);
VOID ItFsJffs037(VOID);
VOID ItFsJffs038(VOID);
VOID ItFsJffs039(VOID);
VOID ItFsJffs040(VOID);
VOID ItFsJffs041(VOID);
VOID ItFsJffs042(VOID);
VOID ItFsJffs043(VOID);
VOID ItFsJffs044(VOID);
VOID ItFsJffs045(VOID);
VOID ItFsJffs046(VOID);
VOID ItFsJffs047(VOID);
VOID ItFsJffs048(VOID);
VOID ItFsJffs049(VOID);
VOID ItFsJffs050(VOID);
VOID ItFsJffs052(VOID);
VOID ItFsJffs053(VOID);
VOID ItFsJffs054(VOID);
VOID ItFsJffs055(VOID);
VOID ItFsJffs056(VOID);
VOID ItFsJffs057(VOID);
VOID ItFsJffs058(VOID);
VOID ItFsJffs059(VOID);
VOID ItFsJffs060(VOID);
VOID ItFsJffs061(VOID);
VOID ItFsJffs062(VOID);
VOID ItFsJffs063(VOID);
VOID ItFsJffs064(VOID);
VOID ItFsJffs065(VOID);
VOID ItFsJffs066(VOID);
VOID ItFsJffs067(VOID);
VOID ItFsJffs068(VOID);
VOID ItFsJffs069(VOID);
VOID ItFsJffs070(VOID);
VOID ItFsJffs071(VOID);
VOID ItFsJffs072(VOID);
VOID ItFsJffs073(VOID);
VOID ItFsJffs074(VOID);
VOID ItFsJffs075(VOID);
VOID ItFsJffs076(VOID);
VOID ItFsJffs077(VOID);
VOID ItFsJffs078(VOID);
VOID ItFsJffs079(VOID);
VOID ItFsJffs080(VOID);
VOID ItFsJffs081(VOID);
VOID ItFsJffs082(VOID);
VOID ItFsJffs083(VOID);
VOID ItFsJffs084(VOID);
VOID ItFsJffs085(VOID);
VOID ItFsJffs086(VOID);
VOID ItFsJffs088(VOID);
VOID ItFsJffs089(VOID);
VOID ItFsJffs090(VOID);
VOID ItFsJffs091(VOID);
VOID ItFsJffs092(VOID);
VOID ItFsJffs093(VOID);
VOID ItFsJffs096(VOID);
VOID ItFsJffs097(VOID);
VOID ItFsJffs098(VOID);
VOID ItFsJffs099(VOID);
VOID ItFsJffs100(VOID);
VOID ItFsJffs101(VOID);
VOID ItFsJffs102(VOID);
VOID ItFsJffs104(VOID);
VOID ItFsJffs105(VOID);
VOID ItFsJffs106(VOID);
VOID ItFsJffs107(VOID);
VOID ItFsJffs109(VOID);
VOID ItFsJffs110(VOID);
VOID ItFsJffs111(VOID);
VOID ItFsJffs112(VOID);
VOID ItFsJffs113(VOID);
VOID ItFsJffs114(VOID);
VOID ItFsJffs115(VOID);
VOID ItFsJffs116(VOID);
VOID ItFsJffs117(VOID);
VOID ItFsJffs118(VOID);
VOID ItFsJffs119(VOID);
VOID ItFsJffs120(VOID);
VOID ItFsJffs121(VOID);
VOID ItFsJffs122(VOID);
VOID ItFsJffs123(VOID);
VOID ItFsJffs124(VOID);
VOID ItFsJffs125(VOID);
VOID ItFsJffs126(VOID);
VOID ItFsJffs127(VOID);
VOID ItFsJffs128(VOID);
VOID ItFsJffs129(VOID);
VOID ItFsJffs130(VOID);
VOID ItFsJffs131(VOID);
VOID ItFsJffs132(VOID);
VOID ItFsJffs133(VOID);
VOID ItFsJffs134(VOID);
VOID ItFsJffs135(VOID);
VOID ItFsJffs136(VOID);
VOID ItFsJffs137(VOID);
VOID ItFsJffs138(VOID);
VOID ItFsJffs139(VOID);
VOID ItFsJffs140(VOID);
VOID ItFsJffs141(VOID);
VOID ItFsJffs142(VOID);
VOID ItFsJffs143(VOID);
VOID ItFsJffs144(VOID);
VOID ItFsJffs145(VOID);
VOID ItFsJffs146(VOID);
VOID ItFsJffs147(VOID);
VOID ItFsJffs148(VOID);
VOID ItFsJffs149(VOID);
VOID ItFsJffs150(VOID);
VOID ItFsJffs151(VOID);
VOID ItFsJffs152(VOID);
VOID ItFsJffs153(VOID);
VOID ItFsJffs154(VOID);
VOID ItFsJffs155(VOID);
VOID ItFsJffs156(VOID);
VOID ItFsJffs157(VOID);
VOID ItFsJffs158(VOID);
VOID ItFsJffs159(VOID);
VOID ItFsJffs160(VOID);
VOID ItFsJffs161(VOID);
VOID ItFsJffs162(VOID);
VOID ItFsJffs163(VOID);
VOID ItFsJffs164(VOID);
VOID ItFsJffs165(VOID);
VOID ItFsJffs166(VOID);
VOID ItFsJffs167(VOID);
VOID ItFsJffs168(VOID);
VOID ItFsJffs169(VOID);
VOID ItFsJffs170(VOID);
VOID ItFsJffs171(VOID);
VOID ItFsJffs172(VOID);
VOID ItFsJffs173(VOID);
VOID ItFsJffs174(VOID);
VOID ItFsJffs175(VOID);
VOID ItFsJffs176(VOID);
VOID ItFsJffs177(VOID);
VOID ItFsJffs178(VOID);
VOID ItFsJffs179(VOID);
VOID ItFsJffs180(VOID);
VOID ItFsJffs182(VOID);
VOID ItFsJffs183(VOID);
VOID ItFsJffs184(VOID);
VOID ItFsJffs185(VOID);
VOID ItFsJffs187(VOID);
VOID ItFsJffs188(VOID);
VOID ItFsJffs189(VOID);
VOID ItFsJffs190(VOID);
VOID ItFsJffs191(VOID);
VOID ItFsJffs192(VOID);
VOID ItFsJffs193(VOID);
VOID ItFsJffs194(VOID);
VOID ItFsJffs195(VOID);
VOID ItFsJffs196(VOID);
VOID ItFsJffs197(VOID);
VOID ItFsJffs198(VOID);
VOID ItFsJffs199(VOID);
VOID ItFsJffs200(VOID);
VOID ItFsJffs201(VOID);
VOID ItFsJffs202(VOID);
VOID ItFsJffs203(VOID);
VOID ItFsJffs204(VOID);
VOID ItFsJffs205(VOID);
VOID ItFsJffs206(VOID);
VOID ItFsJffs207(VOID);
VOID ItFsJffs208(VOID);
VOID ItFsJffs209(VOID);
VOID ItFsJffs210(VOID);
VOID ItFsJffs211(VOID);
VOID ItFsJffs212(VOID);
VOID ItFsJffs213(VOID);
VOID ItFsJffs214(VOID);
VOID ItFsJffs215(VOID);
VOID ItFsJffs216(VOID);
VOID ItFsJffs217(VOID);
VOID ItFsJffs218(VOID);
VOID ItFsJffs219(VOID);
VOID ItFsJffs220(VOID);
VOID ItFsJffs221(VOID);
VOID ItFsJffs222(VOID);
VOID ItFsJffs223(VOID);
VOID ItFsJffs224(VOID);
VOID ItFsJffs225(VOID);
VOID ItFsJffs226(VOID);
VOID ItFsJffs227(VOID);
VOID ItFsJffs228(VOID);
VOID ItFsJffs229(VOID);
VOID ItFsJffs230(VOID);
VOID ItFsJffs231(VOID);
VOID ItFsJffs232(VOID);
VOID ItFsJffs233(VOID);
VOID ItFsJffs234(VOID);
VOID ItFsJffs235(VOID);
VOID ItFsJffs236(VOID);
VOID ItFsJffs237(VOID);
VOID ItFsJffs238(VOID);
VOID ItFsJffs239(VOID);
VOID ItFsJffs240(VOID);
VOID ItFsJffs241(VOID);
VOID ItFsJffs242(VOID);
VOID ItFsJffs243(VOID);
VOID ItFsJffs244(VOID);
VOID ItFsJffs245(VOID);
VOID ItFsJffs246(VOID);
VOID ItFsJffs247(VOID);
VOID ItFsJffs248(VOID);
VOID ItFsJffs249(VOID);
VOID ItFsJffs250(VOID);
VOID ItFsJffs251(VOID);
VOID ItFsJffs252(VOID);
VOID ItFsJffs253(VOID);
VOID ItFsJffs254(VOID);
VOID ItFsJffs255(VOID);
VOID ItFsJffs256(VOID);
VOID ItFsJffs257(VOID);
VOID ItFsJffs258(VOID);
VOID ItFsJffs259(VOID);
VOID ItFsJffs260(VOID);
VOID ItFsJffs261(VOID);
VOID ItFsJffs262(VOID);
VOID ItFsJffs263(VOID);
VOID ItFsJffs264(VOID);
VOID ItFsJffs265(VOID);
VOID ItFsJffs266(VOID);
VOID ItFsJffs267(VOID);
VOID ItFsJffs268(VOID);
VOID ItFsJffs269(VOID);
VOID ItFsJffs270(VOID);
VOID ItFsJffs271(VOID);
VOID ItFsJffs272(VOID);
VOID ItFsJffs273(VOID);
VOID ItFsJffs274(VOID);
VOID ItFsJffs275(VOID);
VOID ItFsJffs276(VOID);
VOID ItFsJffs277(VOID);
VOID ItFsJffs278(VOID);
VOID ItFsJffs279(VOID);
VOID ItFsJffs280(VOID);
VOID ItFsJffs281(VOID);
VOID ItFsJffs282(VOID);
VOID ItFsJffs283(VOID);
VOID ItFsJffs284(VOID);
VOID ItFsJffs285(VOID);
VOID ItFsJffs286(VOID);
VOID ItFsJffs287(VOID);
VOID ItFsJffs288(VOID);
VOID ItFsJffs289(VOID);
VOID ItFsJffs290(VOID);
VOID ItFsJffs291(VOID);
VOID ItFsJffs292(VOID);
VOID ItFsJffs293(VOID);
VOID ItFsJffs294(VOID);
VOID ItFsJffs295(VOID);
VOID ItFsJffs296(VOID);
VOID ItFsJffs297(VOID);
VOID ItFsJffs298(VOID);
VOID ItFsJffs299(VOID);
VOID ItFsJffs300(VOID);
VOID ItFsJffs301(VOID);
VOID ItFsJffs302(VOID);
VOID ItFsJffs303(VOID);
VOID ItFsJffs304(VOID);
VOID ItFsJffs305(VOID);
VOID ItFsJffs306(VOID);
VOID ItFsJffs307(VOID);
VOID ItFsJffs308(VOID);
VOID ItFsJffs309(VOID);
VOID ItFsJffs310(VOID);
VOID ItFsJffs311(VOID);
VOID ItFsJffs312(VOID);
VOID ItFsJffs313(VOID);
VOID ItFsJffs314(VOID);
VOID ItFsJffs315(VOID);
VOID ItFsJffs316(VOID);
VOID ItFsJffs317(VOID);
VOID ItFsJffs318(VOID);
VOID ItFsJffs319(VOID);
VOID ItFsJffs320(VOID);
VOID ItFsJffs321(VOID);
VOID ItFsJffs322(VOID);
VOID ItFsJffs323(VOID);
VOID ItFsJffs324(VOID);
VOID ItFsJffs325(VOID);
VOID ItFsJffs326(VOID);
VOID ItFsJffs327(VOID);
VOID ItFsJffs328(VOID);
VOID ItFsJffs329(VOID);
VOID ItFsJffs330(VOID);
VOID ItFsJffs331(VOID);
VOID ItFsJffs332(VOID);
VOID ItFsJffs333(VOID);
VOID ItFsJffs334(VOID);
VOID ItFsJffs335(VOID);
VOID ItFsJffs336(VOID);
VOID ItFsJffs337(VOID);
VOID ItFsJffs338(VOID);
VOID ItFsJffs339(VOID);
VOID ItFsJffs340(VOID);
VOID ItFsJffs341(VOID);
VOID ItFsJffs342(VOID);
VOID ItFsJffs343(VOID);
VOID ItFsJffs344(VOID);
VOID ItFsJffs345(VOID);
VOID ItFsJffs346(VOID);
VOID ItFsJffs347(VOID);
VOID ItFsJffs348(VOID);
VOID ItFsJffs349(VOID);
VOID ItFsJffs350(VOID);
VOID ItFsJffs351(VOID);
VOID ItFsJffs352(VOID);
VOID ItFsJffs353(VOID);
VOID ItFsJffs354(VOID);
VOID ItFsJffs355(VOID);
VOID ItFsJffs356(VOID);
VOID ItFsJffs357(VOID);
VOID ItFsJffs358(VOID);
VOID ItFsJffs359(VOID);
VOID ItFsJffs360(VOID);
VOID ItFsJffs361(VOID);
VOID ItFsJffs362(VOID);
VOID ItFsJffs363(VOID);
VOID ItFsJffs364(VOID);
VOID ItFsJffs365(VOID);
VOID ItFsJffs366(VOID);
VOID ItFsJffs367(VOID);
VOID ItFsJffs368(VOID);
VOID ItFsJffs369(VOID);
VOID ItFsJffs370(VOID);
VOID ItFsJffs371(VOID);
VOID ItFsJffs372(VOID);
VOID ItFsJffs373(VOID);
VOID ItFsJffs374(VOID);
VOID ItFsJffs375(VOID);
VOID ItFsJffs376(VOID);
VOID ItFsJffs377(VOID);
VOID ItFsJffs378(VOID);
VOID ItFsJffs379(VOID);
VOID ItFsJffs380(VOID);
VOID ItFsJffs381(VOID);
VOID ItFsJffs382(VOID);
VOID ItFsJffs383(VOID);
VOID ItFsJffs384(VOID);
VOID ItFsJffs385(VOID);
VOID ItFsJffs386(VOID);
VOID ItFsJffs387(VOID);
VOID ItFsJffs388(VOID);
VOID ItFsJffs389(VOID);
VOID ItFsJffs390(VOID);
VOID ItFsJffs391(VOID);
VOID ItFsJffs392(VOID);
VOID ItFsJffs393(VOID);
VOID ItFsJffs394(VOID);
VOID ItFsJffs395(VOID);
VOID ItFsJffs396(VOID);
VOID ItFsJffs397(VOID);
VOID ItFsJffs398(VOID);
VOID ItFsJffs399(VOID);
VOID ItFsJffs400(VOID);
VOID ItFsJffs401(VOID);
VOID ItFsJffs402(VOID);
VOID ItFsJffs403(VOID);
VOID ItFsJffs404(VOID);
VOID ItFsJffs405(VOID);
VOID ItFsJffs406(VOID);
VOID ItFsJffs407(VOID);
VOID ItFsJffs408(VOID);
VOID ItFsJffs409(VOID);
VOID ItFsJffs410(VOID);
VOID ItFsJffs411(VOID);
VOID ItFsJffs412(VOID);
VOID ItFsJffs413(VOID);
VOID ItFsJffs414(VOID);
VOID ItFsJffs415(VOID);
VOID ItFsJffs416(VOID);
VOID ItFsJffs417(VOID);
VOID ItFsJffs418(VOID);
VOID ItFsJffs419(VOID);
VOID ItFsJffs420(VOID);
VOID ItFsJffs421(VOID);
VOID ItFsJffs422(VOID);
VOID ItFsJffs423(VOID);
VOID ItFsJffs424(VOID);
VOID ItFsJffs425(VOID);
VOID ItFsJffs426(VOID);
VOID ItFsJffs427(VOID);
VOID ItFsJffs428(VOID);
VOID ItFsJffs429(VOID);
VOID ItFsJffs430(VOID);
VOID ItFsJffs431(VOID);
VOID ItFsJffs432(VOID);
VOID ItFsJffs433(VOID);
VOID ItFsJffs434(VOID);
VOID ItFsJffs435(VOID);
VOID ItFsJffs436(VOID);
VOID ItFsJffs437(VOID);
VOID ItFsJffs438(VOID);
VOID ItFsJffs439(VOID);
VOID ItFsJffs440(VOID);
VOID ItFsJffs441(VOID);
VOID ItFsJffs442(VOID);
VOID ItFsJffs443(VOID);
VOID ItFsJffs444(VOID);
VOID ItFsJffs445(VOID);
VOID ItFsJffs446(VOID);
VOID ItFsJffs447(VOID);
VOID ItFsJffs448(VOID);
VOID ItFsJffs449(VOID);
VOID ItFsJffs450(VOID);
VOID ItFsJffs451(VOID);
VOID ItFsJffs452(VOID);
VOID ItFsJffs453(VOID);
VOID ItFsJffs454(VOID);
VOID ItFsJffs455(VOID);
VOID ItFsJffs456(VOID);
VOID ItFsJffs457(VOID);
VOID ItFsJffs458(VOID);
VOID ItFsJffs459(VOID);
VOID ItFsJffs460(VOID);
VOID ItFsJffs461(VOID);
VOID ItFsJffs462(VOID);
VOID ItFsJffs463(VOID);
VOID ItFsJffs464(VOID);
VOID ItFsJffs465(VOID);
VOID ItFsJffs466(VOID);
VOID ItFsJffs467(VOID);
VOID ItFsJffs468(VOID);
VOID ItFsJffs469(VOID);
VOID ItFsJffs470(VOID);
VOID ItFsJffs471(VOID);
VOID ItFsJffs472(VOID);
VOID ItFsJffs473(VOID);
VOID ItFsJffs475(VOID);
VOID ItFsJffs476(VOID);
VOID ItFsJffs477(VOID);
VOID ItFsJffs478(VOID);
VOID ItFsJffs479(VOID);
VOID ItFsJffs480(VOID);
VOID ItFsJffs481(VOID);
VOID ItFsJffs482(VOID);
VOID ItFsJffs483(VOID);
VOID ItFsJffs484(VOID);
VOID ItFsJffs485(VOID);
VOID ItFsJffs486(VOID);
VOID ItFsJffs487(VOID);
VOID ItFsJffs488(VOID);
VOID ItFsJffs489(VOID);
VOID ItFsJffs490(VOID);
VOID ItFsJffs491(VOID);
VOID ItFsJffs492(VOID);
VOID ItFsJffs493(VOID);
VOID ItFsJffs494(VOID);
VOID ItFsJffs495(VOID);
VOID ItFsJffs496(VOID);
VOID ItFsJffs497(VOID);
VOID ItFsJffs498(VOID);
VOID ItFsJffs499(VOID);
VOID ItFsJffs500(VOID);
VOID ItFsJffs501(VOID);
VOID ItFsJffs502(VOID);
VOID ItFsJffs503(VOID);
VOID ItFsJffs504(VOID);
VOID ItFsJffs505(VOID);
VOID ItFsJffs506(VOID);
VOID ItFsJffs507(VOID);
VOID ItFsJffs508(VOID);
VOID ItFsJffs509(VOID);
VOID ItFsJffs510(VOID);
VOID ItFsJffs511(VOID);
VOID ItFsJffs512(VOID);
VOID ItFsJffs513(VOID);
VOID ItFsJffs514(VOID);
VOID ItFsJffs515(VOID);
VOID ItFsJffs516(VOID);
VOID ItFsJffs517(VOID);
VOID ItFsJffs518(VOID);
VOID ItFsJffs519(VOID);
VOID ItFsJffs520(VOID);
VOID ItFsJffs521(VOID);
VOID ItFsJffs522(VOID);
VOID ItFsJffs523(VOID);
VOID ItFsJffs524(VOID);
VOID ItFsJffs525(VOID);
VOID ItFsJffs526(VOID);
VOID ItFsJffs528(VOID);
VOID ItFsJffs529(VOID);
VOID ItFsJffs530(VOID);
VOID ItFsJffs531(VOID);
VOID ItFsJffs532(VOID);
VOID ItFsJffs533(VOID);
VOID ItFsJffs534(VOID);
VOID ItFsJffs536(VOID);
VOID ItFsJffs537(VOID);
VOID ItFsJffs538(VOID);
VOID ItFsJffs539(VOID);
VOID ItFsJffs540(VOID);
VOID ItFsJffs541(VOID);
VOID ItFsJffs542(VOID);
VOID ItFsJffs543(VOID);
VOID ItFsJffs544(VOID);
VOID ItFsJffs545(VOID);
VOID ItFsJffs546(VOID);
VOID ItFsJffs547(VOID);
VOID ItFsJffs548(VOID);
VOID ItFsJffs549(VOID);
VOID ItFsJffs550(VOID);
VOID ItFsJffs551(VOID);
VOID ItFsJffs552(VOID);
VOID ItFsJffs553(VOID);
VOID ItFsJffs554(VOID);
VOID ItFsJffs555(VOID);
VOID ItFsJffs556(VOID);
VOID ItFsJffs557(VOID);
VOID ItFsJffs560(VOID);
VOID ItFsJffs562(VOID);
VOID ItFsJffs563(VOID);
VOID ItFsJffs564(VOID);
VOID ItFsJffs565(VOID);
VOID ItFsJffs566(VOID);
VOID ItFsJffs567(VOID);
VOID ItFsJffs568(VOID);
VOID ItFsJffs569(VOID);
VOID ItFsJffs570(VOID);
VOID ItFsJffs571(VOID);
VOID ItFsJffs572(VOID);
VOID ItFsJffs573(VOID);
VOID ItFsJffs574(VOID);

VOID ItFsJffs124(VOID);
VOID ItFsJffs125(VOID);
VOID ItFsJffs577(VOID);
VOID ItFsJffs578(VOID);
VOID ItFsJffs578(VOID);
VOID ItFsJffs579(VOID);
VOID ItFsJffs580(VOID);
VOID ItFsJffs581(VOID);
VOID ItFsJffs582(VOID);
VOID ItFsJffs583(VOID);
VOID ItFsJffs584(VOID);
VOID ItFsJffs585(VOID);
VOID ItFsJffs586(VOID);
VOID ItFsJffs589(VOID);
VOID ItFsJffs590(VOID);
VOID ItFsJffs591(VOID);
VOID ItFsJffs592(VOID);
VOID ItFsJffs593(VOID);
VOID ItFsJffs594(VOID);
VOID ItFsJffs595(VOID);
VOID ItFsJffs596(VOID);
VOID ItFsJffs603(VOID);
VOID ItFsJffs605(VOID);
VOID ItFsJffs606(VOID);
VOID ItFsJffs607(VOID);
VOID ItFsJffs608(VOID);
VOID ItFsJffs609(VOID);
VOID ItFsJffs610(VOID);
VOID ItFsJffs611(VOID);
VOID ItFsJffs612(VOID);
VOID ItFsJffs613(VOID);
VOID ItFsJffs614(VOID);
VOID ItFsJffs615(VOID);
VOID ItFsJffs618(VOID);
VOID ItFsJffs619(VOID);
VOID ItFsJffs620(VOID);
VOID ItFsJffs621(VOID);
VOID ItFsJffs622(VOID);
VOID ItFsJffs623(VOID);
VOID ItFsJffs624(VOID);
VOID ItFsJffs625(VOID);
VOID ItFsJffs626(VOID);
VOID ItFsJffs627(VOID);
VOID ItFsJffs628(VOID);
VOID ItFsJffs629(VOID);
VOID ItFsJffs636(VOID);
VOID ItFsJffs643(VOID);
VOID ItFsJffs644(VOID);
VOID ItFsJffs645(VOID);
VOID ItFsJffs646(VOID);
VOID ItFsJffs648(VOID);
VOID ItFsJffs649(VOID);
VOID ItFsJffs650(VOID);
VOID ItFsJffs651(VOID);
VOID ItFsJffs652(VOID);
VOID ItFsJffs653(VOID);
VOID ItFsJffs654(VOID);
VOID ItFsJffs655(VOID);
VOID ItFsJffs656(VOID);
VOID ItFsJffs663(VOID);
VOID ItFsJffs664(VOID);
VOID ItFsJffs665(VOID);
VOID ItFsJffs666(VOID);
VOID ItFsJffs668(VOID);
VOID ItFsJffs669(VOID);
VOID ItFsJffs670(VOID);
VOID ItFsJffs671(VOID);
VOID ItFsJffs672(VOID);
VOID ItFsJffs673(VOID);
VOID ItFsJffs674(VOID);
VOID ItFsJffs675(VOID);
VOID ItFsJffs676(VOID);
VOID ItFsJffs690(VOID);
VOID ItFsJffs694(VOID);
VOID ItFsJffs696(VOID);
VOID ItFsJffs697(VOID);
VOID ItFsJffs700(VOID);
VOID ItFsJffs701(VOID);
VOID ItFsJffs800(VOID);
VOID ItFsJffs807(VOID);
VOID ItFsJffs808(VOID);

VOID ItFsJffsLSFD_001(VOID);
VOID ItFsJffsLSFD_002(VOID);
VOID ItFsJffsLSFD_003(VOID);
VOID ItFsJffsLSFD_004(VOID);
VOID ItFsJffsLSFD_005(VOID);
VOID ItFsJffsLSFD_006(VOID);
VOID ItFsJffsLSFD_007(VOID);

VOID ItFsTestLink001(VOID);
VOID ItFsTestLink002(VOID);
VOID ItFsTestLink003(VOID);
VOID ItFsTestLinkat001(VOID);
VOID ItFsTestLinkat002(VOID);
VOID ItFsTestLinkat003(VOID);
VOID ItFsTestReadlink001(VOID);
VOID ItFsTestSymlink001(VOID);
VOID ItFsTestSymlink002(VOID);
VOID ItFsTestSymlink003(VOID);
VOID ItFsTestSymlinkat001(VOID);
VOID ItFsTestMountRdonly001(VOID);
VOID ItFsTestMountRdonly002(VOID);
VOID ItFsTestMountRdonly003(VOID);
#endif

#if defined(LOSCFG_USER_TESTSUIT_SHELL)
VOID ItFsJffsLSFD_008(VOID);
VOID ItFsJffsLSFD_009(VOID);
#endif

#if defined(LOSCFG_USER_TEST_PRESSURE)
VOID ItFsJffsMultipthread001(VOID);
VOID ItFsJffsMultipthread002(VOID);
VOID ItFsJffsMultipthread003(VOID);
VOID ItFsJffsMultipthread004(VOID);
VOID ItFsJffsMultipthread005(VOID);
VOID ItFsJffsMultipthread006(VOID);
VOID ItFsJffsMultipthread007(VOID);
VOID ItFsJffsMultipthread008(VOID);
VOID ItFsJffsMultipthread009(VOID);
VOID ItFsJffsMultipthread010(VOID);
VOID ItFsJffsMultipthread011(VOID);
VOID ItFsJffsMultipthread012(VOID);
VOID ItFsJffsMultipthread013(VOID);
VOID ItFsJffsMultipthread014(VOID);
VOID ItFsJffsMultipthread015(VOID);
VOID ItFsJffsMultipthread016(VOID);
VOID ItFsJffsMultipthread017(VOID);
VOID ItFsJffsMultipthread018(VOID);
VOID ItFsJffsMultipthread019(VOID);
VOID ItFsJffsMultipthread020(VOID);
VOID ItFsJffsMultipthread021(VOID);
VOID ItFsJffsMultipthread022(VOID);
VOID ItFsJffsMultipthread023(VOID);
VOID ItFsJffsMultipthread024(VOID);
VOID ItFsJffsMultipthread025(VOID);
VOID ItFsJffsMultipthread026(VOID);
VOID ItFsJffsMultipthread027(VOID);
VOID ItFsJffsMultipthread028(VOID);
VOID ItFsJffsMultipthread029(VOID);
VOID ItFsJffsMultipthread030(VOID);
VOID ItFsJffsMultipthread031(VOID);
VOID ItFsJffsMultipthread032(VOID);
VOID ItFsJffsMultipthread033(VOID);
VOID ItFsJffsMultipthread034(VOID);
VOID ItFsJffsMultipthread035(VOID);
VOID ItFsJffsMultipthread036(VOID);
VOID ItFsJffsMultipthread037(VOID);
VOID ItFsJffsMultipthread038(VOID);
VOID ItFsJffsMultipthread039(VOID);
VOID ItFsJffsMultipthread040(VOID);
VOID ItFsJffsMultipthread041(VOID);
VOID ItFsJffsMultipthread042(VOID);
VOID ItFsJffsMultipthread043(VOID);
VOID ItFsJffsMultipthread044(VOID);
VOID ItFsJffsMultipthread045(VOID);
VOID ItFsJffsMultipthread046(VOID);
VOID ItFsJffsMultipthread047(VOID);
VOID ItFsJffsMultipthread048(VOID);
VOID ItFsJffsMultipthread049(VOID);
VOID ItFsJffsMultipthread050(VOID);
VOID ItFsJffsMultipthread051(VOID);
VOID ItFsJffsMultipthread052(VOID);
VOID ItFsJffsMultipthread053(VOID);
VOID ItFsJffsMultipthread054(VOID);
VOID ItFsJffsMultipthread055(VOID);
VOID ItFsJffsMultipthread056(VOID);
VOID ItFsJffsMultipthread057(VOID);
VOID ItFsJffsMultipthread058(VOID);
VOID ItFsJffsMultipthread059(VOID);
VOID ItFsJffsMultipthread060(VOID);
VOID ItFsJffsMultipthread061(VOID);
VOID ItFsJffsMultipthread062(VOID);
VOID ItFsJffsMultipthread063(VOID);

VOID ItFsJffsPressure001(VOID);
VOID ItFsJffsPressure002(VOID);
VOID ItFsJffsPressure003(VOID);
VOID ItFsJffsPressure004(VOID);
VOID ItFsJffsPressure005(VOID);
VOID ItFsJffsPressure006(VOID);
VOID ItFsJffsPressure007(VOID);
VOID ItFsJffsPressure008(VOID);
VOID ItFsJffsPressure009(VOID);
VOID ItFsJffsPressure010(VOID);
VOID ItFsJffsPressure011(VOID);
VOID ItFsJffsPressure012(VOID);
VOID ItFsJffsPRESSURE_013(VOID);
VOID ItFsJffsPressure014(VOID);
VOID ItFsJffsPressure015(VOID);
VOID ItFsJffsPressure016(VOID);
VOID ItFsJffsPressure017(VOID);
VOID ItFsJffsPressure018(VOID);
VOID ItFsJffsPressure019(VOID);
VOID ItFsJffsPressure020(VOID);
VOID ItFsJffsPressure021(VOID);
VOID ItFsJffsPressure022(VOID);
VOID ItFsJffsPressure023(VOID);
VOID ItFsJffsPressure024(VOID);
VOID ItFsJffsPressure025(VOID);
VOID ItFsJffsPressure026(VOID);
VOID ItFsJffsPressure027(VOID);
VOID ItFsJffsPressure028(VOID);
VOID ItFsJffsPressure029(VOID);
VOID ItFsJffsPressure030(VOID);
VOID ItFsJffsPressure031(VOID);
VOID ItFsJffsPressure032(VOID);
VOID ItFsJffsPressure033(VOID);
VOID ItFsJffsPressure034(VOID);
VOID ItFsJffsPressure035(VOID);
VOID ItFsJffsPressure036(VOID);
VOID ItFsJffsPressure037(VOID);
VOID ItFsJffsPressure038(VOID);
VOID ItFsJffsPressure039(VOID);
VOID ItFsJffsPressure040(VOID);
VOID ItFsJffsPressure041(VOID);
VOID ItFsJffsPressure042(VOID);
VOID ItFsJffsPressure043(VOID);
VOID ItFsJffsPressure044(VOID);
VOID ItFsJffsPressure045(VOID);
VOID ItFsJffsPressure046(VOID);
VOID ItFsJffsPressure047(VOID);
VOID ItFsJffsPressure048(VOID);
VOID ItFsJffsPressure049(VOID);
VOID ItFsJffsPressure050(VOID);
VOID ItFsJffsPressure051(VOID);
VOID ItFsJffsPressure052(VOID);
VOID ItFsJffsPressure053(VOID);

VOID ItFsJffsPressure301(VOID);
VOID ItFsJffsPressure302(VOID);
VOID ItFsJffsPressure303(VOID);
VOID ItFsJffsPressure304(VOID);
VOID ItFsJffsPressure305(VOID);
VOID ItFsJffsPressure306(VOID);
VOID ItFsJffsPressure307(VOID);
VOID ItFsJffsPressure308(VOID);
VOID ItFsJffsPressure309(VOID);
VOID ItFsJffsPressure310(VOID);
VOID ItFsJffsPressure311(VOID);
VOID ItFsJffsPressure312(VOID);
VOID ItFsJffsPressure313(VOID);
VOID ItFsJffsPressure314(VOID);
VOID ItFsJffsPressure315(VOID);

VOID ItFsJffsPerformance001(VOID);
VOID ItFsJffsPerformance002(VOID);
VOID ItFsJffsPerformance003(VOID);
VOID ItFsJffsPerformance004(VOID);
VOID ItFsJffsPerformance005(VOID);
VOID ItFsJffsPerformance006(VOID);
VOID ItFsJffsPerformance007(VOID);
VOID ItFsJffsPerformance008(VOID);
VOID ItFsJffsPerformance009(VOID);
VOID ItFsJffsPerformance010(VOID);
VOID ItFsJffsPerformance011(VOID);
VOID ItFsJffsPerformance012(VOID);
VOID ItFsJffsPerformance013(VOID);
#endif

#if defined(LOSCFG_USER_TEST_LLT)
VOID ItFsJffs036(VOID);
VOID ItFsJffs087(VOID);
VOID ItFsJffs108(VOID);
VOID ItFsJffs181(VOID);
VOID ItFsJffs186(VOID);
VOID ItFsJffs474(VOID);
VOID ItFsJffs527(VOID);
VOID ItFsJffs558(VOID);
VOID ItFsJffs559(VOID);
VOID ItFsJffs561(VOID);
VOID ItFsJffs616(VOID);
VOID ItFsJffs698(VOID);
VOID ItFsJffs699(VOID);
VOID LLT_VFS_JFFS_003(VOID);
VOID LLT_VFS_JFFS_014(VOID);
#endif
