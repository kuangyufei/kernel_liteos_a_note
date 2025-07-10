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

#include <unistd.h>
#include <securec.h>
#include <sys/wait.h>
#include "perf.h"
#include "option.h"
#include "perf_stat.h"

/**
 * @brief 性能统计配置属性全局变量
 * @details 存储perf stat命令的所有配置参数，包括事件配置、进程/线程ID过滤等
 */
static PerfConfigAttr g_statAttr;

/**
 * @brief 解析事件参数并填充到配置结构体
 * @param[in] argv 命令行事件参数字符串
 * @return 成功返回0，失败返回非0值
 * @note 调用ParseEvents函数完成实际解析工作，结果存储在g_statAttr.eventsCfg中
 */
static inline int GetEvents(const char *argv)
{
    return ParseEvents(argv, &g_statAttr.eventsCfg, &g_statAttr.eventsCfg.eventsNr);
}

/**
 * @brief 解析线程ID参数并填充到配置结构体
 * @param[in] argv 命令行线程ID参数字符串
 * @return 成功返回0，失败返回非0值
 * @note 调用ParseIds函数完成实际解析工作，结果存储在g_statAttr.taskIds中
 */
static inline int GetTids(const char *argv)
{
    return ParseIds(argv, (int *)g_statAttr.taskIds, &g_statAttr.taskIdsNr);
}

/**
 * @brief 解析进程ID参数并填充到配置结构体
 * @param[in] argv 命令行进程ID参数字符串
 * @return 成功返回0，失败返回非0值
 * @note 调用ParseIds函数完成实际解析工作，结果存储在g_statAttr.processIds中
 */
static inline int GetPids(const char *argv)
{
    return ParseIds(argv, (int *)g_statAttr.processIds, &g_statAttr.processIdsNr);
}

/**
 * @brief perf stat命令选项配置数组
 * @details 定义了stat子命令支持的所有命令行选项及其处理方式
 */
static PerfOption g_statOpts[] = {
    OPTION_CALLBACK("-e", GetEvents),  // 指定性能事件，调用GetEvents处理
    OPTION_CALLBACK("-t", GetTids),  // 指定跟踪的线程ID，调用GetTids处理
    OPTION_CALLBACK("-P", GetPids),  // 指定跟踪的进程ID，调用GetPids处理
    OPTION_UINT("-p", &g_statAttr.eventsCfg.events[0].period),  // 设置采样周期值
    OPTION_UINT("-s", &g_statAttr.sampleType),  // 设置采样数据类型
    OPTION_UINT("-d", &g_statAttr.eventsCfg.predivided),  // 设置预分频值
};

/**
 * @brief 初始化性能统计配置属性
 * @return 成功返回0，失败返回-1
 * @details 根据不同的PMU配置（硬件/定时/软件）初始化默认事件配置，包括事件类型、事件ID和采样周期
 */
static int PerfStatAttrInit(void)
{
    PerfConfigAttr attr = {
        .eventsCfg = {
#ifdef LOSCFG_PERF_HW_PMU  // 硬件性能监控单元配置
            .type = PERF_EVENT_TYPE_HW,                     // 事件类型：硬件事件
            .events = {
                [0] = {PERF_COUNT_HW_CPU_CYCLES, 0xFFFF},  // 事件ID：CPU周期计数，采样周期：65535（0xFFFF的十进制值）
                [1] = {PERF_COUNT_HW_INSTRUCTIONS, 0xFFFFFF00},  // 事件ID：指令计数，采样周期：4294966016（0xFFFFFF00的十进制值）
                [2] = {PERF_COUNT_HW_ICACHE_REFERENCES, 0xFFFF},  // 事件ID：指令缓存引用，采样周期：65535
                [3] = {PERF_COUNT_HW_DCACHE_REFERENCES, 0xFFFF},  // 事件ID：数据缓存引用，采样周期：65535
            },
            .eventsNr = 4, /* 4 events */                     // 事件数量：4个
#elif defined LOSCFG_PERF_TIMED_PMU  // 定时性能监控配置
            .type = PERF_EVENT_TYPE_TIMED,                  // 事件类型：定时事件
            .events = {
                [0] = {PERF_COUNT_CPU_CLOCK, 100},          // 事件ID：CPU时钟，采样周期：100ms
            },
            .eventsNr = 1, /* 1 event */                     // 事件数量：1个
#elif defined LOSCFG_PERF_SW_PMU  // 软件性能监控配置
            .type = PERF_EVENT_TYPE_SW,                     // 事件类型：软件事件
            .events = {
                [0] = {PERF_COUNT_SW_TASK_SWITCH, 1},       // 事件ID：任务切换，采样周期：1（每次切换都统计）
                [1] = {PERF_COUNT_SW_IRQ_RESPONSE, 1},      // 事件ID：中断响应，采样周期：1（每次中断都统计）
                [2] = {PERF_COUNT_SW_MEM_ALLOC, 1},         // 事件ID：内存分配，采样周期：1（每次分配都统计）
                [3] = {PERF_COUNT_SW_MUX_PEND, 1},          // 事件ID：多路复用挂起，采样周期：1（每次挂起都统计）
            },
            .eventsNr = 4, /* 4 events */                     // 事件数量：4个
#endif
            .predivided = 0,                                // 预分频值：0（不使用预分频）
        },
        .taskIds = {0},                                    // 线程ID过滤列表，默认为空
        .taskIdsNr = 0,                                    // 线程ID数量：0
        .processIds = {0},                                 // 进程ID过滤列表，默认为空
        .processIdsNr = 0,                                 // 进程ID数量：0
        .needSample = 0,                                   // 需要采样：禁用（0表示禁用）
        .sampleType = 0,                                   // 采样数据类型：未设置
    };

    // 将初始化的配置复制到全局变量g_statAttr中
    return memcpy_s(&g_statAttr, sizeof(PerfConfigAttr), &attr, sizeof(PerfConfigAttr)) != EOK ? -1 : 0;
}

/**
 * @brief perf stat命令主函数
 * @param[in] fd 性能设备文件描述符
 * @param[in] argc 命令行参数数量
 * @param[in] argv 命令行参数数组
 * @details 实现性能数据统计的完整流程：初始化配置→解析参数→配置性能监控→启动统计→执行目标命令→停止统计
 */
void PerfStat(int fd, int argc, char **argv)
{
    int ret;               // 函数返回值
    int child;             // 子进程ID
    SubCmd cmd = {0};      // 子命令结构（存储待执行的目标命令）

    // 参数数量检查：perf stat命令至少需要3个参数（perf stat [options] command）
    if (argc < 3) { /* perf stat argc is at least 3 */
        return;
    }

    // 初始化性能统计配置属性
    ret = PerfStatAttrInit();
    if (ret != 0) {
        printf("perf stat attr init failed\n");
        return;
    }

    // 解析命令行选项和子命令（从第2个参数开始解析）
    ret = ParseOptions(argc - 2, &argv[2], g_statOpts, &cmd); /* parse option and cmd begin at index 2 */
    if (ret != 0) {
        printf("parse error\n");
        return;
    }

    // 根据配置初始化性能监控
    ret = PerfConfig(fd, &g_statAttr);
    if (ret != 0) {
        printf("perf config failed\n");
        return;
    }

    // 启动性能数据统计
    PerfStart(fd, 0);
    // 创建子进程执行目标命令
    child = fork();
    if (child < 0) {
        printf("fork error\n");
        goto EXIT;  // 创建进程失败，跳转到EXIT标签执行清理
    } else if (child == 0) {
        // 子进程：执行目标命令
        (void)execve(cmd.path, cmd.params, NULL);
        exit(0);  // 执行失败时退出子进程
    }

    // 等待子进程执行完成
    (void)waitpid(child, 0, 0);
EXIT:  // 统一清理出口
    PerfStop(fd);  // 停止性能数据统计
}

