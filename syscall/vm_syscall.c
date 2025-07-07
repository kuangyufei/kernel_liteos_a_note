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

#include "sys/types.h"
#include "sys/shm.h"
#include "errno.h"
#include "unistd.h"
#include "los_vm_syscall.h"
#include "fs/file.h"

/**
 * @brief 系统调用：内存映射
 * @param addr 用户指定的映射起始地址，可为NULL由系统自动分配
 * @param size 映射区域大小（字节）
 * @param prot 内存保护标志（PROT_READ/PROT_WRITE等）
 * @param flags 映射标志（MAP_SHARED/MAP_PRIVATE等）
 * @param fd 要映射的文件描述符
 * @param offset 文件偏移量（必须是页大小的整数倍）
 * @return 成功返回映射区域起始地址，失败返回MAP_FAILED
 */
void *SysMmap(void *addr, size_t size, int prot, int flags, int fd, size_t offset)
{
    /* 将用户空间文件描述符转换为系统全局文件描述符 */
    fd = GetAssociatedSystemFd(fd);

    return (void *)LOS_MMap((uintptr_t)addr, size, prot, flags, fd, offset);  // 调用内核MMAP接口完成映射
}

/**
 * @brief 系统调用：解除内存映射
 * @param addr 要解除映射的内存起始地址
 * @param size 要解除映射的内存大小（字节）
 * @return 成功返回0，失败返回-1并设置errno
 */
int SysMunmap(void *addr, size_t size)
{
    return LOS_UnMMap((uintptr_t)addr, size);  // 调用内核UNMAP接口解除映射
}

/**
 * @brief 系统调用：重新映射内存区域
 * @param oldAddr 旧映射区域起始地址
 * @param oldLen 旧映射区域大小
 * @param newLen 新映射区域大小
 * @param flags 重映射标志（MREMAP_MAYMOVE等）
 * @param newAddr 建议的新映射地址（可为NULL）
 * @return 成功返回新映射区域起始地址，失败返回(void *)-1
 */
void *SysMremap(void *oldAddr, size_t oldLen, size_t newLen, int flags, void *newAddr)
{
    return (void *)LOS_DoMremap((vaddr_t)oldAddr, oldLen, newLen, flags, (vaddr_t)newAddr);  // 调用内核MREMAP接口
}

/**
 * @brief 系统调用：修改内存保护属性
 * @param vaddr 内存区域起始地址
 * @param len 内存区域长度
 * @param prot 新的内存保护标志
 * @return 成功返回0，失败返回-1并设置errno
 */
int SysMprotect(void *vaddr, size_t len, int prot)
{
    return LOS_DoMprotect((uintptr_t)vaddr, len, (unsigned long)prot);  // 调用内核MPROTECT接口
}

/**
 * @brief 系统调用：调整程序数据段大小（brk机制）
 * @param addr 新的程序断点地址，为NULL时返回当前断点
 * @return 成功返回新的断点地址，失败返回(void *)-1
 */
void *SysBrk(void *addr)
{
    return LOS_DoBrk(addr);  // 调用内核BRK接口调整断点
}

#ifdef LOSCFG_KERNEL_SHM  // 若启用共享内存配置
/**
 * @brief 系统调用：获取共享内存标识符
 * @param key 共享内存键值
 * @param size 共享内存大小
 * @param shmflg 创建标志（IPC_CREAT/IPC_EXCL等）和权限位
 * @return 成功返回共享内存ID，失败返回-1并设置errno
 */
int SysShmGet(key_t key, size_t size, int shmflg)
{
    int ret;

    ret = ShmGet(key, size, shmflg);  // 调用共享内存获取接口
    if (ret < 0) {  // 检查返回值，小于0表示失败
        return -get_errno();  // 返回错误码（取反）
    }

    return ret;  // 返回共享内存ID
}

/**
 * @brief 系统调用：将共享内存附加到进程地址空间
 * @param shmid 共享内存标识符
 * @param shmaddr 建议的附加地址（可为NULL由系统分配）
 * @param shmflg 附加标志（SHM_RDONLY等）
 * @return 成功返回附加地址，失败返回(void *)-1并设置errno
 */
void *SysShmAt(int shmid, const void *shmaddr, int shmflg)
{
    void *ret = NULL;

    ret = ShmAt(shmid, shmaddr, shmflg);  // 调用共享内存附加接口
    if (ret == (void *)-1) {  // 检查是否附加失败
        return (void *)(intptr_t)-get_errno();  // 返回错误码（取反）
    }

    return ret;  // 返回附加地址
}

/**
 * @brief 系统调用：控制共享内存（查询/设置属性）
 * @param shmid 共享内存标识符
 * @param cmd 控制命令（IPC_STAT/IPC_SET/IPC_RMID等）
 * @param buf 共享内存属性结构体指针
 * @return 成功返回0或命令特定值，失败返回-1并设置errno
 */
int SysShmCtl(int shmid, int cmd, struct shmid_ds *buf)
{
    int ret;

    ret = ShmCtl(shmid, cmd, buf);  // 调用共享内存控制接口
    if (ret < 0) {  // 检查返回值，小于0表示失败
        return -get_errno();  // 返回错误码（取反）
    }

    return ret;  // 返回控制结果
}

/**
 * @brief 系统调用：将共享内存从进程地址空间分离
 * @param shmaddr 要分离的共享内存地址
 * @return 成功返回0，失败返回-1并设置errno
 */
int SysShmDt(const void *shmaddr)
{
    int ret;

    ret = ShmDt(shmaddr);  // 调用共享内存分离接口
    if (ret < 0) {  // 检查返回值，小于0表示失败
        return -get_errno();  // 返回错误码（取反）
    }

    return ret;  // 返回分离结果
}
#endif
#endif

