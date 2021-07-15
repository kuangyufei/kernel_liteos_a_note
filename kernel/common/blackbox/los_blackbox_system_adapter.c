/*
 * Copyright (c) 2021-2021 Huawei Device Co., Ltd. All rights reserved.
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

/* ------------ includes ------------ */
#include "los_blackbox_system_adapter.h"
#include "los_blackbox_common.h"
#include "los_blackbox_detector.h"
#ifdef LOSCFG_LIB_LIBC
#include "stdlib.h"
#include "unistd.h"
#endif
#include "los_base.h"
#include "los_config.h"
#ifdef LOSCFG_SAVE_EXCINFO
#include "los_excinfo_pri.h"
#endif
#include "los_hw.h"
#include "los_init.h"
#include "los_memory.h"
#include "los_vm_phys.h"
#include "los_vm_common.h"
#include "securec.h"

/* ------------ local macroes ------------ */
#define MEM_OVERLAP_COUNT 50
#define LOG_FLAG "GOODLOG"
#define FAULT_LOG_SIZE 0x4000 /* 16KB */

/* ------------ local prototypes ------------ */
struct FaultLogInfo {
    char flag[8]; /* 8 is the length of the flag */
    int len; /* length of the fault log saved by the module excinfo */
    struct ErrorInfo info;
};

/* ------------ local function declarations ------------ */
/* ------------ global function declarations ------------ */
/* ------------ local variables ------------ */
static char *g_logBuffer = NULL;

/* ------------ function definitions ------------ */
static void SaveFaultLog(const char *filePath, const char *dataBuf, size_t bufSize, struct ErrorInfo *info)
{
    (void)SaveBasicErrorInfo(filePath, info);
    (void)FullWriteFile(filePath, dataBuf, bufSize, 1);
}

#ifdef LOSCFG_SAVE_EXCINFO
static void WriteExcFile(UINT32 startAddr, UINT32 space, UINT32 rwFlag, char *buf)
{
    (void)startAddr;
    (void)space;
    (void)rwFlag;
    (void)buf;
}
#endif

static void RegisterExcInfoHook(void)
{
    if (g_logBuffer != NULL) {
#ifdef LOSCFG_SAVE_EXCINFO
        LOS_ExcInfoRegHook(0, FAULT_LOG_SIZE - sizeof(struct FaultLogInfo),
            g_logBuffer + sizeof(struct FaultLogInfo), WriteExcFile);
#endif
    } else {
        BBOX_PRINT_ERR("Alloc mem failed!\n");
    }
}

static int AllocLogBuffer(void)
{
    int i = 0;
    size_t nPages = ROUNDUP(FAULT_LOG_SIZE, PAGE_SIZE) >> PAGE_SHIFT;
    void *tempBuffer[MEM_OVERLAP_COUNT] = { NULL };

    for (i = 0; i < MEM_OVERLAP_COUNT; i++) {
        tempBuffer[i] = LOS_PhysPagesAllocContiguous(nPages);
    }
    for (i = 0; i < (MEM_OVERLAP_COUNT - 1); i++) {
        LOS_PhysPagesFreeContiguous(tempBuffer[i], nPages);
    }
    g_logBuffer = tempBuffer[i];
    BBOX_PRINT_INFO("g_logBuffer: %p for blackbox!\n", g_logBuffer);

    return (g_logBuffer != NULL) ? 0 : -1;
}

static void Dump(const char *logDir, struct ErrorInfo *info)
{
    struct FaultLogInfo *pLogInfo = NULL;

    if (logDir == NULL || info == NULL) {
        BBOX_PRINT_ERR("logDir: %p, info: %p!\n", logDir, info);
        return;
    }
    if (g_logBuffer == NULL) {
        BBOX_PRINT_ERR("g_logBuffer is NULL, alloc physical pages failed!\n");
        return;
    }

    if (strcmp(info->event, EVENT_PANIC) == 0) {
        pLogInfo = (struct FaultLogInfo *)g_logBuffer;
        (void)memset_s(pLogInfo, sizeof(*pLogInfo), 0, sizeof(*pLogInfo));
#ifdef LOSCFG_SAVE_EXCINFO
        pLogInfo->len = GetExcInfoIndex();
#else
        pLogInfo->len = 0;
#endif
        (void)memcpy_s(&pLogInfo->flag, sizeof(pLogInfo->flag), LOG_FLAG, strlen(LOG_FLAG));
        (void)memcpy_s(&pLogInfo->info, sizeof(pLogInfo->info), info, sizeof(*info));
        DCacheFlushRange((UINTPTR)g_logBuffer, (UINTPTR)(g_logBuffer + FAULT_LOG_SIZE));
    } else {
#ifdef LOSCFG_SAVE_EXCINFO
        SaveFaultLog(USER_FAULT_LOG_PATH, g_logBuffer + sizeof(struct FaultLogInfo),
            Min(FAULT_LOG_SIZE - sizeof(struct FaultLogInfo), GetExcInfoIndex()), info);
#else
        SaveFaultLog(USER_FAULT_LOG_PATH, g_logBuffer + sizeof(struct FaultLogInfo), 0, info);
#endif
    }
}

static void Reset(struct ErrorInfo *info)
{
    if (info == NULL) {
        BBOX_PRINT_ERR("info is NULL!\n");
        return;
    }

    if (strcmp(info->event, EVENT_PANIC) != 0) {
        BBOX_PRINT_INFO("[%s] starts uploading event [%s]\n", info->module, info->event);
        (void)UploadEventByFile(USER_FAULT_LOG_PATH);
        BBOX_PRINT_INFO("[%s] ends uploading event [%s]\n", info->module, info->event);
    }
}

static int GetLastLogInfo(struct ErrorInfo *info)
{
    struct FaultLogInfo *pLogInfo = NULL;

    if (info == NULL) {
        BBOX_PRINT_ERR("info is NULL!\n");
        return -1;
    }
    if (g_logBuffer == NULL) {
        BBOX_PRINT_ERR("g_logBuffer is NULL, alloc physical pages failed!\n");
        return -1;
    }

    pLogInfo = (struct FaultLogInfo *)g_logBuffer;
    if (memcmp(pLogInfo->flag, LOG_FLAG, strlen(LOG_FLAG)) == 0) {
        (void)memcpy_s(info, sizeof(*info), &pLogInfo->info, sizeof(pLogInfo->info));
        return 0;
    }

    return -1;
}

static int SaveLastLog(const char *logDir, struct ErrorInfo *info)
{
#ifdef LOSCFG_FS_VFS
    struct FaultLogInfo *pLogInfo = NULL;

    if (logDir == NULL || info == NULL) {
        BBOX_PRINT_ERR("logDir: %p, info: %p!\n", logDir, info);
        return -1;
    }
    if (g_logBuffer == NULL) {
        BBOX_PRINT_ERR("g_logBuffer is NULL, alloc physical pages failed!\n");
        return -1;
    }

    pLogInfo = (struct FaultLogInfo *)g_logBuffer;
    if (memcmp(pLogInfo->flag, LOG_FLAG, strlen(LOG_FLAG)) == 0) {
        SaveFaultLog(KERNEL_FAULT_LOG_PATH, g_logBuffer + sizeof(*pLogInfo),
            Min(FAULT_LOG_SIZE - sizeof(*pLogInfo), pLogInfo->len), info);
    }
    (void)memset_s(g_logBuffer, FAULT_LOG_SIZE, 0, FAULT_LOG_SIZE);
    BBOX_PRINT_INFO("[%s] starts uploading event [%s]\n", info->module, info->event);
    (void)UploadEventByFile(KERNEL_FAULT_LOG_PATH);
    BBOX_PRINT_INFO("[%s] ends uploading event [%s]\n", info->module, info->event);
    return 0;
#else
    (VOID)logDir;
    (VOID)info;
    BBOX_PRINT_ERR("LOSCFG_FS_VFS isn't defined!\n");
    return -1;
#endif
}

#ifdef LOSCFG_BLACKBOX_TEST
static void BBoxTest(void)
{
      struct ModuleOps ops = {
        .module = "MODULE_TEST",
        .Dump = NULL,
        .Reset = NULL,
        .GetLastLogInfo = NULL,
        .SaveLastLog = NULL,
    };

    if (BBoxRegisterModuleOps(&ops) != 0) {
        BBOX_PRINT_ERR("BBoxRegisterModuleOps failed!\n");
        return;
    }
    BBoxNotifyError("EVENT_TEST1", "MODULE_TEST", "Test BBoxNotifyError111", 0);
}
#endif

int OsBBoxSystemAdapterInit(void)
{
    struct ModuleOps ops = {
        .module = MODULE_SYSTEM,
        .Dump = Dump,
        .Reset = Reset,
        .GetLastLogInfo = GetLastLogInfo,
        .SaveLastLog = SaveLastLog,
    };

    /* allocate buffer for kmsg */
    if (AllocLogBuffer() == 0) {
        RegisterExcInfoHook();
        if (BBoxRegisterModuleOps(&ops) != 0) {
            BBOX_PRINT_ERR("BBoxRegisterModuleOps failed!\n");
            LOS_PhysPagesFreeContiguous(g_logBuffer, ROUNDUP(FAULT_LOG_SIZE, PAGE_SIZE) >> PAGE_SHIFT);
            g_logBuffer = NULL;
            return LOS_NOK;
        }
    } else {
        BBOX_PRINT_ERR("AllocLogBuffer failed!\n");
    }

#ifdef LOSCFG_BLACKBOX_TEST
    BBoxTest();
#endif

    return LOS_OK;
}
LOS_MODULE_INIT(OsBBoxSystemAdapterInit, LOS_INIT_LEVEL_PLATFORM);