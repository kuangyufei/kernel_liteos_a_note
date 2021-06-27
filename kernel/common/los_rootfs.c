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
#include "los_base.h"
#include "los_typedef.h"
#include "string.h"
#if defined(LOSCFG_STORAGE_SPINOR) || defined(LOSCFG_STORAGE_SPINAND)
#include "mtd_partition.h"
#endif
#ifdef LOSCFG_DRIVERS_MMC
#include "disk.h"
#endif
#include "sys/mount.h"
#ifdef LOSCFG_PLATFORM_ROOTFS
#include "los_rootfs.h"
#endif
#include "mtd_list.h"
#include "fs/driver.h"

#ifdef LOSCFG_PLATFORM_QEMU_ARM_VIRT_CA7
#include "mtd_partition.h"
#include "cfiflash.h"
#define DEV_STORAGE_PATH        "/dev/cfiflash1"
#define SECOND_MTD_PART_NUM 1
#define STORAGE_SIZE 0x1400000
#endif

#ifdef LOSCFG_STORAGE_SPINOR
#define DEV_STORAGE_PATH       "/dev/spinorblk2"
#define SECOND_MTD_PART_NUM 2
#define STORAGE_SIZE 0x80000
#endif

#ifdef LOSCFG_STORAGE_SPINAND
#define DEV_STORAGE_PATH       "/dev/nandblk2"
#define SECOND_MTD_PART_NUM 2
#define STORAGE_SIZE 0xa00000
#endif

#ifdef LOSCFG_STORAGE_EMMC
#include "ff.h"
#define STORAGE_SIZE 0x3200000
STATIC los_disk *g_emmcDisk = NULL; 
#endif

#ifndef LOSCFG_SECURITY_BOOT
STATIC INT32 g_alignSize = 0;
#endif

#define VFAT_STORAGE_MOUNT_DIR_MODE 0777
#define DEFAULT_STORAGE_MOUNT_DIR_MODE 0755

#ifdef LOSCFG_DRIVERS_MMC //mmc代表的是磁盘
los_disk *GetMmcDisk(UINT8 type)//type值( EMMC | 
{
    const CHAR *mmcDevHead = "/dev/mmcblk";

    for (INT32 diskId = 0; diskId < SYS_MAX_DISK; diskId++) {//默认最大支持5块磁盘
        los_disk *disk = get_disk(diskId);
        if (disk == NULL) {
            continue;
        } else if (disk->disk_name == NULL) {
            continue;
        } else if (strncmp(disk->disk_name, mmcDevHead, strlen(mmcDevHead))) {//例如 /dev/mmcblk0p2 就符合要求
            continue;
        } else {
            if (disk->type == type) {//
                return disk;
            }
        }
    }
    PRINT_ERR("Cannot find the mmc disk!\n");
    return NULL;
}
#endif

#ifdef LOSCFG_STORAGE_EMMC
struct disk_divide_info *StorageBlockGetEmmc(void);
struct block_operations *StorageBlockGetMmcOps(void);
char *StorageBlockGetEmmcNodeName(void *block);

STATIC const CHAR *AddEmmcRootfsPart(INT32 rootAddr, INT32 rootSize, INT32 userAddr, INT32 userSize)
{//在EMMC介质上增加一个根文件系统分区
    INT32 ret;

    void *block = ((struct drv_data *)g_emmcDisk->dev->data)->priv;
    const char *node_name = StorageBlockGetEmmcNodeName(block);
    if (los_disk_deinit(g_emmcDisk->disk_id) != ENOERR) {
        PRINT_ERR("Failed to deinit emmc disk!\n");
        return NULL;
    }

    struct disk_divide_info *emmc = StorageBlockGetEmmc();
    ret = add_mmc_partition(emmc, rootAddr / EMMC_SEC_SIZE, rootSize / EMMC_SEC_SIZE);
    if (ret != LOS_OK) {
        PRINT_ERR("Failed to add mmc root partition!\n");
        return NULL;
    } else {
        UINT64 storageStartCnt = userAddr / EMMC_SEC_SIZE;
        UINT64 storageSizeCnt = userSize / EMMC_SEC_SIZE;
        UINT64 userdataStartCnt = storageStartCnt + storageSizeCnt;
        UINT64 userdataSizeCnt = g_emmcDisk->sector_count - userdataStartCnt;
        ret = add_mmc_partition(emmc, storageStartCnt, storageSizeCnt);
        if (ret != LOS_OK) {
            PRINT_ERR("Failed to add mmc storage partition!\n");
        }
        ret = add_mmc_partition(emmc, userdataStartCnt, userdataSizeCnt);
        if (ret != LOS_OK) {
            PRINT_ERR("Failed to add mmc userdata partition!\n");
        }
        LOS_Msleep(10); /* waiting for device identification */
        INT32 diskId = los_alloc_diskid_byname(node_name);//通过节点名称分配磁盘ID
        if (diskId < 0) {
            PRINT_ERR("Failed to alloc disk %s!\n", node_name);
            return NULL;
        }//磁盘初始化
        if (los_disk_init(node_name, StorageBlockGetMmcOps(), block, diskId, emmc) != ENOERR) {
            PRINT_ERR("Failed to init emmc disk!\n");
            return NULL;
        }
        return node_name;
    }
}
#endif
//系统可以有多个根文件系统并存,可放在 USB,SD卡,flash上
STATIC const CHAR *GetDevName(const CHAR *rootType, INT32 rootAddr, INT32 rootSize, INT32 userAddr, INT32 userSize)
{
    const CHAR *rootDev = NULL;

#if defined(LOSCFG_STORAGE_SPINOR) || defined(LOSCFG_STORAGE_SPINAND)
    INT32 ret;
    if (strcmp(rootType, "flash") == 0) { //flash模式启动
        ret = add_mtd_partition(FLASH_TYPE, rootAddr, rootSize, 0);//增加MTD分区
        if (ret != LOS_OK) {
            PRINT_ERR("Failed to add spinor/spinand root partition!\n");
        } else {
            rootDev = FLASH_DEV_NAME;
            ret = add_mtd_partition(FLASH_TYPE, userAddr, userSize, SECOND_MTD_PART_NUM);
            if (ret != LOS_OK) {
                PRINT_ERR("Failed to add spinor/spinand storage partition!\n");
            }
        }
    } else
#endif

#ifdef LOSCFG_DRIVERS_USB_MASS_STORAGE
    if (strcmp(rootType, "usb") == 0) {//usb模式启动
        rootDev = "/dev/sda";
    } else
#endif

#ifdef LOSCFG_DRIVERS_SD
    if (strcmp(rootType, "sdcard") == 0) {//sd卡模式启动
        los_disk *sdDisk = GetMmcDisk(OTHERS);
        if (sdDisk == NULL) {
            PRINT_ERR("Get sdcard failed!\n");
        } else {
            rootDev = sdDisk->disk_name;
        }
    } else
#endif

#ifdef LOSCFG_STORAGE_EMMC
    if (strcmp(rootType, "emmc") == 0) {//磁盘启动
        rootDev = AddEmmcRootfsPart(rootAddr, rootSize, userAddr, userSize);
    } else
#endif

#ifdef LOSCFG_PLATFORM_QEMU_ARM_VIRT_CA7
    if (strcmp(rootType, FLASH_TYPE) == 0) {
        INT32 ret;
        if (rootAddr != CFIFLASH_ROOT_ADDR) {
            PRINT_ERR("Error rootAddr, must be %#0x!\n", CFIFLASH_ROOT_ADDR);
            return NULL;
        }
        ret = add_mtd_partition(FLASH_TYPE, rootAddr, rootSize, 0);
        if (ret != LOS_OK) {
            PRINT_ERR("Failed to add %s root partition!\n", FLASH_TYPE);
        } else {
            rootDev = "/dev/cfiflash0";
            ret = add_mtd_partition(FLASH_TYPE, (rootAddr + rootSize),
                                    CFIFLASH_CAPACITY - rootAddr - rootSize, SECOND_MTD_PART_NUM);
            if (ret != LOS_OK) {
                PRINT_ERR("Failed to add %s storage partition!\n", FLASH_TYPE);
            }
        }
    } else
#endif
    {
        PRINT_ERR("Failed to find root dev type: %s\n", rootType);
    }
    return rootDev;
}

#ifndef LOSCFG_SECURITY_BOOT
STATIC INT32 GetArgs(CHAR **args)
{
#ifdef LOSCFG_BOOTENV_RAM
    *args = OsGetArgsAddr();
    return LOS_OK;

#else
    INT32 ret;
    INT32 i;
    INT32 len = 0;
    CHAR *cmdLine = NULL;
    CHAR *tmp = NULL;
    const CHAR *bootargName = "bootargs=";

    cmdLine = (CHAR *)malloc(COMMAND_LINE_SIZE);
    if (cmdLine == NULL) {
        PRINT_ERR("Malloc cmdLine space error!\n");
        return LOS_NOK;
    }

#ifdef LOSCFG_STORAGE_EMMC
    g_emmcDisk = GetMmcDisk(EMMC);//获取磁盘信息
    if (g_emmcDisk == NULL) {
        PRINT_ERR("Get EMMC disk failed!\n");
        goto ERROUT;
    }
    /* param4 is TRUE for not reading large contiguous data */
    ret = los_disk_read(g_emmcDisk->disk_id, cmdLine, COMMAND_LINE_ADDR / EMMC_SEC_SIZE,
                        COMMAND_LINE_SIZE / EMMC_SEC_SIZE, TRUE);
    if (ret != 0) {
        PRINT_ERR("Read EMMC command line failed!\n");
        goto ERROUT;
    }
    g_alignSize = EMMC_SEC_SIZE;
#endif

#if defined(LOSCFG_STORAGE_SPINOR) || defined(LOSCFG_STORAGE_SPINAND)
    struct MtdDev *mtd = GetMtd(FLASH_TYPE);
    if (mtd == NULL) {
        PRINT_ERR("Get spinor mtd failed!\n");
        goto ERROUT;
    }
    g_alignSize = mtd->eraseSize;
    ret = mtd->read(mtd, COMMAND_LINE_ADDR, COMMAND_LINE_SIZE, cmdLine);
    if (ret != COMMAND_LINE_SIZE) {
        PRINT_ERR("Read spinor command line failed!\n");
        goto ERROUT;
    }
#endif
#ifdef LOSCFG_PLATFORM_QEMU_ARM_VIRT_CA7
    struct MtdDev *mtd = GetCfiMtdDev();
    if (mtd == NULL) {
        PRINT_ERR("Get CFI mtd failed!\n");
        goto ERROUT;
    }
    g_alignSize = mtd->eraseSize;
    ret = mtd->read(mtd, CFIFLASH_BOOTARGS_ADDR, COMMAND_LINE_SIZE, cmdLine);
    if (ret != COMMAND_LINE_SIZE) {
        PRINT_ERR("Read CFI command line failed!\n");
        goto ERROUT;
    }
#endif

    for (i = 0; i < COMMAND_LINE_SIZE; i += len + 1) {
        len = strlen(cmdLine + i);
        tmp = strstr(cmdLine + i, bootargName);
        if (tmp != NULL) {
            *args = strdup(tmp + strlen(bootargName));
            if (*args == NULL) {
                goto ERROUT;
            }
            free(cmdLine);
            return LOS_OK;
        }
    }
    PRINT_ERR("Cannot find bootargs!\n");

ERROUT:
    free(cmdLine);
    return LOS_NOK;
#endif
}

STATIC INT32 MatchRootPos(CHAR *p, const CHAR *rootInfoName, INT32 *rootInfo)
{
    UINT32 offset;
    CHAR *value = NULL;

    if (strncmp(p, rootInfoName, strlen(rootInfoName)) == 0) {
        value = p + strlen(rootInfoName);
        offset = strspn(value, DEC_NUMBER_STRING);
        if (strcmp(p + strlen(p) - 1, "M") == 0) {
            if ((offset < (strlen(value) - 1)) || (sscanf_s(value, "%d", rootInfo) <= 0)) {
                goto ERROUT;
            }
            *rootInfo = *rootInfo * BYTES_PER_MBYTE;
        } else if (strcmp(p + strlen(p) - 1, "K") == 0) {
            if ((offset < (strlen(value) - 1)) || (sscanf_s(value, "%d", rootInfo) <= 0)) {
                goto ERROUT;
            }
            *rootInfo = *rootInfo * BYTES_PER_KBYTE;
        } else if (sscanf_s(value, "0x%x", rootInfo) > 0) {
            value += strlen("0x");
            if (strspn(value, HEX_NUMBER_STRING) < strlen(value)) {
                goto ERROUT;
            }
        } else {
            goto ERROUT;
        }
    }

    if ((*rootInfo >= 0) && (g_alignSize != 0) && ((UINT32)(*rootInfo) & (UINT32)(g_alignSize - 1))) {
        PRINT_ERR("The bootarg \"%s\" will be 0x%x aligned!\n", p, g_alignSize);
    }
    return LOS_OK;

ERROUT:
    PRINT_ERR("Invalid bootarg \"%s\"!\n", p);
    return LOS_NOK;
}
//匹配根文件系统信息
STATIC INT32 MatchRootInfo(CHAR *p, CHAR **rootType, CHAR **fsType, INT32 *rootAddr, INT32 *rootSize, INT32 *userAddr, INT32 *userSize)
{
    const CHAR *rootName = "root=";
    const CHAR *fsName = "fstype=";
    const CHAR *rootAddrName = "rootaddr=";
    const CHAR *rootSizeName = "rootsize=";
    const CHAR *userAddrName = "useraddr=";
    const CHAR *userSizeName = "usersize=";

    if ((*rootType == NULL) && (strncmp(p, rootName, strlen(rootName)) == 0)) {
        *rootType = strdup(p + strlen(rootName));
        if (*rootType == NULL) {
            return LOS_NOK;
        }
        return LOS_OK;
    }

    if ((*fsType == NULL) && (strncmp(p, fsName, strlen(fsName)) == 0)) {
        *fsType = strdup(p + strlen(fsName));
        if (*fsType == NULL) {
            return LOS_NOK;
        }
        return LOS_OK;
    }

    if (*rootAddr < 0) {
        if (MatchRootPos(p, rootAddrName, rootAddr) != LOS_OK) {
            return LOS_NOK;
        } else if (*rootAddr >= 0) {
            return LOS_OK;
        }
    }

    if (*rootSize < 0) {
        if (MatchRootPos(p, rootSizeName, rootSize) != LOS_OK) {
            return LOS_NOK;
        }
    }

    if (*userAddr < 0) {
        if (MatchRootPos(p, userAddrName, userAddr) != LOS_OK) {
            return LOS_NOK;
        } else if (*userAddr >= 0) {
            return LOS_OK;
        }
    }

    if (*userSize < 0) {
        if (MatchRootPos(p, userSizeName, userSize) != LOS_OK) {
            return LOS_NOK;
        }
    }

    return LOS_OK;
}
//获取根类型
STATIC INT32 GetRootType(CHAR **rootType, CHAR **fsType, INT32 *rootAddr, INT32 *rootSize, INT32 *userAddr, INT32 *userSize)
{
    CHAR *args = NULL;
    CHAR *p = NULL;

    if (GetArgs(&args) != LOS_OK) {
        PRINT_ERR("Cannot get bootargs!\n");
        return LOS_NOK;
    }
#ifndef LOSCFG_BOOTENV_RAM
    CHAR *argsBak = NULL;
    argsBak = args;
#endif
    p = strsep(&args, " ");
    while (p != NULL) {
        if (MatchRootInfo(p, rootType, fsType, rootAddr, rootSize, userAddr, userSize) != LOS_OK) {
            goto ERROUT;
        }
        p = strsep(&args, " ");
    }
    if ((*fsType != NULL) && (*rootType != NULL)) {
#ifndef LOSCFG_BOOTENV_RAM
        free(argsBak);
#endif
        return LOS_OK;
    }

ERROUT:
    PRINT_ERR("Invalid rootfs information!\n");
    if (*rootType != NULL) {
        free(*rootType);
        *rootType = NULL;
    }
    if (*fsType != NULL) {
        free(*fsType);
        *fsType = NULL;
    }
#ifndef LOSCFG_BOOTENV_RAM
    free(argsBak);
#endif
    return LOS_NOK;
}
#endif

#ifdef LOSCFG_STORAGE_EMMC
STATIC VOID OsMountUserdata(const CHAR *fsType)//mount emmc /userdata 用户数据目录
{
    INT32 ret;
    INT32 err;
    const CHAR *userdataDir = "/userdata";
    ret = mkdir(userdataDir, VFAT_STORAGE_MOUNT_DIR_MODE);//创建/userdata 目录
    if ((ret != LOS_OK) && ((err = get_errno()) != EEXIST)) {
        PRINT_ERR("Failed to mkdir /userdata, errno %d: %s\n", err, strerror(err));
        return;
    }
    CHAR emmcUserdataDev[DISK_NAME] = {0};
    if (snprintf_s(emmcUserdataDev, sizeof(emmcUserdataDev), sizeof(emmcUserdataDev) - 1,
        "%s%s", g_emmcDisk->disk_name, "p2") < 0) {
        PRINT_ERR("Failed to get emmc userdata dev name!\n");
        return;
    }
    ret = mount(emmcUserdataDev, userdataDir, fsType, 0, "umask=000");
    if (ret == LOS_OK) {
        return;
    }
    err = get_errno();
    if (err == ENOTSUP) {
#ifdef LOSCFG_FS_FAT
        ret = format(emmcUserdataDev, 0, FM_FAT32);
        if (ret != LOS_OK) {
            PRINT_ERR("Failed to format %s\n", emmcUserdataDev);
            return;
        }
#endif
        ret = mount(emmcUserdataDev, userdataDir, fsType, 0, "umask=000");
        if (ret != LOS_OK) {
            err = get_errno();
            PRINT_ERR("Failed to mount /userdata, errno %d: %s\n", err, strerror(err));
        }
    } else {
        PRINT_ERR("Failed to mount /userdata, errno %d: %s\n", err, strerror(err));
    }
    return;
}
#endif
//挂载根文件系统和用户文件系统
STATIC INT32 OsMountRootfsAndUserfs(const CHAR *rootDev, const CHAR *fsType)
{
    INT32 ret;
    INT32 err;
    if (strcmp(fsType, "vfat") == 0) { //vfat 格式处理  VFAT (Virtual File Allocation Table) 虚拟文件分配表
        ret = mount(rootDev, "/", fsType, MS_RDONLY, NULL);// mount /
        if (ret != LOS_OK) {
            err = get_errno();
            PRINT_ERR("Failed to mount vfat rootfs, errno %d: %s\n", err, strerror(err));
            return ret;
        }
#ifdef LOSCFG_STORAGE_EMMC
        ret = mkdir("/storage", VFAT_STORAGE_MOUNT_DIR_MODE);
        if ((ret != LOS_OK) && ((err = get_errno()) != EEXIST)) {
            PRINT_ERR("Failed to mkdir /storage, errno %d: %s\n", err, strerror(err));
            return ret;
        } else {
            CHAR emmcStorageDev[DISK_NAME] = {0};
            if (snprintf_s(emmcStorageDev, sizeof(emmcStorageDev), sizeof(emmcStorageDev) - 1,
                "%s%s", g_emmcDisk->disk_name, "p1") < 0) {
                PRINT_ERR("Failed to get emmc storage dev name!\n");
            } else {
                ret = mount(emmcStorageDev, "/storage", fsType, 0, "umask=000");//挂载storage目录
                if (ret != LOS_OK) {
                    err = get_errno();
                    PRINT_ERR("Failed to mount /storage, errno %d: %s\n", err, strerror(err));
                }
            }
        }
        OsMountUserdata(fsType);//挂载用户数据
#endif
    } else { //非 vfat格式
        ret = mount(rootDev, "/", fsType, MS_RDONLY, NULL);
        if (ret != LOS_OK) {
            err = get_errno();
            PRINT_ERR("Failed to mount rootfs,rootDev %s, errno %d: %s\n", rootDev, err, strerror(err));
            return ret;
        }
#if defined(LOSCFG_STORAGE_SPINOR) || defined(LOSCFG_STORAGE_SPINAND) || defined(LOSCFG_PLATFORM_QEMU_ARM_VIRT_CA7)
        ret = mkdir("/storage", DEFAULT_STORAGE_MOUNT_DIR_MODE);
        if ((ret != LOS_OK) && ((err = get_errno()) != EEXIST)) {
            PRINT_ERR("Failed to mkdir /storage, errno %d: %s\n", err, strerror(err));
            return ret;
        } else {
            ret = mount(DEV_STORAGE_PATH, "/storage", fsType, 0, NULL);
            if (ret != LOS_OK) {
                err = get_errno();
                PRINT_ERR("Failed to mount /storage, errno %d: %s\n", err, strerror(err));
            }
        }
#endif
    }
    return LOS_OK;
}
/******************************************************
	挂载根文件系统 分两个阶段
1.文件系统提供一个作为初始安装点的空目录
2.内核在空目录上安装根文件系统
******************************************************/
INT32 OsMountRootfs(VOID)
{
    INT32 ret = LOS_OK;
    INT32 err;
    INT32 rootAddr = -1;
    INT32 rootSize = -1;
    INT32 userAddr = -1;
    INT32 userSize = -1;
    CHAR *rootType = NULL;
    CHAR *fsType = NULL;
    const CHAR *rootDev = NULL;

#ifdef LOSCFG_SECURITY_BOOT	//使能安全启动
    rootType = strdup(ROOTFS_ROOT_TYPE);
    fsType = strdup(ROOTFS_FS_TYPE);
    rootAddr = ROOTFS_FLASH_ADDR;
    rootSize = ROOTFS_FLASH_SIZE;
#else
    ret = GetRootType(&rootType, &fsType, &rootAddr, &rootSize, &userAddr, &userSize);
    if (ret != LOS_OK) {
        return ret;
    }
    rootAddr = (rootAddr >= 0) ? rootAddr : ROOTFS_FLASH_ADDR;
    rootSize = (rootSize >= 0) ? rootSize : ROOTFS_FLASH_SIZE;
#endif
    userAddr = (userAddr >= 0) ? userAddr : rootAddr + rootSize;
    userSize = (userSize >= 0) ? userSize : STORAGE_SIZE;

    rootDev = GetDevName(rootType, rootAddr, rootSize, userAddr, userSize);
    if (rootDev != NULL) {
        ret = OsMountRootfsAndUserfs(rootDev, fsType);//将根设备挂到 /下， 
        if (ret != LOS_OK) {
            err = get_errno();
            PRINT_ERR("Failed to mount rootfs, errno %d: %s\n", err, strerror(err));
        }
    }

    free(rootType);
    free(fsType);
    return ret;
}
