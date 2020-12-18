/*
 * Copyright (c) 2013-2019, Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020, Huawei Device Co., Ltd. All rights reserved.
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
#include "fs_file.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */
//鸿蒙与Linux标准库的差异 https://gitee.com/openharmony/docs/blob/master/kernel/%E4%B8%8ELinux%E6%A0%87%E5%87%86%E5%BA%93%E7%9A%84%E5%B7%AE%E5%BC%82.md
/**************************************************
系统调用|申请虚拟内存
参数		描述		
addr	用来请求使用某个特定的虚拟内存地址。如果取NULL，结果地址就将自动分配（这是推荐的做法），
		否则会降低程序的可移植性，因为不同系统的可用地址范围不一样。
length	内存段的大小。
prot	用于设置内存段的访问权限，有如下权限：
		PROT_READ：允许读该内存段。
		PROT_WRITE：允许写该内存段。
		PROT_EXEC：允许执行该内存段。
		PROT_NONE：不能访问。
flags	控制程序对内存段的改变所造成的影响，有如下属性：
		MAP_PRIVATE：内存段私有，对它的修改值仅对本进程有效。
		MAP_SHARED：把对该内存段的修改保存到磁盘文件中。
fd		打开的文件描述符。
offset	用以改变经共享内存段访问的文件中数据的起始偏移值。
成功返回：虚拟内存地址，这地址是页对齐。
失败返回：(void *)-1。
**************************************************/
void *SysMmap(void *addr, size_t size, int prot, int flags, int fd, size_t offset)
{
    /* Process fd convert to system global fd */
    fd = GetAssociatedSystemFd(fd);

    return (void *)LOS_MMap((uintptr_t)addr, size, prot, flags, fd, offset);
}
/**************************************************
释放虚拟内存
addr	虚拟内存起始位置
length	内存段的大小
成功返回0		失败返回-1。
**************************************************/
int SysMunmap(void *addr, size_t size)
{
    return LOS_UnMMap((uintptr_t)addr, size);
}
/**************************************************
重新映射虚拟内存地址
参数				描述
old_address		需要扩大（或缩小）的内存段的原始地址。注意old_address必须是页对齐。
old_size		内存段的原始大小。
new_size		新内存段的大小。
flags			如果没有足够的空间在当前位置展开映射，则返回失败
				MREMAP_MAYMOVE：允许内核将映射重定位到新的虚拟地址。
				MREMAP_FIXED：mremap()接受第五个参数，void *new_address，该参数指定映射地址必须页对齐；
							在new_address和new_size指定的地址范围内的所有先前映射都被解除映射。如果指定了MREMAP_FIXED，
							还必须指定MREMAP_MAYMOVE。
成功返回：重新映射后的虚拟内存地址	
失败返回：((void *)-1)。
**************************************************/
void *SysMremap(void *oldAddr, size_t oldLen, size_t newLen, int flags, void *newAddr)
{
    return (void *)LOS_DoMremap((vaddr_t)oldAddr, oldLen, newLen, flags, (vaddr_t)newAddr);
}
/**************************************************

**************************************************/
int SysMprotect(void *vaddr, size_t len, int prot)
{
    return LOS_DoMprotect((uintptr_t)vaddr, len, (unsigned long)prot);
}
/**************************************************

**************************************************/
void *SysBrk(void *addr)
{
    return LOS_DoBrk(addr);
}
/**************************************************
获得共享内存对象
**************************************************/

int SysShmGet(key_t key, size_t size, int shmflg)
{
    int ret;

    ret = ShmGet(key, size, shmflg);
    if (ret < 0) {
        return -get_errno();
    }

    return ret;
}
/**************************************************

**************************************************/
void *SysShmAt(int shmid, const void *shmaddr, int shmflg)
{
    void *ret = NULL;

    ret = ShmAt(shmid, shmaddr, shmflg);
    if (ret == (void *)-1) {
        return (void *)(intptr_t)-get_errno();
    }

    return ret;
}

int SysShmCtl(int shmid, int cmd, struct shmid_ds *buf)
{
    int ret;

    ret = ShmCtl(shmid, cmd, buf);
    if (ret < 0) {
        return -get_errno();
    }

    return ret;
}

int SysShmDt(const void *shmaddr)
{
    int ret;

    ret = ShmDt(shmaddr);
    if (ret < 0) {
        return -get_errno();
    }

    return ret;
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */
