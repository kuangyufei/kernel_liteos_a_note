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
#ifndef _LOS_PARTITION_UTILS_H
#define _LOS_PARTITION_UTILS_H

#include "los_typedef.h"
#include "los_base.h"

#define BYTES_PER_MBYTE         0x100000
#define BYTES_PER_KBYTE         0x400
#define DEC_NUMBER_STRING       "0123456789"
#define HEX_NUMBER_STRING       "0123456789abcdefABCDEF"

#define COMMAND_LINE_ADDR       LOSCFG_BOOTENV_ADDR * BYTES_PER_KBYTE
#define COMMAND_LINE_SIZE       1024

#if defined(LOSCFG_STORAGE_SPINOR)
#define FLASH_TYPE              "spinor"
#define STORAGE_TYPE            "flash"
#define FS_TYPE                 "jffs2"
#else
#define STORAGE_TYPE            "emmc"
#endif

struct PartitionInfo {
    const CHAR   *partName;
    const CHAR   *cmdlineArgName;
    const CHAR   *storageTypeArgName;
    CHAR         *storageType;
    const CHAR   *fsTypeArgName;
    CHAR         *fsType;
    const CHAR   *addrArgName;
    INT32        startAddr;
    const CHAR   *partSizeArgName;
    INT32        partSize;
    CHAR         *devName;
    UINT32       partNum;
};

INT32 GetPartitionInfo(struct PartitionInfo *partInfo);
const CHAR *GetDevNameOfPartition(const struct PartitionInfo *partInfo);
INT32 ResetDevNameofPartition(const struct PartitionInfo *partInfo);

#endif /* _LOS_PARTITION_UTILS_H */
