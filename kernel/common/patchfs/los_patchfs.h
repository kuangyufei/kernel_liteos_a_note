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
#ifndef _LOS_PATCHFS_H
#define _LOS_PATCHFS_H

#include "los_typedef.h"
#include "los_base.h"

#define __stringify_1(x...)     #x
#define __stringify(x...)       __stringify_1(x)

#define PATCH_PARTITIONNUM      1
#ifdef TEE_ENABLE
#define PATCHFS_FLASH_ADDR      0xC00000
#define PATCHFS_FLASH_SIZE      0x200000
#else
#define PATCHFS_FLASH_ADDR      0xD00000
#define PATCHFS_FLASH_SIZE      0x300000
#endif

#if defined(LOSCFG_STORAGE_SPINOR)
#define PATCH_FLASH_DEV_NAME          "/dev/spinorblk"__stringify(PATCH_PARTITIONNUM)
#else
#define PATCH_FLASH_DEV_NAME          "/dev/mmcblk"__stringify(PATCH_PARTITIONNUM)
#endif

#define PATCHFS_MOUNT_POINT     "/patch"

#define PATCHPART_NAME          "patchfs"
#define PATCH_CMDLINE_ARGNAME   "patchfs="
#define PATCH_STORAGE_ARGNAME   "patch="
#define PATCH_FSTYPE_ARGNAME    "patchfstype="
#define PATCH_ADDR_ARGNAME      "patchaddr="
#define PATCH_SIZE_ARGNAME      "patchsize="

INT32 OsMountPatchFs(VOID);

#endif /* _LOS_PATCHFS_H */
