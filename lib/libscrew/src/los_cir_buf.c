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

/**
 * @brief 获取循环缓冲区已使用大小
 * @param[in] cirbufCB 循环缓冲区控制块指针
 * @return 已使用大小
 */
UINT32 LOS_CirBufUsedSize(CirBuf *cirbufCB)
{
    UINT32 size;                          // 已使用大小变量
    UINT32 intSave;                       // 中断状态保存变量

    LOS_SpinLockSave(&cirbufCB->lock, &intSave);  // 获取自旋锁并保存中断状态
    size = cirbufCB->size - cirbufCB->remain;     // 计算已使用大小：总大小 - 剩余大小
    LOS_SpinUnlockRestore(&cirbufCB->lock, intSave);  // 恢复中断状态并释放自旋锁

    return size;                          // 返回已使用大小
}

/* 图形表示写循环buf linear 模式 ，图表示 写之前的样子 @note_pic 
 *                    startIdx
 *                    |
 *    0 0 0 0 0 0 0 0 X X X X X X X X 0 0 0 0 0 0
 *                                    |
 *                                  endIdx
 *///写操作的是endIdx    | X 表示剩余size index 从左到右递减
/**
 * @brief 线性模式写入循环缓冲区
 * @param[in] cirbufCB 循环缓冲区控制块指针
 * @param[in] buf 待写入数据缓冲区
 * @param[in] size 待写入数据大小
 * @return 实际写入大小
 */
STATIC UINT32 OsCirBufWriteLinear(CirBuf *cirbufCB, const CHAR *buf, UINT32 size)
{
    UINT32 cpSize;                        // 实际拷贝大小
    errno_t err;                          // 错误码

    cpSize = (cirbufCB->remain < size) ? cirbufCB->remain : size;  // 计算可写入大小（取剩余空间与请求大小的较小值）

    if (cpSize == 0) {                    // 无空间可写
        return 0;                         // 返回0
    }

    err = memcpy_s(cirbufCB->fifo + cirbufCB->endIdx, cirbufCB->remain, buf, cpSize);  // 拷贝数据到缓冲区
    if (err != EOK) {                     // 拷贝失败
        return 0;                         // 返回0
    }

    cirbufCB->remain -= cpSize;           // 更新剩余空间
    cirbufCB->endIdx += cpSize;           // 更新结束索引

    return cpSize;                        // 返回实际写入大小
}

/* 图形表示写循环buf        loop 模式 ，图表示 写之前的样子              @note_pic
 *                    endIdx            第二阶段拷贝
 *                    |               |             |
 *    X X X X X X X X 0 0 0 0 0 0 0 0 X X X X X X X X
 *    |               |               |
 *     第一阶段拷贝                         startIdx
 *///写操作的是endIdx | X 表示剩余size index 从左到右递减
/**
 * @brief 循环模式写入循环缓冲区
 * @param[in] cirbufCB 循环缓冲区控制块指针
 * @param[in] buf 待写入数据缓冲区
 * @param[in] size 待写入数据大小
 * @return 实际写入大小
 */
STATIC UINT32 OsCirBufWriteLoop(CirBuf *cirbufCB, const CHAR *buf, UINT32 size)
{
    UINT32 right, cpSize;                 // right: 从endIdx到缓冲区末尾的空间；cpSize: 实际拷贝大小
    errno_t err;                          // 错误码

    right = cirbufCB->size - cirbufCB->endIdx;  // 计算endIdx到缓冲区末尾的空间
    cpSize = (right < size) ? right : size;     // 计算第一阶段可写入大小

    err = memcpy_s(cirbufCB->fifo + cirbufCB->endIdx, right, buf, cpSize);  // 第一阶段拷贝
    if (err != EOK) {                     // 拷贝失败
        return 0;                         // 返回0
    }

    cirbufCB->remain -= cpSize;           // 更新剩余空间
    cirbufCB->endIdx += cpSize;           // 更新结束索引
    if (cirbufCB->endIdx == cirbufCB->size) {  // 若已到缓冲区末尾
        cirbufCB->endIdx = 0;             // 重置endIdx为0
    }

    if (cpSize == size) {                 // 若已完成全部数据写入
        return size;                      // 返回请求大小
    } else {                              // 若仍有数据未写入
        cpSize += OsCirBufWriteLinear(cirbufCB, buf + cpSize, size - cpSize);  // 调用线性写入处理剩余数据
    }

    return cpSize;                        // 返回实际写入大小
}

/**
 * @brief 写入循环缓冲区（对外接口）
 * @param[in] cirbufCB 循环缓冲区控制块指针
 * @param[in] buf 待写入数据缓冲区
 * @param[in] size 待写入数据大小
 * @return 实际写入大小
 */
UINT32 LOS_CirBufWrite(CirBuf *cirbufCB, const CHAR *buf, UINT32 size)
{
    UINT32 cpSize = 0;                    // 实际写入大小
    UINT32 intSave;                       // 中断状态保存变量

    if ((cirbufCB == NULL) || (buf == NULL) || (size == 0) || (cirbufCB->status != CBUF_USED)) {  // 参数合法性检查
        return 0;                         // 参数无效，返回0
    }

    LOS_SpinLockSave(&cirbufCB->lock, &intSave);  // 获取自旋锁并保存中断状态

    if ((cirbufCB->fifo == NULL) || (cirbufCB->remain == 0))  {  // 缓冲区未初始化或无剩余空间
        goto EXIT;                        // 跳转到出口
    }

    if (cirbufCB->startIdx <= cirbufCB->endIdx) {  // 判断缓冲区状态，决定写入模式
        cpSize = OsCirBufWriteLoop(cirbufCB, buf, size);  // 循环模式写入
    } else {
        cpSize = OsCirBufWriteLinear(cirbufCB, buf, size);  // 线性模式写入
    }

EXIT:
    LOS_SpinUnlockRestore(&cirbufCB->lock, intSave);  // 恢复中断状态并释放自旋锁
    return cpSize;                        // 返回实际写入大小
}

/* 图形表示读线性buf        linear 模式 ，图表示 读之前的样子             @note_pic
 *                    endIdx            
 *                    |               
 *    X X X X X X X X 0 0 0 0 0 0 0 0 X X X X X X X X
 *                   				  |
 *                              	  startIdx
 *///读操作的是startIdx | X 表示剩余size index 从左到右递减
/**
 * @brief 线性模式读取循环缓冲区
 * @param[in] cirbufCB 循环缓冲区控制块指针
 * @param[out] buf 接收数据缓冲区
 * @param[in] size 请求读取大小
 * @return 实际读取大小
 */
STATIC UINT32 OsCirBufReadLinear(CirBuf *cirbufCB, CHAR *buf, UINT32 size)
{
    UINT32 cpSize, remain;                // remain: 可读取数据大小；cpSize: 实际读取大小
    errno_t err;                          // 错误码

    remain = cirbufCB->endIdx - cirbufCB->startIdx;  // 计算可读取数据大小
    cpSize = (remain < size) ? remain : size;        // 计算实际读取大小（取可读取大小与请求大小的较小值）

    if (cpSize == 0) {                    // 无可读取数据
        return 0;                         // 返回0
    }

    err = memcpy_s(buf, size, cirbufCB->fifo + cirbufCB->startIdx, cpSize);  // 拷贝数据到接收缓冲区
    if (err != EOK) {                     // 拷贝失败
        return 0;                         // 返回0
    }

    cirbufCB->remain += cpSize;           // 更新剩余空间
    cirbufCB->startIdx += cpSize;         // 更新起始索引

    return cpSize;                        // 返回实际读取大小
}

/* 图形表示读循环buf loop 模式 ，图表示 读之前的样子 @note_pic 
 *                    startIdx
 *                    |
 *    0 0 0 0 0 0 0 0 X X X X X X X X 0 0 0 0 0 0
 *                                    |
 *                                  endIdx
 *///读操作的是startIdx    | X 表示剩余size index 从左到右递减
/**
 * @brief 循环模式读取循环缓冲区
 * @param[in] cirbufCB 循环缓冲区控制块指针
 * @param[out] buf 接收数据缓冲区
 * @param[in] size 请求读取大小
 * @return 实际读取大小
 */
STATIC UINT32 OsCirBufReadLoop(CirBuf *cirbufCB, CHAR *buf, UINT32 size)
{
    UINT32 right, cpSize;                 // right: 从startIdx到缓冲区末尾的空间；cpSize: 实际读取大小
    errno_t err;                          // 错误码

    right = cirbufCB->size - cirbufCB->startIdx;  // 计算startIdx到缓冲区末尾的空间
    cpSize = (right < size) ? right : size;       // 计算第一阶段可读取大小

    err = memcpy_s(buf, size, cirbufCB->fifo + cirbufCB->startIdx, cpSize);  // 第一阶段拷贝
    if (err != EOK) {                     // 拷贝失败
        return 0;                         // 返回0
    }

    cirbufCB->remain += cpSize;           // 更新剩余空间
    cirbufCB->startIdx += cpSize;         // 更新起始索引
    if (cirbufCB->startIdx == cirbufCB->size) {  // 若已到缓冲区末尾
        cirbufCB->startIdx = 0;           // 重置startIdx为0
    }

    if (cpSize < size) {                  // 若仍有数据未读取
        cpSize += OsCirBufReadLinear(cirbufCB, buf + cpSize, size - cpSize);  // 调用线性读取处理剩余数据
    }

    return cpSize;                        // 返回实际读取大小
}

/**
 * @brief 读取循环缓冲区（对外接口）
 * @param[in] cirbufCB 循环缓冲区控制块指针
 * @param[out] buf 接收数据缓冲区
 * @param[in] size 请求读取大小
 * @return 实际读取大小
 */
UINT32 LOS_CirBufRead(CirBuf *cirbufCB, CHAR *buf, UINT32 size)
{
    UINT32 cpSize = 0;                    // 实际读取大小
    UINT32 intSave;                       // 中断状态保存变量

    if ((cirbufCB == NULL) || (buf == NULL) || (size == 0) || (cirbufCB->status != CBUF_USED)) {  // 参数合法性检查
        return 0;                         // 参数无效，返回0
    }

    LOS_SpinLockSave(&cirbufCB->lock, &intSave);  // 获取自旋锁并保存中断状态

    if ((cirbufCB->fifo == NULL) || (cirbufCB->remain == cirbufCB->size)) {  // 缓冲区未初始化或为空
        goto EXIT;                        // 跳转到出口
    }

    if (cirbufCB->startIdx >= cirbufCB->endIdx) {  // 判断缓冲区状态，决定读取模式
        cpSize = OsCirBufReadLoop(cirbufCB, buf, size);  // 循环模式读取
    } else {
        cpSize = OsCirBufReadLinear(cirbufCB, buf, size);  // 线性模式读取
    }

EXIT:
    LOS_SpinUnlockRestore(&cirbufCB->lock, intSave);  // 恢复中断状态并释放自旋锁
    return cpSize;                        // 返回实际读取大小
}

/**
 * @brief 初始化循环缓冲区
 * @param[in] cirbufCB 循环缓冲区控制块指针
 * @param[in] fifo 数据存储缓冲区指针
 * @param[in] size 缓冲区大小
 * @return 成功返回LOS_OK，失败返回LOS_NOK
 */
UINT32 LOS_CirBufInit(CirBuf *cirbufCB, CHAR *fifo, UINT32 size)
{
    if ((cirbufCB == NULL) || (fifo == NULL)) {  // 参数合法性检查
        return LOS_NOK;                   // 参数无效，返回失败
    }

    (VOID)memset_s(cirbufCB, sizeof(CirBuf), 0, sizeof(CirBuf));  // 初始化控制块
    LOS_SpinInit(&cirbufCB->lock);        // 初始化自旋锁
    cirbufCB->size = size;                // 设置缓冲区总大小
    cirbufCB->remain = size;              // 初始化剩余空间为总大小
    cirbufCB->status = CBUF_USED;         // 设置缓冲区状态为已使用
    cirbufCB->fifo = fifo;                // 设置数据存储缓冲区指针

    return LOS_OK;                        // 返回成功
}

/**
 * @brief 反初始化循环缓冲区
 * @param[in] cirbufCB 循环缓冲区控制块指针
 */
VOID LOS_CirBufDeinit(CirBuf *cirbufCB)
{
    (VOID)memset_s(cirbufCB, sizeof(CirBuf), 0, sizeof(CirBuf));  // 清空控制块
}
