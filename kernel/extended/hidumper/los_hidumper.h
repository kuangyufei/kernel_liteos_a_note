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

#ifndef LOS_HIDUMPER_H
#define LOS_HIDUMPER_H

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

#ifndef __user
#define __user
#endif
#define PATH_MAX_LEN     256  // 文件路径最大长度

/**
 * @brief 内存转储类型枚举
 * @details 定义不同的内存转储目标和范围选项
 */
enum MemDumpType {
    DUMP_TO_STDOUT,             // 转储全部内容到标准输出
    DUMP_REGION_TO_STDOUT,      // 转储指定区域到标准输出
    DUMP_TO_FILE,               // 转储全部内容到文件
    DUMP_REGION_TO_FILE         // 转储指定区域到文件
};

/**
 * @brief 内存转储参数结构体
 * @details 包含内存转储所需的类型、地址范围和文件路径信息
 */
struct MemDumpParam {
    enum MemDumpType type;      // 转储类型，参考MemDumpType枚举
    unsigned long long start;   // 起始地址
    unsigned long long size;    // 转储大小（字节）
    char filePath[PATH_MAX_LEN];// 输出文件路径，当类型为文件转储时有效
};

/**
 * @brief HiDumper适配器结构体
 * @details 定义系统信息转储的回调函数集合
 */
struct HiDumperAdapter {
    void (*DumpSysInfo)(void);          // 转储系统信息
    void (*DumpCpuUsage)(void);         // 转储CPU使用率
    void (*DumpMemUsage)(void);         // 转储内存使用率
    void (*DumpTaskInfo)(void);         // 转储任务信息
    void (*DumpFaultLog)(void);         // 转储故障日志
    void (*DumpMemData)(struct MemDumpParam *param);  // 转储内存数据
    void (*InjectKernelCrash)(void);    // 注入内核崩溃（用于测试）
};

#define HIDUMPER_IOC_BASE            'd'  // IO控制命令基值，使用字符'd'
#define HIDUMPER_DUMP_ALL            _IO(HIDUMPER_IOC_BASE, 1)  // 转储所有系统信息
#define HIDUMPER_CPU_USAGE           _IO(HIDUMPER_IOC_BASE, 2)  // 转储CPU使用率信息
#define HIDUMPER_MEM_USAGE           _IO(HIDUMPER_IOC_BASE, 3)  // 转储内存使用信息
#define HIDUMPER_TASK_INFO           _IO(HIDUMPER_IOC_BASE, 4)  // 转储任务信息
#define HIDUMPER_INJECT_KERNEL_CRASH _IO(HIDUMPER_IOC_BASE, 5)  // 注入内核崩溃测试
#define HIDUMPER_DUMP_FAULT_LOG      _IO(HIDUMPER_IOC_BASE, 6)  // 转储故障日志
#define HIDUMPER_MEM_DATA            _IOW(HIDUMPER_IOC_BASE, 7, struct MemDumpParam)  // 转储指定内存区域，参数为MemDumpParam结构体

int HiDumperRegisterAdapter(struct HiDumperAdapter *pAdapter);
int OsHiDumperDriverInit(void);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif