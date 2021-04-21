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
#ifdef LOSCFG_SHELL_EXCINFO
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


#define MEM_SIZE_1K 0x400
#define MEM_SIZE_1M 0x100000

#define MEM_SIZE_TO_KB(size)    (((size) + (MEM_SIZE_1K >> 1)) / MEM_SIZE_1K)
#define MEM_SIZE_TO_MB(size)    (((size) + (MEM_SIZE_1M >> 1)) / MEM_SIZE_1M)

VOID OsDumpMemByte(size_t length, UINTPTR addr)
{
    size_t dataLen;
    UINTPTR *alignAddr = NULL;
    UINT32 count = 0;

    dataLen = ALIGN(length, sizeof(UINTPTR));
    alignAddr = (UINTPTR *)TRUNCATE(addr, sizeof(UINTPTR));
    if ((dataLen == 0) || (alignAddr == NULL)) {
        return;
    }
    while (dataLen) {
        if (IS_ALIGNED(count, sizeof(CHAR *))) {
            PRINTK("\n 0x%lx :", alignAddr);
#ifdef LOSCFG_SHELL_EXCINFO
            WriteExcInfoToBuf("\n 0x%lx :", alignAddr);
#endif
        }
#ifdef __LP64__
        PRINTK("%0+16lx ", *alignAddr);
#else
        PRINTK("%0+8lx ", *alignAddr);
#endif
#ifdef LOSCFG_SHELL_EXCINFO
#ifdef __LP64__
        WriteExcInfoToBuf("0x%0+16x ", *alignAddr);
#else
        WriteExcInfoToBuf("0x%0+8x ", *alignAddr);
#endif
#endif
        alignAddr++;
        dataLen -= sizeof(CHAR *);
        count++;
    }
    PRINTK("\n");
#ifdef LOSCFG_SHELL_EXCINFO
    WriteExcInfoToBuf("\n");
#endif

    return;
}
/***************************************************************
memcheck
命令功能
检查动态申请的内存块是否完整，是否存在内存越界造成节点损坏。

命令格式
memcheck

参数说明
无。

使用指南
当内存池所有节点完整时，输出"system memcheck over, all passed!"。
当内存池存在节点不完整时，输出被损坏节点的内存块信息。
***************************************************************/
LITE_OS_SEC_TEXT_MINOR UINT32 OsShellCmdMemCheck(INT32 argc, const CHAR *argv[])
{
    if (argc > 0) {
        PRINTK("\nUsage: memcheck\n");
        return OS_ERROR;
    }

    if (LOS_MemIntegrityCheck(m_aucSysMem1) == LOS_OK) {
        PRINTK("system memcheck over, all passed!\n");
#ifdef LOSCFG_SHELL_EXCINFO //支持异常缓存时,写缓存
        WriteExcInfoToBuf("system memcheck over, all passed!\n");
#endif
    }
#ifdef LOSCFG_EXC_INTERACTION 
    if (LOS_MemIntegrityCheck(m_aucSysMem0) == LOS_OK) {
        PRINTK("exc interaction memcheck over, all passed!\n");
#ifdef LOSCFG_SHELL_EXCINFO
        WriteExcInfoToBuf("exc interaction memcheck over, all passed!\n");
#endif
    }
#endif
    return 0;
}

#ifdef LOSCFG_SHELL

/*********************************************************

	text    表示代码段大小。
	data    表示数据段大小。
	rodata  表示只读数据段大小。
	bss 	表示未初始化全局变量占用内存大小。

*********************************************************/
LITE_OS_SEC_TEXT_MINOR STATIC VOID OsShellCmdSectionInfo(INT32 argc, const CHAR *argv[])
{
    size_t textLen = &__text_end - &__text_start;
    size_t dataLen = &__ram_data_end - &__ram_data_start;
    size_t rodataLen = &__rodata_end - &__rodata_start;
    size_t bssLen = &__bss_end - &__bss_start;

    PRINTK("\r\n        text         data          rodata        bss\n");
    if ((argc == 1) && (strcmp(argv[0], "-k") == 0)) {
        PRINTK("Mem:    %-9lu    %-10lu    %-10lu    %-10lu\n", MEM_SIZE_TO_KB(textLen), MEM_SIZE_TO_KB(dataLen),
               MEM_SIZE_TO_KB(rodataLen), MEM_SIZE_TO_KB(bssLen));
    } else if ((argc == 1) && (strcmp(argv[0], "-m") == 0)) {
        PRINTK("Mem:    %-9lu    %-10lu    %-10lu    %-10lu\n", MEM_SIZE_TO_MB(textLen), MEM_SIZE_TO_MB(dataLen),
               MEM_SIZE_TO_MB(rodataLen), MEM_SIZE_TO_MB(bssLen));
    } else {
        PRINTK("Mem:    %-9lu    %-10lu    %-10lu    %-10lu\n", textLen, dataLen, rodataLen, bssLen);
    }
}
/*********************************************************

	total 	表示系统动态内存池总量。
	used    表示已使用内存总量。
	free    表示未被分配的内存大小。
	heap    表示已分配堆大小。

*********************************************************/
LITE_OS_SEC_TEXT_MINOR STATIC UINT32 OsShellCmdFreeInfo(INT32 argc, const CHAR *argv[])
{
#ifdef LOSCFG_EXC_INTERACTION
    UINT32 memUsed0 = LOS_MemTotalUsedGet(m_aucSysMem0);
    UINT32 totalMem0 = LOS_MemPoolSizeGet(m_aucSysMem0);
    UINT32 freeMem0 = totalMem0 - memUsed0;
#endif
    UINT32 memUsed = LOS_MemTotalUsedGet(m_aucSysMem1);
    UINT32 totalMem = LOS_MemPoolSizeGet(m_aucSysMem1);
    UINT32 freeMem = totalMem - memUsed;
    UINT32 memUsedHeap = memUsed;

#ifdef LOSCFG_KERNEL_VM
    UINT32 usedCount, totalCount;
    OsVmPhysUsedInfoGet(&usedCount, &totalCount);
    totalMem = SYS_MEM_SIZE_DEFAULT;
    memUsed = SYS_MEM_SIZE_DEFAULT - (totalCount << PAGE_SHIFT);
    memUsed += (usedCount << PAGE_SHIFT) - freeMem;
    freeMem = totalMem - memUsed;
#else
    totalMem = SYS_MEM_SIZE_DEFAULT;
    memUsed = g_vmBootMemBase - KERNEL_ASPACE_BASE;
    memUsed -= freeMem;
    freeMem -= totalMem - memUsed;
#endif

    if ((argc == 0) ||
        ((argc == 1) && (strcmp(argv[0], "-k") == 0)) ||
        ((argc == 1) && (strcmp(argv[0], "-m") == 0))) {
#ifdef LOSCFG_EXC_INTERACTION
        PRINTK("\r\n***** Mem:system mem      Mem1:exception interaction mem *****\n");
#endif
        PRINTK("\r\n        total        used          free          heap\n");
    }

    if ((argc == 1) && (strcmp(argv[0], "-k") == 0)) {
        PRINTK("Mem:    %-9u    %-10u    %-10u    %-10u\n", MEM_SIZE_TO_KB(totalMem), MEM_SIZE_TO_KB(memUsed),
               MEM_SIZE_TO_KB(freeMem), MEM_SIZE_TO_KB(memUsedHeap));
#ifdef LOSCFG_EXC_INTERACTION
        PRINTK("Mem1:   %-9u    %-10u    %-10u\n", MEM_SIZE_TO_KB(totalMem), MEM_SIZE_TO_KB(memUsed),
               MEM_SIZE_TO_KB(freeMem));
#endif
    } else if ((argc == 1) && (strcmp(argv[0], "-m") == 0)) {
        PRINTK("Mem:    %-9u    %-10u    %-10u    %-10u\n", MEM_SIZE_TO_MB(totalMem), MEM_SIZE_TO_MB(memUsed),
               MEM_SIZE_TO_MB(freeMem), MEM_SIZE_TO_MB(memUsedHeap));
#ifdef LOSCFG_EXC_INTERACTION
        PRINTK("Mem1:   %-9u    %-10u    %-10u\n", MEM_SIZE_TO_MB(totalMem), MEM_SIZE_TO_MB(memUsed),
               MEM_SIZE_TO_MB(freeMem));
#endif
    } else if (argc == 0) {
        PRINTK("Mem:    %-9u    %-10u    %-10u    %-10u\n", totalMem, memUsed, freeMem, memUsedHeap);
#ifdef LOSCFG_EXC_INTERACTION
        PRINTK("Mem1:   %-9u    %-10u    %-10u\n", totalMem0, memUsed0, freeMem0);
#endif
    } else {
        PRINTK("\nUsage: free or free [-k/-m]\n");
        return OS_ERROR;
    }
    return 0;
}
/*************************************************************
命令功能
free命令可显示系统内存的使用情况，同时显示系统的text段、data段、rodata段、bss段大小。

命令格式
free [-k | -m]

参数说明

参数 参数说明 取值范围 

无参数    以Byte为单位显示。N/A
-k	以KB为单位显示。N/A
-m	以MB为单位显示。N/A
使用实例
举例：分别输入free、free -k、free -m.
*************************************************************/
LITE_OS_SEC_TEXT_MINOR UINT32 OsShellCmdFree(INT32 argc, const CHAR *argv[])
{
    if (argc > 1) {
        PRINTK("\nUsage: free or free [-k/-m]\n");
        return OS_ERROR;
    }
    if (OsShellCmdFreeInfo(argc, argv) != 0) {//剩余内存统计
        return OS_ERROR;
    }
    OsShellCmdSectionInfo(argc, argv);//显示系统各段使用情况
    return 0;
}
/*************************************************************
命令功能
uname命令用于显示当前操作系统的名称，版本创建时间，系统名称，版本信息等。

命令格式
uname [-a | -s | -t | -v | --help]

参数说明


参数		参数说明		
无参数		默认显示操作系统名称。
-a		显示全部信息。
-t		显示版本创建的时间。
-s		显示操作系统名称。
-v		显示版本信息。
--help	显示uname指令格式提示。

使用指南
uname用于显示当前操作系统名称。语法uname -a | -t| -s| -v 描述uname 命令将正在使用的操作系统名写到标准输出中，这几个参数不能混合使用。

使用实例
举例：输入uname -a
*************************************************************/
LITE_OS_SEC_TEXT_MINOR UINT32 OsShellCmdUname(INT32 argc, const CHAR *argv[])
{
    if (argc == 0) {
        PRINTK("%s\n", KERNEL_NAME);
        return 0;
    }

    if (argc == 1) {
        if (strcmp(argv[0], "-a") == 0) {
            PRINTK("%s %d.%d.%d.%d %s %s\n", KERNEL_NAME, KERNEL_MAJOR, KERNEL_MINOR, KERNEL_PATCH, KERNEL_ITRE,\
                __DATE__, __TIME__);
            return 0;
        } else if (strcmp(argv[0], "-s") == 0) {
            PRINTK("%s\n", KERNEL_NAME);
            return 0;
        } else if (strcmp(argv[0], "-t") == 0) {
            PRINTK("build date : %s %s\n", __DATE__, __TIME__);
            return 0;
        } else if (strcmp(argv[0], "-v") == 0) {
            PRINTK("%d.%d.%d.%d\n", KERNEL_MAJOR, KERNEL_MINOR, KERNEL_PATCH, KERNEL_ITRE);
            return 0;
        } else if (strcmp(argv[0], "--help") == 0) {
            PRINTK("-a,            print all information\n"
                   "-s,            print the kernel name\n"
                   "-t,            print the build date\n"
                   "-v,            print the kernel version\n");
            return 0;
        }
    }

    PRINTK("uname: invalid option %s\n"
           "Try 'uname --help' for more information.\n",
           argv[0]);
    return OS_ERROR;
}
#ifdef LOSCFG_MEM_LEAKCHECK
LITE_OS_SEC_TEXT_MINOR UINT32 OsShellCmdMemUsed(INT32 argc, const CHAR *argv[])
{
    if (argc > 0) {
        PRINTK("\nUsage: memused\n");
        return OS_ERROR;
    }

    OsMemUsedNodeShow(m_aucSysMem1);

#ifdef LOSCFG_EXC_INTERACTION
    PRINTK("\n exc interaction memory\n");
    OsMemUsedNodeShow(m_aucSysMem0);
#endif
    return 0;
}
#endif

#ifdef LOSCFG_MEM_LEAKCHECK
SHELLCMD_ENTRY(memused_shellcmd, CMD_TYPE_EX, "memused", 0, (CmdCallBackFunc)OsShellCmdMemUsed);//shell memused 命令静态注册方式
#endif

#ifdef LOSCFG_SHELL_CMD_DEBUG
SHELLCMD_ENTRY(memcheck_shellcmd, CMD_TYPE_EX, "memcheck", 0, (CmdCallBackFunc)OsShellCmdMemCheck);//shell memcheck 命令静态注册方式
#endif
SHELLCMD_ENTRY(free_shellcmd, CMD_TYPE_EX, "free", XARGS, (CmdCallBackFunc)OsShellCmdFree);//shell free 命令静态注册方式
SHELLCMD_ENTRY(uname_shellcmd, CMD_TYPE_EX, "uname", XARGS, (CmdCallBackFunc)OsShellCmdUname);//shell uname命令静态注册方式
#endif

