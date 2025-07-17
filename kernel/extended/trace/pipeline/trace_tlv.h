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

#ifndef _TRACE_TLV_H
#define _TRACE_TLV_H

#include "los_typedef.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

#define TRACE_TLV_MSG_HEAD  0xFF  // TLV消息头部标识，十六进制0xFF对应十进制255
#define TRACE_TLV_TYPE_NULL 0xFF  // 空类型标识，用于表示未定义或无效的TLV类型

/**
 * @ingroup los_trace
 * TLV消息头部结构：用于标识跟踪消息的基本信息和完整性校验
 */
typedef struct {
    UINT8 magicNum;   // 魔术数，固定为TRACE_TLV_MSG_HEAD(0xFF)，用于消息校验
    UINT8 msgType;    // 消息类型：标识不同的跟踪数据类别
    UINT16 len;       // 消息长度：包括头部和主体的总字节数
    UINT16 crc;       // CRC校验值：用于检测消息传输过程中的错误
} TraceMsgTlvHead;

/**
 * @ingroup los_trace
 * TLV消息主体结构：采用类型-长度-值三元组格式存储具体数据
 */
typedef struct {
    UINT8 type;       // 数据类型：标识value字段的内容类型
    UINT8 len;        // 数据长度：value字段的字节数
    UINT8 value[];    // 数据值：柔性数组，存储变长数据内容
} TraceMsgTlvBody;

/**
 * @ingroup los_trace
 * TLV编码表结构：描述原始数据到TLV格式的映射关系
 */
typedef struct {
    UINT8 tag;        // TLV标签：标识该元素的类型
    UINT8 elemOffset; // 元素偏移：原始数据结构中该元素的偏移量(字节)
    UINT8 elemSize;   // 元素大小：原始数据结构中该元素的长度(字节)
} TlvTable;

/**
 * @ingroup los_trace
 * @brief 将跟踪原始数据编码为TLV格式
 *
 * @par 功能描述
 * 本API用于将原始跟踪数据按照TLV(Type-Length-Value)格式进行编码，便于数据传输和解析
 * @attention
 * <ul>
 * <li>编码过程会在目标缓冲区生成包含TLV头部和主体的完整消息</li>
 * <li>确保目标缓冲区有足够空间，避免缓冲区溢出</li>
 * </ul>
 *
 * @param  type     [IN] 类型 #UINT8。标识源数据的结构体类型，用于选择不同编码规则
 * @param  table    [IN] 类型 #const TlvTable *。TLV编码表，描述原始数据元素的偏移和大小
 * @param  src      [IN] 类型 #UINT8 *。原始跟踪数据缓冲区指针
 * @param  dest     [OUT] 类型 #UINT8 *。输出的TLV格式数据缓冲区指针
 * @param  destLen  [IN] 类型 #INT32。目标缓冲区的最大长度(字节)
 *
 * @retval #0        编码失败
 * @retval #UINT32   成功编码的字节数
 *
 * @par 依赖
 * <ul><li>trace_tlv.h: 包含本API声明的头文件</li></ul>
 * @see OsTraceDataEncode
 */
extern UINT32 OsTraceDataEncode(UINT8 type, const TlvTable *table, UINT8 *src, UINT8 *dest, INT32 destLen);


#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* _TRACE_TLV_H */
