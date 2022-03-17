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


/*************************************************
从用户空间到内核空间的数据拷贝
*************************************************/
INT32 LOS_StrncpyFromUser(CHAR *dst, const CHAR *src, INT32 count)
{
    CHAR character;
    INT32 i;
    INT32 maxCount;
    size_t offset = 0;

    if ((!LOS_IsKernelAddress((VADDR_T)(UINTPTR)dst)) || (!LOS_IsUserAddress((VADDR_T)(UINTPTR)src)) || (count <= 0)) {
        return -EFAULT;//判断是否在各自空间
    }

    maxCount = (LOS_IsUserAddressRange((VADDR_T)(UINTPTR)src, (size_t)count)) ? \
                count : (INT32)(USER_ASPACE_TOP_MAX - (UINTPTR)src);//最大能拷贝的数据量,结束地址不能超过 USER_ASPACE_TOP_MAX
															//USER_ASPACE_TOP_MAX 是用户空间能触及的最大虚拟内存空间地址
    for (i = 0; i < maxCount; ++i) {//一个个字符拷贝
        if (LOS_GetUser(&character, src + offset) != LOS_OK) {
            return -EFAULT;
        }
        *(CHAR *)(dst + offset) = character;
        if (character == '\0') {
            return offset;
        }
        ++offset;
    }

    return offset;
}

