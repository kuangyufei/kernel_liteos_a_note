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

#include "los_cir_buf.h"
#include "los_spinlock.h"


/// 返回循环buf已使用的大小
UINT32 LOS_CirBufUsedSize(CirBuf *cirbufCB)
{
    UINT32 size;
    UINT32 intSave;

    LOS_SpinLockSave(&cirbufCB->lock, &intSave);
    size = cirbufCB->size - cirbufCB->remain; //得到已使用大小
    LOS_SpinUnlockRestore(&cirbufCB->lock, intSave);

    return size;
}

/* 图形表示写循环buf linear 模式 ，图表示 写之前的样子 @note_pic 
 *                    startIdx
 *                    |
 *    0 0 0 0 0 0 0 0 X X X X X X X X 0 0 0 0 0 0
 *                                    |
 *                                  endIdx
 *///写操作的是endIdx    | X 表示剩余size index 从左到右递减
STATIC UINT32 OsCirBufWriteLinear(CirBuf *cirbufCB, const CHAR *buf, UINT32 size)
{
    UINT32 cpSize;
    errno_t err;

    cpSize = (cirbufCB->remain < size) ? cirbufCB->remain : size;//得到cp的大小

    if (cpSize == 0) {
        return 0;
    }

    err = memcpy_s(cirbufCB->fifo + cirbufCB->endIdx, cirbufCB->remain, buf, cpSize);//完成拷贝
    if (err != EOK) {
        return 0;
    }

    cirbufCB->remain -= cpSize;//写完了那总剩余size肯定是要减少的
    cirbufCB->endIdx += cpSize;//结尾变大

    return cpSize;
}
/* 图形表示写循环buf        loop 模式 ，图表示 写之前的样子              @note_pic
 *                    endIdx            第二阶段拷贝
 *                    |               |             |
 *    X X X X X X X X 0 0 0 0 0 0 0 0 X X X X X X X X
 *    |               |               |
 *     第一阶段拷贝                         startIdx
 *///写操作的是endIdx | X 表示剩余size index 从左到右递减
STATIC UINT32 OsCirBufWriteLoop(CirBuf *cirbufCB, const CHAR *buf, UINT32 size)
{
    UINT32 right, cpSize;
    errno_t err;

    right = cirbufCB->size - cirbufCB->endIdx;//先计算右边部分
    cpSize = (right < size) ? right : size;//算出cpSize，很可能要分两次

    err = memcpy_s(cirbufCB->fifo + cirbufCB->endIdx, right, buf, cpSize);//先拷贝一部分
    if (err != EOK) {
        return 0;
    }

    cirbufCB->remain -= cpSize;//写完了那总剩余size肯定是要减少的
    cirbufCB->endIdx += cpSize;//endIdx 增加，一直往后移
    if (cirbufCB->endIdx == cirbufCB->size) {//这个相当于移到最后一个了
        cirbufCB->endIdx = 0;//循环从这里开始
    }

    if (cpSize == size) {//拷贝完了的情况
        return size;//返回size
    } else {
        cpSize += OsCirBufWriteLinear(cirbufCB, buf + cpSize, size - cpSize);//需第二次拷贝
    }

    return cpSize;
}
///写入数据到循环buf区
UINT32 LOS_CirBufWrite(CirBuf *cirbufCB, const CHAR *buf, UINT32 size)
{
    UINT32 cpSize = 0;
    UINT32 intSave;

    if ((cirbufCB == NULL) || (buf == NULL) || (size == 0) || (cirbufCB->status != CBUF_USED)) {
        return 0;
    }

    LOS_SpinLockSave(&cirbufCB->lock, &intSave);

    if ((cirbufCB->fifo == NULL) || (cirbufCB->remain == 0))  {
        goto EXIT;;
    }

    if (cirbufCB->startIdx <= cirbufCB->endIdx) {//开始位置在前面
        cpSize = OsCirBufWriteLoop(cirbufCB, buf, size);//循环方式写入，分两次拷贝
    } else {
        cpSize = OsCirBufWriteLinear(cirbufCB, buf, size);//线性方式写入，分一次拷贝
    }

EXIT:
    LOS_SpinUnlockRestore(&cirbufCB->lock, intSave);
    return cpSize;
}
/* 图形表示读线性buf        linear 模式 ，图表示 读之前的样子             @note_pic
 *                    endIdx            
 *                    |               
 *    X X X X X X X X 0 0 0 0 0 0 0 0 X X X X X X X X
 *                   				  |
 *                              	  startIdx
 *///读操作的是startIdx | X 表示剩余size index 从左到右递减
STATIC UINT32 OsCirBufReadLinear(CirBuf *cirbufCB, CHAR *buf, UINT32 size)
{
    UINT32 cpSize, remain;
    errno_t err;

    remain = cirbufCB->endIdx - cirbufCB->startIdx;//这里表示剩余可读区
    cpSize = (remain < size) ? remain : size;

    if (cpSize == 0) {
        return 0;
    }

    err = memcpy_s(buf, size, cirbufCB->fifo + cirbufCB->startIdx, cpSize);//完成拷贝
    if (err != EOK) {
        return 0;
    }

    cirbufCB->remain += cpSize;//读完了那总剩余size肯定是要增加的
    cirbufCB->startIdx += cpSize;//startIdx也要往前移动

    return cpSize;
}
/* 图形表示读循环buf loop 模式 ，图表示 读之前的样子 @note_pic 
 *                    startIdx
 *                    |
 *    0 0 0 0 0 0 0 0 X X X X X X X X 0 0 0 0 0 0
 *                                    |
 *                                  endIdx
 *///读操作的是startIdx    | X 表示剩余size index 从左到右递减
STATIC UINT32 OsCirBufReadLoop(CirBuf *cirbufCB, CHAR *buf, UINT32 size)
{
    UINT32 right, cpSize;
    errno_t err;

    right = cirbufCB->size - cirbufCB->startIdx;//先算出要读取的部分
    cpSize = (right < size) ? right : size;

    err = memcpy_s(buf, size, cirbufCB->fifo + cirbufCB->startIdx, cpSize);//先读第一部分数据
    if (err != EOK) {
        return 0;
    }

    cirbufCB->remain += cpSize;//读完了那总剩余size肯定是要增加的
    cirbufCB->startIdx += cpSize;//startIdx也要往前移动
    if (cirbufCB->startIdx == cirbufCB->size) {//如果移动到头了
        cirbufCB->startIdx = 0;//循环buf的关键,新的循环开始了
    }

    if (cpSize < size) {
        cpSize += OsCirBufReadLinear(cirbufCB, buf + cpSize, size - cpSize);//剩余的就交给线性方式读取了
    }

    return cpSize;
}
///读取循环buf的数据
UINT32 LOS_CirBufRead(CirBuf *cirbufCB, CHAR *buf, UINT32 size)
{
    UINT32 cpSize = 0;
    UINT32 intSave;

    if ((cirbufCB == NULL) || (buf == NULL) || (size == 0) || (cirbufCB->status != CBUF_USED)) {
        return 0;
    }

    LOS_SpinLockSave(&cirbufCB->lock, &intSave);

    if ((cirbufCB->fifo == NULL) || (cirbufCB->remain == cirbufCB->size)) {
        goto EXIT;
    }

    if (cirbufCB->startIdx >= cirbufCB->endIdx) {//开始位置大于结束位置的情况怎么读
        cpSize = OsCirBufReadLoop(cirbufCB, buf, size);//循环读取buf
    } else {//开始位置小于结束位置的情况怎么读
        cpSize = OsCirBufReadLinear(cirbufCB, buf, size);//线性读取，读取 endIdx - startIdx 部分就行了，所以是线性读取 
    }

EXIT:
    LOS_SpinUnlockRestore(&cirbufCB->lock, intSave);
    return cpSize;
}
///初始化循环buf
UINT32 LOS_CirBufInit(CirBuf *cirbufCB, CHAR *fifo, UINT32 size)
{
    if ((cirbufCB == NULL) || (fifo == NULL)) {
        return LOS_NOK;
    }

    (VOID)memset_s(cirbufCB, sizeof(CirBuf), 0, sizeof(CirBuf));//清0
    LOS_SpinInit(&cirbufCB->lock);//自旋锁初始化
    cirbufCB->size = size;	//记录size
    cirbufCB->remain = size;//剩余size
    cirbufCB->status = CBUF_USED;//标记为已使用
    cirbufCB->fifo = fifo;	//顺序buf ,这1K buf 是循环利用

    return LOS_OK;
}
///删除初始化操作，其实就是清0
VOID LOS_CirBufDeinit(CirBuf *cirbufCB)
{
    (VOID)memset_s(cirbufCB, sizeof(CirBuf), 0, sizeof(CirBuf));
}

