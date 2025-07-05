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

#include "stdlib.h"
#include "stdio.h"
#include "ctype.h"
#include "los_printf.h"
#include "string.h"
#include "securec.h"
#ifdef LOSCFG_SHELL
#include "shcmd.h"
#include "shell.h"
#endif
#include "los_oom.h"
#include "los_vm_dump.h"
#include "los_process_pri.h"
#ifdef LOSCFG_FS_VFS
#include "path_cache.h"
#endif

#ifdef LOSCFG_KERNEL_VM

// 参数数量常量定义
#define ARGC_2             2  // 两个参数
#define ARGC_1             1  // 一个参数
#define ARGC_0             0  // 无参数
// 命令名称常量定义
#define VMM_CMD            "vmm"       // 虚拟内存管理命令
#define OOM_CMD            "oom"       // 内存溢出管理命令
#define VMM_PMM_CMD        "v2p"       // 虚拟地址转物理地址命令

/**
 * @brief 转储内核虚拟地址空间信息
 * @details 调用 OsDumpAspace 函数打印内核地址空间的详细映射情况
 */
LITE_OS_SEC_TEXT_MINOR VOID OsDumpKernelAspace(VOID)
{
    LosVmSpace *kAspace = LOS_GetKVmSpace();  // 获取内核虚拟地址空间
    if (kAspace != NULL)  {                   // 检查内核地址空间是否有效
        OsDumpAspace(kAspace);                // 转储内核地址空间信息
    } else {
        VM_ERR("kernel aspace is NULL");     // 内核地址空间为空时输出错误
    }
    return;
}

/**
 * @brief 验证并转换进程ID字符串
 * @param str 输入的进程ID字符串
 * @return 有效的进程ID（0~63），无效时返回-1
 * @note 进程ID范围限制为0~63，因此字符串长度最多为2位
 */
LITE_OS_SEC_TEXT_MINOR INT32 OsPid(const CHAR *str)
{
    UINT32 len = strlen(str);
    if (len <= 2) { // pid range is 0~63, max pid string length is 2
        for (UINT32 i = 0; i < len; i++) {
            if (isdigit(str[i]) == 0) {       // 检查每个字符是否为数字
                return -1;
            }
        }
        return atoi(str);                     // 转换为整数PID返回
    }
    return -1;                                // 长度超过2位视为无效
}

/**
 * @brief 打印vmm命令的使用帮助
 * @details 输出vmm命令支持的所有选项及其功能说明
 */
LITE_OS_SEC_TEXT_MINOR VOID OsPrintUsage(VOID)
{
    PRINTK("-a,            print all vm address space information\n"
           "-k,            print the kernel vm address space information\n"
           "pid(0~63),     print process[pid] vm address space information\n"
           "-h | --help,   print vmm command usage\n");
}

/**
 * @brief 转储指定进程的虚拟地址空间信息
 * @param pid 目标进程ID
 * @details 验证进程有效性后，调用OsDumpAspace打印进程地址空间映射
 */
LITE_OS_SEC_TEXT_MINOR VOID OsDoDumpVm(pid_t pid)
{
    LosProcessCB *processCB = NULL;

    if (OsProcessIDUserCheckInvalid(pid)) {    // 检查PID是否在有效范围内
        PRINTK("\tThe process [%d] not valid\n", pid);
        return;
    }

    processCB = OS_PCB_FROM_PID(pid);         // 通过PID获取进程控制块
    // 检查进程是否活动且虚拟地址空间有效
    if (!OsProcessIsUnused(processCB) && (processCB->vmSpace != NULL)) {
        OsDumpAspace(processCB->vmSpace);     // 转储进程虚拟地址空间
    } else {
        PRINTK("\tThe process [%d] not active\n", pid);
    }
}

/**
 * @brief vmm命令处理函数
 * @param argc 参数数量
 * @param argv 参数数组
 * @return 始终返回OS_ERROR（命令处理状态码）
 * @details 支持查看所有进程、内核或指定进程的虚拟内存信息
 */
LITE_OS_SEC_TEXT_MINOR UINT32 OsShellCmdDumpVm(INT32 argc, const CHAR *argv[])
{
    if (argc == 0) { //没有参数 使用 # vmm 查看所有进程使用虚拟内存的情况
        OsDumpAllAspace();                    // 转储所有进程地址空间
    } else if (argc == 1) {
        pid_t pid = OsPid(argv[0]);           // 解析PID参数
        if (strcmp(argv[0], "-a") == 0) {	//# vmm -a 查看所有进程使用虚拟内存的情况
            OsDumpAllAspace();                // 转储所有进程地址空间
        } else if (strcmp(argv[0], "-k") == 0) {//# vmm -k 查看内核进程使用虚拟内存的情况
            OsDumpKernelAspace();             // 转储内核地址空间
        } else if (pid >= 0) { //# vmm 3 查看3号进程使用虚拟内存的情况
            OsDoDumpVm(pid);                  // 转储指定进程地址空间
        } else if (strcmp(argv[0], "-h") == 0 || strcmp(argv[0], "--help") == 0) { //# vmm -h 或者  vmm --help
            OsPrintUsage();                   // 打印帮助信息
        } else {
            PRINTK("%s: invalid option: %s\n", VMM_CMD, argv[0]);	//格式错误，输出规范格式
            OsPrintUsage();
        }
    } else {	//多于一个参数 例如 # vmm 3 9
        OsPrintUsage();                       // 参数数量错误时打印帮助
    }

    return OS_ERROR;
}

/**
 * @brief v2p命令的使用帮助
 * @details 输出虚拟地址转物理地址命令的参数格式和使用说明
 */
LITE_OS_SEC_TEXT_MINOR VOID V2PPrintUsage(VOID)
{
    PRINTK("pid vaddr(0x1000000~0x3e000000),     print physical address of virtual address\n"
           "-h | --help,                     print v2p command usage\n");
}

/**
 * @brief 虚拟地址转物理地址命令处理函数
 * @param argc 参数数量
 * @param argv 参数数组（格式：pid vaddr）
 * @return 成功返回LOS_OK，失败返回OS_ERROR
 * @details 解析进程ID和虚拟地址，查询并打印对应的物理地址
 */
LITE_OS_SEC_TEXT_MINOR UINT32 OsShellCmdV2P(INT32 argc, const CHAR *argv[])
{
    UINT32 vaddr;                             // 虚拟地址
    PADDR_T paddr;                            // 物理地址
    CHAR *endPtr = NULL;                      // 字符串转换结束指针

    if (argc == 0) {  // 如果没有参数，打印使用说明
        V2PPrintUsage();
    } else if (argc == 1) {  // 如果只有一个参数
        if (strcmp(argv[0], "-h") == 0 || strcmp(argv[0], "--help") == 0) {  // 如果是帮助参数
            V2PPrintUsage();  // 打印使用说明
        }
    } else if (argc == 2) {  // 如果有两个参数
        pid_t pid = OsPid(argv[0]);  // 获取进程ID
        // 将虚拟地址字符串转换为无符号长整型
        vaddr = strtoul((CHAR *)argv[1], &endPtr, 0);
        // 检查虚拟地址是否有效（转换成功且在用户地址范围内）
        if ((endPtr == NULL) || (*endPtr != 0) || !LOS_IsUserAddress(vaddr)) {
            PRINTK("vaddr %s invalid. should be in range(0x1000000~0x3e000000) \n", argv[1]);
            return OS_ERROR;
        } else {
            if (pid >= 0) {  // 如果进程ID有效
                if (pid < g_processMaxNum) {  // 检查进程ID是否在有效范围内
                    LosProcessCB *processCB = OS_PCB_FROM_PID(pid);  // 获取进程控制块
                    if (!OsProcessIsUnused(processCB)) {  // 检查进程是否处于活动状态
                        paddr = 0;
                        // 查询虚拟地址对应的物理地址
                        LOS_ArchMmuQuery(&processCB->vmSpace->archMmu, (VADDR_T)vaddr, &paddr, 0);
                        if (paddr == 0) {  // 如果物理地址为0，表示未映射
                            PRINTK("vaddr %#x is not in range or mapped\n", vaddr);
                        } else {  // 打印虚拟地址和物理地址
                            PRINTK("vaddr %#x is paddr %#x\n", vaddr, paddr);
                        }
                    } else {  // 进程不活动
                        PRINTK("\tThe process [%d] not active\n", pid);
                    }
                } else {  // 进程ID无效
                    PRINTK("\tThe process [%d] not valid\n", pid);
                }
            } else {  // 进程ID无效
                PRINTK("%s: invalid option: %s %s\n", VMM_PMM_CMD, argv[0], argv[1]);
            }
        }
    }

    return LOS_OK;  // 返回成功
}


/**
 * @brief 转储物理内存管理信息
 * @return 始终返回OS_ERROR
 * @details 依次调用物理内存、路径缓存和虚拟节点的内存转储函数
 */
LITE_OS_SEC_TEXT_MINOR UINT32 OsShellCmdDumpPmm(VOID)
{
    OsVmPhysDump();          // 转储物理内存信息
    PathCacheMemoryDump();   // 转储路径缓存内存信息
    VnodeMemoryDump();       // 转储虚拟节点内存信息
    return OS_ERROR;
}

/**
 * @brief oom命令的使用帮助
 * @details 输出OOM管理命令支持的参数选项及其功能说明
 */
LITE_OS_SEC_TEXT_MINOR VOID OomPrintUsage(VOID)
{
    PRINTK("\t-i [interval],     set oom check interval (ms)\n"	//设置oom线程任务检查的时间间隔。
           "\t-m [mem byte],     set oom low memory threshold (Byte)\n"	//设置低内存阈值。
           "\t-r [mem byte],     set page cache reclaim memory threshold (Byte)\n"	//设置pagecache内存回收阈值。
           "\t-h | --help,       print vmm command usage\n");	//使用帮助。
}

/**
 * @brief OOM管理命令处理函数
 * @param argc 参数数量
 * @param argv 参数数组
 * @return 成功返回LOS_OK，失败返回OS_ERROR
 * @details 支持设置OOM检查间隔、低内存阈值和页缓存回收阈值
 */
LITE_OS_SEC_TEXT_MINOR UINT32 OsShellCmdOom(INT32 argc, const CHAR *argv[])
{
    UINT32 lowMemThreshold;      // 低内存阈值
    UINT32 reclaimMemThreshold;  // 内存回收阈值
    UINT32 checkInterval;        // 检查间隔
    CHAR *endPtr = NULL;         // 字符串转换结束指针

    if (argc == ARGC_0) {
        OomInfodump();           // 无参数时转储OOM当前信息
    } else if (argc == ARGC_1) {
        // 单个参数且非帮助选项时提示错误
        if (strcmp(argv[0], "-h") != 0 && strcmp(argv[0], "--help") != 0) {
            PRINTK("%s: invalid option: %s\n", OOM_CMD, argv[0]);
        }
        OomPrintUsage();         // 打印帮助信息
    } else if (argc == ARGC_2) {
        if (strcmp(argv[0], "-m") == 0) {  // 设置低内存阈值
            lowMemThreshold = strtoul((CHAR *)argv[1], &endPtr, 0);
            if ((endPtr == NULL) || (*endPtr != 0)) {  // 检查参数有效性
                PRINTK("[oom] low mem threshold %s(byte) invalid.\n", argv[1]);
                return OS_ERROR;
            } else {
                OomSetLowMemThreashold(lowMemThreshold);//设置低内存阈值
            }
        } else if (strcmp(argv[0], "-i") == 0) {  // 设置检查间隔
            checkInterval = strtoul((CHAR *)argv[1], &endPtr, 0);
            if ((endPtr == NULL) || (*endPtr != 0)) {  // 检查参数有效性
                PRINTK("[oom] check interval %s(us) invalid.\n", argv[1]);
                return OS_ERROR;
            } else {
                OomSetCheckInterval(checkInterval);//设置oom线程任务检查的时间间隔
            }
        } else if (strcmp(argv[0], "-r") == 0) {  // 设置回收阈值
            reclaimMemThreshold = strtoul((CHAR *)argv[1], &endPtr, 0);
            if ((endPtr == NULL) || (*endPtr != 0)) {  // 检查参数有效性
                PRINTK("[oom] reclaim mem threshold %s(byte) invalid.\n", argv[1]);
                return OS_ERROR;
            } else {
                OomSetReclaimMemThreashold(reclaimMemThreshold);//设置pagecache内存回收阈值
            }
        } else {  // 未知选项
            PRINTK("%s: invalid option: %s %s\n", OOM_CMD, argv[0], argv[1]);
            OomPrintUsage();
        }
    } else {  // 参数数量错误
        PRINTK("%s: invalid option\n", OOM_CMD);
        OomPrintUsage();
    }
    return OS_ERROR;
}

#ifdef LOSCFG_SHELL_CMD_DEBUG
SHELLCMD_ENTRY(oom_shellcmd, CMD_TYPE_SHOW, OOM_CMD, 2, (CmdCallBackFunc)OsShellCmdOom);//采用shell命令静态注册方式
SHELLCMD_ENTRY(vm_shellcmd, CMD_TYPE_SHOW, VMM_CMD, 1, (CmdCallBackFunc)OsShellCmdDumpVm);//采用shell命令静态注册方式 vmm
SHELLCMD_ENTRY(v2p_shellcmd, CMD_TYPE_SHOW, VMM_PMM_CMD, 1, (CmdCallBackFunc)OsShellCmdV2P);//采用shell命令静态注册方式 v2p
#endif

#ifdef LOSCFG_SHELL
SHELLCMD_ENTRY(pmm_shellcmd, CMD_TYPE_SHOW, "pmm", 0, (CmdCallBackFunc)OsShellCmdDumpPmm);//采用shell命令静态注册方式
#endif
#endif

