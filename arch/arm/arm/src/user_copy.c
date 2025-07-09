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

#include "user_copy.h"
#include "arm_user_copy.h"
#include "arm_user_clear.h"
#include "securec.h"
#include "los_memory.h"
#include "los_vm_map.h"

/**
 * @brief   用户空间到内核空间数据复制的架构层包装函数
 * @param   dst     内核空间目标地址
 * @param   src     用户空间源地址
 * @param   len     要复制的字节数
 * @return  成功复制的字节数，失败返回剩余未复制字节数
 * @note    实际调用LOS_ArchCopyFromUser实现核心逻辑
 */
size_t arch_copy_from_user(void *dst, const void *src, size_t len)
{
    return LOS_ArchCopyFromUser(dst, src, len);
}

/**
 * @brief   用户空间到内核空间数据复制的实际实现
 * @param   dst     内核空间目标地址
 * @param   src     用户空间源地址
 * @param   len     要复制的字节数
 * @return  0表示成功，非0表示失败（返回未复制的字节数）
 * @note    首先验证用户空间地址范围合法性，非法则直接返回len表示复制失败
 */
size_t LOS_ArchCopyFromUser(void *dst, const void *src, size_t len)
{
    // 检查源地址是否完全位于用户空间
    if (!LOS_IsUserAddressRange((VADDR_T)(UINTPTR)src, len)) {
        return len;  // 地址非法，返回未复制字节数
    }

    // 调用ARM架构专用的用户空间复制函数
    return _arm_user_copy(dst, src, len);
}

/**
 * @brief   内核空间到用户空间数据复制的架构层包装函数
 * @param   dst     用户空间目标地址
 * @param   src     内核空间源地址
 * @param   len     要复制的字节数
 * @return  成功复制的字节数，失败返回剩余未复制字节数
 * @note    实际调用LOS_ArchCopyToUser实现核心逻辑
 */
size_t arch_copy_to_user(void *dst, const void *src, size_t len)
{
    return LOS_ArchCopyToUser(dst, src, len);
}

/**
 * @brief   内核空间到用户空间数据复制的实际实现
 * @param   dst     用户空间目标地址
 * @param   src     内核空间源地址
 * @param   len     要复制的字节数
 * @return  0表示成功，非0表示失败（返回未复制的字节数）
 * @note    首先验证用户空间地址范围合法性，非法则直接返回len表示复制失败
 */
size_t LOS_ArchCopyToUser(void *dst, const void *src, size_t len)
{
    // 检查目标地址是否完全位于用户空间
    if (!LOS_IsUserAddressRange((VADDR_T)(UINTPTR)dst, len)) {
        return len;  // 地址非法，返回未复制字节数
    }

    // 调用ARM架构专用的用户空间复制函数
    return _arm_user_copy(dst, src, len);
}

/**
 * @brief   从内核空间复制数据到用户空间（带长度检查）
 * @param   dest    用户空间目标缓冲区
 * @param   max     目标缓冲区最大容量
 * @param   src     内核空间源数据地址
 * @param   count   要复制的数据字节数
 * @return  0表示成功，非0表示失败（EFAULT/ERANGE）
 * @note    根据目标地址空间类型选择不同复制策略，确保安全边界检查
 */
INT32 LOS_CopyFromKernel(VOID *dest, UINT32 max, const VOID *src, UINT32 count)
{
    INT32 ret;  // 复制操作返回值

    // 判断目标地址是否为内核空间
    if (!LOS_IsUserAddressRange((VADDR_T)(UINTPTR)dest, count)) {
        // 内核空间复制：使用安全的memcpy_s（带缓冲区溢出检查）
        ret = memcpy_s(dest, max, src, count);
    } else {
        // 用户空间复制：先检查缓冲区容量，足够则调用架构复制函数
        ret = ((max >= count) ? _arm_user_copy(dest, src, count) : ERANGE_AND_RESET);
    }

    return ret;
}

/**
 * @brief   从用户空间复制数据到内核空间（带长度检查）
 * @param   dest    内核空间目标缓冲区
 * @param   max     目标缓冲区最大容量
 * @param   src     用户空间源数据地址
 * @param   count   要复制的数据字节数
 * @return  0表示成功，非0表示失败（EFAULT/ERANGE）
 * @note    根据源地址空间类型选择不同复制策略，确保安全边界检查
 */
INT32 LOS_CopyToKernel(VOID *dest, UINT32 max, const VOID *src, UINT32 count)
{
    INT32 ret;  // 复制操作返回值

    // 判断源地址是否为内核空间
    if (!LOS_IsUserAddressRange((vaddr_t)(UINTPTR)src, count)) {
        // 内核空间复制：使用安全的memcpy_s（带缓冲区溢出检查）
        ret = memcpy_s(dest, max, src, count);
    } else {
        // 用户空间复制：先检查缓冲区容量，足够则调用架构复制函数
        ret = ((max >= count) ? _arm_user_copy(dest, src, count) : ERANGE_AND_RESET);
    }

    return ret;
}

/**
 * @brief   清除用户空间内存区域（填充0）
 * @param   buf     用户空间内存起始地址
 * @param   len     要清除的字节数
 * @return  0表示成功，-EFAULT表示失败
 * @note    根据地址空间类型选择不同清除策略，确保操作安全性
 */
INT32 LOS_UserMemClear(unsigned char *buf, UINT32 len)
{
    INT32 ret = 0;  // 清除操作返回值
    // 判断缓冲区是否为内核空间
    if (!LOS_IsUserAddressRange((vaddr_t)(UINTPTR)buf, len)) {
        // 内核空间清除：使用安全的memset_s（带缓冲区溢出检查）
        (VOID)memset_s(buf, len, 0, len);
    } else {
        // 用户空间清除：调用ARM架构专用清除函数，失败返回-EFAULT
        if (_arm_clear_user(buf, len)) {
            return -EFAULT;
        }
    }
    return ret;
}

