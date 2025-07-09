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
#ifndef LOSCFG_FS_FAT_CACHE
#include "los_vm_common.h"
#include "user_copy.h"
#endif

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

// 系统磁盘数组，存储所有磁盘设备信息
los_disk g_sysDisk[SYS_MAX_DISK];
// 系统分区数组，存储所有分区信息
los_part g_sysPart[SYS_MAX_PART];

// FAT文件系统每块扇区数，初始化为配置值
UINT32 g_uwFatSectorsPerBlock = CONFIG_FS_FAT_SECTOR_PER_BLOCK;
// FAT文件系统块数量，初始化为配置值
UINT32 g_uwFatBlockNums = CONFIG_FS_FAT_BLOCK_NUMS;

// 磁盘操作自旋锁，用于保护磁盘相关数据结构的并发访问
spinlock_t g_diskSpinlock;
// FAT块操作自旋锁，用于保护FAT块相关数据的并发访问
spinlock_t g_diskFatBlockSpinlock;

// USB模式标志位，用于标记磁盘是否为USB设备
UINT32 g_usbMode = 0;

// 内存地址对齐字节数，64字节对齐
#define MEM_ADDR_ALIGN_BYTE  64
// 文件权限宏，可读可写可执行（所有者），可读可写（组用户），可读可写（其他用户）
#define RWE_RW_RW            0755

// 磁盘互斥锁加锁宏
// 参数：mux - 互斥锁指针
#define DISK_LOCK(mux) do {                                              \
    if (pthread_mutex_lock(mux) != 0) {                                  \
        PRINT_ERR("%s %d, mutex lock failed\n", __FUNCTION__, __LINE__); \
    }                                                                    \
} while (0)

// 磁盘互斥锁解锁宏
// 参数：mux - 互斥锁指针
#define DISK_UNLOCK(mux) do {                                              \
    if (pthread_mutex_unlock(mux) != 0) {                                  \
        PRINT_ERR("%s %d, mutex unlock failed\n", __FUNCTION__, __LINE__); \
    }                                                                      \
} while (0)

// 存储钩子函数类型定义，用于存储设备相关的钩子处理
typedef VOID *(*StorageHookFunction)(VOID *);

#ifdef LOSCFG_FS_FAT_CACHE
// 添加磁盘引用的钩子函数（弱引用osReHookFuncAdd）
// 参数：handler - 钩子函数，param - 函数参数
// 返回值：操作结果
static UINT32 OsReHookFuncAddDiskRef(StorageHookFunction handler,
                                     VOID *param) __attribute__((weakref("osReHookFuncAdd")));

// 删除磁盘引用的钩子函数（弱引用osReHookFuncDel）
// 参数：handler - 钩子函数
// 返回值：操作结果
static UINT32 OsReHookFuncDelDiskRef(StorageHookFunction handler) __attribute__((weakref("osReHookFuncDel")));

// 获取FAT块数量
// 返回值：当前FAT块数量
UINT32 GetFatBlockNums(VOID)
{
    return g_uwFatBlockNums;  // 返回全局变量g_uwFatBlockNums的值
}

// 设置FAT块数量
// 参数：blockNums - 新的块数量
VOID SetFatBlockNums(UINT32 blockNums)
{
    g_uwFatBlockNums = blockNums;  // 更新全局变量g_uwFatBlockNums
}

// 获取FAT每块扇区数
// 返回值：当前每块扇区数
UINT32 GetFatSectorsPerBlock(VOID)
{
    return g_uwFatSectorsPerBlock;  // 返回全局变量g_uwFatSectorsPerBlock的值
}

// 设置FAT每块扇区数（带有效性检查）
// 参数：sectorsPerBlock - 新的每块扇区数
VOID SetFatSectorsPerBlock(UINT32 sectorsPerBlock)
{
    // 检查扇区数是否为32的倍数（UNSIGNED_INTEGER_BITS通常为32）且移位后不超过缓存块标志位
    if (((sectorsPerBlock % UNSIGNED_INTEGER_BITS) == 0) &&
        ((sectorsPerBlock >> UNINT_LOG2_SHIFT) <= BCACHE_BLOCK_FLAGS)) {
        g_uwFatSectorsPerBlock = sectorsPerBlock;  // 满足条件则更新全局变量
    }
}
#endif

// 根据磁盘名称分配磁盘ID
// 参数：diskName - 磁盘名称
// 返回值：成功返回磁盘ID，失败返回VFS_ERROR
INT32 los_alloc_diskid_byname(const CHAR *diskName)
{
    INT32 diskID;                // 磁盘ID
    los_disk *disk = NULL;       // 磁盘结构体指针
    UINT32 intSave;              // 中断状态保存变量
    size_t nameLen;              // 磁盘名称长度

    if (diskName == NULL) {      // 检查输入参数是否为空
        PRINT_ERR("The parameter disk_name is NULL");
        return VFS_ERROR;        // 参数错误，返回错误码
    }

    nameLen = strlen(diskName);  // 获取磁盘名称长度
    if (nameLen > DISK_NAME) {   // 检查名称长度是否超过最大限制
        PRINT_ERR("diskName is too long!\n");
        return VFS_ERROR;        // 名称过长，返回错误码
    }
    spin_lock_irqsave(&g_diskSpinlock, intSave);  // 加锁并禁止中断

    // 遍历磁盘数组，寻找未使用的磁盘槽位
    for (diskID = 0; diskID < SYS_MAX_DISK; diskID++) {
        disk = get_disk(diskID);  // 获取当前ID对应的磁盘结构体
        // 如果磁盘存在且状态为未使用
        if ((disk != NULL) && (disk->disk_status == STAT_UNUSED)) {
            disk->disk_status = STAT_UNREADY;  // 将状态设置为未就绪
            break;                             // 找到可用槽位，跳出循环
        }
    }

    spin_unlock_irqrestore(&g_diskSpinlock, intSave);  // 解锁并恢复中断

    // 检查是否找到可用磁盘槽位
    if ((disk == NULL) || (diskID == SYS_MAX_DISK)) {
        PRINT_ERR("los_alloc_diskid_byname failed %d!\n", diskID);
        return VFS_ERROR;  // 未找到可用槽位，返回错误码
    }

    // 如果磁盘已有名称，先释放原有内存
    if (disk->disk_name != NULL) {
        LOS_MemFree(m_aucSysMem0, disk->disk_name);
        disk->disk_name = NULL;
    }

    // 为磁盘名称分配内存（+1用于存储字符串结束符）
    disk->disk_name = LOS_MemAlloc(m_aucSysMem0, (nameLen + 1));
    if (disk->disk_name == NULL) {  // 检查内存分配是否成功
        PRINT_ERR("los_alloc_diskid_byname alloc disk name failed\n");
        return VFS_ERROR;  // 内存分配失败，返回错误码
    }

    // 安全拷贝磁盘名称到分配的内存
    if (strncpy_s(disk->disk_name, (nameLen + 1), diskName, nameLen) != EOK) {
        PRINT_ERR("The strncpy_s failed.\n");
        LOS_MemFree(m_aucSysMem0, disk->disk_name);  // 拷贝失败，释放内存
        disk->disk_name = NULL;
        return VFS_ERROR;  // 返回错误码
    }

    disk->disk_name[nameLen] = '\0';  // 手动添加字符串结束符

    return diskID;  // 返回分配的磁盘ID
}

// 根据磁盘名称获取磁盘ID
// 参数：diskName - 磁盘名称
// 返回值：成功返回磁盘ID，失败返回VFS_ERROR
INT32 los_get_diskid_byname(const CHAR *diskName)
{
    INT32 diskID;                // 磁盘ID
    los_disk *disk = NULL;       // 磁盘结构体指针
    size_t diskNameLen;          // 磁盘名称长度

    if (diskName == NULL) {      // 检查输入参数是否为空
        PRINT_ERR("The parameter diskName is NULL");
        return VFS_ERROR;        // 参数错误，返回错误码
    }

    diskNameLen = strlen(diskName);  // 获取磁盘名称长度
    if (diskNameLen > DISK_NAME) {   // 检查名称长度是否超过最大限制
        PRINT_ERR("diskName is too long!\n");
        return VFS_ERROR;        // 名称过长，返回错误码
    }

    // 遍历磁盘数组，查找匹配名称的已使用磁盘
    for (diskID = 0; diskID < SYS_MAX_DISK; diskID++) {
        disk = get_disk(diskID);  // 获取当前ID对应的磁盘结构体
        // 检查磁盘是否存在、名称是否存在且状态为已使用
        if ((disk != NULL) && (disk->disk_name != NULL) && (disk->disk_status == STAT_INUSED)) {
            if (strlen(disk->disk_name) != diskNameLen) {  // 长度不匹配则跳过
                continue;
            }
            if (strcmp(diskName, disk->disk_name) == 0) {  // 名称完全匹配
                break;  // 找到目标磁盘，跳出循环
            }
        }
    }
    // 检查是否找到匹配的磁盘
    if ((disk == NULL) || (diskID == SYS_MAX_DISK)) {
        PRINT_ERR("los_get_diskid_byname failed!\n");
        return VFS_ERROR;  // 未找到，返回错误码
    }
    return diskID;  // 返回找到的磁盘ID
}

// 根据类型获取MMC磁盘
// 参数：type - 磁盘类型
// 返回值：成功返回磁盘结构体指针，失败返回NULL
los_disk *los_get_mmcdisk_bytype(UINT8 type)
{
    const CHAR *mmcDevHead = "/dev/mmcblk";  // MMC设备名称前缀

    // 遍历所有磁盘，查找类型匹配且名称以"/dev/mmcblk"开头的磁盘
    for (INT32 diskId = 0; diskId < SYS_MAX_DISK; diskId++) {
        los_disk *disk = get_disk(diskId);  // 获取当前ID对应的磁盘结构体
        if (disk == NULL) {
            continue;  // 磁盘不存在，跳过
        } else if ((disk->type == type) && (strncmp(disk->disk_name, mmcDevHead, strlen(mmcDevHead)) == 0)) {
            return disk;  // 找到目标磁盘，返回指针
        }
    }
    PRINT_ERR("Cannot find the mmc disk!\n");
    return NULL;  // 未找到，返回NULL
}

// 设置USB磁盘状态（标记为USB设备）
// 参数：diskID - 磁盘ID
VOID OsSetUsbStatus(UINT32 diskID)
{
    if (diskID < SYS_MAX_DISK) {  // 检查磁盘ID是否有效
        // 设置对应位为1（UINT_MAX确保无符号操作）
        g_usbMode |= (1u << diskID) & UINT_MAX;
    }
}

// 清除USB磁盘状态（取消USB设备标记）
// 参数：diskID - 磁盘ID
VOID OsClearUsbStatus(UINT32 diskID)
{
    if (diskID < SYS_MAX_DISK) {  // 检查磁盘ID是否有效
        // 清除对应位（设置为0）
        g_usbMode &= ~((1u << diskID) & UINT_MAX);
    }
}

#ifdef LOSCFG_FS_FAT_CACHE
// 获取磁盘USB状态
// 参数：diskID - 磁盘ID
// 返回值：TRUE表示是USB磁盘，FALSE表示不是
static BOOL GetDiskUsbStatus(UINT32 diskID)
{
    // 检查对应位是否为1
    return (g_usbMode & (1u << diskID)) ? TRUE : FALSE;
}
#endif

// 根据ID获取磁盘结构体
// 参数：id - 磁盘ID
// 返回值：成功返回磁盘结构体指针，失败返回NULL
los_disk *get_disk(INT32 id)
{
    // 检查ID是否在有效范围内
    if ((id >= 0) && (id < SYS_MAX_DISK)) {
        return &g_sysDisk[id];  // 返回对应磁盘结构体指针
    }

    return NULL;  // ID无效，返回NULL
}

// 根据ID获取分区结构体
// 参数：id - 分区ID
// 返回值：成功返回分区结构体指针，失败返回NULL
los_part *get_part(INT32 id)
{
    // 检查ID是否在有效范围内
    if ((id >= 0) && (id < SYS_MAX_PART)) {
        return &g_sysPart[id];  // 返回对应分区结构体指针
    }

    return NULL;  // ID无效，返回NULL
}

// 获取磁盘第一个分区的起始扇区
// 参数：part - 分区结构体指针
// 返回值：第一个分区的起始扇区，无分区则返回0
static UINT64 GetFirstPartStart(const los_part *part)
{
    los_part *firstPart = NULL;  // 第一个分区指针
    // 获取分区所属磁盘
    los_disk *disk = get_disk((INT32)part->disk_id);
    // 如果磁盘存在，获取磁盘分区链表中的第一个分区
    firstPart = (disk == NULL) ? NULL : LOS_DL_LIST_ENTRY(disk->head.pstNext, los_part, list);
    return (firstPart == NULL) ? 0 : firstPart->sector_start;  // 返回起始扇区
}

// 将分区添加到磁盘的分区链表
// 参数：disk - 磁盘结构体指针，part - 分区结构体指针
static VOID DiskPartAddToDisk(los_disk *disk, los_part *part)
{
    part->disk_id = disk->disk_id;      // 设置分区所属磁盘ID
    part->part_no_disk = disk->part_count;  // 设置分区在磁盘内的编号
    LOS_ListTailInsert(&disk->head, &part->list);  // 将分区添加到磁盘分区链表尾部
    disk->part_count++;  // 磁盘分区数量加1
}

// 从磁盘的分区链表中删除分区
// 参数：disk - 磁盘结构体指针，part - 分区结构体指针
static VOID DiskPartDelFromDisk(los_disk *disk, los_part *part)
{
    LOS_ListDelete(&part->list);  // 从链表中删除分区
    disk->part_count--;  // 磁盘分区数量减1
}

// 分配并初始化一个新分区
// 参数：dev - 设备节点指针，start - 起始扇区，count - 扇区数量
// 返回值：成功返回分区结构体指针，失败返回NULL
static los_part *DiskPartAllocate(struct Vnode *dev, UINT64 start, UINT64 count)
{
    UINT32 i;  // 循环计数器
    // 从分区数组起始位置开始遍历
    los_part *part = get_part(0); /* traversing from the beginning of the array */

    if (part == NULL) {
        return NULL;  // 分区数组为空，返回NULL
    }

    // 遍历分区数组，寻找未使用的分区槽位
    for (i = 0; i < SYS_MAX_PART; i++) {
        if (part->dev == NULL) {  // 找到未使用的分区（dev字段为空）
            part->part_id = i;    // 设置分区ID
            part->part_no_mbr = 0;  // MBR分区编号初始化为0
            part->dev = dev;      // 设置分区关联的设备节点
            part->sector_start = start;  // 设置起始扇区
            part->sector_count = count;  // 设置扇区数量
            part->part_name = NULL;      // 分区名称初始化为空
            LOS_ListInit(&part->list);   // 初始化分区链表节点

            return part;  // 返回初始化好的分区指针
        }
        part++;  // 移动到下一个分区
    }

    return NULL;  // 没有找到可用分区槽位，返回NULL
}

// 释放分区资源
// 参数：part - 分区结构体指针
static VOID DiskPartRelease(los_part *part)
{
    part->dev = NULL;                // 重置设备节点指针
    part->part_no_disk = 0;          // 重置分区在磁盘内的编号
    part->part_no_mbr = 0;           // 重置MBR分区编号
    if (part->part_name != NULL) {   // 如果分区名称存在
        free(part->part_name);       // 释放名称内存
        part->part_name = NULL;      // 重置名称指针
    }
}

/*
 * 设备名称由磁盘名称、'p'和分区数量组合而成，例如 "/dev/mmcblk0p0"
 * disk_name : DISK_NAME + 1
 * 'p' : 1
 * part_count: 1
 */
// 设备名称缓冲区大小宏定义（磁盘名称长度+3）
#define DEV_NAME_BUFF_SIZE  (DISK_NAME + 3)

// 向磁盘添加分区
// 参数：disk - 磁盘结构体指针，sectorStart - 起始扇区，sectorCount - 扇区数量，IsValidPart - 是否为有效分区
// 返回值：成功返回分区ID，失败返回VFS_ERROR
static INT32 DiskAddPart(los_disk *disk, UINT64 sectorStart, UINT64 sectorCount, BOOL IsValidPart)
{
    CHAR devName[DEV_NAME_BUFF_SIZE];  // 设备名称缓冲区
    struct Vnode *diskDev = NULL;      // 磁盘设备节点指针
    struct Vnode *partDev = NULL;      // 分区设备节点指针
    los_part *part = NULL;             // 分区结构体指针
    INT32 ret;                         // 返回值

    // 检查磁盘是否有效（存在、已使用且设备节点有效）
    if ((disk == NULL) || (disk->disk_status == STAT_UNUSED) ||
        (disk->dev == NULL)) {
        return VFS_ERROR;  // 磁盘无效，返回错误码
    }

    // 检查扇区范围是否有效（不超过磁盘总扇区数）
    if ((sectorCount > disk->sector_count) || ((disk->sector_count - sectorCount) < sectorStart)) {
        PRINT_ERR("DiskAddPart failed: sector start is %llu, sector count is %llu\n", sectorStart, sectorCount);
        return VFS_ERROR;  // 扇区范围无效，返回错误码
    }

    diskDev = disk->dev;  // 获取磁盘设备节点
    if (IsValidPart == TRUE) {  // 如果是有效分区，需要注册为块设备
        // 格式化分区设备名称（磁盘名称+p+分区数量）
        ret = snprintf_s(devName, sizeof(devName), sizeof(devName) - 1, "%s%c%u",
                         ((disk->disk_name == NULL) ? "null" : disk->disk_name), 'p', disk->part_count);
        if (ret < 0) {  // 格式化失败
            return VFS_ERROR;
        }

        // 注册块设备驱动
        if (register_blockdriver(devName, ((struct drv_data *)diskDev->data)->ops,
                                 RWE_RW_RW, ((struct drv_data *)diskDev->data)->priv)) {
            PRINT_ERR("DiskAddPart : register %s fail!\n", devName);
            return VFS_ERROR;  // 注册失败，返回错误码
        }

        VnodeHold();  // 持有Vnode锁
        VnodeLookup(devName, &partDev, 0);  // 查找分区设备节点

        // 分配并初始化分区
        part = DiskPartAllocate(partDev, sectorStart, sectorCount);
        VnodeDrop();  // 释放Vnode锁
        if (part == NULL) {  // 分区分配失败
            (VOID)unregister_blockdriver(devName);  // 注销已注册的块设备
            return VFS_ERROR;
        }
    } else {  // 非有效分区，直接分配分区（不注册设备）
        part = DiskPartAllocate(diskDev, sectorStart, sectorCount);
        if (part == NULL) {  // 分区分配失败
            return VFS_ERROR;
        }
    }

    DiskPartAddToDisk(disk, part);  // 将分区添加到磁盘的分区链表
    if (disk->type == EMMC) {       // 如果磁盘类型是EMMC
        part->type = EMMC;          // 设置分区类型为EMMC
    }
    return (INT32)part->part_id;  // 返回分区ID
}

// 磁盘分区划分
// 参数：disk - 磁盘结构体指针，info - 分区信息结构体指针
// 返回值：成功返回ENOERR，失败返回VFS_ERROR
static INT32 DiskDivide(los_disk *disk, struct disk_divide_info *info)
{
    UINT32 i;       // 循环计数器
    INT32 ret;      // 返回值

    disk->type = info->part[0].type;  // 设置磁盘类型为第一个分区的类型
    // 遍历所有分区信息
    for (i = 0; i < info->part_count; i++) {
        // 检查分区起始扇区是否超过磁盘总扇区数
        if (info->sector_count < info->part[i].sector_start) {
            return VFS_ERROR;  // 起始扇区无效，返回错误码
        }
        // 检查分区扇区数量是否超过剩余扇区数
        if (info->part[i].sector_count > (info->sector_count - info->part[i].sector_start)) {
            PRINT_ERR("Part[%u] sector_start:%llu, sector_count:%llu, exceed emmc sector_count:%llu.\n", i,
                      info->part[i].sector_start, info->part[i].sector_count,
                      (info->sector_count - info->part[i].sector_start));
            // 调整分区扇区数量为剩余扇区数
            info->part[i].sector_count = info->sector_count - info->part[i].sector_start;
            PRINT_ERR("Part[%u] sector_count change to %llu.\n", i, info->part[i].sector_count);

            // 添加调整后的分区
            ret = DiskAddPart(disk, info->part[i].sector_start, info->part[i].sector_count, TRUE);
            if (ret == VFS_ERROR) {
                return VFS_ERROR;  // 添加失败，返回错误码
            }
            break;  // 调整后只添加当前分区，跳出循环
        }
        // 添加分区
        ret = DiskAddPart(disk, info->part[i].sector_start, info->part[i].sector_count, TRUE);
        if (ret == VFS_ERROR) {
            return VFS_ERROR;  // 添加失败，返回错误码
        }
    }

    return ENOERR;  // 所有分区添加成功
}

// GPT分区类型识别
// 参数：parBuf - 分区数据缓冲区指针
// 返回值：成功返回文件系统类型（BS_FS_TYPE_FAT/BS_FS_TYPE_NTFS），失败返回ENOERR
static CHAR GPTPartitionTypeRecognition(const CHAR *parBuf)
{
    const CHAR *buf = parBuf;                       // 分区数据缓冲区指针
    const CHAR *fsType = "FAT";                     // FAT文件系统标识字符串
    const CHAR *str = "\xEB\x52\x90" "NTFS    ";  /* NTFS引导扇区特征码 */

    // 检查是否为FAT文件系统（通过32位文件系统类型或字符串标识）
    if (((LD_DWORD_DISK(&buf[BS_FILSYSTEMTYPE32]) & BS_FS_TYPE_MASK) == BS_FS_TYPE_VALUE) ||
        (strncmp(&buf[BS_FILSYSTYPE], fsType, strlen(fsType)) == 0)) {
        return BS_FS_TYPE_FAT;  // 返回FAT类型
    } else if (strncmp(&buf[BS_JMPBOOT], str, strlen(str)) == 0) {  // 检查是否为NTFS文件系统
        return BS_FS_TYPE_NTFS;  // 返回NTFS类型
    }

    return ENOERR;  // 无法识别的类型
}

// 分配并初始化GPT分区识别所需的缓冲区
// 参数：boundary - 内存对齐边界，size - 缓冲区大小，gptBuf - GPT缓冲区指针，partitionBuf - 分区数据缓冲区指针
// 返回值：成功返回ENOERR，失败返回-ENOMEM
static INT32 DiskPartitionMemZalloc(size_t boundary, size_t size, CHAR **gptBuf, CHAR **partitionBuf)
{
    CHAR *buffer1 = NULL;  // GPT缓冲区
    CHAR *buffer2 = NULL;  // 分区数据缓冲区

    // 分配对齐的内存用于GPT缓冲区
    buffer1 = (CHAR *)memalign(boundary, size);
    if (buffer1 == NULL) {
        PRINT_ERR("%s buffer1 malloc %lu failed! %d\n", __FUNCTION__, size, __LINE__);
        return -ENOMEM;  // 分配失败
    }
    // 分配对齐的内存用于分区数据缓冲区
    buffer2 = (CHAR *)memalign(boundary, size);
    if (buffer2 == NULL) {
        PRINT_ERR("%s buffer2 malloc %lu failed! %d\n", __FUNCTION__, size, __LINE__);
        free(buffer1);  // 释放已分配的第一个缓冲区
        return -ENOMEM;  // 分配失败
    }
    (VOID)memset_s(buffer1, size, 0, size);  // 初始化GPT缓冲区为0
    (VOID)memset_s(buffer2, size, 0, size);  // 初始化分区数据缓冲区为0

    *gptBuf = buffer1;        // 设置GPT缓冲区指针
    *partitionBuf = buffer2;  // 设置分区数据缓冲区指针

    return ENOERR;  // 成功分配并初始化
}

// 获取GPT分区表信息
// 参数：blkDrv - 块设备节点指针，gptBuf - 存储GPT信息的缓冲区
// 返回值：成功返回ENOERR，失败返回-EIO或VFS_ERROR
static INT32 GPTInfoGet(struct Vnode *blkDrv, CHAR *gptBuf)
{
    INT32 ret;  // 返回值

    // 获取块设备操作函数集
    struct block_operations *bops = (struct block_operations *)((struct drv_data *)blkDrv->data)->ops;

    // 读取设备的第1个扇区（GPT分区表通常位于此处）
    ret = bops->read(blkDrv, (UINT8 *)gptBuf, 1, 1); /* Read the device first sector */
    if (ret != 1) { /* 读取失败（未读取到1个扇区） */
        PRINT_ERR("%s %d\n", __FUNCTION__, __LINE__);
        return -EIO;  // 返回I/O错误
    }

    // 验证GPT签名是否有效
    if (!VERIFY_GPT(gptBuf)) {
        PRINT_ERR("%s %d\n", __FUNCTION__, __LINE__);
        return VFS_ERROR;  // GPT无效，返回错误码
    }

    return ENOERR;  // 成功获取并验证GPT信息
}

// GPT分区识别子函数（处理单个分区）
// 参数：info - 分区信息结构体指针，partitionBuf - 分区数据缓冲区，partitionCount - 分区计数器，partitionStart - 分区起始扇区，partitionEnd - 分区结束扇区
// 返回值：成功返回ENOERR，失败返回VFS_ERROR
static INT32 OsGPTPartitionRecognitionSub(struct disk_divide_info *info, const CHAR *partitionBuf,
                                          UINT32 *partitionCount, UINT64 partitionStart, UINT64 partitionEnd)
{
    CHAR partitionType;  // 分区类型

    if (VERIFY_FS(partitionBuf)) {  // 验证文件系统是否支持
        partitionType = GPTPartitionTypeRecognition(partitionBuf);  // 识别分区类型
        if (partitionType) {  // 如果识别到有效类型
            // 检查分区数量是否超过最大限制
            if (*partitionCount >= MAX_DIVIDE_PART_PER_DISK) {
                return VFS_ERROR;  // 超过最大分区数，返回错误码
            }
            // 设置分区信息
            info->part[*partitionCount].type = partitionType;
            info->part[*partitionCount].sector_start = partitionStart;
            info->part[*partitionCount].sector_count = (partitionEnd - partitionStart) + 1;  // 计算扇区数量
            (*partitionCount)++;  // 分区计数器加1
        } else {
            PRINT_ERR("The partition type is not allowed to use!\n");  // 不支持的分区类型
        }
    } else {
        PRINT_ERR("Do not support the partition type!\n");  // 文件系统验证失败
    }
    return ENOERR;  // 处理完成
}

// GPT分区识别（处理GPT分区表项）
// 参数：blkDrv - 块设备节点指针，info - 分区信息结构体指针，gptBuf - GPT缓冲区，partitionBuf - 分区数据缓冲区，partitionCount - 分区计数器
// 返回值：成功返回ENOERR，失败返回VFS_ERROR或-EIO
static INT32 OsGPTPartitionRecognition(struct Vnode *blkDrv, struct disk_divide_info *info,
                                       const CHAR *gptBuf, CHAR *partitionBuf, UINT32 *partitionCount)
{
    UINT32 j;  // 循环计数器
    INT32 ret = VFS_ERROR;  // 返回值
    UINT64 partitionStart, partitionEnd;  // 分区起始和结束扇区
    struct block_operations *bops = NULL;  // 块设备操作函数集

    // 遍历扇区内的所有GPT分区表项
    for (j = 0; j < PAR_ENTRY_NUM_PER_SECTOR; j++) {
        // 跳过ESP或MSR分区（不可用分区）
        if (!VERITY_AVAILABLE_PAR(&gptBuf[j * TABLE_SIZE])) {
            PRINTK("The partition type is ESP or MSR!\n");
            continue;
        }

        // 验证分区表项是否有效
        if (!VERITY_PAR_VALID(&gptBuf[j * TABLE_SIZE])) {
            return VFS_ERROR;  // 分区表项无效，返回错误码
        }

        // 读取分区起始和结束扇区（从GPT表项中）
        partitionStart = LD_QWORD_DISK(&gptBuf[(j * TABLE_SIZE) + GPT_PAR_START_OFFSET]);
        partitionEnd = LD_QWORD_DISK(&gptBuf[(j * TABLE_SIZE) + GPT_PAR_END_OFFSET]);
        // 检查分区范围是否有效
        if ((partitionStart >= partitionEnd) || (partitionEnd > info->sector_count)) {
            PRINT_ERR("GPT partition %u recognition failed : partitionStart = %llu, partitionEnd = %llu\n",
                      j, partitionStart, partitionEnd);
            return VFS_ERROR;  // 分区范围无效，返回错误码
        }

        (VOID)memset_s(partitionBuf, info->sector_size, 0, info->sector_size);  // 清空分区数据缓冲区

        bops = (struct block_operations *)((struct drv_data *)blkDrv->data)->ops;  // 获取块设备操作函数

        // 读取分区的第一个扇区数据（用于识别文件系统类型）
        ret = bops->read(blkDrv, (UINT8 *)partitionBuf, partitionStart, 1);
        if (ret != 1) { /* 读取失败 */
            PRINT_ERR("%s %d\n", __FUNCTION__, __LINE__);
            return -EIO;  // 返回I/O错误
        }

        // 处理当前分区（识别类型并添加到分区信息）
        ret = OsGPTPartitionRecognitionSub(info, partitionBuf, partitionCount, partitionStart, partitionEnd);
        if (ret != ENOERR) {
            return VFS_ERROR;  // 处理失败，返回错误码
        }
    }

    return ret;  // 所有分区表项处理完成
}

// GPT磁盘分区识别主函数
// 参数：blkDrv - 块设备节点指针，info - 分区信息结构体指针
// 返回值：成功返回识别到的分区数量，失败返回错误码
static INT32 DiskGPTPartitionRecognition(struct Vnode *blkDrv, struct disk_divide_info *info)
{
    CHAR *gptBuf = NULL;        // GPT缓冲区
    CHAR *partitionBuf = NULL;  // 分区数据缓冲区
    UINT32 tableNum, i, index;  // tableNum:分区表项数量，i:循环计数器，index:扇区索引
    UINT32 partitionCount = 0;  // 分区计数器
    INT32 ret;                  // 返回值

    // 分配并初始化GPT和分区数据缓冲区
    ret = DiskPartitionMemZalloc(MEM_ADDR_ALIGN_BYTE, info->sector_size, &gptBuf, &partitionBuf);
    if (ret != ENOERR) {
        return ret;  // 分配失败，返回错误码
    }

    // 获取并验证GPT信息
    ret = GPTInfoGet(blkDrv, gptBuf);
    if (ret < 0) {
        goto OUT_WITH_MEM;  // 获取失败，跳转到内存释放
    }

    // 读取分区表项数量（从GPT头中）
    tableNum = LD_DWORD_DISK(&gptBuf[TABLE_NUM_OFFSET]);
    if (tableNum > TABLE_MAX_NUM) {  // 限制最大分区表项数量
        tableNum = TABLE_MAX_NUM;
    }

    // 计算需要读取的GPT扇区数（向上取整）
    index = (tableNum % PAR_ENTRY_NUM_PER_SECTOR) ? ((tableNum / PAR_ENTRY_NUM_PER_SECTOR) + 1) :
            (tableNum / PAR_ENTRY_NUM_PER_SECTOR);

    // 遍历所有GPT扇区
    for (i = 0; i < index; i++) {
        (VOID)memset_s(gptBuf, info->sector_size, 0, info->sector_size);  // 清空GPT缓冲区
        // 获取块设备操作函数集
        struct block_operations *bops = (struct block_operations *)((struct drv_data *)blkDrv->data)->ops;
        // 读取GPT分区表扇区（从TABLE_START_SECTOR开始）
        ret = bops->read(blkDrv, (UINT8 *)gptBuf, TABLE_START_SECTOR + i, 1);
        if (ret != 1) { /* 读取失败 */
            PRINT_ERR("%s %d\n", __FUNCTION__, __LINE__);
            ret = -EIO;  // 设置I/O错误
            goto OUT_WITH_MEM;  // 跳转到内存释放
        }

        // 识别当前扇区中的GPT分区表项
        ret = OsGPTPartitionRecognition(blkDrv, info, gptBuf, partitionBuf, &partitionCount);
        if (ret < 0) {
            if (ret == VFS_ERROR) {
                ret = (INT32)partitionCount;  // 返回已识别的分区数量
            }
            goto OUT_WITH_MEM;  // 跳转到内存释放
        }
    }
    ret = (INT32)partitionCount;  // 设置返回值为识别到的分区数量

OUT_WITH_MEM:  // 内存释放标签
    free(gptBuf);       // 释放GPT缓冲区
    free(partitionBuf); // 释放分区数据缓冲区
    return ret;         // 返回结果
}
/**
 * @brief   获取MBR（主引导记录）信息
 * @param   blkDrv  块设备Vnode指针
 * @param   mbrBuf  存储MBR数据的缓冲区
 * @return  成功返回ENOERR，失败返回错误码
 */
static INT32 OsMBRInfoGet(struct Vnode *blkDrv, CHAR *mbrBuf)
{
    INT32 ret;

    /* 读取MBR，从0号扇区开始，长度为1个扇区 */
    struct block_operations *bops = (struct block_operations *)((struct drv_data *)blkDrv->data)->ops;

    ret = bops->read(blkDrv, (UINT8 *)mbrBuf, 0, 1);
    if (ret != 1) { /* 读取失败 */
        PRINT_ERR("driver read return error: %d\n", ret);
        return -EIO;
    }

    /* 检查引导记录签名 */
    if (LD_WORD_DISK(&mbrBuf[BS_SIG55AA]) != BS_SIG55AA_VALUE) {
        return VFS_ERROR;
    }

    return ENOERR;
}

/**
 * @brief   获取EBR（扩展引导记录）信息
 * @param   blkDrv  块设备Vnode指针
 * @param   info    磁盘分区信息结构体指针
 * @param   ebrBuf  存储EBR数据的缓冲区
 * @param   mbrBuf  MBR数据缓冲区
 * @return  成功返回ENOERR，失败返回错误码
 */
static INT32 OsEBRInfoGet(struct Vnode *blkDrv, const struct disk_divide_info *info,
                          CHAR *ebrBuf, const CHAR *mbrBuf)
{
    INT32 ret;

    if (VERIFY_FS(mbrBuf)) {
        if (info->sector_count <= LD_DWORD_DISK(&mbrBuf[PAR_OFFSET + PAR_START_OFFSET])) {
            return VFS_ERROR;
        }

        struct block_operations *bops = (struct block_operations *)((struct drv_data *)blkDrv->data)->ops;
        ret = bops->read(blkDrv, (UINT8 *)ebrBuf, LD_DWORD_DISK(&mbrBuf[PAR_OFFSET + PAR_START_OFFSET]), 1);
        if ((ret != 1) || (!VERIFY_FS(ebrBuf))) { /* 读取失败 */
            PRINT_ERR("OsEBRInfoGet, verify_fs error, ret = %d\n", ret);
            return -EIO;
        }
    }

    return ENOERR;
}

/**
 * @brief   识别主分区信息
 * @param   mbrBuf      MBR数据缓冲区
 * @param   info        磁盘分区信息结构体指针
 * @param   extendedPos 扩展分区位置指针
 * @param   mbrCount    主分区数量指针
 * @return  存在扩展分区返回1，否则返回0
 */
static INT32 OsPrimaryPartitionRecognition(const CHAR *mbrBuf, struct disk_divide_info *info,
                                           INT32 *extendedPos, INT32 *mbrCount)
{
    INT32 i;
    CHAR mbrPartitionType;
    INT32 extendedFlag = 0;  // 扩展分区标志
    INT32 count = 0;         // 主分区计数

    for (i = 0; i < MAX_PRIMARY_PART_PER_DISK; i++) {
        mbrPartitionType = mbrBuf[PAR_OFFSET + PAR_TYPE_OFFSET + (i * PAR_TABLE_SIZE)];
        if (mbrPartitionType) {
            info->part[i].type = mbrPartitionType;
            info->part[i].sector_start = LD_DWORD_DISK(&mbrBuf[PAR_OFFSET + PAR_START_OFFSET + (i * PAR_TABLE_SIZE)]);
            info->part[i].sector_count = LD_DWORD_DISK(&mbrBuf[PAR_OFFSET + PAR_COUNT_OFFSET + (i * PAR_TABLE_SIZE)]);
            // 检查是否为扩展分区
            if ((mbrPartitionType == EXTENDED_PAR) || (mbrPartitionType == EXTENDED_8G)) {
                extendedFlag = 1;
                *extendedPos = i;
                continue;
            }
            count++;
        }
    }
    *mbrCount = count;

    return extendedFlag;
}

/**
 * @brief   识别逻辑分区信息
 * @param   blkDrv          块设备Vnode指针
 * @param   info            磁盘分区信息结构体指针
 * @param   extendedAddress 扩展分区起始地址
 * @param   ebrBuf          EBR数据缓冲区
 * @param   mbrCount        主分区数量
 * @return  成功返回逻辑分区数量，失败返回错误码
 */
static INT32 OsLogicalPartitionRecognition(struct Vnode *blkDrv, struct disk_divide_info *info,
                                           UINT32 extendedAddress, CHAR *ebrBuf, INT32 mbrCount)
{
    INT32 ret;
    UINT32 extendedOffset = 0;  // 扩展分区偏移量
    CHAR ebrPartitionType;
    INT32 ebrCount = 0;         // 逻辑分区计数

    do {
        (VOID)memset_s(ebrBuf, info->sector_size, 0, info->sector_size);
        // 检查扩展分区是否超出磁盘范围
        if (((UINT64)(extendedAddress) + extendedOffset) >= info->sector_count) {
            PRINT_ERR("extended partition is out of disk range: extendedAddress = %u, extendedOffset = %u\n",
                      extendedAddress, extendedOffset);
            break;
        }
        struct block_operations *bops = (struct block_operations *)((struct drv_data *)blkDrv->data)->ops;
        ret = bops->read(blkDrv, (UINT8 *)ebrBuf, extendedAddress + extendedOffset, 1);
        if (ret != 1) { /* 读取失败 */
            PRINT_ERR("driver read return error: %d, extendedAddress = %u, extendedOffset = %u\n", ret,
                      extendedAddress, extendedOffset);
            return -EIO;
        }
        ebrPartitionType = ebrBuf[PAR_OFFSET + PAR_TYPE_OFFSET];
        // 检查分区类型并确保不超过最大分区数
        if (ebrPartitionType && ((mbrCount + ebrCount) < MAX_DIVIDE_PART_PER_DISK)) {
            info->part[MAX_PRIMARY_PART_PER_DISK + ebrCount].type = ebrPartitionType;
            info->part[MAX_PRIMARY_PART_PER_DISK + ebrCount].sector_start = extendedAddress + extendedOffset +
                                                                            LD_DWORD_DISK(&ebrBuf[PAR_OFFSET +
                                                                                                  PAR_START_OFFSET]);
            info->part[MAX_PRIMARY_PART_PER_DISK + ebrCount].sector_count = LD_DWORD_DISK(&ebrBuf[PAR_OFFSET +
                                                                                                  PAR_COUNT_OFFSET]);
            ebrCount++;
        }
        // 获取下一个EBR的偏移量
        extendedOffset = LD_DWORD_DISK(&ebrBuf[PAR_OFFSET + PAR_START_OFFSET + PAR_TABLE_SIZE]);
    } while ((ebrBuf[PAR_OFFSET + PAR_TYPE_OFFSET + PAR_TABLE_SIZE] != 0) &&
             ((mbrCount + ebrCount) < MAX_DIVIDE_PART_PER_DISK));

    return ebrCount;
}

/**
 * @brief   磁盘分区识别主函数
 * @param   blkDrv  块设备Vnode指针
 * @param   info    磁盘分区信息结构体指针
 * @return  成功返回分区总数，失败返回错误码
 */
static INT32 DiskPartitionRecognition(struct Vnode *blkDrv, struct disk_divide_info *info)
{
    INT32 ret;
    INT32 extendedFlag;         // 扩展分区标志
    INT32 extendedPos = 0;      // 扩展分区位置
    INT32 mbrCount = 0;         // 主分区数量
    UINT32 extendedAddress;     // 扩展分区起始地址
    CHAR *mbrBuf = NULL;        // MBR缓冲区
    CHAR *ebrBuf = NULL;        // EBR缓冲区

    if (blkDrv == NULL) {
        return -EINVAL;
    }

    struct block_operations *bops = (struct block_operations *)((struct drv_data *)blkDrv->data)->ops;

    if ((bops == NULL) || (bops->read == NULL)) {
        return -EINVAL;
    }

    // 分配MBR和EBR缓冲区
    ret = DiskPartitionMemZalloc(MEM_ADDR_ALIGN_BYTE, info->sector_size, &mbrBuf, &ebrBuf);
    if (ret != ENOERR) {
        return ret;
    }

    // 获取MBR信息
    ret = OsMBRInfoGet(blkDrv, mbrBuf);
    if (ret < 0) {
        goto OUT_WITH_MEM;  // 跳转到内存释放处
    }

    /* 分区类型为GPT */
    if (mbrBuf[PARTION_MODE_BTYE] == (CHAR)PARTION_MODE_GPT) {
        ret = DiskGPTPartitionRecognition(blkDrv, info);
        goto OUT_WITH_MEM;
    }

    // 获取EBR信息
    ret = OsEBRInfoGet(blkDrv, info, ebrBuf, mbrBuf);
    if (ret < 0) {
        ret = 0; /* 没有mbr */
        goto OUT_WITH_MEM;
    }

    // 识别主分区
    extendedFlag = OsPrimaryPartitionRecognition(mbrBuf, info, &extendedPos, &mbrCount);
    if (extendedFlag) {
        extendedAddress = LD_DWORD_DISK(&mbrBuf[PAR_OFFSET + PAR_START_OFFSET + (extendedPos * PAR_TABLE_SIZE)]);
        // 识别逻辑分区
        ret = OsLogicalPartitionRecognition(blkDrv, info, extendedAddress, ebrBuf, mbrCount);
        if (ret <= 0) {
            goto OUT_WITH_MEM;
        }
    }
    ret += mbrCount;  // 总分区数 = 主分区数 + 逻辑分区数

OUT_WITH_MEM:  // 内存释放标签
    free(ebrBuf);
    free(mbrBuf);
    return ret;
}

/**
 * @brief   磁盘分区注册函数
 * @param   disk    磁盘结构体指针
 * @return  成功返回ENOERR，失败返回错误码
 */
INT32 DiskPartitionRegister(los_disk *disk)
{
    INT32 count;
    UINT32 i, partSize;
    los_part *part = NULL;
    struct disk_divide_info parInfo;  // 分区信息结构体

    /* 填充disk_divide_info结构体以设置分区信息 */
    (VOID)memset_s(parInfo.part, sizeof(parInfo.part), 0, sizeof(parInfo.part));
    partSize = sizeof(parInfo.part) / sizeof(parInfo.part[0]);

    parInfo.sector_size = disk->sector_size;
    parInfo.sector_count = disk->sector_count;
    // 识别磁盘分区
    count = DiskPartitionRecognition(disk->dev, &parInfo);
    if (count == VFS_ERROR) {
        // 添加整个磁盘作为一个分区
        part = get_part(DiskAddPart(disk, 0, disk->sector_count, FALSE));
        if (part == NULL) {
            return VFS_ERROR;
        }
        part->part_no_mbr = 0;
        PRINTK("Disk %s doesn't contain a valid partition table.\n", disk->disk_name);
        return ENOERR;
    } else if (count < 0) {
        return VFS_ERROR;
    }

    parInfo.part_count = count;
    if (count == 0) {
        // 没有检测到分区，添加整个磁盘作为一个分区
        part = get_part(DiskAddPart(disk, 0, disk->sector_count, TRUE));
        if (part == NULL) {
            return VFS_ERROR;
        }
        part->part_no_mbr = 0;

        PRINTK("No MBR detected.\n");
        return ENOERR;
    }

    // 遍历所有分区信息并注册
    for (i = 0; i < partSize; i++) {
        /* 读取disk_divide_info结构体获取分区信息 */
        if ((parInfo.part[i].type != 0) && (parInfo.part[i].type != EXTENDED_PAR) &&
            (parInfo.part[i].type != EXTENDED_8G)) {
            part = get_part(DiskAddPart(disk, parInfo.part[i].sector_start, parInfo.part[i].sector_count, TRUE));
            if (part == NULL) {
                return VFS_ERROR;
            }
            part->part_no_mbr = i + 1;
            part->filesystem_type = parInfo.part[i].type;
        }
    }

    return ENOERR;
}

#ifndef LOSCFG_FS_FAT_CACHE
/**
 * @brief   直接读取磁盘数据（不使用缓存）
 * @param   disk    磁盘结构体指针
 * @param   buf     数据缓冲区
 * @param   sector  起始扇区
 * @param   count   扇区数量
 * @return  成功返回ENOERR，失败返回错误码
 */
static INT32 disk_read_directly(los_disk *disk, VOID *buf, UINT64 sector, UINT32 count)
{
    INT32 result = VFS_ERROR;
    struct block_operations *bops = (struct block_operations *)((struct drv_data *)disk->dev->data)->ops;
    if ((bops == NULL) || (bops->read == NULL)) {
        return VFS_ERROR;
    }
    // 检查是否为用户空间地址
    if (LOS_IsUserAddressRange((VADDR_T)buf, count * disk->sector_size)) {
        UINT32 cnt = 0;
        UINT8 *buffer = disk->buff;  // 使用内核缓冲区
        for (; count != 0; count -= cnt) {
            // 计算每次读取的扇区数（不超过缓冲区大小）
            cnt = (count > DISK_DIRECT_BUFFER_SIZE)? DISK_DIRECT_BUFFER_SIZE : count;
            result = bops->read(disk->dev, buffer, sector, cnt);
            if (result == (INT32)cnt) {
                result = ENOERR;
            } else {
                break;
            }
            // 将数据从内核缓冲区复制到用户缓冲区
            if (LOS_CopyFromKernel(buf, disk->sector_size * cnt, buffer, disk->sector_size * cnt)) {
                result = VFS_ERROR;
                break;
            }
            buf = (UINT8 *)buf + disk->sector_size * cnt;
            sector += cnt;
        }
    } else {
        // 内核空间地址，直接读取
        result = bops->read(disk->dev, buf, sector, count);
        if (result == count) {
            result = ENOERR;
        }
    }

    return result;
}

/**
 * @brief   直接写入磁盘数据（不使用缓存）
 * @param   disk    磁盘结构体指针
 * @param   buf     数据缓冲区
 * @param   sector  起始扇区
 * @param   count   扇区数量
 * @return  成功返回ENOERR，失败返回错误码
 */
static INT32 disk_write_directly(los_disk *disk, const VOID *buf, UINT64 sector, UINT32 count)
{
    struct block_operations *bops = (struct block_operations *)((struct drv_data *)disk->dev->data)->ops;
    INT32 result = VFS_ERROR;
    if ((bops == NULL) || (bops->read == NULL)) {
        return VFS_ERROR;
    }
    // 检查是否为用户空间地址
    if (LOS_IsUserAddressRange((VADDR_T)buf, count * disk->sector_size)) {
        UINT32 cnt = 0;
        UINT8 *buffer = disk->buff;  // 使用内核缓冲区
        for (; count != 0; count -= cnt) {
            // 计算每次写入的扇区数（不超过缓冲区大小）
            cnt = (count > DISK_DIRECT_BUFFER_SIZE)? DISK_DIRECT_BUFFER_SIZE : count;
            // 将数据从用户缓冲区复制到内核缓冲区
            if (LOS_CopyToKernel(buffer, disk->sector_size * cnt, buf, disk->sector_size * cnt)) {
                result = VFS_ERROR;
                break;
            }
            result = bops->write(disk->dev, buffer, sector, cnt);
            if (result == (INT32)cnt) {
                result = ENOERR;
            } else {
                break;
            }
            buf = (UINT8 *)buf + disk->sector_size * cnt;
            sector += cnt;
        }
    } else {
        // 内核空间地址，直接写入
        result = bops->write(disk->dev, buf, sector, count);
        if (result == count) {
            result = ENOERR;
        }
    }

    return result;
}
#endif

/**
 * @brief   磁盘读取函数（支持缓存）
 * @param   drvID   驱动ID
 * @param   buf     数据缓冲区
 * @param   sector  起始扇区
 * @param   count   扇区数量
 * @param   useRead 是否使用读取操作
 * @return  成功返回ENOERR，失败返回错误码
 */
INT32 los_disk_read(INT32 drvID, VOID *buf, UINT64 sector, UINT32 count, BOOL useRead)
{
#ifdef LOSCFG_FS_FAT_CACHE
    UINT32 len;
#endif
    INT32 result = VFS_ERROR;
    los_disk *disk = get_disk(drvID);

    if ((buf == NULL) || (count == 0)) { /* 缓冲区为空或读取数量为0 */
        return result;
    }

    if (disk == NULL) {
        return result;
    }

    DISK_LOCK(&disk->disk_mutex);  // 加锁保护磁盘操作

    if (disk->disk_status != STAT_INUSED) {
        goto ERROR_HANDLE;  // 磁盘未使用，跳转到错误处理
    }

    // 检查扇区范围是否有效
    if ((count > disk->sector_count) || ((disk->sector_count - count) < sector)) {
        goto ERROR_HANDLE;
    }

#ifdef LOSCFG_FS_FAT_CACHE
    if (disk->bcache != NULL) {
        if (((UINT64)(disk->bcache->sectorSize) * count) > UINT_MAX) {
            goto ERROR_HANDLE;
        }
        len = disk->bcache->sectorSize * count;
        /* 读取大量连续数据时useRead应设为FALSE */
        result = BlockCacheRead(disk->bcache, (UINT8 *)buf, &len, sector, useRead);
        if (result != ENOERR) {
            PRINT_ERR("los_disk_read read err = %d, sector = %llu, len = %u\n", result, sector, len);
        }
    } else {
        result = VFS_ERROR;
    }
#else
    if (disk->dev == NULL) {
        goto ERROR_HANDLE;
    }
    result = disk_read_directly(disk, buf, sector, count);  // 直接读取磁盘数据
#endif
    if (result != ENOERR) {
        goto ERROR_HANDLE;
    }

    DISK_UNLOCK(&disk->disk_mutex);  // 解锁
    return ENOERR;

ERROR_HANDLE:  // 错误处理标签
    DISK_UNLOCK(&disk->disk_mutex);  // 解锁
    return VFS_ERROR;
}
/**
 * @brief   磁盘写入函数（支持缓存）
 * @param   drvID   驱动ID
 * @param   buf     待写入数据缓冲区
 * @param   sector  起始扇区
 * @param   count   扇区数量
 * @return  成功返回ENOERR，失败返回错误码
 */
INT32 los_disk_write(INT32 drvID, const VOID *buf, UINT64 sector, UINT32 count)
{
#ifdef LOSCFG_FS_FAT_CACHE
    UINT32 len;
#endif
    INT32 result = VFS_ERROR;
    los_disk *disk = get_disk(drvID);
    // 检查磁盘及设备有效性
    if (disk == NULL || disk->dev == NULL || disk->dev->data == NULL) {
        return result;
    }

    if ((buf == NULL) || (count == 0)) { /* 缓冲区为空或写入数量为0 */
        return result;
    }

    DISK_LOCK(&disk->disk_mutex);  // 加锁保护磁盘操作

    if (disk->disk_status != STAT_INUSED) {  // 检查磁盘状态是否可用
        goto ERROR_HANDLE;
    }

    // 检查扇区范围是否有效
    if ((count > disk->sector_count) || ((disk->sector_count - count) < sector)) {
        goto ERROR_HANDLE;
    }

#ifdef LOSCFG_FS_FAT_CACHE  // 启用FAT缓存时的处理
    if (disk->bcache != NULL) {
        if (((UINT64)(disk->bcache->sectorSize) * count) > UINT_MAX) {
            goto ERROR_HANDLE;
        }
        len = disk->bcache->sectorSize * count;
        result = BlockCacheWrite(disk->bcache, (const UINT8 *)buf, &len, sector);
        if (result != ENOERR) {
            PRINT_ERR("los_disk_write write err = %d, sector = %llu, len = %u\n", result, sector, len);
        }
    } else {
        result = VFS_ERROR;
    }
#else  // 不使用缓存时直接写入
    if (disk->dev == NULL) {
        goto ERROR_HANDLE;
    }
    result = disk_write_directly(disk, buf, sector, count);
#endif
    if (result != ENOERR) {
        goto ERROR_HANDLE;
    }

    DISK_UNLOCK(&disk->disk_mutex);  // 解锁
    return ENOERR;

ERROR_HANDLE:  // 错误处理标签
    DISK_UNLOCK(&disk->disk_mutex);  // 解锁
    return VFS_ERROR;
}

/**
 * @brief   磁盘I/O控制函数
 * @param   drvID   驱动ID
 * @param   cmd     控制命令
 * @param   buf     数据缓冲区
 * @return  成功返回ENOERR，失败返回错误码
 */
INT32 los_disk_ioctl(INT32 drvID, INT32 cmd, VOID *buf)
{
    struct geometry info;  // 磁盘几何信息结构体
    los_disk *disk = get_disk(drvID);
    if (disk == NULL) {
        return VFS_ERROR;
    }

    DISK_LOCK(&disk->disk_mutex);  // 加锁保护磁盘操作

    if ((disk->dev == NULL) || (disk->disk_status != STAT_INUSED)) {  // 检查设备和状态
        goto ERROR_HANDLE;
    }

    if (cmd == DISK_CTRL_SYNC) {  // 同步命令无需额外处理
        DISK_UNLOCK(&disk->disk_mutex);
        return ENOERR;
    }

    if (buf == NULL) {  // 缓冲区为空
        goto ERROR_HANDLE;
    }

    (VOID)memset_s(&info, sizeof(info), 0, sizeof(info));  // 初始化几何信息结构体

    struct block_operations *bops = (struct block_operations *)((struct drv_data *)disk->dev->data)->ops;
    // 检查块设备操作及几何信息获取函数
    if ((bops == NULL) || (bops->geometry == NULL) ||
        (bops->geometry(disk->dev, &info) != 0)) {
        goto ERROR_HANDLE;
    }

    if (cmd == DISK_GET_SECTOR_COUNT) {  // 获取扇区总数
        *(UINT64 *)buf = info.geo_nsectors;
        if (info.geo_nsectors == 0) {  // 扇区数无效
            goto ERROR_HANDLE;
        }
    } else if (cmd == DISK_GET_SECTOR_SIZE) {  // 获取扇区大小
        *(size_t *)buf = info.geo_sectorsize;
    } else if (cmd == DISK_GET_BLOCK_SIZE) { /* 获取擦除块大小（以扇区为单位） */
        /* SDHC块数 == 512，SD可设置为512或其他值 */
        *(size_t *)buf = DISK_MAX_SECTOR_SIZE / info.geo_sectorsize;
    } else {  // 不支持的命令
        goto ERROR_HANDLE;
    }

    DISK_UNLOCK(&disk->disk_mutex);  // 解锁
    return ENOERR;

ERROR_HANDLE:  // 错误处理标签
    DISK_UNLOCK(&disk->disk_mutex);  // 解锁
    return VFS_ERROR;
}

/**
 * @brief   分区读取函数
 * @param   pt      分区ID
 * @param   buf     数据缓冲区
 * @param   sector  起始扇区（分区内相对扇区）
 * @param   count   扇区数量
 * @param   useRead 是否使用读取操作
 * @return  成功返回ENOERR，失败返回错误码
 */
INT32 los_part_read(INT32 pt, VOID *buf, UINT64 sector, UINT32 count, BOOL useRead)
{
    const los_part *part = get_part(pt);
    los_disk *disk = NULL;
    INT32 ret;

    if (part == NULL) {  // 分区不存在
        return VFS_ERROR;
    }

    disk = get_disk((INT32)part->disk_id);  // 获取分区所属磁盘
    if (disk == NULL) {
        return VFS_ERROR;
    }

    DISK_LOCK(&disk->disk_mutex);  // 加锁保护磁盘操作
    if ((part->dev == NULL) || (disk->disk_status != STAT_INUSED)) {  // 检查设备和状态
        goto ERROR_HANDLE;
    }

    if (count > part->sector_count) {  // 读取数量超过分区总扇区数
        PRINT_ERR("los_part_read failed, invalid count, count = %u\n", count);
        goto ERROR_HANDLE;
    }

    /* 读取绝对扇区 */
    if (part->type == EMMC) {  // EMMC类型分区处理
        if ((disk->sector_count - part->sector_start) > sector) {
            sector += part->sector_start;  // 转换为磁盘绝对扇区
        } else {
            PRINT_ERR("los_part_read failed, invalid sector, sector = %llu\n", sector);
            goto ERROR_HANDLE;
        }
    }

    // 检查扇区范围是否在分区内
    if ((sector >= GetFirstPartStart(part)) &&
        (((sector + count) > (part->sector_start + part->sector_count)) || (sector < part->sector_start))) {
        PRINT_ERR("los_part_read error, sector = %llu, count = %u, part->sector_start = %llu, "
                  "part->sector_count = %llu\n", sector, count, part->sector_start, part->sector_count);
        goto ERROR_HANDLE;
    }

    /* 读取大量连续数据时useRead应设为FALSE */
    ret = los_disk_read((INT32)part->disk_id, buf, sector, count, useRead);
    if (ret < 0) {
        goto ERROR_HANDLE;
    }

    DISK_UNLOCK(&disk->disk_mutex);  // 解锁
    return ENOERR;

ERROR_HANDLE:  // 错误处理标签
    DISK_UNLOCK(&disk->disk_mutex);  // 解锁
    return VFS_ERROR;
}

/**
 * @brief   分区写入函数
 * @param   pt      分区ID
 * @param   buf     待写入数据缓冲区
 * @param   sector  起始扇区（分区内相对扇区）
 * @param   count   扇区数量
 * @return  成功返回ENOERR，失败返回错误码
 */
INT32 los_part_write(INT32 pt, const VOID *buf, UINT64 sector, UINT32 count)
{
    const los_part *part = get_part(pt);
    los_disk *disk = NULL;
    INT32 ret;

    if (part == NULL) {  // 分区不存在
        return VFS_ERROR;
    }

    disk = get_disk((INT32)part->disk_id);  // 获取分区所属磁盘
    if (disk == NULL) {
        return VFS_ERROR;
    }

    DISK_LOCK(&disk->disk_mutex);  // 加锁保护磁盘操作
    if ((part->dev == NULL) || (disk->disk_status != STAT_INUSED)) {  // 检查设备和状态
        goto ERROR_HANDLE;
    }

    if (count > part->sector_count) {  // 写入数量超过分区总扇区数
        PRINT_ERR("los_part_write failed, invalid count, count = %u\n", count);
        goto ERROR_HANDLE;
    }

    /* 写入绝对扇区 */
    if (part->type == EMMC) {  // EMMC类型分区处理
        if ((disk->sector_count - part->sector_start) > sector) {
            sector += part->sector_start;  // 转换为磁盘绝对扇区
        } else {
            PRINT_ERR("los_part_write failed, invalid sector, sector = %llu\n", sector);
            goto ERROR_HANDLE;
        }
    }

    // 检查扇区范围是否在分区内
    if ((sector >= GetFirstPartStart(part)) &&
        (((sector + count) > (part->sector_start + part->sector_count)) || (sector < part->sector_start))) {
        PRINT_ERR("los_part_write, sector = %llu, count = %u, part->sector_start = %llu, "
                  "part->sector_count = %llu\n", sector, count, part->sector_start, part->sector_count);
        goto ERROR_HANDLE;
    }

    ret = los_disk_write((INT32)part->disk_id, buf, sector, count);
    if (ret < 0) {
        goto ERROR_HANDLE;
    }

    DISK_UNLOCK(&disk->disk_mutex);  // 解锁
    return ENOERR;

ERROR_HANDLE:  // 错误处理标签
    DISK_UNLOCK(&disk->disk_mutex);  // 解锁
    return VFS_ERROR;
}

#define GET_ERASE_BLOCK_SIZE 0x2  // 获取擦除块大小命令

/**
 * @brief   分区I/O控制函数
 * @param   pt      分区ID
 * @param   cmd     控制命令
 * @param   buf     数据缓冲区
 * @return  成功返回ENOERR，失败返回错误码
 */
INT32 los_part_ioctl(INT32 pt, INT32 cmd, VOID *buf)
{
    struct geometry info;  // 磁盘几何信息结构体
    los_part *part = get_part(pt);
    los_disk *disk = NULL;

    if (part == NULL) {  // 分区不存在
        return VFS_ERROR;
    }

    disk = get_disk((INT32)part->disk_id);  // 获取分区所属磁盘
    if (disk == NULL) {
        return VFS_ERROR;
    }

    DISK_LOCK(&disk->disk_mutex);  // 加锁保护磁盘操作
    if ((part->dev == NULL) || (disk->disk_status != STAT_INUSED)) {  // 检查设备和状态
        goto ERROR_HANDLE;
    }

    if (cmd == DISK_CTRL_SYNC) {  // 同步命令无需额外处理
        DISK_UNLOCK(&disk->disk_mutex);
        return ENOERR;
    }

    if (buf == NULL) {  // 缓冲区为空
        goto ERROR_HANDLE;
    }

    (VOID)memset_s(&info, sizeof(info), 0, sizeof(info));  // 初始化几何信息结构体

    struct block_operations *bops = (struct block_operations *)((struct drv_data *)part->dev->data)->ops;
    // 检查块设备操作及几何信息获取函数
    if ((bops == NULL) || (bops->geometry == NULL) ||
        (bops->geometry(part->dev, &info) != 0)) {
        goto ERROR_HANDLE;
    }

    if (cmd == DISK_GET_SECTOR_COUNT) {  // 获取分区扇区总数
        *(UINT64 *)buf = part->sector_count;
        if (*(UINT64 *)buf == 0) {  // 扇区数无效
            goto ERROR_HANDLE;
        }
    } else if (cmd == DISK_GET_SECTOR_SIZE) {  // 获取扇区大小
        *(size_t *)buf = info.geo_sectorsize;
    } else if (cmd == DISK_GET_BLOCK_SIZE) { /* 获取擦除块大小（以扇区为单位） */
        if ((bops->ioctl == NULL) ||
            (bops->ioctl(part->dev, GET_ERASE_BLOCK_SIZE, (UINTPTR)buf) != 0)) {
            goto ERROR_HANDLE;
        }
    } else {  // 不支持的命令
        goto ERROR_HANDLE;
    }

    DISK_UNLOCK(&disk->disk_mutex);  // 解锁
    return ENOERR;

ERROR_HANDLE:  // 错误处理标签
    DISK_UNLOCK(&disk->disk_mutex);  // 解锁
    return VFS_ERROR;
}
/**
 * @brief 清除磁盘缓存
 * @param drvID 驱动ID
 * @return 成功返回ENOERR，失败返回错误码
 */
INT32 los_disk_cache_clear(INT32 drvID)
{
    INT32 result = ENOERR;  // 初始化结果为成功
#ifdef LOSCFG_FS_FAT_CACHE
    los_part *part = get_part(drvID);  // 根据驱动ID获取分区信息
    los_disk *disk = NULL;  // 磁盘指针初始化

    if (part == NULL) {  // 分区不存在时返回错误
        return VFS_ERROR;
    }
    result = OsSdSync(part->disk_id);  // 同步SD卡数据
    if (result != ENOERR) {  // 同步失败时打印错误信息并返回
        PRINTK("[ERROR]disk_cache_clear SD sync failed!\n");
        return result;
    }

    disk = get_disk(part->disk_id);  // 根据磁盘ID获取磁盘信息
    if (disk == NULL) {  // 磁盘不存在时返回错误
        return VFS_ERROR;
    }

    DISK_LOCK(&disk->disk_mutex);  // 加锁保护磁盘操作
    result = BcacheClearCache(disk->bcache);  // 清除块缓存
    DISK_UNLOCK(&disk->disk_mutex);  // 解锁磁盘
#endif
    return result;  // 返回操作结果
}

#ifdef LOSCFG_FS_FAT_CACHE
/**
 * @brief 初始化磁盘缓存线程
 * @param diskID 磁盘ID
 * @param bc 块缓存结构体指针
 */
static VOID DiskCacheThreadInit(UINT32 diskID, OsBcache *bc)
{
    bc->prereadFun = NULL;  // 预读函数指针初始化为空

    if (GetDiskUsbStatus(diskID) == FALSE) {  // 非USB磁盘时初始化预读线程
        if (BcacheAsyncPrereadInit(bc) == LOS_OK) {  // 初始化异步预读
            bc->prereadFun = ResumeAsyncPreread;  // 设置预读恢复函数
        }

#ifdef LOSCFG_FS_FAT_CACHE_SYNC_THREAD
        BcacheSyncThreadInit(bc, diskID);  // 初始化同步线程
#endif
    }

    if (OsReHookFuncAddDiskRef != NULL) {  // 如果注册了磁盘引用钩子函数
        (VOID)OsReHookFuncAddDiskRef((StorageHookFunction)OsSdSync, (VOID *)0);  // 添加同步钩子
        (VOID)OsReHookFuncAddDiskRef((StorageHookFunction)OsSdSync, (VOID *)1);  // 添加同步钩子
    }
}

/**
 * @brief 初始化磁盘缓存
 * @param diskID 磁盘ID
 * @param diskInfo 磁盘几何信息结构体
 * @param blkDriver 块设备Vnode指针
 * @return 成功返回块缓存结构体指针，失败返回NULL
 */
static OsBcache *DiskCacheInit(UINT32 diskID, const struct geometry *diskInfo, struct Vnode *blkDriver)
{
#define SECTOR_SIZE 512  // 定义扇区大小为512字节

    OsBcache *bc = NULL;  // 块缓存结构体指针初始化
    UINT32 sectorPerBlock = diskInfo->geo_sectorsize / SECTOR_SIZE;  // 计算每块扇区数
    if (sectorPerBlock != 0) {  // 扇区数有效时
        sectorPerBlock = g_uwFatSectorsPerBlock / sectorPerBlock;  // 计算每块扇区数
        if (sectorPerBlock != 0) {  // 扇区数有效时初始化块缓存
            bc = BlockCacheInit(blkDriver, diskInfo->geo_sectorsize, sectorPerBlock,
                                g_uwFatBlockNums, diskInfo->geo_nsectors / sectorPerBlock);
        }
    }

    if (bc == NULL) {  // 块缓存初始化失败时打印错误信息
        PRINT_ERR("disk_init : disk have not init bcache cache!\n");
        return NULL;
    }

    DiskCacheThreadInit(diskID, bc);  // 初始化缓存线程
    return bc;  // 返回块缓存结构体指针
}

/**
 * @brief 反初始化磁盘缓存
 * @param disk 磁盘结构体指针
 */
static VOID DiskCacheDeinit(los_disk *disk)
{
    UINT32 diskID = disk->disk_id;  // 获取磁盘ID
    if (GetDiskUsbStatus(diskID) == FALSE) {  // 非USB磁盘时反初始化预读线程
        if (BcacheAsyncPrereadDeinit(disk->bcache) != LOS_OK) {  // 反初始化异步预读
            PRINT_ERR("Blib async preread deinit failed in %s, %d\n", __FUNCTION__, __LINE__);
        }
#ifdef LOSCFG_FS_FAT_CACHE_SYNC_THREAD
        BcacheSyncThreadDeinit(disk->bcache);  // 反初始化同步线程
#endif
    }

    BlockCacheDeinit(disk->bcache);  // 反初始化块缓存
    disk->bcache = NULL;  // 块缓存指针置空

    if (OsReHookFuncDelDiskRef != NULL) {  // 如果注册了磁盘引用钩子函数
        (VOID)OsReHookFuncDelDiskRef((StorageHookFunction)OsSdSync);  // 删除同步钩子
    }
}
#endif

/**
 * @brief 初始化磁盘结构体
 * @param diskName 磁盘名称
 * @param diskID 磁盘ID
 * @param diskInfo 磁盘几何信息结构体
 * @param blkDriver 块设备Vnode指针
 * @param disk 磁盘结构体指针
 */
static VOID DiskStructInit(const CHAR *diskName, INT32 diskID, const struct geometry *diskInfo,
                           struct Vnode *blkDriver, los_disk *disk)
{
    size_t nameLen;  // 磁盘名称长度
    disk->disk_id = diskID;  // 设置磁盘ID
    disk->dev = blkDriver;  // 设置块设备Vnode
    disk->sector_start = 0;  // 设置起始扇区
    disk->sector_size = diskInfo->geo_sectorsize;  // 设置扇区大小
    disk->sector_count = diskInfo->geo_nsectors;  // 设置扇区总数

    nameLen = strlen(diskName); /* 调用者los_disk_init已检查名称 */

    if (disk->disk_name != NULL) {  // 如果磁盘名称已存在则释放内存
        LOS_MemFree(m_aucSysMem0, disk->disk_name);
        disk->disk_name = NULL;
    }

    disk->disk_name = LOS_MemAlloc(m_aucSysMem0, (nameLen + 1));  // 分配磁盘名称内存
    if (disk->disk_name == NULL) {  // 内存分配失败时打印错误信息
        PRINT_ERR("DiskStructInit alloc memory failed.\n");
        return;
    }

    if (strncpy_s(disk->disk_name, (nameLen + 1), diskName, nameLen) != EOK) {  // 复制磁盘名称
        PRINT_ERR("DiskStructInit strncpy_s failed.\n");
        LOS_MemFree(m_aucSysMem0, disk->disk_name);  // 复制失败时释放内存
        disk->disk_name = NULL;
        return;
    }
    disk->disk_name[nameLen] = '\0';  // 添加字符串结束符
    LOS_ListInit(&disk->head);  // 初始化磁盘分区链表头
}

/**
 * @brief 磁盘分区划分和注册
 * @param info 磁盘分区信息结构体
 * @param disk 磁盘结构体指针
 * @return 成功返回ENOERR，失败返回错误码
 */
static INT32 DiskDivideAndPartitionRegister(struct disk_divide_info *info, los_disk *disk)
{
    INT32 ret;  // 返回值

    if (info != NULL) {  // 如果分区信息存在则划分分区
        ret = DiskDivide(disk, info);
        if (ret != ENOERR) {  // 划分失败时打印错误信息
            PRINT_ERR("DiskDivide failed, ret = %d\n", ret);
            return ret;
        }
    } else {  // 否则注册分区
        ret = DiskPartitionRegister(disk);
        if (ret != ENOERR) {  // 注册失败时打印错误信息
            PRINT_ERR("DiskPartitionRegister failed, ret = %d\n", ret);
            return ret;
        }
    }
    return ENOERR;  // 返回成功
}

/**
 * @brief 反初始化磁盘
 * @param disk 磁盘结构体指针
 * @return 成功返回ENOERR，失败返回错误码
 */
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
            DiskPartDelFromDisk(disk, part);
            (VOID)unregister_blockdriver(devName);
            DiskPartRelease(part);

            part = LOS_DL_LIST_ENTRY(disk->head.pstNext, los_part, list);
        }
    }

    DISK_LOCK(&disk->disk_mutex);

#ifdef LOSCFG_FS_FAT_CACHE
    DiskCacheDeinit(disk);
#else
    if (disk->buff != NULL) {
        free(disk->buff);
    }
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

/**
 * @brief 磁盘初始化子函数
 * @param diskName 磁盘名称
 * @param diskID 磁盘ID
td> * @param disk 磁盘结构体指针
 * @param disktd>Info 磁盘几何信息结构体
 * @param blkDriver 块设备Vnode指针
 * @return 成功返回ENOERR，失败返回错误码
 */
static UINT32 OsDiskInitSub(const CHAR *diskName, INT32 diskID, los_disk *disk,
                            struct geometry *diskInfo, struct Vnode *blkDriver)
{
    pthread_mutexattr_t attr;  // 互斥锁属性
#ifdef LOSCFG_FS_FAT_CACHE
    OsBcache *bc = DiskCacheInit((UINT32)diskID, diskInfo, blkDriver);  // 初始化磁盘缓存
    if (bc == NULL) {  // 缓存初始化失败返回错误
        return VFS_ERROR;
    }
    disk->bcache = bc;  // 设置块缓存指针
#endif

    (VOID)pthread_mutexattr_init(&attr);  // 初始化互斥锁属性
    attr.type = PTHREAD_MUTEX_RECURSIVE;  // 设置互斥锁为可重入
    (VOID)pthread_mutex_init(&disk->disk_mutex, &attr);  // 初始化互斥锁

    DiskStructInit(diskName, diskID, diskInfo, blkDriver, disk);  // 初始化磁盘结构体

#ifndef LOSCFG_FS_FAT_CACHE
    disk->buff = malloc(diskInfo->geo_sectorsize * DISK_DIRECT_BUFFER_SIZE);  // 分配直接缓冲区
    if (disk->buff == NULL) {  // 分配失败时打印错误信息
        PRINT_ERR("OsDiskInitSub: direct buffer of disk init failed\n");
        return VFS_ERROR;
    }
#endif

    return ENOERR;  // 返回成功
}

INT32 los_disk_init(const CHAR *diskName, const struct block_operations *bops,
                    VOID *priv, INT32 diskID, VOID *info)
{
    struct geometry diskInfo;
    struct Vnode *blkDriver = NULL;
    los_disk *disk = get_disk(diskID);
    INT32 ret;

    if ((diskName == NULL) || (disk == NULL) ||
        (disk->disk_status != STAT_UNREADY) || (strlen(diskName) > DISK_NAME)) {
        return VFS_ERROR;
    }

    if (register_blockdriver(diskName, bops, RWE_RW_RW, priv) != 0) {
        PRINT_ERR("disk_init : register %s fail!\n", diskName);
        return VFS_ERROR;
    }

    VnodeHold();
    ret = VnodeLookup(diskName, &blkDriver, 0);
    if (ret < 0) {
        VnodeDrop();
        PRINT_ERR("disk_init : %s, failed to find the vnode, ERRNO=%d\n", diskName, ret);
        goto DISK_FIND_ERROR;
    }
    struct block_operations *bops2 = (struct block_operations *)((struct drv_data *)blkDriver->data)->ops;

    if ((bops2 == NULL) || (bops2->geometry == NULL) || (bops2->geometry(blkDriver, &diskInfo) != 0)) {
        goto DISK_BLKDRIVER_ERROR;
    }

    if (diskInfo.geo_sectorsize < DISK_MAX_SECTOR_SIZE) {
        goto DISK_BLKDRIVER_ERROR;
    }

    ret = OsDiskInitSub(diskName, diskID, disk, &diskInfo, blkDriver);
    if (ret != ENOERR) {
        (VOID)DiskDeinit(disk);
        VnodeDrop();
        return VFS_ERROR;
    }
    VnodeDrop();
    if (DiskDivideAndPartitionRegister(info, disk) != ENOERR) {
        (VOID)DiskDeinit(disk);
        return VFS_ERROR;
    }

    disk->disk_status = STAT_INUSED;
    if (info != NULL) {
        disk->type = EMMC;
    } else {
        disk->type = OTHERS;
    }
    return ENOERR;

DISK_BLKDRIVER_ERROR:
    PRINT_ERR("disk_init : register %s ok but get disk info fail!\n", diskName);
    VnodeDrop();
DISK_FIND_ERROR:
    (VOID)unregister_blockdriver(diskName);
    return VFS_ERROR;
}

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

    disk->disk_status = STAT_UNREADY;
    DISK_UNLOCK(&disk->disk_mutex);

    return DiskDeinit(disk);
}

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

static los_part *OsPartFind(los_disk *disk, const struct Vnode *blkDriver)
{
    los_part *part = NULL;

    DISK_LOCK(&disk->disk_mutex);
    if ((disk->disk_status != STAT_INUSED) || (LOS_ListEmpty(&disk->head) == TRUE)) {
        goto EXIT;
    }
    part = LOS_DL_LIST_ENTRY(disk->head.pstNext, los_part, list);
    if (disk->dev == blkDriver) {
        goto EXIT;
    }

    while (&part->list != &disk->head) {
        if (part->dev == blkDriver) {
            goto EXIT;
        }
        part = LOS_DL_LIST_ENTRY(part->list.pstNext, los_part, list);
    }
    part = NULL;

EXIT:
    DISK_UNLOCK(&disk->disk_mutex);
    return part;
}

los_part *los_part_find(struct Vnode *blkDriver)
{
    INT32 i;
    los_disk *disk = NULL;
    los_part *part = NULL;

    if (blkDriver == NULL) {
        return NULL;
    }

    for (i = 0; i < SYS_MAX_DISK; i++) {
        disk = get_disk(i);
        if (disk == NULL) {
            continue;
        }
        part = OsPartFind(disk, blkDriver);
        if (part != NULL) {
            return part;
        }
    }

    return NULL;
}

INT32 los_part_access(const CHAR *dev, mode_t mode)
{
    los_part *part = NULL;
    struct Vnode *node = NULL;

    VnodeHold();
    if (VnodeLookup(dev, &node, 0) < 0) {
        VnodeDrop();
        return VFS_ERROR;
    }

    part = los_part_find(node);
    VnodeDrop();
    if (part == NULL) {
        return VFS_ERROR;
    }

    return ENOERR;
}

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

    part->part_name = (CHAR *)zalloc(len + 1);
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

INT32 add_mmc_partition(struct disk_divide_info *info, size_t sectorStart, size_t sectorCount)
{
    UINT32 index, i;

    if (info == NULL) {
        return VFS_ERROR;
    }

    if ((info->part_count >= MAX_DIVIDE_PART_PER_DISK) || (sectorCount == 0)) {
        return VFS_ERROR;
    }

    if ((sectorCount > info->sector_count) || ((info->sector_count - sectorCount) < sectorStart)) {
        return VFS_ERROR;
    }

    index = info->part_count;
    for (i = 0; i < index; i++) {
        if (sectorStart < (info->part[i].sector_start + info->part[i].sector_count)) {
            return VFS_ERROR;
        }
    }

    info->part[index].sector_start = sectorStart;
    info->part[index].sector_count = sectorCount;
    info->part[index].type = EMMC;
    info->part_count++;

    return ENOERR;
}

VOID show_part(los_part *part)
{
    if ((part == NULL) || (part->dev == NULL)) {
        PRINT_ERR("part is NULL\n");
        return;
    }

    PRINTK("\npart info :\n");
    PRINTK("disk id          : %u\n", part->disk_id);
    PRINTK("part_id in system: %u\n", part->part_id);
    PRINTK("part no in disk  : %u\n", part->part_no_disk);
    PRINTK("part no in mbr   : %u\n", part->part_no_mbr);
    PRINTK("part filesystem  : %02X\n", part->filesystem_type);
    PRINTK("part sec start   : %llu\n", part->sector_start);
    PRINTK("part sec count   : %llu\n", part->sector_count);
}

#ifdef LOSCFG_DRIVERS_MMC
ssize_t StorageBlockMmcErase(uint32_t blockId, size_t secStart, size_t secNr);
#endif

INT32 EraseDiskByID(UINT32 diskID, size_t startSector, UINT32 sectors)
{
    INT32 ret = VFS_ERROR;
#ifdef LOSCFG_DRIVERS_MMC
    los_disk *disk = get_disk((INT32)diskID);
    if (disk != NULL) {
        ret = StorageBlockMmcErase(diskID, startSector, sectors);
    }
#endif

    return ret;
}

