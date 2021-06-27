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

INT32 OsMountPatchFs(VOID)
{
    INT32 ret;
    struct PartitionInfo partInfo = {
        PATCHPART_NAME, PATCH_CMDLINE_ARGNAME,
        PATCH_STORAGE_ARGNAME, NULL,
        PATCH_FSTYPE_ARGNAME, NULL,
        PATCH_ADDR_ARGNAME, -1,
        PATCH_SIZE_ARGNAME, -1,
        NULL, PATCH_PARTITIONNUM
    };

#ifdef LOSCFG_SECURITY_BOOT
    partInfo.storageType = strdup(STORAGE_TYPE);
    if (partInfo.storageType == NULL) {
        return LOS_NOK;
    }
    partInfo.fsType      = strdup(FS_TYPE);
    if (partInfo.fsType == NULL) {
        return LOS_NOK;
    }
    partInfo.startAddr   = PATCHFS_FLASH_ADDR;
    partInfo.partSize    = PATCHFS_FLASH_SIZE;
#else
    ret = GetPartitionInfo(&partInfo);
    if (ret != LOS_OK) {
        return ret;
    }
    partInfo.startAddr = (partInfo.startAddr >= 0) ? partInfo.startAddr : PATCHFS_FLASH_ADDR;
    partInfo.partSize = (partInfo.partSize >= 0) ? partInfo.partSize : PATCHFS_FLASH_SIZE;
#endif

    ret = LOS_NOK;
    partInfo.devName = strdup(PATCH_FLASH_DEV_NAME);
    if (partInfo.devName == NULL) {
        return LOS_NOK;
    }
    const CHAR *devName = GetDevNameOfPartition(&partInfo);
    if (devName != NULL) {
        ret = mkdir(PATCHFS_MOUNT_POINT, 0);
        if (ret == LOS_OK) {
            ret = mount(devName, PATCHFS_MOUNT_POINT, partInfo.fsType, MS_RDONLY, NULL);
            if (ret != LOS_OK) {
                int err = get_errno();
                PRINT_ERR("Failed to mount %s, errno %d: %s\n", partInfo.partName, err, strerror(err));
            }
        } else {
            int err = get_errno();
            PRINT_ERR("Failed to mkdir %s, errno %d: %s\n", PATCHFS_MOUNT_POINT, err, strerror(err));
        }
    }

    if ((ret != LOS_OK) && (devName != NULL)) {
        ResetDevNameofPartition(&partInfo);
    }

    free(partInfo.devName);
    free(partInfo.storageType);
    free(partInfo.fsType);
    return ret;
}

#endif // LOSCFG_PLATFORM_PATCHFS
