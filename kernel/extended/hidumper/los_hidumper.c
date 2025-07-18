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
/* ------------ 本地宏定义 ------------ */
#define CPUP_TYPE_COUNT      3  // CPU类型数量
#define HIDUMPER_DEVICE      "/dev/hidumper"  // hidumper设备路径
#define HIDUMPER_DEVICE_MODE 0666  // hidumper设备文件权限
#define KERNEL_FAULT_ADDR    0x1  // 内核错误地址标识
#define KERNEL_FAULT_VALUE   0x2  // 内核错误值标识
#define READ_BUF_SIZE        128  // 读取缓冲区大小
#define SYS_INFO_HEADER      "************ sys info ***********"  // 系统信息头部字符串
#define CPU_USAGE_HEADER     "************ cpu usage ***********"  // CPU使用率头部字符串
#define MEM_USAGE_HEADER     "************ mem usage ***********"  // 内存使用率头部字符串
#define PAGE_USAGE_HEADER    "************ physical page usage ***********"  // 物理页使用率头部字符串
#define TASK_INFO_HEADER     "************ task info ***********"  // 任务信息头部字符串
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(array)    (sizeof(array) / sizeof(array[0]))  // 计算数组元素个数
#endif
#define REPLACE_INTERFACE(dst, src, type, func) {           \
    if (((type *)src)->func != NULL) {                      \
        ((type *)dst)->func = ((type *)src)->func;          \
    } else {                                                \
        PRINT_ERR("%s->%s is NULL!\n", #src, #func);        \
    }                                                       \
}  // 替换接口函数指针，如果源接口不为空
#define INVOKE_INTERFACE(adapter, type, func) {             \
    if (((type *)adapter)->func != NULL) {                  \
        ((type *)adapter)->func();                          \
    } else {                                                \
        PRINT_ERR("%s->%s is NULL!\n", #adapter, #func);    \
    }                                                       \
}  // 调用接口函数，如果接口不为空

/* ------------ 本地函数声明 ------------ */
/**
 * @brief 打开hidumper设备
 * @param filep 文件指针
 * @return 操作结果，0表示成功，非0表示失败
 */
STATIC INT32 HiDumperOpen(struct file *filep);
/**
 * @brief 关闭hidumper设备
 * @param filep 文件指针
 * @return 操作结果，0表示成功，非0表示失败
 */
STATIC INT32 HiDumperClose(struct file *filep);
/**
 * @brief hidumper设备IO控制
 * @param filep 文件指针
 * @param cmd 控制命令
 * @param arg 命令参数
 * @return 操作结果，0表示成功，非0表示失败
 */
STATIC INT32 HiDumperIoctl(struct file *filep, INT32 cmd, unsigned long arg);

/* ------------ 全局函数声明 ------------ */
#ifdef LOSCFG_SHELL
extern VOID   OsShellCmdSystemInfoGet(VOID);  // 获取系统信息的shell命令
extern UINT32 OsShellCmdFree(INT32 argc, const CHAR *argv[]);  // 显示内存使用情况的shell命令
extern UINT32 OsShellCmdUname(INT32 argc, const CHAR *argv[]);  // 显示系统信息的shell命令
extern UINT32 OsShellCmdDumpPmm(VOID);  // 转储物理内存管理器信息的shell命令
#endif

/* ------------ 本地变量 ------------ */
static struct HiDumperAdapter g_adapter;  // hidumper适配器实例
STATIC struct file_operations_vfs g_hidumperDevOps = {  // hidumper设备文件操作结构体
    HiDumperOpen,  /* open */  // 打开设备函数
    HiDumperClose, /* close */  // 关闭设备函数
    NULL,          /* read */  // 读取函数（未实现）
    NULL,          /* write */  // 写入函数（未实现）
    NULL,          /* seek */  // 定位函数（未实现）
    HiDumperIoctl, /* ioctl */  // IO控制函数
    NULL,          /* mmap */  // 内存映射函数（未实现）
#ifndef CONFIG_DISABLE_POLL
    NULL,          /* poll */  // 轮询函数（未实现）
#endif
    NULL,          /* unlink */  // 删除函数（未实现）
};
/* ------------ 函数定义 ------------ */
/**
 * @brief 打开hidumper设备
 * @param filep 文件指针
 * @return 操作结果，0表示成功
 */
STATIC INT32 HiDumperOpen(struct file *filep)
{
    (VOID)filep;  // 未使用的参数
    return 0;     // 返回成功
}

/**
 * @brief 关闭hidumper设备
 * @param filep 文件指针
 * @return 操作结果，0表示成功
 */
STATIC INT32 HiDumperClose(struct file *filep)
{
    (VOID)filep;  // 未使用的参数
    return 0;     // 返回成功
}

/**
 * @brief 转储系统信息
 */
static void DumpSysInfo(void)
{
    PRINTK("\n%s\n", SYS_INFO_HEADER);  // 打印系统信息头部
#ifdef LOSCFG_SHELL
    const char *argv[1] = {"-a"};  // uname命令参数：显示所有信息
    (VOID)OsShellCmdUname(ARRAY_SIZE(argv), &argv[0]);  // 调用uname命令获取系统信息
    (VOID)OsShellCmdSystemInfoGet();  // 获取并打印系统信息
#else
    PRINTK("\nUnsupported!\n");  // 未配置shell时提示不支持
#endif
}

#ifdef LOSCFG_KERNEL_CPUP
/**
 * @brief 不安全地转储CPU使用率（无锁保护）
 * @param processCpupAll 所有时间的CPU使用率数据
 * @param processCpup10s 最近10秒的CPU使用率数据
 * @param processCpup1s 最近1秒的CPU使用率数据
 */
static void DoDumpCpuUsageUnsafe(CPUP_INFO_S *processCpupAll,
    CPUP_INFO_S *processCpup10s,
    CPUP_INFO_S *processCpup1s)
{
    UINT32 pid;  // 进程ID

    PRINTK("%-32s PID CPUUSE CPUUSE10S CPUUSE1S\n", "PName");  // 打印表头
    for (pid = 0; pid < g_processMaxNum; pid++) {  // 遍历所有进程
        LosProcessCB *processCB = g_processCBArray + pid;  // 获取进程控制块
        if (OsProcessIsUnused(processCB)) {  // 跳过未使用的进程
            continue;
        }
        PRINTK("%-32s %u %5u.%1u%8u.%1u%7u.%-1u\n",  // 打印进程CPU使用率信息
            processCB->processName, processCB->processID,
            processCpupAll[pid].usage / LOS_CPUP_PRECISION_MULT,  // 所有时间CPU使用率整数部分
            processCpupAll[pid].usage % LOS_CPUP_PRECISION_MULT,  // 所有时间CPU使用率小数部分
            processCpup10s[pid].usage / LOS_CPUP_PRECISION_MULT,  // 最近10秒CPU使用率整数部分
            processCpup10s[pid].usage % LOS_CPUP_PRECISION_MULT,  // 最近10秒CPU使用率小数部分
            processCpup1s[pid].usage / LOS_CPUP_PRECISION_MULT,  // 最近1秒CPU使用率整数部分
            processCpup1s[pid].usage % LOS_CPUP_PRECISION_MULT);  // 最近1秒CPU使用率小数部分
    }
}
#endif

/**
 * @brief 不安全地转储CPU使用率（无锁保护）
 */
static void DumpCpuUsageUnsafe(void)
{
    PRINTK("\n%s\n", CPU_USAGE_HEADER);  // 打印CPU使用率头部
#ifdef LOSCFG_KERNEL_CPUP
    UINT32 size;  // 内存大小
    CPUP_INFO_S *processCpup = NULL;  // CPU使用率数据缓冲区
    CPUP_INFO_S *processCpupAll = NULL;  // 所有时间CPU使用率数据
    CPUP_INFO_S *processCpup10s = NULL;  // 最近10秒CPU使用率数据
    CPUP_INFO_S *processCpup1s = NULL;  // 最近1秒CPU使用率数据

    size = sizeof(*processCpup) * g_processMaxNum * CPUP_TYPE_COUNT;  // 计算缓冲区大小
    processCpup = LOS_MemAlloc(m_aucSysMem1, size);  // 分配内存
    if (processCpup == NULL) {  // 内存分配失败
        PRINT_ERR("func: %s, LOS_MemAlloc failed, Line: %d\n", __func__, __LINE__);
        return;
    }
    processCpupAll = processCpup;  // 设置所有时间CPU使用率数据指针
    processCpup10s = processCpupAll + g_processMaxNum;  // 设置最近10秒CPU使用率数据指针
    processCpup1s = processCpup10s + g_processMaxNum;  // 设置最近1秒CPU使用率数据指针
    (VOID)memset_s(processCpup, size, 0, size);  // 初始化缓冲区
    LOS_GetAllProcessCpuUsage(CPUP_ALL_TIME, processCpupAll, g_processMaxNum * sizeof(CPUP_INFO_S));  // 获取所有时间CPU使用率
    LOS_GetAllProcessCpuUsage(CPUP_LAST_TEN_SECONDS, processCpup10s, g_processMaxNum * sizeof(CPUP_INFO_S));  // 获取最近10秒CPU使用率
    LOS_GetAllProcessCpuUsage(CPUP_LAST_ONE_SECONDS, processCpup1s, g_processMaxNum * sizeof(CPUP_INFO_S));  // 获取最近1秒CPU使用率
    DoDumpCpuUsageUnsafe(processCpupAll, processCpup10s, processCpup1s);  // 打印CPU使用率信息
    (VOID)LOS_MemFree(m_aucSysMem1, processCpup);  // 释放内存
#else
    PRINTK("\nUnsupported!\n");  // 未配置CPU性能统计时提示不支持
#endif
}

/**
 * @brief 转储内存使用情况
 */
static void DumpMemUsage(void)
{
    PRINTK("\n%s\n", MEM_USAGE_HEADER);  // 打印内存使用情况头部
#ifdef LOSCFG_SHELL
    PRINTK("Unit: KB\n");  // 打印单位
    const char *argv[1] = {"-k"};  // free命令参数：以KB为单位
    (VOID)OsShellCmdFree(ARRAY_SIZE(argv), &argv[0]);  // 调用free命令获取内存使用情况
    PRINTK("%s\n", PAGE_USAGE_HEADER);  // 打印物理页使用情况头部
    (VOID)OsShellCmdDumpPmm();  // 调用DumpPmm命令获取物理内存管理器信息
#else
    PRINTK("\nUnsupported!\n");  // 未配置shell时提示不支持
#endif
}

/**
 * @brief 转储任务信息
 */
static void DumpTaskInfo(void)
{
    PRINTK("\n%s\n", TASK_INFO_HEADER);  // 打印任务信息头部
#ifdef LOSCFG_SHELL
    (VOID)OsShellCmdTskInfoGet(OS_ALL_TASK_MASK, NULL, OS_PROCESS_INFO_ALL);  // 获取并打印所有任务信息
#else
    PRINTK("\nUnsupported!\n");  // 未配置shell时提示不支持
#endif
}

#ifdef LOSCFG_BLACKBOX
/**
 * @brief 打印文件数据
 * @param fd 文件描述符
 */
static void PrintFileData(INT32 fd)
{
#ifdef LOSCFG_FS_VFS
    CHAR buf[READ_BUF_SIZE];  // 读取缓冲区

    if (fd < 0) {  // 文件描述符无效
        PRINT_ERR("fd: %d!\n", fd);
        return;
    }

    (void)memset_s(buf, sizeof(buf), 0, sizeof(buf));  // 初始化缓冲区
    while (read(fd, buf, sizeof(buf) - 1) > 0) {  // 循环读取文件内容
        PRINTK("%s", buf);  // 打印读取到的数据
        (void)memset_s(buf, sizeof(buf), 0, sizeof(buf));  // 清空缓冲区
    }
#else
    (VOID)fd;  // 未使用的参数
    PRINT_ERR("LOSCFG_FS_VFS isn't defined!\n");  // 未配置VFS时提示错误
#endif
}

/**
 * @brief 打印文件内容
 * @param filePath 文件路径
 * @param pHeader 头部信息
 */
static void PrintFile(const char *filePath, const char *pHeader)
{
#ifdef LOSCFG_FS_VFS
    int fd;  // 文件描述符

    if (filePath == NULL || pHeader == NULL) {  // 参数校验
        PRINT_ERR("filePath: %p, pHeader: %p\n", filePath, pHeader);
        return;
    }

    fd = open(filePath, O_RDONLY);  // 以只读方式打开文件
    if (fd >= 0) {  // 文件打开成功
        PRINTK("\n%s\n", pHeader);  // 打印头部信息
        PrintFileData(fd);  // 打印文件数据
        (void)close(fd);  // 关闭文件
    } else {  // 文件打开失败
        PRINT_ERR("Open [%s] failed or there's no fault log!\n", filePath);
    }
#else
    (VOID)filePath;  // 未使用的参数
    (VOID)pHeader;  // 未使用的参数
    PRINT_ERR("LOSCFG_FS_VFS isn't defined!\n");  // 未配置VFS时提示错误
#endif
}
#endif

/**
 * @brief 转储故障日志
 */
static void DumpFaultLog(void)
{
#ifdef LOSCFG_BLACKBOX
    PrintFile(KERNEL_FAULT_LOG_PATH, "************kernel fault info************");  // 打印内核故障日志
    PrintFile(USER_FAULT_LOG_PATH, "************user fault info************");  // 打印用户故障日志
#endif
}

/**
 * @brief 转储内存数据（当前未实现）
 * @param param 内存转储参数
 */
static void DumpMemData(struct MemDumpParam *param)
{
    PRINTK("Unsupported now!\n");  // 提示未实现
}

/**
 * @brief 注入内核崩溃
 */
static void InjectKernelCrash(void)
{
#ifdef LOSCFG_DEBUG_VERSION
    *((INT32 *)KERNEL_FAULT_ADDR) = KERNEL_FAULT_VALUE;  // 向内核错误地址写入错误值，触发崩溃
#else
    PRINTK("\nUnsupported!\n");  // 非调试版本不支持
#endif
}

/**
 * @brief hidumper设备IO控制
 * @param filep 文件指针
 * @param cmd 控制命令
 * @param arg 命令参数
 * @return 操作结果，0表示成功，非0表示失败
 */
static INT32 HiDumperIoctl(struct file *filep, INT32 cmd, unsigned long arg)
{
    INT32 ret = 0;  // 返回值

    switch (cmd) {  // 根据命令类型处理
        case HIDUMPER_DUMP_ALL:  // 转储所有信息
            INVOKE_INTERFACE(&g_adapter, struct HiDumperAdapter, DumpSysInfo);  // 调用系统信息转储接口
            INVOKE_INTERFACE(&g_adapter, struct HiDumperAdapter, DumpCpuUsage);  // 调用CPU使用率转储接口
            INVOKE_INTERFACE(&g_adapter, struct HiDumperAdapter, DumpMemUsage);  // 调用内存使用情况转储接口
            INVOKE_INTERFACE(&g_adapter, struct HiDumperAdapter, DumpTaskInfo);  // 调用任务信息转储接口
            break;
        case HIDUMPER_CPU_USAGE:  // 转储CPU使用率
            INVOKE_INTERFACE(&g_adapter, struct HiDumperAdapter, DumpCpuUsage);  // 调用CPU使用率转储接口
            break;
        case HIDUMPER_MEM_USAGE:  // 转储内存使用情况
            INVOKE_INTERFACE(&g_adapter, struct HiDumperAdapter, DumpMemUsage);  // 调用内存使用情况转储接口
            break;
        case HIDUMPER_TASK_INFO:  // 转储任务信息
            INVOKE_INTERFACE(&g_adapter, struct HiDumperAdapter, DumpTaskInfo);  // 调用任务信息转储接口
            break;
        case HIDUMPER_INJECT_KERNEL_CRASH:  // 注入内核崩溃
            INVOKE_INTERFACE(&g_adapter, struct HiDumperAdapter, InjectKernelCrash);  // 调用内核崩溃注入接口
            break;
        case HIDUMPER_DUMP_FAULT_LOG:  // 转储故障日志
            INVOKE_INTERFACE(&g_adapter, struct HiDumperAdapter, DumpFaultLog);  // 调用故障日志转储接口
            break;
        case HIDUMPER_MEM_DATA:  // 转储内存数据
            if (g_adapter.DumpMemData != NULL) {  // 检查接口是否有效
                g_adapter.DumpMemData((struct MemDumpParam *)((UINTPTR)arg));  // 调用内存数据转储接口
            }
            break;
        default:  // 未知命令
            ret = EPERM;  // 返回权限错误
            PRINTK("Invalid CMD: 0x%x\n", (UINT32)cmd);  // 打印错误信息
            break;
    }

    return ret;  // 返回结果
}

/**
 * @brief 注册通用适配器
 */
static void RegisterCommonAdapter(void)
{
    struct HiDumperAdapter adapter;  // 适配器实例

    adapter.DumpSysInfo = DumpSysInfo;  // 设置系统信息转储函数
    adapter.DumpCpuUsage = DumpCpuUsageUnsafe;  // 设置CPU使用率转储函数
    adapter.DumpMemUsage = DumpMemUsage;  // 设置内存使用情况转储函数
    adapter.DumpTaskInfo = DumpTaskInfo;  // 设置任务信息转储函数
    adapter.DumpFaultLog = DumpFaultLog;  // 设置故障日志转储函数
    adapter.DumpMemData = DumpMemData;  // 设置内存数据转储函数
    adapter.InjectKernelCrash = InjectKernelCrash;  // 设置内核崩溃注入函数
    HiDumperRegisterAdapter(&adapter);  // 注册适配器
}

/**
 * @brief 注册hidumper适配器
 * @param pAdapter 适配器指针
 * @return 操作结果，0表示成功，-1表示失败
 */
int HiDumperRegisterAdapter(struct HiDumperAdapter *pAdapter)
{
    if (pAdapter == NULL) {  // 参数校验
        PRINT_ERR("pAdapter: %p\n", pAdapter);
        return -1;
    }

    REPLACE_INTERFACE(&g_adapter, pAdapter, struct HiDumperAdapter, DumpSysInfo);  // 替换系统信息转储接口
    REPLACE_INTERFACE(&g_adapter, pAdapter, struct HiDumperAdapter, DumpCpuUsage);  // 替换CPU使用率转储接口
    REPLACE_INTERFACE(&g_adapter, pAdapter, struct HiDumperAdapter, DumpMemUsage);  // 替换内存使用情况转储接口
    REPLACE_INTERFACE(&g_adapter, pAdapter, struct HiDumperAdapter, DumpTaskInfo);  // 替换任务信息转储接口
    REPLACE_INTERFACE(&g_adapter, pAdapter, struct HiDumperAdapter, DumpFaultLog);  // 替换故障日志转储接口
    REPLACE_INTERFACE(&g_adapter, pAdapter, struct HiDumperAdapter, DumpMemData);  // 替换内存数据转储接口
    REPLACE_INTERFACE(&g_adapter, pAdapter, struct HiDumperAdapter, InjectKernelCrash);  // 替换内核崩溃注入接口

    return 0;  // 返回成功
}

/**
 * @brief hidumper驱动初始化
 * @return 操作结果，0表示成功，-1表示失败
 */
int OsHiDumperDriverInit(void)
{
    INT32 ret;  // 返回值

#ifdef LOSCFG_DEBUG_VERSION
    RegisterCommonAdapter();  // 注册通用适配器
    ret = register_driver(HIDUMPER_DEVICE, &g_hidumperDevOps, HIDUMPER_DEVICE_MODE, NULL);  // 注册设备驱动
    if (ret != 0) {  // 驱动注册失败
        PRINT_ERR("Hidumper register driver failed!\n");
        return -1;
    }
#endif

    return 0;  // 返回成功
}
LOS_MODULE_INIT(OsHiDumperDriverInit, LOS_INIT_LEVEL_KMOD_EXTENDED);  // 注册模块初始化函数，初始化级别为扩展内核模块