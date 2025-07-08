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

#include "proc_fs.h"
#include "time.h"
#include "errno.h"
#include "los_cpup.h"
// 毫秒到纳秒的转换系数  (1毫秒 = 1000000纳秒)
#define MSEC_TO_NSEC        1000000
// 秒到毫秒的转换系数    (1秒 = 1000毫秒)
#define SEC_TO_MSEC         1000
// 小数转百分比的转换系数 (将小数乘以100得到百分比)
#define DECIMAL_TO_PERCENT  100

/**
 * @brief   填充/proc/uptime文件内容
 * @param   seqBuf [out] 用于存储输出内容的缓冲区结构体指针
 * @param   v      [in]  未使用的参数
 * @return  int    成功返回0，失败返回错误码
 * @details 该函数会获取系统运行时间，并在启用CPU统计功能时同时计算并输出CPU空闲时间
 */
static int UptimeProcFill(struct SeqBuf *seqBuf, void *v)
{
    struct timespec curtime = {0, 0};  // 用于存储当前单调时间的结构体
    int ret;                          // 系统调用返回值
#ifdef LOSCFG_KERNEL_CPUP
    float idleRate;                   // CPU空闲率
    float idleMSec;                   // CPU空闲时间(毫秒)
    float usage;                      // CPU使用率
#endif
    // 获取单调时钟时间，不受系统时间调整影响，适合测量系统运行时间
    ret = clock_gettime(CLOCK_MONOTONIC, &curtime);
    if (ret < 0) {                     // 检查系统调用是否失败
        PRINT_ERR("clock_gettime error!\n");  // 打印错误信息
        return -get_errno();           // 返回错误码
    }

#ifdef LOSCFG_KERNEL_CPUP  // 条件编译：如果启用了CPU性能统计功能
    // 获取系统CPU历史使用率(所有时间)
    usage = (float)LOS_HistorySysCpuUsage(CPUP_ALL_TIME);
    // 计算CPU空闲率：核心数 - (使用率 / 精度系数 / 百分比系数)
    idleRate = LOSCFG_KERNEL_CORE_NUM - usage / LOS_CPUP_PRECISION_MULT / DECIMAL_TO_PERCENT;
    // 计算CPU空闲时间(毫秒)：总运行时间 * 空闲率
    idleMSec = ((float)curtime.tv_sec * SEC_TO_MSEC + curtime.tv_nsec / MSEC_TO_NSEC) * idleRate;

    // 格式化输出：系统运行时间(秒.毫秒) 和 CPU空闲时间(秒.毫秒)
    (void)LosBufPrintf(seqBuf, "%llu.%03llu %llu.%03llu\n", (unsigned long long)curtime.tv_sec,
                       (unsigned long long)(curtime.tv_nsec / MSEC_TO_NSEC),
                       (unsigned long long)(idleMSec / SEC_TO_MSEC),
                       (unsigned long long)((unsigned long long)idleMSec % SEC_TO_MSEC));
#else  // 如果未启用CPU性能统计功能
    // 仅输出系统运行时间(秒.毫秒)
    (void)LosBufPrintf(seqBuf, "%llu.%03llu\n", (unsigned long long)curtime.tv_sec,
                       (unsigned long long)(curtime.tv_nsec / MSEC_TO_NSEC));

#endif
    return 0;  // 成功返回0
}

// uptime文件的proc操作结构体，仅实现读操作
static const struct ProcFileOperations UPTIME_PROC_FOPS = {
    .read       = UptimeProcFill,  // 关联读操作函数
};

/**
 * @brief   初始化/proc/uptime文件节点
 * @return  void
 * @details 创建uptime文件节点并关联文件操作结构体
 */
void ProcUptimeInit(void)
{
    // 创建/proc/uptime文件节点，权限0，父目录为NULL(即/proc目录)
    struct ProcDirEntry *pde = CreateProcEntry("uptime", 0, NULL);
    if (pde == NULL) {                // 检查节点创建是否失败
        PRINT_ERR("create /proc/uptime error!\n");  // 打印错误信息
        return;                       // 创建失败直接返回
    }

    pde->procFileOps = &UPTIME_PROC_FOPS;  // 关联uptime文件操作结构体
}
