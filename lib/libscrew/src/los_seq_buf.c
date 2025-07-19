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

#include "los_seq_buf.h"
#include "los_typedef.h"
#include <stdlib.h>
#include "securec.h"

/**
 * @brief 扩展序列缓冲区大小
 * @param seqBuf [IN/OUT] 指向序列缓冲区结构体的指针
 * @param oldCount [IN] 扩展前的有效数据长度
 * @return 成功返回LOS_OK，失败返回-LOS_NOK
 * @note 缓冲区大小翻倍扩展，最大不超过SEQBUF_LIMIT_SIZE
 */
static int ExpandSeqBuf(struct SeqBuf *seqBuf, size_t oldCount)
{
    char *newBuf = NULL;  // 新缓冲区指针
    int ret;              // 函数返回值

    // 检查缓冲区结构体及内部缓冲区指针有效性
    if ((seqBuf == NULL) || (seqBuf->buf == NULL)) {
        return -LOS_NOK;
    }

    // 检查是否达到最大限制大小，避免无限扩展
    if (seqBuf->size >= SEQBUF_LIMIT_SIZE) {
        goto EXPAND_FAILED;  // 跳转至失败处理流程
    }

    // 分配新缓冲区（大小翻倍，使用左移运算实现）
    newBuf = (char*)malloc(seqBuf->size <<= 1);
    if (newBuf == NULL) {  // 内存分配失败检查
        goto EXPAND_FAILED;
    }
    // 初始化新缓冲区中超出原数据长度的部分（从oldCount开始）
    (void)memset_s(newBuf + oldCount, seqBuf->size - oldCount, 0, seqBuf->size - oldCount);

    // 将原缓冲区数据复制到新缓冲区
    ret = memcpy_s(newBuf, seqBuf->size, seqBuf->buf, oldCount);
    if (ret != LOS_OK) {  // 复制失败处理
        free(newBuf);     // 释放已分配的新缓冲区
        goto EXPAND_FAILED;
    }
    seqBuf->count = oldCount;  // 更新有效数据长度

    free(seqBuf->buf);   // 释放原缓冲区
    seqBuf->buf = newBuf;  // 指向新缓冲区

    return LOS_OK;       // 扩展成功
EXPAND_FAILED:          // 扩展失败处理标签
    free(seqBuf->buf);   // 释放原缓冲区
    seqBuf->buf = NULL;  // 重置缓冲区指针
    seqBuf->count = 0;   // 重置有效数据长度
    seqBuf->size = 0;    // 重置缓冲区大小

    return -LOS_NOK;     // 返回失败
}

/**
 * @brief 创建序列缓冲区结构体
 * @return 成功返回缓冲区结构体指针，失败返回NULL
 * @note 初始化为空缓冲区，需通过LosBufPrintf等函数填充数据
 */
struct SeqBuf *LosBufCreat(void)
{
    struct SeqBuf *seqBuf = NULL;  // 序列缓冲区结构体指针

    // 分配结构体内存
    seqBuf = (struct SeqBuf *)malloc(sizeof(struct SeqBuf));
    if (seqBuf == NULL) {  // 内存分配失败
        errno = -LOS_ENOMEM;  // 设置错误码为内存不足
        return NULL;
    }
    // 初始化结构体所有成员为0
    (void)memset_s(seqBuf, sizeof(struct SeqBuf), 0, sizeof(struct SeqBuf));

    return seqBuf;  // 返回创建的结构体指针
}

/**
 * @brief 向序列缓冲区写入格式化字符串（支持可变参数列表）
 * @param seqBuf [IN/OUT] 指向序列缓冲区结构体的指针
 * @param fmt [IN] 格式化字符串
 * @param argList [IN] 可变参数列表
 * @return 成功返回0，失败返回-LOS_NOK
 * @note 当缓冲区不足时会自动扩展，直至达到SEQBUF_LIMIT_SIZE
 */
int LosBufVprintf(struct SeqBuf *seqBuf, const char *fmt, va_list argList)
{
    bool needreprintf = FALSE;  // 是否需要重新打印标志
    int bufLen;                 // vsnprintf_s返回值

    if (seqBuf == NULL) {  // 检查缓冲区结构体有效性
        return -LOS_EPERM;  // 返回权限错误
    }

    // 如果缓冲区未初始化，则进行首次初始化
    if (seqBuf->buf == NULL) {
        seqBuf->size = SEQBUF_PAGE_SIZE;  // 设置初始大小为页大小
        seqBuf->buf = (char *)malloc(seqBuf->size);  // 分配初始缓冲区
        if (seqBuf->buf == NULL) {  // 内存分配失败
            return -LOS_ENOMEM;     // 返回内存不足错误
        }
        // 初始化缓冲区为全0
        (void)memset_s(seqBuf->buf, seqBuf->size, 0, seqBuf->size);
        seqBuf->count = 0;  // 初始有效数据长度为0
    }

    do {
        // 尝试写入格式化字符串（从当前count位置开始）
        bufLen = vsnprintf_s(seqBuf->buf + seqBuf->count, seqBuf->size - seqBuf->count,
                             seqBuf->size - seqBuf->count - 1, fmt, argList);
        if (bufLen >= 0) {  // 写入成功（vsnprintf_s返回正值表示实际写入长度）
            seqBuf->count += bufLen;  // 更新有效数据长度
            return 0;                 // 返回成功
        }
        // 检查是否写入了空字符（异常情况处理）
        if (seqBuf->buf[seqBuf->count] == '\0') {
            free(seqBuf->buf);  // 释放缓冲区
            seqBuf->buf = NULL; // 重置缓冲区指针
            break;              // 跳出循环
        }

        needreprintf = TRUE;  // 需要重新打印标志置位

        // 扩展缓冲区，若扩展失败则跳出循环
        if (ExpandSeqBuf(seqBuf, seqBuf->count) != 0) {
            break;
        }
    } while (needreprintf);  // 当需要重新打印且缓冲区扩展成功时继续尝试

    return -LOS_NOK;  // 失败返回
}

/**
 * @brief 向序列缓冲区写入格式化字符串（可变参数版本）
 * @param seqBuf [IN/OUT] 指向序列缓冲区结构体的指针
 * @param fmt [IN] 格式化字符串
 * @param ... [IN] 可变参数
 * @return 成功返回0，失败返回-LOS_NOK
 * @note 内部调用LosBufVprintf实现功能
 */
int LosBufPrintf(struct SeqBuf *seqBuf, const char *fmt, ...)
{
    va_list argList;  // 可变参数列表
    int ret;          // 函数返回值

    va_start(argList, fmt);  // 初始化可变参数列表
    ret = LosBufVprintf(seqBuf, fmt, argList);  // 调用变参版本函数
    va_end(argList);         // 结束可变参数处理

    return ret;  // 返回结果
}

/**
 * @brief 释放序列缓冲区资源
 * @param seqBuf [IN] 指向序列缓冲区结构体的指针
 * @return 成功返回LOS_OK，失败返回-LOS_EPERM
 * @note 释放内部缓冲区和结构体本身的内存
 */
int LosBufRelease(struct SeqBuf *seqBuf)
{
    if (seqBuf == NULL) {  // 检查结构体指针有效性
        return -LOS_EPERM;  // 返回权限错误
    }

    // 如果缓冲区存在则释放
    if (seqBuf->buf != NULL) {
        free(seqBuf->buf);
        seqBuf->buf = NULL;
    }
    free(seqBuf);  // 释放结构体本身

    return LOS_OK;  // 返回成功
}
