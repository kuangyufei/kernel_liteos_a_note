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
#include "los_config.h"
#include "los_vm_map.h"
#include "los_vm_syscall.h"
#include "los_vm_phys.h"
#include "los_vm_dump.h"
#ifdef LOSCFG_KERNEL_VDSO
#include "los_vdso.h"
#endif
#ifdef LOSCFG_DRIVERS_TZDRIVER
#include "tzdriver.h"
#endif

static int OsELFOpen(const CHAR *fileName, INT32 oflags)
{
    int ret = -LOS_NOK;
    int procFd;

    procFd = AllocProcessFd();
    if (procFd < 0) {
        return -EMFILE;
    }

    ret = open(fileName, oflags);
    if (ret < 0) {
        FreeProcessFd(procFd);
        return -get_errno();
    }

    AssociateSystemFd(procFd, ret);
    return ret;
}

STATIC INT32 OsGetFileLength(UINT32 *fileLen, const CHAR *fileName)
{
    struct stat buf;
    INT32 ret;

    ret = stat(fileName, &buf);
    if (ret < 0) {
#ifndef LOSCFG_SHELL
        if (strcmp(fileName, "/bin/shell") != 0) {
#endif
            PRINT_ERR("%s[%d], Failed to stat file: %s, errno: %d\n", __FUNCTION__, __LINE__, fileName, errno);
#ifndef LOSCFG_SHELL
        }
#endif
        return LOS_NOK;
    }

    if (S_ISREG(buf.st_mode) == 0) {
        PRINT_ERR("%s[%d], The file: %s is invalid!\n", __FUNCTION__, __LINE__, fileName);
        return LOS_NOK;
    }
    if (buf.st_size > FILE_LENGTH_MAX) {
        PRINT_ERR("%s[%d], The file: %s length is out of limit!\n", __FUNCTION__, __LINE__, fileName);
        return LOS_NOK;
    }

    *fileLen = (UINT32)buf.st_size;
    return LOS_OK;
}

STATIC INT32 OsReadELFInfo(INT32 fd, UINT8 *buffer, size_t readSize, off_t offset)
{
    ssize_t byteNum;
    off_t returnPos;

    if (readSize > 0) {
        returnPos = lseek(fd, offset, SEEK_SET);
        if (returnPos != offset) {
            PRINT_ERR("%s[%d], Failed to seek the position!, offset: %#x\n", __FUNCTION__, __LINE__, offset);
            return LOS_NOK;
        }

        byteNum = read(fd, buffer, readSize);
        if (byteNum <= 0) {
            PRINT_ERR("%s[%d], Failed to read from offset: %#x!\n", __FUNCTION__, __LINE__, offset);
            return LOS_NOK;
        }
    }
    return LOS_OK;
}
//确认ELF头信息是否异常
STATIC INT32 OsVerifyELFEhdr(const LD_ELF_EHDR *ehdr, UINT32 fileLen)
{
    if (memcmp(ehdr->elfIdent, LD_ELFMAG, LD_SELFMAG) != 0) {
        PRINT_ERR("%s[%d], The file is not elf!\n", __FUNCTION__, __LINE__);
        return LOS_NOK;
    }
	//必须为 LD_ET_EXEC 和 LD_ET_DYN 才可以被执行,
    if ((ehdr->elfType != LD_ET_EXEC) && (ehdr->elfType != LD_ET_DYN)) {
        PRINT_ERR("%s[%d], The type of file is not ET_EXEC or ET_DYN!\n", __FUNCTION__, __LINE__);
        return LOS_NOK;
    }
    if (ehdr->elfMachine != LD_EM_ARM) {//CPU架构,目前只支持arm系列
        PRINT_ERR("%s[%d], The type of machine is not EM_ARM!\n", __FUNCTION__, __LINE__);
        return LOS_NOK;
    }
    if (ehdr->elfPhNum > ELF_PHDR_NUM_MAX) {//程序头数量不能大于128个
        PRINT_ERR("%s[%d], The num of program header is out of limit\n", __FUNCTION__, __LINE__);
        return LOS_NOK;
    }
    if (ehdr->elfPhoff > fileLen) {
        PRINT_ERR("%s[%d], The offset of program header is invalid, elf file is bad\n", __FUNCTION__, __LINE__);
        return LOS_NOK;
    }
    if (OsIsBadUserAddress((VADDR_T)ehdr->elfEntry)) {
        PRINT_ERR("%s[%d], The entry of program is invalid\n", __FUNCTION__, __LINE__);
        return LOS_NOK;
    }

    return LOS_OK;
}

STATIC INT32 OsVerifyELFPhdr(const LD_ELF_PHDR *phdr)
{
    if ((phdr->fileSize > FILE_LENGTH_MAX) || (phdr->offset > FILE_LENGTH_MAX)) {
        PRINT_ERR("%s[%d], The size of phdr is out of limit\n", __FUNCTION__, __LINE__);
        return LOS_NOK;
    }
    if (phdr->memSize > MEM_SIZE_MAX) {
        PRINT_ERR("%s[%d], The mem size of phdr is out of limit\n", __FUNCTION__, __LINE__);
        return LOS_NOK;
    }
    if (OsIsBadUserAddress((VADDR_T)phdr->vAddr)) {
        PRINT_ERR("%s[%d], The vaddr of phdr is invalid\n", __FUNCTION__, __LINE__);
        return LOS_NOK;
    }

    return LOS_OK;
}

STATIC VOID OsLoadInit(ELFLoadInfo *loadInfo)
{
#ifdef LOSCFG_FS_VFS
    const struct files_struct *oldFiles = OsCurrProcessGet()->files;
    loadInfo->oldFiles = (UINTPTR)create_files_snapshot(oldFiles);//为当前进程做文件镜像
#else
    loadInfo->oldFiles = NULL;
#endif
    loadInfo->execInfo.fd = INVALID_FD;
    loadInfo->interpInfo.fd = INVALID_FD;
}
//读 ELF头信息
STATIC INT32 OsReadEhdr(const CHAR *fileName, ELFInfo *elfInfo, BOOL isExecFile)
{
    INT32 ret;

    ret = OsGetFileLength(&elfInfo->fileLen, fileName);
    if (ret != LOS_OK) {
        return -ENOENT;
    }

    ret = OsELFOpen(fileName, O_RDONLY | O_EXECVE);
    if (ret < 0) {
        PRINT_ERR("%s[%d], Failed to open ELF file: %s!\n", __FUNCTION__, __LINE__, fileName);
        return ret;
    }
    elfInfo->fd = ret;//文件描述符

#ifdef LOSCFG_DRIVERS_TZDRIVER
    if (isExecFile) {
        ret = fs_getfilep(elfInfo->fd, &OsCurrProcessGet()->execFile);
        if (ret) {
            PRINT_ERR("%s[%d], Failed to get struct file %s!\n", __FUNCTION__, __LINE__, fileName);
        }
    }
#endif
    ret = OsReadELFInfo(elfInfo->fd, (UINT8 *)&elfInfo->elfEhdr, sizeof(LD_ELF_EHDR), 0);//读取ELF头部信息
    if (ret != LOS_OK) {
        PRINT_ERR("%s[%d]\n", __FUNCTION__, __LINE__);
        return -EIO;
    }

    ret = OsVerifyELFEhdr(&elfInfo->elfEhdr, elfInfo->fileLen);//确认是否为有效的ELF文件
    if (ret != LOS_OK) {
        PRINT_ERR("%s[%d]\n", __FUNCTION__, __LINE__);
        return isExecFile ? -ENOEXEC : -ELIBBAD;
    }

    return LOS_OK;
}
//读程序头信息
STATIC INT32 OsReadPhdrs(ELFInfo *elfInfo, BOOL isExecFile)
{
    LD_ELF_EHDR *elfEhdr = &elfInfo->elfEhdr;
    UINT32 size;
    INT32 ret;

    if (elfEhdr->elfPhNum < 1) {
        goto OUT;
    }

    if (elfEhdr->elfPhEntSize != sizeof(LD_ELF_PHDR)) {
        goto OUT;
    }

    size = sizeof(LD_ELF_PHDR) * elfEhdr->elfPhNum;//计算所有程序头大小
    if ((elfEhdr->elfPhoff + size) > elfInfo->fileLen) {
        goto OUT;
    }

    elfInfo->elfPhdr = LOS_MemAlloc(m_aucSysMem0, size);//在内核空间分配内存保存ELF程序头
    if (elfInfo->elfPhdr == NULL) {
        PRINT_ERR("%s[%d], Failed to allocate for elfPhdr!\n", __FUNCTION__, __LINE__);
        return -ENOMEM;
    }
	//读取所有程序头信息
    ret = OsReadELFInfo(elfInfo->fd, (UINT8 *)elfInfo->elfPhdr, size, elfEhdr->elfPhoff);
    if (ret != LOS_OK) {
        (VOID)LOS_MemFree(m_aucSysMem0, elfInfo->elfPhdr);
        elfInfo->elfPhdr = NULL;
        PRINT_ERR("%s[%d]\n", __FUNCTION__, __LINE__);
        return -EIO;
    }

    return LOS_OK;
OUT:
    PRINT_ERR("%s[%d], elf file is bad!\n", __FUNCTION__, __LINE__);
    return isExecFile ? -ENOEXEC : -ELIBBAD;
}
//读取 INTERP段信息,描述了一个特殊内存段,该段内存记录了动态加载解析器的访问路径字符串.
STATIC INT32 OsReadInterpInfo(ELFLoadInfo *loadInfo)
{
    LD_ELF_PHDR *elfPhdr = loadInfo->execInfo.elfPhdr;
    CHAR *elfInterpName = NULL;
    INT32 ret, i;

    for (i = 0; i < loadInfo->execInfo.elfEhdr.elfPhNum; ++i, ++elfPhdr) {
        if (elfPhdr->type != LD_PT_INTERP) {//该可执行文件所需的动态链接器的路径
            continue;//[Requesting program interpreter: /lib/ld-musl-arm.so.1]
        }//lib/ld-musl-arm.so.1 -> /lib/libc.so
		//./out/hispark_aries/ipcamera_hispark_aries/obj/kernel/liteos_a/rootfs/lib/libc.so
        if (OsVerifyELFPhdr(elfPhdr) != LOS_OK) {
            return -ENOEXEC;
        }

        if ((elfPhdr->fileSize > FILE_PATH_MAX) || (elfPhdr->fileSize < FILE_PATH_MIN) ||
            (elfPhdr->offset + elfPhdr->fileSize > loadInfo->execInfo.fileLen)) {
            PRINT_ERR("%s[%d], The size of file is out of limit!\n", __FUNCTION__, __LINE__);
            return -ENOEXEC;
        }

        elfInterpName = LOS_MemAlloc(m_aucSysMem0, elfPhdr->fileSize);
        if (elfInterpName == NULL) {
            PRINT_ERR("%s[%d], Failed to allocate for elfInterpName!\n", __FUNCTION__, __LINE__);
            return -ENOMEM;
        }

        ret = OsReadELFInfo(loadInfo->execInfo.fd, (UINT8 *)elfInterpName, elfPhdr->fileSize, elfPhdr->offset);
        if (ret != LOS_OK) {
            PRINT_ERR("%s[%d]\n", __FUNCTION__, __LINE__);
            ret = -EIO;
            goto OUT;
        }

        if (elfInterpName[elfPhdr->fileSize - 1] != '\0') {
            PRINT_ERR("%s[%d], The name of interpreter is invalid!\n", __FUNCTION__, __LINE__);
            ret = -ENOEXEC;
            goto OUT;
        }
		//读"/lib/libc.so" ELF头信息
        ret = OsReadEhdr(INTERP_FULL_PATH, &loadInfo->interpInfo, FALSE);//打开.so文件,绑定 fd
        if (ret != LOS_OK) {
            PRINT_ERR("%s[%d]\n", __FUNCTION__, __LINE__);
            goto OUT;
        }
		//读"/lib/libc.so" ELF程序头信息
        ret = OsReadPhdrs(&loadInfo->interpInfo, FALSE);
        if (ret != LOS_OK) {
            goto OUT;
        }

        (VOID)LOS_MemFree(m_aucSysMem0, elfInterpName);
        break;
    }

    return LOS_OK;

OUT:
    (VOID)LOS_MemFree(m_aucSysMem0, elfInterpName);
    return ret;
}

STATIC UINT32 OsGetProt(UINT32 pFlags)
{
    UINT32 prot;

    prot = (((pFlags & PF_R) ? PROT_READ : 0) |
            ((pFlags & PF_W) ? PROT_WRITE : 0) |
            ((pFlags & PF_X) ? PROT_EXEC : 0));
    return prot;
}

STATIC UINT32 OsGetAllocSize(const LD_ELF_PHDR *elfPhdr, INT32 phdrNum)
{
    const LD_ELF_PHDR *elfPhdrTemp = elfPhdr;
    UINTPTR addrMin = SIZE_MAX;
    UINTPTR addrMax = 0;
    UINT32 offStart = 0;
    UINT64 size;
    INT32 i;

    for (i = 0; i < phdrNum; ++i, ++elfPhdrTemp) {
        if (elfPhdrTemp->type != LD_PT_LOAD) {
            continue;
        }

        if (OsVerifyELFPhdr(elfPhdrTemp) != LOS_OK) {
            return 0;
        }

        if (elfPhdrTemp->vAddr < addrMin) {
            addrMin = elfPhdrTemp->vAddr;
            offStart = elfPhdrTemp->offset;
        }
        if ((elfPhdrTemp->vAddr + elfPhdrTemp->memSize) > addrMax) {
            addrMax = elfPhdrTemp->vAddr + elfPhdrTemp->memSize;
        }
    }

    if (OsIsBadUserAddress((VADDR_T)addrMax) || OsIsBadUserAddress((VADDR_T)addrMin) || (addrMax < addrMin)) {
        return 0;
    }
    size = ROUNDUP(addrMax, PAGE_SIZE) - ROUNDDOWN(addrMin, PAGE_SIZE) + ROUNDDOWN(offStart, PAGE_SIZE);

    return (size > UINT_MAX) ? 0 : (UINT32)size;
}

STATIC UINTPTR OsDoMmapFile(INT32 fd, UINTPTR addr, const LD_ELF_PHDR *elfPhdr, UINT32 prot, UINT32 flags,
                            UINT32 mapSize)
{
    UINTPTR mapAddr;
    UINT32 size;
    UINT32 offset = elfPhdr->offset - ROUNDOFFSET(elfPhdr->vAddr, PAGE_SIZE);//相对文件的偏移量
    addr = ROUNDDOWN(addr, PAGE_SIZE);

    if (mapSize != 0) {
        mapAddr = (UINTPTR)LOS_MMap(addr, mapSize, prot, flags, fd, offset >> PAGE_SHIFT);
    } else {
        size = elfPhdr->memSize + ROUNDOFFSET(elfPhdr->vAddr, PAGE_SIZE);
        if (size == 0) {
            return addr;
        }
        mapAddr = (UINTPTR)LOS_MMap(addr, size, prot, flags, fd, offset >> PAGE_SHIFT);
    }
    if (!LOS_IsUserAddress((VADDR_T)mapAddr)) {
        PRINT_ERR("%s %d, Failed to map a valid addr\n", __FUNCTION__, __LINE__);
        return 0;
    }
    return mapAddr;
}
//获取内核虚拟地址
INT32 OsGetKernelVaddr(const LosVmSpace *space, VADDR_T vaddr, VADDR_T *kvaddr)
{
    INT32 ret;
    PADDR_T paddr = 0;

    if ((space == NULL) || (vaddr == 0) || (kvaddr == NULL)) {
        PRINT_ERR("%s[%d], space: %#x, vaddr: %#x\n", __FUNCTION__, __LINE__, space, vaddr);
        return LOS_NOK;
    }

    if (LOS_IsKernelAddress(vaddr)) {//如果vaddr是内核空间地址
        *kvaddr = vaddr;//直接返回
        return LOS_OK;
    }

    ret = LOS_ArchMmuQuery(&space->archMmu, vaddr, &paddr, NULL);//通过虚拟地址查询物理地址
    if (ret != LOS_OK) {
        PRINT_ERR("%s[%d], Failed to query the vaddr: %#x, status: %d\n", __FUNCTION__, __LINE__, vaddr, ret);
        return LOS_NOK;
    }
    *kvaddr = (VADDR_T)(UINTPTR)LOS_PaddrToKVaddr(paddr);//获取通过物理地址获取内核虚拟地址
    if (*kvaddr == 0) {
        PRINT_ERR("%s[%d], kvaddr is null\n", __FUNCTION__, __LINE__);
        return LOS_NOK;
    }

    return LOS_OK;
}
//设置.bss 区
STATIC INT32 OsSetBss(const LD_ELF_PHDR *elfPhdr, INT32 fd, UINTPTR bssStart, UINT32 bssEnd, UINT32 elfProt)
{
    UINTPTR bssPageStart, bssStartPageAlign, bssEndPageAlign;
    UINTPTR mapBase;
    UINT32 offset, size;
    UINT32 bssMapSize;
    INT32 stackFlags;
    INT32 ret;
    vaddr_t kvaddr = 0;

    bssPageStart = ROUNDDOWN(bssStart, PAGE_SIZE);//bss区开始页位置
    bssStartPageAlign = ROUNDUP(bssStart, PAGE_SIZE);
    bssEndPageAlign = ROUNDUP(bssEnd, PAGE_SIZE);
    ret = LOS_UnMMap(bssPageStart, (bssEndPageAlign - bssPageStart));//先解除映射关系
    if ((ret != LOS_OK) && (bssPageStart != 0)) {
        PRINT_ERR("%s[%d], Failed to unmap a region, vaddr: %#x!\n", __FUNCTION__, __LINE__, bssPageStart);
    }
	//再分配空间,bss虽在文件中不占用空间,但要占用实际的内存空间
    ret = LOS_UserSpaceVmAlloc(OsCurrProcessGet()->vmSpace, PAGE_SIZE, (VOID **)&bssPageStart,
                               0, OsCvtProtFlagsToRegionFlags(elfProt, MAP_FIXED));
    if (ret != LOS_OK) {
        PRINT_ERR("%s[%d], Failed to do vmm alloc!\n", __FUNCTION__, __LINE__);
        return -ENOMEM;
    }
    ret = OsGetKernelVaddr(OsCurrProcessGet()->vmSpace, bssPageStart, &kvaddr);
    if (ret != LOS_OK) {
        PRINT_ERR("%s[%d]\n", __FUNCTION__, __LINE__);
        return -EFAULT;
    }
    (VOID)memset_s((VOID *)(UINTPTR)kvaddr, PAGE_SIZE, 0, PAGE_SIZE);

    offset = ROUNDDOWN(elfPhdr->offset + elfPhdr->fileSize, PAGE_SIZE);
    size = ROUNDOFFSET(elfPhdr->offset + elfPhdr->fileSize, PAGE_SIZE);
    ret = OsReadELFInfo(fd, (UINT8 *)(UINTPTR)kvaddr, size, offset);
    if (ret != LOS_OK) {
        PRINT_ERR("%s[%d]\n", __FUNCTION__, __LINE__);
        return -EIO;
    }

    bssMapSize = bssEndPageAlign - bssStartPageAlign;
    if (bssMapSize > 0) {
        stackFlags = MAP_PRIVATE | MAP_FIXED | MAP_ANONYMOUS;//私有/覆盖/匿名映射
        mapBase = (UINTPTR)LOS_MMap(bssStartPageAlign, bssMapSize, elfProt, stackFlags, -1, 0);//再建立映射关系
        if (!LOS_IsUserAddress((VADDR_T)mapBase)) {
            PRINT_ERR("%s[%d], Failed to map bss\n", __FUNCTION__, __LINE__);
            return -ENOMEM;
        }
    }

    return LOS_OK;
}
//映射ELF文件,处理 LD_PT_LOAD 类型,(代码区,数据区) 
STATIC INT32 OsMmapELFFile(INT32 fd, const LD_ELF_PHDR *elfPhdr, const LD_ELF_EHDR *elfEhdr, UINTPTR *elfLoadAddr,
                           UINT32 mapSize, UINTPTR *loadBase)
{
    const LD_ELF_PHDR *elfPhdrTemp = elfPhdr;
    UINTPTR vAddr, mapAddr, bssStart;
    UINT32 bssEnd, elfProt, elfFlags;
    INT32 ret, i;

    for (i = 0; i < elfEhdr->elfPhNum; ++i, ++elfPhdrTemp) {//一段一段处理
        if (elfPhdrTemp->type != LD_PT_LOAD) {//只处理 LD_PT_LOAD 类型段
            continue;
        }
        if (elfEhdr->elfType == LD_ET_EXEC) {//验证可执行文件
            if (OsVerifyELFPhdr(elfPhdrTemp) != LOS_OK) {
                return -ENOEXEC;
            }
        }

        elfProt = OsGetProt(elfPhdrTemp->flags);//获取段权限
        if ((elfProt & PROT_READ) == 0) {// LD_PT_LOAD 必有 R 属性
            return -ENOEXEC;
        }
        elfFlags = MAP_PRIVATE | MAP_FIXED;//进程私有并覆盖方式,非匿名即文件映射
        vAddr = elfPhdrTemp->vAddr;//虚拟地址
        if ((vAddr == 0) && (*loadBase == 0)) {//这种情况不用覆盖.
            elfFlags &= ~MAP_FIXED;
        }
		//处理文件段和虚拟空间的映射关系
        mapAddr = OsDoMmapFile(fd, (vAddr + *loadBase), elfPhdrTemp, elfProt, elfFlags, mapSize);
        if (!LOS_IsUserAddress((VADDR_T)mapAddr)) {//映射地址必在用户空间.
            return -ENOMEM;
        }
#ifdef LOSCFG_DRIVERS_TZDRIVER
        if ((elfPhdrTemp->flags & PF_R) && (elfPhdrTemp->flags & PF_X) && !(elfPhdrTemp->flags & PF_W)) {
            SetVmmRegionCodeStart(vAddr + *loadBase, elfPhdrTemp->memSize);
        }
#endif
        mapSize = 0;

        if (*elfLoadAddr == 0) {
            *elfLoadAddr = mapAddr + ROUNDOFFSET(vAddr, PAGE_SIZE);
        }

        if ((*loadBase == 0) && (elfEhdr->elfType == LD_ET_DYN)) {
            *loadBase = mapAddr;//改变装载基地址
        }
		//.bss 的区分标识为 实际使用内存大小要大于文件大小,而且必是可写,.bss采用匿名映射
        if ((elfPhdrTemp->memSize > elfPhdrTemp->fileSize) && (elfPhdrTemp->flags & PF_W)) {
            bssStart = mapAddr + ROUNDOFFSET(vAddr, PAGE_SIZE) + elfPhdrTemp->fileSize;//bss区开始位置
            bssEnd = mapAddr + ROUNDOFFSET(vAddr, PAGE_SIZE) + elfPhdrTemp->memSize;//bss区结束位置
            ret = OsSetBss(elfPhdrTemp, fd, bssStart, bssEnd, elfProt);//设置bss区
            if (ret != LOS_OK) {
                return ret;
            }
        }
    }

    return LOS_OK;
}
//加载解析器二进制
STATIC INT32 OsLoadInterpBinary(const ELFLoadInfo *loadInfo, UINTPTR *interpMapBase)
{
    UINTPTR loadBase = 0;
    UINT32 mapSize;
    INT32 ret;

    mapSize = OsGetAllocSize(loadInfo->interpInfo.elfPhdr, loadInfo->interpInfo.elfEhdr.elfPhNum);//获取要分配映射内存大小
    if (mapSize == 0) {
        PRINT_ERR("%s[%d], Failed to get interp allocation size!\n", __FUNCTION__, __LINE__);
        return -EINVAL;
    }
	//对动态链接库做映射
    ret = OsMmapELFFile(loadInfo->interpInfo.fd, loadInfo->interpInfo.elfPhdr, &loadInfo->interpInfo.elfEhdr,
                        interpMapBase, mapSize, &loadBase);
    if (ret != LOS_OK) {
        PRINT_ERR("%s[%d]\n", __FUNCTION__, __LINE__);
        return ret;
    }

    return LOS_OK;
}

STATIC CHAR *OsGetParamPtr(CHAR * const *ptr, INT32 index)
{
    UINTPTR userStrPtr = 0;
    INT32 ret;

    if (ptr == NULL) {
        return NULL;
    }

    if (LOS_IsKernelAddress((UINTPTR)ptr)) {
        return ptr[index];
    }
    ret = LOS_GetUser(&userStrPtr, (UINTPTR *)(ptr + index));
    if (ret != LOS_OK) {
        PRINT_ERR("%s[%d], %#x\n", __FUNCTION__, __LINE__, ptr);
        return NULL;
    }

    return (CHAR *)userStrPtr;
}

STATIC INT32 OsPutUserArg(INT32 val, const UINTPTR *sp)
{
    INT32 ret;

    if (sp == NULL) {
        return LOS_NOK;
    }

    ret = LOS_PutUser((INT32 *)&val, (INT32 *)sp);
    if (ret != LOS_OK) {
        PRINT_ERR("%s[%d]\n", __FUNCTION__, __LINE__);
        return -EFAULT;
    }

    return LOS_OK;
}

STATIC INT32 OsPutUserArgv(UINTPTR *strPtr, UINTPTR **sp, INT32 count)
{
    INT32 len;
    INT32 i;
    CHAR *ptr = NULL;

    if ((strPtr == NULL) || (sp == NULL)) {
        return LOS_NOK;
    }

    for (i = 0; i < count; ++i) {
        /* put the addr of arg strings to user stack */
        if (OsPutUserArg(*strPtr, *sp) != LOS_OK) {
            return LOS_NOK;
        }
        ptr = OsGetParamPtr((CHAR **)strPtr, 0);
        if (ptr == NULL) {
            return LOS_NOK;
        }
        len = LOS_StrnlenUser(ptr, PATH_MAX);
        if (len == 0) {
            return LOS_NOK;
        }
        *strPtr += len;
        ++(*sp);
    }
    /* put zero to end of argv */
    if (OsPutUserArg(0, *sp) != LOS_OK) {
        return LOS_NOK;
    }
    ++(*sp);

    return LOS_OK;
}
//拷贝参数,将内核空间参数拷贝到用户栈空间.
STATIC INT32 OsCopyParams(ELFLoadInfo *loadInfo, INT32 argc, CHAR *const *argv)
{
    CHAR *strPtr = NULL;
    UINT32 offset, strLen;
    errno_t err;
    INT32 ret, i;
    vaddr_t kvaddr = 0;

    if ((argc > 0) && (argv == NULL)) {
        return -EINVAL;
    }

    ret = OsGetKernelVaddr(loadInfo->newSpace, loadInfo->stackParamBase, &kvaddr);//获取内核虚拟地址
    if (ret != LOS_OK) {
        PRINT_ERR("%s[%d]\n", __FUNCTION__, __LINE__);
        return -EFAULT;
    }

    for (i = argc - 1; i >= 0; --i) {
        strPtr = OsGetParamPtr(argv, i);
        if (LOS_IsKernelAddress((VADDR_T)(UINTPTR)strPtr)) {
            strLen = strlen(strPtr) + 1;
        } else {
            strLen = LOS_StrnlenUser(strPtr, PATH_MAX);
        }
        if (strLen < 1) {
            continue;
        }

        offset = loadInfo->topOfMem - loadInfo->stackParamBase;
        if (offset < strLen) {
            PRINT_ERR("%s[%d], The size of param is out of limit: %#x bytes!\n", __FUNCTION__, __LINE__,
                      USER_PARAM_BYTE_MAX);
            return -E2BIG;
        }
        loadInfo->topOfMem -= strLen;
        offset -= strLen;

        /* copy strings to user stack *///拷贝参数到用户栈空间
        if (LOS_IsKernelAddress((VADDR_T)(UINTPTR)strPtr)) {
            err = memcpy_s((VOID *)(UINTPTR)(kvaddr + offset), strLen, strPtr, strLen);
        } else {
            err = LOS_ArchCopyFromUser((VOID *)(UINTPTR)(kvaddr + offset), strPtr, strLen);
        }

        if (err != EOK) {
            PRINT_ERR("%s[%d], copy strings failed! err: %d\n", __FUNCTION__, __LINE__, err);
            return -EFAULT;
        }
    }

    return LOS_OK;
}
//获取参数
STATIC INT32 OsGetParamNum(CHAR *const *argv)
{
    CHAR *argPtr = NULL;
    INT32 count = 0;
    INT32 ret;

    if (argv == NULL) {
        return count;
    }

    argPtr = OsGetParamPtr(argv, count);
    while (argPtr != NULL) {
        ret = LOS_StrnlenUser(argPtr, PATH_MAX);
        if ((ret == 0) || (ret > PATH_MAX)) {
            PRINT_ERR("%s[%d], the len of string of argv is invalid, index: %d, len: %d\n", __FUNCTION__,
                      __LINE__, count, ret);
            break;
        }
        ++count;
        if (count >= STRINGS_COUNT_MAX) {
            break;
        }
        argPtr = OsGetParamPtr(argv, count);
    }

    return count;
}
//对齐
STATIC UINT32 OsGetRndOffset(const ELFLoadInfo *loadInfo)
{
    UINT32 randomValue = 0;

#ifdef LOSCFG_ASLR
    if (read(loadInfo->randomDevFD, &randomValue, sizeof(UINT32)) == sizeof(UINT32)) {
        randomValue &= RANDOM_MASK;
    } else {
        randomValue = 0;
    }
#else
    (VOID)loadInfo;
#endif

    return ROUNDDOWN(randomValue, PAGE_SIZE);//按4K页大小向下圆整
}
//获取LD_PT_GNU_STACK栈的权限
STATIC VOID OsGetStackProt(ELFLoadInfo *loadInfo)
{
    LD_ELF_PHDR *elfPhdrTemp = loadInfo->execInfo.elfPhdr;
    INT32 i;

    for (i = 0; i < loadInfo->execInfo.elfEhdr.elfPhNum; ++i, ++elfPhdrTemp) {
        if (elfPhdrTemp->type == LD_PT_GNU_STACK) {
            loadInfo->stackProt = OsGetProt(elfPhdrTemp->flags);
        }
    }
}
//设置命令行参数
STATIC INT32 OsSetArgParams(ELFLoadInfo *loadInfo, CHAR *const *argv, CHAR *const *envp)
{
    UINT32 vmFlags;
    INT32 ret;
    status_t status;

#ifdef LOSCFG_ASLR
    loadInfo->randomDevFD = open("/dev/urandom", O_RDONLY);
    if (loadInfo->randomDevFD < 0) {
        PRINT_ERR("%s: open /dev/urandom failed\n", __FUNCTION__);
    }
#endif

    (VOID)OsGetStackProt(loadInfo);//获取栈权限
    if (((UINT32)loadInfo->stackProt & (PROT_READ | PROT_WRITE)) != (PROT_READ | PROT_WRITE)) {//要求对栈有读写权限
        return -ENOEXEC;
    }
    loadInfo->stackTopMax = USER_STACK_TOP_MAX - OsGetRndOffset(loadInfo);//设置栈底位置,递减满栈方式
    loadInfo->stackBase = loadInfo->stackTopMax - USER_STACK_SIZE;//设置栈顶位置
    loadInfo->stackSize = USER_STACK_SIZE;
    loadInfo->stackParamBase = loadInfo->stackTopMax - USER_PARAM_BYTE_MAX;
    vmFlags = OsCvtProtFlagsToRegionFlags(loadInfo->stackProt, MAP_FIXED);//权限转化
    vmFlags |= VM_MAP_REGION_FLAG_STACK;//栈区标识
    status = LOS_UserSpaceVmAlloc((VOID *)loadInfo->newSpace, USER_PARAM_BYTE_MAX,
                                  (VOID **)&loadInfo->stackParamBase, 0, vmFlags);//从用户空间中 申请USER_PARAM_BYTE_MAX的空间
    if (status != LOS_OK) {
        PRINT_ERR("%s[%d], Failed to create user stack! status: %d\n", __FUNCTION__, __LINE__, status);
        return -ENOMEM;
    }
    loadInfo->topOfMem = loadInfo->stackTopMax - sizeof(UINTPTR);//虚拟空间顶部位置

    loadInfo->argc = OsGetParamNum(argv);//获取参数个数
    loadInfo->envc = OsGetParamNum(envp);//获取环境变量个数
    ret = OsCopyParams(loadInfo, 1, (CHAR *const *)&loadInfo->fileName);//保存文件名称到用户空间
    if (ret != LOS_OK) {
        PRINT_ERR("%s[%d], Failed to copy filename to user stack!\n", __FUNCTION__, __LINE__);
        return ret;
    }
    loadInfo->execName = (CHAR *)loadInfo->topOfMem;

    ret = OsCopyParams(loadInfo, loadInfo->envc, envp);//保存环境变量
    if (ret != LOS_OK) {
        return ret;
    }
    ret = OsCopyParams(loadInfo, loadInfo->argc, argv);//保存命令行参数
    if (ret != LOS_OK) {
        return ret;
    }

    return LOS_OK;
}
//将参数放到栈底保存
STATIC INT32 OsPutParamToStack(ELFLoadInfo *loadInfo, const UINTPTR *auxVecInfo, INT32 vecIndex)
{
    UINTPTR argStart = loadInfo->topOfMem;
    UINTPTR *topMem = (UINTPTR *)ROUNDDOWN(loadInfo->topOfMem, sizeof(UINTPTR));
    UINTPTR *argsPtr = NULL;
    UINTPTR stackBase;
    INT32 items = (loadInfo->argc + 1) + (loadInfo->envc + 1) + 1;
    size_t size;
    INT32 stackFlags;

    loadInfo->topOfMem = ROUNDDOWN((UINTPTR)(topMem - vecIndex - items), STACK_ALIGN_SIZE);
    argsPtr = (UINTPTR *)loadInfo->topOfMem;
    loadInfo->stackTop = (UINTPTR)argsPtr;

    if ((loadInfo->stackTopMax - loadInfo->stackTop) > USER_PARAM_BYTE_MAX) {
        return -E2BIG;
    }

    if (OsPutUserArg(loadInfo->argc, argsPtr)) {
        PRINT_ERR("%s[%d], Failed to put argc to user stack!\n", __FUNCTION__, __LINE__);
        return -EFAULT;
    }

    argsPtr++;

    if ((OsPutUserArgv(&argStart, &argsPtr, loadInfo->argc) != LOS_OK) ||
        (OsPutUserArgv(&argStart, &argsPtr, loadInfo->envc) != LOS_OK)) {
        PRINT_ERR("%s[%d], Failed to put argv or envp to user stack!\n", __FUNCTION__, __LINE__);
        return -EFAULT;
    }

    size = LOS_ArchCopyToUser(argsPtr, auxVecInfo, vecIndex * sizeof(UINTPTR));
    if (size != 0) {
        PRINT_ERR("%s[%d], Failed to copy strings! Bytes not copied: %d\n", __FUNCTION__, __LINE__, size);
        return -EFAULT;
    }

    if ((loadInfo->stackSize - USER_PARAM_BYTE_MAX) > 0) {
        /* mmap an external region for user stack */
        stackFlags = MAP_PRIVATE | MAP_FIXED | MAP_ANONYMOUS;
        stackBase = (UINTPTR)LOS_MMap(loadInfo->stackBase, (loadInfo->stackSize - USER_PARAM_BYTE_MAX),
                                      loadInfo->stackProt, stackFlags, -1, 0);
        if (!LOS_IsUserAddress((VADDR_T)stackBase)) {
            PRINT_ERR("%s[%d], Failed to map user stack\n", __FUNCTION__, __LINE__);
            return -ENOMEM;
        }
    }

    return LOS_OK;
}
/*
ELF辅助向量:从内核空间到用户空间的神秘信息载体。
ELF辅助向量是一种将某些内核级信息传递给用户进程的机制。此类信息的一个示例是指向系统呼叫内存中的入口点(AT_SYSINFO)；
这些信息本质上是动态的，只有在内核完成加载之后才知道。
*/
STATIC INT32 OsMakeArgsStack(ELFLoadInfo *loadInfo, UINTPTR interpMapBase)
{
    UINTPTR auxVector[AUX_VECTOR_SIZE] = { 0 };//辅助向量
    UINTPTR *auxVecInfo = (UINTPTR *)auxVector;
    INT32 vecIndex = 0;
    INT32 ret;
#ifdef LOSCFG_KERNEL_VDSO
    vaddr_t vdsoLoadAddr;
#endif

    AUX_VEC_ENTRY(auxVector, vecIndex, AUX_PHDR,   loadInfo->loadAddr + loadInfo->execInfo.elfEhdr.elfPhoff);
    AUX_VEC_ENTRY(auxVector, vecIndex, AUX_PHENT,  sizeof(LD_ELF_PHDR));
    AUX_VEC_ENTRY(auxVector, vecIndex, AUX_PHNUM,  loadInfo->execInfo.elfEhdr.elfPhNum);
    AUX_VEC_ENTRY(auxVector, vecIndex, AUX_PAGESZ, PAGE_SIZE);
    AUX_VEC_ENTRY(auxVector, vecIndex, AUX_BASE,   interpMapBase);
    AUX_VEC_ENTRY(auxVector, vecIndex, AUX_FLAGS,  0);
    AUX_VEC_ENTRY(auxVector, vecIndex, AUX_ENTRY,  loadInfo->execInfo.elfEhdr.elfEntry);
    AUX_VEC_ENTRY(auxVector, vecIndex, AUX_UID,    0);
    AUX_VEC_ENTRY(auxVector, vecIndex, AUX_EUID,   0);
    AUX_VEC_ENTRY(auxVector, vecIndex, AUX_GID,    0);
    AUX_VEC_ENTRY(auxVector, vecIndex, AUX_EGID,   0);
    AUX_VEC_ENTRY(auxVector, vecIndex, AUX_HWCAP,  0);
    AUX_VEC_ENTRY(auxVector, vecIndex, AUX_CLKTCK, 0);
    AUX_VEC_ENTRY(auxVector, vecIndex, AUX_SECURE, 0);
    AUX_VEC_ENTRY(auxVector, vecIndex, AUX_RANDOM, 0);
    AUX_VEC_ENTRY(auxVector, vecIndex, AUX_EXECFN, (UINTPTR)loadInfo->execName);

#ifdef LOSCFG_KERNEL_VDSO
    vdsoLoadAddr = OsVdsoLoad(OsCurrProcessGet());
    if (vdsoLoadAddr != 0) {
        AUX_VEC_ENTRY(auxVector, vecIndex, AUX_SYSINFO_EHDR, vdsoLoadAddr);
    }
#endif
    AUX_VEC_ENTRY(auxVector, vecIndex, AUX_NULL,   0);

    ret = OsPutParamToStack(loadInfo, auxVecInfo, vecIndex);//设置到栈底保存
    if (ret != LOS_OK) {
        PRINT_ERR("%s[%d], Failed to put param to user stack\n", __FUNCTION__, __LINE__);
        return ret;
    }

    return LOS_OK;
}
//加载段信息,这是主体函数
STATIC INT32 OsLoadELFSegment(ELFLoadInfo *loadInfo)
{
    LD_ELF_PHDR *elfPhdrTemp = loadInfo->execInfo.elfPhdr;
    UINTPTR loadBase = 0;//加载基地址
    UINTPTR interpMapBase = 0;//解析器基地址
    UINT32 mapSize = 0;
    INT32 ret;
    loadInfo->loadAddr = 0;

    if (loadInfo->execInfo.elfEhdr.elfType == LD_ET_DYN) {//共享目标文件
        loadBase = EXEC_MMAP_BASE + OsGetRndOffset(loadInfo);//加载基地址
        mapSize = OsGetAllocSize(elfPhdrTemp, loadInfo->execInfo.elfEhdr.elfPhNum);
        if (mapSize == 0) {
            PRINT_ERR("%s[%d], Failed to get allocation size of file: %s!\n", __FUNCTION__, __LINE__,
                      loadInfo->fileName);
            return -EINVAL;
        }
    }
	//先映射 ELF文件各段
    ret = OsMmapELFFile(loadInfo->execInfo.fd, loadInfo->execInfo.elfPhdr, &loadInfo->execInfo.elfEhdr,
                        &loadInfo->loadAddr, mapSize, &loadBase);
    if (ret != LOS_OK) {
        PRINT_ERR("%s[%d]\n", __FUNCTION__, __LINE__);
        return ret;
    }

    if (loadInfo->interpInfo.fd != INVALID_FD) {//存在解析器的情况
        ret = OsLoadInterpBinary(loadInfo, &interpMapBase);//加载和映射 libc.so相关段 
        if (ret != LOS_OK) {
            return ret;
        }

        loadInfo->elfEntry = loadInfo->interpInfo.elfEhdr.elfEntry + interpMapBase;//解析器的装载点为进程程序装载点
        loadInfo->execInfo.elfEhdr.elfEntry = loadInfo->execInfo.elfEhdr.elfEntry + loadBase;
    } else {
        loadInfo->elfEntry = loadInfo->execInfo.elfEhdr.elfEntry;//直接用ELF头信息的程序装载点 "_start"
    }

    ret = OsMakeArgsStack(loadInfo, interpMapBase);//保存辅助向量
    if (ret != LOS_OK) {
        return ret;
    }
    if (!LOS_IsUserAddress((VADDR_T)loadInfo->stackTop)) {
        PRINT_ERR("%s[%d], StackTop is out of limit!\n", __FUNCTION__, __LINE__);
        return -EINVAL;
    }

    return LOS_OK;
}
//更新进程空间
STATIC VOID OsFlushAspace(ELFLoadInfo *loadInfo)
{
    LosProcessCB *processCB = OsCurrProcessGet();//获取当前进程

    OsExecDestroyTaskGroup();//任务退出

    loadInfo->oldSpace = processCB->vmSpace;//当前进程空间变成老空间
    processCB->vmSpace = loadInfo->newSpace;//ELF进程空间变成当前进程空间,腾笼换鸟
    processCB->vmSpace->heapBase += OsGetRndOffset(loadInfo);//堆起始地址
    processCB->vmSpace->heapNow = processCB->vmSpace->heapBase;//堆现地址
    processCB->vmSpace->mapBase += OsGetRndOffset(loadInfo);//映射地址
    processCB->vmSpace->mapSize = loadInfo->stackBase - processCB->vmSpace->mapBase;//映射区大小
    LOS_ArchMmuContextSwitch(&OsCurrProcessGet()->vmSpace->archMmu);//MMU切换
}
//反初始化信息加载
STATIC VOID OsDeInitLoadInfo(ELFLoadInfo *loadInfo)
{
#ifdef LOSCFG_ASLR
    (VOID)close(loadInfo->randomDevFD);
#endif

    if (loadInfo->execInfo.elfPhdr != NULL) {//ELF程序头
        (VOID)LOS_MemFree(m_aucSysMem0, loadInfo->execInfo.elfPhdr);
    }

    if (loadInfo->interpInfo.elfPhdr != NULL) {// lib/libc.so 程序头
        (VOID)LOS_MemFree(m_aucSysMem0, loadInfo->interpInfo.elfPhdr);
    }
}
//反初始化文件
STATIC VOID OsDeInitFiles(ELFLoadInfo *loadInfo)
{
    if (loadInfo->execInfo.fd != INVALID_FD) {
        (VOID)close(loadInfo->execInfo.fd);
    }

    if (loadInfo->interpInfo.fd != INVALID_FD) {
        (VOID)close(loadInfo->interpInfo.fd);
    }
#ifdef LOSCFG_FS_VFS
    delete_files_snapshot((struct files_struct *)loadInfo->oldFiles);//删除镜像文件
#endif
}
//加载ELF格式文件
INT32 OsLoadELFFile(ELFLoadInfo *loadInfo)
{
    INT32 ret;

    OsLoadInit(loadInfo);//初始化加载信息

    ret = OsReadEhdr(loadInfo->fileName, &loadInfo->execInfo, TRUE);//读ELF头信息
    if (ret != LOS_OK) {
        goto OUT;
    }

    ret = OsReadPhdrs(&loadInfo->execInfo, TRUE);//读ELF程序头信息,构建进程映像所需信息.
    if (ret != LOS_OK) {
        goto OUT;
    }

    ret = OsReadInterpInfo(loadInfo);//读取段 INTERP 解析器信息
    if (ret != LOS_OK) {
        goto OUT;
    }

    ret = OsSetArgParams(loadInfo, loadInfo->argv, loadInfo->envp);//设置外部参数内容
    if (ret != LOS_OK) {
        goto OUT;
    }

    OsFlushAspace(loadInfo);//擦除空间

    ret = OsLoadELFSegment(loadInfo);//加载段信息
    if (ret != LOS_OK) {//加载失败时
        OsCurrProcessGet()->vmSpace = loadInfo->oldSpace;//切回原有虚拟空间
        LOS_ArchMmuContextSwitch(&OsCurrProcessGet()->vmSpace->archMmu);//切回原有MMU
        goto OUT;
    }

    OsDeInitLoadInfo(loadInfo);//ELF和.so 加载完成后释放内存

    return LOS_OK;

OUT:
    OsDeInitFiles(loadInfo);
    (VOID)LOS_VmSpaceFree(loadInfo->newSpace);
    (VOID)OsDeInitLoadInfo(loadInfo);
    return ret;
}
