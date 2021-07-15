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
#include "los_blackbox.h"
#include "los_blackbox_common.h"
#include "los_blackbox_detector.h"
#ifdef LOSCFG_LIB_LIBC
#include "stdlib.h"
#include "unistd.h"
#endif
#include "los_base.h"
#include "los_config.h"
#include "los_excinfo_pri.h"
#include "los_hw.h"
#include "los_init.h"
#include "los_memory.h"
#include "los_sem.h"
#include "los_syscall.h"
#include "los_task_pri.h"
#include "securec.h"
#include "sys/reboot.h"

/* ------------ local macroes ------------ */
#define LOG_WAIT_TIMES 10
#define LOG_PART_WAIT_TIME 1000

/* ------------ local prototypes ------------ */
typedef struct BBoxOps {
    LOS_DL_LIST opsList;
    struct ModuleOps ops;
} BBoxOps;

/* ------------ local function declarations ------------ */
/* ------------ global function declarations ------------ */
/* ------------ local variables ------------ */
static bool g_bboxInitSucc = FALSE;
static UINT32 g_opsListSem = 0;
static UINT32 g_tempErrInfoSem = 0;
static UINT32 g_tempErrLogSaveSem = 0;
static LOS_DL_LIST_HEAD(g_opsList);
struct ErrorInfo *g_tempErrInfo;

/* ------------ function definitions ------------ */
static void FormatErrorInfo(struct ErrorInfo *info,
    const char event[EVENT_MAX_LEN],
    const char module[MODULE_MAX_LEN],
    const char errorDesc[ERROR_DESC_MAX_LEN])
{
    if (info == NULL || event == NULL || module == NULL || errorDesc == NULL) {
        BBOX_PRINT_ERR("info: %p, event: %p, module: %p, errorDesc: %p!\n", info, event, module, errorDesc);
        return;
    }

    (void)memset_s(info, sizeof(*info), 0, sizeof(*info));
    (void)strncpy_s(info->event, sizeof(info->event), event, Min(strlen(event), sizeof(info->event) - 1));
    (void)strncpy_s(info->module, sizeof(info->module), module, Min(strlen(module), sizeof(info->module) - 1));
    (void)strncpy_s(info->errorDesc, sizeof(info->errorDesc), errorDesc,
        Min(strlen(errorDesc), sizeof(info->errorDesc) - 1));
}

#ifdef LOSCFG_FS_VFS
static void WaitForLogPart(void)
{
    BBOX_PRINT_INFO("wait for log part [%s] begin!\n", LOSCFG_LOG_ROOT_PATH);
    while (!IsLogPartReady()) {
        LOS_Msleep(LOG_PART_WAIT_TIME);
    }
    BBOX_PRINT_INFO("wait for log part [%s] end!\n", LOSCFG_LOG_ROOT_PATH);
}
#else
static void WaitForLogPart(void)
{
    int i = 0;

    BBOX_PRINT_INFO("wait for log part [%s] begin!\n", LOSCFG_LOG_ROOT_PATH);
    while (i++ < LOG_WAIT_TIMES) {
        LOS_Msleep(LOG_PART_WAIT_TIME);
    }
    BBOX_PRINT_INFO("wait for log part [%s] end!\n", LOSCFG_LOG_ROOT_PATH);
}
#endif

static bool FindModuleOps(struct ErrorInfo *info, BBoxOps **ops)
{
    bool found = false;

    if (info == NULL || ops == NULL) {
        BBOX_PRINT_ERR("info: %p, ops: %p!\n", info, ops);
        return found;
    }

    LOS_DL_LIST_FOR_EACH_ENTRY(*ops, &g_opsList, BBoxOps, opsList) {
        if (*ops != NULL && strcmp((*ops)->ops.module, info->module) == 0) {
            found = true;
            break;
        }
    }
    if (!found) {
        BBOX_PRINT_ERR("[%s] hasn't been registered!\n", info->module);
    }

    return found;
}

static void InvokeModuleOps(struct ErrorInfo *info, BBoxOps *ops)
{
    if (info == NULL || ops == NULL) {
        BBOX_PRINT_ERR("info: %p, ops: %p!\n", info, ops);
        return;
    }

    if (ops->ops.Dump != NULL) {
        BBOX_PRINT_INFO("[%s] starts dumping log!\n", ops->ops.module);
        ops->ops.Dump(LOSCFG_LOG_ROOT_PATH, info);
        BBOX_PRINT_INFO("[%s] ends dumping log!\n", ops->ops.module);
    }
    if (ops->ops.Reset != NULL) {
        BBOX_PRINT_INFO("[%s] starts resetting!\n", ops->ops.module);
        ops->ops.Reset(info);
        BBOX_PRINT_INFO("[%s] ends resetting!\n", ops->ops.module);
    }
}

static void SaveLastLog(const char *logDir)
{
    struct ErrorInfo *info = NULL;
    BBoxOps *ops = NULL;

    info = LOS_MemAlloc(m_aucSysMem1, sizeof(*info));
    if (info == NULL) {
        BBOX_PRINT_ERR("LOS_MemAlloc failed!\n");
        return;
    }

    if (LOS_SemPend(g_opsListSem, LOS_WAIT_FOREVER) != LOS_OK) {
        BBOX_PRINT_ERR("Request g_opsListSem failed!\n");
        (void)LOS_MemFree(m_aucSysMem1, info);
        return;
    }

    LOS_DL_LIST_FOR_EACH_ENTRY(ops, &g_opsList, BBoxOps, opsList) {
        if (ops == NULL) {
            BBOX_PRINT_ERR("ops: NULL, please check it!\n");
            continue;
        }
        if (ops->ops.GetLastLogInfo != NULL && ops->ops.SaveLastLog != NULL) {
            (void)memset_s(info, sizeof(*info), 0, sizeof(*info));
            if (ops->ops.GetLastLogInfo(info) != 0) {
                BBOX_PRINT_ERR("[%s] failed to get log info!\n", ops->ops.module);
                continue;
            }
            BBOX_PRINT_INFO("[%s] starts saving log!\n", ops->ops.module);
            if (ops->ops.SaveLastLog(logDir, info) != 0) {
                BBOX_PRINT_ERR("[%s] failed to save log!\n", ops->ops.module);
            } else {
                BBOX_PRINT_INFO("[%s] ends saving log!\n", ops->ops.module);
                BBOX_PRINT_INFO("[%s] starts uploading event [%s]\n", info->module, info->event);
#ifdef LOSCFG_FS_VFS
                (void)UploadEventByFile(KERNEL_FAULT_LOG_PATH);
#else
                BBOX_PRINT_INFO("LOSCFG_FS_VFS isn't defined!\n");
#endif
                BBOX_PRINT_INFO("[%s] ends uploading event [%s]\n", info->module, info->event);
            }
        } else {
            BBOX_PRINT_ERR("module [%s], GetLastLogInfo: %p, SaveLastLog: %p!\n",
                 ops->ops.module, ops->ops.GetLastLogInfo, ops->ops.SaveLastLog);
        }
    }
    (void)LOS_SemPost(g_opsListSem);
    (void)LOS_MemFree(m_aucSysMem1, info);
}

static void SaveLogWithoutReset(struct ErrorInfo *info)
{
    BBoxOps *ops = NULL;

    if (info == NULL) {
        BBOX_PRINT_ERR("info is NULL!\n");
        return;
    }

    if (LOS_SemPend(g_opsListSem, LOS_WAIT_FOREVER) != LOS_OK) {
        BBOX_PRINT_ERR("Request g_opsListSem failed!\n");
        return;
    }
    if (!FindModuleOps(info, &ops)) {
        (void)LOS_SemPost(g_opsListSem);
        return;
    }
    if (ops->ops.Dump == NULL && ops->ops.Reset == NULL) {
        (void)LOS_SemPost(g_opsListSem);
        if (SaveBasicErrorInfo(USER_FAULT_LOG_PATH, info) == 0) {
            BBOX_PRINT_INFO("[%s] starts uploading event [%s]\n", info->module, info->event);
#ifdef LOSCFG_FS_VFS
            (void)UploadEventByFile(USER_FAULT_LOG_PATH);
#else
            BBOX_PRINT_INFO("LOSCFG_FS_VFS isn't defined!\n");
#endif
            BBOX_PRINT_INFO("[%s] ends uploading event [%s]\n", info->module, info->event);
        }
        return;
    }
    InvokeModuleOps(info, ops);
    (void)LOS_SemPost(g_opsListSem);
}

static void SaveTempErrorLog(void)
{
    if (LOS_SemPend(g_tempErrInfoSem, LOS_WAIT_FOREVER) != LOS_OK) {
        BBOX_PRINT_ERR("Request g_tempErrInfoSem failed!\n");
        return;
    }
    if (g_tempErrInfo == NULL) {
        BBOX_PRINT_ERR("g_tempErrInfo is NULL!\n");
        (void)LOS_SemPost(g_tempErrInfoSem);
        return;
    }
    if (strlen(g_tempErrInfo->event) != 0) {
        SaveLogWithoutReset(g_tempErrInfo);
    }
    (void)LOS_SemPost(g_tempErrInfoSem);
}

static void SaveLogWithReset(struct ErrorInfo *info)
{
    int ret;
    BBoxOps *ops = NULL;

    if (info == NULL) {
        BBOX_PRINT_ERR("info is NULL!\n");
        return;
    }

    if (!FindModuleOps(info, &ops)) {
        return;
    }
    InvokeModuleOps(info, ops);
    ret = SysReboot(0, 0, RB_AUTOBOOT);
    BBOX_PRINT_INFO("SysReboot, ret: %d\n", ret);
}

static void SaveTempErrorInfo(const char event[EVENT_MAX_LEN],
    const char module[MODULE_MAX_LEN],
    const char errorDesc[ERROR_DESC_MAX_LEN])
{
    if (event == NULL || module == NULL || errorDesc == NULL) {
        BBOX_PRINT_ERR("event: %p, module: %p, errorDesc: %p!\n", event, module, errorDesc);
        return;
    }
    if (LOS_SemPend(g_tempErrInfoSem, LOS_NO_WAIT) != LOS_OK) {
        BBOX_PRINT_ERR("Request g_tempErrInfoSem failed!\n");
        return;
    }
    FormatErrorInfo(g_tempErrInfo, event, module, errorDesc);
    (void)LOS_SemPost(g_tempErrInfoSem);
}

static int SaveErrorLog(UINTPTR uwParam1, UINTPTR uwParam2, UINTPTR uwParam3, UINTPTR uwParam4)
{
    const char *logDir = (const char *)uwParam1;
    (void)uwParam2;
    (void)uwParam3;
    (void)uwParam4;

#ifdef LOSCFG_FS_VFS
    WaitForLogPart();
#endif
    SaveLastLog(logDir);
    while (1) {
        if (LOS_SemPend(g_tempErrLogSaveSem, LOS_WAIT_FOREVER) != LOS_OK) {
            BBOX_PRINT_ERR("Request g_tempErrLogSaveSem failed!\n");
            continue;
        }
        SaveTempErrorLog();
    }
    return 0;
}

#ifdef LOSCFG_BLACKBOX_DEBUG
static void PrintModuleOps(void)
{
    struct BBoxOps *ops = NULL;

    BBOX_PRINT_INFO("The following modules have been registered!\n");
    LOS_DL_LIST_FOR_EACH_ENTRY(ops, &g_opsList, BBoxOps, opsList) {
        if (ops == NULL) {
            continue;
        }
        BBOX_PRINT_INFO("module: %s, Dump: %p, Reset: %p, GetLastLogInfo: %p, SaveLastLog: %p\n",
            ops->ops.module, ops->ops.Dump, ops->ops.Reset, ops->ops.GetLastLogInfo, ops->ops.SaveLastLog);
    }
}
#endif

int BBoxRegisterModuleOps(struct ModuleOps *ops)
{
    BBoxOps *newOps = NULL;
    BBoxOps *temp = NULL;

    if (!g_bboxInitSucc) {
        BBOX_PRINT_ERR("BlackBox isn't initialized successfully!\n");
        return -1;
    }
    if (ops == NULL) {
        BBOX_PRINT_ERR("ops is NULL!\n");
        return -1;
    }

    newOps = LOS_MemAlloc(m_aucSysMem1, sizeof(*newOps));
    if (newOps == NULL) {
        BBOX_PRINT_ERR("LOS_MemAlloc failed!\n");
        return -1;
    }
    (void)memset_s(newOps, sizeof(*newOps), 0, sizeof(*newOps));
    (void)memcpy_s(&newOps->ops, sizeof(newOps->ops), ops, sizeof(*ops));
    if (LOS_SemPend(g_opsListSem, LOS_WAIT_FOREVER) != LOS_OK) {
        BBOX_PRINT_ERR("Request g_opsListSem failed!\n");
        (void)LOS_MemFree(m_aucSysMem1, newOps);
        return -1;
    }
    if (LOS_ListEmpty(&g_opsList)) {
        goto __out;
    }

    LOS_DL_LIST_FOR_EACH_ENTRY(temp, &g_opsList, BBoxOps, opsList) {
        if (temp == NULL) {
            continue;
        }
        if (strcmp(temp->ops.module, ops->module) == 0) {
            BBOX_PRINT_ERR("module [%s] has been registered!\n", ops->module);
            (void)LOS_SemPost(g_opsListSem);
            (void)LOS_MemFree(m_aucSysMem1, newOps);
            return -1;
        }
    }

__out:
    LOS_ListTailInsert(&g_opsList, &newOps->opsList);
    (void)LOS_SemPost(g_opsListSem);
    BBOX_PRINT_INFO("module [%s] is registered successfully!\n", ops->module);
#ifdef LOSCFG_BLACKBOX_DEBUG
    PrintModuleOps();
#endif

    return 0;
}

int BBoxNotifyError(const char event[EVENT_MAX_LEN],
    const char module[MODULE_MAX_LEN],
    const char errorDesc[ERROR_DESC_MAX_LEN],
    int needSysReset)
{
    if (event == NULL || module == NULL || errorDesc == NULL) {
        BBOX_PRINT_ERR("event: %p, module: %p, errorDesc: %p!\n", event, module, errorDesc);
        return -1;
    }
    if (!g_bboxInitSucc) {
        BBOX_PRINT_ERR("BlackBox isn't initialized successfully!\n");
        return -1;
    }

    if (needSysReset == 0) {
        SaveTempErrorInfo(event, module, errorDesc);
        (void)LOS_SemPost(g_tempErrLogSaveSem);
    } else {
        struct ErrorInfo *info = LOS_MemAlloc(m_aucSysMem1, sizeof(struct ErrorInfo));
        if (info == NULL) {
            BBOX_PRINT_ERR("LOS_MemAlloc failed!\n");
            return -1;
        }
        FormatErrorInfo(info, event, module, errorDesc);
        SaveLogWithReset(info);
        (void)LOS_MemFree(m_aucSysMem1, info);
    }

    return 0;
}

int OsBBoxDriverInit(void)
{
    UINT32 taskID;
    TSK_INIT_PARAM_S taskParam;

    if (LOS_BinarySemCreate(1, &g_opsListSem) != LOS_OK) {
        BBOX_PRINT_ERR("Create g_opsListSem failed!\n");
        return LOS_NOK;
    }
    if (LOS_BinarySemCreate(1, &g_tempErrInfoSem) != LOS_OK) {
        BBOX_PRINT_ERR("Create g_tempErrInfoSem failed!\n");
        goto __err;
    }
    if (LOS_BinarySemCreate(0, &g_tempErrLogSaveSem) != LOS_OK) {
        BBOX_PRINT_ERR("Create g_tempErrLogSaveSem failed!\n");
        goto __err;
    }
    LOS_ListInit(&g_opsList);
    g_tempErrInfo = LOS_MemAlloc(m_aucSysMem1, sizeof(*g_tempErrInfo));
    if (g_tempErrInfo == NULL) {
        BBOX_PRINT_ERR("LOS_MemAlloc failed!\n");
        goto __err;
    }
    (void)memset_s(g_tempErrInfo, sizeof(*g_tempErrInfo), 0, sizeof(*g_tempErrInfo));
    (void)memset_s(&taskParam, sizeof(taskParam), 0, sizeof(taskParam));
    taskParam.auwArgs[0] = (UINTPTR)LOSCFG_LOG_ROOT_PATH;
    taskParam.pfnTaskEntry = (TSK_ENTRY_FUNC)SaveErrorLog;
    taskParam.uwStackSize = LOSCFG_BASE_CORE_TSK_DEFAULT_STACK_SIZE;
    taskParam.pcName = "SaveErrorLog";
    taskParam.usTaskPrio = LOSCFG_BASE_CORE_TSK_DEFAULT_PRIO;
    taskParam.uwResved = LOS_TASK_STATUS_DETACHED;
#ifdef LOSCFG_KERNEL_SMP
    taskParam.usCpuAffiMask = CPUID_TO_AFFI_MASK(ArchCurrCpuid());
#endif
    (void)LOS_TaskCreate(&taskID, &taskParam);
    g_bboxInitSucc = TRUE;
    return LOS_OK;

__err:
    if (g_opsListSem != 0) {
        (void)LOS_SemDelete(g_opsListSem);
    }
    if (g_tempErrInfoSem != 0) {
        (void)LOS_SemDelete(g_tempErrInfoSem);
    }
    if (g_tempErrLogSaveSem != 0) {
        (void)LOS_SemDelete(g_tempErrLogSaveSem);
    }
    return LOS_NOK;
}
LOS_MODULE_INIT(OsBBoxDriverInit, LOS_INIT_LEVEL_ARCH);