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

#include "los_typedef.h"
#include "los_memory.h"

/**
 * @brief 按指定对齐方式分配内存
 * @param memAddr 用于存储分配的内存地址的指针
 * @param alignment 内存对齐要求（必须是2的幂且是void*大小的倍数）
 * @param size 要分配的内存大小（字节）
 * @return 成功返回ENOERR，失败返回错误码（EINVAL或ENOMEM）
 */
int posix_memalign(void **memAddr, size_t alignment, size_t size)
{
    // 检查对齐参数是否有效：非零、2的幂、且是指针大小的倍数
    if ((alignment == 0) || ((alignment & (alignment - 1)) != 0) || ((alignment % sizeof(void *)) != 0)) {
        return EINVAL;  // 对齐参数无效，返回EINVAL
    }

    // 从系统内存区域按指定对齐方式分配内存
    *memAddr = LOS_MemAllocAlign(OS_SYS_MEM_ADDR, size, alignment);
    if (*memAddr == NULL) {  // 检查内存分配是否成功
        return ENOMEM;       // 分配失败，返回ENOMEM
    }

    return ENOERR;  // 成功返回ENOERR
}
