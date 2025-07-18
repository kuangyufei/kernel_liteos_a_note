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
#define LOG_WAIT_TIMES 10                                  // 日志分区等待尝试次数
#define LOG_PART_WAIT_TIME 1000                            // 日志分区等待间隔时间(毫秒)

/* ------------ local prototypes ------------ */
/**
 * @brief 黑盒模块操作集结构体
 * @note 用于管理黑盒系统中各模块的操作接口，通过链表节点挂载到全局操作链表
 */
typedef struct BBoxOps {
    LOS_DL_LIST opsList;                                    // 链表节点，用于挂载到全局操作链表
    struct ModuleOps ops;                                   // 模块操作接口集合
} BBoxOps;

/* ------------ local function declarations ------------ */
/* ------------ global function declarations ------------ */
/* ------------ local variables ------------ */
static bool g_bboxInitSucc = FALSE;                         // 黑盒初始化成功标志：FALSE-未初始化，TRUE-已初始化
static UINT32 g_opsListSem = 0;                             // 操作链表保护信号量
static UINT32 g_tempErrInfoSem = 0;                         // 临时错误信息信号量
static UINT32 g_tempErrLogSaveSem = 0;                      // 临时错误日志保存信号量
static LOS_DL_LIST_HEAD(g_opsList);                         // 全局操作链表头节点
struct ErrorInfo *g_tempErrInfo;                            // 临时错误信息结构体指针

/* ------------ function definitions ------------ */
/**
 * @brief 格式化错误信息到结构体
 * @param info 错误信息结构体指针，用于存储格式化后的错误信息
 * @param event 事件名称字符串，长度不超过EVENT_MAX_LEN
 * @param module 模块名称字符串，长度不超过MODULE_MAX_LEN
 * @param errorDesc 错误描述字符串，长度不超过ERROR_DESC_MAX_LEN
 * @note 所有输入指针参数必须非空，否则会打印错误信息并直接返回
 */
static void FormatErrorInfo(struct ErrorInfo *info,
                            const char event[EVENT_MAX_LEN],
                            const char module[MODULE_MAX_LEN],
                            const char errorDesc[ERROR_DESC_MAX_LEN])
{
    // 参数有效性检查：任何指针为空则打印错误信息并返回
    if (info == NULL || event == NULL || module == NULL || errorDesc == NULL) {
        BBOX_PRINT_ERR("info: %p, event: %p, module: %p, errorDesc: %p!\n", info, event, module, errorDesc);
        return;
    }

    (void)memset_s(info, sizeof(*info), 0, sizeof(*info));  // 清空错误信息结构体
    // 拷贝事件名称，确保不越界
    if (strncpy_s(info->event, sizeof(info->event), event, Min(strlen(event), sizeof(info->event) - 1)) != EOK) {
        BBOX_PRINT_ERR("info->event is not enough or strncpy_s failed!\n");
    }
    // 拷贝模块名称，确保不越界
    if (strncpy_s(info->module, sizeof(info->module), module, Min(strlen(module), sizeof(info->module) - 1)) != EOK) {
        BBOX_PRINT_ERR("info->module is not enough or strncpy_s failed!\n");
    }
    // 拷贝错误描述，确保不越界
    if (strncpy_s(info->errorDesc, sizeof(info->errorDesc), errorDesc,
        Min(strlen(errorDesc), sizeof(info->errorDesc) - 1)) != EOK) {
        BBOX_PRINT_ERR("info->errorDesc is not enough or strncpy_s failed!\n");
    }
}

#ifdef LOSCFG_FS_VFS
/**
 * @brief VFS模式下等待日志分区就绪
 * @note 循环等待直到IsLogPartReady()返回TRUE，表示日志分区已挂载
 */
static void WaitForLogPart(void)
{
    BBOX_PRINT_INFO("wait for log part [%s] begin!\n", LOSCFG_BLACKBOX_LOG_PART_MOUNT_POINT);
    while (!IsLogPartReady()) {                             // 检查日志分区是否就绪
        LOS_Msleep(LOG_PART_WAIT_TIME);                     // 未就绪则等待指定间隔
    }
    BBOX_PRINT_INFO("wait for log part [%s] end!\n", LOSCFG_BLACKBOX_LOG_PART_MOUNT_POINT);
}
#else
/**
 * @brief 非VFS模式下等待日志分区就绪
 * @note 有限次数等待（LOG_WAIT_TIMES次），每次等待LOG_PART_WAIT_TIME毫秒
 */
static void WaitForLogPart(void)
{
    int i = 0;

    BBOX_PRINT_INFO("wait for log part [%s] begin!\n", LOSCFG_BLACKBOX_LOG_PART_MOUNT_POINT);
    while (i++ < LOG_WAIT_TIMES) {                          // 最多等待LOG_WAIT_TIMES次
        LOS_Msleep(LOG_PART_WAIT_TIME);                     // 每次等待指定间隔
    }
    BBOX_PRINT_INFO("wait for log part [%s] end!\n", LOSCFG_BLACKBOX_LOG_PART_MOUNT_POINT);
}
#endif

/**
 * @brief 根据错误信息查找对应的模块操作集
 * @param info 错误信息结构体，包含目标模块名称
 * @param ops 输出参数，用于返回找到的模块操作集指针
 * @return TRUE-找到对应模块操作集，FALSE-未找到
 * @note 查找过程通过遍历全局操作链表g_opsList实现，匹配模块名称字符串
 */
static bool FindModuleOps(struct ErrorInfo *info, BBoxOps **ops)
{
    bool found = false;                                     // 查找结果标志

    // 参数有效性检查：info或ops为空则直接返回失败
    if (info == NULL || ops == NULL) {
        BBOX_PRINT_ERR("info: %p, ops: %p!\n", info, ops);
        return found;
    }

    // 遍历全局操作链表，查找模块名称匹配的操作集
    LOS_DL_LIST_FOR_EACH_ENTRY(*ops, &g_opsList, BBoxOps, opsList) {
        if (*ops != NULL && strcmp((*ops)->ops.module, info->module) == 0) {
            found = true;                                   // 找到匹配模块，设置标志并退出循环
            break;
        }
    }
    if (!found) {
        BBOX_PRINT_ERR("[%s] hasn't been registered!\n", info->module); // 未找到时打印错误信息
    }

    return found;
}

/**
 * @brief 调用模块操作集中的Dump和Reset接口
 * @param info 错误信息结构体，包含事件和模块信息
 * @param ops 模块操作集指针，包含要调用的接口函数
 * @note 仅当接口函数指针非空时才调用对应的操作
 */
static void InvokeModuleOps(struct ErrorInfo *info, const BBoxOps *ops)
{
    // 参数有效性检查：info或ops为空则直接返回
    if (info == NULL || ops == NULL) {
        BBOX_PRINT_ERR("info: %p, ops: %p!\n", info, ops);
        return;
    }

    // 如果Dump接口存在，则调用模块日志转储功能
    if (ops->ops.Dump != NULL) {
        BBOX_PRINT_INFO("[%s] starts dumping log!\n", ops->ops.module);
        ops->ops.Dump(LOSCFG_BLACKBOX_LOG_ROOT_PATH, info);
        BBOX_PRINT_INFO("[%s] ends dumping log!\n", ops->ops.module);
    }
    // 如果Reset接口存在，则调用模块重置功能
    if (ops->ops.Reset != NULL) {
        BBOX_PRINT_INFO("[%s] starts resetting!\n", ops->ops.module);
        ops->ops.Reset(info);
        BBOX_PRINT_INFO("[%s] ends resetting!\n", ops->ops.module);
    }
}

/**
 * @brief 保存各模块最后日志并触发事件上传
 * @param logDir 日志保存目录路径
 * @note 函数会遍历所有已注册模块，调用其GetLastLogInfo和SaveLastLog接口
 *       操作链表期间会获取g_opsListSem信号量进行互斥保护
 */
static void SaveLastLog(const char *logDir)
{
    struct ErrorInfo *info = NULL;                          // 错误信息结构体指针
    BBoxOps *ops = NULL;                                    // 模块操作集指针

    // 分配错误信息结构体内存
    info = LOS_MemAlloc(m_aucSysMem1, sizeof(*info));
    if (info == NULL) {
        BBOX_PRINT_ERR("LOS_MemAlloc failed!\n");          // 内存分配失败时打印错误
        return;
    }

    // 获取操作链表信号量，确保遍历链表期间的线程安全
    if (LOS_SemPend(g_opsListSem, LOS_WAIT_FOREVER) != LOS_OK) {
        BBOX_PRINT_ERR("Request g_opsListSem failed!\n");
        (void)LOS_MemFree(m_aucSysMem1, info);              // 释放已分配的内存
        return;
    }
    // 创建日志根目录，失败则释放资源并返回
    if (CreateLogDir(LOSCFG_BLACKBOX_LOG_ROOT_PATH) != 0) {
        (void)LOS_SemPost(g_opsListSem);                    // 释放信号量
        (void)LOS_MemFree(m_aucSysMem1, info);              // 释放内存
        BBOX_PRINT_ERR("Create log dir [%s] failed!\n", LOSCFG_BLACKBOX_LOG_ROOT_PATH);
        return;
    }
    // 遍历所有已注册的模块操作集
    LOS_DL_LIST_FOR_EACH_ENTRY(ops, &g_opsList, BBoxOps, opsList) {
        if (ops == NULL) {
            BBOX_PRINT_ERR("ops: NULL, please check it!\n"); // 跳过空操作集
            continue;
        }
        // 检查模块是否实现了必要的日志接口
        if (ops->ops.GetLastLogInfo != NULL && ops->ops.SaveLastLog != NULL) {
            (void)memset_s(info, sizeof(*info), 0, sizeof(*info)); // 清空错误信息结构体
            // 获取模块最后日志信息
            if (ops->ops.GetLastLogInfo(info) != 0) {
                BBOX_PRINT_ERR("[%s] failed to get log info!\n", ops->ops.module);
                continue;                                   // 获取失败则处理下一个模块
            }
            // 保存模块最后日志
            BBOX_PRINT_INFO("[%s] starts saving log!\n", ops->ops.module);
            if (ops->ops.SaveLastLog(logDir, info) != 0) {
                BBOX_PRINT_ERR("[%s] failed to save log!\n", ops->ops.module);
            } else {
                BBOX_PRINT_INFO("[%s] ends saving log!\n", ops->ops.module);
                BBOX_PRINT_INFO("[%s] starts uploading event [%s]\n", info->module, info->event);
#ifdef LOSCFG_FS_VFS
                (void)UploadEventByFile(KERNEL_FAULT_LOG_PATH); // VFS模式下通过文件上传事件
#else
                BBOX_PRINT_INFO("LOSCFG_FS_VFS isn't defined!\n"); // 非VFS模式下不支持文件上传
#endif
                BBOX_PRINT_INFO("[%s] ends uploading event [%s]\n", info->module, info->event);
            }
        } else {
            // 模块未实现必要接口时打印错误信息
            BBOX_PRINT_ERR("module [%s], GetLastLogInfo: %p, SaveLastLog: %p!\n",
                           ops->ops.module, ops->ops.GetLastLogInfo, ops->ops.SaveLastLog);
        }
    }
    (void)LOS_SemPost(g_opsListSem);                        // 释放操作链表信号量
    (void)LOS_MemFree(m_aucSysMem1, info);                  // 释放错误信息结构体内存
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
    if (CreateLogDir(LOSCFG_BLACKBOX_LOG_ROOT_PATH) != 0) {
        (void)LOS_SemPost(g_opsListSem);
        BBOX_PRINT_ERR("Create log dir [%s] failed!\n", LOSCFG_BLACKBOX_LOG_ROOT_PATH);
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
    if (memcpy_s(&newOps->ops, sizeof(newOps->ops), ops, sizeof(*ops)) != EOK) {
        BBOX_PRINT_ERR("newOps->ops is not enough or memcpy_s failed!\n");
        (void)LOS_MemFree(m_aucSysMem1, newOps);
        return -1;
    }
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
    taskParam.auwArgs[0] = (UINTPTR)LOSCFG_BLACKBOX_LOG_ROOT_PATH;
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