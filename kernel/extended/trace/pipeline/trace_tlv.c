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

#define CRC_WIDTH  8
#define CRC_POLY   0x1021
#define CRC_TOPBIT 0x8000

STATIC UINT16 CalcCrc16(const UINT8 *buf, UINT32 len)
{
    UINT32 i;
    UINT16 crc = 0;

    for (; len > 0; len--) {
        crc = crc ^ (*buf++ << CRC_WIDTH);
        for (i = 0; i < CRC_WIDTH; i++) {
            if (crc & CRC_TOPBIT) {
                crc = (crc << 1) ^ CRC_POLY;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

STATIC UINT32 OsWriteTlv(UINT8 *tlvBuf, UINT8 type, UINT8 len, UINT8 *value)
{
    TraceMsgTlvBody *body = (TraceMsgTlvBody *)tlvBuf;

    if (len == 0) {
        return 0;
    }

    body->type = type;
    body->len = len;
    /* Do not check return value for performance, if copy failed, only this package will be discarded */
    (VOID)memcpy_s(body->value, len, value, len);
    return len + sizeof(body->type) + sizeof(body->len);
}
/*
解码步骤

读取 Tag（或Type）并使用 ntohl 将其转成主机字节序，指针偏移4；
读取 Length ntohl** 将其转成主机字节序，指针偏移4；
根据得到的长度读取 Value，若为 int、char、short、long 类型，将其转为主机字节序，指针偏移；若值为字符串，读取后指针偏移 Length；
重复上述三步，继续读取后面的 TLV 单元。
*/
STATIC UINT32 OsTlvEncode(const TlvTable *table, UINT8 *srcBuf, UINT8 *tlvBuf, INT32 tlvBufLen)
{
    UINT32 len = 0;
    const TlvTable *tlvTableItem = table;

    while (tlvTableItem->tag != TRACE_TLV_TYPE_NULL) {
        if ((len + tlvTableItem->elemSize + sizeof(UINT8) + sizeof(UINT8)) > tlvBufLen) {
            break;
        }
        len += OsWriteTlv(tlvBuf + len, tlvTableItem->tag, tlvTableItem->elemSize, srcBuf + tlvTableItem->elemOffset);
        tlvTableItem++;
    }
    return len;
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
UINT32 OsTraceDataEncode(UINT8 type, const TlvTable *table, UINT8 *src, UINT8 *dest, INT32 destLen)
{
    UINT16 crc;
    INT32 len;
    INT32 tlvBufLen;
    UINT8 *tlvBuf = NULL;

    TraceMsgTlvHead *head = (TraceMsgTlvHead *)dest;
    tlvBufLen = destLen - sizeof(TraceMsgTlvHead);

    if ((tlvBufLen <= 0) || (table == NULL)) {
        return 0;
    }

    tlvBuf = dest + sizeof(TraceMsgTlvHead);
    len = OsTlvEncode(table, src, tlvBuf, tlvBufLen);
    crc = CalcCrc16(tlvBuf, len);

    head->magicNum = TRACE_TLV_MSG_HEAD;
    head->msgType  = type;
    head->len      = len;
    head->crc      = crc;
    return len + sizeof(TraceMsgTlvHead);
}
