/*
 * Copyright (c) 2021-2021 Huawei Device Co., Ltd. All rights reserved.
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

/* ------------ includes ------------ */
#include "los_hidumper.h"
#ifdef LOSCFG_BLACKBOX
#include "los_blackbox.h"
#endif
#ifdef LOSCFG_CPUP_INCLUDE_IRQ
#include "los_cpup_pri.h"
#endif
#include "los_hwi_pri.h"
#include "los_init.h"
#include "los_mp.h"
#include "los_mux.h"
#include "los_printf.h"
#include "los_process_pri.h"
#include "los_task_pri.h"
#include "los_vm_dump.h"
#include "los_vm_lock.h"
#include "los_vm_map.h"
#ifdef LOSCFG_FS_VFS
#include "fs/file.h"
#endif
#include "fs/driver.h"
#include "securec.h"
#ifdef LOSCFG_LIB_LIBC
#include "unistd.h"
#endif
#include "user_copy.h"

/* ------------ local macroes ------------ */
#define CPUP_TYPE_COUNT      3
#define HIDUMPER_DEVICE      "/dev/hidumper"
#define HIDUMPER_DEVICE_MODE 0666
#define KERNEL_FAULT_ADDR    0x1
#define KERNEL_FAULT_VALUE   0x2
#define READ_BUF_SIZE        128
#define SYS_INFO_HEADER      "************ sys info ***********"
#define CPU_USAGE_HEADER     "************ cpu usage ***********"
#define MEM_USAGE_HEADER     "************ mem usage ***********"
#define PAGE_USAGE_HEADER    "************ physical page usage ***********"
#define TASK_INFO_HEADER     "************ task info ***********"
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(array)    (sizeof(array) / sizeof(array[0]))
#endif
#define REPLACE_INTERFACE(dst, src, type, func) {\
    if (((type *)src)->func != NULL) {\
        ((type *)dst)->func = ((type *)src)->func;\
    } else {\
        PRINT_ERR("%s->%s is NULL!\n", #src, #func);\
    }\
}
#define INVOKE_INTERFACE(adapter, type, func) {\
    if (((type *)adapter)->func != NULL) {\
        ((type *)adapter)->func();\
    } else {\
        PRINT_ERR("%s->%s is NULL!\n", #adapter, #func);\
    }\
}

/* ------------ local prototypes ------------ */
/* ------------ local function declarations ------------ */
STATIC INT32 HiDumperOpen(struct file *filep);
STATIC INT32 HiDumperClose(struct file *filep);
STATIC INT32 HiDumperIoctl(struct file *filep, INT32 cmd, unsigned long arg);

/* ------------ global function declarations ------------ */
#ifdef LOSCFG_SHELL
extern VOID   OsShellCmdSystemInfoGet(VOID);
extern UINT32 OsShellCmdFree(INT32 argc, const CHAR *argv[]);
extern UINT32 OsShellCmdUname(INT32 argc, const CHAR *argv[]);
extern UINT32 OsShellCmdDumpPmm(VOID);
#endif

/* ------------ local variables ------------ */
static struct HiDumperAdapter g_adapter;
STATIC struct file_operations_vfs g_hidumperDevOps = {
    HiDumperOpen,  /* open */
    HiDumperClose, /* close */
    NULL,          /* read */
    NULL,          /* write */
    NULL,          /* seek */
    HiDumperIoctl, /* ioctl */
    NULL,          /* mmap */
#ifndef CONFIG_DISABLE_POLL
    NULL,          /* poll */
#endif
    NULL,          /* unlink */
};

/* ------------ function definitions ------------ */
STATIC INT32 HiDumperOpen(struct file *filep)
{
    (VOID)filep;
    return 0;
}

STATIC INT32 HiDumperClose(struct file *filep)
{
    (VOID)filep;
    return 0;
}

static void DumpSysInfo(void)
{
    PRINTK("\n%s\n", SYS_INFO_HEADER);
#ifdef LOSCFG_SHELL
    const char *argv[1] = {"-a"};
    (VOID)OsShellCmdUname(ARRAY_SIZE(argv), &argv[0]);
    (VOID)OsShellCmdSystemInfoGet();
#else
    PRINTK("\nUnsupported!\n");
#endif
}

#ifdef LOSCFG_KERNEL_CPUP
static void DoDumpCpuUsageUnsafe(CPUP_INFO_S *processCpupAll,
    CPUP_INFO_S *processCpup10s,
    CPUP_INFO_S *processCpup1s)
{
    UINT32 pid;

    PRINTK("%-32s PID CPUUSE CPUUSE10S CPUUSE1S\n", "PName");
    for (pid = 0; pid < g_processMaxNum; pid++) {
        LosProcessCB *processCB = g_processCBArray + pid;
        if (OsProcessIsUnused(processCB)) {
            continue;
        }
        PRINTK("%-32s %u %5u.%1u%8u.%1u%7u.%-1u\n",
            processCB->processName, processCB->processID,
            processCpupAll[pid].usage / LOS_CPUP_PRECISION_MULT,
            processCpupAll[pid].usage % LOS_CPUP_PRECISION_MULT,
            processCpup10s[pid].usage / LOS_CPUP_PRECISION_MULT,
            processCpup10s[pid].usage % LOS_CPUP_PRECISION_MULT,
            processCpup1s[pid].usage / LOS_CPUP_PRECISION_MULT,
            processCpup1s[pid].usage % LOS_CPUP_PRECISION_MULT);
    }
}
#endif

static void DumpCpuUsageUnsafe(void)
{
    PRINTK("\n%s\n", CPU_USAGE_HEADER);
#ifdef LOSCFG_KERNEL_CPUP
    UINT32 size;
    CPUP_INFO_S *processCpup = NULL;
    CPUP_INFO_S *processCpupAll = NULL;
    CPUP_INFO_S *processCpup10s = NULL;
    CPUP_INFO_S *processCpup1s = NULL;

    size = sizeof(*processCpup) * g_processMaxNum * CPUP_TYPE_COUNT;
    processCpup = LOS_MemAlloc(m_aucSysMem1, size);
    if (processCpup == NULL) {
        PRINT_ERR("func: %s, LOS_MemAlloc failed, Line: %d\n", __func__, __LINE__);
        return;
    }
    processCpupAll = processCpup;
    processCpup10s = processCpupAll + g_processMaxNum;
    processCpup1s = processCpup10s + g_processMaxNum;
    (VOID)memset_s(processCpup, size, 0, size);
    LOS_GetAllProcessCpuUsage(CPUP_ALL_TIME, processCpupAll, g_processMaxNum * sizeof(CPUP_INFO_S));
    LOS_GetAllProcessCpuUsage(CPUP_LAST_TEN_SECONDS, processCpup10s, g_processMaxNum * sizeof(CPUP_INFO_S));
    LOS_GetAllProcessCpuUsage(CPUP_LAST_ONE_SECONDS, processCpup1s, g_processMaxNum * sizeof(CPUP_INFO_S));
    DoDumpCpuUsageUnsafe(processCpupAll, processCpup10s, processCpup1s);
    (VOID)LOS_MemFree(m_aucSysMem1, processCpup);
#else
    PRINTK("\nUnsupported!\n");
#endif
}

static void DumpMemUsage(void)
{
    PRINTK("\n%s\n", MEM_USAGE_HEADER);
#ifdef LOSCFG_SHELL
    PRINTK("Unit: KB\n");
    const char *argv[1] = {"-k"};
    (VOID)OsShellCmdFree(ARRAY_SIZE(argv), &argv[0]);
    PRINTK("%s\n", PAGE_USAGE_HEADER);
    (VOID)OsShellCmdDumpPmm();
#else
    PRINTK("\nUnsupported!\n");
#endif
}

static void DumpTaskInfo(void)
{
    PRINTK("\n%s\n", TASK_INFO_HEADER);
#ifdef LOSCFG_SHELL
    (VOID)OsShellCmdTskInfoGet(OS_ALL_TASK_MASK, NULL, OS_PROCESS_INFO_ALL);
#else
    PRINTK("\nUnsupported!\n");
#endif
}

#ifdef LOSCFG_BLACKBOX
static void PrintFileData(INT32 fd)
{
#ifdef LOSCFG_FS_VFS
    CHAR buf[READ_BUF_SIZE];

    if (fd < 0) {
        PRINT_ERR("fd: %d!\n", fd);
        return;
    }

    (void)memset_s(buf, sizeof(buf), 0, sizeof(buf));
    while (read(fd, buf, sizeof(buf) - 1) > 0) {
        PRINTK("%s", buf);
        (void)memset_s(buf, sizeof(buf), 0, sizeof(buf));
    }
#else
    (VOID)fd;
    PRINT_ERR("LOSCFG_FS_VFS isn't defined!\n");
#endif
}

static void PrintFile(const char *filePath, const char *pHeader)
{
#ifdef LOSCFG_FS_VFS
    int fd;

    if (filePath == NULL || pHeader == NULL) {
        PRINT_ERR("filePath: %p, pHeader: %p\n", filePath, pHeader);
        return;
    }

    fd = open(filePath, O_RDONLY);
    if (fd >= 0) {
        PRINTK("\n%s\n", pHeader);
        PrintFileData(fd);
        (void)close(fd);
    } else {
        PRINT_ERR("Open [%s] failed or there's no fault log!\n", filePath);
    }
#else
    (VOID)filePath;
    (VOID)pHeader;
    PRINT_ERR("LOSCFG_FS_VFS isn't defined!\n");
#endif
}
#endif

static void DumpFaultLog(void)
{
#ifdef LOSCFG_BLACKBOX
    PrintFile(KERNEL_FAULT_LOG_PATH, "************kernel fault info************");
    PrintFile(USER_FAULT_LOG_PATH, "************user fault info************");
#endif
}

static void DumpMemData(struct MemDumpParam *param)
{
    PRINTK("\nDumpType: %d\n", param->type);
    PRINTK("Unsupported now!\n");
}

static void InjectKernelCrash(void)
{
#ifdef LOSCFG_DEBUG_VERSION
    *((INT32 *)KERNEL_FAULT_ADDR) = KERNEL_FAULT_VALUE;
#else
    PRINTK("\nUnsupported!\n");
#endif
}

static INT32 HiDumperIoctl(struct file *filep, INT32 cmd, unsigned long arg)
{
    INT32 ret = 0;

    switch (cmd) {
    case HIDUMPER_DUMP_ALL:
        INVOKE_INTERFACE(&g_adapter, struct HiDumperAdapter, DumpSysInfo);
        INVOKE_INTERFACE(&g_adapter, struct HiDumperAdapter, DumpCpuUsage);
        INVOKE_INTERFACE(&g_adapter, struct HiDumperAdapter, DumpMemUsage);
        INVOKE_INTERFACE(&g_adapter, struct HiDumperAdapter, DumpTaskInfo);
        break;
    case HIDUMPER_CPU_USAGE:
        INVOKE_INTERFACE(&g_adapter, struct HiDumperAdapter, DumpCpuUsage);
        break;
    case HIDUMPER_MEM_USAGE:
        INVOKE_INTERFACE(&g_adapter, struct HiDumperAdapter, DumpMemUsage);
        break;
    case HIDUMPER_TASK_INFO:
        INVOKE_INTERFACE(&g_adapter, struct HiDumperAdapter, DumpTaskInfo);
        break;
    case HIDUMPER_INJECT_KERNEL_CRASH:
        INVOKE_INTERFACE(&g_adapter, struct HiDumperAdapter, InjectKernelCrash);
        break;
    case HIDUMPER_DUMP_FAULT_LOG:
        INVOKE_INTERFACE(&g_adapter, struct HiDumperAdapter, DumpFaultLog);
        break;
    case HIDUMPER_MEM_DATA:
        if (g_adapter.DumpMemData != NULL) {
            g_adapter.DumpMemData((struct MemDumpParam *)arg);
        }
        break;
    default:
        ret = EPERM;
        PRINTK("Invalid CMD: 0x%x\n", (UINT32)cmd);
        break;
    }

    return ret;
}

static void RegisterCommonAdapter(void)
{
    struct HiDumperAdapter adapter;

    adapter.DumpSysInfo = DumpSysInfo;
    adapter.DumpCpuUsage = DumpCpuUsageUnsafe;
    adapter.DumpMemUsage = DumpMemUsage;
    adapter.DumpTaskInfo = DumpTaskInfo;
    adapter.DumpFaultLog = DumpFaultLog;
    adapter.DumpMemData = DumpMemData;
    adapter.InjectKernelCrash = InjectKernelCrash;
    HiDumperRegisterAdapter(&adapter);
}

int HiDumperRegisterAdapter(struct HiDumperAdapter *pAdapter)
{
    if (pAdapter == NULL) {
        PRINT_ERR("pAdapter: %p\n", pAdapter);
        return -1;
    }

    REPLACE_INTERFACE(&g_adapter, pAdapter, struct HiDumperAdapter, DumpSysInfo);
    REPLACE_INTERFACE(&g_adapter, pAdapter, struct HiDumperAdapter, DumpCpuUsage);
    REPLACE_INTERFACE(&g_adapter, pAdapter, struct HiDumperAdapter, DumpMemUsage);
    REPLACE_INTERFACE(&g_adapter, pAdapter, struct HiDumperAdapter, DumpTaskInfo);
    REPLACE_INTERFACE(&g_adapter, pAdapter, struct HiDumperAdapter, DumpFaultLog);
    REPLACE_INTERFACE(&g_adapter, pAdapter, struct HiDumperAdapter, DumpMemData);
    REPLACE_INTERFACE(&g_adapter, pAdapter, struct HiDumperAdapter, InjectKernelCrash);

    return 0;
}

int OsHiDumperDriverInit(void)
{
    INT32 ret;

#ifdef LOSCFG_DEBUG_VERSION
    RegisterCommonAdapter();
    ret = register_driver(HIDUMPER_DEVICE, &g_hidumperDevOps, HIDUMPER_DEVICE_MODE, NULL);
    if (ret != 0) {
        PRINT_ERR("Hidumper register driver failed!\n");
        return -1;
    }
#endif

    return 0;
}
LOS_MODULE_INIT(OsHiDumperDriverInit, LOS_INIT_LEVEL_KMOD_EXTENDED);
