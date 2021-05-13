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

#include "osTest.h"
#include <sys/syscall.h>
#include <unistd.h>

UINT32 g_shellTestQueueID;
INT32 g_iCunitErrCode = 0;
INT32 g_iCunitErrLineNo = 0;

UINT32 g_testTskHandle;
UINT32 g_testCount;
UINT32 g_flowcheck = 0;
UINT32 g_failResult = 0;
UINT32 g_passResult = 0;

#ifdef TEST1980
UINT32 g_testhwiFlag;
UINT32 g_testCpuMask;
#endif

UINT32 g_testCount1;
UINT32 g_testCount2;
UINT32 g_testCount3;
UINT32 g_testTaskID01;
UINT32 g_testTaskID02;
UINT32 g_testTaskID03;
UINT32 g_testTaskID04;
UINT32 g_hwiNum1;
UINT32 g_hwiNum2;
UINT32 g_semID;
UINT32 g_semID2;
UINT32 g_mutexTest;
UINT32 g_cpupTestCount;
UINT32 g_waitTestCount;
UINT32 g_testPeriod;

UINT16 g_swTmrID;
UINT32 g_testQueueID01;
UINT32 g_testQueueID02;
UINT32 g_testQueueID03;
UINT32 g_leavingTaskNum;
UINT32 g_mAuwTestTaskID[32] = {0};
UINT32 g_getTickConsume = 0;
CHAR g_libcPathname[50] = "/usr/jffs0";
UINT32 g_testCircleCount = 0;

UINT32 g_fatFilesystem;
UINT8 g_mUsIndex;

#if TEST_MODULE_CHECK == YES

extern UINT32 g_FailModelResult[];
extern UINT32 g_PassModelResult[];
extern UINT32 g_ExecutModelNum[];
#endif
extern char *StrLayer[];
extern char *StrLevel[];
extern char *StrType[];

extern char *StrModule[];
extern UINT32 g_ModelNum;

#ifdef LOSCFG_USER_TEST_FS_FAT
#define TEST_FAT32 0x02
#define TEST_EXFAT 0x04
#endif

BOOL g_isSpinorInit = FALSE;
BOOL g_isSdInit = FALSE;
BOOL g_isUartDevInit = FALSE;
BOOL g_isTcpipInit = FALSE;
BOOL g_isInitSerial = FALSE;
UINT32 g_vfsCyclesCount = 0;
INT32 g_serialInitFlag = -1;
BOOL g_isAddArray = TRUE;
BOOL g_isUsbInit = FALSE;
BOOL g_isIpcGmacInit = FALSE;

BOOL g_isDriversRandomInit = FALSE;

BOOL g_isHisiEthSetPhyModeInit = FALSE;

BOOL g_isVfsInit = FALSE;
BOOL g_isProcInit = FALSE;

INT32 g_libFilesystem = -1;
enum {
    LIB_USE_FAT = 1,
    LIB_USE_JFFS2,
};
#ifdef LOSCFG_DRIVERS_USB
VOID test_init_usb(controller_type ctype, device_type dtype);
#endif
VOID test_init_ipc_gmac(VOID);
VOID test_init_proc(VOID);
VOID test_init_sd(VOID);
VOID TestInitVfs(VOID);
VOID test_init_spinor(VOID);
VOID test_deinit_jffs(VOID);
VOID test_mtd_jffs(VOID);

VOID Wfi(VOID)
{
    __asm__ __volatile__("wfi" : : : "memory");
}

VOID Dmb(VOID)
{
    __asm__ __volatile__("dmb" : : : "memory");
}

VOID Dsb(VOID)
{
    __asm__ __volatile__("dsb" : : : "memory");
}

__attribute__((weak)) int Gettid()
{
    return syscall(SYS_gettid);
}

UINT32 LosCurTaskIDGet()
{
    return Gettid();
}


UINT32 LosTaskDelay(UINT32 tick)
{
    return usleep(10 * tick * 1000);
}

VOID TestExtraTaskDelay(UINT32 uwTick)
{
#if (LOSCFG_KERNEL_SMP == YES)
    // trigger task schedule may occor on another core
    // needs adding delay and checking status later
    LosTaskDelay(uwTick);
#else
    // do nothing
#endif
}

extern volatile UINT64 g_tickCount[];
UINT64 TestTickCountGet(VOID)
{
    /* not use LOS_TickCountGet for now,
       cause every timer is not match with others.
       use cpu0 timer instead. */
    return clock();
}

UINT64 TestTickCountByCurrCpuid(VOID)
{
    return clock();
}

/*
 * different from calling LOS_TaskDelay,
 * this func will not yeild this task to another one.
 */
VOID TestBusyTaskDelay(UINT32 tick)
{
    UINT64 runtime = 0;

    runtime = TestTickCountByCurrCpuid() + tick;
    while (1) {
        if (runtime <= TestTickCountByCurrCpuid()) {
            break;
        }
        Wfi();
    }
}

VOID TestAssertBusyTaskDelay(UINT32 timeout, UINT32 flag)
{
    UINT64 runtime = 0;

    runtime = TestTickCountGet() + timeout;
    while (1) {
        if ((runtime <= TestTickCountGet()) || (g_testCount == flag)) {
            break;
        }
        Wfi();
    }
}

UINT32 PosixPthreadInit(pthread_attr_t *attr, int pri)
{
    UINT32 uwRet = 0;
    struct sched_param sp;

    uwRet = pthread_attr_init(attr);
    ICUNIT_GOTO_EQUAL(uwRet, 0, uwRet, NOK);

    uwRet = pthread_attr_setinheritsched(attr, PTHREAD_EXPLICIT_SCHED);
    ICUNIT_GOTO_EQUAL(uwRet, 0, uwRet, NOK);

    sp.sched_priority = pri;
    uwRet = pthread_attr_setschedparam(attr, &sp);
    ICUNIT_GOTO_EQUAL(uwRet, 0, uwRet, NOK);

    return LOS_OK;
NOK:
    return LOS_NOK;
}

UINT32 PosixPthreadDestroy(pthread_attr_t *attr, pthread_t thread)
{
    UINT32 uwRet = 0;

    uwRet = pthread_join(thread, NULL);
    ICUNIT_GOTO_EQUAL(uwRet, 0, uwRet, NOK);

    uwRet = pthread_attr_destroy(attr);
    ICUNIT_GOTO_EQUAL(uwRet, 0, uwRet, NOK);

    return LOS_OK;
NOK:
    return LOS_NOK;
}

VOID TestInitVfs(VOID)
{
#if defined(LOSCFG_FS_VFS)
    if (g_isVfsInit) {
        return;
    }

    extern VOID los_vfs_init(VOID);
    los_vfs_init();
    g_isVfsInit = TRUE;

#endif
}

VOID TestInitDriversRandom(VOID)
{
    if (g_isDriversRandomInit) {
        return;
    }

#if defined(LOSCFG_DRIVERS_RANDOM)

    printf("random init ...\n");
    extern int ran_dev_register(VOID);
    ran_dev_register();

#endif

#if defined(LOSCFG_HW_RANDOM_ENABLE)

    extern int random_hw_dev_register(VOID);
    printf("random_hw init ...\n");
    if (random_hw_dev_register() != 0) {
        printf("Failed!\n");
    }

#endif

    g_isDriversRandomInit = TRUE;
}

VOID TestInitUartDev(VOID) {}

/* ****************************************
Function:Test_PartInit
Description: creat a partition for testing,partition num is 0,mount point is jffs0
Input:
 [1]type: "spinor"
 [2]start_addr: the partition start address
 [3]length: the partition length
Output: None
Return: None
***************************************** */
VOID TestPartInit(char *type, UINT32 start_addr, UINT32 length)
{
#if defined(LOSCFG_FS_JFFS)
    int uwRet = 0;

    if ((uwRet = add_mtd_partition(type, start_addr, length, 0)) != 0)
        PRINT_ERR("add %s partition failed, return %d\n", type, uwRet);
    else {
        printf("[OK] add %s partition successful\n", type);
        if (strcmp(type, "spinor") == 0) {
            if ((uwRet = mount("/dev/spinorblk0", "/jffs0", "jffs", 0, NULL)) != 0)
                PRINT_ERR("mount jffs0 failed,err %d\n", uwRet);
            else
                printf("[OK] mount jffs0 successful\n");
        }
    }
#endif
    return;
}

/* ****************************************
Function:Test_PartDelete
Description: delete the partition for test
Input:
 [1]type: "spinor"
Output: None
Return: None
***************************************** */
VOID TestPartDelete(char *type)
{
#if defined(LOSCFG_FS_JFFS)

    int uwRet = 0;
    char *point = "";

    if (strcmp(type, "spinor") == 0) {
        point = "/jffs0";
    }

    if ((uwRet = umount(point)) != 0) {
        PRINT_ERR("umount %s failed,err %d.\n", point, uwRet);
    } else {
        printf("[OK] umount %s OK.\n", point);
        if ((uwRet = delete_mtd_partition(0, type)) != 0)
            PRINT_ERR("delete %s partition failed, return %d\n", type, uwRet);
        else
            printf("[OK] delete %s partition OK.\n", type);
    }
#endif
    return;
}

/* *
 * dir: what you want to delete force
 */
int RemoveDir(const char *dir)
{
    char cur_dir[] = ".";
    char up_dir[] = "..";
    char dir_name[128] = { 0 };
    DIR *dirp = NULL;
    struct dirent *dp = NULL;
    struct stat dir_stat;
    int ret;

    if (access(dir, F_OK) != 0) {
        return 0;
    }

    if (stat(dir, &dir_stat) < 0) {
        perror("get directory stat error");
        return -1;
    }

    if (S_ISREG(dir_stat.st_mode)) {
        remove(dir);
    } else if (S_ISDIR(dir_stat.st_mode)) {
        dirp = opendir(dir);
        while ((dp = readdir(dirp)) != NULL) {
            if ((strcmp(cur_dir, dp->d_name) == 0) || (strcmp(up_dir, dp->d_name) == 0)) {
                continue;
            }

            ret = sprintf_s(dir_name, sizeof(dir_name), "%s/%s", dir, dp->d_name);
            if (ret < 0) {
                perror("sprintf dir_name error");
                return -1;
            }
            RemoveDir(dir_name);
        }
        closedir(dirp);

        rmdir(dir); /* now dir is empty */
    } else {
        perror("unknow file type!");
    }
    return 0;
}
