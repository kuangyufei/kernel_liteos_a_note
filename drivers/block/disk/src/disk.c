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

#include "disk.h"
#include "stdio.h"
#include "stdlib.h"
#include "unistd.h"
#include "sys/mount.h"
#include "linux/spinlock.h"
#include "path_cache.h"

/**
 * @file disk.c
 * @verbatim
GPT和MBR是做什么的？
	在使用磁盘驱动器之前，必须对其进行分区。。MBR(主引导记录)和GPT(GUID分区表)是在驱动器上存储分区信息的两种不同方法。
	这些信息包括分区在物理磁盘上的开始和结束位置，因此您的操作系统知道哪些扇区属于每个分区，哪些分区是可引导的。
	这就是为什么在驱动器上创建分区之前必须选择MBR或GPT的原因。

	MBR于1983年首次在IBMPCDOS2.0中引入。它被称为主引导记录，因为MBR是一个特殊的引导扇区，位于驱动器的开头。
	此扇区包含已安装操作系统的引导加载程序和有关驱动器逻辑分区的信息。引导加载程序是一小部分代码，
	通常从驱动器上的另一个分区加载较大的引导加载程序。	

	GPT代表GUID划分表。这是一个正在逐步取代MBR的新标准。它与UEFI相关，UEFI用更现代的东西取代笨重的旧BIOS。
	反过来，GPT用更现代的东西取代了笨重的旧MBR分区系统。它被称为GUIDPartitionTable【全局唯一标识磁盘分区表】，
	因为驱动器上的每个分区都有一个“全局唯一标识符”，或者GUID--一个长时间的随机字符串，
	以至于地球上的每个GPT分区都有自己的唯一标识符。

	在MBR磁盘上，分区和引导数据存储在一个地方。如果该数据被覆盖或损坏，您就有麻烦了。
	相反，GPT跨磁盘存储此数据的多个副本，因此它更健壮，如果数据损坏，可以恢复。

	GPT还存储循环冗余校验(CRC)值，以检查其数据是否完整。如果数据损坏，GPT会注意到问题，
	并尝试从磁盘上的另一个位置恢复损坏的数据。MBR无法知道其数据是否已损坏--只有在引导进程失败
	或驱动器分区消失时才会看到问题。
	GUID分区表自带备份。在磁盘的首尾部分分别保存了一份相同的分区表，因此当一份损坏可以通过另一份恢复。
	而MBR磁盘分区表一旦被破坏就无法恢复，需要重新分区
    https://www.howtogeek.com/193669/whats-the-difference-between-gpt-and-mbr-when-partitioning-a-drive/
    https://www.html.cn/qa/other/20741.html
 * @endverbatim 
 * @brief 
 */

//磁盘的最小单位就是扇区
los_disk g_sysDisk[SYS_MAX_DISK]; ///< 支持挂载的磁盘总数量 5个
los_part g_sysPart[SYS_MAX_PART]; ///< 支持磁盘的分区总数量 5*16,每个磁盘最大分16个区

UINT32 g_uwFatSectorsPerBlock = CONFIG_FS_FAT_SECTOR_PER_BLOCK; ///< 每块支持扇区数 默认64个扇区
UINT32 g_uwFatBlockNums = CONFIG_FS_FAT_BLOCK_NUMS; ///< 块数量 默认28

spinlock_t g_diskSpinlock; ///< 磁盘自锁锁
spinlock_t g_diskFatBlockSpinlock; ///< 磁盘Fat块自旋锁

UINT32 g_usbMode = 0;

#define MEM_ADDR_ALIGN_BYTE  64
#define RWE_RW_RW            0755

#define DISK_LOCK(mux) do {                                              \
    if (pthread_mutex_lock(mux) != 0) {                                  \
        PRINT_ERR("%s %d, mutex lock failed\n", __FUNCTION__, __LINE__); \
    }                                                                    \
} while (0)

#define DISK_UNLOCK(mux) do {                                              \
    if (pthread_mutex_unlock(mux) != 0) {                                  \
        PRINT_ERR("%s %d, mutex unlock failed\n", __FUNCTION__, __LINE__); \
    }                                                                      \
} while (0)

typedef VOID *(*StorageHookFunction)(VOID *);

#ifdef LOSCFG_FS_FAT_CACHE
static UINT32 OsReHookFuncAddDiskRef(StorageHookFunction handler,
                                     VOID *param) __attribute__((weakref("osReHookFuncAdd")));

static UINT32 OsReHookFuncDelDiskRef(StorageHookFunction handler) __attribute__((weakref("osReHookFuncDel")));

UINT32 GetFatBlockNums(VOID)
{
    return g_uwFatBlockNums;
}

VOID SetFatBlockNums(UINT32 blockNums)
{
    g_uwFatBlockNums = blockNums;
}

UINT32 GetFatSectorsPerBlock(VOID)
{
    return g_uwFatSectorsPerBlock;
}
///设置FAR每块扇区数
VOID SetFatSectorsPerBlock(UINT32 sectorsPerBlock)
{
    if (((sectorsPerBlock % UNSIGNED_INTEGER_BITS) == 0) && //1.必须是整数倍
        ((sectorsPerBlock >> UNINT_LOG2_SHIFT) <= BCACHE_BLOCK_FLAGS)) {
        g_uwFatSectorsPerBlock = sectorsPerBlock;
    }
}
#endif
///通过名称分配磁盘ID
INT32 los_alloc_diskid_byname(const CHAR *diskName)
{
    INT32 diskID;
    los_disk *disk = NULL;
    UINT32 intSave;
    size_t nameLen;

    if (diskName == NULL) {
        PRINT_ERR("The paramter disk_name is NULL");
        return VFS_ERROR;
    }

    nameLen = strlen(diskName);
    if (nameLen > DISK_NAME) {
        PRINT_ERR("diskName is too long!\n");
        return VFS_ERROR;
    }
    spin_lock_irqsave(&g_diskSpinlock, intSave);//加锁

    for (diskID = 0; diskID < SYS_MAX_DISK; diskID++) {//磁盘池中分配一个磁盘描述符
        disk = get_disk(diskID);
        if ((disk != NULL) && (disk->disk_status == STAT_UNUSED)) {
            disk->disk_status = STAT_UNREADY;//初始状态,可理解为未格式化
            break;
        }
    }

    spin_unlock_irqrestore(&g_diskSpinlock, intSave);

    if ((disk == NULL) || (diskID == SYS_MAX_DISK)) {
        PRINT_ERR("los_alloc_diskid_byname failed %d!\n", diskID);
        return VFS_ERROR;
    }

    if (disk->disk_name != NULL) {//分配的磁盘如果有名称,可能之前用过,先释放内核内存
        LOS_MemFree(m_aucSysMem0, disk->disk_name);
        disk->disk_name = NULL;
    }

    disk->disk_name = LOS_MemAlloc(m_aucSysMem0, (nameLen + 1));
    if (disk->disk_name == NULL) {
        PRINT_ERR("los_alloc_diskid_byname alloc disk name failed\n");
        return VFS_ERROR;
    }
	//disk->disk_name 等于 参数 名称
    if (strncpy_s(disk->disk_name, (nameLen + 1), diskName, nameLen) != EOK) {
        PRINT_ERR("The strncpy_s failed.\n");
        LOS_MemFree(m_aucSysMem0, disk->disk_name);
        disk->disk_name = NULL;
        return VFS_ERROR;
    }

    disk->disk_name[nameLen] = '\0';

    return diskID;
}
///通过名称查询使用中(STAT_INUSED)磁盘ID
INT32 los_get_diskid_byname(const CHAR *diskName)
{
    INT32 diskID;
    los_disk *disk = NULL;
    size_t diskNameLen;

    if (diskName == NULL) {
        PRINT_ERR("The paramter diskName is NULL");
        return VFS_ERROR;
    }

    diskNameLen = strlen(diskName);
    if (diskNameLen > DISK_NAME) {
        PRINT_ERR("diskName is too long!\n");
        return VFS_ERROR;
    }

    for (diskID = 0; diskID < SYS_MAX_DISK; diskID++) {
        disk = get_disk(diskID);
        if ((disk != NULL) && (disk->disk_name != NULL) && (disk->disk_status == STAT_INUSED)) {
            if (strlen(disk->disk_name) != diskNameLen) {
                continue;
            }
            if (strcmp(diskName, disk->disk_name) == 0) {
                break;
            }
        }
    }
    if ((disk == NULL) || (diskID == SYS_MAX_DISK)) {
        PRINT_ERR("los_get_diskid_byname failed!\n");
        return VFS_ERROR;
    }
    return diskID;
}

los_disk *los_get_mmcdisk_bytype(UINT8 type)
{
    const CHAR *mmcDevHead = "/dev/mmcblk";

    for (INT32 diskId = 0; diskId < SYS_MAX_DISK; diskId++) {
        los_disk *disk = get_disk(diskId);
        if (disk == NULL) {
            continue;
        } else if ((disk->type == type) && (strncmp(disk->disk_name, mmcDevHead, strlen(mmcDevHead)) == 0)) {
            return disk;
        }
    }
    PRINT_ERR("Cannot find the mmc disk!\n");
    return NULL;
}

VOID OsSetUsbStatus(UINT32 diskID)
{
    if (diskID < SYS_MAX_DISK) {
        g_usbMode |= (1u << diskID) & UINT_MAX;
    }
}

VOID OsClearUsbStatus(UINT32 diskID)
{
    if (diskID < SYS_MAX_DISK) {
        g_usbMode &= ~((1u << diskID) & UINT_MAX);
    }
}

#ifdef LOSCFG_FS_FAT_CACHE
static BOOL GetDiskUsbStatus(UINT32 diskID)
{
    return (g_usbMode & (1u << diskID)) ? TRUE : FALSE;
}
#endif
//获取某个磁盘的描述符
los_disk *get_disk(INT32 id)
{
    if ((id >= 0) && (id < SYS_MAX_DISK)) {
        return &g_sysDisk[id];
    }

    return NULL;
}
///获取某个分区的描述符
los_part *get_part(INT32 id)
{
    if ((id >= 0) && (id < SYS_MAX_PART)) {
        return &g_sysPart[id];
    }

    return NULL;
}

static UINT64 GetFirstPartStart(const los_part *part)
{
    los_part *firstPart = NULL;
    los_disk *disk = get_disk((INT32)part->disk_id);
    firstPart = (disk == NULL) ? NULL : LOS_DL_LIST_ENTRY(disk->head.pstNext, los_part, list);
    return (firstPart == NULL) ? 0 : firstPart->sector_start;
}
///磁盘增加一个分区
static VOID DiskPartAddToDisk(los_disk *disk, los_part *part)
{
    part->disk_id = disk->disk_id;
    part->part_no_disk = disk->part_count;
    LOS_ListTailInsert(&disk->head, &part->list);
    disk->part_count++;
}
///从磁盘上删除分区
static VOID DiskPartDelFromDisk(los_disk *disk, los_part *part)
{
    LOS_ListDelete(&part->list);//从分区链表上摘掉
    disk->part_count--;//分区数量减少
}
///分配一个磁盘分区,必须要有索引节点才给分配.
static los_part *DiskPartAllocate(struct Vnode *dev, UINT64 start, UINT64 count)
{
    UINT32 i;//从数组的开头遍历,先获取第一个分区
    los_part *part = get_part(0); /* traversing from the beginning of the array */

    if (part == NULL) {
        return NULL;
    }

    for (i = 0; i < SYS_MAX_PART; i++) {//从数组开头开始遍历
        if (part->dev == NULL) {//分区是否被使用,通过dev来判断.
            part->part_id = i;	//分区ID
            part->part_no_mbr = 0;	//主分区一般为0
            part->dev = dev;	//设置索引节点,所以节点这个概念很重要,贯彻了整个文件/磁盘系统,一定要理解透彻!!!
            part->sector_start = start;//开始扇区
            part->sector_count = count;//扇区总大小
            part->part_name = NULL;	   //分区名称,暂无
            LOS_ListInit(&part->list);	//初始化链表节点,将通过它挂入磁盘分区链表上(los_disk->head).

            return part; //就这个分区了.
        }
        part++;//找下一个空闲分区位
    }

    return NULL;
}
///清空分区信息
static VOID DiskPartRelease(los_part *part)
{
    part->dev = NULL;//全局释放这步最关键,因为 DiskPartAllocate 中是通过它来判断是否全局分区项已被占用
    part->part_no_disk = 0; //复位归0
    part->part_no_mbr = 0;	//复位归0
    if (part->part_name != NULL) {
        free(part->part_name);//分区名称是内核内存分配的,所以要单独释放.
        part->part_name = NULL;
    }
}

/*
 * name is a combination of disk_name, 'p' and part_count, such as "/dev/mmcblk0p0"
 * disk_name : DISK_NAME + 1
 * 'p' : 1
 * part_count: 1
 */
#define DEV_NAME_BUFF_SIZE  (DISK_NAME + 3)//为何加3, 就是上面的"disk_name"+"p"+"part_count"
//磁盘增加一个分区
static INT32 DiskAddPart(los_disk *disk, UINT64 sectorStart, UINT64 sectorCount, BOOL IsValidPart)
{
    CHAR devName[DEV_NAME_BUFF_SIZE];
    struct Vnode *diskDev = NULL;
    struct Vnode *partDev = NULL;
    los_part *part = NULL;
    INT32 ret;

    if ((disk == NULL) || (disk->disk_status == STAT_UNUSED) ||
        (disk->dev == NULL)) {
        return VFS_ERROR;
    }
	//扇区判断,磁盘在创建伊始就扇区数量就固定了
    if ((sectorCount > disk->sector_count) || ((disk->sector_count - sectorCount) < sectorStart)) {
        PRINT_ERR("DiskAddPart failed: sector start is %llu, sector count is %llu\n", sectorStart, sectorCount);
        return VFS_ERROR;
    }
	//devName = /dev/mmcblk0p2 代表的是 0号磁盘的2号分区

    diskDev = disk->dev;
    if (IsValidPart == TRUE) {//有效分区处理
        ret = snprintf_s(devName, sizeof(devName), sizeof(devName) - 1, "%s%c%u",
                         ((disk->disk_name == NULL) ? "null" : disk->disk_name), 'p', disk->part_count);
        if (ret < 0) {
            return VFS_ERROR;
        }
		//为节点注册块设备驱动,目的是从此可以通过块操作访问私有数据
        if (register_blockdriver(devName, ((struct drv_data *)diskDev->data)->ops,
                                 RWE_RW_RW, ((struct drv_data *)diskDev->data)->priv)) {
            PRINT_ERR("DiskAddPart : register %s fail!\n", devName);
            return VFS_ERROR;
        }

        VnodeHold();
        VnodeLookup(devName, &partDev, 0);//通过路径找到索引节点

        part = DiskPartAllocate(partDev, sectorStart, sectorCount);//从全局分区池中分配分区项
        VnodeDrop();
        if (part == NULL) {
            (VOID)unregister_blockdriver(devName);
            return VFS_ERROR;
        }
    } else { //无效的分区也分配一个? @note_why 
        part = DiskPartAllocate(diskDev, sectorStart, sectorCount);
        if (part == NULL) {
            return VFS_ERROR;
        }
    }

    DiskPartAddToDisk(disk, part);
    if (disk->type == EMMC) {
        part->type = EMMC;
    }
    return (INT32)part->part_id;
}
///磁盘分区
static INT32 DiskDivide(los_disk *disk, struct disk_divide_info *info)
{
    UINT32 i;
    INT32 ret;

    disk->type = info->part[0].type;// 类型一般为 EMMC
    for (i = 0; i < info->part_count; i++) {//遍历分区列表
        if (info->sector_count < info->part[i].sector_start) {//总扇区都已经被分完
            return VFS_ERROR;
        }
        if (info->part[i].sector_count > (info->sector_count - info->part[i].sector_start)) {//边界检测
            PRINT_ERR("Part[%u] sector_start:%llu, sector_count:%llu, exceed emmc sector_count:%llu.\n", i,
                      info->part[i].sector_start, info->part[i].sector_count,
                      (info->sector_count - info->part[i].sector_start));
            info->part[i].sector_count = info->sector_count - info->part[i].sector_start;//改变区的扇区数量
            PRINT_ERR("Part[%u] sector_count change to %llu.\n", i, info->part[i].sector_count);
			//增加一个分区
            ret = DiskAddPart(disk, info->part[i].sector_start, info->part[i].sector_count, TRUE);
            if (ret == VFS_ERROR) {
                return VFS_ERROR;
            }
            break;
        }
        ret = DiskAddPart(disk, info->part[i].sector_start, info->part[i].sector_count, TRUE);
        if (ret == VFS_ERROR) {
            return VFS_ERROR;
        }
    }

    return ENOERR;
}
///GPT分区类型识别
static CHAR GPTPartitionTypeRecognition(const CHAR *parBuf)
{
    const CHAR *buf = parBuf;
    const CHAR *fsType = "FAT";
    const CHAR *str = "\xEB\x52\x90" "NTFS    "; /* NTFS Boot entry point | NTFS 引导入口点*/

    if (((LD_DWORD_DISK(&buf[BS_FILSYSTEMTYPE32]) & BS_FS_TYPE_MASK) == BS_FS_TYPE_VALUE) ||
        (strncmp(&buf[BS_FILSYSTYPE], fsType, strlen(fsType)) == 0)) {//识别FAT的方法
        return BS_FS_TYPE_FAT;
    } else if (strncmp(&buf[BS_JMPBOOT], str, strlen(str)) == 0) {//识别NTFS的方法
        return BS_FS_TYPE_NTFS;
    }

    return ENOERR;
}
///磁盘分区内存申请
static INT32 DiskPartitionMemZalloc(size_t boundary, size_t size, CHAR **gptBuf, CHAR **partitionBuf)
{
    CHAR *buffer1 = NULL;
    CHAR *buffer2 = NULL;
	//分配一个 size 字节的块，其地址是边界的倍数。边界必须是 2 的幂！
    buffer1 = (CHAR *)memalign(boundary, size);//对齐 memalign -> LOS_KernelMallocAlign
    if (buffer1 == NULL) {
        PRINT_ERR("%s buffer1 malloc %lu failed! %d\n", __FUNCTION__, size, __LINE__);
        return -ENOMEM;
    }
    buffer2 = (CHAR *)memalign(boundary, size);
    if (buffer2 == NULL) {
        PRINT_ERR("%s buffer2 malloc %lu failed! %d\n", __FUNCTION__, size, __LINE__);
        free(buffer1);
        return -ENOMEM;
    }
    (VOID)memset_s(buffer1, size, 0, size);
    (VOID)memset_s(buffer2, size, 0, size);

    *gptBuf = buffer1;
    *partitionBuf = buffer2;

    return ENOERR;
}
///获取GPT信息,是GPT还是MBR取决于磁盘上放的内容
static INT32 GPTInfoGet(struct Vnode *blkDrv, CHAR *gptBuf)
{
    INT32 ret;
	//获取驱动程序
    struct block_operations *bops = (struct block_operations *)((struct drv_data *)blkDrv->data)->ops;
	//从第一个扇区,从这可以看出 GPT是兼容 MBR的,因为 MBR的内容是放在 0 扇区.
    ret = bops->read(blkDrv, (UINT8 *)gptBuf, 1, 1); /* Read the device first sector */
    if (ret != 1) { /* Read failed */
        PRINT_ERR("%s %d\n", __FUNCTION__, __LINE__);
        return -EIO;
    }

    if (!VERIFY_GPT(gptBuf)) {
        PRINT_ERR("%s %d\n", __FUNCTION__, __LINE__);
        return VFS_ERROR;
    }

    return ENOERR;
}
///解析GPT分区内容
static INT32 OsGPTPartitionRecognitionSub(struct disk_divide_info *info, const CHAR *partitionBuf,
                                          UINT32 *partitionCount, UINT64 partitionStart, UINT64 partitionEnd)
{
    CHAR partitionType;

    if (VERIFY_FS(partitionBuf)) {
        partitionType = GPTPartitionTypeRecognition(partitionBuf);//文件系统类型,支持FAT和NTFS
        if (partitionType) {
            if (*partitionCount >= MAX_DIVIDE_PART_PER_DISK) {
                return VFS_ERROR;
            }
            info->part[*partitionCount].type = partitionType;//文件系统
            info->part[*partitionCount].sector_start = partitionStart;//开始扇区
            info->part[*partitionCount].sector_count = (partitionEnd - partitionStart) + 1;//扇区总数
            (*partitionCount)++;//磁盘扇区数量增加
        } else {
            PRINT_ERR("The partition type is not allowed to use!\n");
        }
    } else {
        PRINT_ERR("Do not support the partition type!\n");
    }
    return ENOERR;
}
///识别GPT分区内容
static INT32 OsGPTPartitionRecognition(struct Vnode *blkDrv, struct disk_divide_info *info,
                                       const CHAR *gptBuf, CHAR *partitionBuf, UINT32 *partitionCount)
{
    UINT32 j;
    INT32 ret = VFS_ERROR;
    UINT64 partitionStart, partitionEnd;
    struct block_operations *bops = NULL;

    for (j = 0; j < PAR_ENTRY_NUM_PER_SECTOR; j++) {
        if (!VERITY_AVAILABLE_PAR(&gptBuf[j * TABLE_SIZE])) {//ESP 和 MSR 分区
            PRINTK("The partition type is ESP or MSR!\n");
            continue;
        }

        if (!VERITY_PAR_VALID(&gptBuf[j * TABLE_SIZE])) {
            return VFS_ERROR;
        }

        partitionStart = LD_QWORD_DISK(&gptBuf[(j * TABLE_SIZE) + GPT_PAR_START_OFFSET]);//读取开始扇区
        partitionEnd = LD_QWORD_DISK(&gptBuf[(j * TABLE_SIZE) + GPT_PAR_END_OFFSET]);//读取结束扇区
        if ((partitionStart >= partitionEnd) || (partitionEnd > info->sector_count)) {
            PRINT_ERR("GPT partition %u recognition failed : partitionStart = %llu, partitionEnd = %llu\n",
                      j, partitionStart, partitionEnd);
            return VFS_ERROR;
        }

        (VOID)memset_s(partitionBuf, info->sector_size, 0, info->sector_size);

        bops = (struct block_operations *)((struct drv_data *)blkDrv->data)->ops;

        ret = bops->read(blkDrv, (UINT8 *)partitionBuf, partitionStart, 1);//读取扇区内容
        if (ret != 1) { /* read failed */
            PRINT_ERR("%s %d\n", __FUNCTION__, __LINE__);
            return -EIO;
        }

        ret = OsGPTPartitionRecognitionSub(info, partitionBuf, partitionCount, partitionStart, partitionEnd);
        if (ret != ENOERR) {
            return VFS_ERROR;
        }
    }

    return ret;
}

static INT32 DiskGPTPartitionRecognition(struct Vnode *blkDrv, struct disk_divide_info *info)
{
    CHAR *gptBuf = NULL;
    CHAR *partitionBuf = NULL;
    UINT32 tableNum, i, index;
    UINT32 partitionCount = 0;
    INT32 ret;

    ret = DiskPartitionMemZalloc(MEM_ADDR_ALIGN_BYTE, info->sector_size, &gptBuf, &partitionBuf);
    if (ret != ENOERR) {
        return ret;
    }

    ret = GPTInfoGet(blkDrv, gptBuf);//获取GDT类型的引导扇区信息
    if (ret < 0) {
        goto OUT_WITH_MEM;
    }

    tableNum = LD_DWORD_DISK(&gptBuf[TABLE_NUM_OFFSET]);
    if (tableNum > TABLE_MAX_NUM) {
        tableNum = TABLE_MAX_NUM;
    }

    index = (tableNum % PAR_ENTRY_NUM_PER_SECTOR) ? ((tableNum / PAR_ENTRY_NUM_PER_SECTOR) + 1) :
            (tableNum / PAR_ENTRY_NUM_PER_SECTOR);

    for (i = 0; i < index; i++) {
        (VOID)memset_s(gptBuf, info->sector_size, 0, info->sector_size);
        struct block_operations *bops = (struct block_operations *)((struct drv_data *)blkDrv->data)->ops;
        ret = bops->read(blkDrv, (UINT8 *)gptBuf, TABLE_START_SECTOR + i, 1);
        if (ret != 1) { /* read failed */
            PRINT_ERR("%s %d\n", __FUNCTION__, __LINE__);
            ret = -EIO;
            goto OUT_WITH_MEM;
        }

        ret = OsGPTPartitionRecognition(blkDrv, info, gptBuf, partitionBuf, &partitionCount);
        if (ret < 0) {
            if (ret == VFS_ERROR) {
                ret = (INT32)partitionCount;
            }
            goto OUT_WITH_MEM;
        }
    }
    ret = (INT32)partitionCount;

OUT_WITH_MEM:
    free(gptBuf);
    free(partitionBuf);
    return ret;
}
///获取MBR信息,即特殊扇区信息,在MBR硬盘中，分区信息直接存储于主引导记录(MBR)中（主引导记录中还存储着系统的引导程序）。
static INT32 OsMBRInfoGet(struct Vnode *blkDrv, CHAR *mbrBuf)
{
    INT32 ret;
	//读取MBR，从0扇区开始，长度为1扇区
    /* read MBR, start from sector 0, length is 1 sector */
	//以块为操作的接口封装
    struct block_operations *bops = (struct block_operations *)((struct drv_data *)blkDrv->data)->ops;

    ret = bops->read(blkDrv, (UINT8 *)mbrBuf, 0, 1);//读一个扇区 [0-1]
    if (ret != 1) { /* read failed */
        PRINT_ERR("driver read return error: %d\n", ret);
        return -EIO;
    }

    /* Check boot record signature. *///检查引导记录签名
    if (LD_WORD_DISK(&mbrBuf[BS_SIG55AA]) != BS_SIG55AA_VALUE) {//高低换位
        return VFS_ERROR;
    }

    return ENOERR;
}

static INT32 OsEBRInfoGet(struct Vnode *blkDrv, const struct disk_divide_info *info,
                          CHAR *ebrBuf, const CHAR *mbrBuf)
{
    INT32 ret;

    if (VERIFY_FS(mbrBuf)) {//确认是个文件系统的引导扇区,因为紧接着要去读这个扇区内容,所以先判断有没有这个扇区.
        if (info->sector_count <= LD_DWORD_DISK(&mbrBuf[PAR_OFFSET + PAR_START_OFFSET])) {
            return VFS_ERROR;
        }
		//获取块设备驱动程序
        struct block_operations *bops = (struct block_operations *)((struct drv_data *)blkDrv->data)->ops;
        ret = bops->read(blkDrv, (UINT8 *)ebrBuf, LD_DWORD_DISK(&mbrBuf[PAR_OFFSET + PAR_START_OFFSET]), 1);//读取某个扇区信息
        if ((ret != 1) || (!VERIFY_FS(ebrBuf))) { /* read failed */ //这里同样要确认下读出来的扇区是文件系统的引导扇区
            PRINT_ERR("OsEBRInfoGet, verify_fs error, ret = %d\n", ret);
            return -EIO;
        }
    }

    return ENOERR;
}
///识别主分区
static INT32 OsPrimaryPartitionRecognition(const CHAR *mbrBuf, struct disk_divide_info *info,
                                           INT32 *extendedPos, INT32 *mbrCount)
{
    INT32 i;
    CHAR mbrPartitionType;
    INT32 extendedFlag = 0;
    INT32 count = 0;
	//mbrBuf的内容是磁盘的第0柱面第0磁盘的首个扇区内容,也称为特殊扇区
    for (i = 0; i < MAX_PRIMARY_PART_PER_DISK; i++) {//最多4个主分区,这是由MBR的第二部分DPT（Disk Partition Table，硬盘分区表）规定死的
        mbrPartitionType = mbrBuf[PAR_OFFSET + PAR_TYPE_OFFSET + (i * PAR_TABLE_SIZE)];//读取分区类型
        if (mbrPartitionType) {
            info->part[i].type = mbrPartitionType;//记录分区
            info->part[i].sector_start = LD_DWORD_DISK(&mbrBuf[PAR_OFFSET + PAR_START_OFFSET + (i * PAR_TABLE_SIZE)]);//读取开始扇区编号
            info->part[i].sector_count = LD_DWORD_DISK(&mbrBuf[PAR_OFFSET + PAR_COUNT_OFFSET + (i * PAR_TABLE_SIZE)]);//读取共有多少扇区
            if ((mbrPartitionType == EXTENDED_PAR) || (mbrPartitionType == EXTENDED_8G)) {//分区类型如果是扩展分区
                extendedFlag = 1;//贴上硬盘分区表有扩展分区
                *extendedPos = i;//记录第几个位置是扩展分区
                continue;//继续,说明 count 记录的是主分区的数量
            }
            count++;
        }
    }
    *mbrCount = count;//带出主分区数量

    return extendedFlag;//返回是否为扩展分区标签
}
///识别扩展分区
static INT32 OsLogicalPartitionRecognition(struct Vnode *blkDrv, struct disk_divide_info *info,
                                           UINT32 extendedAddress, CHAR *ebrBuf, INT32 mbrCount)
{
    INT32 ret;
    UINT32 extendedOffset = 0;
    CHAR ebrPartitionType;
    INT32 ebrCount = 0;

    do {//循环读取一个个扩展分区信息
        (VOID)memset_s(ebrBuf, info->sector_size, 0, info->sector_size);
        if (((UINT64)(extendedAddress) + extendedOffset) >= info->sector_count) {
            PRINT_ERR("extended partition is out of disk range: extendedAddress = %u, extendedOffset = %u\n",
                      extendedAddress, extendedOffset);
            break;
        }
        struct block_operations *bops = (struct block_operations *)((struct drv_data *)blkDrv->data)->ops;
        ret = bops->read(blkDrv, (UINT8 *)ebrBuf, extendedAddress + extendedOffset, 1);//读取扇区
        if (ret != 1) { /* read failed */
            PRINT_ERR("driver read return error: %d, extendedAddress = %u, extendedOffset = %u\n", ret,
                      extendedAddress, extendedOffset);
            return -EIO;
        }
        ebrPartitionType = ebrBuf[PAR_OFFSET + PAR_TYPE_OFFSET];//读取分区类型
        if (ebrPartitionType && ((mbrCount + ebrCount) < MAX_DIVIDE_PART_PER_DISK)) {//填充分区信息
            info->part[MAX_PRIMARY_PART_PER_DISK + ebrCount].type = ebrPartitionType;
            info->part[MAX_PRIMARY_PART_PER_DISK + ebrCount].sector_start = extendedAddress + extendedOffset +
                                                                            LD_DWORD_DISK(&ebrBuf[PAR_OFFSET +
                                                                                                  PAR_START_OFFSET]);
            info->part[MAX_PRIMARY_PART_PER_DISK + ebrCount].sector_count = LD_DWORD_DISK(&ebrBuf[PAR_OFFSET +
                                                                                                  PAR_COUNT_OFFSET]);
            ebrCount++;//扩展分区的数量
        }
        extendedOffset = LD_DWORD_DISK(&ebrBuf[PAR_OFFSET + PAR_START_OFFSET + PAR_TABLE_SIZE]);//继续下一个分区
    } while ((ebrBuf[PAR_OFFSET + PAR_TYPE_OFFSET + PAR_TABLE_SIZE] != 0) &&
             ((mbrCount + ebrCount) < MAX_DIVIDE_PART_PER_DISK));//结束条件1.读完分区表 2.总分区数超了 

    return ebrCount;
}
///识别磁盘分区信息 https://blog.csdn.net/Hilavergil/article/details/79270379
static INT32 DiskPartitionRecognition(struct Vnode *blkDrv, struct disk_divide_info *info)
{
    INT32 ret;
    INT32 extendedFlag;
    INT32 extendedPos = 0;
    INT32 mbrCount = 0;
    UINT32 extendedAddress;
    CHAR *mbrBuf = NULL;
    CHAR *ebrBuf = NULL;

    if (blkDrv == NULL) {
        return -EINVAL;
    }
	//获取块设备驱动程序
    struct block_operations *bops = (struct block_operations *)((struct drv_data *)blkDrv->data)->ops;

    if ((bops == NULL) || (bops->read == NULL)) {
        return -EINVAL;
    }

    ret = DiskPartitionMemZalloc(MEM_ADDR_ALIGN_BYTE, info->sector_size, &mbrBuf, &ebrBuf);
    if (ret != ENOERR) {
        return ret;
    }

    ret = OsMBRInfoGet(blkDrv, mbrBuf);//获取(MBR)首个扇区内容,
    if (ret < 0) {
        goto OUT_WITH_MEM;
    }

    /* The partition type is GPT */
    if (mbrBuf[PARTION_MODE_BTYE] == (CHAR)PARTION_MODE_GPT) {//如果采用了GPT的分区方式 0xEE
        ret = DiskGPTPartitionRecognition(blkDrv, info);//用GPT方式解析分区信息
        goto OUT_WITH_MEM;
    }

    ret = OsEBRInfoGet(blkDrv, info, ebrBuf, mbrBuf);//获取扩展分区信息
    if (ret < 0) {//4个分区表中不一定会有扩展分区
        ret = 0; /* no mbr */
        goto OUT_WITH_MEM;
    }

    extendedFlag = OsPrimaryPartitionRecognition(mbrBuf, info, &extendedPos, &mbrCount);//解析主分区信息
    if (extendedFlag) {//如果有扩展分区
        extendedAddress = LD_DWORD_DISK(&mbrBuf[PAR_OFFSET + PAR_START_OFFSET + (extendedPos * PAR_TABLE_SIZE)]);//逻辑分区开始解析位置
        ret = OsLogicalPartitionRecognition(blkDrv, info, extendedAddress, ebrBuf, mbrCount);//解析逻辑分区
        if (ret <= 0) {
            goto OUT_WITH_MEM;
        }
    }
    ret += mbrCount;

OUT_WITH_MEM:
    free(ebrBuf);
    free(mbrBuf);
    return ret;
}
///磁盘分区注册
INT32 DiskPartitionRegister(los_disk *disk)
{
    INT32 count;
    UINT32 i, partSize;
    los_part *part = NULL;
    struct disk_divide_info parInfo;
	//填充disk_divide_info结构体设置分区信息
    /* Fill disk_divide_info structure to set partition's infomation. */
    (VOID)memset_s(parInfo.part, sizeof(parInfo.part), 0, sizeof(parInfo.part));
    partSize = sizeof(parInfo.part) / sizeof(parInfo.part[0]);//获取能分区的最大数量
    parInfo.sector_size = disk->sector_size;//磁盘扇区大小,这是固定的,不管什么分区,扇区大小是不变的(512字节)
    parInfo.sector_count = disk->sector_count;//扇区数量,注意:这里只是默认的值,最终的值来源于读取的MBR或EBR内容
    //即每个分区的扇区大小和总数量是可以不一样的.
    count = DiskPartitionRecognition(disk->dev, &parInfo);//有了分区信息就阔以开始解析了
    if (count == VFS_ERROR) {//分区表错误, @note_thinking 直接返回就行了,做 get_part 没意义了吧
        part = get_part(DiskAddPart(disk, 0, disk->sector_count, FALSE));//
        if (part == NULL) {
            return VFS_ERROR;
        }
        part->part_no_mbr = 0;//打印磁盘没有一个可用的分区表
        PRINTK("Disk %s doesn't contain a valid partition table.\n", disk->disk_name);
        return ENOERR;
    } else if (count < 0) {
        return VFS_ERROR;
    }

    parInfo.part_count = count;//保存分区数量
    if (count == 0) { //整个函数写的很怪,建议重写 @note_thinking
        part = get_part(DiskAddPart(disk, 0, disk->sector_count, TRUE));
        if (part == NULL) {
            return VFS_ERROR;
        }
        part->part_no_mbr = 0;

        PRINTK("No MBR detected.\n");
        return ENOERR;
    }

    for (i = 0; i < partSize; i++) {//磁盘的分区是解析出来了,接着要纳入全局统一管理了.
        /* Read the disk_divide_info structure to get partition's infomation. */
        if ((parInfo.part[i].type != 0) && (parInfo.part[i].type != EXTENDED_PAR) &&
            (parInfo.part[i].type != EXTENDED_8G)) {
            part = get_part(DiskAddPart(disk, parInfo.part[i].sector_start, parInfo.part[i].sector_count, TRUE));
            if (part == NULL) {
                return VFS_ERROR;
            }
            part->part_no_mbr = i + 1;//全局记录 第几个主引导分区
            part->filesystem_type = parInfo.part[i].type;//全局记录 文件系统类型
        }
    }

    return ENOERR;
}
///读磁盘数据
INT32 los_disk_read(INT32 drvID, VOID *buf, UINT64 sector, UINT32 count, BOOL useRead)
{
#ifdef LOSCFG_FS_FAT_CACHE
    UINT32 len;
#endif
    INT32 result = VFS_ERROR;
    los_disk *disk = get_disk(drvID);

    if ((buf == NULL) || (count == 0)) { /* buff equal to NULL or count equal to 0 */
        return result;
    }

    if (disk == NULL) {
        return result;
    }

    DISK_LOCK(&disk->disk_mutex);

    if (disk->disk_status != STAT_INUSED) {
        goto ERROR_HANDLE;
    }

    if ((count > disk->sector_count) || ((disk->sector_count - count) < sector)) {
        goto ERROR_HANDLE;
    }

#ifdef LOSCFG_FS_FAT_CACHE
    if (disk->bcache != NULL) {
        if (((UINT64)(disk->bcache->sectorSize) * count) > UINT_MAX) {
            goto ERROR_HANDLE;
        }
        len = disk->bcache->sectorSize * count;
        /* useRead should be FALSE when reading large contiguous data *///读取大量连续数据时，useRead 应为 FALSE
        result = BlockCacheRead(disk->bcache, (UINT8 *)buf, &len, sector, useRead);//从缓存区里读
        if (result != ENOERR) {
            PRINT_ERR("los_disk_read read err = %d, sector = %llu, len = %u\n", result, sector, len);
        }
    } else {
#endif
    if (disk->dev == NULL) {
        goto ERROR_HANDLE;
    }
    struct block_operations *bops = (struct block_operations *)((struct drv_data *)disk->dev->data)->ops;//获取块操作数据方式
    if ((bops != NULL) && (bops->read != NULL)) {
        result = bops->read(disk->dev, (UINT8 *)buf, sector, count);//读取绝对扇区内容至buf
        if (result == (INT32)count) {
            result = ENOERR;
        }
    }
#ifdef LOSCFG_FS_FAT_CACHE
    }
#endif

    if (result != ENOERR) {
        goto ERROR_HANDLE;
    }

    DISK_UNLOCK(&disk->disk_mutex);
    return ENOERR;

ERROR_HANDLE:
    DISK_UNLOCK(&disk->disk_mutex);
    return VFS_ERROR;
}
///将buf内容写到指定扇区,
INT32 los_disk_write(INT32 drvID, const VOID *buf, UINT64 sector, UINT32 count)
{
#ifdef LOSCFG_FS_FAT_CACHE
    UINT32 len;
#endif
    INT32 result = VFS_ERROR;
    los_disk *disk = get_disk(drvID);
    if (disk == NULL || disk->dev == NULL || disk->dev->data == NULL) {
        return result;
    }

    if ((buf == NULL) || (count == 0)) { /* buff equal to NULL or count equal to 0 */
        return result;
    }

    DISK_LOCK(&disk->disk_mutex);

    if (disk->disk_status != STAT_INUSED) {
        goto ERROR_HANDLE;
    }

    if ((count > disk->sector_count) || ((disk->sector_count - count) < sector)) {
        goto ERROR_HANDLE;
    }

#ifdef LOSCFG_FS_FAT_CACHE
    if (disk->bcache != NULL) {
        if (((UINT64)(disk->bcache->sectorSize) * count) > UINT_MAX) {
            goto ERROR_HANDLE;
        }
        len = disk->bcache->sectorSize * count;
        result = BlockCacheWrite(disk->bcache, (const UINT8 *)buf, &len, sector);//写入缓存,后续由缓存同步至磁盘
        if (result != ENOERR) {
            PRINT_ERR("los_disk_write write err = %d, sector = %llu, len = %u\n", result, sector, len);
        }
    } else {
#endif//无缓存情况下获取操作块实现函数指针结构体
    struct block_operations *bops = (struct block_operations *)((struct drv_data *)disk->dev->data)->ops;
    if ((bops != NULL) && (bops->write != NULL)) {
        result = bops->write(disk->dev, (UINT8 *)buf, sector, count);//真正的写磁盘
        if (result == (INT32)count) {
            result = ENOERR;
        }
    }
#ifdef LOSCFG_FS_FAT_CACHE
    }
#endif

    if (result != ENOERR) {
        goto ERROR_HANDLE;
    }

    DISK_UNLOCK(&disk->disk_mutex);
    return ENOERR;

ERROR_HANDLE:
    DISK_UNLOCK(&disk->disk_mutex);
    return VFS_ERROR;
}

INT32 los_disk_ioctl(INT32 drvID, INT32 cmd, VOID *buf)
{
    struct geometry info;
    los_disk *disk = get_disk(drvID);
    if (disk == NULL) {
        return VFS_ERROR;
    }

    DISK_LOCK(&disk->disk_mutex);

    if ((disk->dev == NULL) || (disk->disk_status != STAT_INUSED)) {
        goto ERROR_HANDLE;
    }

    if (cmd == DISK_CTRL_SYNC) {
        DISK_UNLOCK(&disk->disk_mutex);
        return ENOERR;
    }

    if (buf == NULL) {
        goto ERROR_HANDLE;
    }

    (VOID)memset_s(&info, sizeof(info), 0, sizeof(info));

    struct block_operations *bops = (struct block_operations *)((struct drv_data *)disk->dev->data)->ops;
    if ((bops == NULL) || (bops->geometry == NULL) ||
        (bops->geometry(disk->dev, &info) != 0)) {
        goto ERROR_HANDLE;
    }

    if (cmd == DISK_GET_SECTOR_COUNT) {
        *(UINT64 *)buf = info.geo_nsectors;
        if (info.geo_nsectors == 0) {
            goto ERROR_HANDLE;
        }
    } else if (cmd == DISK_GET_SECTOR_SIZE) {
        *(size_t *)buf = info.geo_sectorsize;
    } else if (cmd == DISK_GET_BLOCK_SIZE) { /* Get erase block size in unit of sectors (UINT32) */
        /* Block Num SDHC == 512, SD can be set to 512 or other */
        *(size_t *)buf = DISK_MAX_SECTOR_SIZE / info.geo_sectorsize;
    } else {
        goto ERROR_HANDLE;
    }

    DISK_UNLOCK(&disk->disk_mutex);
    return ENOERR;

ERROR_HANDLE:
    DISK_UNLOCK(&disk->disk_mutex);
    return VFS_ERROR;
}
///读分区上扇区内容
INT32 los_part_read(INT32 pt, VOID *buf, UINT64 sector, UINT32 count, BOOL useRead)
{
    const los_part *part = get_part(pt);//先拿到分区信息
    los_disk *disk = NULL;
    INT32 ret;

    if (part == NULL) {
        return VFS_ERROR;
    }

    disk = get_disk((INT32)part->disk_id);//再拿到磁盘信息
    if (disk == NULL) {
        return VFS_ERROR;
    }

    DISK_LOCK(&disk->disk_mutex);//锁磁盘
    if ((part->dev == NULL) || (disk->disk_status != STAT_INUSED)) {
        goto ERROR_HANDLE;
    }

    if (count > part->sector_count) {//不能超过分区总扇区数量
        PRINT_ERR("los_part_read failed, invalid count, count = %u\n", count);
        goto ERROR_HANDLE;
    }

    /* Read from absolute sector. */
    if (part->type == EMMC) {//如果分区类型是磁盘介质
        if ((disk->sector_count - part->sector_start) > sector) {//从绝对扇区读取
            sector += part->sector_start;
        } else {
            PRINT_ERR("los_part_read failed, invalid sector, sector = %llu\n", sector);
            goto ERROR_HANDLE;
        }
    }

    if ((sector >= GetFirstPartStart(part)) &&	//扇区范围判断
        (((sector + count) > (part->sector_start + part->sector_count)) || (sector < part->sector_start))) {
        PRINT_ERR("los_part_read error, sector = %llu, count = %u, part->sector_start = %llu, "
                  "part->sector_count = %llu\n", sector, count, part->sector_start, part->sector_count);
        goto ERROR_HANDLE;
    }
	//读取大量连续数据时，useRead 应为 FALSE
    /* useRead should be FALSE when reading large contiguous data */
    ret = los_disk_read((INT32)part->disk_id, buf, sector, count, useRead);
    if (ret < 0) {
        goto ERROR_HANDLE;
    }

    DISK_UNLOCK(&disk->disk_mutex);
    return ENOERR;

ERROR_HANDLE:
    DISK_UNLOCK(&disk->disk_mutex);
    return VFS_ERROR;
}
///将数据写入指定分区的指定扇区
INT32 los_part_write(INT32 pt, const VOID *buf, UINT64 sector, UINT32 count)
{
    const los_part *part = get_part(pt);
    los_disk *disk = NULL;
    INT32 ret;

    if (part == NULL) {
        return VFS_ERROR;
    }

    disk = get_disk((INT32)part->disk_id);
    if (disk == NULL) {
        return VFS_ERROR;
    }

    DISK_LOCK(&disk->disk_mutex);
    if ((part->dev == NULL) || (disk->disk_status != STAT_INUSED)) {
        goto ERROR_HANDLE;
    }

    if (count > part->sector_count) {
        PRINT_ERR("los_part_write failed, invalid count, count = %u\n", count);
        goto ERROR_HANDLE;
    }

    /* Write to absolute sector. */
    if (part->type == EMMC) {
        if ((disk->sector_count - part->sector_start) > sector) {
            sector += part->sector_start;
        } else {
            PRINT_ERR("los_part_write failed, invalid sector, sector = %llu\n", sector);
            goto ERROR_HANDLE;
        }
    }

    if ((sector >= GetFirstPartStart(part)) &&
        (((sector + count) > (part->sector_start + part->sector_count)) || (sector < part->sector_start))) {
        PRINT_ERR("los_part_write, sector = %llu, count = %u, part->sector_start = %llu, "
                  "part->sector_count = %llu\n", sector, count, part->sector_start, part->sector_count);
        goto ERROR_HANDLE;
    }
	//sector已变成磁盘绝对扇区
    ret = los_disk_write((INT32)part->disk_id, buf, sector, count);//直接写入磁盘,
    if (ret < 0) {
        goto ERROR_HANDLE;
    }

    DISK_UNLOCK(&disk->disk_mutex);
    return ENOERR;

ERROR_HANDLE:
    DISK_UNLOCK(&disk->disk_mutex);
    return VFS_ERROR;
}

#define GET_ERASE_BLOCK_SIZE 0x2

INT32 los_part_ioctl(INT32 pt, INT32 cmd, VOID *buf)
{
    struct geometry info;
    los_part *part = get_part(pt);
    los_disk *disk = NULL;

    if (part == NULL) {
        return VFS_ERROR;
    }

    disk = get_disk((INT32)part->disk_id);
    if (disk == NULL) {
        return VFS_ERROR;
    }

    DISK_LOCK(&disk->disk_mutex);
    if ((part->dev == NULL) || (disk->disk_status != STAT_INUSED)) {
        goto ERROR_HANDLE;
    }

    if (cmd == DISK_CTRL_SYNC) {
        DISK_UNLOCK(&disk->disk_mutex);
        return ENOERR;
    }

    if (buf == NULL) {
        goto ERROR_HANDLE;
    }

    (VOID)memset_s(&info, sizeof(info), 0, sizeof(info));

    struct block_operations *bops = (struct block_operations *)((struct drv_data *)part->dev->data)->ops;
    if ((bops == NULL) || (bops->geometry == NULL) ||
        (bops->geometry(part->dev, &info) != 0)) {
        goto ERROR_HANDLE;
    }

    if (cmd == DISK_GET_SECTOR_COUNT) {
        *(UINT64 *)buf = part->sector_count;
        if (*(UINT64 *)buf == 0) {
            goto ERROR_HANDLE;
        }
    } else if (cmd == DISK_GET_SECTOR_SIZE) {
        *(size_t *)buf = info.geo_sectorsize;
    } else if (cmd == DISK_GET_BLOCK_SIZE) { /* Get erase block size in unit of sectors (UINT32) */
        if ((bops->ioctl == NULL) ||
            (bops->ioctl(part->dev, GET_ERASE_BLOCK_SIZE, (UINTPTR)buf) != 0)) {
            goto ERROR_HANDLE;
        }
    } else {
        goto ERROR_HANDLE;
    }

    DISK_UNLOCK(&disk->disk_mutex);
    return ENOERR;

ERROR_HANDLE:
    DISK_UNLOCK(&disk->disk_mutex);
    return VFS_ERROR;
}

INT32 los_disk_cache_clear(INT32 drvID)
{
    INT32 result;
    los_part *part = get_part(drvID);
    los_disk *disk = NULL;

    if (part == NULL) {
        return VFS_ERROR;
    }
    result = OsSdSync(part->disk_id);
    if (result != 0) {
        PRINTK("[ERROR]disk_cache_clear SD sync failed!\n");
        return result;
    }

    disk = get_disk(part->disk_id);
    if (disk == NULL) {
        return VFS_ERROR;
    }

    DISK_LOCK(&disk->disk_mutex);
    result = BcacheClearCache(disk->bcache);
    DISK_UNLOCK(&disk->disk_mutex);

    return result;
}

#ifdef LOSCFG_FS_FAT_CACHE
static VOID DiskCacheThreadInit(UINT32 diskID, OsBcache *bc)
{
    bc->prereadFun = NULL;

    if (GetDiskUsbStatus(diskID) == FALSE) {
        if (BcacheAsyncPrereadInit(bc) == LOS_OK) {
            bc->prereadFun = ResumeAsyncPreread;
        }

#ifdef LOSCFG_FS_FAT_CACHE_SYNC_THREAD
        BcacheSyncThreadInit(bc, diskID);
#endif
    }

    if (OsReHookFuncAddDiskRef != NULL) {
        (VOID)OsReHookFuncAddDiskRef((StorageHookFunction)OsSdSync, (VOID *)0);
        (VOID)OsReHookFuncAddDiskRef((StorageHookFunction)OsSdSync, (VOID *)1);
    }
}

static OsBcache *DiskCacheInit(UINT32 diskID, const struct geometry *diskInfo, struct Vnode *blkDriver)
{
#define SECTOR_SIZE 512

    OsBcache *bc = NULL;
    UINT32 sectorPerBlock = diskInfo->geo_sectorsize / SECTOR_SIZE;
    if (sectorPerBlock != 0) {
        sectorPerBlock = g_uwFatSectorsPerBlock / sectorPerBlock;
        if (sectorPerBlock != 0) {
            bc = BlockCacheInit(blkDriver, diskInfo->geo_sectorsize, sectorPerBlock,
                                g_uwFatBlockNums, diskInfo->geo_nsectors / sectorPerBlock);
        }
    }

    if (bc == NULL) {
        PRINT_ERR("disk_init : disk have not init bcache cache!\n");
        return NULL;
    }

    DiskCacheThreadInit(diskID, bc);
    return bc;
}

static VOID DiskCacheDeinit(los_disk *disk)
{
    UINT32 diskID = disk->disk_id;
    if (GetDiskUsbStatus(diskID) == FALSE) {
        if (BcacheAsyncPrereadDeinit(disk->bcache) != LOS_OK) {
            PRINT_ERR("Blib async preread deinit failed in %s, %d\n", __FUNCTION__, __LINE__);
        }
#ifdef LOSCFG_FS_FAT_CACHE_SYNC_THREAD
        BcacheSyncThreadDeinit(disk->bcache);
#endif
    }

    BlockCacheDeinit(disk->bcache);
    disk->bcache = NULL;

    if (OsReHookFuncDelDiskRef != NULL) {
        (VOID)OsReHookFuncDelDiskRef((StorageHookFunction)OsSdSync);
    }
}
#endif
///磁盘结构体初始化,初始化 los_disk 结构体
static VOID DiskStructInit(const CHAR *diskName, INT32 diskID, const struct geometry *diskInfo,
                           struct Vnode *blkDriver, los_disk *disk)
{
    size_t nameLen;
    disk->disk_id = diskID;
    disk->dev = blkDriver;
    disk->sector_start = 0;
    disk->sector_size = diskInfo->geo_sectorsize;
    disk->sector_count = diskInfo->geo_nsectors;

    nameLen = strlen(diskName); /* caller los_disk_init has chek name */

    if (disk->disk_name != NULL) {
        LOS_MemFree(m_aucSysMem0, disk->disk_name);
        disk->disk_name = NULL;
    }

    disk->disk_name = LOS_MemAlloc(m_aucSysMem0, (nameLen + 1));
    if (disk->disk_name == NULL) {
        PRINT_ERR("DiskStructInit alloc memory failed.\n");
        return;
    }

    if (strncpy_s(disk->disk_name, (nameLen + 1), diskName, nameLen) != EOK) {
        PRINT_ERR("DiskStructInit strncpy_s failed.\n");
        LOS_MemFree(m_aucSysMem0, disk->disk_name);
        disk->disk_name = NULL;
        return;
    }
    disk->disk_name[nameLen] = '\0';
    LOS_ListInit(&disk->head);
}
///磁盘分区和注册分区
static INT32 DiskDivideAndPartitionRegister(struct disk_divide_info *info, los_disk *disk)
{
    INT32 ret;

    if (info != NULL) {//有分区信息则直接分区
        ret = DiskDivide(disk, info);
        if (ret != ENOERR) {
            PRINT_ERR("DiskDivide failed, ret = %d\n", ret);
            return ret;
        }
    } else {//木有分区信息,则先注册分区
        ret = DiskPartitionRegister(disk);
        if (ret != ENOERR) {
            PRINT_ERR("DiskPartitionRegister failed, ret = %d\n", ret);
            return ret;
        }
    }
    return ENOERR;
}
///磁盘反初始化
static INT32 DiskDeinit(los_disk *disk)
{
    los_part *part = NULL;
    char *diskName = NULL;
    CHAR devName[DEV_NAME_BUFF_SIZE];
    INT32 ret;

    if (LOS_ListEmpty(&disk->head) == FALSE) {
        part = LOS_DL_LIST_ENTRY(disk->head.pstNext, los_part, list);
        while (&part->list != &disk->head) {
            diskName = (disk->disk_name == NULL) ? "null" : disk->disk_name;
            ret = snprintf_s(devName, sizeof(devName), sizeof(devName) - 1, "%s%c%d",
                             diskName, 'p', disk->part_count - 1);
            if (ret < 0) {
                return -ENAMETOOLONG;
            }
            DiskPartDelFromDisk(disk, part);//从磁盘删除分区
            (VOID)unregister_blockdriver(devName);//注销块驱动程序
            DiskPartRelease(part);//释放分区信息

            part = LOS_DL_LIST_ENTRY(disk->head.pstNext, los_part, list);//下一个分区内容
        }
    }

    DISK_LOCK(&disk->disk_mutex);

#ifdef LOSCFG_FS_FAT_CACHE
    DiskCacheDeinit(disk);
#endif

    disk->dev = NULL;
    DISK_UNLOCK(&disk->disk_mutex);
    (VOID)unregister_blockdriver(disk->disk_name);
    if (disk->disk_name != NULL) {
        LOS_MemFree(m_aucSysMem0, disk->disk_name);
        disk->disk_name = NULL;
    }
    ret = pthread_mutex_destroy(&disk->disk_mutex);
    if (ret != 0) {
        PRINT_ERR("%s %d, mutex destroy failed, ret = %d\n", __FUNCTION__, __LINE__, ret);
        return -EFAULT;
    }

    disk->disk_status = STAT_UNUSED;

    return ENOERR;
}
///磁盘初始化
static VOID OsDiskInitSub(const CHAR *diskName, INT32 diskID, los_disk *disk,
                          struct geometry *diskInfo, struct Vnode *blkDriver)
{
    pthread_mutexattr_t attr;
#ifdef LOSCFG_FS_FAT_CACHE //使能FAT缓存
    OsBcache *bc = DiskCacheInit((UINT32)diskID, diskInfo, blkDriver);
    disk->bcache = bc;
#endif

    (VOID)pthread_mutexattr_init(&attr);
    attr.type = PTHREAD_MUTEX_RECURSIVE;
    (VOID)pthread_mutex_init(&disk->disk_mutex, &attr);

    DiskStructInit(diskName, diskID, diskInfo, blkDriver, disk);
}
///磁盘初始化
INT32 los_disk_init(const CHAR *diskName, const struct block_operations *bops,
                    VOID *priv, INT32 diskID, VOID *info)
{
    struct geometry diskInfo;
    struct Vnode *blkDriver = NULL;
    los_disk *disk = get_disk(diskID);
    INT32 ret;
	//参数检查
    if ((diskName == NULL) || (disk == NULL) ||
        (disk->disk_status != STAT_UNREADY) || (strlen(diskName) > DISK_NAME)) {
        return VFS_ERROR;
    }
	//注册块设备驱动,设备驱动程序由设备提供
    if (register_blockdriver(diskName, bops, RWE_RW_RW, priv) != 0) {
        PRINT_ERR("disk_init : register %s fail!\n", diskName);
        return VFS_ERROR;
    }

    VnodeHold();
    ret = VnodeLookup(diskName, &blkDriver, 0);
    if (ret < 0) {
        VnodeDrop();
        PRINT_ERR("disk_init : find %s fail!\n", diskName);
        ret = ENOENT;
        goto DISK_FIND_ERROR;
    }
    struct block_operations *bops2 = (struct block_operations *)((struct drv_data *)blkDriver->data)->ops;
	//块操作,块是文件系统层面的概念,块（Block）是文件系统存取数据的最小单位，一般大小是4KB
    if ((bops2 == NULL) || (bops2->geometry == NULL) ||
        (bops2->geometry(blkDriver, &diskInfo) != 0)) {//geometry 就是 CHS
        goto DISK_BLKDRIVER_ERROR;
    }

    if (diskInfo.geo_sectorsize < DISK_MAX_SECTOR_SIZE) {//验证扇区大小
        goto DISK_BLKDRIVER_ERROR;
    }

    OsDiskInitSub(diskName, diskID, disk, &diskInfo, blkDriver);//初始化磁盘描述符
    VnodeDrop();
    if (DiskDivideAndPartitionRegister(info, disk) != ENOERR) {
        (VOID)DiskDeinit(disk);
        return VFS_ERROR;
    }

    disk->disk_status = STAT_INUSED;//磁盘状态变成使用中
    if (info != NULL) {//https://www.huaweicloud.com/articles/bcdefd0d9da5de83d513123ef3aabcf0.html
        disk->type = EMMC;//eMMC 是 embedded MultiMediaCard 的简称
    } else {
        disk->type = OTHERS;
    }
    return ENOERR;

DISK_BLKDRIVER_ERROR:
    PRINT_ERR("disk_init : register %s ok but get disk info fail!\n", diskName);
    VnodeDrop();
DISK_FIND_ERROR:
    (VOID)unregister_blockdriver(diskName);//注销块设备驱动
    return VFS_ERROR;
}
///磁盘反初始化
INT32 los_disk_deinit(INT32 diskID)
{
    int ret;
    los_disk *disk = get_disk(diskID);
    if (disk == NULL) {
        return -EINVAL;
    }
    ret = ForceUmountDev(disk->dev);
    PRINTK("warning: %s lost, force umount ret = %d\n", disk->disk_name, ret);

    DISK_LOCK(&disk->disk_mutex);

    if (disk->disk_status != STAT_INUSED) {
        DISK_UNLOCK(&disk->disk_mutex);
        return -EINVAL;
    }

    disk->disk_status = STAT_UNREADY;//未格式化状态
    DISK_UNLOCK(&disk->disk_mutex);

    return DiskDeinit(disk);
}
///磁盘同步,同步指的是缓存同步
INT32 los_disk_sync(INT32 drvID)
{
    INT32 ret = ENOERR;
    los_disk *disk = get_disk(drvID);
    if (disk == NULL) {
        return EINVAL;
    }

    DISK_LOCK(&disk->disk_mutex);
    if (disk->disk_status != STAT_INUSED) {
        DISK_UNLOCK(&disk->disk_mutex);
        return EINVAL;
    }

#ifdef LOSCFG_FS_FAT_CACHE
        if (disk->bcache != NULL) {
            ret = BlockCacheSync(disk->bcache);
        }
#endif

    DISK_UNLOCK(&disk->disk_mutex);
    return ret;
}
///设置磁盘块缓存
INT32 los_disk_set_bcache(INT32 drvID, UINT32 sectorPerBlock, UINT32 blockNum)
{
#ifdef LOSCFG_FS_FAT_CACHE

    INT32 ret;
    UINT32 intSave;
    OsBcache *bc = NULL;
    los_disk *disk = get_disk(drvID);
    if ((disk == NULL) || (sectorPerBlock == 0)) {
        return EINVAL;
    }

    /*
     * Because we use UINT32 flag[BCACHE_BLOCK_FLAGS] in bcache for sectors bitmap tag, so it must
     * be less than 32 * BCACHE_BLOCK_FLAGS.
     */
    if (((sectorPerBlock % UNSIGNED_INTEGER_BITS) != 0) ||
        ((sectorPerBlock >> UNINT_LOG2_SHIFT) > BCACHE_BLOCK_FLAGS)) {
        return EINVAL;
    }

    DISK_LOCK(&disk->disk_mutex);

    if (disk->disk_status != STAT_INUSED) {
        goto ERROR_HANDLE;
    }

    if (disk->bcache != NULL) {
        ret = BlockCacheSync(disk->bcache);
        if (ret != ENOERR) {
            DISK_UNLOCK(&disk->disk_mutex);
            return ret;
        }
    }

    spin_lock_irqsave(&g_diskFatBlockSpinlock, intSave);
    DiskCacheDeinit(disk);

    g_uwFatBlockNums = blockNum;
    g_uwFatSectorsPerBlock = sectorPerBlock;

    bc = BlockCacheInit(disk->dev, disk->sector_size, sectorPerBlock, blockNum, disk->sector_count / sectorPerBlock);
    if ((bc == NULL) && (blockNum != 0)) {
        spin_unlock_irqrestore(&g_diskFatBlockSpinlock, intSave);
        DISK_UNLOCK(&disk->disk_mutex);
        return ENOMEM;
    }

    if (bc != NULL) {
        DiskCacheThreadInit((UINT32)drvID, bc);
    }

    disk->bcache = bc;
    spin_unlock_irqrestore(&g_diskFatBlockSpinlock, intSave);
    DISK_UNLOCK(&disk->disk_mutex);
    return ENOERR;

ERROR_HANDLE:
    DISK_UNLOCK(&disk->disk_mutex);
    return EINVAL;
#else
    return VFS_ERROR;
#endif
}
///通过索引节点从磁盘中找分区信息
static los_part *OsPartFind(los_disk *disk, const struct Vnode *blkDriver)
{
    los_part *part = NULL;

    DISK_LOCK(&disk->disk_mutex);//先上锁,disk->head上挂的是本磁盘所有的分区
    if ((disk->disk_status != STAT_INUSED) || (LOS_ListEmpty(&disk->head) == TRUE)) {
        goto EXIT;
    }
    part = LOS_DL_LIST_ENTRY(disk->head.pstNext, los_part, list);//拿出首个分区信息
    if (disk->dev == blkDriver) {//如果节点信息等于 disk->dev,注意此处是  disk 而不是 part->dev
        goto EXIT;//找到 goto
    }

    while (&part->list != &disk->head) {//遍历链表
        if (part->dev == blkDriver) {//找到节点所在分区
            goto EXIT;
        }
        part = LOS_DL_LIST_ENTRY(part->list.pstNext, los_part, list);//下一个分区信息
    }
    part = NULL;

EXIT:
    DISK_UNLOCK(&disk->disk_mutex);
    return part;
}
///通过索引节点找到分区信息
los_part *los_part_find(struct Vnode *blkDriver)
{
    INT32 i;
    los_disk *disk = NULL;
    los_part *part = NULL;

    if (blkDriver == NULL) {
        return NULL;
    }

    for (i = 0; i < SYS_MAX_DISK; i++) {//遍历所有磁盘
        disk = get_disk(i);
        if (disk == NULL) {
            continue;
        }
        part = OsPartFind(disk, blkDriver);//从磁盘中通过节点找分区信息
        if (part != NULL) {
            return part;
        }
    }

    return NULL;
}
///访问分区
INT32 los_part_access(const CHAR *dev, mode_t mode)
{
    los_part *part = NULL;
    struct Vnode *node = NULL;

    VnodeHold();
    if (VnodeLookup(dev, &node, 0) < 0) {
        VnodeDrop();
        return VFS_ERROR;
    }

    part = los_part_find(node);//通过节点找到分区
    VnodeDrop();
    if (part == NULL) {
        return VFS_ERROR;
    }

    return ENOERR;
}
///设置分区名称
INT32 SetDiskPartName(los_part *part, const CHAR *src)
{
    size_t len;
    los_disk *disk = NULL;

    if ((part == NULL) || (src == NULL)) {
        return VFS_ERROR;
    }

    len = strlen(src);
    if ((len == 0) || (len >= DISK_NAME)) {
        return VFS_ERROR;
    }

    disk = get_disk((INT32)part->disk_id);
    if (disk == NULL) {
        return VFS_ERROR;
    }

    DISK_LOCK(&disk->disk_mutex);
    if (disk->disk_status != STAT_INUSED) {
        goto ERROR_HANDLE;
    }

    part->part_name = (CHAR *)zalloc(len + 1);//分区名称内存需来自内核空间
    if (part->part_name == NULL) {
        PRINT_ERR("%s[%d] zalloc failure\n", __FUNCTION__, __LINE__);
        goto ERROR_HANDLE;
    }

    if (strcpy_s(part->part_name, len + 1, src) != EOK) {
        free(part->part_name);
        part->part_name = NULL;
        goto ERROR_HANDLE;
    }

    DISK_UNLOCK(&disk->disk_mutex);
    return ENOERR;

ERROR_HANDLE:
    DISK_UNLOCK(&disk->disk_mutex);
    return VFS_ERROR;
}

/**
 * @brief 添加 MMC分区
    \n  MMC， 是一种闪存卡（Flash Memory Card）标准，它定义了 MMC 的架构以及访问　Flash Memory 的接口和协议。
    \n  而eMMC 则是对 MMC 的一个拓展，以满足更高标准的性能、成本、体积、稳定、易用等的需求。
 * @param info 
 * @param sectorStart 
 * @param sectorCount 
 * @return INT32 
 */
INT32 add_mmc_partition(struct disk_divide_info *info, size_t sectorStart, size_t sectorCount)
{
    UINT32 index, i;

    if (info == NULL) {
        return VFS_ERROR;
    }
	//磁盘判断
    if ((info->part_count >= MAX_DIVIDE_PART_PER_DISK) || (sectorCount == 0)) {
        return VFS_ERROR;
    }
	//扇区判断
    if ((sectorCount > info->sector_count) || ((info->sector_count - sectorCount) < sectorStart)) {
        return VFS_ERROR;
    }

    index = info->part_count;
    for (i = 0; i < index; i++) {//验证目的是确保分区的顺序,扇区顺序从小到大排列
        if (sectorStart < (info->part[i].sector_start + info->part[i].sector_count)) {
            return VFS_ERROR;
        }
    }

    info->part[index].sector_start = sectorStart;//开始扇区
    info->part[index].sector_count = sectorCount;//扇区总数
    info->part[index].type = EMMC;//分区类型
    info->part_count++;//分区数量增加,鸿蒙分区数量上限默认是80个.

    return ENOERR;
}
///OHOS #partinfo /dev/mmcblk0p0 命令输出信息
VOID show_part(los_part *part)
{
    if ((part == NULL) || (part->dev == NULL)) {
        PRINT_ERR("part is NULL\n");
        return;
    }

    PRINTK("\npart info :\n");
    PRINTK("disk id          : %u\n", part->disk_id);	//磁盘ID
    PRINTK("part_id in system: %u\n", part->part_id);	//在整个系统的分区ID
    PRINTK("part no in disk  : %u\n", part->part_no_disk);//在磁盘分区的ID
    PRINTK("part no in mbr   : %u\n", part->part_no_mbr);//主分区ID
    PRINTK("part filesystem  : %02X\n", part->filesystem_type);//文件系统(FAT | NTFS)
    PRINTK("part sec start   : %llu\n", part->sector_start);//开始扇区
    PRINTK("part sec count   : %llu\n", part->sector_count);//扇区大小
}
///通过磁盘ID 擦除磁盘信息
#ifdef LOSCFG_DRIVERS_MMC
ssize_t StorageBlockMmcErase(uint32_t blockId, size_t secStart, size_t secNr);
#endif

INT32 EraseDiskByID(UINT32 diskID, size_t startSector, UINT32 sectors)
{
    INT32 ret = VFS_ERROR;
#ifdef LOSCFG_DRIVERS_MMC	//使能MMC
    los_disk *disk = get_disk((INT32)diskID);//找到磁盘信息
    if (disk != NULL) {
        ret = StorageBlockMmcErase(diskID, startSector, sectors);
    }//..\code-2.0-canary\device\hisilicon\third_party\uboot\u-boot-2020.01\cmd\mmc.c
#endif

    return ret;
}

