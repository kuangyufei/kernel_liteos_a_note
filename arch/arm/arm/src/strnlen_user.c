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

#include "los_strnlen_user.h"
#include "los_user_get.h"
#include "los_vm_map.h"

/**
 * @brief   安全计算用户空间字符串长度，带长度限制
 * @param   src     用户空间字符串起始地址
 * @param   count   最大允许检查的字节数
 * @return  字符串长度（不包含终止符'\0'），若：
 *          - 地址非法或count<=0：返回0
 *          - 读取用户空间失败：返回0
 *          - 未找到终止符：返回count + 1
 * @note    确保用户空间访问安全性，防止越界访问内核空间
 */
INT32 LOS_StrnlenUser(const CHAR *src, INT32 count)
{
    CHAR character;          // 临时存储读取的字符
    INT32 maxCount;          // 实际可安全检查的最大字节数
    INT32 i;                 // 循环计数器
    size_t offset = 0;       // 字符串偏移量，用于计算实际长度

    // 检查输入参数合法性：地址非用户空间或长度<=0直接返回0
    if ((!LOS_IsUserAddress((VADDR_T)(UINTPTR)src)) || (count <= 0)) {
        return 0;
    }

    // 确定安全的最大检查长度：
    // - 若整个count长度都在用户空间内，则使用count
    // - 否则使用从src到用户空间顶部的剩余长度
    maxCount = (LOS_IsUserAddressRange((VADDR_T)(UINTPTR)src, (size_t)count)) ? \
                count : (USER_ASPACE_TOP_MAX - (UINTPTR)src);

    // 遍历字符串查找终止符'\0'
    for (i = 0; i < maxCount; ++i) {
        // 安全读取用户空间字符，失败则返回0
        if (LOS_GetUser(&character, src + offset) != LOS_OK) {
            return 0;
        }
        ++offset;               // 偏移量递增（长度计数）
        if (character == '\0') { // 找到终止符，返回当前长度
            return offset;
        }
    }

    // 遍历完maxCount仍未找到终止符，返回count + 1表示未完全读取
    return count + 1;
}
