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


//鸿蒙与Linux标准库的差异 https://weharmony.gitee.io/zh-cn/device-dev/kernel/%E4%B8%8ELinux%E6%A0%87%E5%87%86%E5%BA%93%E7%9A%84%E5%B7%AE%E5%BC%82/
/**************************************************
系统调用|申请虚拟内存(分配线性地址区间)
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
		MAP_PRIVATE：标志指定线性区中的页可以被进程独享
		MAP_SHARED：标志指定线性区中的页可以被几个进程共享
fd		打开的文件描述符,如果新的线性区将把一个文件映射到内存的情况
offset	用以改变经共享内存段访问的文件中数据的起始偏移值。
成功返回：虚拟内存地址，这地址是页对齐。
失败返回：(void *)-1。
**************************************************/
void *SysMmap(void *addr, size_t size, int prot, int flags, int fd, size_t offset)
{
    /* Process fd convert to system global fd */
    fd = GetAssociatedSystemFd(fd);

    return (void *)LOS_MMap((uintptr_t)addr, size, prot, flags, fd, offset);//分配线性地址区间
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
brk也是申请堆内存的一种方式,一般小于 128K 会使用它
**************************************************/
void *SysBrk(void *addr)
{
    return LOS_DoBrk(addr);
}
/**************************************************
得到一个共享内存标识符或创建一个共享内存对象
key_t:	建立新共享内存对象 标识符是IPC对象的内部名。为使多个合作进程能够在同一IPC对象上汇聚，需要提供一个外部命名方案。
		为此，每个IPC对象都与一个键（key）相关联，这个键作为该对象的外部名,无论何时创建IPC结构（通过msgget、semget、shmget创建），
		都应给IPC指定一个键, key_t由ftok创建,ftok当然在本工程里找不到,所以要写这么多.
size: 新建的共享内存大小，以字节为单位
shmflg: IPC_CREAT IPC_EXCL
		IPC_CREAT：	在创建新的IPC时，如果key参数是IPC_PRIVATE或者和当前某种类型的IPC结构无关，则需要指明flag参数的IPC_CREAT标志位，
					则用来创建一个新的IPC结构。（如果IPC结构已存在，并且指定了IPC_CREAT，则IPC_CREAT什么都不做，函数也不出错）
		IPC_EXCL：	此参数一般与IPC_CREAT配合使用来创建一个新的IPC结构。如果创建的IPC结构已存在函数就出错返回，
					返回EEXIST（这与open函数指定O_CREAT和O_EXCL标志原理相同）
**************************************************/
#ifdef LOSCFG_KERNEL_SHM
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
连接共享内存标识符为shmid的共享内存，连接成功后把共享内存区对象映射到调用进程的地址空间，随后可像本地空间一样访问
一旦创建/引用了一个共享存储段，那么进程就可调用shmat函数将其连接到它的地址空间中
如果shmat成功执行，那么内核将使与该共享存储相关的shmid_ds结构中的shm_nattch计数器值加1
shmid 就是个索引,就跟进程和线程的ID一样 g_shmSegs[shmid] shmid > 192个
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
/**************************************************
完成对共享内存的控制
此函数可以对shmid指定的共享存储进行多种操作（删除、取信息、加锁、解锁等）

msqid	共享内存标识符
cmd		IPC_STAT：得到共享内存的状态，把共享内存的shmid_ds结构复制到buf中
		IPC_SET：改变共享内存的状态，把buf所指的shmid_ds结构中的uid、gid、mode复制到共享内存的shmid_ds结构内
		IPC_RMID：删除这片共享内存
buf		共享内存管理结构体。
**************************************************/
int SysShmCtl(int shmid, int cmd, struct shmid_ds *buf)
{
    int ret;

    ret = ShmCtl(shmid, cmd, buf);
    if (ret < 0) {
        return -get_errno();
    }

    return ret;
}
/**************************************************
与shmat函数相反，是用来断开与共享内存附加点的地址，禁止本进程访问此片共享内存
shmaddr：连接的共享内存的起始地址
本函数调用并不删除所指定的共享内存区，而只是将先前用shmat函数连接（attach）好的共享内存脱离（detach）目前的进程
返回值	成功：0	出错：-1，错误原因存于error中r
**************************************************/
int SysShmDt(const void *shmaddr)
{
    int ret;

    ret = ShmDt(shmaddr);
    if (ret < 0) {
        return -get_errno();
    }

    return ret;
}
#endif

