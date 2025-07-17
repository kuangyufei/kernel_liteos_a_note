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

#include "trace_tlv.h"
#include "securec.h"

#define CRC_WIDTH  8  // CRC计算宽度，8位(一个字节)
#define CRC_POLY   0x1021  // CRC多项式，十六进制0x1021对应十进制4129
#define CRC_TOPBIT 0x8000  // CRC最高位掩码，十六进制0x8000对应十进制32768

/**
 * @ingroup los_trace
 * @brief 计算缓冲区数据的CRC16校验值
 *
 * @par 功能描述
 * 本函数采用CRC16-CCITT标准算法计算数据缓冲区的校验值，用于数据完整性验证。
 * 多项式为0x1021，初始值为0，不进行数据反转和结果异或。
 * @attention
 * <ul>
 * <li>仅在当前文件内可见(STATIC)，不对外暴露接口</li>
 * <li>循环中先处理高字节后处理低字节，符合网络字节序要求</li>
 * </ul>
 *
 * @param  buf  [IN] const UINT8*类型，指向待计算CRC的数据缓冲区
 * @param  len  [IN] UINT32类型，待计算CRC的数据长度(字节)
 *
 * @retval #UINT16 计算得到的16位CRC校验值
 *
 * @see CRC_WIDTH, CRC_POLY, CRC_TOPBIT
 */
STATIC UINT16 CalcCrc16(const UINT8 *buf, UINT32 len)
{
    UINT32 i;                  // 位循环计数器
    UINT16 crc = 0;            // CRC校验值，初始化为0

    // 遍历数据缓冲区的每个字节
    for (; len > 0; len--) {
        // 将当前字节左移CRC_WIDTH(8)位后与CRC值异或，对齐高字节
        crc = crc ^ (*buf++ << CRC_WIDTH);
        
        // 对当前字节的每一位进行CRC计算
        for (i = 0; i < CRC_WIDTH; i++) {
            // 如果CRC最高位为1，则左移后与多项式异或
            if (crc & CRC_TOPBIT) {
                crc = (crc << 1) ^ CRC_POLY;
            } else {
                // 否则仅左移一位
                crc <<= 1;
            }
        }
    }
    return crc;  // 返回最终计算的CRC16值
}

/**
 * @ingroup los_trace
 * @brief 将数据按照TLV格式写入缓冲区
 *
 * @par 功能描述
 * 本函数实现单个TLV(Type-Length-Value)三元组的构建，将类型、长度和数据值依次写入目标缓冲区。
 * TLV格式为：1字节类型 + 1字节长度 + N字节数据(N=len参数)。
 * @attention
 * <ul>
 * <li>仅在当前文件内可见(STATIC)，不对外暴露接口</li>
 * <li>当len为0时直接返回0，不写入任何数据</li>
 * <li>为追求性能未检查memcpy_s返回值，拷贝失败时仅丢弃当前TLV包</li>
 * </ul>
 *
 * @param  tlvBuf  [OUT] UINT8*类型，指向输出TLV缓冲区的指针
 * @param  type    [IN]  UINT8类型，TLV类型字段
 * @param  len     [IN]  UINT8类型，数据部分长度(字节)
 * @param  value   [IN]  UINT8*类型，指向待写入数据的指针
 *
 * @retval #0        输入长度为0，写入失败
 * @retval #UINT32   成功写入的总字节数(类型1字节+长度1字节+数据len字节)
 *
 * @see TraceMsgTlvBody, memcpy_s
 */
STATIC UINT32 OsWriteTlv(UINT8 *tlvBuf, UINT8 type, UINT8 len, UINT8 *value)
{
    // 将输出缓冲区指针转换为TLV主体结构体指针
    TraceMsgTlvBody *body = (TraceMsgTlvBody *)tlvBuf;

    // 长度为0时不写入数据，直接返回
    if (len == 0) {
        return 0;
    }

    // 设置TLV类型和长度字段
    body->type = type;
    body->len = len;
    /* 为追求性能未检查返回值，若拷贝失败仅丢弃当前包 */
    (VOID)memcpy_s(body->value, len, value, len);  // 拷贝数据值到TLV缓冲区

    // 返回总写入字节数：类型(1字节) + 长度(1字节) + 数据(len字节)
    return len + sizeof(body->type) + sizeof(body->len);
}

/*
解码步骤

读取 Tag（或Type）并使用 ntohl 将其转成主机字节序，指针偏移4；
读取 Length ntohl** 将其转成主机字节序，指针偏移4；
根据得到的长度读取 Value，若为 int、char、short、long 类型，将其转为主机字节序，指针偏移；若值为字符串，读取后指针偏移 Length；
重复上述三步，继续读取后面的 TLV 单元。
*/

/**
 * @ingroup los_trace
 * @brief 将原始数据按照TLV编码表转换为TLV格式数组
 *
 * @par 功能描述
 * 本函数是TLV编码的核心实现，通过遍历编码表(TlvTable)将原始数据缓冲区中的字段逐个转换为TLV三元组，
 * 并依次写入目标缓冲区。编码表以TRACE_TLV_TYPE_NULL(0xFF)作为结束标志。
 * @attention
 * <ul>
 * <li>仅在当前文件内可见(STATIC)，不对外暴露接口</li>
 * <li>当剩余缓冲区不足时会提前终止编码并返回已编码长度</li>
 * </ul>
 *
 * @param  table      [IN]  const TlvTable*类型，TLV编码表数组，每个元素描述一个字段的编码规则
 * @param  srcBuf     [IN]  UINT8*类型，指向原始数据缓冲区的指针
 * @param  tlvBuf     [OUT] UINT8*类型，指向输出TLV数组缓冲区的指针
 * @param  tlvBufLen  [IN]  INT32类型，TLV缓冲区的总长度(字节)
 *
 * @retval #UINT32 成功编码的总字节数，可能小于预期(缓冲区不足时)
 *
 * @see TlvTable, OsWriteTlv, TRACE_TLV_TYPE_NULL
 */
STATIC UINT32 OsTlvEncode(const TlvTable *table, UINT8 *srcBuf, UINT8 *tlvBuf, INT32 tlvBufLen)
{
    UINT32 len = 0;                      // 已编码的总长度计数器(字节)
    const TlvTable *tlvTableItem = table; // 编码表当前项指针，用于遍历编码表

    // 遍历编码表，直到遇到结束标志(TRACE_TLV_TYPE_NULL=0xFF)
    while (tlvTableItem->tag != TRACE_TLV_TYPE_NULL) {
        // 检查缓冲区是否足够：当前长度 + 当前项所需空间(类型1字节+长度1字节+数据长度) > 总长度
        if ((len + tlvTableItem->elemSize + sizeof(UINT8) + sizeof(UINT8)) > tlvBufLen) {
            break;  // 缓冲区不足，终止编码
        }
        // 写入单个TLV三元组，并累加编码长度
        // 参数：目标位置=tlvBuf+len，标签=tag，长度=elemSize，数据=srcBuf+elemOffset
        len += OsWriteTlv(tlvBuf + len, tlvTableItem->tag, tlvTableItem->elemSize, srcBuf + tlvTableItem->elemOffset);
        tlvTableItem++;  // 移动到编码表下一项
    }
    return len;  // 返回实际编码的总字节数
}


/*!
对trace数据进行 Tlv 编码

TLV 是一种可变的格式，其中：

T 可以理解为 Tag 或 Type ，用于标识标签或者编码格式信息；
L 定义数值的长度；
V 表示实际的数值。
T 和 L 的长度固定，一般是2或4个字节，V 的长度由 Length 指定。

要正确的解析对方发来的数据除了统一数据格式之外还要统一字节序。字节序是指多字节数据在计算机内存中存储
或者网络传输时各字节的存储顺序。字节序一般分为大端和小端。

大端模式（Big-Endian）： 高位字节放在内存的低地址端，低位字节排放在内存的高地址端。
小端模式（Little-Endian）： 低位字节放在内存的低地址端，高位字节放在内存的高地址端。

使用 htonl 将 Tag（或Type）转成网络字节序，指针偏移 4；
使用 htonl 将 Length 转成网络字节序，指针偏移 4；
若值 Value 为 int、char、short、long 类型，将其转为网络字节序，指针偏移；若值为字符串，写入后指针偏移 Length；
重复上述三步，继续编码后面的 TLV 单元。
*/

/**
 * @ingroup los_trace
 * @brief 将跟踪原始数据编码为TLV格式消息
 *
 * @par 功能描述
 * 本函数实现跟踪数据的TLV格式编码，包括头部构造、数据编码和CRC校验。
 * 完整的TLV消息由头部(TraceMsgTlvHead)和主体(TLV数组)组成，用于可靠的数据传输。
 * @attention
 * <ul>
 * <li>目标缓冲区必须足够大，至少能容纳头部(sizeof(TraceMsgTlvHead))和编码后的数据</li>
 * <li>table参数为NULL时会直接返回失败</li>
 * </ul>
 *
 * @param  type     [IN]  UINT8类型，标识源数据结构体类型，用于选择编码规则
 * @param  table    [IN]  TlvTable*类型，TLV编码表，定义原始数据到TLV元素的映射关系
 * @param  src      [IN]  UINT8*类型，指向原始跟踪数据缓冲区的指针
 * @param  dest     [OUT] UINT8*类型，指向输出TLV消息缓冲区的指针
 * @param  destLen  [IN]  INT32类型，目标缓冲区的总长度(字节)
 *
 * @retval #0        编码失败，可能原因：缓冲区不足或参数无效
 * @retval #UINT32   成功编码的总字节数(包括头部和主体)
 *
 * @see TraceMsgTlvHead, TlvTable, OsTlvEncode, CalcCrc16
 */
UINT32 OsTraceDataEncode(UINT8 type, const TlvTable *table, UINT8 *src, UINT8 *dest, INT32 destLen)
{
    UINT16 crc;                  // CRC16校验值，用于验证TLV主体数据完整性
    INT32 len;                   // TLV主体编码后的长度(字节)
    INT32 tlvBufLen;             // TLV主体缓冲区可用长度 = 总长度 - 头部长度
    UINT8 *tlvBuf = NULL;        // 指向TLV主体缓冲区的指针

    // 将目标缓冲区起始地址转换为TLV头部结构体指针
    TraceMsgTlvHead *head = (TraceMsgTlvHead *)dest;
    // 计算TLV主体可用缓冲区长度
    tlvBufLen = destLen - sizeof(TraceMsgTlvHead);

    // 参数合法性检查：主体缓冲区不足或编码表为空则返回失败
    if ((tlvBufLen <= 0) || (table == NULL)) {
        return 0;
    }

    // 设置TLV主体缓冲区起始地址(跳过头部)
    tlvBuf = dest + sizeof(TraceMsgTlvHead);
    // 调用TLV编码核心函数，将原始数据编码为TLV格式
    len = OsTlvEncode(table, src, tlvBuf, tlvBufLen);
    // 计算TLV主体数据的CRC16校验值
    crc = CalcCrc16(tlvBuf, len);

    // 填充TLV消息头部信息
    head->magicNum = TRACE_TLV_MSG_HEAD;  // 设置魔术数(0xFF)，用于消息识别
    head->msgType  = type;                // 设置消息类型，标识数据类别
    head->len      = len;                 // 设置主体数据长度
    head->crc      = crc;                 // 设置CRC16校验值

    // 返回总编码长度 = 头部长度 + 主体长度
    return len + sizeof(TraceMsgTlvHead);
}
