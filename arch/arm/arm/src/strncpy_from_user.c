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

#include "los_strncpy_from_user.h"
#include "los_user_get.h"
#include "los_vm_map.h"

/**
 * @brief 从用户空间复制字符串到内核空间（带长度限制）
 *
 * 安全地将字符串从用户空间复制到内核空间，进行地址有效性检查，并限制最大复制长度
 * 当遇到字符串结束符'\0'或达到最大长度时停止复制
 *
 * @param[in]  dst   内核空间目标缓冲区指针
 * @param[in]  src   用户空间源字符串指针
 * @param[in]  count 最大复制字节数（包含结束符）
 *
 * @return 成功时返回实际复制的字节数（不含结束符）；失败时返回-EFAULT
 */
INT32 LOS_StrncpyFromUser(CHAR *dst, const CHAR *src, INT32 count)
{
    CHAR character;               // 临时存储从用户空间读取的字符
    INT32 i;                      // 循环计数器
    INT32 maxCount;               // 实际允许的最大复制长度
    size_t offset = 0;            // 复制偏移量（已复制字节数）

    // 参数合法性检查：
    // 1. 目标地址必须是内核空间地址
    // 2. 源地址必须是用户空间地址
    // 3. 复制长度必须大于0
    if ((!LOS_IsKernelAddress((VADDR_T)(UINTPTR)dst)) || (!LOS_IsUserAddress((VADDR_T)(UINTPTR)src)) || (count <= 0)) {
        return -EFAULT;           // 地址非法或长度无效，返回错误码
    }

    // 计算实际可安全复制的最大长度：
    // 如果用户空间源地址范围有效（长度count内无越界），则使用count
    // 否则使用从src到用户空间顶部的剩余字节数作为最大长度
    maxCount = (LOS_IsUserAddressRange((VADDR_T)(UINTPTR)src, (size_t)count)) ? \
                count : (INT32)(USER_ASPACE_TOP_MAX - (UINTPTR)src);

    // 逐个字节从用户空间复制到内核空间
    for (i = 0; i < maxCount; ++i) {
        // 安全读取用户空间字符（包含地址有效性检查）
        if (LOS_GetUser(&character, src + offset) != LOS_OK) {
            return -EFAULT;       // 读取失败（地址越界或访问异常）
        }
        // 将字符写入内核空间缓冲区
        *(CHAR *)(dst + offset) = character;
        // 检查是否遇到字符串结束符
        if (character == '\0') {
            return offset;        // 成功复制，返回实际字节数（不含结束符）
        }
        ++offset;                 // 偏移量递增
    }

    // 达到最大长度仍未遇到结束符，返回实际复制的字节数
    return offset;
}