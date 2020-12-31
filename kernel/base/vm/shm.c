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

#include "stdio.h"
#include "string.h"
#include "time.h"
#include "sys/types.h"
#include "sys/shm.h"
#include "sys/stat.h"
#include "los_config.h"
#include "los_vm_map.h"
#include "los_vm_filemap.h"
#include "los_vm_phys.h"
#include "los_arch_mmu.h"
#include "los_vm_page.h"
#include "los_vm_lock.h"
#include "los_process.h"
#include "los_process_pri.h"
#include "user_copy.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

/******************************************************************************
共享内存是进程间通信中最简单的方式之一。共享内存允许两个或更多进程访问同一块内存
不同进程返回了指向同一个物理内存区域的指针。当一个进程改变了这块地址中的内容的时候，
其它进程都会察觉到这个更改。

共享线性区可以由任意的进程创建,每个使用共享线性区都必须经过映射.
******************************************************************************/

STATIC LosMux g_sysvShmMux; //互斥锁,共享内存本身并不保证操作的同步性,所以需用互斥锁

/* private macro */
#define SYSV_SHM_LOCK()     (VOID)LOS_MuxLock(&g_sysvShmMux, LOS_WAIT_FOREVER)	//申请永久等待锁
#define SYSV_SHM_UNLOCK()   (VOID)LOS_MuxUnlock(&g_sysvShmMux)	//释放锁

#define SHM_MAX_PAGES 12800	//共享最大页
#define SHM_MAX (SHM_MAX_PAGES * PAGE_SIZE) // 最大共享空间,12800*4K = 50M
#define SHM_MIN 1	
#define SHM_MNI 192
#define SHM_SEG 128
#define SHM_ALL (SHM_MAX_PAGES)

#define SHM_SEG_FREE    0x2000	//空闲未使用
#define SHM_SEG_USED    0x4000	//已使用
#define SHM_SEG_REMOVE  0x8000	//删除

#ifndef SHM_M
#define SHM_M   010000
#endif

#ifndef ACCESSPERMS
#define ACCESSPERMS (S_IRWXU | S_IRWXG | S_IRWXO)//文件权限值意思就是 用户,用户组,其他可读可写.
#endif //代表含义U:user G:group O:other

#define SHM_GROUPE_TO_USER  3
#define SHM_OTHER_TO_USER   6

#if 0 // @note_#if0

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

struct shmid_ds {
	struct ipc_perm shm_perm; //内核为每一个IPC对象保存一个ipc_perm结构体，该结构说明了IPC对象的权限和所有者
	size_t shm_segsz;	//段大小
	time_t shm_atime;	//访问时间	
	time_t shm_dtime; 	//分离时间
	time_t shm_ctime; 	//创建时间
	pid_t shm_cpid;		//当前操作进程的ID
	pid_t shm_lpid;		//最后一个操作的进程ID,常用于分离操作
	unsigned long shm_nattch;	//绑定进程的数量
	unsigned long __pad1;	//保留扩展用
	unsigned long __pad2;
};

struct shminfo {
	unsigned long shmmax, shmmin, shmmni, shmseg, shmall, __unused[4];
};

#endif
/* private structure */
struct shmSegMap {
    vaddr_t vaddr;	//虚拟地址
    INT32 shmID;	//可看出共享内存使用了ID管理机制
};


struct shmIDSource {//共享内存描述符
    struct shmid_ds ds; //是内核为每一个共享内存段维护的数据结构,包含权限,各进程最后操作的时间,进程ID等信息
    UINT32 status;	//状态 SHM_SEG_FREE ...
    LOS_DL_LIST node; //节点,挂vmPage
};

/* private data */
STATIC struct shminfo g_shmInfo = { //描述共享内存范围的全局变量
    .shmmax = SHM_MAX,//最大的内存segment的大小 50M
    .shmmin = SHM_MIN,//最小的内存segment的大小 1M
    .shmmni = SHM_MNI,//整个系统的内存segment的总个数  :默认192     			ShmAllocSeg 
    .shmseg = SHM_SEG,//每个进程可以使用的内存segment的最大个数 128
    .shmall = SHM_ALL,//系统范围内共享内存的最大页数
};

STATIC struct shmIDSource *g_shmSegs = NULL;
//共享内存初始化
INT32 ShmInit(VOID)
{
    UINT32 ret;
    UINT32 i;

    ret = LOS_MuxInit(&g_sysvShmMux, NULL);//初始化互斥锁
    if (ret != LOS_OK) {
        return -1;
    }

    g_shmSegs = LOS_MemAlloc((VOID *)OS_SYS_MEM_ADDR, sizeof(struct shmIDSource) * g_shmInfo.shmmni);//分配shm段数组
    if (g_shmSegs == NULL) {
        (VOID)LOS_MuxDestroy(&g_sysvShmMux);
        return -1;
    }
    (VOID)memset_s(g_shmSegs, (sizeof(struct shmIDSource) * g_shmInfo.shmmni),
                   0, (sizeof(struct shmIDSource) * g_shmInfo.shmmni));//数组清零

    for (i = 0; i < g_shmInfo.shmmni; i++) {
        g_shmSegs[i].status = SHM_SEG_FREE;//节点初始状态为空闲
        g_shmSegs[i].ds.shm_perm.seq = i + 1;//struct ipc_perm shm_perm;系统为每一个IPC对象保存一个ipc_perm结构体,结构说明了IPC对象的权限和所有者
        LOS_ListInit(&g_shmSegs[i].node);//初始化节点
    }

    return 0;
}
//共享内存反初始化
INT32 ShmDeinit(VOID)
{
    UINT32 ret;

    (VOID)LOS_MemFree((VOID *)OS_SYS_MEM_ADDR, g_shmSegs);//归还内存池
    g_shmSegs = NULL;

    ret = LOS_MuxDestroy(&g_sysvShmMux);//销毁互斥量
    if (ret != LOS_OK) {
        return -1;
    }

    return 0;
}
//给共享段中所有物理页框贴上共享标签
STATIC inline VOID ShmSetSharedFlag(struct shmIDSource *seg)
{
    LosVmPage *page = NULL;

    LOS_DL_LIST_FOR_EACH_ENTRY(page, &seg->node, LosVmPage, node) {
        OsSetPageShared(page);
    }
}
//给共享段中所有物理页框撕掉共享标签
STATIC inline VOID ShmClearSharedFlag(struct shmIDSource *seg)
{
    LosVmPage *page = NULL;

    LOS_DL_LIST_FOR_EACH_ENTRY(page, &seg->node, LosVmPage, node) {
        OsCleanPageShared(page);
    }
}
//seg下所有共享页引用减少
STATIC VOID ShmPagesRefDec(struct shmIDSource *seg)
{
    LosVmPage *page = NULL;

    LOS_DL_LIST_FOR_EACH_ENTRY(page, &seg->node, LosVmPage, node) {
        LOS_AtomicDec(&page->refCounts);
    }
}

/******************************************************************************
 为共享段分配物理内存
 例如:参数size = 4097, LOS_Align(size, PAGE_SIZE) = 8192
 分配页数    size >> PAGE_SHIFT = 2页 
******************************************************************************/
STATIC INT32 ShmAllocSeg(key_t key, size_t size, int shmflg)
{
    INT32 i;
    INT32 segNum = -1;
    struct shmIDSource *seg = NULL;
    size_t count;

    if ((size == 0) || (size < g_shmInfo.shmmin) ||
        (size > g_shmInfo.shmmax)) {
        return -EINVAL;
    }
    size = LOS_Align(size, PAGE_SIZE);//必须对齐 

    for (i = 0; i < g_shmInfo.shmmni; i++) {//试图找到一个空闲段与参数key绑定
        if (g_shmSegs[i].status & SHM_SEG_FREE) {//找到空闲段
            g_shmSegs[i].status &= ~SHM_SEG_FREE;//变成非空闲状态
            segNum = i;//标号
            break;
        }
    }

    if (segNum < 0) {
        return -ENOSPC;
    }

    seg = &g_shmSegs[segNum];
    count = LOS_PhysPagesAlloc(size >> PAGE_SHIFT, &seg->node);//分配共享页面,函数内部把node都挂好了.
    if (count != (size >> PAGE_SHIFT)) {//当未分配到足够的内存时,处理方式是:不稀罕给那么点,舍弃!
        (VOID)LOS_PhysPagesFree(&seg->node);//释放节点上的物理页框
        seg->status = SHM_SEG_FREE;//共享段变回空闲状态
        return -ENOMEM;
    }
    ShmSetSharedFlag(seg);//将node的每个页面设置为共享页

    seg->status |= SHM_SEG_USED;	//共享段贴上已在使用的标签
    seg->ds.shm_perm.mode = (unsigned int)shmflg & ACCESSPERMS; //使用权限
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

    return segNum;
}
//释放seg->node 所占物理页框,seg本身重置
STATIC INLINE VOID ShmFreeSeg(struct shmIDSource *seg)
{
    UINT32 count;

    ShmClearSharedFlag(seg);//先撕掉 seg->node 中vmpage的共享标签
    count = LOS_PhysPagesFree(&seg->node);//再挨个删除物理页框
    if (count != (seg->ds.shm_segsz >> PAGE_SHIFT)) {//异常,必须要一样
        VM_ERR("free physical pages failed, count = %d, size = %d", count, seg->ds.shm_segsz >> PAGE_SHIFT);
        return;
    }

    seg->status = SHM_SEG_FREE;//seg恢复自由之身
    LOS_ListInit(&seg->node);//重置node
}
//通过key查找 shmId
STATIC INT32 ShmFindSegByKey(key_t key)
{
    INT32 i;
    struct shmIDSource *seg = NULL;

    for (i = 0; i < g_shmInfo.shmmni; i++) {//遍历共享段池,找到与key绑定的共享ID
        seg = &g_shmSegs[i];
        if ((seg->status & SHM_SEG_USED) &&
            (seg->ds.shm_perm.key == key)) {//满足两个条件,找到后返回
            return i;
        }
    }

    return -1;
}
//共享内存段有效性检查
STATIC INT32 ShmSegValidCheck(INT32 segNum, size_t size, int shmFalg)
{
    struct shmIDSource *seg = &g_shmSegs[segNum];//拿到shmID

    if (size > seg->ds.shm_segsz) {//段长
        return -EINVAL;
    }

    if ((shmFalg & (IPC_CREAT | IPC_EXCL)) ==
        (IPC_CREAT | IPC_EXCL)) {
        return -EEXIST;
    }

    return segNum;
}
//通过ID找到共享内存资源
STATIC struct shmIDSource *ShmFindSeg(int shmid)
{
    struct shmIDSource *seg = NULL;

    if ((shmid < 0) || (shmid >= g_shmInfo.shmmni)) {
        set_errno(EINVAL);
        return NULL;
    }

    seg = &g_shmSegs[shmid];
    if ((seg->status & SHM_SEG_FREE) || (seg->status & SHM_SEG_REMOVE)) {//空闲或删除时
        set_errno(EIDRM);
        return NULL;
    }

    return seg;
}
//共享内存映射
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
//fork 一个共享线性区
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
//释放共享线性区
VOID OsShmRegionFree(LosVmSpace *space, LosVmMapRegion *region)
{
    struct shmIDSource *seg = NULL;

    SYSV_SHM_LOCK();
    seg = ShmFindSeg(region->shmid);//通过线性区ID获取对应的共享资源ID结构体
    if (seg == NULL) {
        SYSV_SHM_UNLOCK();
        return;
    }

    ShmPagesRefDec(seg);//ref -- 
    seg->ds.shm_nattch--;//附在共享线性区上的进程数--
    if (seg->ds.shm_nattch <= 0) {//没有任何进程在使用这个线性区的情况
        ShmFreeSeg(seg);//就释放掉物理内存!注意是:物理内存
    }
    SYSV_SHM_UNLOCK();
}
//是否为共享线性区,是否有标签?
BOOL OsIsShmRegion(LosVmMapRegion *region)
{
    return (region->regionFlags & VM_MAP_REGION_FLAG_SHM) ? TRUE : FALSE;
}
//获取共享内存池中已被使用的段数量
STATIC INT32 ShmSegUsedCount(VOID)
{
    INT32 i;
    INT32 count = 0;
    struct shmIDSource *seg = NULL;

    for (i = 0; i < g_shmInfo.shmmni; i++) {
        seg = &g_shmSegs[i];
        if (seg->status & SHM_SEG_USED) {//找到一个
            count++;
        }
    }
    return count;
}
//对共享内存段权限检查
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

    if ((mode == SHM_M) && (tmpMode & SHM_M)) {
        return 0;
    }

    tmpMode &= ~SHM_M;
    if ((tmpMode & mode) == accMode) {
        return 0;
    } else {
        return EACCES;
    }
}
/*
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
*/
INT32 ShmGet(key_t key, size_t size, INT32 shmflg)
{
    INT32 ret;
    INT32 shmid;

    SYSV_SHM_LOCK();
    if ((((UINT32)shmflg & IPC_CREAT) == 0) && //IPC没有创建时
        (((UINT32)shmflg & IPC_EXCL) == 0)) {	//IPC
        ret = -EINVAL;
        goto ERROR;
    }

    if (key == IPC_PRIVATE) {
        ret = ShmAllocSeg(key, size, shmflg);//分配共享页
        if (ret < 0) {
            goto ERROR;
        }
    } else {
        ret = ShmFindSegByKey(key);//通过key查找shmId
        if (ret < 0) {
            if (((unsigned int)shmflg & IPC_CREAT) == 0) {
                ret = -ENOENT;
                goto ERROR;
            } else {
                ret = ShmAllocSeg(key, size, shmflg);
            }
        } else {
            shmid = ret;
            ret = ShmPermCheck(ShmFindSeg(shmid), (unsigned int)shmflg & ACCESSPERMS);
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

INT32 ShmatParamCheck(const void *shmaddr, int shmflg)
{
    if ((shmflg & SHM_REMAP) && (shmaddr == NULL)) {
        return EINVAL;
    }

    if ((shmaddr != NULL) && !IS_PAGE_ALIGNED(shmaddr) &&
        ((shmflg & SHM_RND) == 0)) {//取整 SHM_RND 
        return EINVAL;
    }

    return 0;
}
//分配一个共享线性区
LosVmMapRegion *ShmatVmmAlloc(struct shmIDSource *seg, const VOID *shmaddr,
                              INT32 shmflg, UINT32 prot)
{
    LosVmSpace *space = OsCurrProcessGet()->vmSpace;
    LosVmMapRegion *region = NULL;
    VADDR_T vaddr;
    UINT32 regionFlags;
    INT32 ret;

    regionFlags = OsCvtProtFlagsToRegionFlags(prot, MAP_ANONYMOUS | MAP_SHARED);//映射方式:共享或匿名
    (VOID)LOS_MuxAcquire(&space->regionMux);
    if (shmaddr == NULL) {//shm_segsz:段的大小（以字节为单位）
        region = LOS_RegionAlloc(space, 0, seg->ds.shm_segsz, regionFlags, 0);//分配一个区,注意这里的虚拟地址传的是0,
    } else {//如果地址虚拟地址传入是0，则由内核选择创建映射的虚拟地址，    这是创建新映射的最便捷的方法。
        if (shmflg & SHM_RND) {
            vaddr = ROUNDDOWN((VADDR_T)(UINTPTR)shmaddr, SHMLBA);
        } else {
            vaddr = (VADDR_T)(UINTPTR)shmaddr;
        }
        if ((shmflg & SHM_REMAP)) {
            vaddr = (VADDR_T)LOS_MMap(vaddr, seg->ds.shm_segsz, prot,
                                      MAP_ANONYMOUS | MAP_SHARED, -1, 0);
            region = LOS_RegionFind(space, vaddr);
        } else {
            region = LOS_RegionAlloc(space, vaddr, seg->ds.shm_segsz, regionFlags, 0);
        }
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
/**************************************************
连接共享内存标识符为shmid的共享内存，连接成功后把共享内存区对象映射到调用进程的地址空间，随后可像本地空间一样访问
一旦创建/引用了一个共享存储段，那么进程就可调用shmat函数将其连接到它的地址空间中
如果shmat成功执行，那么内核将使与该共享存储相关的shmid_ds结构中的shm_nattch计数器值加1
shmid 就是个索引,就跟进程和线程的ID一样 g_shmSegs[shmid] shmid > 192个
**************************************************/
VOID *ShmAt(INT32 shmid, const VOID *shmaddr, INT32 shmflg)
{
    INT32 ret;
    UINT32 prot = PROT_READ;
    struct shmIDSource *seg = NULL;
    LosVmMapRegion *r = NULL;
    mode_t mode;

    ret = ShmatParamCheck(shmaddr, shmflg);//参数检查
    if (ret != 0) {
        set_errno(ret);
        return (VOID *)-1;
    }

    if ((UINT32)shmflg & SHM_EXEC) {//flag 转换
        prot |= PROT_EXEC;
    } else if (((UINT32)shmflg & SHM_RDONLY) == 0) {
        prot |= PROT_WRITE;
    }

    SYSV_SHM_LOCK();
    seg = ShmFindSeg(shmid);//找到段
    if (seg == NULL) {
        SYSV_SHM_UNLOCK();
        return (VOID *)-1;
    }

    mode = ((unsigned int)shmflg & SHM_RDONLY) ? SHM_R : (SHM_R | SHM_W);//读写模式判断
    ret = ShmPermCheck(seg, mode);
    if (ret != 0) {
        goto ERROR;
    }

    seg->ds.shm_nattch++;//ds上记录有一个进程绑定上来
    r = ShmatVmmAlloc(seg, shmaddr, shmflg, prot);
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
//此函数可以对shmid指定的共享存储进行多种操作（删除、取信息、加锁、解锁等）
INT32 ShmCtl(INT32 shmid, INT32 cmd, struct shmid_ds *buf)
{
    struct shmIDSource *seg = NULL;
    INT32 ret = 0;
    struct shm_info shmInfo;
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
            ret = ShmPermCheck(seg, SHM_R);
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
            break;
        case IPC_RMID://删除共享段
            ret = ShmPermCheck(seg, SHM_M);
            if (ret != 0) {
                ret = EPERM;
                goto ERROR;
            }

            seg->status |= SHM_SEG_REMOVE;
            if (seg->ds.shm_nattch <= 0) {//没有任何进程在使用了
                ShmFreeSeg(seg);//释放 归还内存
            }
            break;
        case IPC_INFO://把内核空间的共享页数据拷贝到用户空间
            ret = LOS_ArchCopyToUser(buf, &g_shmInfo, sizeof(struct shminfo));
            if (ret != 0) {
                ret = EFAULT;
                goto ERROR;
            }
            ret = g_shmInfo.shmmni;
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
            ret = g_shmInfo.shmmni;
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

/******************************************************************************
 当对共享存储的操作已经结束时，则调用shmdt与该存储段分离
	如果shmat成功执行，那么内核将使与该共享存储相关的shmid_ds结构中的shm_nattch计数器值减1
注意：这并不从系统中删除共享存储的标识符以及其相关的数据结构。共享存储的仍然存在，
	直至某个进程带IPC_RMID命令的调用shmctl特地删除共享存储为止
******************************************************************************/
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
    /* free it */
    free(region);//释放线性区所占内存池中的内存

    SYSV_SHM_LOCK();
    seg = ShmFindSeg(shmid);//找到seg,线性区和共享段的关系是 1:N 的关系,其他空间的线性区也会绑在共享段上
    if (seg == NULL) {
        ret = EINVAL;
        SYSV_SHM_UNLOCK();
        goto ERROR_WITH_LOCK;
    }

    ShmPagesRefDec(seg);//页面引用数 --
    seg->ds.shm_nattch--;//使用共享内存的进程数少了一个
    if ((seg->ds.shm_nattch <= 0) && //无任何进程使用共享内存
        (seg->status & SHM_SEG_REMOVE)) {//状态为删除时需要释放物理页内存了,否则其他进程还要继续使用共享内存
        ShmFreeSeg(seg);//释放seg 页框链表中的页框内存,再重置seg状态
    }

    seg->ds.shm_dtime = time(NULL);//记录分离的时间
    seg->ds.shm_lpid = LOS_GetCurrProcessID();//记录操作进程ID
    SYSV_SHM_UNLOCK();
    (VOID)LOS_MuxRelease(&space->regionMux);
    return 0;

ERROR_WITH_LOCK:
    (VOID)LOS_MuxRelease(&space->regionMux);
ERROR:
    set_errno(ret);
    PRINT_DEBUG("%s %d, ret = %d\n", __FUNCTION__, __LINE__, ret);
    return -1;
}

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif
