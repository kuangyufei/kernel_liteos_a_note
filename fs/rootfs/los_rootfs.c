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

#if defined(LOSCFG_STORAGE_SPINOR) || defined(LOSCFG_STORAGE_SPINAND)
#include "mtd_list.h"
#include "mtd_partition.h"
#endif

#ifdef LOSCFG_STORAGE_EMMC
#include "disk.h"
#include "ff.h"
#endif

#ifdef LOSCFG_STORAGE_EMMC
/**
 * @brief 获取EMMC磁盘分区信息
 * @return 指向disk_divide_info结构体的指针，包含EMMC分区信息
 */
struct disk_divide_info *StorageBlockGetEmmc(void);
/**
 * @brief 获取MMC块设备操作函数集
 * @return 指向block_operations结构体的指针，包含MMC设备的操作方法
 */
struct block_operations *StorageBlockGetMmcOps(void);
/**
 * @brief 获取EMMC设备节点名称
 * @param block 块设备私有数据指针
 * @return 设备节点名称字符串
 */
char *StorageBlockGetEmmcNodeName(void *block);

/**
 * @brief 添加EMMC存储分区
 * @param rootAddr 根分区起始地址
 * @param rootSize 根分区大小
 * @param userAddr 用户分区起始地址
 * @param userSize 用户分区大小
 * @return 成功返回LOS_OK，失败返回LOS_NOK
 */
STATIC INT32 AddEmmcParts(INT32 rootAddr, INT32 rootSize, INT32 userAddr, INT32 userSize)
{
    INT32 ret;  // 函数返回值，用于错误处理

    los_disk *emmcDisk = los_get_mmcdisk_bytype(EMMC);  // 获取EMMC类型的磁盘设备
    if (emmcDisk == NULL) {  // 检查EMMC磁盘是否获取成功
        PRINT_ERR("Get EMMC disk failed!\n");  // 打印错误信息
        return LOS_NOK;  // 返回失败
    }

    void *block = ((struct drv_data *)emmcDisk->dev->data)->priv;  // 获取块设备私有数据
    const char *node_name = StorageBlockGetEmmcNodeName(block);  // 获取设备节点名称
    if (los_disk_deinit(emmcDisk->disk_id) != ENOERR) {  // 反初始化EMMC磁盘
        PRINT_ERR("Failed to deinit emmc disk!\n");  // 打印错误信息
        return LOS_NOK;  // 返回失败
    }

    struct disk_divide_info *emmc = StorageBlockGetEmmc();  // 获取EMMC分区信息
    // 添加根分区，将地址和大小转换为扇区数（EMMC_SEC_SIZE为扇区大小）
    ret = add_mmc_partition(emmc, rootAddr / EMMC_SEC_SIZE, rootSize / EMMC_SEC_SIZE);
    if (ret != LOS_OK) {  // 检查根分区添加是否成功
        PRINT_ERR("Failed to add mmc root partition!\n");  // 打印错误信息
        return LOS_NOK;  // 返回失败
    }

#ifdef LOSCFG_PLATFORM_PATCHFS  // 条件编译：如果启用补丁文件系统
    UINT64 patchStartCnt = userAddr / EMMC_SEC_SIZE;  // 补丁分区起始扇区
    UINT64 patchSizeCnt = PATCH_SIZE / EMMC_SEC_SIZE;  // 补丁分区大小（扇区数）
    // 添加补丁分区
    ret = add_mmc_partition(emmc, patchStartCnt, patchSizeCnt);
    if (ret != LOS_OK) {  // 检查补丁分区添加是否成功
        PRINT_ERR("Failed to add mmc patch partition!\n");  // 打印错误信息
        return LOS_NOK;  // 返回失败
    }
    userAddr += PATCH_SIZE;  // 更新用户分区起始地址（跳过补丁分区）
#endif

    UINT64 storageStartCnt = userAddr / EMMC_SEC_SIZE;  // 存储分区起始扇区
    UINT64 storageSizeCnt = userSize / EMMC_SEC_SIZE;  // 存储分区大小（扇区数）
    UINT64 userdataStartCnt = storageStartCnt + storageSizeCnt;  // 用户数据分区起始扇区
    // 用户数据分区大小 = 总扇区数 - 用户数据分区起始扇区
    UINT64 userdataSizeCnt = emmcDisk->sector_count - userdataStartCnt;
    // 添加存储分区
    ret = add_mmc_partition(emmc, storageStartCnt, storageSizeCnt);
    if (ret != LOS_OK) {  // 检查存储分区添加是否成功
        PRINT_ERR("Failed to add mmc storage partition!\n");  // 打印错误信息
        return LOS_NOK;  // 返回失败
    }

    // 添加用户数据分区
    ret = add_mmc_partition(emmc, userdataStartCnt, userdataSizeCnt);
    if (ret != LOS_OK) {  // 检查用户数据分区添加是否成功
        PRINT_ERR("Failed to add mmc userdata partition!\n");  // 打印错误信息
        return LOS_NOK;  // 返回失败
    }

    LOS_Msleep(10); /* 100, sleep time. waiting for device identification */  // 等待设备识别

    INT32 diskId = los_alloc_diskid_byname(node_name);  // 根据节点名称分配磁盘ID
    if (diskId < 0) {  // 检查磁盘ID分配是否成功
        PRINT_ERR("Failed to alloc disk %s!\n", node_name);  // 打印错误信息
        return LOS_NOK;  // 返回失败
    }

    // 初始化EMMC磁盘
    if (los_disk_init(node_name, StorageBlockGetMmcOps(), block, diskId, emmc) != ENOERR) {
        PRINT_ERR("Failed to init emmc disk!\n");  // 打印错误信息
        return LOS_NOK;  // 返回失败
    }

    return LOS_OK;  // 函数执行成功
}
#endif


/**
 * @brief 根据设备类型添加分区
 * @param dev 设备类型字符串（如"flash"或"emmc"）
 * @param rootAddr 根分区起始地址
 * @param rootSize 根分区大小
 * @param userAddr 用户分区起始地址
 * @param userSize 用户分区大小
 * @return 成功返回LOS_OK，失败返回LOS_NOK
 */
STATIC INT32 AddPartitions(CHAR *dev, UINT64 rootAddr, UINT64 rootSize, UINT64 userAddr, UINT64 userSize)
{
#if defined(LOSCFG_STORAGE_SPINOR) || defined(LOSCFG_STORAGE_SPINAND)  // 条件编译：如果启用SPI NOR/NAND存储
    INT32 ret;  // 函数返回值，用于错误处理
    INT32 blk0 = 0;  // 根分区块设备号
    INT32 blk2 = 2;  // 存储分区块设备号
    // 检查设备类型是否为flash
    if (strcmp(dev, "flash") == 0 || strcmp(dev, FLASH_TYPE) == 0) {
        // 添加MTD根分区
        ret = add_mtd_partition(FLASH_TYPE, rootAddr, rootSize, blk0);
        if (ret != LOS_OK) {  // 检查根分区添加是否成功
            PRINT_ERR("Failed to add mtd root partition!\n");  // 打印错误信息
            return LOS_NOK;  // 返回失败
        }

        // 添加MTD存储分区
        ret = add_mtd_partition(FLASH_TYPE, userAddr, userSize, blk2);
        if (ret != LOS_OK) {  // 检查存储分区添加是否成功
            PRINT_ERR("Failed to add mtd storage partition!\n");  // 打印错误信息
            return LOS_NOK;  // 返回失败
        }

        return LOS_OK;  // 函数执行成功
    }
#endif

#ifdef LOSCFG_STORAGE_EMMC  // 条件编译：如果启用EMMC存储
    if (strcmp(dev, "emmc") == 0) {  // 检查设备类型是否为emmc
        // 调用EMMC分区添加函数
        return AddEmmcParts(rootAddr, rootSize, userAddr, userSize);
    }
#endif

    PRINT_ERR("Unsupport dev type: %s\n", dev);  // 打印不支持的设备类型错误
    return LOS_NOK;  // 返回失败
}


/**
 * @brief 解析根文件系统启动参数
 * @param dev 输出参数，设备类型字符串
 * @param fstype 输出参数，文件系统类型
 * @param rootAddr 输出参数，根分区起始地址
 * @param rootSize 输出参数，根分区大小
 * @param mountFlags 输出参数，挂载标志（如只读）
 * @return 成功返回LOS_OK，失败返回LOS_NOK
 */
STATIC INT32 ParseRootArgs(CHAR **dev, CHAR **fstype, UINT64 *rootAddr, UINT64 *rootSize, UINT32 *mountFlags)
{
    INT32 ret;  // 函数返回值，用于错误处理
    CHAR *rootAddrStr = NULL;  // 根分区地址字符串
    CHAR *rootSizeStr = NULL;  // 根分区大小字符串
    CHAR *rwTag = NULL;  // 读写标志字符串

    ret = LOS_GetArgValue("root", dev);  // 获取"root"启动参数（设备类型）
    if (ret != LOS_OK) {  // 检查参数获取是否成功
        PRINT_ERR("Cannot find root!");  // 打印错误信息
        return ret;  // 返回失败
    }

    ret = LOS_GetArgValue("fstype", fstype);  // 获取"fstype"启动参数（文件系统类型）
    if (ret != LOS_OK) {  // 检查参数获取是否成功
        PRINT_ERR("Cannot find fstype!");  // 打印错误信息
        return ret;  // 返回失败
    }

    ret = LOS_GetArgValue("rootaddr", &rootAddrStr);  // 获取"rootaddr"启动参数（根分区地址）
    if (ret != LOS_OK) {  // 如果未指定，使用默认值
        *rootAddr = ROOTFS_ADDR;  // 默认根分区地址
    } else {
        *rootAddr = LOS_SizeStrToNum(rootAddrStr);  // 转换字符串地址为数值
    }

    ret = LOS_GetArgValue("rootsize", &rootSizeStr);  // 获取"rootsize"启动参数（根分区大小）
    if (ret != LOS_OK) {  // 如果未指定，使用默认值
        *rootSize = ROOTFS_SIZE;  // 默认根分区大小
    } else {
        *rootSize = LOS_SizeStrToNum(rootSizeStr);  // 转换字符串大小为数值
    }

    ret = LOS_GetArgValue("ro", &rwTag);  // 获取"ro"启动参数（只读标志）
    if (ret == LOS_OK) {  // 如果指定了"ro"，设置只读标志
        *mountFlags = MS_RDONLY;  // 只读挂载
    } else {
        *mountFlags = 0;  // 读写挂载
    }

    return LOS_OK;  // 函数执行成功
}

/**
 * @brief 解析用户分区启动参数
 * @param rootAddr 根分区起始地址
 * @param rootSize 根分区大小
 * @param userAddr 输出参数，用户分区起始地址
 * @param userSize 输出参数，用户分区大小
 * @return 成功返回LOS_OK，失败返回LOS_NOK
 */
STATIC INT32 ParseUserArgs(UINT64 rootAddr, UINT64 rootSize, UINT64 *userAddr, UINT64 *userSize)
{
    INT32 ret;  // 函数返回值，用于错误处理
    CHAR *userAddrStr = NULL;  // 用户分区地址字符串
    CHAR *userSizeStr = NULL;  // 用户分区大小字符串

    ret = LOS_GetArgValue("useraddr", &userAddrStr);  // 获取"useraddr"启动参数（用户分区地址）
    if (ret != LOS_OK) {  // 如果未指定，默认紧跟根分区之后
        *userAddr = rootAddr + rootSize;  // 用户分区起始地址 = 根分区起始地址 + 根分区大小
    } else {
        *userAddr = LOS_SizeStrToNum(userAddrStr);  // 转换字符串地址为数值
    }

    ret = LOS_GetArgValue("usersize", &userSizeStr);  // 获取"usersize"启动参数（用户分区大小）
    if (ret != LOS_OK) {  // 如果未指定，使用默认值
        *userSize = USERFS_SIZE;  // 默认用户分区大小
    } else {
        *userSize = LOS_SizeStrToNum(userSizeStr);  // 转换字符串大小为数值
    }

    return LOS_OK;  // 函数执行成功
}

/**
 * @brief 挂载文件系统分区
 * @param fsType 文件系统类型
 * @param mountFlags 挂载标志（如只读）
 * @return 成功返回LOS_OK，失败返回LOS_NOK
 */
STATIC INT32 MountPartitions(CHAR *fsType, UINT32 mountFlags)
{
    INT32 ret;  // 函数返回值，用于错误处理
    INT32 err;  // 错误码存储变量

    /* Mount rootfs */  // 挂载根文件系统
    ret = mount(ROOT_DEV_NAME, ROOT_DIR_NAME, fsType, mountFlags, NULL);
    if (ret != LOS_OK) {  // 检查根文件系统挂载是否成功
        err = get_errno();  // 获取错误码
        PRINT_ERR("Failed to mount %s, rootDev %s, errno %d: %s\n", ROOT_DIR_NAME, ROOT_DEV_NAME, err, strerror(err));  // 打印错误信息
        return ret;  // 返回失败
    }

#ifdef LOSCFG_STORAGE_EMMC  // 条件编译：如果启用EMMC存储
#ifdef LOSCFG_PLATFORM_PATCHFS  // 条件编译：如果启用补丁文件系统
    /* Mount patch */  // 挂载补丁分区
    // 创建补丁分区挂载点目录
    ret = mkdir(PATCH_DIR_NAME, DEFAULT_MOUNT_DIR_MODE);
    if ((ret != LOS_OK) && ((err = get_errno()) != EEXIST)) {  // 检查目录是否创建成功（已存在也视为成功）
        PRINT_ERR("Failed to mkdir %s, errno %d: %s\n", PATCH_DIR_NAME, err, strerror(err));  // 打印错误信息
        return ret;  // 返回失败
    }

    // 挂载补丁分区
    ret = mount(PATCH_DEV_NAME, PATCH_DIR_NAME, fsType, 0, DEFAULT_MOUNT_DATA);
    if ((ret != LOS_OK) && ((err = get_errno()) == ENOTSUP)) {  // 如果不支持该文件系统，尝试格式化
        ret = format(PATCH_DEV_NAME, 0, FM_FAT32);  // 格式化补丁分区为FAT32
        if (ret != LOS_OK) {  // 检查格式化是否成功
            PRINT_ERR("Failed to format %s\n", PATCH_DEV_NAME);  // 打印错误信息
            return ret;  // 返回失败
        }

        // 格式化后再次尝试挂载
        ret = mount(PATCH_DEV_NAME, PATCH_DIR_NAME, fsType, 0, DEFAULT_MOUNT_DATA);
        if (ret != LOS_OK) {  // 检查挂载是否成功
            err = get_errno();  // 获取错误码
        }
    }
    if (ret != LOS_OK) {  // 检查最终挂载结果
        PRINT_ERR("Failed to mount %s, errno %d: %s\n", PATCH_DIR_NAME, err, strerror(err));  // 打印错误信息
        return ret;  // 返回失败
    }
#endif
#endif

    /* Mount userfs */  // 挂载用户存储分区
    // 创建用户存储分区挂载点目录
    ret = mkdir(STORAGE_DIR_NAME, DEFAULT_MOUNT_DIR_MODE);
    if ((ret != LOS_OK) && ((err = get_errno()) != EEXIST)) {  // 检查目录是否创建成功（已存在也视为成功）
        PRINT_ERR("Failed to mkdir %s, errno %d: %s\n", STORAGE_DIR_NAME, err, strerror(err));  // 打印错误信息
        return ret;  // 返回失败
    }

    // 挂载用户存储分区
    ret = mount(USER_DEV_NAME, STORAGE_DIR_NAME, fsType, 0, DEFAULT_MOUNT_DATA);
    if (ret != LOS_OK) {  // 检查挂载是否成功
        err = get_errno();  // 获取错误码
        PRINT_ERR("Failed to mount %s, errno %d: %s\n", STORAGE_DIR_NAME, err, strerror(err));  // 打印错误信息
        return ret;  // 返回失败
    }

#ifdef LOSCFG_STORAGE_EMMC  // 条件编译：如果启用EMMC存储
    /* Mount userdata */  // 挂载用户数据分区
    // 创建用户数据分区挂载点目录
    ret = mkdir(USERDATA_DIR_NAME, DEFAULT_MOUNT_DIR_MODE);
    if ((ret != LOS_OK) && ((err = get_errno()) != EEXIST)) {  // 检查目录是否创建成功（已存在也视为成功）
        PRINT_ERR("Failed to mkdir %s, errno %d: %s\n", USERDATA_DIR_NAME, err, strerror(err));  // 打印错误信息
        return ret;  // 返回失败
    }

    // 挂载用户数据分区
    ret = mount(USERDATA_DEV_NAME, USERDATA_DIR_NAME, fsType, 0, DEFAULT_MOUNT_DATA);
    if ((ret != LOS_OK) && ((err = get_errno()) == ENOTSUP)) {  // 如果不支持该文件系统，尝试格式化
        ret = format(USERDATA_DEV_NAME, 0, FM_FAT32);  // 格式化用户数据分区为FAT32
        if (ret != LOS_OK) {  // 检查格式化是否成功
            PRINT_ERR("Failed to format %s\n", USERDATA_DEV_NAME);  // 打印错误信息
            return ret;  // 返回失败
        }

        // 格式化后再次尝试挂载
        ret = mount(USERDATA_DEV_NAME, USERDATA_DIR_NAME, fsType, 0, DEFAULT_MOUNT_DATA);
        if (ret != LOS_OK) {  // 检查挂载是否成功
            err = get_errno();  // 获取错误码
        }
    }
    if (ret != LOS_OK) {  // 检查最终挂载结果
        PRINT_ERR("Failed to mount %s, errno %d: %s\n", USERDATA_DIR_NAME, err, strerror(err));  // 打印错误信息
        return ret;  // 返回失败
    }
#endif
    return LOS_OK;  // 函数执行成功
}

/**
 * @brief 检查地址和大小的对齐有效性
 * @param rootAddr 根分区起始地址
 * @param rootSize 根分区大小
 * @param userAddr 用户分区起始地址
 * @param userSize 用户分区大小
 * @return 对齐有效返回LOS_OK，否则返回LOS_NOK
 */
STATIC INT32 CheckValidation(UINT64 rootAddr, UINT64 rootSize, UINT64 userAddr, UINT64 userSize)
{
    UINT64 alignSize = LOS_GetAlignsize();  // 获取系统对齐大小
    if (alignSize == 0) {  // 如果对齐大小为0，无需检查
        return LOS_OK;  // 返回成功
    }

    // 检查所有地址和大小是否按alignSize对齐（对齐检查公式：value & (alignSize - 1) == 0）
    if ((rootAddr & (alignSize - 1)) || (rootSize & (alignSize - 1)) ||
        (userAddr & (alignSize - 1)) || (userSize & (alignSize - 1))) {
        PRINT_ERR("The address or size value should be 0x%llx aligned!\n", alignSize);  // 打印对齐错误信息
        return LOS_NOK;  // 返回失败
    }

    return LOS_OK;  // 对齐检查通过
}

/**
 * @brief 挂载根文件系统入口函数
 * @return 成功返回LOS_OK，失败返回LOS_NOK
 */
INT32 OsMountRootfs()
{
    INT32 ret;  // 函数返回值，用于错误处理
    CHAR *dev = NULL;  // 设备类型
    CHAR *fstype = NULL;  // 文件系统类型
    UINT64 rootAddr;  // 根分区起始地址
    UINT64 rootSize;  // 根分区大小
    UINT64 userAddr;  // 用户分区起始地址
    UINT64 userSize;  // 用户分区大小
    UINT32 mountFlags;  // 挂载标志

    ret = ParseRootArgs(&dev, &fstype, &rootAddr, &rootSize, &mountFlags);  // 解析根分区参数
    if (ret != LOS_OK) {  // 检查参数解析是否成功
        return ret;  // 返回失败
    }

    ret = ParseUserArgs(rootAddr, rootSize, &userAddr, &userSize);  // 解析用户分区参数
    if (ret != LOS_OK) {  // 检查参数解析是否成功
        return ret;  // 返回失败
    }

    ret = CheckValidation(rootAddr, rootSize, userAddr, userSize);  // 检查地址和大小对齐
    if (ret != LOS_OK) {  // 检查对齐是否有效
        return ret;  // 返回失败
    }

    ret = AddPartitions(dev, rootAddr, rootSize, userAddr, userSize);  // 添加存储分区
    if (ret != LOS_OK) {  // 检查分区添加是否成功
        return ret;  // 返回失败
    }

    ret = MountPartitions(fstype, mountFlags);  // 挂载文件系统分区
    if (ret != LOS_OK) {  // 检查分区挂载是否成功
        return ret;  // 返回失败
    }

    return LOS_OK;  // 根文件系统挂载成功
}