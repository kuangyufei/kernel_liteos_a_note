/*
 * Copyright (c) 2013-2019 Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020-2023 Huawei Device Co., Ltd. All rights reserved.
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

#ifndef __LOS_SHM_PRI_H__
#define __LOS_SHM_PRI_H__

#include "los_typedef.h"
#include "los_vm_map.h"
#include "los_process_pri.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

/* The upper limit size of total shared memory is default 16M */
#define SHM_MAX_PAGES 4096
#define SHM_MAX (SHM_MAX_PAGES * PAGE_SIZE)
#define SHM_MIN 1
#define SHM_MNI 192
#define SHM_SEG 128
#define SHM_ALL (SHM_MAX_PAGES)

struct shmIDSource {
    struct shmid_ds ds;
    UINT32 status;
    LOS_DL_LIST node;
#ifdef LOSCFG_SHELL
    CHAR ownerName[OS_PCB_NAME_LEN];
#endif
};

VOID OsShmFork(LosVmSpace *space, LosVmMapRegion *oldRegion, LosVmMapRegion *newRegion);
VOID OsShmRegionFree(LosVmSpace *space, LosVmMapRegion *region);
BOOL OsIsShmRegion(LosVmMapRegion *region);
struct shmIDSource *OsShmCBInit(LosMux *sysvShmMux, struct shminfo *shmInfo, UINT32 *shmUsedPageCount);
VOID OsShmCBDestroy(struct shmIDSource *shmSegs, struct shminfo *shmInfo, LosMux *sysvShmMux);
#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* __LOS_SHM_PRI_H__ */
