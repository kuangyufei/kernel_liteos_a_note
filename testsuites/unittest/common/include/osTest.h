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

#ifndef _OSTEST_H
#define _OSTEST_H

#ifndef SWTMR_TEST
#define SWTMR_TEST
#endif
#include "iCunit.h"
#include <sched.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <stdint.h>
#include <stdarg.h>
#include <bits/alltypes.h>
#include <sys/prctl.h>
#include <time.h>
#include <search.h>
#include <sys/mount.h>
#include "los_typedef.h"
#include "sys/wait.h"
#include "glob.h"
#include "mntent.h"
#include "securectype.h"
#include "securec.h"
#include <wchar.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <semaphore.h>

#ifndef OK
#define OK 0
#endif

#define dprintf printf
#define ENOERR OK
#define LOSCFG_BASE_CORE_TSK_CONFIG 1024

#define USER_PROCESS_PRIORITY_HIGHEST 10
#define USER_PROCESS_PRIORITY_LOWEST 31
#define TEST_TASK_PARAM_INIT(stTestTask, task_name, entry, prio) \
    do {                                \
        (void)memset_s(&stTestTask, sizeof(TSK_INIT_PARAM_S), 0, sizeof(TSK_INIT_PARAM_S)); \
        stTestTask.pfnTaskEntry = (TSK_ENTRY_FUNC)entry; \
        stTestTask.uwStackSize = LOS_TASK_MIN_STACK_SIZE; \
        stTestTask.pcName = task_name; \
        stTestTask.usTaskPrio = prio; \
        stTestTask.uwResved = LOS_TASK_STATUS_DETACHED; \
    } while (0)

#ifdef LOSCFG_KERNEL_SMP
#define TEST_TASK_PARAM_INIT_AFFI(stTestTask, task_name, entry, prio, affi) \
    TEST_TASK_PARAM_INIT(stTestTask, task_name, entry, prio)                \
    stTestTask.usCpuAffiMask = affi;
#else
#define TEST_TASK_PARAM_INIT_AFFI(stTestTask, task_name, entry, prio, affi) \
    TEST_TASK_PARAM_INIT(stTestTask, task_name, entry, prio)
#endif
#define JFFS_BASE_MTD_ADDR 0x100000
#define JFFS_BASE_MTD_LEN 0x600000

#define LOS_TASK_MIN_STACK_SIZE 2048
#define TASK_PRIO_TEST 20
#ifdef LOSCFG_AARCH64
#define TASK_STACK_SIZE_TEST (LOS_TASK_MIN_STACK_SIZE * 3)
#else
#define TASK_STACK_SIZE_TEST LOS_TASK_MIN_STACK_SIZE
#endif
#define LOS_MS_PER_TICK (OS_SYS_MS_PER_SECOND / LOSCFG_BASE_CORE_TICK_PER_SECOND)

#define HWI_NUM_INTVALID OS_HWI_MAX_NUM
#define writel(value, address) WRITE_UINT32(value, address)

extern UINT32 PosixPthreadInit(pthread_attr_t *attr, int pri);
extern UINT32 PosixPthreadDestroy(pthread_attr_t *attr, pthread_t thread);

extern VOID TaskHold(UINT64 sec);

extern UINT32 TaskCountGetTest(VOID);
extern UINT32 Sem_Count_Get_Test(VOID);
extern UINT32 QueueCountGetTest(VOID);
extern UINT32 Swtmr_Count_Get_Test(VOID);
extern void hal_interrupt_set_affinity(uint32_t irq, uint32_t cpuMask);

#define TASK_EXISTED_NUM (TaskCountGetTest())
#define QUEUE_EXISTED_NUM (QueueCountGetTest())
#define SWTMR_EXISTED_NUM (Swtmr_Count_Get_Test())
#define SEM_EXISTED_NUM (Sem_Count_Get_Test())

extern void TEST_TEST_HwiDelete(unsigned int irq, void *dev_id);
extern void TEST_HwiTrigger(unsigned int irq);
extern void TestExtraTaskDelay(UINT32 tick);
extern UINT64 TestTickCountGet(void);
extern UINT64 TestTickCountByCurrCpuid(void);
extern void TestBusyTaskDelay(UINT32 tick);
extern void *malloc(size_t size);
extern void TEST_DumpCpuid(void);
extern u_long T_random(void);
extern VOID TestAssertWaitDelay(UINT32 *testCount, UINT32 flag);

UINT32 LosTaskDelay(UINT32 tick);
#define TEST_HwiDelete(ID) TEST_TEST_HwiDelete(ID, NULL)
#define TEST_HwiClear(ID) HalIrqMask(ID)
#define TEST_HwiTriggerDelay LosTaskDelay(200 * LOSCFG_BASE_CORE_TICK_PER_SECOND / 1000)
#define TEST_HwiCreate(ID, prio, mode, Func, arg) LOS_HwiCreate(ID, prio, mode, Func, arg)

#if HUAWEI_ENV_NFS
#define NFS_MOUNT_DIR "/nfs"
#define NFS_MAIN_DIR NFS_MOUNT_DIR
#define NFS_PATH_NAME "/nfs/test"
#else
#define NFS_MOUNT_DIR "/nfs"
#define NFS_MAIN_DIR NFS_MOUNT_DIR
#define NFS_PATH_NAME "/nfs/test"
#endif

#define WIN_MOUNT_PATH "/nfs"
#define WIN_NFS_MOUNT_DIR WIN_MOUNT_PATH
#define WIN_NFS_MAIN_DIR WIN_NFS_MOUNT_DIR
#define WIN_NFS_PATH_NAME "/nfs/test"

#define HWI_NUM_INT0 0
#define HWI_NUM_INT1 1
#define HWI_NUM_INT2 2
#define HWI_NUM_INT3 3
#define HWI_NUM_INT4 4
#define HWI_NUM_INT5 5
#define HWI_NUM_INT6 6
#define HWI_NUM_INT7 7
#define HWI_NUM_INT11 11
#define HWI_NUM_INT12 12
#define HWI_NUM_INT13 13
#define HWI_NUM_INT14 14
#define HWI_NUM_INT15 15
#define HWI_NUM_INT16 16
#define HWI_NUM_INT17 17
#define HWI_NUM_INT18 18
#define HWI_NUM_INT19 19
#define HWI_NUM_INT21 21
#define HWI_NUM_INT22 22
#define HWI_NUM_INT23 23
#define HWI_NUM_INT24 24
#define HWI_NUM_INT25 25
#define HWI_NUM_INT26 26
#define HWI_NUM_INT27 27
#define HWI_NUM_INT28 28
#define HWI_NUM_INT30 30
#define HWI_NUM_INT31 31
#define HWI_NUM_INT32 32
#define HWI_NUM_INT33 33
#define HWI_NUM_INT34 34
#define HWI_NUM_INT35 35
#define HWI_NUM_INT42 42
#define HWI_NUM_INT45 45
#define HWI_NUM_INT46 46
#define HWI_NUM_INT50 50
#define HWI_NUM_INT55 55
#define HWI_NUM_INT56 56
#define HWI_NUM_INT57 57
#define HWI_NUM_INT58 58
#define HWI_NUM_INT59 59
#define HWI_NUM_INT60 60
#define HWI_NUM_INT61 61
#define HWI_NUM_INT63 63
#define HWI_NUM_INT62 62
#define HWI_NUM_INT68 68
#define HWI_NUM_INT69 69

#define HWI_NUM_INT95 95
#define HWI_NUM_INT114 114
#define HWI_NUM_INT169 169

#if defined TESTPBXA9
#define HWI_NUM_TEST HWI_NUM_INT56
#define HWI_NUM_TEST1 HWI_NUM_INT57
#define HWI_NUM_TEST0 HWI_NUM_INT58
#define HWI_NUM_TEST2 HWI_NUM_INT59
#define HWI_NUM_TEST3 HWI_NUM_INT60
#elif defined TEST3518EV300
#define HWI_NUM_TEST0 HWI_NUM_INT58
#define HWI_NUM_TEST HWI_NUM_INT59
#define HWI_NUM_TEST1 HWI_NUM_INT60
#define HWI_NUM_TEST2 HWI_NUM_INT61
#define HWI_NUM_TEST3 HWI_NUM_INT68
#elif defined TEST3516DV300
#define HWI_NUM_TEST HWI_NUM_INT56
#define HWI_NUM_TEST1 HWI_NUM_INT57
#define HWI_NUM_TEST0 HWI_NUM_INT58
#define HWI_NUM_TEST2 HWI_NUM_INT59
#define HWI_NUM_TEST3 HWI_NUM_INT60
#endif

#define TEST_TASKDELAY_1TICK 1
#define TEST_TASKDELAY_2TICK 2
#define TEST_TASKDELAY_4TICK 4
#define TEST_TASKDELAY_10TICK 10
#define TEST_TASKDELAY_20TICK 20
#define TEST_TASKDELAY_50TICK 50

#ifdef TEST3731
#define TestTimer2ValueGet(temp) READ_UINT32(temp, TIMER1_REG_BASE + TIMER_VALUE)
#elif defined TEST3559
#define TestTimer2ValueGet(temp) READ_UINT32(temp, TIMER3_REG_BASE + TIMER_VALUE)
#else
#define TestTimer2ValueGet(temp) READ_UINT32(temp, TIMER2_REG_BASE + TIMER_VALUE)
#endif
extern void TestTimer2Config(void);

#define REALTIME(time) (UINT32)((UINT64)(0xffffffff - time) * 1000 / OS_SYS_CLOCK)           /* accuracy:ms */
#define HW_TMI(time) (UINT32)((UINT64)(0xffffffff - time) * 1000 / (OS_SYS_CLOCK / 1000000)) /* accuracy:ns */

#define uart_printf_func printf

#ifndef VFS_STAT_PRINTF
#define VFS_STAT_PRINTF 0
#endif

#ifndef VFS_STATFS_PRINTF
#define VFS_STATFS_PRINTF 0
#endif

#define OPEN_FILE_MAX 20

#define HUAWEI_ENV_NFS 0

#ifndef TEST_RESOURCELEAK_CHECK
#define TEST_RESOURCELEAK_CHECK YES
#endif

#ifndef TEST_MODULE_CHECK
#define TEST_MODULE_CHECK YES
#endif

extern UINT32 g_shellTestQueueID;
extern int g_min_mempool_size;
extern UINT32 g_testCount;
extern UINT32 g_testCount1;
extern UINT32 g_testCount2;
extern UINT32 g_testCount3;
extern UINT32 g_flowcheck;
extern UINT32 g_failResult;
extern UINT32 g_passResult;
extern UINT32 g_testTskHandle;
extern UINT32 g_testTaskID01;
extern UINT32 g_testTaskID02;
extern UINT32 g_testTaskID03;
extern UINT32 g_testTaskID04;
extern UINT32 g_hwiNum1;
extern UINT32 g_hwiNum2;
extern UINT32 g_semID;
extern UINT32 g_semID2;
extern UINT32 g_mutexTest;
extern UINT32 g_cpupTestCount;
extern UINT16 g_swTmrID;
extern UINT32 g_semID;
extern UINT32 g_testQueueID01;
extern UINT32 g_testQueueID02;
extern UINT32 g_testQueueID03;
extern UINT32 g_testTskHandle;
extern UINT32 g_leavingTaskNum;
extern UINT32 g_mAuwTestTaskID[32];
extern UINT8 g_mUsIndex;
extern UINT32 g_usSemID3[];
extern UINT32 g_testPeriod;
extern BOOL g_isAddArray;
extern BOOL g_isSpinorInit;
extern BOOL g_isSdInit;
extern UINT32 g_getTickConsume;
extern UINT32 g_waitTestCount;
extern INT32 g_libFilesystem;

extern UINT32 GetTimer2Value(VOID);
extern int hinand_erase(unsigned long start, unsigned long size);
#define hispinor_erase(start, size)     \
    do {                                \
        struct erase_info opts;         \
        struct mtd_info *pstMtd;        \
        pstMtd = get_mtd("spinor");     \
        (void)memset_s(&opts, sizeof(opts), 0, sizeof(opts)); \
        opts.addr = start;              \
        opts.len = size;                \
        pstMtd->erase(pstMtd, &opts);   \
    } while (0)
extern void ipc_gmac_init(void);

extern UINT32 Mem_Consume_Show(void);
extern VOID shell_cmd_register(void);
extern INT32 OsShellCmdSystemInfo(INT32 argc, const CHAR **argv);
extern UINT32 OsShellCmdDumpTask(INT32 argc, const CHAR **argv);
extern UINT32 OsShellCmdTaskCntGet(VOID);
extern UINT32 OsShellCmdSwtmrCntGet(VOID);
extern void msleep(unsigned int msecs);
extern unsigned int sleep(unsigned int seconds);
extern int usleep(unsigned useconds);

extern VOID ipc_network_init(void);
#ifdef LOSCFG_DRIVERS_MMC
extern INT32 SD_MMC_Host_init(void);
#endif
extern VOID rdk_fs_init(void);
extern VOID jffs2_fs_init(void);
extern VOID ProcFsInit(void);

extern UINT32 LOS_MemTotalUsedGet(VOID *pool);
extern VOID ptestTickConsume(VOID);
extern UINT32 TEST_TskDelete(UINT32 taskID);
extern UINT32 TEST_SemDelete(UINT32 semHandle);
extern VOID irq_trigger(unsigned int irq);
extern VOID TestPartInit(char *type, UINT32 startAddr, UINT32 length);
extern VOID TestPartDelete(char *type);

extern VOID TestRunShell(VOID);

extern VOID It_Usb_AutoTest(VOID);
extern VOID Test_hid_dev_mode(VOID);

extern UINT32 usbshell_cmd_reg(VOID);
extern void usbshell_queue_control(VOID);
extern UINT32 OsTestInit(VOID);

extern void TEST_DT_COMMON(void);

extern void it_process_testcase(void);
extern void it_pthread_testcase(void);
extern void it_mutex_test(void);
extern void it_rwlock_test(void);
extern void it_spinlock_test(void);

/* Format options (3rd argument of f_mkfs) */
#define TEST_FM_FAT 0x01
#define TEST_FM_FAT32 0x02
#define TEST_FM_EXFAT 0x04
#define TEST_FM_ANY 0x07
#define TEST_FM_SFD 0x08

#define BIG_FD 512
typedef struct testrunParam {
    CHAR testcase_sequence[16];
    CHAR testcase_num[16];
    CHAR testcase_layer[32];
    CHAR testcase_module[32];
    CHAR testcase_level[16];
    CHAR testcase_type[16];
    CHAR testcase_id[128];
} TEST_RUN_PARAM;

typedef enum test_type {
    HOST_U,     // USB U PERFORMANCE
    HOST_MUTIL, // MUTIL
    HOST_DISK,  // USB DISKPARTION
    HOST_HUB,   // USB HUB
    HOST_ETH,   // USB HOST ETH
    USB_SMP,
    HOST_UVC,   // USB HOST UVC
    HOST_NULL
} usb_test_type;


#define SHELLTEST_QUEUE_BUFSIZE sizeof(TEST_RUN_PARAM)
#ifdef LOSCFG_DRIVERS_USB

void Test_usb_shellcmd(controller_type ctype, device_type dtype, usb_test_type typetest);
#endif

extern int Gettid(void);

#define COLOR(c) "\033[" c "m"
#define COLOR_RED(text) COLOR("1;31") text COLOR("0")
#define COLOR_GREEN(text) COLOR("1;32") text COLOR("0")

/* like the ctime/asctime api, use static buffer, though not thread-safe. */
static inline const char *Curtime()
{
    struct timespec ts;
    struct tm t;
    static char buf[32];
    (void)clock_gettime(CLOCK_REALTIME, &ts);
    (void)localtime_r(&ts.tv_sec, &t);
    (void)sprintf_s(buf, sizeof(buf), "%d-%02d-%02d %02d:%02d:%02d.%06ld", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour,
        t.tm_min, t.tm_sec, ts.tv_nsec / 1000);
    return buf;
}

#define LogPrintln(fmt, ...)                                                                  \
    printf("%s [%d] %s:%d " fmt "%c", Curtime(), Gettid(), __FILE__, __LINE__, ##__VA_ARGS__, \
        ('\n' == " " fmt[sizeof(" " fmt) - 2]) ? '\0' : '\n') // trailing newline is auto appended

#if !_REDIR_TIME64 || defined(__LP64__)
#define TIME_F "ld"
#else
#define TIME_F "lld"
#endif

static void noprintf (...)
{
    return;
}
#define TEST_PRINT printf

/* the files with different access privilege used in testcases are define below */
#define FILEPATH_ENOENT "/storage/test_nosuchfile.txt"
#define FILEPATHLEN_ENOENT (strlen(FILEPATH_ENOENT) +1U)

#define FILEPATH_NOACCESS "noaccessssssssssssssssssssssssssssssssssssssssssss"
#define FILEPATHLEN_NOACCESS (strlen(FILEPATH_NOACCESS) +1U)

#define FILEPATH_000 "/storage/test_000.txt"
#define FILEPATHLEN_000 (strlen(FILEPATH_000) +1U)

#define FILEPATH_775 "/storage/test_775.txt"
#define FILEPATHLEN_775 (strlen(FILEPATH_775) +1U)

#define FILEPATH_755 "/storage/test_775.txt"
#define FILEPATHLEN_755 (strlen(FILEPATH_755) +1U)

#define FILEPATH_RELATIVE "./1.txt"
#define FILEPATHLEN_RELATIVE (strlen(FILEPATH_RELATIVE) +1U)

#define DIRPATH_775 "/storage"

#define FD_EBADF 513
#define FD_EFAULT -1000

#define PATHNAME_ENAMETOOLONG "ENAMETOOLONG12345678912345678912345678912345678912345678912345678912345678911111\
12345678912345678912345678913245678912345678912345678913245678913456789123456789\
12345678912345678912345678913245678912345678912345678913245678913456789123456789\
12345678912345678912345678913245678912345678912345678913245678913456789123456789\
12345678912345678912345678913245678912345678912345678913245678913456789123456789\
12345678912345678912345678913245678912345678912345678913245678913456789123456789\
12345678912345678912345678913245678912345678912345678913245678913456789123456789\
12345678912345678912345678913245678912345678912345678913245678913456789123456789\
12345678912345678912345678913245678912345678912345678913245678913456789123456789\
12345678912345678912345678913245678912345678912345678913245678913456789123456789\
12345678912345678912345678913245678912345678912345678913245678913456789123456789\
12345678912345678912345678913245678912345678912345678913245678913456789123456789\
12345678912345678912345678913245678912345678912345678913245678913456789123456789\
12345678912345678912345678913245678912345678912345678913245678913456789123456789\
12345678912345678912345678913245678912345678912345678913245678913456789123456789\
12345678912345678912345678913245678912345678912345678913245678913456789123456789\
12345678912345678912345678913245678912345678912345678913245678913456789123456789\
12345678912345678912345678913245678912345678912345678913245678913456789123456789\
12345678912345678912345678913245678912345678912345678913245678913456789123456789\
12345678912345678912345678913245678912345678912345678913245678913456789123456789\
12345678912345678912345678913245678912345678912345678913245678913456789123456789\
12345678912345678912345678913245678912345678912345678913245678913456789123456789\
12345678912345678912345678913245678912345678912345678913245678913456789123456789\
12345678912345678912345678913245678912345678912345678913245678913456789123456789\
12345678912345678912345678913245678912345678912345678913245678913456789123456789\
12345678912345678912345678913245678912345678912345678913245678913456789123456789\
12345678912345678912345678913245678912345678912345678913245678913456789123456789\
12345678912345678912345678913245678912345678912345678913245678913456789123456789\
12345678912345678912345678913245678912345678912345678913245678913456789123456789\
12345678912345678912345678913245678912345678912345678913245678913456789123456789\
12345678912345678912345678913245678912345678912345678913245678913456789123456789\
12345678912345678912345678913245678912345678912345678913245678913456789123456789\
12345678912345678912345678913245678912345678912345678913245678913456789123456789\
12345678912345678912345678913245678912345678912345678913245678913456789123456789\
12345678912345678912345678913245678912345678912345678913245678913456789123456789\
12345678912345678912345678913245678912345678912345678913245678913456789123456789\
12345678912345678912345678913245678912345678912345678913245678913456789123456789\
12345678912345678912345678913245678912345678912345678913245678913456789123456789\
12345678912345678912345678913245678912345678912345678913245678913456789123456789\
12345678912345678912345678913245678912345678912345678913245678913456789123456789\
12345678912345678912345678913245678912345678912345678913245678913456789123456789\
12345678912345678912345678913245678912345678912345678913245678913456789123456789\
12345678912345678912345678913245678912345678912345678913245678913456789123456789\
12345678912345678912345678913245678912345678912345678913245678913456789123456789\
12345678912345678912345678913245678912345678912345678913245678913456789123456789\
12345678912345678912345678913245678912345678912345678913245678913456789123456789\
12345678912345678912345678913245678912345678912345678913245678913456789123456789\
12345678912345678912345678913245678912345678912345678913245678913456789123456789\
12345678912345678912345678913245678912345678912345678913245678913456789123456789\
12345678912345678912345678913245678912345678912345678913245678913456789123456789\
12345678912345678912345678913245678912345678912345678913245678913456789123456789\
12345678912345678912345678913245678912345678912345678913245678913456789123456789\
12345678912345678912345678913245678912345678912345678913245678913456789123456789"

#endif /* _OSTEST_H */
