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

#define PATH_MAX_LEN     256

enum MemDumpType {
    DUMP_TO_STDOUT,
    DUMP_REGION_TO_STDOUT,
    DUMP_TO_FILE,
    DUMP_REGION_TO_FILE
};

struct MemDumpParam {
    enum MemDumpType type;
    unsigned long long start;
    unsigned long long size;
    char filePath[PATH_MAX_LEN];
};

struct HiDumperAdapter {
    void (*DumpSysInfo)(void);
    void (*DumpCpuUsage)(void);
    void (*DumpMemUsage)(void);
    void (*DumpTaskInfo)(void);
    void (*DumpFaultLog)(void);
    void (*DumpMemData)(struct MemDumpParam *param);
    void (*InjectKernelCrash)(void);
};

#define HIDUMPER_IOC_BASE            'd'
#define HIDUMPER_DUMP_ALL            _IO(HIDUMPER_IOC_BASE, 1)
#define HIDUMPER_CPU_USAGE           _IO(HIDUMPER_IOC_BASE, 2)
#define HIDUMPER_MEM_USAGE           _IO(HIDUMPER_IOC_BASE, 3)
#define HIDUMPER_TASK_INFO           _IO(HIDUMPER_IOC_BASE, 4)
#define HIDUMPER_INJECT_KERNEL_CRASH _IO(HIDUMPER_IOC_BASE, 5)
#define HIDUMPER_DUMP_FAULT_LOG      _IO(HIDUMPER_IOC_BASE, 6)
#define HIDUMPER_MEM_DATA            _IOW(HIDUMPER_IOC_BASE, 7, struct MemDumpParam)

int HiDumperRegisterAdapter(struct HiDumperAdapter *pAdapter);
int OsHiDumperDriverInit(void);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif