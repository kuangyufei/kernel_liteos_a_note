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

#ifndef _PROC_INTERNAL_H
#define _PROC_INTERNAL_H

#include "proc_fs.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

#define MAX_NON_LFS ((1UL << 31) - 1)

extern spinlock_t procfsLock;
extern bool procfsInit;

#ifdef LOSCFG_PROC_PROCESS_DIR
int ProcCreateProcessDir(UINT32 pid, uintptr_t process);

void ProcFreeProcessDir(struct ProcDirEntry *processDir);

void ProcSysMemInfoInit(void);

void ProcFileSysInit(void);
#endif

#ifdef LOSCFG_KERNEL_PLIMITS
void ProcLimitsInit(void);
#endif

void ProcEntryClearVnode(struct ProcDirEntry *entry);

void ProcDetachNode(struct ProcDirEntry *pn);

void RemoveProcEntryTravalsal(struct ProcDirEntry *pn);

void ProcPmInit(void);

void ProcVmmInit(void);

void ProcProcessInit(void);

int ProcMatch(unsigned int len, const char *name, struct ProcDirEntry *pde);

struct ProcDirEntry *ProcFindEntry(const char *path);

void ProcFreeEntry(struct ProcDirEntry *pde);

int ProcStat(const char *file, struct ProcStat *buf);

void ProcMountsInit(void);

void ProcUptimeInit(void);

void ProcFsCacheInit(void);

void ProcFdInit(void);

#ifdef LOSCFG_KERNEL_CONTAINER
void *ProcfsContainerGet(int fd, unsigned int *containerType);
void ProcSysUserInit(void);
#endif

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */
#endif
