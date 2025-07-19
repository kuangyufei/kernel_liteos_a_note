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

#ifndef _LOS_CIRBUF_PRI_H
#define _LOS_CIRBUF_PRI_H

#include "los_typedef.h"
#include "los_spinlock.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

typedef enum {
    CBUF_UNUSED,  // 缓冲区未使用状态
    CBUF_USED     // 缓冲区已使用状态
} CirBufStatus;  // 循环缓冲区状态枚举类型

typedef struct {
    UINT32 startIdx;       // 缓冲区起始索引
    UINT32 endIdx;         // 缓冲区结束索引
    UINT32 size;           // 缓冲区总大小
    UINT32 remain;         // 缓冲区剩余空间
    SPIN_LOCK_S lock;      // 自旋锁，用于多线程同步
    CirBufStatus status;   // 缓冲区状态标识
    CHAR *fifo;            // 数据存储指针
} CirBuf;  // 循环缓冲区结构体定义


extern UINT32 LOS_CirBufInit(CirBuf *cirbufCB, CHAR *fifo, UINT32 size);
extern VOID LOS_CirBufDeinit(CirBuf *cirbufCB);
extern UINT32 LOS_CirBufWrite(CirBuf *cirbufCB, const CHAR *buf, UINT32 size);
extern UINT32 LOS_CirBufRead(CirBuf *cirbufCB, CHAR *buf, UINT32 size);
extern UINT32 LOS_CirBufUsedSize(CirBuf *cirbufCB);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _LOS_CIRBUF_PRI_H */
