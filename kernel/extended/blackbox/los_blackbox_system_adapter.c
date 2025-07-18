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
#include "los_vm_common.h"
#include "los_vm_phys.h"
#include "los_vm_zone.h"
#include "securec.h"

/* ------------ 本地宏定义 ------------ */
#define LOG_FLAG "GOODLOG"  /* 日志标识，用于验证日志有效性 */

/* ------------ 本地数据结构 ------------ */
/**
 * @brief 故障日志信息结构体
 * @details 包含日志标识、长度和错误信息，用于存储和验证故障日志
 */
struct FaultLogInfo {
    char flag[8];          /* 日志标识，固定为LOG_FLAG（长度8字节） */
    int len;               /* 模块异常信息保存的日志长度 */
    struct ErrorInfo info; /* 错误信息结构体，包含事件、模块和描述 */
};

/* ------------ 本地变量 ------------ */
static char *g_logBuffer = NULL;  /* 全局日志缓冲区指针，用于存储故障日志数据 */

/* ------------ 函数定义 ------------ */
/**
 * @brief 保存故障日志到文件
 * @param[in] filePath 日志文件路径
 * @param[in] dataBuf 日志数据缓冲区
 * @param[in] bufSize 数据缓冲区大小
 * @param[in] info 错误信息结构体指针
 * @return 无
 * @note 先保存基本错误信息，再写入详细日志数据
 */
static void SaveFaultLog(const char *filePath, const char *dataBuf, size_t bufSize, struct ErrorInfo *info)
{
    (void)SaveBasicErrorInfo(filePath, info);  /* 保存基本错误信息（事件、模块、描述） */
    (void)FullWriteFile(filePath, dataBuf, bufSize, 1);  /* 写入详细日志数据，1表示追加模式 */
}

#ifdef LOSCFG_SAVE_EXCINFO
/**
 * @brief 异常信息文件写入函数（仅LOSCFG_SAVE_EXCINFO开启时有效）
 * @param[in] startAddr 起始地址（未使用）
 * @param[in] space 空间大小（未使用）
 * @param[in] rwFlag 读写标志（未使用）
 * @param[in] buf 缓冲区指针（未使用）
 * @return 无
 * @note 当前为空实现，预留异常信息写入接口
 */
static void WriteExcFile(UINT32 startAddr, UINT32 space, UINT32 rwFlag, char *buf)
{
    (void)startAddr;  /* 未使用参数，避免编译警告 */
    (void)space;      /* 未使用参数，避免编译警告 */
    (void)rwFlag;     /* 未使用参数，避免编译警告 */
    (void)buf;        /* 未使用参数，避免编译警告 */
}
#endif

/**
 * @brief 注册异常信息钩子函数
 * @return 无
 * @note 将异常信息写入日志缓冲区，仅当缓冲区分配成功时执行
 */
static void RegisterExcInfoHook(void)
{
    if (g_logBuffer != NULL) {  /* 检查日志缓冲区是否分配成功 */
#ifdef LOSCFG_SAVE_EXCINFO
        /* 注册异常信息钩子：偏移0，长度为日志缓冲区总大小减去FaultLogInfo结构大小 */
        LOS_ExcInfoRegHook(0, LOSCFG_BLACKBOX_LOG_SIZE - sizeof(struct FaultLogInfo),
            g_logBuffer + sizeof(struct FaultLogInfo), WriteExcFile);
#endif
    } else {
        BBOX_PRINT_ERR("Alloc mem failed!\n");  /* 缓冲区分配失败，打印错误日志 */
    }
}

/**
 * @brief 分配日志缓冲区
 * @return 成功返回0，失败返回-1
 * @note 从预留物理内存区域分配缓冲区，需确保大小不小于FaultLogInfo结构
 */
static int AllocLogBuffer(void)
{
    /* 检查配置的日志大小是否足够容纳基础日志结构 */
    if (LOSCFG_BLACKBOX_LOG_SIZE < sizeof(struct FaultLogInfo)) {
        BBOX_PRINT_ERR("LOSCFG_BLACKBOX_LOG_SIZE [%d] is too short, it must be >= %u\n",
            LOSCFG_BLACKBOX_LOG_SIZE, sizeof(struct FaultLogInfo));
        return -1;
    }

    /*
     * LOSCFG_BLACKBOX_RESERVE_MEM_ADDR指向的物理内存为黑盒专用
     * 系统运行期间不可被其他模块占用，启动阶段不可与其他系统内存区域重叠
     */
    g_logBuffer = (char *)MEM_CACHED_ADDR(LOSCFG_BLACKBOX_RESERVE_MEM_ADDR);  /* 将物理地址转换为缓存地址 */
    BBOX_PRINT_INFO("g_logBuffer: %p, len: 0x%x for blackbox!\n", g_logBuffer, (UINT32)LOSCFG_BLACKBOX_LOG_SIZE);

    return (g_logBuffer != NULL) ? 0 : -1;  /* 返回分配结果 */
}

/**
 * @brief 日志转储函数（ModuleOps接口实现）
 * @param[in] logDir 日志目录（未使用）
 * @param[in] info 错误信息结构体指针
 * @return 无
 * @note 根据事件类型（如PANIC）将日志数据写入缓冲区或文件
 */
static void Dump(const char *logDir, struct ErrorInfo *info)
{
    struct FaultLogInfo *pLogInfo = NULL;

    if (logDir == NULL || info == NULL) {  /* 参数有效性检查 */
        BBOX_PRINT_ERR("logDir: %p, info: %p!\n", logDir, info);
        return;
    }
    if (g_logBuffer == NULL) {  /* 检查日志缓冲区是否初始化 */
        BBOX_PRINT_ERR("g_logBuffer is NULL, alloc physical pages failed!\n");
        return;
    }

    if (strcmp(info->event, EVENT_PANIC) == 0) {  /* 处理系统崩溃事件 */
        pLogInfo = (struct FaultLogInfo *)g_logBuffer;  /* 日志缓冲区首地址即为FaultLogInfo结构 */
        (void)memset_s(pLogInfo, sizeof(*pLogInfo), 0, sizeof(*pLogInfo));  /* 初始化日志信息结构 */
#ifdef LOSCFG_SAVE_EXCINFO
        pLogInfo->len = GetExcInfoIndex();  /* 获取异常信息长度 */
#else
        pLogInfo->len = 0;  /* 未开启异常信息保存，长度为0 */
#endif
        (void)memcpy_s(&pLogInfo->flag, sizeof(pLogInfo->flag), LOG_FLAG, strlen(LOG_FLAG));  /* 设置日志标识 */
        (void)memcpy_s(&pLogInfo->info, sizeof(pLogInfo->info), info, sizeof(*info));  /* 复制错误信息 */
        /* 刷新数据缓存，确保数据写入物理内存 */
        DCacheFlushRange((UINTPTR)g_logBuffer, (UINTPTR)(g_logBuffer + LOSCFG_BLACKBOX_LOG_SIZE));
    } else {  /* 处理非崩溃事件（如用户态错误） */
#ifdef LOSCFG_SAVE_EXCINFO
        /* 保存用户故障日志：从缓冲区偏移FaultLogInfo大小处开始写入，长度取异常信息长度与剩余空间的最小值 */
        SaveFaultLog(USER_FAULT_LOG_PATH, g_logBuffer + sizeof(struct FaultLogInfo),
            Min(LOSCFG_BLACKBOX_LOG_SIZE - sizeof(struct FaultLogInfo), GetExcInfoIndex()), info);
#else
        SaveFaultLog(USER_FAULT_LOG_PATH, g_logBuffer + sizeof(struct FaultLogInfo), 0, info);  /* 无异常信息，写入0长度数据 */
#endif
    }
}

/**
 * @brief 重置函数（ModuleOps接口实现）
 * @param[in] info 错误信息结构体指针
 * @return 无
 * @note 非崩溃事件触发日志上传，崩溃事件不处理（由专门流程处理）
 */
static void Reset(struct ErrorInfo *info)
{
    if (info == NULL) {  /* 参数有效性检查 */
        BBOX_PRINT_ERR("info is NULL!\n");
        return;
    }

    if (strcmp(info->event, EVENT_PANIC) != 0) {  /* 非崩溃事件需要上传日志 */
        BBOX_PRINT_INFO("[%s] starts uploading event [%s]\n", info->module, info->event);
        (void)UploadEventByFile(USER_FAULT_LOG_PATH);  /* 通过文件上传事件日志 */
        BBOX_PRINT_INFO("[%s] ends uploading event [%s]\n", info->module, info->event);
    }
}

/**
 * @brief 获取最后日志信息（ModuleOps接口实现）
 * @param[out] info 输出参数，用于存储获取到的错误信息
 * @return 成功返回0，失败返回-1
 * @note 从日志缓冲区读取并验证日志标识，有效则返回错误信息
 */
static int GetLastLogInfo(struct ErrorInfo *info)
{
    struct FaultLogInfo *pLogInfo = NULL;

    if (info == NULL) {  /* 参数有效性检查 */
        BBOX_PRINT_ERR("info is NULL!\n");
        return -1;
    }
    if (g_logBuffer == NULL) {  /* 检查日志缓冲区是否初始化 */
        BBOX_PRINT_ERR("g_logBuffer is NULL, alloc physical pages failed!\n");
        return -1;
    }

    pLogInfo = (struct FaultLogInfo *)g_logBuffer;  /* 指向日志缓冲区首地址 */
    /* 验证日志标识是否有效 */
    if (memcmp(pLogInfo->flag, LOG_FLAG, strlen(LOG_FLAG)) == 0) {
        (void)memcpy_s(info, sizeof(*info), &pLogInfo->info, sizeof(pLogInfo->info));  /* 复制错误信息到输出参数 */
        return 0;
    }

    return -1;  /* 日志标识无效，返回失败 */
}

/**
 * @brief 保存最后日志（ModuleOps接口实现）
 * @param[in] logDir 日志目录（未使用）
 * @param[in] info 错误信息结构体指针
 * @return 成功返回0，失败返回-1
 * @note 仅当VFS文件系统开启时有效，将内核故障日志写入文件并上传
 */
static int SaveLastLog(const char *logDir, struct ErrorInfo *info)
{
#ifdef LOSCFG_FS_VFS  /* 仅当VFS文件系统配置开启时编译 */
    struct FaultLogInfo *pLogInfo = NULL;

    if (logDir == NULL || info == NULL) {  /* 参数有效性检查 */
        BBOX_PRINT_ERR("logDir: %p, info: %p!\n", logDir, info);
        return -1;
    }
    if (g_logBuffer == NULL) {  /* 检查日志缓冲区是否初始化 */
        BBOX_PRINT_ERR("g_logBuffer is NULL, alloc physical pages failed!\n");
        return -1;
    }

    pLogInfo = (struct FaultLogInfo *)g_logBuffer;  /* 指向日志缓冲区首地址 */
    /* 验证日志标识有效，则保存内核故障日志 */
    if (memcmp(pLogInfo->flag, LOG_FLAG, strlen(LOG_FLAG)) == 0) {
        SaveFaultLog(KERNEL_FAULT_LOG_PATH, g_logBuffer + sizeof(*pLogInfo),
            Min(LOSCFG_BLACKBOX_LOG_SIZE - sizeof(*pLogInfo), pLogInfo->len), info);
    }
    (void)memset_s(g_logBuffer, LOSCFG_BLACKBOX_LOG_SIZE, 0, LOSCFG_BLACKBOX_LOG_SIZE);  /* 清空日志缓冲区 */
    BBOX_PRINT_INFO("[%s] starts uploading event [%s]\n", info->module, info->event);
    (void)UploadEventByFile(KERNEL_FAULT_LOG_PATH);  /* 上传内核故障日志文件 */
    BBOX_PRINT_INFO("[%s] ends uploading event [%s]\n", info->module, info->event);
    return 0;
#else
    (VOID)logDir;  /* 未使用参数，避免编译警告 */
    (VOID)info;    /* 未使用参数，避免编译警告 */
    BBOX_PRINT_ERR("LOSCFG_FS_VFS isn't defined!\n");  /* 提示VFS未开启 */
    return -1;
#endif
}

#ifdef LOSCFG_BLACKBOX_TEST  /* 黑盒测试功能，仅测试配置开启时编译 */
/**
 * @brief 黑盒测试函数
 * @return 无
 * @note 注册测试模块并发送测试事件，用于验证黑盒功能
 */
static void BBoxTest(void)
{
    struct ModuleOps ops = {  /* 测试模块操作结构体 */
        .module = "MODULE_TEST",  /* 测试模块名称 */
        .Dump = NULL,             /* 未实现Dump接口 */
        .Reset = NULL,            /* 未实现Reset接口 */
        .GetLastLogInfo = NULL,   /* 未实现GetLastLogInfo接口 */
        .SaveLastLog = NULL,      /* 未实现SaveLastLog接口 */
    };

    if (BBoxRegisterModuleOps(&ops) != 0) {  /* 注册测试模块 */
        BBOX_PRINT_ERR("BBoxRegisterModuleOps failed!\n");
        return;
    }
    /* 发送测试事件：事件类型EVENT_TEST1，模块MODULE_TEST，描述信息Test BBoxNotifyError111，无需系统重置 */
    BBoxNotifyError("EVENT_TEST1", "MODULE_TEST", "Test BBoxNotifyError111", 0);
}
#endif

/**
 * @brief 黑盒系统适配器初始化
 * @return 成功返回LOS_OK，失败返回LOS_NOK
 * @note 初始化日志缓冲区，注册异常钩子，注册系统模块操作接口
 */
int OsBBoxSystemAdapterInit(void)
{
    struct ModuleOps ops = {  /* 系统模块操作接口结构体 */
        .module = MODULE_SYSTEM,  /* 模块名称：系统模块 */
        .Dump = Dump,             /* 日志转储接口 */
        .Reset = Reset,           /* 重置接口 */
        .GetLastLogInfo = GetLastLogInfo,  /* 获取最后日志接口 */
        .SaveLastLog = SaveLastLog,        /* 保存最后日志接口 */
    };

    /* 分配日志缓冲区 */
    if (AllocLogBuffer() == 0) {
        RegisterExcInfoHook();  /* 注册异常信息钩子 */
        if (BBoxRegisterModuleOps(&ops) != 0) {  /* 注册系统模块操作接口 */
            BBOX_PRINT_ERR("BBoxRegisterModuleOps failed!\n");
            g_logBuffer = NULL;  /* 注册失败，释放缓冲区 */
            return LOS_NOK;
        }
    } else {
        BBOX_PRINT_ERR("AllocLogBuffer failed!\n");  /* 缓冲区分配失败 */
    }

#ifdef LOSCFG_BLACKBOX_TEST
    BBoxTest();  /* 执行黑盒测试（若测试配置开启） */
#endif

    return LOS_OK;
}
/* 模块初始化：将OsBBoxSystemAdapterInit注册为平台级初始化函数 */
LOS_MODULE_INIT(OsBBoxSystemAdapterInit, LOS_INIT_LEVEL_PLATFORM);

