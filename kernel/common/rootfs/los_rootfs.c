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

#include "los_rootfs.h"
#include "los_bootargs.h"
#include "los_base.h"
#include "string.h"
#include "sys/mount.h"
#include "sys/stat.h"
#include "sys/types.h"

#if defined(LOSCFG_STORAGE_SPINOR) || defined(LOSCFG_STORAGE_SPINAND) || defined(LOSCFG_PLATFORM_QEMU_ARM_VIRT_CA7)
#include "mtd_list.h"
#include "mtd_partition.h"
#endif

#ifdef LOSCFG_PLATFORM_QEMU_ARM_VIRT_CA7
#include "cfiflash.h"
#endif

#ifdef LOSCFG_STORAGE_EMMC
#include "disk.h"
#include "ff.h"
#endif


#ifdef LOSCFG_STORAGE_EMMC
struct disk_divide_info *StorageBlockGetEmmc(void);
struct block_operations *StorageBlockGetMmcOps(void);
char *StorageBlockGetEmmcNodeName(void *block);

STATIC INT32 AddEmmcParts(INT32 rootAddr, INT32 rootSize, INT32 userAddr, INT32 userSize)
{
    INT32 ret;

    los_disk *emmcDisk = los_get_mmcdisk_bytype(EMMC);
    if (emmcDisk == NULL) {
        PRINT_ERR("Get EMMC disk failed!\n");
        return LOS_NOK;
    }

    void *block = ((struct drv_data *)emmcDisk->dev->data)->priv;
    const char *node_name = StorageBlockGetEmmcNodeName(block);
    if (los_disk_deinit(emmcDisk->disk_id) != ENOERR) {
        PRINT_ERR("Failed to deinit emmc disk!\n");
        return LOS_NOK;
    }

    struct disk_divide_info *emmc = StorageBlockGetEmmc();
    ret = add_mmc_partition(emmc, rootAddr / EMMC_SEC_SIZE, rootSize / EMMC_SEC_SIZE);
    if (ret != LOS_OK) {
        PRINT_ERR("Failed to add mmc root partition!\n");
        return LOS_NOK;
    }

    UINT64 storageStartCnt = userAddr / EMMC_SEC_SIZE;
    UINT64 storageSizeCnt = userSize / EMMC_SEC_SIZE;
    UINT64 userdataStartCnt = storageStartCnt + storageSizeCnt;
    UINT64 userdataSizeCnt = emmcDisk->sector_count - userdataStartCnt;
    ret = add_mmc_partition(emmc, storageStartCnt, storageSizeCnt);
    if (ret != LOS_OK) {
        PRINT_ERR("Failed to add mmc storage partition!\n");
        return LOS_NOK;
    }

    ret = add_mmc_partition(emmc, userdataStartCnt, userdataSizeCnt);
    if (ret != LOS_OK) {
        PRINT_ERR("Failed to add mmc userdata partition!\n");
        return LOS_NOK;
    }

    LOS_Msleep(10); /* 100, sleep time. waiting for device identification */

    INT32 diskId = los_alloc_diskid_byname(node_name);
    if (diskId < 0) {
        PRINT_ERR("Failed to alloc disk %s!\n", node_name);
        return LOS_NOK;
    }

    if (los_disk_init(node_name, StorageBlockGetMmcOps(), block, diskId, emmc) != ENOERR) {
        PRINT_ERR("Failed to init emmc disk!\n");
        return LOS_NOK;
    }

    return LOS_OK;
}
#endif

//增加一个分区
STATIC INT32 AddPartitions(CHAR *dev, UINT64 rootAddr, UINT64 rootSize, UINT64 userAddr, UINT64 userSize)
{
#ifdef LOSCFG_PLATFORM_QEMU_ARM_VIRT_CA7
    if ((strcmp(dev, "cfi-flash") == 0) && (rootAddr != CFIFLASH_ROOT_ADDR)) {
        PRINT_ERR("Error rootAddr, must be %#0x!\n", CFIFLASH_ROOT_ADDR);
        return LOS_NOK;
    }
#endif

#if defined(LOSCFG_STORAGE_SPINOR) || defined(LOSCFG_STORAGE_SPINAND) || defined(LOSCFG_PLATFORM_QEMU_ARM_VIRT_CA7)
    INT32 ret;
    INT32 blk0 = 0;
    INT32 blk2 = 2;
    if (strcmp(dev, "flash") == 0 || strcmp(dev, FLASH_TYPE) == 0) {
        ret = add_mtd_partition(FLASH_TYPE, rootAddr, rootSize, blk0);//增加一个MTD分区
        if (ret != LOS_OK) {
            PRINT_ERR("Failed to add mtd root partition!\n");
            return LOS_NOK;
        }

        ret = add_mtd_partition(FLASH_TYPE, userAddr, userSize, blk2);
        if (ret != LOS_OK) {
            PRINT_ERR("Failed to add mtd storage partition!\n");
            return LOS_NOK;
        }

        return LOS_OK;
    }
#endif

#ifdef LOSCFG_STORAGE_EMMC
    if (strcmp(dev, "emmc") == 0) {
        return AddEmmcParts(rootAddr, rootSize, userAddr, userSize);
    }
#endif

    PRINT_ERR("Unsupport dev type: %s\n", dev);
    return LOS_NOK;
}

//获取根文件系统参数
STATIC INT32 ParseRootArgs(CHAR **dev, CHAR **fstype, UINT64 *rootAddr, UINT64 *rootSize, UINT32 *mountFlags)
{
    INT32 ret;
    CHAR *rootAddrStr = NULL;
    CHAR *rootSizeStr = NULL;
    CHAR *rwTag = NULL;
	//获取文件系统放在哪种设备上
    ret = LOS_GetArgValue("root", dev);//root = flash | mmc | 
    if (ret != LOS_OK) {
        PRINT_ERR("Cannot find root!");
        return ret;
    }
	//获取文件系统类型
    ret = LOS_GetArgValue("fstype", fstype);
    if (ret != LOS_OK) {
        PRINT_ERR("Cannot find fstype!");
        return ret;
    }
	//获取内核地址空间开始位置
    ret = LOS_GetArgValue("rootaddr", &rootAddrStr);
    if (ret != LOS_OK) {
        *rootAddr = ROOTFS_ADDR;
    } else {
        *rootAddr = LOS_SizeStrToNum(rootAddrStr);
    }
	//获取内核地址空间大小
    ret = LOS_GetArgValue("rootsize", &rootSizeStr);
    if (ret != LOS_OK) {
        *rootSize = ROOTFS_SIZE;
    } else {
        *rootSize = LOS_SizeStrToNum(rootSizeStr);
    }

    ret = LOS_GetArgValue("ro", &rwTag);
    if (ret == LOS_OK) {
        *mountFlags = MS_RDONLY;
    } else {
        *mountFlags = 0;
    }

    return LOS_OK;
}

STATIC INT32 ParseUserArgs(UINT64 rootAddr, UINT64 rootSize, UINT64 *userAddr, UINT64 *userSize)
{
    INT32 ret;
    CHAR *userAddrStr = NULL;
    CHAR *userSizeStr = NULL;

    ret = LOS_GetArgValue("useraddr", &userAddrStr);
    if (ret != LOS_OK) {
        *userAddr = rootAddr + rootSize;
    } else {
        *userAddr = LOS_SizeStrToNum(userAddrStr);
    }

    ret = LOS_GetArgValue("usersize", &userSizeStr);
    if (ret != LOS_OK) {
        *userSize = USERFS_SIZE;
    } else {
        *userSize = LOS_SizeStrToNum(userSizeStr);
    }

    return LOS_OK;
}
//挂载分区,即挂载 "/","/storage"
STATIC INT32 MountPartitions(CHAR *fsType, UINT32 mountFlags)
{
    INT32 ret;
    INT32 err;

    /* Mount rootfs */
    ret = mount(ROOT_DEV_NAME, ROOT_DIR_NAME, fsType, mountFlags, NULL);//挂载根文件系统
    if (ret != LOS_OK) {
        err = get_errno();
        PRINT_ERR("Failed to mount %s, rootDev %s, errno %d: %s\n", ROOT_DIR_NAME, ROOT_DEV_NAME, err, strerror(err));
        return ret;
    }

    /* Mount userfs */
    ret = mkdir(STORAGE_DIR_NAME, DEFAULT_MOUNT_DIR_MODE);//创建目录"/storage"
    if ((ret != LOS_OK) && ((err = get_errno()) != EEXIST)) {
        PRINT_ERR("Failed to mkdir %s, errno %d: %s\n", STORAGE_DIR_NAME, err, strerror(err));
        return ret;
    }

    ret = mount(USER_DEV_NAME, STORAGE_DIR_NAME, fsType, 0, DEFAULT_MOUNT_DATA);//挂载用户数据文件系统
    if (ret != LOS_OK) {
        err = get_errno();
        PRINT_ERR("Failed to mount %s, errno %d: %s\n", STORAGE_DIR_NAME, err, strerror(err));
        return ret;
    }

#ifdef LOSCFG_STORAGE_EMMC
    /* Mount userdata */
    ret = mkdir(USERDATA_DIR_NAME, DEFAULT_MOUNT_DIR_MODE);//创建目录"/userdata"
    if ((ret != LOS_OK) && ((err = get_errno()) != EEXIST)) {
        PRINT_ERR("Failed to mkdir %s, errno %d: %s\n", USERDATA_DIR_NAME, err, strerror(err));
        return ret;
    }

    ret = mount(USERDATA_DEV_NAME, USERDATA_DIR_NAME, fsType, 0, DEFAULT_MOUNT_DATA);//挂载用户数据文件系统
    if ((ret != LOS_OK) && ((err = get_errno()) == ENOTSUP)) {
        ret = format(USERDATA_DEV_NAME, 0, FM_FAT32);
        if (ret != LOS_OK) {
            PRINT_ERR("Failed to format %s\n", USERDATA_DEV_NAME);
            return ret;
        }

        ret = mount(USERDATA_DEV_NAME, USERDATA_DIR_NAME, fsType, 0, DEFAULT_MOUNT_DATA);
        if (ret != LOS_OK) {
            err = get_errno();
        }
    }
    if (ret != LOS_OK) {
        PRINT_ERR("Failed to mount %s, errno %d: %s\n", USERDATA_DIR_NAME, err, strerror(err));
        return ret;
    }
#endif
    return LOS_OK;
}

STATIC INT32 CheckValidation(UINT64 rootAddr, UINT64 rootSize, UINT64 userAddr, UINT64 userSize)
{
    UINT64 alignSize = LOS_GetAlignsize();

    if (alignSize == 0) {
        return LOS_OK;
    }

    if ((rootAddr & (alignSize - 1)) || (rootSize & (alignSize - 1)) ||
        (userAddr & (alignSize - 1)) || (userSize & (alignSize - 1))) {
        PRINT_ERR("The address or size value should be 0x%x aligned!\n", alignSize);
        return LOS_NOK;
    }

    return LOS_OK;
}
//挂载根文件系统 由 SystemInit 调用
INT32 OsMountRootfs()
{
    INT32 ret;
    CHAR *dev = NULL;
    CHAR *fstype = NULL;
    UINT64 rootAddr;
    UINT64 rootSize;
    UINT64 userAddr;
    UINT64 userSize;
    UINT32 mountFlags;
	//获取根文件系统s参数
    ret = ParseRootArgs(&dev, &fstype, &rootAddr, &rootSize, &mountFlags);
    if (ret != LOS_OK) {
        return ret;
    }
	//获取用户文件参数
    ret = ParseUserArgs(rootAddr, rootSize, &userAddr, &userSize);
    if (ret != LOS_OK) {
        return ret;
    }
	//检查内核和用户空间的有效性
    ret = CheckValidation(rootAddr, rootSize, userAddr, userSize);
    if (ret != LOS_OK) {
        return ret;
    }
	//添加分区
    ret = AddPartitions(dev, rootAddr, rootSize, userAddr, userSize);
    if (ret != LOS_OK) {
        return ret;
    }
	//挂载分区,即挂载 `/`
    ret = MountPartitions(fstype, mountFlags);
    if (ret != LOS_OK) {
        return ret;
    }

    return LOS_OK;
}
