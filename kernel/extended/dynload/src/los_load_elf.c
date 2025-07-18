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

#include "los_load_elf.h"
#include "fcntl.h"
#include "fs/fd_table.h"
#include "fs/file.h"
#include "fs/fs_operation.h"
#include "los_config.h"
#include "los_vm_map.h"
#include "los_vm_syscall.h"
#include "los_vm_phys.h"
#include "los_vm_dump.h"
#include "los_vm_lock.h"
#ifdef LOSCFG_KERNEL_VDSO
#include "los_vdso.h"
#endif
#ifdef LOSCFG_DRIVERS_TZDRIVER
#include "tzdriver.h"
#endif

STATIC BOOL g_srandInit;  // 随机数生成器初始化标志

/**
 * @brief 打开ELF文件并关联进程文件描述符
 * @param fileName 文件名
 * @param oflags 打开标志
 * @return 成功返回进程文件描述符，失败返回错误码
 */
STATIC INT32 OsELFOpen(const CHAR *fileName, INT32 oflags)
{
    INT32 ret;                               // 返回值
    INT32 procFd;                            // 进程文件描述符

    procFd = AllocProcessFd();               // 分配进程文件描述符
    if (procFd < 0) {                        // 分配失败
        return -EMFILE;                      // 返回打开文件过多错误
    }

    if (oflags & O_CLOEXEC) {                // 如果设置了执行后关闭标志
        SetCloexecFlag(procFd);              // 设置文件描述符的CLOEXEC标志
    }

    ret = open(fileName, oflags);            // 打开文件
    if (ret < 0) {                           // 打开失败
        FreeProcessFd(procFd);               // 释放进程文件描述符
        return -get_errno();                 // 返回错误码
    }

    AssociateSystemFd(procFd, ret);          // 关联进程文件描述符和系统文件描述符
    return procFd;                           // 返回进程文件描述符
}

/**
 * @brief 关闭ELF文件并释放进程文件描述符
 * @param procFd 进程文件描述符
 * @return 成功返回LOS_OK，失败返回错误码
 */
STATIC INT32 OsELFClose(INT32 procFd)
{
    INT32 ret;                               // 返回值
    /* 进程文件描述符转换为系统全局文件描述符 */
    INT32 sysfd = DisassociateProcessFd(procFd);  // 解除进程文件描述符关联
    if (sysfd < 0) {                         // 无效的进程文件描述符
        return -EBADF;                       // 返回错误的文件描述符错误
    }

    ret = close(sysfd);                      // 关闭系统文件描述符
    if (ret < 0) {                           // 关闭失败
        AssociateSystemFd(procFd, sysfd);    // 重新关联文件描述符
        return -get_errno();                 // 返回错误码
    }
    FreeProcessFd(procFd);                   // 释放进程文件描述符
    return ret;                              // 返回结果
}

/**
 * @brief 获取文件长度并验证文件有效性
 * @param fileLen 文件长度输出指针
 * @param fileName 文件名
 * @return 成功返回LOS_OK，失败返回LOS_NOK
 */
STATIC INT32 OsGetFileLength(UINT32 *fileLen, const CHAR *fileName)
{
    struct stat buf;                         // 文件状态结构体
    INT32 ret;                               // 返回值

    ret = stat(fileName, &buf);              // 获取文件状态
    if (ret < 0) {                           // 获取失败
#ifndef LOSCFG_SHELL
        if (strcmp(fileName, "/bin/shell") != 0) {  // 如果不是shell文件（非shell配置）
#endif
            PRINT_ERR("%s[%d], Failed to stat file: %s, errno: %d\n", __FUNCTION__, __LINE__, fileName, errno);
#ifndef LOSCFG_SHELL
        }
#endif
        return LOS_NOK;                      // 返回失败
    }

    if (S_ISREG(buf.st_mode) == 0) {         // 检查是否为普通文件
        PRINT_ERR("%s[%d], The file: %s is invalid!\n", __FUNCTION__, __LINE__, fileName);
        return LOS_NOK;                      // 返回失败
    }
    if (buf.st_size > FILE_LENGTH_MAX) {     // 检查文件长度是否超过最大值
        PRINT_ERR("%s[%d], The file: %s length is out of limit!\n", __FUNCTION__, __LINE__, fileName);
        return LOS_NOK;                      // 返回失败
    }

    *fileLen = (UINT32)buf.st_size;          // 设置文件长度
    return LOS_OK;                           // 返回成功
}

/**
 * @brief 从ELF文件读取指定长度信息
 * @param procfd 进程文件描述符
 * @param buffer 数据缓冲区
 * @param readSize 读取大小
 * @param offset 读取偏移量
 * @return 成功返回LOS_OK，失败返回LOS_NOK
 */
STATIC INT32 OsReadELFInfo(INT32 procfd, UINT8 *buffer, size_t readSize, off_t offset)
{
    ssize_t byteNum;                         // 实际读取字节数
    off_t returnPos;                         // 定位后的位置
    INT32 fd = GetAssociatedSystemFd(procfd);  // 获取关联的系统文件描述符
    if (fd < 0) {                            // 无效的文件描述符
        PRINT_ERR("%s[%d], Invalid procfd!\n", __FUNCTION__, __LINE__);
        return LOS_NOK;                      // 返回失败
    }

    if (readSize > 0) {                      // 如果需要读取数据
        returnPos = lseek(fd, offset, SEEK_SET);  // 定位到指定偏移量
        if (returnPos != offset) {           // 定位失败
            PRINT_ERR("%s[%d], Failed to seek the position!, offset: %#x\n", __FUNCTION__, __LINE__, offset);
            return LOS_NOK;                  // 返回失败
        }

        byteNum = read(fd, buffer, readSize);  // 读取数据
        if (byteNum <= 0) {                  // 读取失败或读取字节数为0
            PRINT_ERR("%s[%d], Failed to read from offset: %#x!\n", __FUNCTION__, __LINE__, offset);
            return LOS_NOK;                  // 返回失败
        }
    }
    return LOS_OK;                           // 返回成功
}

/**
 * @brief 验证ELF文件头部有效性
 * @param ehdr ELF头部指针
 * @param fileLen 文件长度
 * @return 成功返回LOS_OK，失败返回LOS_NOK
 */
STATIC INT32 OsVerifyELFEhdr(const LD_ELF_EHDR *ehdr, UINT32 fileLen)
{
    if (memcmp(ehdr->elfIdent, LD_ELFMAG, LD_SELFMAG) != 0) {  // 检查ELF魔数
        PRINT_ERR("%s[%d], The file is not elf!\n", __FUNCTION__, __LINE__);
        return LOS_NOK;                      // 返回失败
    }
    if ((ehdr->elfType != LD_ET_EXEC) && (ehdr->elfType != LD_ET_DYN)) {  // 检查文件类型是否为可执行文件或共享对象
        PRINT_ERR("%s[%d], The type of file is not ET_EXEC or ET_DYN!\n", __FUNCTION__, __LINE__);
        return LOS_NOK;                      // 返回失败
    }
    if (ehdr->elfMachine != LD_EM_ARM) {     // 检查机器架构是否为ARM
        PRINT_ERR("%s[%d], The type of machine is not EM_ARM!\n", __FUNCTION__, __LINE__);
        return LOS_NOK;                      // 返回失败
    }
    if (ehdr->elfPhNum > ELF_PHDR_NUM_MAX) {  // 检查程序头数量是否超过最大值
        PRINT_ERR("%s[%d], The num of program header is out of limit\n", __FUNCTION__, __LINE__);
        return LOS_NOK;                      // 返回失败
    }
    if (ehdr->elfPhoff > fileLen) {          // 检查程序头偏移量是否超过文件长度
        PRINT_ERR("%s[%d], The offset of program header is invalid, elf file is bad\n", __FUNCTION__, __LINE__);
        return LOS_NOK;                      // 返回失败
    }
    if (OsIsBadUserAddress((VADDR_T)ehdr->elfEntry)) {  // 检查入口地址是否为有效的用户空间地址
        PRINT_ERR("%s[%d], The entry of program is invalid\n", __FUNCTION__, __LINE__);
        return LOS_NOK;                      // 返回失败
    }

    return LOS_OK;                           // 返回成功
}

/**
 * @brief 验证ELF程序头有效性
 * @param phdr 程序头指针
 * @return 成功返回LOS_OK，失败返回LOS_NOK
 */
STATIC INT32 OsVerifyELFPhdr(const LD_ELF_PHDR *phdr)
{
    if ((phdr->fileSize > FILE_LENGTH_MAX) || (phdr->offset > FILE_LENGTH_MAX)) {  // 检查文件大小和偏移量是否超过最大值
        PRINT_ERR("%s[%d], The size of phdr is out of limit\n", __FUNCTION__, __LINE__);
        return LOS_NOK;                      // 返回失败
    }
    if (phdr->memSize > MEM_SIZE_MAX) {      // 检查内存大小是否超过最大值
        PRINT_ERR("%s[%d], The mem size of phdr is out of limit\n", __FUNCTION__, __LINE__);
        return LOS_NOK;                      // 返回失败
    }
    if (OsIsBadUserAddress((VADDR_T)phdr->vAddr)) {  // 检查虚拟地址是否为有效的用户空间地址
        PRINT_ERR("%s[%d], The vaddr of phdr is invalid\n", __FUNCTION__, __LINE__);
        return LOS_NOK;                      // 返回失败
    }

    return LOS_OK;                           // 返回成功
}

/**
 * @brief 初始化ELF加载信息
 * @param loadInfo ELF加载信息结构体指针
 */
STATIC VOID OsLoadInit(ELFLoadInfo *loadInfo)
{
#ifdef LOSCFG_FS_VFS
    const struct files_struct *oldFiles = OsCurrProcessGet()->files;  // 获取当前进程的文件结构体
    loadInfo->oldFiles = (UINTPTR)create_files_snapshot(oldFiles);    // 创建文件快照
#else
    loadInfo->oldFiles = NULL;               // 非VFS配置，文件快照为空
#endif
    loadInfo->execInfo.procfd = INVALID_FD;  // 初始化可执行文件描述符为无效
    loadInfo->interpInfo.procfd = INVALID_FD;  // 初始化解释器文件描述符为无效
}

/**
 * @brief 读取并验证ELF头部
 * @param fileName 文件名
 * @param elfInfo ELF信息结构体指针
 * @param isExecFile 是否为可执行文件
 * @return 成功返回LOS_OK，失败返回错误码
 */
STATIC INT32 OsReadEhdr(const CHAR *fileName, ELFInfo *elfInfo, BOOL isExecFile)
{
    INT32 ret;                               // 返回值

    ret = OsGetFileLength(&elfInfo->fileLen, fileName);  // 获取文件长度
    if (ret != LOS_OK) {                     // 获取失败
        return -ENOENT;                      // 返回文件不存在错误
    }

    ret = OsELFOpen(fileName, O_RDONLY | O_EXECVE | O_CLOEXEC);  // 以只读、执行和关闭执行标志打开文件
    if (ret < 0) {                           // 打开失败
        PRINT_ERR("%s[%d], Failed to open ELF file: %s!\n", __FUNCTION__, __LINE__, fileName);
        return ret;                          // 返回错误码
    }
    elfInfo->procfd = ret;                   // 设置进程文件描述符

#ifdef LOSCFG_DRIVERS_TZDRIVER
    if (isExecFile) {                        // 如果是可执行文件且启用TZ驱动
        struct file *filep = NULL;           // 文件结构体指针
        ret = fs_getfilep(GetAssociatedSystemFd(elfInfo->procfd), &filep);  // 获取文件结构体
        if (ret) {                           // 获取失败
            PRINT_ERR("%s[%d], Failed to get struct file %s!\n", __FUNCTION__, __LINE__, fileName);
            /* 文件将由OsLoadELFFile关闭 */
            return ret;                      // 返回错误码
        }
        OsCurrProcessGet()->execVnode = filep->f_vnode;  // 设置当前进程的执行虚拟节点
    }
#endif
    ret = OsReadELFInfo(elfInfo->procfd, (UINT8 *)&elfInfo->elfEhdr, sizeof(LD_ELF_EHDR), 0);  // 读取ELF头部
    if (ret != LOS_OK) {                     // 读取失败
        PRINT_ERR("%s[%d]\n", __FUNCTION__, __LINE__);
        return -EIO;                         // 返回I/O错误
    }

    ret = OsVerifyELFEhdr(&elfInfo->elfEhdr, elfInfo->fileLen);  // 验证ELF头部
    if (ret != LOS_OK) {                     // 验证失败
        PRINT_ERR("%s[%d]\n", __FUNCTION__, __LINE__);
        return isExecFile ? -ENOEXEC : -ELIBBAD;  // 可执行文件返回执行错误，库文件返回库错误
    }

    return LOS_OK;                           // 返回成功
}
/**
 * @brief 读取ELF文件的程序头表
 * @param elfInfo ELF文件信息结构体指针
 * @param isExecFile 是否为可执行文件标志
 * @return 成功返回LOS_OK，失败返回错误码
 */
STATIC INT32 OsReadPhdrs(ELFInfo *elfInfo, BOOL isExecFile)
{
    LD_ELF_EHDR *elfEhdr = &elfInfo->elfEhdr;  // ELF文件头指针
    UINT32 size;                              // 程序头表总大小
    INT32 ret;                                // 函数返回值

    if (elfEhdr->elfPhNum < 1) {              // 检查程序头数量是否有效
        goto OUT;                             // 无效则跳转到错误处理
    }

    if (elfEhdr->elfPhEntSize != sizeof(LD_ELF_PHDR)) {  // 检查程序头表项大小是否匹配
        goto OUT;                                       // 不匹配则跳转到错误处理
    }

    size = sizeof(LD_ELF_PHDR) * elfEhdr->elfPhNum;     // 计算程序头表总大小
    if ((elfEhdr->elfPhoff + size) > elfInfo->fileLen) { // 检查程序头表是否超出文件范围
        goto OUT;                                       // 超出则跳转到错误处理
    }

    elfInfo->elfPhdr = LOS_MemAlloc(m_aucSysMem0, size); // 分配内存存储程序头表
    if (elfInfo->elfPhdr == NULL) {                      // 检查内存分配是否成功
        PRINT_ERR("%s[%d], Failed to allocate for elfPhdr!\n", __FUNCTION__, __LINE__);
        return -ENOMEM;                                 // 分配失败返回内存不足错误
    }

    ret = OsReadELFInfo(elfInfo->procfd, (UINT8 *)elfInfo->elfPhdr, size, elfEhdr->elfPhoff); // 读取程序头表
    if (ret != LOS_OK) {                                                                      // 检查读取是否成功
        (VOID)LOS_MemFree(m_aucSysMem0, elfInfo->elfPhdr);                                   // 读取失败释放已分配内存
        elfInfo->elfPhdr = NULL;                                                              // 指针置空
        PRINT_ERR("%s[%d]\n", __FUNCTION__, __LINE__);
        return -EIO;                                                                         // 返回I/O错误
    }

    return LOS_OK;                                                                             // 成功返回
OUT:
    PRINT_ERR("%s[%d], elf file is bad!\n", __FUNCTION__, __LINE__);
    return isExecFile ? -ENOEXEC : -ELIBBAD;  // 根据文件类型返回不同错误码
}

/**
 * @brief 读取ELF文件的解释器信息
 * @param loadInfo ELF加载信息结构体指针
 * @return 成功返回LOS_OK，失败返回错误码
 */
STATIC INT32 OsReadInterpInfo(ELFLoadInfo *loadInfo)
{
    LD_ELF_PHDR *elfPhdr = loadInfo->execInfo.elfPhdr;  // 程序头表指针
    CHAR *elfInterpName = NULL;                         // 解释器名称缓冲区
    INT32 ret, i;                                       // 返回值和循环变量

    for (i = 0; i < loadInfo->execInfo.elfEhdr.elfPhNum; ++i, ++elfPhdr) { // 遍历程序头表
        if (elfPhdr->type != LD_PT_INTERP) {                              // 查找解释器段
            continue;                                                     // 不是解释器段则继续
        }

        if (OsVerifyELFPhdr(elfPhdr) != LOS_OK) {                          // 验证程序头有效性
            return -ENOEXEC;                                               // 验证失败返回执行错误
        }

        if ((elfPhdr->fileSize > FILE_PATH_MAX) || (elfPhdr->fileSize < FILE_PATH_MIN) ||  // 检查解释器名称长度
            (elfPhdr->offset + elfPhdr->fileSize > loadInfo->execInfo.fileLen)) {          // 检查是否超出文件范围
            PRINT_ERR("%s[%d], The size of file is out of limit!\n", __FUNCTION__, __LINE__);
            return -ENOEXEC;                                                               // 超出范围返回执行错误
        }

        elfInterpName = LOS_MemAlloc(m_aucSysMem0, elfPhdr->fileSize);     // 分配解释器名称缓冲区
        if (elfInterpName == NULL) {                                       // 检查内存分配是否成功
            PRINT_ERR("%s[%d], Failed to allocate for elfInterpName!\n", __FUNCTION__, __LINE__);
            return -ENOMEM;                                                // 分配失败返回内存不足错误
        }

        ret = OsReadELFInfo(loadInfo->execInfo.procfd, (UINT8 *)elfInterpName, elfPhdr->fileSize, elfPhdr->offset); // 读取解释器名称
        if (ret != LOS_OK) {                                                                                       // 检查读取是否成功
            PRINT_ERR("%s[%d]\n", __FUNCTION__, __LINE__);
            ret = -EIO;                                                                                            // 设置I/O错误码
            goto OUT;                                                                                              // 跳转到错误处理
        }

        if (elfInterpName[elfPhdr->fileSize - 1] != '\0') {  // 检查解释器名称是否以null结尾
            PRINT_ERR("%s[%d], The name of interpreter is invalid!\n", __FUNCTION__, __LINE__);
            ret = -ENOEXEC;                                 // 无效名称返回执行错误
            goto OUT;                                       // 跳转到错误处理
        }

        ret = OsReadEhdr(INTERP_FULL_PATH, &loadInfo->interpInfo, FALSE);  // 读取解释器的ELF头
        if (ret != LOS_OK) {                                              // 检查读取是否成功
            PRINT_ERR("%s[%d]\n", __FUNCTION__, __LINE__);
            goto OUT;                                                    // 跳转到错误处理
        }

        ret = OsReadPhdrs(&loadInfo->interpInfo, FALSE);  // 读取解释器的程序头表
        if (ret != LOS_OK) {                              // 检查读取是否成功
            goto OUT;                                    // 跳转到错误处理
        }

        (VOID)LOS_MemFree(m_aucSysMem0, elfInterpName);  // 释放解释器名称缓冲区
        break;                                          // 找到并处理完成，跳出循环
    }

    return LOS_OK;                                       // 成功返回

OUT:
    (VOID)LOS_MemFree(m_aucSysMem0, elfInterpName);      // 错误处理：释放解释器名称缓冲区
    return ret;                                          // 返回错误码
}

/**
 * @brief 将ELF段标志转换为内存保护属性
 * @param pFlags ELF段标志
 * @return 对应的内存保护属性
 */
STATIC UINT32 OsGetProt(UINT32 pFlags)
{
    UINT32 prot;  // 内存保护属性

    // 根据段标志计算内存保护属性（读、写、执行权限组合）
    prot = (((pFlags & PF_R) ? PROT_READ : 0) |
            ((pFlags & PF_W) ? PROT_WRITE : 0) |
            ((pFlags & PF_X) ? PROT_EXEC : 0));
    return prot;  // 返回计算得到的内存保护属性
}

/**
 * @brief 计算加载ELF段所需的内存分配大小
 * @param elfPhdr 程序头表指针
 * @param phdrNum 程序头数量
 * @return 所需内存大小，失败返回0
 */
STATIC UINT32 OsGetAllocSize(const LD_ELF_PHDR *elfPhdr, INT32 phdrNum)
{
    const LD_ELF_PHDR *elfPhdrTemp = elfPhdr;  // 程序头遍历指针
    UINTPTR addrMin = SIZE_MAX;                // 最小虚拟地址
    UINTPTR addrMax = 0;                       // 最大虚拟地址
    UINT32 offStart = 0;                       // 起始偏移量
    UINT64 size;                               // 计算得到的大小
    INT32 i;                                   // 循环变量

    for (i = 0; i < phdrNum; ++i, ++elfPhdrTemp) {  // 遍历所有程序头
        if (elfPhdrTemp->type != LD_PT_LOAD) {      // 只处理可加载段
            continue;
        }

        if (OsVerifyELFPhdr(elfPhdrTemp) != LOS_OK) {  // 验证程序头有效性
            return 0;                                 // 验证失败返回0
        }

        if (elfPhdrTemp->vAddr < addrMin) {           // 更新最小虚拟地址
            addrMin = elfPhdrTemp->vAddr;
            offStart = elfPhdrTemp->offset;           // 记录对应偏移量
        }
        if ((elfPhdrTemp->vAddr + elfPhdrTemp->memSize) > addrMax) {  // 更新最大虚拟地址
            addrMax = elfPhdrTemp->vAddr + elfPhdrTemp->memSize;
        }
    }

    // 检查地址范围有效性
    if (OsIsBadUserAddress((VADDR_T)addrMax) || OsIsBadUserAddress((VADDR_T)addrMin) || (addrMax < addrMin)) {
        return 0;  // 地址无效返回0
    }
    // 计算内存分配大小（按页对齐）
    size = ROUNDUP(addrMax, PAGE_SIZE) - ROUNDDOWN(addrMin, PAGE_SIZE) + ROUNDDOWN(offStart, PAGE_SIZE);

    return (size > UINT_MAX) ? 0 : (UINT32)size;  // 检查溢出并返回结果
}

/**
 * @brief 将ELF段映射到进程地址空间
 * @param fd 文件描述符
 * @param addr 映射起始地址
 * @param elfPhdr 程序头指针
 * @param prot 内存保护属性
 * @param flags 映射标志
 * @param mapSize 映射大小
 * @return 映射后的地址，失败返回0
 */
STATIC UINTPTR OsDoMmapFile(INT32 fd, UINTPTR addr, const LD_ELF_PHDR *elfPhdr, UINT32 prot, UINT32 flags,
                            UINT32 mapSize)
{
    UINTPTR mapAddr;  // 映射结果地址
    UINT32 size;      // 映射大小
    // 计算文件偏移量（页对齐）
    UINT32 offset = elfPhdr->offset - ROUNDOFFSET(elfPhdr->vAddr, PAGE_SIZE);
    addr = ROUNDDOWN(addr, PAGE_SIZE);  // 地址页对齐

    if (mapSize != 0) {  // 如果指定了映射大小
        // 使用指定大小进行内存映射
        mapAddr = (UINTPTR)LOS_MMap(addr, mapSize, prot, flags, fd, offset >> PAGE_SHIFT);
    } else {  // 未指定映射大小
        // 计算映射大小（包含页内偏移）
        size = elfPhdr->memSize + ROUNDOFFSET(elfPhdr->vAddr, PAGE_SIZE);
        if (size == 0) {  // 大小为0则直接返回地址
            return addr;
        }
        // 使用计算得到的大小进行内存映射
        mapAddr = (UINTPTR)LOS_MMap(addr, size, prot, flags, fd, offset >> PAGE_SHIFT);
    }
    if (!LOS_IsUserAddress((VADDR_T)mapAddr)) {  // 检查映射地址是否为有效用户地址
        PRINT_ERR("%s %d, Failed to map a valid addr\n", __FUNCTION__, __LINE__);
        return 0;  // 映射失败返回0
    }
    return mapAddr;  // 返回映射地址
}

/**
 * @brief 将用户虚拟地址转换为内核虚拟地址
 * @param space 进程地址空间结构体
 * @param vaddr 用户虚拟地址
 * @param kvaddr 输出参数，存储转换后的内核虚拟地址
 * @return 成功返回LOS_OK，失败返回LOS_NOK
 */
INT32 OsGetKernelVaddr(LosVmSpace *space, VADDR_T vaddr, VADDR_T *kvaddr)
{
    INT32 ret;       // 返回值
    PADDR_T paddr = 0;  // 物理地址

    if ((space == NULL) || (vaddr == 0) || (kvaddr == NULL)) {  // 检查输入参数有效性
        PRINT_ERR("%s[%d], space: %#x, vaddr: %#x\n", __FUNCTION__, __LINE__, space, vaddr);
        return LOS_NOK;  // 参数无效返回失败
    }

    if (LOS_IsKernelAddress(vaddr)) {  // 如果已是内核地址
        *kvaddr = vaddr;               // 直接赋值
        return LOS_OK;                 // 返回成功
    }

    // 查询MMU获取物理地址
    ret = LOS_ArchMmuQuery(&space->archMmu, vaddr, &paddr, NULL);
    if (ret != LOS_OK) {  // 检查查询是否成功
        PRINT_ERR("%s[%d], Failed to query the vaddr: %#x, status: %d\n", __FUNCTION__, __LINE__, vaddr, ret);
        return LOS_NOK;   // 查询失败返回
    }
    *kvaddr = (VADDR_T)(UINTPTR)LOS_PaddrToKVaddr(paddr);  // 物理地址转换为内核虚拟地址
    if (*kvaddr == 0) {  // 检查转换结果
        PRINT_ERR("%s[%d], kvaddr is null\n", __FUNCTION__, __LINE__);
        return LOS_NOK;  // 转换失败返回
    }

    return LOS_OK;  // 成功返回
}

/**
 * @brief 初始化BSS段（清零未初始化数据段）
 * @param elfPhdr 程序头指针
 * @param fd 文件描述符
 * @param bssStart BSS段起始地址
 * @param bssEnd BSS段结束地址
 * @param elfProt 内存保护属性
 * @return 成功返回LOS_OK，失败返回错误码
 */
STATIC INT32 OsSetBss(const LD_ELF_PHDR *elfPhdr, INT32 fd, UINTPTR bssStart, UINT32 bssEnd, UINT32 elfProt)
{
    UINTPTR bssStartPageAlign, bssEndPageAlign;  // BSS段页对齐后的起始和结束地址
    UINTPTR mapBase;                             // 映射基地址
    UINT32 bssMapSize;                           // BSS段映射大小
    INT32 stackFlags;                            // 映射标志
    INT32 ret;                                   // 返回值

    bssStartPageAlign = ROUNDUP(bssStart, PAGE_SIZE);  // BSS段起始地址页对齐
    bssEndPageAlign = ROUNDUP(bssEnd, PAGE_SIZE);      // BSS段结束地址页对齐

    // 清零BSS段起始页的非对齐部分
    ret = LOS_UserMemClear((VOID *)bssStart, PAGE_SIZE - ROUNDOFFSET(bssStart, PAGE_SIZE));
    if (ret != 0) {  // 检查清零操作是否成功
        PRINT_ERR("%s[%d], Failed to clear bss\n", __FUNCTION__, __LINE__);
        return -EFAULT;  // 失败返回错误码
    }

    bssMapSize = bssEndPageAlign - bssStartPageAlign;  // 计算BSS段需要映射的大小
    if (bssMapSize > 0) {  // 如果需要映射的大小大于0
        stackFlags = MAP_PRIVATE | MAP_FIXED | MAP_ANONYMOUS;  // 设置映射标志：私有、固定地址、匿名映射
        // 创建匿名映射用于BSS段
        mapBase = (UINTPTR)LOS_MMap(bssStartPageAlign, bssMapSize, elfProt, stackFlags, -1, 0);
        if (!LOS_IsUserAddress((VADDR_T)mapBase)) {  // 检查映射地址是否有效
            PRINT_ERR("%s[%d], Failed to map bss\n", __FUNCTION__, __LINE__);
            return -ENOMEM;  // 映射失败返回内存不足错误
        }
    }

    return LOS_OK;  // 成功返回
}
/**
 * @brief 将ELF文件段映射到进程地址空间
 * @param procfd 进程文件描述符
 * @param elfPhdr ELF程序头表指针
 * @param elfEhdr ELF文件头指针
 * @param elfLoadAddr 输出参数，ELF加载地址
 * @param mapSize 映射大小
 * @param loadBase 输出参数，加载基地址
 * @return 成功返回LOS_OK，失败返回错误码
 */
STATIC INT32 OsMmapELFFile(INT32 procfd, const LD_ELF_PHDR *elfPhdr, const LD_ELF_EHDR *elfEhdr, UINTPTR *elfLoadAddr,
                           UINT32 mapSize, UINTPTR *loadBase)
{
    const LD_ELF_PHDR *elfPhdrTemp = elfPhdr;  // 程序头遍历指针
    UINTPTR vAddr, mapAddr, bssStart;          // 虚拟地址、映射地址、BSS段起始地址
    UINT32 bssEnd, elfProt, elfFlags;          // BSS段结束地址、内存保护属性、映射标志
    INT32 ret, i;                              // 返回值和循环变量
    INT32 fd = GetAssociatedSystemFd(procfd);  // 获取关联的系统文件描述符

    for (i = 0; i < elfEhdr->elfPhNum; ++i, ++elfPhdrTemp) {  // 遍历所有程序头
        if (elfPhdrTemp->type != LD_PT_LOAD) {                // 跳过非可加载段
            continue;
        }
        if (elfEhdr->elfType == LD_ET_EXEC) {                 // 如果是可执行文件
            if (OsVerifyELFPhdr(elfPhdrTemp) != LOS_OK) {     // 验证程序头有效性
                return -ENOEXEC;                              // 验证失败返回执行错误
            }
        }

        elfProt = OsGetProt(elfPhdrTemp->flags);  // 获取内存保护属性
        if ((elfProt & PROT_READ) == 0) {         // 检查是否有读权限
            return -ENOEXEC;                      // 无读权限返回执行错误
        }
        elfFlags = MAP_PRIVATE | MAP_FIXED;       // 设置映射标志：私有映射、固定地址
        vAddr = elfPhdrTemp->vAddr;               // 获取段虚拟地址
        if ((vAddr == 0) && (*loadBase == 0)) {   // 如果虚拟地址和加载基地址都为0
            elfFlags &= ~MAP_FIXED;               // 清除固定地址标志
        }

        // 执行内存映射
        mapAddr = OsDoMmapFile(fd, (vAddr + *loadBase), elfPhdrTemp, elfProt, elfFlags, mapSize);
        if (!LOS_IsUserAddress((VADDR_T)mapAddr)) {  // 检查映射地址是否为有效用户地址
            return -ENOMEM;                          // 映射失败返回内存不足错误
        }
#ifdef LOSCFG_DRIVERS_TZDRIVER  // 如果启用TZ驱动
        // 如果是只读可执行段（代码段）
        if ((elfPhdrTemp->flags & PF_R) && (elfPhdrTemp->flags & PF_X) && !(elfPhdrTemp->flags & PF_W)) {
            SetVmmRegionCodeStart(vAddr + *loadBase, elfPhdrTemp->memSize);  // 设置代码段起始地址
        }
#endif
        mapSize = 0;  // 重置映射大小，后续段使用默认大小

        if (*elfLoadAddr == 0) {  // 如果尚未设置加载地址
            // 计算并设置ELF加载地址（页内偏移调整）
            *elfLoadAddr = mapAddr + ROUNDOFFSET(vAddr, PAGE_SIZE);
        }

        // 如果是动态链接文件且尚未设置加载基地址
        if ((*loadBase == 0) && (elfEhdr->elfType == LD_ET_DYN)) {
            *loadBase = mapAddr;  // 设置加载基地址
        }

        // 如果段内存大小大于文件大小且有写权限（存在BSS段）
        if ((elfPhdrTemp->memSize > elfPhdrTemp->fileSize) && (elfPhdrTemp->flags & PF_W)) {
            // 计算BSS段起始和结束地址
            bssStart = mapAddr + ROUNDOFFSET(vAddr, PAGE_SIZE) + elfPhdrTemp->fileSize;
            bssEnd = mapAddr + ROUNDOFFSET(vAddr, PAGE_SIZE) + elfPhdrTemp->memSize;
            // 初始化BSS段（清零）
            ret = OsSetBss(elfPhdrTemp, fd, bssStart, bssEnd, elfProt);
            if (ret != LOS_OK) {
                return ret;  // BSS初始化失败返回错误
            }
        }
    }

    return LOS_OK;  // 所有段映射完成，返回成功
}

/**
 * @brief 加载ELF解释器（动态链接器）
 * @param loadInfo ELF加载信息结构体指针
 * @param interpMapBase 输出参数，解释器映射基地址
 * @return 成功返回LOS_OK，失败返回错误码
 */
STATIC INT32 OsLoadInterpBinary(ELFLoadInfo *loadInfo, UINTPTR *interpMapBase)
{
    UINTPTR loadBase = 0;  // 加载基地址
    UINT32 mapSize;        // 映射大小
    INT32 ret;             // 返回值

    // 计算解释器所需内存分配大小
    mapSize = OsGetAllocSize(loadInfo->interpInfo.elfPhdr, loadInfo->interpInfo.elfEhdr.elfPhNum);
    if (mapSize == 0) {    // 检查分配大小是否有效
        PRINT_ERR("%s[%d], Failed to get interp allocation size!\n", __FUNCTION__, __LINE__);
        return -EINVAL;    // 无效大小返回参数错误
    }

    // 将解释器映射到内存
    ret = OsMmapELFFile(loadInfo->interpInfo.procfd, loadInfo->interpInfo.elfPhdr, &loadInfo->interpInfo.elfEhdr,
                        interpMapBase, mapSize, &loadBase);
    if (ret != LOS_OK) {   // 检查映射是否成功
        PRINT_ERR("%s[%d]\n", __FUNCTION__, __LINE__);
    }

    return ret;  // 返回映射结果
}

/**
 * @brief 获取参数指针（支持内核/用户空间地址）
 * @param ptr 参数数组指针
 * @param index 参数索引
 * @return 参数指针，失败返回NULL
 */
STATIC CHAR *OsGetParamPtr(CHAR * const *ptr, INT32 index)
{
    UINTPTR userStrPtr = 0;  // 用户空间字符串指针
    INT32 ret;               // 返回值

    if (ptr == NULL) {       // 检查输入指针是否为空
        return NULL;
    }

    if (LOS_IsKernelAddress((UINTPTR)ptr)) {  // 如果是内核地址
        return ptr[index];                    // 直接返回数组元素
    }
    // 从用户空间读取指针值
    ret = LOS_GetUser(&userStrPtr, (UINTPTR *)(ptr + index));
    if (ret != LOS_OK) {                      // 检查读取是否成功
        PRINT_ERR("%s[%d], %#x\n", __FUNCTION__, __LINE__, ptr);
        return NULL;
    }

    return (CHAR *)userStrPtr;  // 返回用户空间字符串指针
}

/**
 * @brief 将参数值写入用户栈
 * @param val 要写入的值
 * @param sp 栈指针
 * @return 成功返回LOS_OK，失败返回错误码
 */
STATIC INT32 OsPutUserArg(INT32 val, const UINTPTR *sp)
{
    INT32 ret;  // 返回值

    if (sp == NULL) {  // 检查栈指针是否为空
        return LOS_NOK;
    }

    // 将值写入用户栈
    ret = LOS_PutUser((INT32 *)&val, (INT32 *)sp);
    if (ret != LOS_OK) {  // 检查写入是否成功
        PRINT_ERR("%s[%d]\n", __FUNCTION__, __LINE__);
        return -EFAULT;  // 写入失败返回错误
    }

    return LOS_OK;  // 成功返回
}

/**
 * @brief 将参数数组写入用户栈
 * @param strPtr 字符串指针数组
 * @param sp 栈指针（输出参数，会被更新）
 * @param count 参数数量
 * @return 成功返回LOS_OK，失败返回错误码
 */
STATIC INT32 OsPutUserArgv(UINTPTR *strPtr, UINTPTR **sp, INT32 count)
{
    INT32 len;       // 字符串长度
    INT32 i;         // 循环变量
    CHAR *ptr = NULL;  // 字符串指针

    if ((strPtr == NULL) || (sp == NULL)) {  // 检查输入参数
        return LOS_NOK;
    }

    for (i = 0; i < count; ++i) {  // 遍历所有参数
        /* 将参数字符串地址写入用户栈 */
        if (OsPutUserArg(*strPtr, *sp) != LOS_OK) {
            return LOS_NOK;  // 写入失败返回
        }
        ptr = OsGetParamPtr((CHAR **)strPtr, 0);  // 获取参数字符串
        if (ptr == NULL) {
            return LOS_NOK;  // 获取失败返回
        }
        len = LOS_StrnlenUser(ptr, PATH_MAX);  // 获取字符串长度
        if (len == 0) {
            return LOS_NOK;  // 空字符串返回错误
        }
        *strPtr += len;  // 更新字符串指针到下一个参数
        ++(*sp);         // 更新栈指针
    }
    /* 在参数数组末尾写入NULL终止符 */
    if (OsPutUserArg(0, *sp) != LOS_OK) {
        return LOS_NOK;
    }
    ++(*sp);  // 栈指针最后调整

    return LOS_OK;  // 成功返回
}

/**
 * @brief 将命令行参数复制到用户栈
 * @param loadInfo ELF加载信息结构体指针
 * @param argc 参数数量
 * @param argv 参数数组
 * @return 成功返回LOS_OK，失败返回错误码
 */
STATIC INT32 OsCopyParams(ELFLoadInfo *loadInfo, INT32 argc, CHAR *const *argv)
{
    CHAR *strPtr = NULL;       // 字符串指针
    UINT32 offset, strLen;     // 偏移量和字符串长度
    errno_t err;               // 错误码
    INT32 ret, i;              // 返回值和循环变量
    vaddr_t kvaddr = 0;        // 内核虚拟地址

    if ((argc > 0) && (argv == NULL)) {  // 参数数量大于0但参数数组为空
        return -EINVAL;                  // 返回参数错误
    }

    // 将用户栈地址转换为内核虚拟地址
    ret = OsGetKernelVaddr(loadInfo->newSpace, loadInfo->stackParamBase, &kvaddr);
    if (ret != LOS_OK) {                 // 检查转换是否成功
        PRINT_ERR("%s[%d]\n", __FUNCTION__, __LINE__);
        return -EFAULT;                  // 转换失败返回错误
    }

    for (i = argc - 1; i >= 0; --i) {  // 逆序遍历参数（栈是向下增长的）
        strPtr = OsGetParamPtr(argv, i);  // 获取参数指针
        if (LOS_IsKernelAddress((VADDR_T)(UINTPTR)strPtr)) {  // 内核空间字符串
            strLen = strlen(strPtr) + 1;                      // 计算长度（包含终止符）
        } else {                                              // 用户空间字符串
            strLen = LOS_StrnlenUser(strPtr, PATH_MAX);       // 安全计算长度
        }
        if (strLen < 1) {  // 无效字符串长度
            continue;
        }

        offset = loadInfo->topOfMem - loadInfo->stackParamBase;  // 计算当前偏移
        if (offset < strLen) {  // 检查栈空间是否足够
            PRINT_ERR("%s[%d], The size of param is out of limit: %#x bytes!\n", __FUNCTION__, __LINE__,
                      USER_PARAM_BYTE_MAX);
            return -E2BIG;  // 参数过大返回错误
        }
        loadInfo->topOfMem -= strLen;  // 调整栈顶指针
        offset -= strLen;              // 调整偏移量

        /* 将字符串复制到用户栈 */
        if (LOS_IsKernelAddress((VADDR_T)(UINTPTR)strPtr)) {
            // 从内核空间复制
            err = memcpy_s((VOID *)(UINTPTR)(kvaddr + offset), strLen, strPtr, strLen);
        } else {
            // 从用户空间复制
            err = LOS_ArchCopyFromUser((VOID *)(UINTPTR)(kvaddr + offset), strPtr, strLen);
        }

        if (err != EOK) {  // 检查复制是否成功
            PRINT_ERR("%s[%d], copy strings failed! err: %d\n", __FUNCTION__, __LINE__, err);
            return -EFAULT;  // 复制失败返回错误
        }
    }

    return LOS_OK;  // 所有参数复制完成
}

/**
 * @brief 获取参数数量（处理用户空间参数）
 * @param argv 参数数组
 * @return 参数数量，失败返回0
 */
STATIC INT32 OsGetParamNum(CHAR *const *argv)
{
    CHAR *argPtr = NULL;  // 参数指针
    INT32 count = 0;      // 参数计数
    INT32 ret;            // 返回值

    if (argv == NULL) {   // 参数数组为空
        return count;
    }

    argPtr = OsGetParamPtr(argv, count);  // 获取第一个参数
    while (argPtr != NULL) {              // 遍历所有参数
        ret = LOS_StrnlenUser(argPtr, PATH_MAX);  // 获取参数长度
        if ((ret == 0) || (ret > PATH_MAX)) {     // 长度无效
            PRINT_ERR("%s[%d], the len of string of argv is invalid, index: %d, len: %d\n", __FUNCTION__,
                      __LINE__, count, ret);
            break;
        }
        ++count;  // 参数计数加1
        if (count >= STRINGS_COUNT_MAX) {  // 检查参数数量是否超限
            break;
        }
        argPtr = OsGetParamPtr(argv, count);  // 获取下一个参数
    }

    return count;  // 返回参数数量
}

/**
 * @brief 获取随机偏移量（用于ASLR地址空间布局随机化）
 * @param randomDevFD 随机设备文件描述符
 * @return 页对齐的随机偏移量
 */
UINT32 OsGetRndOffset(INT32 randomDevFD)
{
    UINT32 randomValue = 0;  // 随机值

#ifdef LOSCFG_ASLR  // 如果启用ASLR
    // 从随机设备读取随机值
    if (read(randomDevFD, &randomValue, sizeof(UINT32)) == sizeof(UINT32)) {
        randomValue &= RANDOM_MASK;  // 应用掩码限制范围
    } else {
        // 读取失败时使用伪随机数
        randomValue = (UINT32)random() & RANDOM_MASK;
    }
#else
    (VOID)randomDevFD;  // 未启用ASLR，忽略参数
#endif

    return ROUNDDOWN(randomValue, PAGE_SIZE);  // 返回页对齐的随机偏移量
}

/**
 * @brief 获取栈内存保护属性
 * @param loadInfo ELF加载信息结构体指针
 */
STATIC VOID OsGetStackProt(ELFLoadInfo *loadInfo)
{
    LD_ELF_PHDR *elfPhdrTemp = loadInfo->execInfo.elfPhdr;  // 程序头遍历指针
    INT32 i;                                                // 循环变量

    // 遍历程序头查找GNU_STACK段
    for (i = 0; i < loadInfo->execInfo.elfEhdr.elfPhNum; ++i, ++elfPhdrTemp) {
        if (elfPhdrTemp->type == LD_PT_GNU_STACK) {  // 找到栈段
            // 设置栈保护属性
            loadInfo->stackProt = OsGetProt(elfPhdrTemp->flags);
        }
    }
}

/**
 * @brief 分配并映射栈内存
 * @param space 虚拟地址空间结构体
 * @param vaddr 虚拟地址
 * @param vsize 虚拟大小
 * @param psize 物理大小
 * @param regionFlags 内存区域标志
 * @return 成功返回LOS_OK，失败返回LOS_NOK
 */
STATIC UINT32 OsStackAlloc(LosVmSpace *space, VADDR_T vaddr, UINT32 vsize, UINT32 psize, UINT32 regionFlags)
{
    LosVmPage *vmPage = NULL;          // 虚拟内存页
    VADDR_T *kvaddr = NULL;            // 内核虚拟地址
    LosVmMapRegion *region = NULL;     // 虚拟内存区域
    VADDR_T vaddrTemp;                 // 临时虚拟地址
    PADDR_T paddrTemp;                 // 临时物理地址
    UINT32 len;                        // 长度

    (VOID)LOS_MuxAcquire(&space->regionMux);  // 获取地址空间锁
    // 分配连续物理页
    kvaddr = LOS_PhysPagesAllocContiguous(psize >> PAGE_SHIFT);
    if (kvaddr == NULL) {                     // 分配失败
        goto OUT;
    }

    // 分配虚拟内存区域
    region = LOS_RegionAlloc(space, vaddr, vsize, regionFlags | VM_MAP_REGION_FLAG_FIXED, 0);
    if (region == NULL) {                     // 区域分配失败
        goto PFREE;
    }

    len = psize;                              // 物理内存长度
    // 计算栈起始虚拟地址（栈从高地址向低地址增长）
    vaddrTemp = region->range.base + vsize - psize;
    paddrTemp = LOS_PaddrQuery(kvaddr);       // 获取物理地址
    while (len > 0) {                         // 映射所有物理页
        vmPage = LOS_VmPageGet(paddrTemp);    // 获取物理页
        LOS_AtomicInc(&vmPage->refCounts);    // 增加引用计数

        // 建立页表映射
        (VOID)LOS_ArchMmuMap(&space->archMmu, vaddrTemp, paddrTemp, 1, region->regionFlags);

        paddrTemp += PAGE_SIZE;  // 下一个物理页
        vaddrTemp += PAGE_SIZE;  // 下一个虚拟页
        len -= PAGE_SIZE;        // 剩余长度
    }
    (VOID)LOS_MuxRelease(&space->regionMux);  // 释放地址空间锁
    return LOS_OK;                            // 成功返回

PFREE:
    (VOID)LOS_PhysPagesFreeContiguous(kvaddr, psize >> PAGE_SHIFT);  // 释放物理页
OUT:
    (VOID)LOS_MuxRelease(&space->regionMux);  // 释放地址空间锁
    return LOS_NOK;                            // 失败返回
}
/**
 * @brief 设置ELF程序的参数和环境变量，并分配用户栈空间
 * @param loadInfo ELF加载信息结构体指针
 * @param argv 命令行参数数组
 * @param envp 环境变量数组
 * @return 成功返回LOS_OK，失败返回错误码
 */
STATIC INT32 OsSetArgParams(ELFLoadInfo *loadInfo, CHAR *const *argv, CHAR *const *envp)
{
    UINT32 vmFlags;                                                              // 虚拟内存区域标志位
    INT32 ret;                                                                  // 函数返回值

    // 打开随机设备，用于生成随机偏移量
    loadInfo->randomDevFD = open("/dev/urandom", O_RDONLY);
    if (loadInfo->randomDevFD < 0) {                                             // 随机设备打开失败时
        if (!g_srandInit) {                                                     // 如果随机数种子未初始化
            srand((UINT32)time(NULL));                                          // 使用当前时间初始化随机数种子
            g_srandInit = TRUE;                                                 // 标记随机数种子已初始化
        }
    }

    (VOID)OsGetStackProt(loadInfo);                                              // 获取栈内存保护属性
    // 检查栈内存是否同时具有读写权限
    if (((UINT32)loadInfo->stackProt & (PROT_READ | PROT_WRITE)) != (PROT_READ | PROT_WRITE)) {
        return -ENOEXEC;                                                        // 栈权限不足，返回执行错误
    }
    // 计算栈顶最大值，减去随机偏移量增强安全性
    loadInfo->stackTopMax = USER_STACK_TOP_MAX - OsGetRndOffset(loadInfo->randomDevFD);
    loadInfo->stackBase = loadInfo->stackTopMax - USER_STACK_SIZE;               // 计算栈基地址
    loadInfo->stackSize = USER_STACK_SIZE;                                       // 设置栈大小
    // 计算参数区域基地址（栈顶下方预留参数空间）
    loadInfo->stackParamBase = loadInfo->stackTopMax - USER_PARAM_BYTE_MAX;
    // 将保护标志转换为虚拟内存区域标志，并设置固定映射
    vmFlags = OsCvtProtFlagsToRegionFlags(loadInfo->stackProt, MAP_FIXED);
    vmFlags |= VM_MAP_REGION_FLAG_STACK;                                         // 添加栈区域标志
    // 分配用户栈内存
    ret = OsStackAlloc((VOID *)loadInfo->newSpace, loadInfo->stackBase, USER_STACK_SIZE,
                       USER_PARAM_BYTE_MAX, vmFlags);
    if (ret != LOS_OK) {                                                        // 栈内存分配失败
        PRINT_ERR("%s[%d], Failed to alloc memory for user stack!\n", __FUNCTION__, __LINE__);
        return -ENOMEM;                                                        // 返回内存不足错误
    }
    // 栈顶内存地址（预留一个指针大小的空间）
    loadInfo->topOfMem = loadInfo->stackTopMax - sizeof(UINTPTR);

    loadInfo->argc = OsGetParamNum(argv);                                       // 获取命令行参数数量
    loadInfo->envc = OsGetParamNum(envp);                                       // 获取环境变量数量
    // 将文件名复制到用户栈
    ret = OsCopyParams(loadInfo, 1, (CHAR *const *)&loadInfo->fileName);
    if (ret != LOS_OK) {                                                        // 文件名复制失败
        PRINT_ERR("%s[%d], Failed to copy filename to user stack!\n", __FUNCTION__, __LINE__);
        return ret;
    }
    loadInfo->execName = (CHAR *)loadInfo->topOfMem;                            // 保存可执行文件名地址

    ret = OsCopyParams(loadInfo, loadInfo->envc, envp);                         // 将环境变量复制到用户栈
    if (ret != LOS_OK) {                                                        // 环境变量复制失败
        return ret;
    }
    ret = OsCopyParams(loadInfo, loadInfo->argc, argv);                         // 将命令行参数复制到用户栈
    if (ret != LOS_OK) {                                                        // 命令行参数复制失败
        return ret;
    }
    loadInfo->argStart = loadInfo->topOfMem;                                    // 保存参数起始地址

    return LOS_OK;
}

/**
 * @brief 将参数和辅助向量信息放入用户栈
 * @param loadInfo ELF加载信息结构体指针
 * @param auxVecInfo 辅助向量信息数组
 * @param vecIndex 辅助向量数量
 * @return 成功返回LOS_OK，失败返回错误码
 */
STATIC INT32 OsPutParamToStack(ELFLoadInfo *loadInfo, const UINTPTR *auxVecInfo, INT32 vecIndex)
{
    // 将栈顶地址向下对齐到指针大小边界
    UINTPTR *topMem = (UINTPTR *)ROUNDDOWN(loadInfo->topOfMem, sizeof(UINTPTR));
    UINTPTR *argsPtr = NULL;                                                    // 参数指针
    // 计算总参数项数：(参数数量+1) + (环境变量数量+1) + 1个NULL终止符
    INT32 items = (loadInfo->argc + 1) + (loadInfo->envc + 1) + 1;
    size_t size;                                                                // 复制字节数

    // 计算参数栈顶地址，向下对齐到栈对齐边界
    loadInfo->topOfMem = ROUNDDOWN((UINTPTR)(topMem - vecIndex - items), STACK_ALIGN_SIZE);
    argsPtr = (UINTPTR *)loadInfo->topOfMem;                                    // 设置参数指针
    loadInfo->stackTop = (UINTPTR)argsPtr;                                      // 保存栈顶地址

    // 检查参数总大小是否超过最大限制
    if ((loadInfo->stackTopMax - loadInfo->stackTop) > USER_PARAM_BYTE_MAX) {
        return -E2BIG;                                                         // 参数过大，返回错误
    }

    // 将参数数量写入用户栈
    if (OsPutUserArg(loadInfo->argc, argsPtr)) {
        PRINT_ERR("%s[%d], Failed to put argc to user stack!\n", __FUNCTION__, __LINE__);
        return -EFAULT;                                                        // 写入失败，返回错误
    }

    argsPtr++;                                                                  // 移动到下一个参数位置

    // 将命令行参数和环境变量指针数组写入用户栈
    if ((OsPutUserArgv(&loadInfo->argStart, &argsPtr, loadInfo->argc) != LOS_OK) ||
        (OsPutUserArgv(&loadInfo->argStart, &argsPtr, loadInfo->envc) != LOS_OK)) {
        PRINT_ERR("%s[%d], Failed to put argv or envp to user stack!\n", __FUNCTION__, __LINE__);
        return -EFAULT;                                                        // 写入失败，返回错误
    }

    // 将辅助向量复制到用户栈
    size = LOS_ArchCopyToUser(argsPtr, auxVecInfo, vecIndex * sizeof(UINTPTR));
    if (size != 0) {                                                           // 复制字节数不为0表示失败
        PRINT_ERR("%s[%d], Failed to copy strings! Bytes not copied: %d\n", __FUNCTION__, __LINE__, size);
        return -EFAULT;                                                        // 复制失败，返回错误
    }

    return LOS_OK;
}

/**
 * @brief 生成随机数向量
 * @param loadInfo ELF加载信息结构体指针
 * @param rndVec 存储随机数的向量数组
 * @param vecSize 向量大小
 * @return 成功返回LOS_OK
 */
STATIC INT32 OsGetRndNum(const ELFLoadInfo *loadInfo, UINT32 *rndVec, UINT32 vecSize)
{
    UINT32 randomValue = 0;                                                     // 随机数值
    UINT32 i, ret;                                                              // 循环变量和返回值

    for (i = 0; i < vecSize; ++i) {                                             // 生成vecSize个随机数
        // 从随机设备读取随机数
        ret = read(loadInfo->randomDevFD, &randomValue, sizeof(UINT32));
        if (ret != sizeof(UINT32)) {                                            // 读取失败时
            rndVec[i] = (UINT32)random();                                       // 使用标准库随机函数
            continue;
        }
        rndVec[i] = randomValue;                                                // 保存读取的随机数
    }

    return LOS_OK;
}

/**
 * @brief 构造参数栈，包括参数、环境变量和辅助向量
 * @param loadInfo ELF加载信息结构体指针
 * @param interpMapBase 解释器映射基地址
 * @return 成功返回LOS_OK，失败返回错误码
 */
STATIC INT32 OsMakeArgsStack(ELFLoadInfo *loadInfo, UINTPTR interpMapBase)
{
    UINTPTR auxVector[AUX_VECTOR_SIZE] = { 0 };                                 // 辅助向量数组
    UINTPTR *auxVecInfo = (UINTPTR *)auxVector;                                 // 辅助向量指针
    INT32 vecIndex = 0;                                                        // 辅助向量索引
    UINT32 rndVec[RANDOM_VECTOR_SIZE];                                          // 随机数向量
    UINTPTR rndVecStart;                                                       // 随机数向量起始地址
    INT32 ret;                                                                  // 返回值
#ifdef LOSCFG_KERNEL_VDSO
    vaddr_t vdsoLoadAddr;                                                      // VDSO加载地址
#endif

    // 生成随机数向量
    ret = OsGetRndNum(loadInfo, rndVec, sizeof(rndVec));
    if (ret != LOS_OK) {
        return ret;
    }
    loadInfo->topOfMem -= sizeof(rndVec);                                       // 调整栈顶地址
    rndVecStart = loadInfo->topOfMem;                                           // 保存随机数向量起始地址

    // 将随机数向量复制到用户栈
    ret = LOS_ArchCopyToUser((VOID *)loadInfo->topOfMem, rndVec, sizeof(rndVec));
    if (ret != 0) {
        return -EFAULT;                                                        // 复制失败，返回错误
    }

    // 构造辅助向量（AUX VECTOR）信息
    AUX_VEC_ENTRY(auxVector, vecIndex, AUX_PHDR,   loadInfo->loadAddr + loadInfo->execInfo.elfEhdr.elfPhoff);  // 程序头表地址
    AUX_VEC_ENTRY(auxVector, vecIndex, AUX_PHENT,  sizeof(LD_ELF_PHDR));                                       // 程序头表项大小
    AUX_VEC_ENTRY(auxVector, vecIndex, AUX_PHNUM,  loadInfo->execInfo.elfEhdr.elfPhNum);                       // 程序头表项数量
    AUX_VEC_ENTRY(auxVector, vecIndex, AUX_PAGESZ, PAGE_SIZE);                                                // 页面大小
    AUX_VEC_ENTRY(auxVector, vecIndex, AUX_BASE,   interpMapBase);                                            // 解释器基地址
    AUX_VEC_ENTRY(auxVector, vecIndex, AUX_FLAGS,  0);                                                        // 标志位（未使用）
    AUX_VEC_ENTRY(auxVector, vecIndex, AUX_ENTRY,  loadInfo->execInfo.elfEhdr.elfEntry);                       // 程序入口地址
    AUX_VEC_ENTRY(auxVector, vecIndex, AUX_UID,    0);                                                        // 用户ID
    AUX_VEC_ENTRY(auxVector, vecIndex, AUX_EUID,   0);                                                        // 有效用户ID
    AUX_VEC_ENTRY(auxVector, vecIndex, AUX_GID,    0);                                                        // 组ID
    AUX_VEC_ENTRY(auxVector, vecIndex, AUX_EGID,   0);                                                        // 有效组ID
    AUX_VEC_ENTRY(auxVector, vecIndex, AUX_HWCAP,  0);                                                        // 硬件能力（未使用）
    AUX_VEC_ENTRY(auxVector, vecIndex, AUX_CLKTCK, 0);                                                        // 时钟滴答数（未使用）
    AUX_VEC_ENTRY(auxVector, vecIndex, AUX_SECURE, 0);                                                        // 安全模式标志（未使用）
    AUX_VEC_ENTRY(auxVector, vecIndex, AUX_RANDOM, rndVecStart);                                             // 随机数向量地址
    AUX_VEC_ENTRY(auxVector, vecIndex, AUX_EXECFN, (UINTPTR)loadInfo->execName);                              // 可执行文件名地址

#ifdef LOSCFG_KERNEL_VDSO
    // 加载VDSO（虚拟动态共享对象）
    vdsoLoadAddr = OsVdsoLoad(OsCurrProcessGet());
    if (vdsoLoadAddr != 0) {
        // 添加VDSO入口地址到辅助向量
        AUX_VEC_ENTRY(auxVector, vecIndex, AUX_SYSINFO_EHDR, vdsoLoadAddr);
    }
#endif
    AUX_VEC_ENTRY(auxVector, vecIndex, AUX_NULL,   0);                                                        // 辅助向量结束标志

    // 将参数和辅助向量放入用户栈
    ret = OsPutParamToStack(loadInfo, auxVecInfo, vecIndex);
    if (ret != LOS_OK) {
        PRINT_ERR("%s[%d], Failed to put param to user stack\n", __FUNCTION__, __LINE__);
        return ret;
    }

    return LOS_OK;
}

/**
 * @brief 加载ELF程序段到内存
 * @param loadInfo ELF加载信息结构体指针
 * @return 成功返回LOS_OK，失败返回错误码
 */
STATIC INT32 OsLoadELFSegment(ELFLoadInfo *loadInfo)
{
    LD_ELF_PHDR *elfPhdrTemp = loadInfo->execInfo.elfPhdr;                      // 程序头表指针
    UINTPTR loadBase = 0;                                                      // 加载基地址
    UINTPTR interpMapBase = 0;                                                 // 解释器映射基地址
    UINT32 mapSize = 0;                                                        // 映射大小
    INT32 ret;                                                                  // 返回值
    loadInfo->loadAddr = 0;                                                    // 加载地址

    // 如果是动态链接的ELF可执行文件（位置无关）
    if (loadInfo->execInfo.elfEhdr.elfType == LD_ET_DYN) {
        // 计算随机加载基地址
        loadBase = EXEC_MMAP_BASE + OsGetRndOffset(loadInfo->randomDevFD);
        // 计算所需内存分配大小
        mapSize = OsGetAllocSize(elfPhdrTemp, loadInfo->execInfo.elfEhdr.elfPhNum);
        if (mapSize == 0) {                                                    // 分配大小计算失败
            PRINT_ERR("%s[%d], Failed to get allocation size of file: %s!\n", __FUNCTION__, __LINE__,
                      loadInfo->fileName);
            return -EINVAL;                                                    // 返回无效参数错误
        }
    }

    // 将ELF文件映射到内存
    ret = OsMmapELFFile(loadInfo->execInfo.procfd, loadInfo->execInfo.elfPhdr, &loadInfo->execInfo.elfEhdr,
                        &loadInfo->loadAddr, mapSize, &loadBase);
    OsELFClose(loadInfo->execInfo.procfd);                                      // 关闭ELF文件描述符
    loadInfo->execInfo.procfd = INVALID_FD;                                     // 标记文件描述符为无效
    if (ret != LOS_OK) {                                                        // 内存映射失败
        PRINT_ERR("%s[%d]\n", __FUNCTION__, __LINE__);
        return ret;
    }

    // 如果存在解释器（动态链接器）
    if (loadInfo->interpInfo.procfd != INVALID_FD) {
        // 加载解释器二进制文件
        ret = OsLoadInterpBinary(loadInfo, &interpMapBase);
        OsELFClose(loadInfo->interpInfo.procfd);                               // 关闭解释器文件描述符
        loadInfo->interpInfo.procfd = INVALID_FD;                              // 标记文件描述符为无效
        if (ret != LOS_OK) {                                                   // 解释器加载失败
            return ret;
        }

        // 计算实际入口地址（解释器入口 + 解释器映射基地址）
        loadInfo->elfEntry = loadInfo->interpInfo.elfEhdr.elfEntry + interpMapBase;
        // 计算可执行文件入口地址（文件入口 + 加载基地址）
        loadInfo->execInfo.elfEhdr.elfEntry = loadInfo->execInfo.elfEhdr.elfEntry + loadBase;
    } else {
        loadInfo->elfEntry = loadInfo->execInfo.elfEhdr.elfEntry;               // 直接使用可执行文件入口地址
    }

    // 构造参数栈
    ret = OsMakeArgsStack(loadInfo, interpMapBase);
    if (ret != LOS_OK) {
        return ret;
    }
    // 检查栈顶地址是否为有效的用户空间地址
    if (!LOS_IsUserAddress((VADDR_T)loadInfo->stackTop)) {
        PRINT_ERR("%s[%d], StackTop is out of limit!\n", __FUNCTION__, __LINE__);
        return -EINVAL;                                                        // 栈地址无效，返回错误
    }

    return LOS_OK;
}

/**
 * @brief 刷新地址空间，替换进程的虚拟内存空间
 * @param loadInfo ELF加载信息结构体指针
 */
STATIC VOID OsFlushAspace(ELFLoadInfo *loadInfo)
{
    // 替换进程虚拟内存空间，并保存旧空间信息
    loadInfo->oldSpace = OsExecProcessVmSpaceReplace(loadInfo->newSpace, loadInfo->stackBase, loadInfo->randomDevFD);
}

/**
 * @brief 释放ELF加载信息相关资源
 * @param loadInfo ELF加载信息结构体指针
 */
STATIC VOID OsDeInitLoadInfo(ELFLoadInfo *loadInfo)
{
    (VOID)close(loadInfo->randomDevFD);                                         // 关闭随机设备文件描述符

    if (loadInfo->execInfo.elfPhdr != NULL) {                                   // 如果程序头表不为空
        (VOID)LOS_MemFree(m_aucSysMem0, loadInfo->execInfo.elfPhdr);            // 释放程序头表内存
    }

    if (loadInfo->interpInfo.elfPhdr != NULL) {                                 // 如果解释器程序头表不为空
        (VOID)LOS_MemFree(m_aucSysMem0, loadInfo->interpInfo.elfPhdr);          // 释放解释器程序头表内存
    }
}

/**
 * @brief 关闭文件描述符并清理文件相关资源
 * @param loadInfo ELF加载信息结构体指针
 */
STATIC VOID OsDeInitFiles(ELFLoadInfo *loadInfo)
{
    if (loadInfo->execInfo.procfd != INVALID_FD) {                              // 如果可执行文件描述符有效
        (VOID)OsELFClose(loadInfo->execInfo.procfd);                           // 关闭可执行文件
    }

    if (loadInfo->interpInfo.procfd != INVALID_FD) {                            // 如果解释器文件描述符有效
        (VOID)OsELFClose(loadInfo->interpInfo.procfd);                          // 关闭解释器文件
    }
#ifdef LOSCFG_FS_VFS
    delete_files_snapshot((struct files_struct *)loadInfo->oldFiles);           // 删除文件快照
#endif
}

/**
 * @brief 加载ELF文件的主函数
 * @param loadInfo ELF加载信息结构体指针
 * @return 成功返回LOS_OK，失败返回错误码
 */
INT32 OsLoadELFFile(ELFLoadInfo *loadInfo)
{
    INT32 ret;                                                                  // 返回值

    OsLoadInit(loadInfo);                                                       // 初始化加载信息

    // 读取ELF文件头
    ret = OsReadEhdr(loadInfo->fileName, &loadInfo->execInfo, TRUE);
    if (ret != LOS_OK) {
        goto OUT;                                                              // 读取失败，跳转到清理流程
    }

    // 读取ELF程序头表
    ret = OsReadPhdrs(&loadInfo->execInfo, TRUE);
    if (ret != LOS_OK) {
        goto OUT;
    }

    // 读取解释器信息
    ret = OsReadInterpInfo(loadInfo);
    if (ret != LOS_OK) {
        goto OUT;
    }

    // 设置参数和环境变量
    ret = OsSetArgParams(loadInfo, loadInfo->argv, loadInfo->envp);
    if (ret != LOS_OK) {
        goto OUT;
    }

    OsFlushAspace(loadInfo);                                                    // 刷新地址空间

    // 加载ELF程序段
    ret = OsLoadELFSegment(loadInfo);
    if (ret != LOS_OK) {
        OsExecProcessVmSpaceRestore(loadInfo->oldSpace);                       // 恢复旧地址空间
        goto OUT;
    }

    OsDeInitLoadInfo(loadInfo);                                                 // 释放加载信息资源

    return LOS_OK;

OUT:
    OsDeInitFiles(loadInfo);                                                    // 清理文件资源
    (VOID)LOS_VmSpaceFree(loadInfo->newSpace);                                 // 释放新地址空间
    (VOID)OsDeInitLoadInfo(loadInfo);                                           // 释放加载信息资源
    return ret;
}