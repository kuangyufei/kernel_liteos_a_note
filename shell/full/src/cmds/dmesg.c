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

/*
   +-------------------------------------------------------+
   | Info |          log_space                             |
   +-------------------------------------------------------+
   |
   |__buffer_space

Case A:
   +-------------------------------------------------------+
   |           |#############################|             |
   +-------------------------------------------------------+
               |                             |
              Head                           Tail
Case B:
   +-------------------------------------------------------+
   |##########|                                    |#######|
   +-------------------------------------------------------+
              |                                    |
              Tail                                 Head
*/

#ifdef LOSCFG_SHELL_DMESG
#include "dmesg_pri.h"
#include "show.h"
#include "shcmd.h"
#include "securec.h"
#include "stdlib.h"
#include "unistd.h"
#include "los_init.h"
#include "los_task.h"

// 定义缓冲区最大索引，为日志缓冲区大小减1
#define BUF_MAX_INDEX (g_logBufSize - 1)

// 定义并初始化dmesg自旋锁，用于保护日志操作的线程安全
LITE_OS_SEC_BSS STATIC SPIN_LOCK_INIT(g_dmesgSpin);

// dmesg信息结构体指针，用于管理日志缓冲区元数据
STATIC DmesgInfo *g_dmesgInfo = NULL;
// 日志缓冲区大小
STATIC UINT32 g_logBufSize = 0;
// 动态分配的内存地址
STATIC VOID *g_mallocAddr = NULL;
// dmesg日志级别，默认为3(WARN)
STATIC UINT32 g_dmesgLogLevel = 3;
// 控制台锁定标志，1表示锁定，0表示未锁定
STATIC UINT32 g_consoleLock = 0;
// UART锁定标志，1表示锁定，0表示未锁定
STATIC UINT32 g_uartLock = 0;
// 日志级别字符串数组，与日志级别数值对应
STATIC const CHAR *g_levelString[] = {
    "EMG",    // 0: 紧急信息
    "COMMON", // 1: 普通信息
    "ERR",    // 2: 错误信息
    "WARN",   // 3: 警告信息
    "INFO",   // 4: 提示信息
    "DEBUG"   // 5: 调试信息
};

/**
 * @brief 锁定控制台输出
 * @details 禁止控制台输出日志信息
 * @param 无
 * @return 无
 */
STATIC VOID OsLockConsole(VOID)
{
    g_consoleLock = 1;  // 设置控制台锁定标志为1
}

/**
 * @brief 解锁控制台输出
 * @details 允许控制台输出日志信息
 * @param 无
 * @return 无
 */
STATIC VOID OsUnlockConsole(VOID)
{
    g_consoleLock = 0;  // 设置控制台锁定标志为0
}

/**
 * @brief 锁定UART输出
 * @details 禁止UART输出日志信息
 * @param 无
 * @return 无
 */
STATIC VOID OsLockUart(VOID)
{
    g_uartLock = 1;  // 设置UART锁定标志为1
}

/**
 * @brief 解锁UART输出
 * @details 允许UART输出日志信息
 * @param 无
 * @return 无
 */
STATIC VOID OsUnlockUart(VOID)
{
    g_uartLock = 0;  // 设置UART锁定标志为0
}

/**
 * @brief 检查dmesg模块错误状态
 * @details 验证dmesg信息结构体和日志缓冲区状态的有效性
 * @param 无
 * @return 成功返回LOS_OK，失败返回LOS_NOK
 */
STATIC UINT32 OsCheckError(VOID)
{
    if (g_dmesgInfo == NULL) {  // 检查dmesg信息结构体是否为空
        return LOS_NOK;
    }

    if (g_dmesgInfo->logSize > g_logBufSize) {  // 检查日志大小是否超过缓冲区容量
        return LOS_NOK;
    }

    // 检查缓冲区满或空时，头尾指针是否一致
    if (((g_dmesgInfo->logSize == g_logBufSize) || (g_dmesgInfo->logSize == 0)) &&
        (g_dmesgInfo->logTail != g_dmesgInfo->logHead)) {
        return LOS_NOK;
    }

    return LOS_OK;  // 状态正常返回LOS_OK
}

/**
 * @brief 从日志缓冲区读取数据
 * @details 根据缓冲区头尾指针位置，分情况读取指定长度的数据
 * @param buf 输出缓冲区指针
 * @param len 要读取的数据长度
 * @return 成功返回实际读取的字节数，失败返回-1
 */
STATIC INT32 OsDmesgRead(CHAR *buf, UINT32 len)
{
    UINT32 readLen;                  // 实际读取长度
    UINT32 logSize = g_dmesgInfo->logSize;  // 当前日志数据大小
    UINT32 head = g_dmesgInfo->logHead;    // 缓冲区头指针
    UINT32 tail = g_dmesgInfo->logTail;    // 缓冲区尾指针
    CHAR *logBuf = g_dmesgInfo->logBuf;    // 日志缓冲区指针
    errno_t ret;                     // 函数调用返回值

    if (OsCheckError()) {  // 检查dmesg模块状态是否正常
        return -1;
    }
    if (logSize == 0) {    // 日志缓冲区为空
        return 0;
    }

    // 确定实际读取长度，不超过请求长度和日志实际大小
    readLen = len < logSize ? len : logSize;

    if (head < tail) { /* Case A: 头指针在尾指针前面，缓冲区数据连续 */
        ret = memcpy_s(buf, len, logBuf + head, readLen);  // 拷贝数据
        if (ret != EOK) {  // 拷贝失败
            return -1;
        }
        g_dmesgInfo->logHead += readLen;  // 更新头指针
        g_dmesgInfo->logSize -= readLen;  // 更新日志大小
    } else { /* Case B: 头指针在尾指针后面或相等，缓冲区数据循环 */
        if (readLen <= (g_logBufSize - head)) {  // 读取长度小于等于从头指针到缓冲区末尾的空间
            ret = memcpy_s(buf, len, logBuf + head, readLen);  // 拷贝数据
            if (ret != EOK) {  // 拷贝失败
                return -1;
            }
            g_dmesgInfo->logHead += readLen;  // 更新头指针
            g_dmesgInfo->logSize -= readLen;  // 更新日志大小
        } else {  // 读取长度超过从头指针到缓冲区末尾的空间，需要分两段拷贝
            // 第一段：从头指针到缓冲区末尾
            ret = memcpy_s(buf, len, logBuf + head, g_logBufSize - head);
            if (ret != EOK) {  // 拷贝失败
                return -1;
            }

            // 第二段：从缓冲区开头到所需长度
            ret = memcpy_s(buf + g_logBufSize - head, len - (g_logBufSize - head),
                           logBuf, readLen - (g_logBufSize - head));
            if (ret != EOK) {  // 拷贝失败
                return -1;
            }
            // 更新头指针到第二段数据的末尾
            g_dmesgInfo->logHead = readLen - (g_logBufSize - head);
            g_dmesgInfo->logSize -= readLen;  // 更新日志大小
        }
    }
    return (INT32)readLen;  // 返回实际读取长度
}

/**
 * @brief 将日志数据拷贝到新缓冲区
 * @details 用于缓冲区大小调整时，将旧缓冲区数据迁移到新缓冲区
 * @param addr 新缓冲区地址
 * @param size 新缓冲区总大小（包含DmesgInfo结构体）
 * @return 成功返回拷贝的字节数，失败返回-1
 */
STATIC INT32 OsCopyToNew(const VOID *addr, UINT32 size)
{
    UINT32 copyStart = 0;  // 拷贝起始偏移量
    UINT32 copyLen;        // 拷贝长度
    CHAR *temp = NULL;     // 临时缓冲区指针
    // 新缓冲区的数据区域起始地址（跳过DmesgInfo结构体）
    CHAR *newBuf = (CHAR *)addr + sizeof(DmesgInfo);
    UINT32 bufSize = size - sizeof(DmesgInfo);  // 新缓冲区数据区域大小
    INT32 ret;             // 函数调用返回值
    UINT32 intSave;        // 中断状态保存变量

    LOS_SpinLockSave(&g_dmesgSpin, &intSave);  // 加锁保护临界区
    if (g_dmesgInfo->logSize == 0) {  // 日志缓冲区为空，无需拷贝
        LOS_SpinUnlockRestore(&g_dmesgSpin, intSave);  // 解锁
        return 0;
    }

    // 分配临时缓冲区，用于存储旧日志数据
    temp = (CHAR *)malloc(g_dmesgInfo->logSize);
    if (temp == NULL) {  // 内存分配失败
        LOS_SpinUnlockRestore(&g_dmesgSpin, intSave);  // 解锁
        return -1;
    }

    // 初始化临时缓冲区
    (VOID)memset_s(temp, g_dmesgInfo->logSize, 0, g_dmesgInfo->logSize);
    // 确定拷贝长度，不超过新缓冲区大小和旧日志大小
    copyLen = ((bufSize < g_dmesgInfo->logSize) ? bufSize : g_dmesgInfo->logSize);
    if (bufSize < g_dmesgInfo->logSize) {  // 新缓冲区小于旧日志大小，只拷贝尾部数据
        copyStart = g_dmesgInfo->logSize - bufSize;
    }

    // 从旧缓冲区读取数据到临时缓冲区
    ret = OsDmesgRead(temp, g_dmesgInfo->logSize);
    if (ret <= 0) {  // 读取失败
        goto FREE_OUT;  // 跳转到释放资源
    }

    /* 如果新缓冲区大小小于日志大小 */
    ret = memcpy_s(newBuf, bufSize, temp + copyStart, copyLen);  // 拷贝数据到新缓冲区
    if (ret != EOK) {  // 拷贝失败
        goto FREE_OUT;  // 跳转到释放资源
    }
    LOS_SpinUnlockRestore(&g_dmesgSpin, intSave);  // 解锁
    free(temp);  // 释放临时缓冲区

    return (INT32)copyLen;  // 返回拷贝长度

FREE_OUT:  // 错误处理标签
    LOS_SpinUnlockRestore(&g_dmesgSpin, intSave);  // 解锁
    PRINT_ERR("%s,%d failed, err:%d!\n", __FUNCTION__, __LINE__, ret);  // 打印错误信息
    free(temp);  // 释放临时缓冲区
    return -1;  // 返回错误
}

/**
 * @brief 重置dmesg内存缓冲区
 * @details 使用指定地址的内存作为新的dmesg缓冲区，并迁移旧数据
 * @param addr 新缓冲区地址
 * @param size 新缓冲区总大小
 * @return 成功返回LOS_OK，失败返回LOS_NOK
 */
STATIC UINT32 OsDmesgResetMem(const VOID *addr, UINT32 size)
{
    VOID *temp = NULL;     // 旧dmesgInfo指针
    INT32 copyLen;         // 拷贝数据长度
    UINT32 intSave;        // 中断状态保存变量

    if (size <= sizeof(DmesgInfo)) {  // 缓冲区大小不足以容纳DmesgInfo结构体
        return LOS_NOK;
    }

    LOS_SpinLockSave(&g_dmesgSpin, &intSave);  // 加锁保护临界区
    temp = g_dmesgInfo;  // 保存旧dmesgInfo指针
    LOS_SpinUnlockRestore(&g_dmesgSpin, intSave);  // 解锁
    copyLen = OsCopyToNew(addr, size);  // 将旧数据拷贝到新缓冲区
    if (copyLen < 0) {  // 拷贝失败
        return LOS_NOK;
    }

    LOS_SpinLockSave(&g_dmesgSpin, &intSave);  // 加锁保护临界区
    g_logBufSize = size - sizeof(DmesgInfo);  // 更新日志缓冲区大小
    g_dmesgInfo = (DmesgInfo *)addr;  // 更新dmesgInfo指针为新地址
    g_dmesgInfo->logBuf = (CHAR *)addr + sizeof(DmesgInfo);  // 设置新日志缓冲区指针
    g_dmesgInfo->logSize = copyLen;  // 设置日志大小为拷贝长度
    // 设置尾指针：如果拷贝长度等于缓冲区大小则为0，否则为拷贝长度
    g_dmesgInfo->logTail = ((copyLen == g_logBufSize) ? 0 : copyLen);
    g_dmesgInfo->logHead = 0;  // 头指针置0

    /* 如果旧内存是动态分配的 */
    if (temp == g_mallocAddr) {
        goto FREE_OUT;  // 跳转到释放旧内存
    }
    LOS_SpinUnlockRestore(&g_dmesgSpin, intSave);  // 解锁

    return LOS_OK;  // 返回成功

FREE_OUT:  // 释放旧内存标签
    g_mallocAddr = NULL;  // 重置动态分配地址
    LOS_SpinUnlockRestore(&g_dmesgSpin, intSave);  // 解锁
    free(temp);  // 释放旧内存
    return LOS_OK;  // 返回成功
}

/**
 * @brief 调整dmesg日志缓冲区大小
 * @details 动态分配新的内存空间作为日志缓冲区，并迁移旧数据
 * @param size 新的日志缓冲区数据区域大小
 * @return 成功返回LOS_OK，失败返回LOS_NOK
 */
STATIC UINT32 OsDmesgChangeSize(UINT32 size)
{
    VOID *temp = NULL;     // 旧dmesgInfo指针
    INT32 copyLen;         // 拷贝数据长度
    CHAR *newString = NULL;  // 新分配的内存指针
    UINT32 intSave;        // 中断状态保存变量

    if (size == 0) {  // 新大小不能为0
        return LOS_NOK;
    }

    // 分配新内存，大小为日志缓冲区大小加上DmesgInfo结构体大小
    newString = (CHAR *)malloc(size + sizeof(DmesgInfo));
    if (newString == NULL) {  // 内存分配失败
        return LOS_NOK;
    }

    LOS_SpinLockSave(&g_dmesgSpin, &intSave);  // 加锁保护临界区
    temp = g_dmesgInfo;  // 保存旧dmesgInfo指针
    LOS_SpinUnlockRestore(&g_dmesgSpin, intSave);  // 解锁

    // 将旧数据拷贝到新缓冲区
    copyLen = OsCopyToNew(newString, size + sizeof(DmesgInfo));
    if (copyLen < 0) {  // 拷贝失败
        goto ERR_OUT;  // 跳转到错误处理
    }

    LOS_SpinLockSave(&g_dmesgSpin, &intSave);  // 加锁保护临界区
    g_logBufSize = size;  // 更新日志缓冲区大小
    g_dmesgInfo = (DmesgInfo *)newString;  // 更新dmesgInfo指针
    g_dmesgInfo->logBuf = (CHAR *)newString + sizeof(DmesgInfo);  // 设置日志缓冲区指针
    g_dmesgInfo->logSize = copyLen;  // 设置日志大小
    // 设置尾指针：如果拷贝长度等于缓冲区大小则为0，否则为拷贝长度
    g_dmesgInfo->logTail = ((copyLen == g_logBufSize) ? 0 : copyLen);
    g_dmesgInfo->logHead = 0;  // 头指针置0

    if (temp == g_mallocAddr) {  // 如果旧内存是动态分配的
        goto FREE_OUT;  // 跳转到释放旧内存
    }
    g_mallocAddr = newString;  // 更新动态分配地址为新内存
    LOS_SpinUnlockRestore(&g_dmesgSpin, intSave);  // 解锁

    return LOS_OK;  // 返回成功

ERR_OUT:  // 错误处理标签
    free(newString);  // 释放新分配的内存
    return LOS_NOK;  // 返回失败
FREE_OUT:  // 释放旧内存标签
    g_mallocAddr = newString;  // 更新动态分配地址为新内存
    LOS_SpinUnlockRestore(&g_dmesgSpin, intSave);  // 解锁
    free(temp);  // 释放旧内存
    return LOS_OK;  // 返回成功
}

/**
 * @brief 检查控制台锁定状态
 * @details 获取当前控制台是否被锁定
 * @param 无
 * @return 1表示锁定，0表示未锁定
 */
UINT32 OsCheckConsoleLock(VOID)
{
    return g_consoleLock;  // 返回控制台锁定标志
}

/**
 * @brief 检查UART锁定状态
 * @details 获取当前UART是否被锁定
 * @param 无
 * @return 1表示锁定，0表示未锁定
 */
UINT32 OsCheckUartLock(VOID)
{
    return g_uartLock;  // 返回UART锁定标志
}

/**
 * @brief 初始化dmesg模块
 * @details 分配并初始化dmesg日志缓冲区和相关元数据
 * @param 无
 * @return 成功返回LOS_OK，失败返回LOS_NOK
 */
UINT32 OsDmesgInit(VOID)
{
    CHAR* buffer = NULL;  // 缓冲区指针

    // 分配内存：内核日志缓冲区大小 + DmesgInfo结构体大小
    buffer = (CHAR *)malloc(KERNEL_LOG_BUF_SIZE + sizeof(DmesgInfo));
    if (buffer == NULL) {  // 内存分配失败
        return LOS_NOK;
    }
    g_mallocAddr = buffer;  // 保存动态分配地址
    g_dmesgInfo = (DmesgInfo *)buffer;  // 设置dmesgInfo指针
    g_dmesgInfo->logHead = 0;  // 头指针初始化为0
    g_dmesgInfo->logTail = 0;  // 尾指针初始化为0
    g_dmesgInfo->logSize = 0;  // 日志大小初始化为0
    // 设置日志缓冲区指针（跳过DmesgInfo结构体）
    g_dmesgInfo->logBuf = buffer + sizeof(DmesgInfo);
    g_logBufSize = KERNEL_LOG_BUF_SIZE;  // 设置日志缓冲区大小

    return LOS_OK;  // 返回成功
}

/**
 * @brief 记录单个字符到日志缓冲区
 * @details 将字符写入循环日志缓冲区，并更新相关指针和大小
 * @param c 要记录的字符
 * @return 返回记录的字符
 */
STATIC CHAR OsLogRecordChar(CHAR c)
{
    // 将字符写入尾指针位置，并移动尾指针
    *(g_dmesgInfo->logBuf + g_dmesgInfo->logTail++) = c;

    if (g_dmesgInfo->logTail > BUF_MAX_INDEX) {  // 尾指针超出缓冲区范围，循环到开头
        g_dmesgInfo->logTail = 0;
    }

    if (g_dmesgInfo->logSize < g_logBufSize) {  // 缓冲区未满
        (g_dmesgInfo->logSize)++;  // 增加日志大小
    } else {  // 缓冲区已满，覆盖旧数据
        g_dmesgInfo->logHead = g_dmesgInfo->logTail;  // 头指针跟随尾指针移动
    }
    return c;  // 返回记录的字符
}

/**
 * @brief 记录字符串到日志缓冲区
 * @details 加锁保护下，将字符串逐个字符写入日志缓冲区
 * @param str 要记录的字符串指针
 * @param len 要记录的字符串长度
 * @return 返回实际记录的字符数
 */
UINT32 OsLogRecordStr(const CHAR *str, UINT32 len)
{
    UINT32 i = 0;  // 字符索引
    UINTPTR intSave;  // 中断状态保存变量

    LOS_SpinLockSave(&g_dmesgSpin, &intSave);  // 加锁保护临界区
    while (len--) {  // 循环写入每个字符
        (VOID)OsLogRecordChar(str[i]);
        i++;
    }
    LOS_SpinUnlockRestore(&g_dmesgSpin, intSave);  // 解锁
    return i;  // 返回实际记录的字符数
}

/**
 * @brief 缓冲区满时写入数据
 * @details 当日志缓冲区已满时，覆盖旧数据写入新数据
 * @param dst 要写入的数据指针
 * @param logLen 要写入的数据长度
 * @return 无
 */
STATIC VOID OsBufFullWrite(const CHAR *dst, UINT32 logLen)
{
    UINT32 bufSize = g_logBufSize;  // 缓冲区大小
    UINT32 tail = g_dmesgInfo->logTail;  // 当前尾指针
    CHAR *buf = g_dmesgInfo->logBuf;  // 日志缓冲区指针
    errno_t ret;  // 函数调用返回值

    if (!logLen || (dst == NULL)) {  // 无效参数检查
        return;
    }
    if (logLen > bufSize) { /* 写入长度大于缓冲区大小，完全覆盖 */
        // 从尾指针位置写到缓冲区末尾
        ret = memcpy_s(buf + tail, bufSize - tail, dst, bufSize - tail);
        if (ret != EOK) {  // 拷贝失败
            PRINT_ERR("%s,%d memcpy_s failed, err:%d!\n", __FUNCTION__, __LINE__, ret);
            return;
        }
        // 从缓冲区开头写到尾指针位置，完成循环覆盖
        ret = memcpy_s(buf, bufSize, dst + bufSize - tail, tail);
        if (ret != EOK) {  // 拷贝失败
            PRINT_ERR("%s,%d memcpy_s failed, err:%d!\n", __FUNCTION__, __LINE__, ret);
            return;
        }

        // 递归处理剩余数据
        OsBufFullWrite(dst + bufSize, logLen - bufSize);
    } else {
        if (logLen > (bufSize - tail)) { /* 写入长度超过尾指针到缓冲区末尾的空间，需要循环 */
            // 从尾指针位置写到缓冲区末尾
            ret = memcpy_s(buf + tail, bufSize - tail, dst, bufSize - tail);
            if (ret != EOK) {  // 拷贝失败
                PRINT_ERR("%s,%d memcpy_s failed, err:%d!\n", __FUNCTION__, __LINE__, ret);
                return;
            }
            // 从缓冲区开头写入剩余数据
            ret = memcpy_s(buf, bufSize, dst + bufSize - tail, logLen - (bufSize - tail));
            if (ret != EOK) {  // 拷贝失败
                PRINT_ERR("%s,%d memcpy_s failed, err:%d!\n", __FUNCTION__, __LINE__, ret);
                return;
            }

            // 更新尾指针到新位置
            g_dmesgInfo->logTail = logLen - (bufSize - tail);
            g_dmesgInfo->logHead = g_dmesgInfo->logTail;  // 头指针跟随尾指针
        } else { /* 无需循环，直接写入 */
            ret = memcpy_s(buf + tail, bufSize - tail, dst, logLen);  // 拷贝数据
            if (ret != EOK) {  // 拷贝失败
                PRINT_ERR("%s,%d memcpy_s failed, err:%d!\n", __FUNCTION__, __LINE__, ret);
                return;
            }
            g_dmesgInfo->logTail += logLen;  // 更新尾指针
            if (g_dmesgInfo->logTail > BUF_MAX_INDEX) {  // 尾指针越界处理
                g_dmesgInfo->logTail = 0;
            }
            g_dmesgInfo->logHead = g_dmesgInfo->logTail;  // 头指针跟随尾指针
        }
    }
}

/**
 * @brief 从尾指针写到头指针位置
 * @details 当日志缓冲区非空且头指针在尾指针后面时写入数据
 * @param dst 要写入的数据指针
 * @param logLen 要写入的数据长度
 * @return 无
 */
STATIC VOID OsWriteTailToHead(const CHAR *dst, UINT32 logLen)
{
    UINT32 writeLen;       // 实际写入长度
    UINT32 bufSize = g_logBufSize;  // 缓冲区大小
    UINT32 logSize = g_dmesgInfo->logSize;  // 当前日志大小
    UINT32 tail = g_dmesgInfo->logTail;    // 当前尾指针
    CHAR *buf = g_dmesgInfo->logBuf;    // 日志缓冲区指针
    errno_t ret;           // 函数调用返回值

    if ((!logLen) || (dst == NULL)) {  // 无效参数检查
        return;
    }
    if (logLen > (bufSize - logSize)) { /* 需要写入的长度大于剩余空间 */
        writeLen = bufSize - logSize;  // 计算可写入的最大长度
        // 从尾指针位置写入数据
        ret = memcpy_s(buf + tail, bufSize - tail, dst, writeLen);
        if (ret != EOK) {  // 拷贝失败
            PRINT_ERR("%s,%d memcpy_s failed, err:%d!\n", __FUNCTION__, __LINE__, ret);
            return;
        }

        g_dmesgInfo->logTail = g_dmesgInfo->logHead;  // 尾指针移动到头指针位置
        g_dmesgInfo->logSize = g_logBufSize;  // 日志大小等于缓冲区大小（已满）
        // 处理剩余数据，此时缓冲区已满
        OsBufFullWrite(dst + writeLen, logLen - writeLen);
    } else {  // 剩余空间足够写入
        ret = memcpy_s(buf + tail, bufSize - tail, dst, logLen);  // 拷贝数据
        if (ret != EOK) {  // 拷贝失败
            PRINT_ERR("%s,%d memcpy_s failed, err:%d!\n", __FUNCTION__, __LINE__, ret);
            return;
        }

        g_dmesgInfo->logTail += logLen;  // 更新尾指针
        g_dmesgInfo->logSize += logLen;  // 更新日志大小
    }
}

/**
 * @brief 从尾指针写到缓冲区末尾
 * @details 当日志缓冲区非空且头指针在尾指针前面时写入数据
 * @param dst 要写入的数据指针
 * @param logLen 要写入的数据长度
 * @return 无
 */
STATIC VOID OsWriteTailToEnd(const CHAR *dst, UINT32 logLen)
{
    UINT32 writeLen;       // 实际写入长度
    UINT32 bufSize = g_logBufSize;  // 缓冲区大小
    UINT32 tail = g_dmesgInfo->logTail;    // 当前尾指针
    CHAR *buf = g_dmesgInfo->logBuf;    // 日志缓冲区指针
    errno_t ret;           // 函数调用返回值

    if ((!logLen) || (dst == NULL)) {  // 无效参数检查
        return;
    }
    if (logLen >= (bufSize - tail)) { /* 写入长度大于等于尾指针到缓冲区末尾的空间，需要循环 */
        writeLen = bufSize - tail;  // 计算从尾指针到缓冲区末尾的长度
        ret = memcpy_s(buf + tail, writeLen, dst, writeLen);  // 拷贝数据
        if (ret != EOK) {  // 拷贝失败
            PRINT_ERR("%s,%d memcpy_s failed, err:%d!\n", __FUNCTION__, __LINE__, ret);
            return;
        }

        g_dmesgInfo->logSize += writeLen;  // 更新日志大小
        g_dmesgInfo->logTail = 0;  // 尾指针循环到缓冲区开头
        if (g_dmesgInfo->logSize == g_logBufSize) { /* 日志大小等于缓冲区大小（已满） */
            // 处理剩余数据，缓冲区已满
            OsBufFullWrite(dst + writeLen, logLen - writeLen);
        } else {  // 缓冲区未满，继续写入
            OsWriteTailToHead(dst + writeLen, logLen - writeLen);
        }
    } else { /* 无需循环，直接写入 */
        ret = memcpy_s(buf + tail, bufSize - tail, dst, logLen);  // 拷贝数据
        if (ret != EOK) {  // 拷贝失败
            PRINT_ERR("%s,%d memcpy_s failed, err:%d!\n", __FUNCTION__, __LINE__, ret);
            return;
        }

        g_dmesgInfo->logTail += logLen;  // 更新尾指针
        g_dmesgInfo->logSize += logLen;  // 更新日志大小
    }
}

/**
 * @brief 将数据记录到日志缓冲区
 * @details 根据当前缓冲区状态，选择合适的写入策略
 * @param buf 要记录的数据指针
 * @param logLen 要记录的数据长度
 * @return 成功返回LOS_OK，失败返回-1
 */
INT32 OsLogMemcpyRecord(const CHAR *buf, UINT32 logLen)
{
    UINT32 intSave;  // 中断状态保存变量

    LOS_SpinLockSave(&g_dmesgSpin, &intSave);  // 加锁保护临界区
    if (OsCheckError()) {  // 检查dmesg模块状态
        LOS_SpinUnlockRestore(&g_dmesgSpin, intSave);  // 解锁
        return -1;
    }
    if (g_dmesgInfo->logSize < g_logBufSize) {  // 缓冲区未满
        if (g_dmesgInfo->logHead <= g_dmesgInfo->logTail) {  // 头指针在尾指针前面或相等
            OsWriteTailToEnd(buf, logLen);  // 从尾指针写到缓冲区末尾
        } else {  // 头指针在尾指针后面
            OsWriteTailToHead(buf, logLen);  // 从尾指针写到头指针位置
        }
    } else {  // 缓冲区已满
        OsBufFullWrite(buf, logLen);  // 缓冲区满时写入策略
    }
    LOS_SpinUnlockRestore(&g_dmesgSpin, intSave);  // 解锁

    return LOS_OK;  // 返回成功
}

/**
 * @brief 显示日志内容
 * @details 将日志缓冲区中的内容读取并输出到UART
 * @param 无
 * @return 无
 */
VOID OsLogShow(VOID)
{
    UINT32 intSave;  // 中断状态保存变量
    UINT32 index;    // 当前读取索引
    UINT32 i = 0;    // 输出缓冲区索引
    CHAR *p = NULL;  // 临时输出缓冲区指针

    LOS_SpinLockSave(&g_dmesgSpin, &intSave);  // 加锁保护临界区
    index = g_dmesgInfo->logHead;  // 从日志头指针开始读取

    // 分配临时缓冲区，大小为日志大小+1（用于字符串结束符）
    p = (CHAR *)malloc(g_dmesgInfo->logSize + 1);
    if (p == NULL) {  // 内存分配失败
        LOS_SpinUnlockRestore(&g_dmesgSpin, intSave);  // 解锁
        return;
    }
    // 初始化临时缓冲区
    (VOID)memset_s(p, g_dmesgInfo->logSize + 1, 0, g_dmesgInfo->logSize + 1);

    // 循环读取日志数据到临时缓冲区
    while (i < g_dmesgInfo->logSize) {
        *(p + i) = *(g_dmesgInfo->logBuf + index++);  // 读取一个字符
        if (index > BUF_MAX_INDEX) {  // 索引超出缓冲区范围，循环到开头
            index = 0;
        }
        i++;
        if (index == g_dmesgInfo->logTail) {  // 到达尾指针，停止读取
            break;
        }
    }
    LOS_SpinUnlockRestore(&g_dmesgSpin, intSave);  // 解锁
    UartPuts(p, i, UART_WITH_LOCK);  // 通过UART输出日志内容
    free(p);  // 释放临时缓冲区
}

/**
 * @brief 设置dmesg日志级别
 * @details 将字符串形式的级别转换为数值并设置
 * @param level 日志级别字符串
 * @return 成功返回LOS_OK，失败返回-1
 */
STATIC INT32 OsDmesgLvSet(const CHAR *level)
{
    UINT32 levelNum, ret;  // 级别数值和函数返回值
    CHAR *p = NULL;        // 字符串转换剩余部分指针

    // 将字符串转换为无符号整数
    levelNum = strtoul(level, &p, 0);
    if (*p != 0) {  // 转换失败，存在非数字字符
        PRINTK("dmesg: invalid option or parameter.\n");
        return -1;
    }

    ret = LOS_DmesgLvSet(levelNum);  // 设置日志级别
    if (ret == LOS_OK) {  // 设置成功
        PRINTK("Set current dmesg log level %s\n", g_levelString[g_dmesgLogLevel]);
        return LOS_OK;
    } else {  // 设置失败
        PRINTK("current dmesg log level %s\n", g_levelString[g_dmesgLogLevel]);
        PRINTK("dmesg -l [num] can access as 0:EMG 1:COMMON 2:ERROR 3:WARN 4:INFO 5:DEBUG\n");
        return -1;
    }
}

/**
 * @brief 设置dmesg缓冲区大小
 * @details 将字符串形式的大小转换为数值并设置
 * @param size 缓冲区大小字符串
 * @return 成功返回LOS_OK，失败返回LOS_NOK
 */
STATIC INT32 OsDmesgMemSizeSet(const CHAR *size)
{
    UINT32 sizeVal;  // 大小数值
    CHAR *p = NULL;  // 字符串转换剩余部分指针

    // 将字符串转换为无符号整数
    sizeVal = strtoul(size, &p, 0);
    if (sizeVal > MAX_KERNEL_LOG_BUF_SIZE) {  // 超过最大允许大小
        goto ERR_OUT;  // 跳转到错误处理
    }

    if (!(LOS_DmesgMemSet(NULL, sizeVal))) {  // 设置缓冲区大小
        PRINTK("Set dmesg buf size %u success\n", sizeVal);
        return LOS_OK;
    } else {
        goto ERR_OUT;  // 跳转到错误处理
    }

ERR_OUT:  // 错误处理标签
    PRINTK("Set dmesg buf size %u fail\n", sizeVal);
    return LOS_NOK;
}
/**
 * @brief 获取当前dmesg日志级别
 * @details 返回全局日志级别变量的值
 * @param 无
 * @return 当前日志级别
 */
UINT32 OsDmesgLvGet(VOID)
{
    return g_dmesgLogLevel;  // 返回当前日志级别
}

/**
 * @brief 设置dmesg日志级别
 * @details 验证级别有效性并设置全局日志级别变量
 * @param level 要设置的日志级别（0-5）
 * @return 成功返回LOS_OK，失败返回LOS_NOK
 */
UINT32 LOS_DmesgLvSet(UINT32 level)
{
    if (level > 5) { /* 5: 日志级别总数-1，级别必须在0-5范围内 */
        return LOS_NOK;
    }

    g_dmesgLogLevel = level;  // 设置全局日志级别变量
    return LOS_OK;  // 返回成功
}

/**
 * @brief 清除dmesg日志缓冲区
 * @details 清空日志缓冲区内容并重置相关指针
 * @param 无
 * @return 无
 */
VOID LOS_DmesgClear(VOID)
{
    UINT32 intSave;  // 中断状态保存变量

    LOS_SpinLockSave(&g_dmesgSpin, &intSave);  // 加锁保护临界区
    // 清空日志缓冲区
    (VOID)memset_s(g_dmesgInfo->logBuf, g_logBufSize, 0, g_logBufSize);
    g_dmesgInfo->logHead = 0;  // 头指针置0
    g_dmesgInfo->logTail = 0;  // 尾指针置0
    g_dmesgInfo->logSize = 0;  // 日志大小置0
    LOS_SpinUnlockRestore(&g_dmesgSpin, intSave);  // 解锁
}

/**
 * @brief 设置dmesg内存缓冲区
 * @details 根据地址是否为空，选择重置内存或调整大小
 * @param addr 新缓冲区地址，NULL表示动态分配
 * @param size 新缓冲区大小
 * @return 成功返回0，失败返回非0
 */
UINT32 LOS_DmesgMemSet(const VOID *addr, UINT32 size)
{
    UINT32 ret = 0;  // 返回值

    if (addr == NULL) {  // 地址为空，动态调整大小
        ret = OsDmesgChangeSize(size);
    } else {  // 地址非空，使用指定地址作为缓冲区
        ret = OsDmesgResetMem(addr, size);
    }
    return ret;  // 返回操作结果
}

/**
 * @brief 读取dmesg日志数据
 * @details 加锁保护下从日志缓冲区读取数据
 * @param buf 输出缓冲区指针
 * @param len 要读取的长度
 * @return 成功返回实际读取长度，失败返回-1
 */
INT32 LOS_DmesgRead(CHAR *buf, UINT32 len)
{
    INT32 ret;       // 读取结果
    UINT32 intSave;  // 中断状态保存变量

    if (buf == NULL) {  // 输出缓冲区为空
        return -1;
    }
    if (len == 0) {  // 读取长度为0
        return 0;
    }

    LOS_SpinLockSave(&g_dmesgSpin, &intSave);  // 加锁保护临界区
    ret = OsDmesgRead(buf, len);  // 调用内部读取函数
    LOS_SpinUnlockRestore(&g_dmesgSpin, intSave);  // 解锁
    return ret;  // 返回读取结果
}

/**
 * @brief 将日志数据写入文件
 * @details 仅允许写入/temp/目录下的文件
 * @param fullpath 文件完整路径
 * @param buf 日志数据缓冲区
 * @param logSize 日志数据大小
 * @return 成功返回写入字节数，失败返回-1
 */
INT32 OsDmesgWrite2File(const CHAR *fullpath, const CHAR *buf, UINT32 logSize)
{
    INT32 ret;  // 函数返回值

    const CHAR *prefix = "/temp/";  // 允许写入的目录前缀
    INT32 prefixLen = strlen(prefix);  // 前缀长度
    if (strncmp(fullpath, prefix, prefixLen) != 0) {  // 检查路径前缀
        return -1;
    }

    // 打开文件：创建、读写、追加模式，权限0644
    INT32 fd = open(fullpath, O_CREAT | O_RDWR | O_APPEND, 0644); /* 0644:文件权限 */
    if (fd < 0) {  // 打开文件失败
        return -1;
    }
    ret = write(fd, buf, logSize);  // 写入文件
    (VOID)close(fd);  // 关闭文件
    return ret;  // 返回写入字节数
}

#ifdef LOSCFG_FS_VFS  // 如果启用了VFS文件系统
/**
 * @brief 将dmesg日志保存到文件
 * @details 读取日志缓冲区内容并写入指定文件
 * @param filename 目标文件名
 * @return 成功返回写入字节数，失败返回-1
 */
INT32 LOS_DmesgToFile(const CHAR *filename)
{
    CHAR *fullpath = NULL;  // 规范化后的完整路径
    CHAR *buf = NULL;       // 临时缓冲区
    INT32 ret;              // 函数返回值
    // 获取shell工作目录
    CHAR *shellWorkingDirectory = OsShellGetWorkingDirectory();
    UINT32 logSize, bufSize, head, tail, intSave;  // 日志相关参数
    CHAR *logBuf = NULL;    // 日志缓冲区指针

    LOS_SpinLockSave(&g_dmesgSpin, &intSave);  // 加锁保护临界区
    if (OsCheckError()) {  // 检查dmesg模块状态
        LOS_SpinUnlockRestore(&g_dmesgSpin, intSave);  // 解锁
        return -1;
    }
    logSize = g_dmesgInfo->logSize;  // 获取日志大小
    bufSize = g_logBufSize;          // 获取缓冲区大小
    head = g_dmesgInfo->logHead;     // 获取头指针
    tail = g_dmesgInfo->logTail;     // 获取尾指针
    logBuf = g_dmesgInfo->logBuf;    // 获取日志缓冲区指针
    LOS_SpinUnlockRestore(&g_dmesgSpin, intSave);  // 解锁

    // 规范化文件路径
    ret = vfs_normalize_path(shellWorkingDirectory, filename, &fullpath);
    if (ret != 0) {  // 路径规范化失败
        return -1;
    }

    buf = (CHAR *)malloc(logSize);  // 分配临时缓冲区
    if (buf == NULL) {  // 内存分配失败
        goto ERR_OUT2;  // 跳转到释放路径内存
    }

    if (head < tail) {  // 头指针在尾指针前面，数据连续
        ret = memcpy_s(buf, logSize, logBuf + head, logSize);  // 拷贝数据
        if (ret != EOK) {  // 拷贝失败
            goto ERR_OUT3;  // 跳转到释放临时缓冲区
        }
    } else {  // 头指针在尾指针后面，数据循环
        // 拷贝第一段：从头指针到缓冲区末尾
        ret = memcpy_s(buf, logSize, logBuf + head, bufSize - head);
        if (ret != EOK) {  // 拷贝失败
            goto ERR_OUT3;  // 跳转到释放临时缓冲区
        }
        // 拷贝第二段：从缓冲区开头到尾指针
        ret = memcpy_s(buf + bufSize - head, logSize - (bufSize - head), logBuf, tail);
        if (ret != EOK) {  // 拷贝失败
            goto ERR_OUT3;  // 跳转到释放临时缓冲区
        }
    }

    ret = OsDmesgWrite2File(fullpath, buf, logSize);  // 写入文件
ERR_OUT3:  // 释放临时缓冲区标签
    free(buf);
ERR_OUT2:  // 释放路径内存标签
    free(fullpath);
    return ret;  // 返回写入结果
}
#else  // 未启用VFS文件系统
/**
 * @brief 未启用VFS时的存盘函数
 * @details 提示需要VFS支持
 * @param filename 目标文件名
 * @return 始终返回-1
 */
INT32 LOS_DmesgToFile(CHAR *filename)
{
    (VOID)filename;  // 未使用参数
    PRINTK("File operation need VFS\n");  // 提示需要VFS
    return -1;  // 返回失败
}
#endif  // LOSCFG_FS_VFS

/**
 * @brief 
dmesg全称是display message (or display driver)，即显示信息。

dmesg命令用于控制内核dmesg缓存区
dmesg命令用于显示开机信息
该命令依赖于LOSCFG_SHELL_DMESG，使用时通过menuconfig在配置项中开启"Enable Shell dmesg"：

Debug ---> Enable a Debug Version ---> Enable Shell ---> Enable Shell dmesg

dmesg参数缺省时，默认打印缓存区内容。

各“ - ”选项不能混合使用。

写入文件需确保已挂载文件系统。
关闭串口打印会影响shell使用，建议先连接telnet再尝试关闭串口。

dmesg > /usr/dmesg.log。
 * @param argc 
 * @param argv 
 * @return INT32 
 */

/**
 * @brief dmesg命令处理函数
 * @details 解析命令参数并执行相应操作：显示日志、清除日志、设置级别等
 * @param argc 参数数量
 * @param argv 参数数组
 * @return 成功返回LOS_OK，失败返回-1
 */
INT32 OsShellCmdDmesg(INT32 argc, const CHAR **argv)
{
    if (argc == 1) {  // 无参数，显示日志
        PRINTK("\n");
        OsLogShow();
        return LOS_OK;
    } else if (argc == 2) { /* 2: 参数数量，单个选项 */
        if (argv == NULL) {  // 参数数组为空
            goto ERR_OUT;  // 跳转到错误处理
        }

        if (!strcmp(argv[1], "-c")) {  // 显示并清除日志
            PRINTK("\n");
            OsLogShow();
            LOS_DmesgClear();
            return LOS_OK;
        } else if (!strcmp(argv[1], "-C")) {  // 仅清除日志
            LOS_DmesgClear();
            return LOS_OK;
        } else if (!strcmp(argv[1], "-D")) {  // 锁定控制台
            OsLockConsole();
            return LOS_OK;
        } else if (!strcmp(argv[1], "-E")) {  // 解锁控制台
            OsUnlockConsole();
            return LOS_OK;
        } else if (!strcmp(argv[1], "-L")) {  // 锁定UART
            OsLockUart();
            return LOS_OK;
        } else if (!strcmp(argv[1], "-U")) {  // 解锁UART
            OsUnlockUart();
            return LOS_OK;
        }
    } else if (argc == 3) { /* 3: 参数数量，选项+参数 */
        if (argv == NULL) {  // 参数数组为空
            goto ERR_OUT;  // 跳转到错误处理
        }

        if (!strcmp(argv[1], ">")) {  // 重定向到文件
            if (LOS_DmesgToFile((CHAR *)argv[2]) < 0) { /* 2:参数索引，保存日志到文件 */
                PRINTK("Dmesg write log to %s fail \n", argv[2]); /* 2:参数索引，打印失败信息 */
                return -1;
            } else {
                PRINTK("Dmesg write log to %s success \n", argv[2]); /* 2:参数索引，打印成功信息 */
                return LOS_OK;
            }
        } else if (!strcmp(argv[1], "-l")) {  // 设置日志级别
            return OsDmesgLvSet(argv[2]); /* 2:参数索引，调用级别设置函数 */
        } else if (!strcmp(argv[1], "-s")) {  // 设置缓冲区大小
            return OsDmesgMemSizeSet(argv[2]); /* 2:参数索引，调用大小设置函数 */
        }
    }

ERR_OUT:  // 错误处理标签
    PRINTK("dmesg: invalid option or parameter.\n");  // 打印错误信息
    return -1;  // 返回失败
}

SHELLCMD_ENTRY(dmesg_shellcmd, CMD_TYPE_STD, "dmesg", XARGS, (CmdCallBackFunc)OsShellCmdDmesg);
LOS_MODULE_INIT(OsDmesgInit, LOS_INIT_LEVEL_EARLIEST);

#endif
