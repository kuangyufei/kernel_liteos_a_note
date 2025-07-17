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

#ifndef _LOS_VDSO_DATAPAGE_H
#define _LOS_VDSO_DATAPAGE_H

#include "los_typedef.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */
/**
 * @brief VDSO数据页结构，用于用户空间高效访问内核时间信息
 * 
 * 该结构存储实时时钟和单调时钟的秒级和纳秒级时间戳，
 * 以及数据页的同步锁状态，确保多线程安全访问
 */
typedef struct {
    /* Timeval */
    INT64 realTimeSec;       /* 实时时钟秒数部分，对应CLOCK_REALTIME */
    INT64 realTimeNsec;      /* 实时时钟纳秒数部分（0-999,999,999） */
    INT64 monoTimeSec;       /* 单调时钟秒数部分，对应CLOCK_MONOTONIC */
    INT64 monoTimeNsec;      /* 单调时钟纳秒数部分（0-999,999,999） */
    /* lock DataPage  0:Unlock State  1:Lock State */
    UINT64 lockCount;        /* 数据页自旋锁计数器：0=未锁定，1=已锁定 */
} VdsoDataPage;

/**
 * @def ELF_HEAD
 * @brief ELF文件魔数标识
 *
 * ELF（Executable and Linkable Format）文件的前4字节固定标识，
 * 用于验证文件格式合法性，\177为八进制表示的0x7F
 */
#define ELF_HEAD "\177ELF"

/**
 * @def ELF_HEAD_LEN
 * @brief ELF魔数标识长度
 *
 * 定义ELF文件魔数的字节数，固定为4字节
 */
#define ELF_HEAD_LEN 4

/**
 * @def MAX_PAGES
 * @brief VDSO最大映射页数
 *
 * 指定VDSO（虚拟动态共享对象）可映射的最大内存页数，
 * 当前配置为5页（通常每页4KB，总计20KB）
 */
#define MAX_PAGES 5

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_VDSO_DATAPAGE_H */