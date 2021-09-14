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
#include "los_partition_utils.h"
#if defined(LOSCFG_STORAGE_SPINOR)
#include "mtd_partition.h"
#endif

STATIC INT32 MatchPartPos(CHAR *p, const CHAR *partInfoName, INT32 *partInfo)
{
    UINT32 offset;
    CHAR *value = NULL;

    if (strncmp(p, partInfoName, strlen(partInfoName)) == 0) {
        value = p + strlen(partInfoName);
        offset = strspn(value, DEC_NUMBER_STRING);
        if (strcmp(p + strlen(p) - 1, "M") == 0) {
            if ((offset < strlen(value) - 1) || (sscanf_s(value, "%d", partInfo) <= 0)) {
                goto ERROUT;
            }
            *partInfo = *partInfo * BYTES_PER_MBYTE;
        } else if (strcmp(p + strlen(p) - 1, "K") == 0) {
            if ((offset < (strlen(value) - 1)) || (sscanf_s(value, "%d", partInfo) <= 0)) {
                goto ERROUT;
            }
            *partInfo = *partInfo * BYTES_PER_KBYTE;
        } else if (sscanf_s(value, "0x%x", partInfo) > 0) {
            value += strlen("0x");
            if (strspn(value, HEX_NUMBER_STRING) < strlen(value)) {
                goto ERROUT;
            }
        } else {
            goto ERROUT;
        }
    }

    return LOS_OK;

ERROUT:
    PRINT_ERR("Invalid format: %s\n", p + strlen(partInfoName));
    return LOS_NOK;
}

STATIC INT32 MatchPartInfo(CHAR *p, struct PartitionInfo *partInfo)
{
    const CHAR *storageTypeArgName = partInfo->storageTypeArgName;
    const CHAR *fsTypeArgName      = partInfo->fsTypeArgName;
    const CHAR *addrArgName        = partInfo->addrArgName;
    const CHAR *partSizeArgName    = partInfo->partSizeArgName;

    if ((partInfo->storageType == NULL) && (strncmp(p, storageTypeArgName, strlen(storageTypeArgName)) == 0)) {
        partInfo->storageType = strdup(p + strlen(storageTypeArgName));
        if (partInfo->storageType == NULL) {
            return LOS_NOK;
        }
        return LOS_OK;
    }

    if ((partInfo->fsType == NULL) && (strncmp(p, fsTypeArgName, strlen(fsTypeArgName)) == 0)) {
        partInfo->fsType = strdup(p + strlen(fsTypeArgName));
        if (partInfo->fsType == NULL) {
            return LOS_NOK;
        }
        return LOS_OK;
    }

    if (partInfo->startAddr < 0) {
        if (MatchPartPos(p, addrArgName, &partInfo->startAddr) != LOS_OK) {
            return LOS_NOK;
        } else if (partInfo->startAddr >= 0) {
            return LOS_OK;
        }
    }

    if (partInfo->partSize < 0) {
        if (MatchPartPos(p, partSizeArgName, &partInfo->partSize) != LOS_OK) {
            return LOS_NOK;
        }
    }

    return LOS_OK;
}

STATIC INT32 GetPartitionBootArgs(const CHAR *argName, CHAR **args)
{
    INT32 i;
    INT32 len = 0;
    CHAR *cmdLine = NULL;
    INT32 cmdLineLen;
    CHAR *tmp = NULL;

    cmdLine = (CHAR *)malloc(COMMAND_LINE_SIZE);
    if (cmdLine == NULL) {
        PRINT_ERR("Malloc cmdLine space failed!\n");
        return LOS_NOK;
    }

#if defined(LOSCFG_STORAGE_SPINOR)
    struct MtdDev *mtd = GetMtd(FLASH_TYPE);
    if (mtd == NULL) {
        PRINT_ERR("Get spinor mtd failed!\n");
        goto ERROUT;
    }
    cmdLineLen = mtd->read(mtd, COMMAND_LINE_ADDR, COMMAND_LINE_SIZE, cmdLine);
    if ((cmdLineLen != COMMAND_LINE_SIZE)) {
        PRINT_ERR("Read spinor command line failed!\n");
        goto ERROUT;
    }
#else
    cmdLineLen = 0;
#endif

    for (i = 0; i < cmdLineLen; i += len + 1) {
        len = strlen(cmdLine + i);
        tmp = strstr(cmdLine + i, argName);
        if (tmp != NULL) {
            *args = strdup(tmp + strlen(argName));
            if (*args == NULL) {
                goto ERROUT;
            }
            free(cmdLine);
            return LOS_OK;
        }
    }

    PRINTK("no patch partition bootargs\n");

ERROUT:
    free(cmdLine);
    return LOS_NOK;
}

INT32 GetPartitionInfo(struct PartitionInfo *partInfo)
{
    CHAR *args    = NULL;
    CHAR *argsBak = NULL;
    CHAR *p       = NULL;

    if (GetPartitionBootArgs(partInfo->cmdlineArgName, &args) != LOS_OK) {
        return LOS_NOK;
    }
    argsBak = args;

    p = strsep(&args, " ");
    while (p != NULL) {
        if (MatchPartInfo(p, partInfo) != LOS_OK) {
            goto ERROUT;
        }
        p = strsep(&args, " ");
    }
    if ((partInfo->fsType != NULL) && (partInfo->storageType != NULL)) {
        free(argsBak);
        return LOS_OK;
    }
    PRINT_ERR("Cannot find %s type\n", partInfo->partName);

ERROUT:
    PRINT_ERR("Invalid %s information!\n", partInfo->partName);
    if (partInfo->storageType != NULL) {
        free(partInfo->storageType);
        partInfo->storageType = NULL;
    }
    if (partInfo->fsType != NULL) {
        free(partInfo->fsType);
        partInfo->fsType = NULL;
    }
    free(argsBak);

    return LOS_NOK;
}

const CHAR *GetDevNameOfPartition(const struct PartitionInfo *partInfo)
{
    const CHAR *devName = NULL;

    if (strcmp(partInfo->storageType, STORAGE_TYPE) == 0) {
#if defined(LOSCFG_STORAGE_SPINOR)
        INT32 ret = add_mtd_partition(FLASH_TYPE, partInfo->startAddr, partInfo->partSize, partInfo->partNum);
        if (ret != LOS_OK) {
            PRINT_ERR("Failed to add %s partition! error = %d\n", partInfo->partName, ret);
        } else {
            if (partInfo->devName != NULL) {
                devName = partInfo->devName;
            }
        }
#endif
    } else {
        PRINT_ERR("Failed to find %s dev type: %s\n", partInfo->partName, partInfo->storageType);
    }

    return devName;
}

INT32 ResetDevNameofPartition(const struct PartitionInfo *partInfo)
{
    INT32 ret;
#if defined(LOSCFG_STORAGE_SPINOR)
    ret = delete_mtd_partition(partInfo->partNum, FLASH_TYPE);
    if (ret != ENOERR) {
        int err = get_errno();
        PRINT_ERR("Failed to delete %s, errno %d: %s\n", partInfo->devName, err, strerror(err));
        ret = LOS_NOK;
    }
#else
    ret = LOS_NOK;
#endif
    return ret;
}
