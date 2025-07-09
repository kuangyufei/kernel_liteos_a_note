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
#include "los_memory_pri.h"
#ifdef LOSCFG_SAVE_EXCINFO
#include "los_excinfo_pri.h"
#endif
#ifdef LOSCFG_SHELL
#include "shcmd.h"
#include "shell.h"
#endif
#include "los_vm_common.h"
#include "los_vm_boot.h"
#include "los_vm_map.h"
#include "los_vm_dump.h"


/** @def MEM_SIZE_1K
 *  @brief 1KB 内存大小（字节）
 *  @details 定义 1KB 对应的字节数，十六进制表示为 0x400 (1024字节)
 */
#define MEM_SIZE_1K 0x400

/** @def MEM_SIZE_1M
 *  @brief 1MB 内存大小（字节）
 *  @details 定义 1MB 对应的字节数，十六进制表示为 0x100000 (1048576字节)
 */
#define MEM_SIZE_1M 0x100000

/** @def MEM_SIZE_TO_KB
 *  @brief 字节数转换为KB（带四舍五入）
 *  @param[in] size 内存大小（字节）
 *  @return 转换后的KB数
 *  @details 计算公式：(size + 512) / 1024，通过加上 MEM_SIZE_1K/2 实现四舍五入
 */
#define MEM_SIZE_TO_KB(size)    (((size) + (MEM_SIZE_1K >> 1)) / MEM_SIZE_1K)

/** @def MEM_SIZE_TO_MB
 *  @brief 字节数转换为MB（带四舍五入）
 *  @param[in] size 内存大小（字节）
 *  @return 转换后的MB数
 *  @details 计算公式：(size + 524288) / 1048576，通过加上 MEM_SIZE_1M/2 实现四舍五入
 */
#define MEM_SIZE_TO_MB(size)    (((size) + (MEM_SIZE_1M >> 1)) / MEM_SIZE_1M)

/**
 * @brief 按字节 dump 内存数据
 * @param[in] length 要 dump 的内存长度（字节）
 * @param[in] addr 内存起始地址
 * @details 以机器字长为单位对齐输出内存数据，支持64位/32位系统，可选将信息写入异常缓冲区
 */
VOID OsDumpMemByte(size_t length, UINTPTR addr)
{
    size_t dataLen;                                                       // 对齐后的内存长度
    UINTPTR *alignAddr = NULL;                                            // 对齐后的内存地址指针
    UINT32 count = 0;                                                     // 已输出的机器字计数

    dataLen = ALIGN(length, sizeof(UINTPTR));                             // 将长度按机器字长对齐
    alignAddr = (UINTPTR *)TRUNCATE(addr, sizeof(UINTPTR));               // 将地址按机器字长对齐
    if ((dataLen == 0) || (alignAddr == NULL)) {                          // 无效参数检查
        return;
    }
    while (dataLen) {                                                     // 循环输出所有对齐后的内存数据
        // 每输出 sizeof(CHAR*) 个字后换行并打印地址
        if (IS_ALIGNED(count, sizeof(CHAR *))) {
            PRINTK("\n 0x%lx :", alignAddr);
#ifdef LOSCFG_SAVE_EXCINFO
            WriteExcInfoToBuf("\n 0x%lx :", alignAddr);                 // 若开启异常信息保存，则写入缓冲区
#endif
        }
#ifdef __LP64__
        PRINTK("%0+16lx ", *alignAddr);                                  // 64位系统：输出16位十六进制数
#else
        PRINTK("%0+8lx ", *alignAddr);                                   // 32位系统：输出8位十六进制数
#endif
#ifdef LOSCFG_SAVE_EXCINFO
#ifdef __LP64__
        WriteExcInfoToBuf("0x%0+16x ", *alignAddr);
#else
        WriteExcInfoToBuf("0x%0+8x ", *alignAddr);
#endif
#endif
        alignAddr++;                                                      // 指向下一个机器字
        dataLen -= sizeof(CHAR *);                                        // 减少剩余长度（按字长）
        count++;                                                          // 增加已输出计数
    }
    PRINTK("\n");
#ifdef LOSCFG_SAVE_EXCINFO
    WriteExcInfoToBuf("\n");
#endif

    return;
}

/**
 * @brief shell命令：内存完整性检查
 * @param[in] argc 参数个数
 * @param[in] argv 参数列表
 * @return 操作结果
 * @retval 0 成功
 * @retval OS_ERROR 参数错误
 * @details 执行系统内存完整性检查，无参数时执行检查并输出结果
 */
LITE_OS_SEC_TEXT_MINOR UINT32 OsShellCmdMemCheck(INT32 argc, const CHAR *argv[])
{
    if (argc > 0) {                                                       // 不支持任何参数
        PRINTK("\nUsage: memcheck\n");
        return OS_ERROR;
    }

    if (LOS_MemIntegrityCheck(m_aucSysMem1) == LOS_OK) {                   // 执行内存完整性检查
        PRINTK("system memcheck over, all passed!\n");
#ifdef LOSCFG_SAVE_EXCINFO
        WriteExcInfoToBuf("system memcheck over, all passed!\n");
#endif
    }

    return 0;
}

#ifdef LOSCFG_SHELL
/**
 * @brief 打印内存段信息（text/data/rodata/bss）
 * @param[in] argc 参数个数
 * @param[in] argv 参数列表
 * @details 根据参数显示不同单位的内存段大小：默认字节，-k为KB，-m为MB
 */
LITE_OS_SEC_TEXT_MINOR STATIC VOID OsShellCmdSectionInfo(INT32 argc, const CHAR *argv[])
{
    // 计算各内存段大小 = 段结束地址 - 段起始地址
    size_t textLen = &__text_end - &__text_start;                         // text段大小（代码段）
    size_t dataLen = &__ram_data_end - &__ram_data_start;                 // data段大小（已初始化数据）
    size_t rodataLen = &__rodata_end - &__rodata_start;                   // rodata段大小（只读数据）
    size_t bssLen = &__bss_end - &__bss_start;                            // bss段大小（未初始化数据）

    PRINTK("\r\n        text         data          rodata        bss\n");
    if ((argc == 1) && (strcmp(argv[0], "-k") == 0)) {                    // 参数-k：以KB为单位
        PRINTK("Mem:    %-9lu    %-10lu    %-10lu    %-10lu\n", MEM_SIZE_TO_KB(textLen), MEM_SIZE_TO_KB(dataLen),
               MEM_SIZE_TO_KB(rodataLen), MEM_SIZE_TO_KB(bssLen));
    } else if ((argc == 1) && (strcmp(argv[0], "-m") == 0)) {             // 参数-m：以MB为单位
        PRINTK("Mem:    %-9lu    %-10lu    %-10lu    %-10lu\n", MEM_SIZE_TO_MB(textLen), MEM_SIZE_TO_MB(dataLen),
               MEM_SIZE_TO_MB(rodataLen), MEM_SIZE_TO_MB(bssLen));
    } else {                                                              // 默认：以字节为单位
        PRINTK("Mem:    %-9lu    %-10lu    %-10lu    %-10lu\n", textLen, dataLen, rodataLen, bssLen);
    }
}

/**
 * @brief 获取并打印内存使用信息
 * @param[in] argc 参数个数
 * @param[in] argv 参数列表
 * @return 操作结果
 * @retval 0 成功
 * @retval OS_ERROR 参数错误
 * @details 计算并显示总内存、已用内存、空闲内存和堆内存使用情况，支持不同单位显示
 */
LITE_OS_SEC_TEXT_MINOR STATIC UINT32 OsShellCmdFreeInfo(INT32 argc, const CHAR *argv[])
{
    UINT32 memUsed = LOS_MemTotalUsedGet(m_aucSysMem1);                   // 获取已用内存大小
    UINT32 totalMem = LOS_MemPoolSizeGet(m_aucSysMem1);                   // 获取内存池总大小
    UINT32 freeMem = totalMem - memUsed;                                  // 空闲内存 = 总内存 - 已用内存
    UINT32 memUsedHeap = memUsed;                                         // 堆内存使用量（初始等于已用内存）

#ifdef LOSCFG_KERNEL_VM                                                    // 若启用虚拟内存
    UINT32 usedCount, totalCount;                                         // 已用/总物理页面计数
    OsVmPhysUsedInfoGet(&usedCount, &totalCount);                         // 获取物理内存使用信息
    totalMem = SYS_MEM_SIZE_DEFAULT;                                      // 系统总内存大小
    // 已用内存 = 系统总内存 - (总页面数 << 页面偏移) + (已用页面数 << 页面偏移) - 空闲内存
    memUsed = SYS_MEM_SIZE_DEFAULT - (totalCount << PAGE_SHIFT);
    memUsed += (usedCount << PAGE_SHIFT) - freeMem;
    freeMem = totalMem - memUsed;                                         // 重新计算空闲内存
#else                                                                     // 未启用虚拟内存
    totalMem = SYS_MEM_SIZE_DEFAULT;                                      // 系统总内存大小
    memUsed = g_vmBootMemBase - KERNEL_ASPACE_BASE;                       // 已用内存 = 启动内存基址 - 内核地址空间基址
    memUsed -= freeMem;                                                   // 调整已用内存
    freeMem -= totalMem - memUsed;                                        // 调整空闲内存
#endif

    // 打印表头（仅在参数有效时）
    if ((argc == 0) ||
        ((argc == 1) && (strcmp(argv[0], "-k") == 0)) ||
        ((argc == 1) && (strcmp(argv[0], "-m") == 0))) {
        PRINTK("\r\n        total        used          free          heap\n");
    }

    if ((argc == 1) && (strcmp(argv[0], "-k") == 0)) {                    // 参数-k：以KB为单位显示
        PRINTK("Mem:    %-9u    %-10u    %-10u    %-10u\n", MEM_SIZE_TO_KB(totalMem), MEM_SIZE_TO_KB(memUsed),
               MEM_SIZE_TO_KB(freeMem), MEM_SIZE_TO_KB(memUsedHeap));
    } else if ((argc == 1) && (strcmp(argv[0], "-m") == 0)) {             // 参数-m：以MB为单位显示
        PRINTK("Mem:    %-9u    %-10u    %-10u    %-10u\n", MEM_SIZE_TO_MB(totalMem), MEM_SIZE_TO_MB(memUsed),
               MEM_SIZE_TO_MB(freeMem), MEM_SIZE_TO_MB(memUsedHeap));
    } else if (argc == 0) {                                               // 无参数：以字节为单位显示
        PRINTK("Mem:    %-9u    %-10u    %-10u    %-10u\n", totalMem, memUsed, freeMem, memUsedHeap);
    } else {                                                              // 无效参数
        PRINTK("\nUsage: free or free [-k/-m]\n");
        return OS_ERROR;
    }
    return 0;
}

/**
 * @brief shell命令：显示内存使用情况
 * @param[in] argc 参数个数
 * @param[in] argv 参数列表
 * @return 操作结果
 * @retval 0 成功
 * @retval OS_ERROR 参数错误
 * @details 整合显示内存使用信息和内存段信息，支持-k（KB）和-m（MB）单位选项
 */
LITE_OS_SEC_TEXT_MINOR UINT32 OsShellCmdFree(INT32 argc, const CHAR *argv[])
{
    if (argc > 1) {                                                       // 参数个数不能超过1
        PRINTK("\nUsage: free or free [-k/-m]\n");
        return OS_ERROR;
    }
    if (OsShellCmdFreeInfo(argc, argv) != 0) {                            // 获取并打印内存使用信息
        return OS_ERROR;
    }
    OsShellCmdSectionInfo(argc, argv);                                    // 打印内存段信息
    return 0;
}

/**
 * @brief shell命令：显示系统信息（uname）
 * @param[in] argc 参数个数
 * @param[in] argv 参数列表
 * @return 操作结果
 * @retval 0 成功
 * @retval OS_ERROR 参数错误
 * @details 支持多种参数选项：-a(所有信息)、-s(内核名称)、-t(构建时间)、-v(内核版本)、--help(帮助信息)
 */
LITE_OS_SEC_TEXT_MINOR UINT32 OsShellCmdUname(INT32 argc, const CHAR *argv[])
{
    if (argc == 0) {                                                       // 无参数：显示内核名称
        PRINTK("%s\n", KERNEL_NAME);
        return 0;
    }

    if (argc == 1) {                                                       // 单参数处理
        if (strcmp(argv[0], "-a") == 0) {                                 // -a：显示所有系统信息
            PRINTK("%s %d.%d.%d.%d %s %s\n", KERNEL_NAME, KERNEL_MAJOR, KERNEL_MINOR, KERNEL_PATCH, KERNEL_ITRE, \
                __DATE__, __TIME__);
            return 0;
        } else if (strcmp(argv[0], "-s") == 0) {                          // -s：仅显示内核名称
            PRINTK("%s\n", KERNEL_NAME);
            return 0;
        } else if (strcmp(argv[0], "-t") == 0) {                          // -t：显示构建日期和时间
            PRINTK("build date : %s %s\n", __DATE__, __TIME__);
            return 0;
        } else if (strcmp(argv[0], "-v") == 0) {                          // -v：显示内核版本号
            PRINTK("%d.%d.%d.%d\n", KERNEL_MAJOR, KERNEL_MINOR, KERNEL_PATCH, KERNEL_ITRE);
            return 0;
        } else if (strcmp(argv[0], "--help") == 0) {                      // --help：显示帮助信息
            PRINTK("-a,            print all information\n"
                   "-s,            print the kernel name\n"
                   "-t,            print the build date\n"
                   "-v,            print the kernel version\n");
            return 0;
        }
    }

    // 无效参数处理
    PRINTK("uname: invalid option %s\n"
           "Try 'uname --help' for more information.\n",
           argv[0]);
    return OS_ERROR;
}
#ifdef LOSCFG_MEM_LEAKCHECK                                                // 若启用内存泄漏检查
/**
 * @brief shell命令：显示内存使用节点信息
 * @param[in] argc 参数个数
 * @param[in] argv 参数列表
 * @return 操作结果
 * @retval 0 成功
 * @retval OS_ERROR 参数错误
 * @details 无参数时显示内存使用节点详细信息，用于内存泄漏检测
 */
LITE_OS_SEC_TEXT_MINOR UINT32 OsShellCmdMemUsed(INT32 argc, const CHAR *argv[])
{
    if (argc > 0) {                                                       // 不支持任何参数
        PRINTK("\nUsage: memused\n");
        return OS_ERROR;
    }

    OsMemUsedNodeShow(m_aucSysMem1);                                      // 显示内存使用节点信息

    return 0;
}
#endif

#ifdef LOSCFG_MEM_LEAKCHECK
SHELLCMD_ENTRY(memused_shellcmd, CMD_TYPE_EX, "memused", 0, (CmdCallBackFunc)OsShellCmdMemUsed);
#endif

#ifdef LOSCFG_SHELL_CMD_DEBUG
SHELLCMD_ENTRY(memcheck_shellcmd, CMD_TYPE_EX, "memcheck", 0, (CmdCallBackFunc)OsShellCmdMemCheck);
#endif
SHELLCMD_ENTRY(free_shellcmd, CMD_TYPE_EX, "free", XARGS, (CmdCallBackFunc)OsShellCmdFree);
SHELLCMD_ENTRY(uname_shellcmd, CMD_TYPE_EX, "uname", XARGS, (CmdCallBackFunc)OsShellCmdUname);
#endif

