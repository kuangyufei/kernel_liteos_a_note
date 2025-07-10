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
#include <sys/wait.h>
#include <securec.h>

#ifdef LOSCFG_FS_VFS
#include <fcntl.h>
#include <errno.h>
#endif

#include "perf.h"
#include "option.h"
#include "perf_record.h"

/**
 * @brief 文件访问权限宏定义
 * @note 八进制0644表示：文件所有者具有读写权限，组用户和其他用户具有只读权限
 */
#define PERF_FILE_MODE 0644

/**
 * @brief 性能记录配置属性全局变量
 * @details 存储perf record命令的所有配置参数，包括事件配置、进程/线程ID过滤等
 */
static PerfConfigAttr g_recordAttr;

/**
 * @brief 性能数据保存路径
 * @note 默认路径为"/storage/data/perf.data"
 */
static const char *g_savePath = "/storage/data/perf.data";

/**
 * @brief 解析事件参数并填充到配置结构体
 * @param[in] argv 命令行事件参数字符串
 * @return 成功返回0，失败返回非0值
 * @note 调用ParseEvents函数完成实际解析工作，结果存储在g_recordAttr.eventsCfg中
 */
static inline int GetEvents(const char *argv)
{
    return ParseEvents(argv, &g_recordAttr.eventsCfg, &g_recordAttr.eventsCfg.eventsNr);
}

/**
 * @brief 解析线程ID参数并填充到配置结构体
 * @param[in] argv 命令行线程ID参数字符串
 * @return 成功返回0，失败返回非0值
 * @note 调用ParseIds函数完成实际解析工作，结果存储在g_recordAttr.taskIds中
 */
static inline int GetTids(const char *argv)
{
    return ParseIds(argv, (int *)g_recordAttr.taskIds, &g_recordAttr.taskIdsNr);
}

/**
 * @brief 解析进程ID参数并填充到配置结构体
 * @param[in] argv 命令行进程ID参数字符串
 * @return 成功返回0，失败返回非0值
 * @note 调用ParseIds函数完成实际解析工作，结果存储在g_recordAttr.processIds中
 */
static inline int GetPids(const char *argv)
{
    return ParseIds(argv, (int *)g_recordAttr.processIds, &g_recordAttr.processIdsNr);
}

/**
 * @brief perf record命令选项配置数组
 * @details 定义了record子命令支持的所有命令行选项及其处理方式
 */
static PerfOption g_recordOpts[] = {
    OPTION_CALLBACK("-e", GetEvents),  // 指定性能事件，调用GetEvents处理
    OPTION_CALLBACK("-t", GetTids),  // 指定跟踪的线程ID，调用GetTids处理
    OPTION_CALLBACK("-P", GetPids),  // 指定跟踪的进程ID，调用GetPids处理
    OPTION_STRING("-o", &g_savePath),  // 指定输出文件路径，覆盖默认g_savePath
    OPTION_UINT("-p", &g_recordAttr.eventsCfg.events[0].period),  // 设置采样周期值
    OPTION_UINT("-s", &g_recordAttr.sampleType),  // 设置采样数据类型
    OPTION_UINT("-d", &g_recordAttr.eventsCfg.predivided),  // 设置预分频值
};

/**
 * @brief 初始化性能记录配置属性
 * @return 成功返回0，失败返回-1
 * @details 根据不同的PMU配置（硬件/定时/软件）初始化默认事件配置，包括事件类型、事件ID和采样周期
 */
static int PerfRecordAttrInit(void)
{
    PerfConfigAttr attr = {
        .eventsCfg = {
#ifdef LOSCFG_PERF_HW_PMU  // 硬件性能监控单元配置
            .type = PERF_EVENT_TYPE_HW,                     // 事件类型：硬件事件
            .events = {
                [0] = {PERF_COUNT_HW_CPU_CYCLES, 0xFFFF},  // 事件ID：CPU周期计数，采样周期：65535（0xFFFF的十进制值）
            },
#elif defined LOSCFG_PERF_TIMED_PMU  // 定时性能监控配置
            .type = PERF_EVENT_TYPE_TIMED,                  // 事件类型：定时事件
            .events = {
                [0] = {PERF_COUNT_CPU_CLOCK, 100},          // 事件ID：CPU时钟，采样周期：100ms
            },
#elif defined LOSCFG_PERF_SW_PMU  // 软件性能监控配置
            .type = PERF_EVENT_TYPE_SW,                     // 事件类型：软件事件
            .events = {
                [0] = {PERF_COUNT_SW_TASK_SWITCH, 1},       // 事件ID：任务切换，采样周期：1（每次切换都采样）
            },
#endif
            .eventsNr = 1, /* 1 event */                     // 默认事件数量：1个
            .predivided = 0,                                // 预分频值：0（不使用预分频）
        },
        .taskIds = {0},                                    // 线程ID过滤列表，默认为空
        .taskIdsNr = 0,                                    // 线程ID数量：0
        .processIds = {0},                                 // 进程ID过滤列表，默认为空
        .processIdsNr = 0,                                 // 进程ID数量：0
        .needSample = 1,                                   // 需要采样：启用（1表示启用）
        .sampleType = PERF_RECORD_IP | PERF_RECORD_CALLCHAIN,  // 采样数据类型：指令指针 + 调用链
    };

    // 将初始化的配置复制到全局变量g_recordAttr中
    return memcpy_s(&g_recordAttr, sizeof(PerfConfigAttr), &attr, sizeof(PerfConfigAttr)) != EOK ? -1 : 0;
}

/**
 * @brief 将性能数据写入文件
 * @param[in] filePath 目标文件路径
 * @param[in] buf 数据缓冲区指针
 * @param[in] bufSize 数据大小（字节数）
 * @return 成功返回0，失败返回-1
 * @details 支持两种模式：当LOSCFG_FS_VFS启用时写入文件系统，否则直接打印数据
 */
ssize_t PerfWriteFile(const char *filePath, const char *buf, ssize_t bufSize)
{
#ifdef LOSCFG_FS_VFS  // 启用VFS文件系统
    int fd = -1;                  // 文件描述符
    ssize_t totalToWrite = bufSize;  // 待写入总字节数
    ssize_t totalWrite = 0;       // 已写入总字节数

    // 参数合法性检查
    if (filePath == NULL || buf == NULL || bufSize == 0) {
        return -1;
    }

    // 打开文件（创建+读写+截断模式），权限为PERF_FILE_MODE(0644)
    fd = open(filePath, O_CREAT | O_RDWR | O_TRUNC, PERF_FILE_MODE);
    if (fd < 0) {
        printf("create file [%s] failed, fd: %d, %s!\n", filePath, fd, strerror(errno));
        return -1;
    }
    // 循环写入数据，处理部分写入情况
    while (totalToWrite > 0) {
        ssize_t writeThisTime = write(fd, buf, totalToWrite);
        if (writeThisTime < 0) {
            printf("failed to write file [%s], %s!\n", filePath, strerror(errno));
            (void)close(fd);
            return -1;
        }
        buf += writeThisTime;         // 移动缓冲区指针
        totalToWrite -= writeThisTime; // 更新待写入字节数
        totalWrite += writeThisTime;   // 更新已写入字节数
    }
    (void)fsync(fd);  // 强制刷新文件缓存到磁盘
    (void)close(fd);  // 关闭文件描述符

    // 检查是否所有数据都成功写入
    return (totalWrite == bufSize) ? 0 : -1;
#else  // 未启用VFS文件系统
    (void)filePath;  // 未使用参数，避免编译警告
    PerfPrintBuffer(buf, bufSize);  // 直接打印数据缓冲区内容
    return 0;
#endif
}

/**
 * @brief perf record命令主函数
 * @param[in] fd 性能设备文件描述符
 * @param[in] argc 命令行参数数量
 * @param[in] argv 命令行参数数组
 * @details 实现性能数据采集的完整流程：初始化配置→解析参数→配置性能监控→启动采集→执行目标命令→停止采集→保存数据
 */
void PerfRecord(int fd, int argc, char **argv)
{
    int ret;               // 函数返回值
    int child;             // 子进程ID
    char *buf;             // 数据缓冲区指针
    ssize_t len;           // 读取数据长度
    SubCmd cmd = {0};      // 子命令结构（存储待执行的目标命令）

    // 参数数量检查：perf record命令至少需要3个参数（perf record [options] command）
    if (argc < 3) { /* perf record argc is at least 3 */
        return;
    }

    // 初始化性能记录配置属性
    ret = PerfRecordAttrInit();
    if (ret != 0) {
        printf("perf record attr init failed\n");
        return;
    }

    // 解析命令行选项和子命令（从第2个参数开始解析）
    ret = ParseOptions(argc - 2, &argv[2], g_recordOpts, &cmd); /* parse option and cmd begin at index 2 */
    if (ret != 0) {
        printf("parse error\n");
        return;
    }

    // 根据配置初始化性能监控
    ret = PerfConfig(fd, &g_recordAttr);
    if (ret != 0) {
        printf("perf config failed\n");
        return;
    }

    // 启动性能数据采集
    PerfStart(fd, 0);
    // 创建子进程执行目标命令
    child = fork();
    if (child < 0) {
        printf("fork error\n");
        PerfStop(fd);  // 创建进程失败时停止采集
        return;
    } else if (child == 0) {
        // 子进程：执行目标命令
        (void)execve(cmd.path, cmd.params, NULL);
        exit(0);  // 执行失败时退出子进程
    }

    // 等待子进程执行完成
    waitpid(child, 0, 0);
    // 停止性能数据采集
    PerfStop(fd);

    // 分配内存缓冲区存储性能数据（大小由LOSCFG_PERF_BUFFER_SIZE定义）
    buf = (char *)malloc(LOSCFG_PERF_BUFFER_SIZE);
    if (buf == NULL) {
        printf("no memory for read perf 0x%x\n", LOSCFG_PERF_BUFFER_SIZE);  // 0x%x表示以十六进制打印缓冲区大小
        return;
    }
    // 从性能设备读取采集到的数据
    len = PerfRead(fd, buf, LOSCFG_PERF_BUFFER_SIZE);
    // 将数据写入文件
    ret = PerfWriteFile(g_savePath, buf, len);
    if (ret == 0) {
        printf("save perf data success at %s\n", g_savePath);
    } else {
        printf("save perf data failed at %s\n", g_savePath);
    }
    free(buf);  // 释放内存缓冲区
}