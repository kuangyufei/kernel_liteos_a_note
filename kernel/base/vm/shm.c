/*!
 * @file    shm.c
 * @brief
 * @link
   @verbatim
    什么是共享内存
	顾名思义，共享内存就是允许两个不相关的进程访问同一个物理内存。共享内存是在两个正在运行的进程之间
	共享和传递数据的一种非常有效的方式。不同进程之间共享的内存通常安排为同一段物理内存。进程可以将同
	一段共享内存连接到它们自己的地址空间中，所有进程都可以访问共享内存中的地址，就好像它们是由用C语言
	函数malloc()分配的内存一样。而如果某个进程向共享内存写入数据，所做的改动将立即影响到可以访问同一段
	共享内存的任何其他进程。

	特别提醒：共享内存并未提供同步机制，也就是说，在第一个进程结束对共享内存的写操作之前，并无自动机制
	可以阻止第二个进程开始对它进行读取。所以我们通常需要用其他的机制来同步对共享内存的访问

	共享线性区可以由任意的进程创建,每个使用共享线性区都必须经过映射.
   @endverbatim
 * @version 
 * @author  weharmonyos.com | 鸿蒙研究站 | 每天死磕一点点
 * @date    2021-12-24
 */
/*
 * Copyright (c) 2013-2019 Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020-2023 Huawei Device Co., Ltd. All rights reserved.
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

#include "stdio.h"
#include "string.h"
#include "time.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "los_config.h"
#include "los_init.h"
#include "los_vm_map.h"
#include "los_vm_filemap.h"
#include "los_vm_phys.h"
#include "los_arch_mmu.h"
#include "los_vm_page.h"
#include "los_vm_lock.h"
#include "los_process.h"
#include "los_process_pri.h"
#include "user_copy.h"
#include "los_vm_shm_pri.h"
#include "sys/shm.h"
#ifdef LOSCFG_SHELL
#include "shcmd.h"
#include "shell.h"
#endif


#ifdef LOSCFG_KERNEL_SHM

#define SHM_SEG_FREE    0x2000	//空闲未使用
#define SHM_SEG_USED    0x4000	//已使用
#define SHM_SEG_REMOVE  0x8000	//删除

#ifndef SHM_M
#define SHM_M   010000
#endif

#ifndef SHM_X
#define SHM_X   0100
#endif
#ifndef ACCESSPERMS
#define ACCESSPERMS (S_IRWXU | S_IRWXG | S_IRWXO)//文件权限值意思就是 用户,用户组,其他可读可写.
#endif //代表含义U:user G:group O:other

#define SHM_S_IRUGO (S_IRUSR | S_IRGRP | S_IROTH)
#define SHM_S_IWUGO (S_IWUSR | S_IWGRP | S_IWOTH)
#define SHM_S_IXUGO (S_IXUSR | S_IXGRP | S_IXOTH)
#define SHM_GROUPE_TO_USER  3
#define SHM_OTHER_TO_USER   6
#ifndef LOSCFG_IPC_CONTAINER
STATIC LosMux g_sysvShmMux;
/* private data */
STATIC struct shminfo g_shmInfo;
STATIC struct shmIDSource *g_shmSegs = NULL;
STATIC UINT32 g_shmUsedPageCount;

#define IPC_SHM_INFO            g_shmInfo
#define IPC_SHM_SYS_VSHM_MUTEX  g_sysvShmMux
#define IPC_SHM_SEGS            g_shmSegs
#define IPC_SHM_USED_PAGE_COUNT g_shmUsedPageCount
#endif

/* private macro */
#define SYSV_SHM_LOCK()     (VOID)LOS_MuxLock(&IPC_SHM_SYS_VSHM_MUTEX, LOS_WAIT_FOREVER)
#define SYSV_SHM_UNLOCK()   (VOID)LOS_MuxUnlock(&IPC_SHM_SYS_VSHM_MUTEX)

#if 0 // @note_#if0

//内核为每一个IPC对象保存一个ipc_perm结构体，该结构说明了IPC对象的权限和所有者
struct ipc_perm {
	key_t __ipc_perm_key;	//调用shmget()时给出的关键字
	uid_t uid;				//共享内存所有者的有效用户ID
	gid_t gid;				//共享内存所有者所属组的有效组ID
	uid_t cuid;				//共享内存创建 者的有效用户ID
	gid_t cgid;				//共享内存创建者所属组的有效组ID
	mode_t mode;			//权限 + SHM_DEST / SHM_LOCKED /SHM_HUGETLB 标志位
	int __ipc_perm_seq;		//序列号
	long __pad1;			//保留扩展用
	long __pad2;
};
//每个共享内存段在内核中维护着一个内部结构shmid_ds
struct shmid_ds {
	struct ipc_perm shm_perm;///< 操作许可，里面包含共享内存的用户ID、组ID等信息
	size_t shm_segsz;	///< 共享内存段的大小，单位为字节
	time_t shm_atime;	///< 最后一个进程访问共享内存的时间	
	time_t shm_dtime; 	///< 最后一个进程离开共享内存的时间
	time_t shm_ctime; 	///< 创建时间
	pid_t shm_cpid;		///< 创建共享内存的进程ID
	pid_t shm_lpid;		///< 最后操作共享内存的进程ID
	unsigned long shm_nattch;	///< 当前使用该共享内存段的进程数量
	unsigned long __pad1;	//保留扩展用
	unsigned long __pad2;
};
// 共享内存模块设置信息
struct shminfo {
	unsigned long shmmax, shmmin, shmmni, shmseg, shmall, __unused[4];
};
struct shmIDSource {//共享内存描述符
    struct shmid_ds ds; //是内核为每一个共享内存段维护的数据结构
    UINT32 status;	//状态 SHM_SEG_FREE ...
    LOS_DL_LIST node; //节点,挂VmPage
#ifdef LOSCFG_SHELL
    CHAR ownerName[OS_PCB_NAME_LEN];
#endif
};

/* private data */
STATIC struct shminfo g_shmInfo = { //描述共享内存范围的全局变量
    .shmmax = SHM_MAX,//共享内存单个上限 4096页 即 16M
    .shmmin = SHM_MIN,//共享内存单个下限 1页 即:4K
    .shmmni = SHM_MNI,//共享内存总数 默认192 
    .shmseg = SHM_SEG,//每个用户进程可以使用的最多的共享内存段的数目 128
    .shmall = SHM_ALL,//系统范围内共享内存的总页数, 4096页 
};

STATIC struct shmIDSource *g_shmSegs = NULL;
STATIC UINT32 g_shmUsedPageCount;
#endif

//共享内存初始化
struct shmIDSource *OsShmCBInit(LosMux *sysvShmMux, struct shminfo *shmInfo, UINT32 *shmUsedPageCount)
{
    UINT32 ret;
    UINT32 i;

    if ((sysvShmMux == NULL) || (shmInfo == NULL) || (shmUsedPageCount == NULL)) {
        return NULL;
    }
    ret = LOS_MuxInit(sysvShmMux, NULL);
    if (ret != LOS_OK) {
        goto ERROR;
    }

    shmInfo->shmmax = SHM_MAX;
    shmInfo->shmmin = SHM_MIN;
    shmInfo->shmmni = SHM_MNI;
    shmInfo->shmseg = SHM_SEG;
    shmInfo->shmall = SHM_ALL;
    struct shmIDSource *shmSegs = LOS_MemAlloc((VOID *)OS_SYS_MEM_ADDR, sizeof(struct shmIDSource) * shmInfo->shmmni);
    if (shmSegs == NULL) {
        (VOID)LOS_MuxDestroy(sysvShmMux);
        goto ERROR;
    }
    (VOID)memset_s(shmSegs, (sizeof(struct shmIDSource) * shmInfo->shmmni),
                   0, (sizeof(struct shmIDSource) * shmInfo->shmmni));

    for (i = 0; i < shmInfo->shmmni; i++) {
        shmSegs[i].status = SHM_SEG_FREE;//节点初始状态为空闲
        shmSegs[i].ds.shm_perm.seq = i + 1;//struct ipc_perm shm_perm;系统为每一个IPC对象保存一个ipc_perm结构体,结构说明了IPC对象的权限和所有者
        LOS_ListInit(&shmSegs[i].node);//初始化节点
    }
    *shmUsedPageCount = 0;

    return shmSegs;

ERROR:
    VM_ERR("ShmInit fail\n");
    return NULL;
}
UINT32 ShmInit(VOID)
{
#ifndef LOSCFG_IPC_CONTAINER
    g_shmSegs = OsShmCBInit(&IPC_SHM_SYS_VSHM_MUTEX, &IPC_SHM_INFO, &IPC_SHM_USED_PAGE_COUNT);
    if (g_shmSegs == NULL) {
        return LOS_NOK;
    }
#endif
    return LOS_OK;
}


LOS_MODULE_INIT(ShmInit, LOS_INIT_LEVEL_VM_COMPLETE);//共享内存模块初始化
//共享内存反初始化
UINT32 ShmDeinit(VOID)
{
    UINT32 ret;

    (VOID)LOS_MemFree((VOID *)OS_SYS_MEM_ADDR, IPC_SHM_SEGS);
    IPC_SHM_SEGS = NULL;

    ret = LOS_MuxDestroy(&IPC_SHM_SYS_VSHM_MUTEX);
    if (ret != LOS_OK) {
        return -1;
    }

    return 0;
}
///给共享段中所有物理页框贴上共享标签
STATIC inline VOID ShmSetSharedFlag(struct shmIDSource *seg)
{
    LosVmPage *page = NULL;

    LOS_DL_LIST_FOR_EACH_ENTRY(page, &seg->node, LosVmPage, node) {
        OsSetPageShared(page);
    }
}
///给共享段中所有物理页框撕掉共享标签
STATIC inline VOID ShmClearSharedFlag(struct shmIDSource *seg)
{
    LosVmPage *page = NULL;

    LOS_DL_LIST_FOR_EACH_ENTRY(page, &seg->node, LosVmPage, node) {
        OsCleanPageShared(page);
    }
}
///seg下所有共享页引用减少
STATIC VOID ShmPagesRefDec(struct shmIDSource *seg)
{
    LosVmPage *page = NULL;

    LOS_DL_LIST_FOR_EACH_ENTRY(page, &seg->node, LosVmPage, node) {
        LOS_AtomicDec(&page->refCounts);
    }
}

/**
 * @brief 为共享段分配物理内存
 例如:参数size = 4097, LOS_Align(size, PAGE_SIZE) = 8192
 分配页数    size >> PAGE_SHIFT = 2页 
 * @param key 
 * @param size 
 * @param shmflg 
 * @return STATIC 
 */
STATIC INT32 ShmAllocSegCheck(key_t key, size_t *size, INT32 *segNum)
{
    INT32 i;
    if ((*size == 0) || (*size < IPC_SHM_INFO.shmmin) ||
        (*size > IPC_SHM_INFO.shmmax)) {
        return -EINVAL;
    }

    *size = LOS_Align(*size, PAGE_SIZE);//必须对齐
    if ((IPC_SHM_USED_PAGE_COUNT + (*size >> PAGE_SHIFT)) > IPC_SHM_INFO.shmall) {
        return -ENOMEM;
    }

#ifdef LOSCFG_KERNEL_IPC_PLIMIT
    if (OsIPCLimitShmAlloc(*size) != LOS_OK) {
        return -ENOMEM;
    }
#endif
    for (i = 0; i < IPC_SHM_INFO.shmmni; i++) {//试图找到一个空闲段与参数key绑定
        if (IPC_SHM_SEGS[i].status & SHM_SEG_FREE) {//找到空闲段
            IPC_SHM_SEGS[i].status &= ~SHM_SEG_FREE;//变成非空闲状态
            *segNum = i;//标号
            break;
        }
    }

    if (*segNum < 0) {
        return -ENOSPC;
    }
    return 0;
}

STATIC INT32 ShmAllocSeg(key_t key, size_t size, INT32 shmflg)
{
    INT32 segNum = -1;
    struct shmIDSource *seg = NULL;
    size_t count;

    INT32 ret = ShmAllocSegCheck(key, &size, &segNum);
    if (ret < 0) {
        return ret;
    }
    seg = &IPC_SHM_SEGS[segNum];
    count = LOS_PhysPagesAlloc(size >> PAGE_SHIFT, &seg->node);//分配共享页面,函数内部把node都挂好了.
    if (count != (size >> PAGE_SHIFT)) {//当未分配到足够的内存时,处理方式是:不稀罕给那么点,舍弃!
        (VOID)LOS_PhysPagesFree(&seg->node);//释放节点上的物理页框
        seg->status = SHM_SEG_FREE;//共享段变回空闲状态
#ifdef LOSCFG_KERNEL_IPC_PLIMIT
        OsIPCLimitShmFree(size);
#endif
        return -ENOMEM;
    }
    ShmSetSharedFlag(seg);//将node的每个页面设置为共享页
    IPC_SHM_USED_PAGE_COUNT += size >> PAGE_SHIFT;

    seg->status |= SHM_SEG_USED;	//共享段贴上已在使用的标签
    seg->ds.shm_perm.mode = (UINT32)shmflg & ACCESSPERMS;
    seg->ds.shm_perm.key = key;//保存参数key,如此 key 和 共享ID绑定在一块
    seg->ds.shm_segsz = size;	//共享段的大小
    seg->ds.shm_perm.cuid = LOS_GetUserID();	//设置用户ID
    seg->ds.shm_perm.uid = LOS_GetUserID();		//设置用户ID
    seg->ds.shm_perm.cgid = LOS_GetGroupID();	//设置组ID
    seg->ds.shm_perm.gid = LOS_GetGroupID();	//设置组ID
    seg->ds.shm_lpid = 0; //最后一个操作的进程
    seg->ds.shm_nattch = 0;	//绑定进程的数量					
    seg->ds.shm_cpid = LOS_GetCurrProcessID();	//获取进程ID
    seg->ds.shm_atime = 0;	//访问时间
    seg->ds.shm_dtime = 0;	//detach 分离时间 共享内存使用完之后，需要将它从进程地址空间中分离出来；将共享内存分离并不是删除它，只是使该共享内存对当前的进程不再可用
    seg->ds.shm_ctime = time(NULL);//创建时间
#ifdef LOSCFG_SHELL
    (VOID)memcpy_s(seg->ownerName, OS_PCB_NAME_LEN, OsCurrProcessGet()->processName, OS_PCB_NAME_LEN);
#endif

    return segNum;
}
///释放seg->node 所占物理页框,seg本身重置
STATIC INLINE VOID ShmFreeSeg(struct shmIDSource *seg, UINT32 *shmUsedPageCount)
{
    UINT32 count;

    ShmClearSharedFlag(seg);//先撕掉 seg->node 中vmpage的共享标签
    count = LOS_PhysPagesFree(&seg->node);//再挨个删除物理页框
    if (count != (seg->ds.shm_segsz >> PAGE_SHIFT)) {//异常,必须要一样
        VM_ERR("free physical pages failed, count = %d, size = %d", count, seg->ds.shm_segsz >> PAGE_SHIFT);
        return;
    }
#ifdef LOSCFG_KERNEL_IPC_PLIMIT
    OsIPCLimitShmFree(seg->ds.shm_segsz);
#endif
    if (shmUsedPageCount != NULL) {
        (*shmUsedPageCount) -= seg->ds.shm_segsz >> PAGE_SHIFT;
    }
    seg->status = SHM_SEG_FREE;//seg恢复自由之身
    LOS_ListInit(&seg->node);//重置node
}
///通过key查找 shmId
STATIC INT32 ShmFindSegByKey(key_t key)
{
    INT32 i;
    struct shmIDSource *seg = NULL;

    for (i = 0; i < IPC_SHM_INFO.shmmni; i++) {//遍历共享段池,找到与key绑定的共享ID
        seg = &IPC_SHM_SEGS[i];
        if ((seg->status & SHM_SEG_USED) &&
            (seg->ds.shm_perm.key == key)) {//满足两个条件,找到后返回
            return i;
        }
    }

    return -1;
}
///共享内存段有效性检查
STATIC INT32 ShmSegValidCheck(INT32 segNum, size_t size, INT32 shmFlg)
{
    struct shmIDSource *seg = &IPC_SHM_SEGS[segNum];//拿到shmID

    if (size > seg->ds.shm_segsz) {//段长
        return -EINVAL;
    }

    if (((UINT32)shmFlg & (IPC_CREAT | IPC_EXCL)) ==
        (IPC_CREAT | IPC_EXCL)) {
        return -EEXIST;
    }

    return segNum;
}
///通过ID找到共享内存资源
STATIC struct shmIDSource *ShmFindSeg(int shmid)
{
    struct shmIDSource *seg = NULL;

    if ((shmid < 0) || (shmid >= IPC_SHM_INFO.shmmni)) {
        set_errno(EINVAL);
        return NULL;
    }

    seg = &IPC_SHM_SEGS[shmid];
    if ((seg->status & SHM_SEG_FREE) || ((seg->ds.shm_nattch == 0) && (seg->status & SHM_SEG_REMOVE))) {
        set_errno(EIDRM);
        return NULL;
    }

    return seg;
}
///共享内存映射
STATIC VOID ShmVmmMapping(LosVmSpace *space, LOS_DL_LIST *pageList, VADDR_T vaddr, UINT32 regionFlags)
{
    LosVmPage *vmPage = NULL;
    VADDR_T va = vaddr;
    PADDR_T pa;
    STATUS_T ret;

    LOS_DL_LIST_FOR_EACH_ENTRY(vmPage, pageList, LosVmPage, node) {//遍历一页一页映射
        pa = VM_PAGE_TO_PHYS(vmPage);//拿到物理地址
        LOS_AtomicInc(&vmPage->refCounts);//自增
        ret = LOS_ArchMmuMap(&space->archMmu, va, pa, 1, regionFlags);//虚实映射
        if (ret != 1) {
            VM_ERR("LOS_ArchMmuMap failed, ret = %d", ret);
        }
        va += PAGE_SIZE;
    }
}
///fork 一个共享线性区
VOID OsShmFork(LosVmSpace *space, LosVmMapRegion *oldRegion, LosVmMapRegion *newRegion)
{
    struct shmIDSource *seg = NULL;

    SYSV_SHM_LOCK();
    seg = ShmFindSeg(oldRegion->shmid);//通过老区ID获取对应的共享资源ID结构体
    if (seg == NULL) {
        SYSV_SHM_UNLOCK();
        VM_ERR("shm fork failed!");
        return;
    }

    newRegion->shmid = oldRegion->shmid;//一样的共享区ID
    newRegion->forkFlags = oldRegion->forkFlags;//forkFlags也一样了
    ShmVmmMapping(space, &seg->node, newRegion->range.base, newRegion->regionFlags);//新线性区与共享内存进行映射
    seg->ds.shm_nattch++;//附在共享线性区上的进程数++
    SYSV_SHM_UNLOCK();
}
///释放共享线性区
VOID OsShmRegionFree(LosVmSpace *space, LosVmMapRegion *region)
{
    struct shmIDSource *seg = NULL;

    SYSV_SHM_LOCK();
    seg = ShmFindSeg(region->shmid);//通过线性区ID获取对应的共享资源ID结构体
    if (seg == NULL) {
        SYSV_SHM_UNLOCK();
        return;
    }

    LOS_ArchMmuUnmap(&space->archMmu, region->range.base, region->range.size >> PAGE_SHIFT);//解除线性区的映射
    ShmPagesRefDec(seg);//ref -- 
    seg->ds.shm_nattch--;//附在共享线性区上的进程数--
    if (seg->ds.shm_nattch <= 0 && (seg->status & SHM_SEG_REMOVE)) {
        ShmFreeSeg(seg, &IPC_SHM_USED_PAGE_COUNT);//就释放掉物理内存!注意是:物理内存
    } else {
        seg->ds.shm_dtime = time(NULL);
        seg->ds.shm_lpid = LOS_GetCurrProcessID(); /* may not be the space's PID. */
    }
    SYSV_SHM_UNLOCK();
}
///是否为共享线性区,是否有标签?
BOOL OsIsShmRegion(LosVmMapRegion *region)
{
    return (region->regionFlags & VM_MAP_REGION_FLAG_SHM) ? TRUE : FALSE;
}
///获取共享内存池中已被使用的段数量
STATIC INT32 ShmSegUsedCount(VOID)
{
    INT32 i;
    INT32 count = 0;
    struct shmIDSource *seg = NULL;

    for (i = 0; i < IPC_SHM_INFO.shmmni; i++) {
        seg = &IPC_SHM_SEGS[i];
        if (seg->status & SHM_SEG_USED) {//找到一个
            count++;
        }
    }
    return count;
}
///对共享内存段权限检查
STATIC INT32 ShmPermCheck(struct shmIDSource *seg, mode_t mode)
{
    INT32 uid = LOS_GetUserID();//当前进程的用户ID
    UINT32 tmpMode = 0;
    mode_t privMode = seg->ds.shm_perm.mode;
    mode_t accMode;

    if ((uid == seg->ds.shm_perm.uid) || (uid == seg->ds.shm_perm.cuid)) {
        tmpMode |= SHM_M;
        accMode = mode & S_IRWXU;
    } else if (LOS_CheckInGroups(seg->ds.shm_perm.gid) ||
               LOS_CheckInGroups(seg->ds.shm_perm.cgid)) {
        privMode <<= SHM_GROUPE_TO_USER;
        accMode = (mode & S_IRWXG) << SHM_GROUPE_TO_USER;
    } else {
        privMode <<= SHM_OTHER_TO_USER;
        accMode = (mode & S_IRWXO) << SHM_OTHER_TO_USER;
    }

    if (privMode & SHM_R) {
        tmpMode |= SHM_R;
    }

    if (privMode & SHM_W) {
        tmpMode |= SHM_W;
    }

    if (privMode & SHM_X) {
        tmpMode |= SHM_X;
    }

    if ((mode == SHM_M) && (tmpMode & SHM_M)) {
        return 0;
    } else if (mode == SHM_M) {
        return EACCES;
    }

    if ((tmpMode & accMode) == accMode) {
        return 0;
    } else {
        return EACCES;
    }
}

/*!
 * @brief ShmGet	
 *	得到一个共享内存标识符或创建一个共享内存对象
 * @param key	建立新共享内存对象 标识符是IPC对象的内部名。为使多个合作进程能够在同一IPC对象上汇聚，需要提供一个外部命名方案。
		为此，每个IPC对象都与一个键（key）相关联，这个键作为该对象的外部名,无论何时创建IPC结构（通过msgget、semget、shmget创建），
		都应给IPC指定一个键, key_t由ftok创建,ftok当然在本工程里找不到,所以要写这么多.
 * @param shmflg	IPC_CREAT IPC_EXCL
			IPC_CREAT：	在创建新的IPC时，如果key参数是IPC_PRIVATE或者和当前某种类型的IPC结构无关，则需要指明flag参数的IPC_CREAT标志位，
						则用来创建一个新的IPC结构。（如果IPC结构已存在，并且指定了IPC_CREAT，则IPC_CREAT什么都不做，函数也不出错）
			IPC_EXCL：	此参数一般与IPC_CREAT配合使用来创建一个新的IPC结构。如果创建的IPC结构已存在函数就出错返回，
						返回EEXIST（这与open函数指定O_CREAT和O_EXCL标志原理相同）
 * @param size	新建的共享内存大小，以字节为单位
 * @return	
 *
 * @see
 */
INT32 ShmGet(key_t key, size_t size, INT32 shmflg)
{
    INT32 ret;
    INT32 shmid;

    SYSV_SHM_LOCK();

    if (key == IPC_PRIVATE) {
        ret = ShmAllocSeg(key, size, shmflg);
    } else {
        ret = ShmFindSegByKey(key);//通过key查找资源ID
        if (ret < 0) {
            if (((UINT32)shmflg & IPC_CREAT) == 0) {//
                ret = -ENOENT;
                goto ERROR;
            } else {
                ret = ShmAllocSeg(key, size, shmflg);//分配一个共享内存
            }
        } else {
            shmid = ret;
            if (((UINT32)shmflg & IPC_CREAT) &&
                ((UINT32)shmflg & IPC_EXCL)) {
                ret = -EEXIST;
                goto ERROR;
            }
            ret = ShmPermCheck(ShmFindSeg(shmid), (UINT32)shmflg & ACCESSPERMS);//对共享内存权限检查
            if (ret != 0) {
                ret = -ret;
                goto ERROR;
            }
            ret = ShmSegValidCheck(shmid, size, shmflg);
        }
    }
    if (ret < 0) {
        goto ERROR;
    }

    SYSV_SHM_UNLOCK();

    return ret;
ERROR:
    set_errno(-ret);
    SYSV_SHM_UNLOCK();
    PRINT_DEBUG("%s %d, ret = %d\n", __FUNCTION__, __LINE__, ret);
    return -1;
}

INT32 ShmatParamCheck(const VOID *shmaddr, INT32 shmflg)
{
    if (((UINT32)shmflg & SHM_REMAP) && (shmaddr == NULL)) {
        return EINVAL;
    }

    if ((shmaddr != NULL) && !IS_PAGE_ALIGNED(shmaddr) &&
        (((UINT32)shmflg & SHM_RND) == 0)) {
        return EINVAL;
    }

    return 0;
}
///分配一个共享线性区并映射好
LosVmMapRegion *ShmatVmmAlloc(struct shmIDSource *seg, const VOID *shmaddr,
                              INT32 shmflg, UINT32 prot)
{
    LosVmSpace *space = OsCurrProcessGet()->vmSpace;
    LosVmMapRegion *region = NULL;
    UINT32 flags = MAP_ANONYMOUS | MAP_SHARED;//本线性区为共享+匿名标签
    UINT32 mapFlags = flags | MAP_FIXED;
    VADDR_T vaddr;
    UINT32 regionFlags;
    INT32 ret;

    if (shmaddr != NULL) {
        flags |= MAP_FIXED_NOREPLACE;
    }
    regionFlags = OsCvtProtFlagsToRegionFlags(prot, flags);
    (VOID)LOS_MuxAcquire(&space->regionMux);
    if (shmaddr == NULL) {//未指定了共享内存连接到当前进程中的地址位置
        region = LOS_RegionAlloc(space, 0, seg->ds.shm_segsz, regionFlags, 0);//分配线性区
    } else {//指定时,就需要先找地址所在的线性区
        if ((UINT32)shmflg & SHM_RND) {
            vaddr = ROUNDDOWN((VADDR_T)(UINTPTR)shmaddr, SHMLBA);
        } else {
            vaddr = (VADDR_T)(UINTPTR)shmaddr;
        }//找到线性区并重新映射,当指定地址时需贴上重新映射的标签
        if (!((UINT32)shmflg & SHM_REMAP) && (LOS_RegionFind(space, vaddr) ||
            LOS_RegionFind(space, vaddr + seg->ds.shm_segsz - 1) ||
            LOS_RegionRangeFind(space, vaddr, seg->ds.shm_segsz - 1))) {
            ret = EINVAL;
            goto ERROR;
        }
        vaddr = (VADDR_T)LOS_MMap(vaddr, seg->ds.shm_segsz, prot, mapFlags, -1, 0);//做好映射
        region = LOS_RegionFind(space, vaddr);//重新查找线性区,用于返回.
    }

    if (region == NULL) {
        ret = ENOMEM;
        goto ERROR;
    }
    ShmVmmMapping(space, &seg->node, region->range.base, regionFlags);//共享内存映射
    (VOID)LOS_MuxRelease(&space->regionMux);
    return region;
ERROR:
    set_errno(ret);
    (VOID)LOS_MuxRelease(&space->regionMux);
    return NULL;
}

/*!
 * @brief ShmAt	
 * 用来启动对该共享内存的访问，并把共享内存连接到当前进程的地址空间。
 * @param shm_flg 是一组标志位，通常为0。
 * @param shmaddr 指定共享内存连接到当前进程中的地址位置，通常为空，表示让系统来选择共享内存的地址。
 * @param shmid	是shmget()函数返回的共享内存标识符
 * @return	
 * 如果shmat成功执行，那么内核将使与该共享存储相关的shmid_ds结构中的shm_nattch计数器值加1
   shmid 就是个索引,就跟进程和线程的ID一样 g_shmSegs[shmid] shmid > 192个
 * @see
 */
VOID *ShmAt(INT32 shmid, const VOID *shmaddr, INT32 shmflg)
{
    INT32 ret;
    UINT32 prot = PROT_READ;
    mode_t acc_mode = SHM_S_IRUGO;
    struct shmIDSource *seg = NULL;
    LosVmMapRegion *r = NULL;

    ret = ShmatParamCheck(shmaddr, shmflg);//参数检查
    if (ret != 0) {
        set_errno(ret);
        return (VOID *)-1;
    }

    if ((UINT32)shmflg & SHM_EXEC) {//flag 转换
        prot |= PROT_EXEC;
        acc_mode |= SHM_S_IXUGO;
    } else if (((UINT32)shmflg & SHM_RDONLY) == 0) {
        prot |= PROT_WRITE;
        acc_mode |= SHM_S_IWUGO;
    }

    SYSV_SHM_LOCK();
    seg = ShmFindSeg(shmid);//找到段
    if (seg == NULL) {
        SYSV_SHM_UNLOCK();
        return (VOID *)-1;
    }

    ret = ShmPermCheck(seg, acc_mode);
    if (ret != 0) {
        goto ERROR;
    }

    seg->ds.shm_nattch++;//ds上记录有一个进程绑定上来
    r = ShmatVmmAlloc(seg, shmaddr, shmflg, prot);//在当前进程空间分配一个线性区并映射到共享内存
    if (r == NULL) {
        seg->ds.shm_nattch--;
        SYSV_SHM_UNLOCK();
        return (VOID *)-1;
    }

    r->shmid = shmid;//把ID给线性区的shmid
    r->regionFlags |= VM_MAP_REGION_FLAG_SHM;//这是一个共享线性区
    seg->ds.shm_atime = time(NULL);//访问时间
    seg->ds.shm_lpid = LOS_GetCurrProcessID();//进程ID
    SYSV_SHM_UNLOCK();

    return (VOID *)(UINTPTR)r->range.base;
ERROR:
    set_errno(ret);
    SYSV_SHM_UNLOCK();
    PRINT_DEBUG("%s %d, ret = %d\n", __FUNCTION__, __LINE__, ret);
    return (VOID *)-1;
}

/*!
 * @brief ShmCtl	
 * 此函数可以对shmid指定的共享存储进行多种操作（删除、取信息、加锁、解锁等）
 * @param buf	是一个结构指针，它指向共享内存模式和访问权限的结构。
 * @param cmd	command是要采取的操作，它可以取下面的三个值 ：
	IPC_STAT：把shmid_ds结构中的数据设置为共享内存的当前关联值，即用共享内存的当前关联值覆盖shmid_ds的值。
	IPC_SET：如果进程有足够的权限，就把共享内存的当前关联值设置为shmid_ds结构中给出的值
	IPC_RMID：删除共享内存段
 * @param shmid	是shmget()函数返回的共享内存标识符
 * @return	
 *
 * @see
 */
INT32 ShmCtl(INT32 shmid, INT32 cmd, struct shmid_ds *buf)
{
    struct shmIDSource *seg = NULL;
    INT32 ret = 0;
    struct shm_info shmInfo = { 0 };
    struct ipc_perm shm_perm;

    cmd = ((UINT32)cmd & ~IPC_64);

    SYSV_SHM_LOCK();

    if ((cmd != IPC_INFO) && (cmd != SHM_INFO)) {
        seg = ShmFindSeg(shmid);//通过索引ID找到seg
        if (seg == NULL) {
            SYSV_SHM_UNLOCK();
            return -1;
        }
    }

    if ((buf == NULL) && (cmd != IPC_RMID)) {
        ret = EINVAL;
        goto ERROR;
    }

    switch (cmd) {
        case IPC_STAT:
        case SHM_STAT://取段结构
            ret = ShmPermCheck(seg, SHM_S_IRUGO);
            if (ret != 0) {
                goto ERROR;
            }

            ret = LOS_ArchCopyToUser(buf, &seg->ds, sizeof(struct shmid_ds));//把内核空间的共享页数据拷贝到用户空间
            if (ret != 0) {
                ret = EFAULT;
                goto ERROR;
            }
            if (cmd == SHM_STAT) {
                ret = (unsigned int)((unsigned int)seg->ds.shm_perm.seq << 16) | (unsigned int)((unsigned int)shmid & 0xffff); /* 16: use the seq as the upper 16 bits */
            }
            break;
        case IPC_SET://重置共享段
            ret = ShmPermCheck(seg, SHM_M);
            if (ret != 0) {
                ret = EPERM;
                goto ERROR;
            }
			//从用户空间拷贝数据到内核空间
            ret = LOS_ArchCopyFromUser(&shm_perm, &buf->shm_perm, sizeof(struct ipc_perm));
            if (ret != 0) {
                ret = EFAULT;
                goto ERROR;
            }
            seg->ds.shm_perm.uid = shm_perm.uid;
            seg->ds.shm_perm.gid = shm_perm.gid;
            seg->ds.shm_perm.mode = (seg->ds.shm_perm.mode & ~ACCESSPERMS) |
                                    (shm_perm.mode & ACCESSPERMS);//可访问
            seg->ds.shm_ctime = time(NULL);
#ifdef LOSCFG_SHELL
            if (OsProcessIDUserCheckInvalid(shm_perm.uid)) {
                break;
            }
            (VOID)memcpy_s(seg->ownerName, OS_PCB_NAME_LEN, OS_PCB_FROM_PID(shm_perm.uid)->processName,
                           OS_PCB_NAME_LEN);
#endif
            break;
        case IPC_RMID://删除共享段
            ret = ShmPermCheck(seg, SHM_M);
            if (ret != 0) {
                ret = EPERM;
                goto ERROR;
            }

            seg->status |= SHM_SEG_REMOVE;
            if (seg->ds.shm_nattch <= 0) {//没有任何进程在使用了
                ShmFreeSeg(seg, &IPC_SHM_USED_PAGE_COUNT);
            }
            break;
        case IPC_INFO://把内核空间的共享页数据拷贝到用户空间
            ret = LOS_ArchCopyToUser(buf, &IPC_SHM_INFO, sizeof(struct shminfo));
            if (ret != 0) {
                ret = EFAULT;
                goto ERROR;
            }
            ret = IPC_SHM_INFO.shmmni;
            break;
        case SHM_INFO:
            shmInfo.shm_rss = 0;
            shmInfo.shm_swp = 0;
            shmInfo.shm_tot = 0;
            shmInfo.swap_attempts = 0;
            shmInfo.swap_successes = 0;
            shmInfo.used_ids = ShmSegUsedCount();//在使用的seg数
            ret = LOS_ArchCopyToUser(buf, &shmInfo, sizeof(struct shm_info));//把内核空间的共享页数据拷贝到用户空间
            if (ret != 0) {
                ret = EFAULT;
                goto ERROR;
            }
            ret = IPC_SHM_INFO.shmmni;
            break;
        default:
            VM_ERR("the cmd(%d) is not supported!", cmd);
            ret = EINVAL;
            goto ERROR;
    }

    SYSV_SHM_UNLOCK();
    return ret;

ERROR:
    set_errno(ret);
    SYSV_SHM_UNLOCK();
    PRINT_DEBUG("%s %d, ret = %d\n", __FUNCTION__, __LINE__, ret);
    return -1;
}

/**
 * @brief 当对共享存储的操作已经结束时，则调用shmdt与该存储段分离
	如果shmat成功执行，那么内核将使与该共享存储相关的shmid_ds结构中的shm_nattch计数器值减1
 * @attention 注意：这并不从系统中删除共享存储的标识符以及其相关的数据结构。共享存储的仍然存在，
	直至某个进程带IPC_RMID命令的调用shmctl特地删除共享存储为止
 * @param shmaddr 
 * @return INT32 
 */
INT32 ShmDt(const VOID *shmaddr)
{
    LosVmSpace *space = OsCurrProcessGet()->vmSpace;//获取进程空间
    struct shmIDSource *seg = NULL;
    LosVmMapRegion *region = NULL;
    INT32 shmid;
    INT32 ret;

    if (IS_PAGE_ALIGNED(shmaddr) == 0) {//地址是否对齐
        ret = EINVAL;
        goto ERROR;
    }

    (VOID)LOS_MuxAcquire(&space->regionMux);
    region = LOS_RegionFind(space, (VADDR_T)(UINTPTR)shmaddr);//找到线性区
    if (region == NULL) {
        ret = EINVAL;
        goto ERROR_WITH_LOCK;
    }
    shmid = region->shmid;//线性区共享ID

    if (region->range.base != (VADDR_T)(UINTPTR)shmaddr) {//这是用户空间和内核空间的一次解绑
        ret = EINVAL;									//shmaddr 必须要等于region->range.base
        goto ERROR_WITH_LOCK;
    }

    /* remove it from aspace */
    LOS_RbDelNode(&space->regionRbTree, &region->rbNode);//从红黑树和链表中摘除节点
    LOS_ArchMmuUnmap(&space->archMmu, region->range.base, region->range.size >> PAGE_SHIFT);//解除线性区的映射
    (VOID)LOS_MuxRelease(&space->regionMux);
    /* free it */
    free(region);//释放线性区所占内存池中的内存

    SYSV_SHM_LOCK();
    seg = ShmFindSeg(shmid);//找到seg,线性区和共享段的关系是 1:N 的关系,其他空间的线性区也会绑在共享段上
    if (seg == NULL) {
        ret = EINVAL;
        SYSV_SHM_UNLOCK();
        goto ERROR;
    }

    ShmPagesRefDec(seg);//页面引用数 --
    seg->ds.shm_nattch--;//使用共享内存的进程数少了一个
    if ((seg->ds.shm_nattch <= 0) && //无任何进程使用共享内存
        (seg->status & SHM_SEG_REMOVE)) {//状态为删除时需要释放物理页内存了,否则其他进程还要继续使用共享内存
        ShmFreeSeg(seg, &IPC_SHM_USED_PAGE_COUNT);//释放seg 页框链表中的页框内存,再重置seg状态
    } else {
    seg->ds.shm_dtime = time(NULL);//记录分离的时间
    seg->ds.shm_lpid = LOS_GetCurrProcessID();//记录操作进程ID
    }
    SYSV_SHM_UNLOCK();

    return 0;

ERROR_WITH_LOCK:
    (VOID)LOS_MuxRelease(&space->regionMux);
ERROR:
    set_errno(ret);
    PRINT_DEBUG("%s %d, ret = %d\n", __FUNCTION__, __LINE__, ret);
    return -1;
}

VOID OsShmCBDestroy(struct shmIDSource *shmSegs, struct shminfo *shmInfo, LosMux *sysvShmMux)
{
    if ((shmSegs == NULL) || (shmInfo == NULL) || (sysvShmMux == NULL)) {
        return;
    }

    for (UINT32 index = 0; index < shmInfo->shmmni; index++) {
        struct shmIDSource *seg = &shmSegs[index];
        if (seg->status == SHM_SEG_FREE) {
            continue;
        }

        (VOID)LOS_MuxLock(sysvShmMux, LOS_WAIT_FOREVER);
        ShmFreeSeg(seg, NULL);
        (VOID)LOS_MuxUnlock(sysvShmMux);
    }

    (VOID)LOS_MemFree((VOID *)OS_SYS_MEM_ADDR, shmSegs);
    (VOID)LOS_MuxDestroy(sysvShmMux);
}

#ifdef LOSCFG_SHELL
STATIC VOID OsShmInfoCmd(VOID)
{
    INT32 i;
    struct shmIDSource *seg = NULL;

    PRINTK("\r\n------- Shared Memory Segments -------\n");
    PRINTK("key      shmid    perms      bytes      nattch     status     owner\n");
    SYSV_SHM_LOCK();
    for (i = 0; i < IPC_SHM_INFO.shmmni; i++) {
        seg = &IPC_SHM_SEGS[i];
        if (!(seg->status & SHM_SEG_USED)) {
            continue;
        }
        PRINTK("%08x %-8d %-10o %-10u %-10u %-10x %s\n", seg->ds.shm_perm.key,
               i, seg->ds.shm_perm.mode, seg->ds.shm_segsz, seg->ds.shm_nattch,
               seg->status, seg->ownerName);

    }
    SYSV_SHM_UNLOCK();
}
STATIC VOID OsShmDeleteCmd(INT32 shmid)
{
    struct shmIDSource *seg = NULL;

    if ((shmid < 0) || (shmid >= IPC_SHM_INFO.shmmni)) {
        PRINT_ERR("shmid is invalid: %d\n", shmid);
        return;
    }

    SYSV_SHM_LOCK();
    seg = ShmFindSeg(shmid);
    if (seg == NULL) {
        SYSV_SHM_UNLOCK();
        return;
    }

    if (seg->ds.shm_nattch <= 0) {
        ShmFreeSeg(seg, &IPC_SHM_USED_PAGE_COUNT);
    }
    SYSV_SHM_UNLOCK();
}

STATIC VOID OsShmCmdUsage(VOID)
{
    PRINTK("\tnone option,   print shm usage info\n"
           "\t-r [shmid],    Recycle the specified shared memory about shmid\n"
           "\t-h | --help,   print shm command usage\n");
}
///共享内存
UINT32 OsShellCmdShm(INT32 argc, const CHAR *argv[])
{
    INT32 shmid;
    CHAR *endPtr = NULL;

    if (argc == 0) {
        OsShmInfoCmd();
    } else if (argc == 1) {
        if ((strcmp(argv[0], "-h") != 0) && (strcmp(argv[0], "--help") != 0)) {
            PRINTK("Invalid option: %s\n", argv[0]);
        }
        OsShmCmdUsage();
    } else if (argc == 2) { /* 2: two parameter */
        if (strcmp(argv[0], "-r") != 0) {
            PRINTK("Invalid option: %s\n", argv[0]);
            goto DONE;
        }
        shmid = strtoul((CHAR *)argv[1], &endPtr, 0);
        if ((endPtr == NULL) || (*endPtr != 0)) {
            PRINTK("check shmid %s(us) invalid\n", argv[1]);
            goto DONE;
        }
        /* try to delete shm about shmid */
        OsShmDeleteCmd(shmid);
    }
    return 0;
DONE:
    OsShmCmdUsage();
    return -1;
}

SHELLCMD_ENTRY(shm_shellcmd, CMD_TYPE_SHOW, "shm", 2, (CmdCallBackFunc)OsShellCmdShm);
#endif
#endif

