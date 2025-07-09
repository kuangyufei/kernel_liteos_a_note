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
#include "los_patchfs.h"
#include "los_partition_utils.h"

#include "sys/mount.h"
#include "vnode.h"

#ifdef LOSCFG_PLATFORM_PATCHFS
/**
 * @brief 挂载补丁文件系统
 * @details 初始化分区信息并执行补丁文件系统的挂载操作，支持安全启动和普通启动两种模式
 * @return INT32 - 操作结果
 *         LOS_OK：挂载成功
 *         LOS_NOK：挂载失败
 */
INT32 OsMountPatchFs(VOID)
{
    INT32 ret;  // 函数返回值，默认为失败
    // 初始化分区信息结构体
    struct PartitionInfo partInfo = {
        PATCHPART_NAME,           // 分区名称
        PATCH_CMDLINE_ARGNAME,    // 命令行参数名
        PATCH_STORAGE_ARGNAME,    // 存储类型参数名
        NULL,                     // 存储类型值（动态分配）
        PATCH_FSTYPE_ARGNAME,     // 文件系统类型参数名
        NULL,                     // 文件系统类型值（动态分配）
        PATCH_ADDR_ARGNAME,       // 起始地址参数名
        -1,                       // 起始地址（未设置）
        PATCH_SIZE_ARGNAME,       // 分区大小参数名
        -1,                       // 分区大小（未设置）
        NULL,                     // 设备名（动态分配）
        PATCH_PARTITIONNUM        // 分区编号
    };

#ifdef LOSCFG_SECURITY_BOOT  // 安全启动模式
    // 分配并设置存储类型
    partInfo.storageType = strdup(STORAGE_TYPE);
    if (partInfo.storageType == NULL) {  // 内存分配失败检查
        return LOS_NOK;  // 返回失败
    }
    // 分配并设置文件系统类型
    partInfo.fsType = strdup(FS_TYPE);
    if (partInfo.fsType == NULL) {  // 内存分配失败检查
        ret = LOS_NOK;  // 设置返回值为失败
        goto EXIT;      // 跳转到资源释放
    }
    partInfo.startAddr = PATCHFS_FLASH_ADDR;  // 设置固定起始地址
    partInfo.partSize = PATCHFS_FLASH_SIZE;   // 设置固定分区大小
#else  // 普通启动模式
    // 获取分区信息
    ret = GetPartitionInfo(&partInfo);
    if (ret != LOS_OK) {  // 获取失败检查
        goto EXIT;  // 跳转到资源释放
    }
    // 设置默认起始地址（未配置时使用FLASH地址）
    partInfo.startAddr = (partInfo.startAddr >= 0) ? partInfo.startAddr : PATCHFS_FLASH_ADDR;
    // 设置默认分区大小（未配置时使用FLASH大小）
    partInfo.partSize = (partInfo.partSize >= 0) ? partInfo.partSize : PATCHFS_FLASH_SIZE;
#endif

    ret = LOS_NOK;  // 初始化返回值为失败
    // 分配设备名称
    partInfo.devName = strdup(PATCH_FLASH_DEV_NAME);
    if (partInfo.devName == NULL) {  // 内存分配失败检查
        goto EXIT;  // 跳转到资源释放
    }
    // 获取分区设备名
    const CHAR *devName = GetDevNameOfPartition(&partInfo);
    if (devName != NULL) {  // 设备名获取成功
        // 创建挂载点目录
        ret = mkdir(PATCHFS_MOUNT_POINT, 0);
        if (ret == LOS_OK) {  // 目录创建成功
            // 挂载文件系统（只读模式）
            ret = mount(devName, PATCHFS_MOUNT_POINT, partInfo.fsType, MS_RDONLY, NULL);
            if (ret != LOS_OK) {  // 挂载失败处理
                int err = get_errno();  // 获取错误码
                PRINT_ERR("Failed to mount %s, errno %d: %s\n", partInfo.partName, err, strerror(err));
            }
        } else {  // 目录创建失败处理
            int err = get_errno();  // 获取错误码
            PRINT_ERR("Failed to mkdir %s, errno %d: %s\n", PATCHFS_MOUNT_POINT, err, strerror(err));
        }
    }

    // 挂载失败时重置设备名
    if ((ret != LOS_OK) && (devName != NULL)) {
        ResetDevNameofPartition(&partInfo);
    }

EXIT:  // 资源释放标签
    free(partInfo.devName);        // 释放设备名内存
    partInfo.devName = NULL;       // 置空指针防止野指针
    free(partInfo.storageType);    // 释放存储类型内存
    partInfo.storageType = NULL;   // 置空指针防止野指针
    free(partInfo.fsType);         // 释放文件系统类型内存
    partInfo.fsType = NULL;        // 置空指针防止野指针
    return ret;  // 返回操作结果
}

#endif // LOSCFG_PLATFORM_PATCHFS
