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

STATIC BOOL g_srandInit;

STATIC INT32 OsELFOpen(const CHAR *fileName, INT32 oflags)
{
    INT32 ret;
    INT32 procFd;

    procFd = AllocProcessFd();
    if (procFd < 0) {
        return -EMFILE;
    }

    if (oflags & O_CLOEXEC) {
        SetCloexecFlag(procFd);
    }

    ret = open(fileName, oflags);
    if (ret < 0) {
        FreeProcessFd(procFd);
        return -get_errno();
    }

    AssociateSystemFd(procFd, ret);
    return procFd;
}

STATIC INT32 OsELFClose(INT32 procFd)
{
    INT32 ret;
    /* Process procfd convert to system global procfd */
    INT32 sysfd = DisassociateProcessFd(procFd);
    if (sysfd < 0) {
        return -EBADF;
    }

    ret = close(sysfd);
    if (ret < 0) {
        AssociateSystemFd(procFd, sysfd);
        return -get_errno();
    }
    FreeProcessFd(procFd);
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

STATIC INT32 OsReadELFInfo(INT32 procfd, UINT8 *buffer, size_t readSize, off_t offset)
{
    ssize_t byteNum;
    off_t returnPos;
    INT32 fd = GetAssociatedSystemFd(procfd);
    if (fd < 0) {
        PRINT_ERR("%s[%d], Invalid procfd!\n", __FUNCTION__, __LINE__);
        return LOS_NOK;
    }

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

STATIC INT32 OsVerifyELFEhdr(const LD_ELF_EHDR *ehdr, UINT32 fileLen)
{
    if (memcmp(ehdr->elfIdent, LD_ELFMAG, LD_SELFMAG) != 0) {
        PRINT_ERR("%s[%d], The file is not elf!\n", __FUNCTION__, __LINE__);
        return LOS_NOK;
    }
    if ((ehdr->elfType != LD_ET_EXEC) && (ehdr->elfType != LD_ET_DYN)) {
        PRINT_ERR("%s[%d], The type of file is not ET_EXEC or ET_DYN!\n", __FUNCTION__, __LINE__);
        return LOS_NOK;
    }
    if (ehdr->elfMachine != LD_EM_ARM) {
        PRINT_ERR("%s[%d], The type of machine is not EM_ARM!\n", __FUNCTION__, __LINE__);
        return LOS_NOK;
    }
    if (ehdr->elfPhNum > ELF_PHDR_NUM_MAX) {
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
    loadInfo->oldFiles = (UINTPTR)create_files_snapshot(oldFiles);
#else
    loadInfo->oldFiles = NULL;
#endif
    loadInfo->execInfo.procfd = INVALID_FD;
    loadInfo->interpInfo.procfd = INVALID_FD;
}

STATIC INT32 OsReadEhdr(const CHAR *fileName, ELFInfo *elfInfo, BOOL isExecFile)
{
    INT32 ret;

    ret = OsGetFileLength(&elfInfo->fileLen, fileName);
    if (ret != LOS_OK) {
        return -ENOENT;
    }

    ret = OsELFOpen(fileName, O_RDONLY | O_EXECVE | O_CLOEXEC);
    if (ret < 0) {
        PRINT_ERR("%s[%d], Failed to open ELF file: %s!\n", __FUNCTION__, __LINE__, fileName);
        return ret;
    }
    elfInfo->procfd = ret;

#ifdef LOSCFG_DRIVERS_TZDRIVER
    if (isExecFile) {
        struct file *filep = NULL;
        ret = fs_getfilep(GetAssociatedSystemFd(elfInfo->procfd), &filep);
        if (ret) {
            PRINT_ERR("%s[%d], Failed to get struct file %s!\n", __FUNCTION__, __LINE__, fileName);
            /* File will be closed by OsLoadELFFile */
            return ret;
        }
        OsCurrProcessGet()->execVnode = filep->f_vnode;
    }
#endif
    ret = OsReadELFInfo(elfInfo->procfd, (UINT8 *)&elfInfo->elfEhdr, sizeof(LD_ELF_EHDR), 0);
    if (ret != LOS_OK) {
        PRINT_ERR("%s[%d]\n", __FUNCTION__, __LINE__);
        return -EIO;
    }

    ret = OsVerifyELFEhdr(&elfInfo->elfEhdr, elfInfo->fileLen);
    if (ret != LOS_OK) {
        PRINT_ERR("%s[%d]\n", __FUNCTION__, __LINE__);
        return isExecFile ? -ENOEXEC : -ELIBBAD;
    }

    return LOS_OK;
}

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

    size = sizeof(LD_ELF_PHDR) * elfEhdr->elfPhNum;
    if ((elfEhdr->elfPhoff + size) > elfInfo->fileLen) {
        goto OUT;
    }

    elfInfo->elfPhdr = LOS_MemAlloc(m_aucSysMem0, size);
    if (elfInfo->elfPhdr == NULL) {
        PRINT_ERR("%s[%d], Failed to allocate for elfPhdr!\n", __FUNCTION__, __LINE__);
        return -ENOMEM;
    }

    ret = OsReadELFInfo(elfInfo->procfd, (UINT8 *)elfInfo->elfPhdr, size, elfEhdr->elfPhoff);
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

STATIC INT32 OsReadInterpInfo(ELFLoadInfo *loadInfo)
{
    LD_ELF_PHDR *elfPhdr = loadInfo->execInfo.elfPhdr;
    CHAR *elfInterpName = NULL;
    INT32 ret, i;

    for (i = 0; i < loadInfo->execInfo.elfEhdr.elfPhNum; ++i, ++elfPhdr) {
        if (elfPhdr->type != LD_PT_INTERP) {
            continue;
        }

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

        ret = OsReadELFInfo(loadInfo->execInfo.procfd, (UINT8 *)elfInterpName, elfPhdr->fileSize, elfPhdr->offset);
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

        ret = OsReadEhdr(INTERP_FULL_PATH, &loadInfo->interpInfo, FALSE);
        if (ret != LOS_OK) {
            PRINT_ERR("%s[%d]\n", __FUNCTION__, __LINE__);
            goto OUT;
        }

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
    UINT32 offset = elfPhdr->offset - ROUNDOFFSET(elfPhdr->vAddr, PAGE_SIZE);
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

INT32 OsGetKernelVaddr(LosVmSpace *space, VADDR_T vaddr, VADDR_T *kvaddr)
{
    INT32 ret;
    PADDR_T paddr = 0;

    if ((space == NULL) || (vaddr == 0) || (kvaddr == NULL)) {
        PRINT_ERR("%s[%d], space: %#x, vaddr: %#x\n", __FUNCTION__, __LINE__, space, vaddr);
        return LOS_NOK;
    }

    if (LOS_IsKernelAddress(vaddr)) {
        *kvaddr = vaddr;
        return LOS_OK;
    }

    ret = LOS_ArchMmuQuery(&space->archMmu, vaddr, &paddr, NULL);
    if (ret != LOS_OK) {
        PRINT_ERR("%s[%d], Failed to query the vaddr: %#x, status: %d\n", __FUNCTION__, __LINE__, vaddr, ret);
        return LOS_NOK;
    }
    *kvaddr = (VADDR_T)(UINTPTR)LOS_PaddrToKVaddr(paddr);
    if (*kvaddr == 0) {
        PRINT_ERR("%s[%d], kvaddr is null\n", __FUNCTION__, __LINE__);
        return LOS_NOK;
    }

    return LOS_OK;
}

STATIC INT32 OsSetBss(const LD_ELF_PHDR *elfPhdr, INT32 fd, UINTPTR bssStart, UINT32 bssEnd, UINT32 elfProt)
{
    UINTPTR bssStartPageAlign, bssEndPageAlign;
    UINTPTR mapBase;
    UINT32 bssMapSize;
    INT32 stackFlags;
    INT32 ret;

    bssStartPageAlign = ROUNDUP(bssStart, PAGE_SIZE);
    bssEndPageAlign = ROUNDUP(bssEnd, PAGE_SIZE);

    ret = LOS_UserMemClear((VOID *)bssStart, PAGE_SIZE - ROUNDOFFSET(bssStart, PAGE_SIZE));
    if (ret != 0) {
        PRINT_ERR("%s[%d], Failed to clear bss\n", __FUNCTION__, __LINE__);
        return -EFAULT;
    }

    bssMapSize = bssEndPageAlign - bssStartPageAlign;
    if (bssMapSize > 0) {
        stackFlags = MAP_PRIVATE | MAP_FIXED | MAP_ANONYMOUS;
        mapBase = (UINTPTR)LOS_MMap(bssStartPageAlign, bssMapSize, elfProt, stackFlags, -1, 0);
        if (!LOS_IsUserAddress((VADDR_T)mapBase)) {
            PRINT_ERR("%s[%d], Failed to map bss\n", __FUNCTION__, __LINE__);
            return -ENOMEM;
        }
    }

    return LOS_OK;
}

STATIC INT32 OsMmapELFFile(INT32 procfd, const LD_ELF_PHDR *elfPhdr, const LD_ELF_EHDR *elfEhdr, UINTPTR *elfLoadAddr,
                           UINT32 mapSize, UINTPTR *loadBase)
{
    const LD_ELF_PHDR *elfPhdrTemp = elfPhdr;
    UINTPTR vAddr, mapAddr, bssStart;
    UINT32 bssEnd, elfProt, elfFlags;
    INT32 ret, i;
    INT32 fd = GetAssociatedSystemFd(procfd);

    for (i = 0; i < elfEhdr->elfPhNum; ++i, ++elfPhdrTemp) {
        if (elfPhdrTemp->type != LD_PT_LOAD) {
            continue;
        }
        if (elfEhdr->elfType == LD_ET_EXEC) {
            if (OsVerifyELFPhdr(elfPhdrTemp) != LOS_OK) {
                return -ENOEXEC;
            }
        }

        elfProt = OsGetProt(elfPhdrTemp->flags);
        if ((elfProt & PROT_READ) == 0) {
            return -ENOEXEC;
        }
        elfFlags = MAP_PRIVATE | MAP_FIXED;
        vAddr = elfPhdrTemp->vAddr;
        if ((vAddr == 0) && (*loadBase == 0)) {
            elfFlags &= ~MAP_FIXED;
        }

        mapAddr = OsDoMmapFile(fd, (vAddr + *loadBase), elfPhdrTemp, elfProt, elfFlags, mapSize);
        if (!LOS_IsUserAddress((VADDR_T)mapAddr)) {
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
            *loadBase = mapAddr;
        }

        if ((elfPhdrTemp->memSize > elfPhdrTemp->fileSize) && (elfPhdrTemp->flags & PF_W)) {
            bssStart = mapAddr + ROUNDOFFSET(vAddr, PAGE_SIZE) + elfPhdrTemp->fileSize;
            bssEnd = mapAddr + ROUNDOFFSET(vAddr, PAGE_SIZE) + elfPhdrTemp->memSize;
            ret = OsSetBss(elfPhdrTemp, fd, bssStart, bssEnd, elfProt);
            if (ret != LOS_OK) {
                return ret;
            }
        }
    }

    return LOS_OK;
}

STATIC INT32 OsLoadInterpBinary(ELFLoadInfo *loadInfo, UINTPTR *interpMapBase)
{
    UINTPTR loadBase = 0;
    UINT32 mapSize;
    INT32 ret;

    mapSize = OsGetAllocSize(loadInfo->interpInfo.elfPhdr, loadInfo->interpInfo.elfEhdr.elfPhNum);
    if (mapSize == 0) {
        PRINT_ERR("%s[%d], Failed to get interp allocation size!\n", __FUNCTION__, __LINE__);
        return -EINVAL;
    }

    ret = OsMmapELFFile(loadInfo->interpInfo.procfd, loadInfo->interpInfo.elfPhdr, &loadInfo->interpInfo.elfEhdr,
                        interpMapBase, mapSize, &loadBase);
    if (ret != LOS_OK) {
        PRINT_ERR("%s[%d]\n", __FUNCTION__, __LINE__);
    }

    return ret;
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

    ret = OsGetKernelVaddr(loadInfo->newSpace, loadInfo->stackParamBase, &kvaddr);
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

        /* copy strings to user stack */
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

UINT32 OsGetRndOffset(INT32 randomDevFD)
{
    UINT32 randomValue = 0;

#ifdef LOSCFG_ASLR
    if (read(randomDevFD, &randomValue, sizeof(UINT32)) == sizeof(UINT32)) {
        randomValue &= RANDOM_MASK;
    } else {
        randomValue = (UINT32)random() & RANDOM_MASK;
    }
#else
    (VOID)randomDevFD;
#endif

    return ROUNDDOWN(randomValue, PAGE_SIZE);
}

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

STATIC UINT32 OsStackAlloc(LosVmSpace *space, VADDR_T vaddr, UINT32 vsize, UINT32 psize, UINT32 regionFlags)
{
    LosVmPage *vmPage = NULL;
    VADDR_T *kvaddr = NULL;
    LosVmMapRegion *region = NULL;
    VADDR_T vaddrTemp;
    PADDR_T paddrTemp;
    UINT32 len;

    (VOID)LOS_MuxAcquire(&space->regionMux);
    kvaddr = LOS_PhysPagesAllocContiguous(psize >> PAGE_SHIFT);
    if (kvaddr == NULL) {
        goto OUT;
    }

    region = LOS_RegionAlloc(space, vaddr, vsize, regionFlags | VM_MAP_REGION_FLAG_FIXED, 0);
    if (region == NULL) {
        goto PFREE;
    }

    len = psize;
    vaddrTemp = region->range.base + vsize - psize;
    paddrTemp = LOS_PaddrQuery(kvaddr);
    while (len > 0) {
        vmPage = LOS_VmPageGet(paddrTemp);
        LOS_AtomicInc(&vmPage->refCounts);

        (VOID)LOS_ArchMmuMap(&space->archMmu, vaddrTemp, paddrTemp, 1, region->regionFlags);

        paddrTemp += PAGE_SIZE;
        vaddrTemp += PAGE_SIZE;
        len -= PAGE_SIZE;
    }
    (VOID)LOS_MuxRelease(&space->regionMux);
    return LOS_OK;

PFREE:
    (VOID)LOS_PhysPagesFreeContiguous(kvaddr, psize >> PAGE_SHIFT);
OUT:
    (VOID)LOS_MuxRelease(&space->regionMux);
    return LOS_NOK;
}

STATIC INT32 OsSetArgParams(ELFLoadInfo *loadInfo, CHAR *const *argv, CHAR *const *envp)
{
    UINT32 vmFlags;
    INT32 ret;

    loadInfo->randomDevFD = open("/dev/urandom", O_RDONLY);
    if (loadInfo->randomDevFD < 0) {
        if (!g_srandInit) {
            srand((UINT32)time(NULL));
            g_srandInit = TRUE;
        }
    }

    (VOID)OsGetStackProt(loadInfo);
    if (((UINT32)loadInfo->stackProt & (PROT_READ | PROT_WRITE)) != (PROT_READ | PROT_WRITE)) {
        return -ENOEXEC;
    }
    loadInfo->stackTopMax = USER_STACK_TOP_MAX - OsGetRndOffset(loadInfo->randomDevFD);
    loadInfo->stackBase = loadInfo->stackTopMax - USER_STACK_SIZE;
    loadInfo->stackSize = USER_STACK_SIZE;
    loadInfo->stackParamBase = loadInfo->stackTopMax - USER_PARAM_BYTE_MAX;
    vmFlags = OsCvtProtFlagsToRegionFlags(loadInfo->stackProt, MAP_FIXED);
    vmFlags |= VM_MAP_REGION_FLAG_STACK;
    ret = OsStackAlloc((VOID *)loadInfo->newSpace, loadInfo->stackBase, USER_STACK_SIZE,
                       USER_PARAM_BYTE_MAX, vmFlags);
    if (ret != LOS_OK) {
        PRINT_ERR("%s[%d], Failed to alloc memory for user stack!\n", __FUNCTION__, __LINE__);
        return -ENOMEM;
    }
    loadInfo->topOfMem = loadInfo->stackTopMax - sizeof(UINTPTR);

    loadInfo->argc = OsGetParamNum(argv);
    loadInfo->envc = OsGetParamNum(envp);
    ret = OsCopyParams(loadInfo, 1, (CHAR *const *)&loadInfo->fileName);
    if (ret != LOS_OK) {
        PRINT_ERR("%s[%d], Failed to copy filename to user stack!\n", __FUNCTION__, __LINE__);
        return ret;
    }
    loadInfo->execName = (CHAR *)loadInfo->topOfMem;

    ret = OsCopyParams(loadInfo, loadInfo->envc, envp);
    if (ret != LOS_OK) {
        return ret;
    }
    ret = OsCopyParams(loadInfo, loadInfo->argc, argv);
    if (ret != LOS_OK) {
        return ret;
    }
    loadInfo->argStart = loadInfo->topOfMem;

    return LOS_OK;
}

STATIC INT32 OsPutParamToStack(ELFLoadInfo *loadInfo, const UINTPTR *auxVecInfo, INT32 vecIndex)
{
    UINTPTR *topMem = (UINTPTR *)ROUNDDOWN(loadInfo->topOfMem, sizeof(UINTPTR));
    UINTPTR *argsPtr = NULL;
    INT32 items = (loadInfo->argc + 1) + (loadInfo->envc + 1) + 1;
    size_t size;

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

    if ((OsPutUserArgv(&loadInfo->argStart, &argsPtr, loadInfo->argc) != LOS_OK) ||
        (OsPutUserArgv(&loadInfo->argStart, &argsPtr, loadInfo->envc) != LOS_OK)) {
        PRINT_ERR("%s[%d], Failed to put argv or envp to user stack!\n", __FUNCTION__, __LINE__);
        return -EFAULT;
    }

    size = LOS_ArchCopyToUser(argsPtr, auxVecInfo, vecIndex * sizeof(UINTPTR));
    if (size != 0) {
        PRINT_ERR("%s[%d], Failed to copy strings! Bytes not copied: %d\n", __FUNCTION__, __LINE__, size);
        return -EFAULT;
    }

    return LOS_OK;
}

STATIC INT32 OsGetRndNum(const ELFLoadInfo *loadInfo, UINT32 *rndVec, UINT32 vecSize)
{
    UINT32 randomValue = 0;
    UINT32 i, ret;

    for (i = 0; i < vecSize; ++i) {
        ret = read(loadInfo->randomDevFD, &randomValue, sizeof(UINT32));
        if (ret != sizeof(UINT32)) {
            rndVec[i] = (UINT32)random();
            continue;
        }
        rndVec[i] = randomValue;
    }

    return LOS_OK;
}

STATIC INT32 OsMakeArgsStack(ELFLoadInfo *loadInfo, UINTPTR interpMapBase)
{
    UINTPTR auxVector[AUX_VECTOR_SIZE] = { 0 };
    UINTPTR *auxVecInfo = (UINTPTR *)auxVector;
    INT32 vecIndex = 0;
    UINT32 rndVec[RANDOM_VECTOR_SIZE];
    UINTPTR rndVecStart;
    INT32 ret;
#ifdef LOSCFG_KERNEL_VDSO
    vaddr_t vdsoLoadAddr;
#endif

    ret = OsGetRndNum(loadInfo, rndVec, sizeof(rndVec));
    if (ret != LOS_OK) {
        return ret;
    }
    loadInfo->topOfMem -= sizeof(rndVec);
    rndVecStart = loadInfo->topOfMem;

    ret = LOS_ArchCopyToUser((VOID *)loadInfo->topOfMem, rndVec, sizeof(rndVec));
    if (ret != 0) {
        return -EFAULT;
    }

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
    AUX_VEC_ENTRY(auxVector, vecIndex, AUX_RANDOM, rndVecStart);
    AUX_VEC_ENTRY(auxVector, vecIndex, AUX_EXECFN, (UINTPTR)loadInfo->execName);

#ifdef LOSCFG_KERNEL_VDSO
    vdsoLoadAddr = OsVdsoLoad(OsCurrProcessGet());
    if (vdsoLoadAddr != 0) {
        AUX_VEC_ENTRY(auxVector, vecIndex, AUX_SYSINFO_EHDR, vdsoLoadAddr);
    }
#endif
    AUX_VEC_ENTRY(auxVector, vecIndex, AUX_NULL,   0);

    ret = OsPutParamToStack(loadInfo, auxVecInfo, vecIndex);
    if (ret != LOS_OK) {
        PRINT_ERR("%s[%d], Failed to put param to user stack\n", __FUNCTION__, __LINE__);
        return ret;
    }

    return LOS_OK;
}

STATIC INT32 OsLoadELFSegment(ELFLoadInfo *loadInfo)
{
    LD_ELF_PHDR *elfPhdrTemp = loadInfo->execInfo.elfPhdr;
    UINTPTR loadBase = 0;
    UINTPTR interpMapBase = 0;
    UINT32 mapSize = 0;
    INT32 ret;
    loadInfo->loadAddr = 0;

    if (loadInfo->execInfo.elfEhdr.elfType == LD_ET_DYN) {
        loadBase = EXEC_MMAP_BASE + OsGetRndOffset(loadInfo->randomDevFD);
        mapSize = OsGetAllocSize(elfPhdrTemp, loadInfo->execInfo.elfEhdr.elfPhNum);
        if (mapSize == 0) {
            PRINT_ERR("%s[%d], Failed to get allocation size of file: %s!\n", __FUNCTION__, __LINE__,
                      loadInfo->fileName);
            return -EINVAL;
        }
    }

    ret = OsMmapELFFile(loadInfo->execInfo.procfd, loadInfo->execInfo.elfPhdr, &loadInfo->execInfo.elfEhdr,
                        &loadInfo->loadAddr, mapSize, &loadBase);
    OsELFClose(loadInfo->execInfo.procfd);
    loadInfo->execInfo.procfd = INVALID_FD;
    if (ret != LOS_OK) {
        PRINT_ERR("%s[%d]\n", __FUNCTION__, __LINE__);
        return ret;
    }

    if (loadInfo->interpInfo.procfd != INVALID_FD) {
        ret = OsLoadInterpBinary(loadInfo, &interpMapBase);
        OsELFClose(loadInfo->interpInfo.procfd);
        loadInfo->interpInfo.procfd = INVALID_FD;
        if (ret != LOS_OK) {
            return ret;
        }

        loadInfo->elfEntry = loadInfo->interpInfo.elfEhdr.elfEntry + interpMapBase;
        loadInfo->execInfo.elfEhdr.elfEntry = loadInfo->execInfo.elfEhdr.elfEntry + loadBase;
    } else {
        loadInfo->elfEntry = loadInfo->execInfo.elfEhdr.elfEntry;
    }

    ret = OsMakeArgsStack(loadInfo, interpMapBase);
    if (ret != LOS_OK) {
        return ret;
    }
    if (!LOS_IsUserAddress((VADDR_T)loadInfo->stackTop)) {
        PRINT_ERR("%s[%d], StackTop is out of limit!\n", __FUNCTION__, __LINE__);
        return -EINVAL;
    }

    return LOS_OK;
}

STATIC VOID OsFlushAspace(ELFLoadInfo *loadInfo)
{
    loadInfo->oldSpace = OsExecProcessVmSpaceReplace(loadInfo->newSpace, loadInfo->stackBase, loadInfo->randomDevFD);
}

STATIC VOID OsDeInitLoadInfo(ELFLoadInfo *loadInfo)
{
    (VOID)close(loadInfo->randomDevFD);

    if (loadInfo->execInfo.elfPhdr != NULL) {
        (VOID)LOS_MemFree(m_aucSysMem0, loadInfo->execInfo.elfPhdr);
    }

    if (loadInfo->interpInfo.elfPhdr != NULL) {
        (VOID)LOS_MemFree(m_aucSysMem0, loadInfo->interpInfo.elfPhdr);
    }
}

STATIC VOID OsDeInitFiles(ELFLoadInfo *loadInfo)
{
    if (loadInfo->execInfo.procfd != INVALID_FD) {
        (VOID)OsELFClose(loadInfo->execInfo.procfd);
    }

    if (loadInfo->interpInfo.procfd != INVALID_FD) {
        (VOID)OsELFClose(loadInfo->interpInfo.procfd);
    }
#ifdef LOSCFG_FS_VFS
    delete_files_snapshot((struct files_struct *)loadInfo->oldFiles);
#endif
}

INT32 OsLoadELFFile(ELFLoadInfo *loadInfo)
{
    INT32 ret;

    OsLoadInit(loadInfo);

    ret = OsReadEhdr(loadInfo->fileName, &loadInfo->execInfo, TRUE);
    if (ret != LOS_OK) {
        goto OUT;
    }

    ret = OsReadPhdrs(&loadInfo->execInfo, TRUE);
    if (ret != LOS_OK) {
        goto OUT;
    }

    ret = OsReadInterpInfo(loadInfo);
    if (ret != LOS_OK) {
        goto OUT;
    }

    ret = OsSetArgParams(loadInfo, loadInfo->argv, loadInfo->envp);
    if (ret != LOS_OK) {
        goto OUT;
    }

    OsFlushAspace(loadInfo);

    ret = OsLoadELFSegment(loadInfo);
    if (ret != LOS_OK) {
        OsExecProcessVmSpaceRestore(loadInfo->oldSpace);
        goto OUT;
    }

    OsDeInitLoadInfo(loadInfo);

    return LOS_OK;

OUT:
    OsDeInitFiles(loadInfo);
    (VOID)LOS_VmSpaceFree(loadInfo->newSpace);
    (VOID)OsDeInitLoadInfo(loadInfo);
    return ret;
}
