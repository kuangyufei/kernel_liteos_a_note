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

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include "perf.h"

/**
 * @file perf.c
 * @brief Performance monitoring tool implementation
 */

/**
 * @def PERF_IOC_MAGIC
 * @brief IO控制命令的幻数(magic number)，用于标识PERF相关的IOCTL命令
 * @note 采用字符'T'的ASCII值(0x54)作为幻数，确保与其他设备的IOCTL命令区分
 */
#define PERF_IOC_MAGIC     'T'

/**
 * @def PERF_START
 * @brief 启动性能监控的IOCTL命令
 * @param PERF_IOC_MAGIC - 幻数，固定为'T'
 * @param 1 - 命令序号，用于区分不同的PERF命令
 * @note 命令格式为_IO(type, nr)，其中type为幻数，nr为命令编号
 */
#define PERF_START         _IO(PERF_IOC_MAGIC, 1)

/**
 * @def PERF_STOP
 * @brief 停止性能监控的IOCTL命令
 * @param PERF_IOC_MAGIC - 幻数，固定为'T'
 * @param 2 - 命令序号，用于区分不同的PERF命令
 */
#define PERF_STOP          _IO(PERF_IOC_MAGIC, 2)

/**
 * @brief 打印perf命令的使用帮助信息
 * @details 输出perf工具支持的所有命令格式及选项说明，指导用户正确使用perf功能
 * @note 当用户输入错误命令或使用-h/--help选项时调用此函数
 */
void PerfUsage(void)
{
    printf("\nUsage: ./perf start [id]. Start perf.\n");          // 启动性能监控，[id]为可选的监控区域ID
    printf("\nUsage: ./perf stop. Stop perf.\n");                  // 停止当前性能监控
    printf("\nUsage: ./perf read <nBytes>. Read nBytes raw data from perf buffer and print out.\n");  // 从性能缓冲区读取指定字节数的原始数据并打印
    printf("\nUsage: ./perf list. List events to be used in -e.\n");  // 列出所有可用于-e选项的性能事件
    printf("\nUsage: ./perf stat/record [option] <command>. \n"  // stat:统计性能指标，record:记录性能数据
                    "-e, event selector. use './perf list' to list available events.\n"  // 指定要监控的性能事件，使用list命令查看可用事件
                    "-p, event period.\n"  // 设置事件采样周期
                    "-o, perf data output filename.\n"  // 指定性能数据输出文件名
                    "-t, taskId filter(allowlist), if not set perf will sample all tasks.\n"  // 任务ID过滤(白名单)，不设置则监控所有任务
                    "-s, type of data to sample defined in PerfSampleType los_perf.h.\n"  // 采样数据类型，定义于los_perf.h中的PerfSampleType
                    "-P, processId filter(allowlist), if not set perf will sample all processes.\n"  // 进程ID过滤(白名单)，不设置则监控所有进程
                    "-d, whether to prescaler (once every 64 counts),"
                    "which only take effect on cpu cycle hardware event.\n"  // 是否启用预分频器(每64次计数采样一次)，仅对CPU周期硬件事件有效
    );
}

/**
 * @brief 设置性能事件的采样周期
 * @param[in,out] attr - 性能配置属性结构体指针，包含事件配置信息
 * @details 将第一个事件的采样周期应用到所有后续事件，确保多事件采样周期一致
 * @note 假设events[0]是主事件，其周期将作为所有事件的统一周期
 */
static void PerfSetPeriod(PerfConfigAttr *attr)
{
    int i;
    // 从第二个事件开始(索引1)，将所有事件的周期设置为第一个事件的周期
    for (i = 1; i < attr->eventsCfg.eventsNr; i++) {
        attr->eventsCfg.events[i].period = attr->eventsCfg.events[0].period;
    }
}

/**
 * @brief 以十六进制格式打印缓冲区数据
 * @param[in] buf - 待打印的缓冲区指针
 * @param[in] num - 要打印的字节数
 * @details 按每行4字节的格式打印缓冲区内容，便于查看原始性能数据
 * @note 用于perf read命令输出原始二进制数据
 */
void PerfPrintBuffer(const char *buf, ssize_t num)
{
#define BYTES_PER_LINE  4  // 每行打印的字节数
    ssize_t i;
    for (i = 0; i < num; i++) {
        printf(" %02x", (unsigned char)buf[i]);  // 以两位十六进制格式打印每个字节
        if (((i + 1) % BYTES_PER_LINE) == 0) {    // 每打印BYTES_PER_LINE个字节换行
            printf("\n");
        }
    }
    printf("\n");
}

/**
 * @brief 打印性能配置属性的调试信息
 * @param[in] attr - 性能配置属性结构体指针
 * @details 输出配置的事件类型、采样周期、过滤条件等详细信息，用于调试目的
 * @note 仅在调试模式下通过printf_debug输出信息
 */
void PerfDumpAttr(PerfConfigAttr *attr)
{
    int i;
    printf_debug("attr->type: %d\n", attr->eventsCfg.type);  // 打印事件配置类型
    // 打印所有事件的ID和周期
    for (i = 0; i < attr->eventsCfg.eventsNr; i++) {
        printf_debug("attr->events[%d]: %d, 0x%x\n", i, attr->eventsCfg.events[i].eventId,
            attr->eventsCfg.events[i].period);  // eventId:事件ID，period:采样周期(十六进制)
    }
    printf_debug("attr->predivided: %d\n", attr->eventsCfg.predivided);  // 打印预分频器使能状态(0:禁用,1:启用)
    printf_debug("attr->sampleType: 0x%x\n", attr->sampleType);  // 打印采样数据类型(十六进制掩码)

    // 打印任务ID过滤列表
    for (i = 0; i < attr->taskIdsNr; i++) {
        printf_debug("attr->taskIds[%d]: %d\n", i, attr->taskIds[i]);
    }

    // 打印进程ID过滤列表
    for (i = 0; i < attr->processIdsNr; i++) {
        printf_debug("attr->processIds[%d]: %d\n", i, attr->processIds[i]);
    }

    printf_debug("attr->needSample: %d\n", attr->needSample);  // 打印是否需要采样标志(0:不需要,1:需要)
}

/**
 * @brief 启动性能监控
 * @param[in] fd - perf设备文件描述符
 * @param[in] sectionId - 要监控的代码段ID
 * @details 通过IOCTL命令PERF_START通知内核启动指定代码段的性能数据采集
 * @note 忽略IOCTL调用的返回值
 */
void PerfStart(int fd, size_t sectionId)
{
    (void)ioctl(fd, PERF_START, sectionId);  // 调用IOCTL启动性能监控，sectionId指定监控区域
}

/**
 * @brief 停止性能监控
 * @param[in] fd - perf设备文件描述符
 * @details 通过IOCTL命令PERF_STOP通知内核停止当前性能数据采集
 * @note 忽略IOCTL调用的返回值
 */
void PerfStop(int fd)
{
    (void)ioctl(fd, PERF_STOP, NULL);  // 调用IOCTL停止性能监控，无额外参数
}

/**
 * @brief 配置性能监控参数
 * @param[in] fd - perf设备文件描述符
 * @param[in] attr - 性能配置属性结构体指针
 * @return 成功时返回写入的字节数，失败时返回-1
 * @details 设置事件周期、打印调试信息并将配置写入内核
 * @note 若attr为NULL直接返回-1
 */
int PerfConfig(int fd, PerfConfigAttr *attr)
{
    if (attr == NULL) {  // 参数合法性检查
        return -1;       // 参数无效返回错误
    }
    PerfSetPeriod(attr);   // 统一设置所有事件的周期
    PerfDumpAttr(attr);    // 打印配置调试信息
    return write(fd, attr, sizeof(PerfConfigAttr));  // 将配置写入设备
}

/**
 * @brief 读取性能监控数据
 * @param[in] fd - perf设备文件描述符
 * @param[out] buf - 接收数据的缓冲区指针
 * @param[in] size - 要读取的字节数
 * @return 成功时返回实际读取的字节数，失败或buf为NULL时返回0
 * @details 从perf设备读取指定大小的性能数据到缓冲区
 * @note 若buf为NULL打印错误信息并返回0
 */
ssize_t PerfRead(int fd, char *buf, size_t size)
{
    ssize_t len;
    if (buf == NULL) {              // 缓冲区合法性检查
        printf("Read buffer is null.\n");  // 打印错误信息
        return 0;                   // 缓冲区无效返回0
    }

    len = read(fd, buf, size);  // 从设备读取数据
    return len;                 // 返回实际读取的字节数
}