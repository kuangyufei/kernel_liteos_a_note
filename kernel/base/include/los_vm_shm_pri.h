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

/**
 * @defgroup shm_config 共享内存(Shared Memory)配置参数
 * @brief 定义共享内存系统的资源限制和管理参数
 * @{ 
 */
/* 系统共享内存总上限默认值：16MB (由SHM_MAX_PAGES和PAGE_SIZE计算得出) */
#define SHM_MAX_PAGES 4096                      /*!< 共享内存最大页数：4096页 (4096 * 4KB = 16MB) */
#define SHM_MAX       (SHM_MAX_PAGES * PAGE_SIZE)/*!< 共享内存最大总大小：16MB (PAGE_SIZE通常为4KB) */
#define SHM_MIN       1                          /*!< 共享内存最小分配粒度：1字节 */
#define SHM_MNI       192                        /*!< 系统最大共享内存标识符数量：192个 */
#define SHM_SEG       128                        /*!< 每个进程最大共享内存段数量：128个 */
#define SHM_ALL       (SHM_MAX_PAGES)            /*!< 系统共享内存总页数：等于SHM_MAX_PAGES */
/** @} */

/**
 * @brief 共享内存ID资源管理结构体
 * @details 用于跟踪和管理系统中的共享内存段元数据
 */
struct shmIDSource {
    struct shmid_ds ds;                          /*!< 共享内存段描述符，符合POSIX标准 */
    UINT32 status;                              /*!< 共享内存段状态：0-未使用，1-已分配，2-销毁中 */
    LOS_DL_LIST node;                           /*!< 双向链表节点，用于链接所有共享内存ID */
#ifdef LOSCFG_SHELL
    CHAR ownerName[OS_PCB_NAME_LEN];             /*!< 共享内存创建者进程名，仅在SHELL使能时有效 */
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
