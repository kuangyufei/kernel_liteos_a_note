/*!
 * @file    dmesg.c
 * @brief dmesg命令用于控制内核dmesg缓存区。
 * @link dmesg http://weharmonyos.com/openharmony/zh-cn/device-dev/kernel/kernel-small-debug-shell-cmd-dmesg.html 
 				https://man7.org/linux/man-pages/man1/dmesg.1.html
 * @endlink
   @verbatim
	  +-------------------------------------------------------+
	  | Info |			log_space							  |
	  +-------------------------------------------------------+
	  |
	  |__buffer_space
   
   Case A:
	  +-------------------------------------------------------+
	  | 		  |#############################|			  |
	  +-------------------------------------------------------+
				  | 							|
				 Head							Tail
   Case B:
	  +-------------------------------------------------------+
	  |##########|									  |#######|
	  +-------------------------------------------------------+
				 |									  |
				 Tail								  Head

   @endverbatim
 * @version 
 * @author  weharmonyos.com | 鸿蒙研究站 | 每天死磕一点点
 * @date    2021-11-25
 */
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

#ifdef LOSCFG_SHELL_DMESG
#include "dmesg_pri.h"
#include "show.h"
#include "shcmd.h"
#include "securec.h"
#include "stdlib.h"
#include "unistd.h"
#include "los_init.h"
#include "los_task.h"


#define BUF_MAX_INDEX (g_logBufSize - 1) ///< 缓存区最大索引,可按单个字符保存

LITE_OS_SEC_BSS STATIC SPIN_LOCK_INIT(g_dmesgSpin);

STATIC DmesgInfo *g_dmesgInfo = NULL;///< 保存在 g_mallocAddr 的开始位置,即头信息
STATIC UINT32 g_logBufSize = 0;	///< 缓冲区内容体大小
STATIC VOID *g_mallocAddr = NULL;///< 缓存区开始位置,即头位置
STATIC UINT32 g_dmesgLogLevel = 3;	///< 日志等级
STATIC UINT32 g_consoleLock = 0;	///< 用于关闭和打开控制台
STATIC UINT32 g_uartLock = 0;		///< 用于关闭和打开串口
STATIC const CHAR *g_levelString[] = {///< 日志等级
    "EMG",
    "COMMON",
    "ERR",
    "WARN",
    "INFO",
    "DEBUG"
};
/// 关闭控制台
STATIC VOID OsLockConsole(VOID)
{
    g_consoleLock = 1;
}
/// 打开控制台
STATIC VOID OsUnlockConsole(VOID)
{
    g_consoleLock = 0;
}
/// 关闭串口
STATIC VOID OsLockUart(VOID)
{
    g_uartLock = 1;
}
/// 打开串口
STATIC VOID OsUnlockUart(VOID)
{
    g_uartLock = 0;
}

STATIC UINT32 OsCheckError(VOID)
{
    if (g_dmesgInfo == NULL) {
        return LOS_NOK;
    }

    if (g_dmesgInfo->logSize > g_logBufSize) {
        return LOS_NOK;
    }

    if (((g_dmesgInfo->logSize == g_logBufSize) || (g_dmesgInfo->logSize == 0)) &&
        (g_dmesgInfo->logTail != g_dmesgInfo->logHead)) {
        return LOS_NOK;
    }

    return LOS_OK;
}
///< 读取dmesg日志
STATIC INT32 OsDmesgRead(CHAR *buf, UINT32 len)
{
    UINT32 readLen;
    UINT32 logSize = g_dmesgInfo->logSize;
    UINT32 head = g_dmesgInfo->logHead;
    UINT32 tail = g_dmesgInfo->logTail;
    CHAR *logBuf = g_dmesgInfo->logBuf;
    errno_t ret;

    if (OsCheckError()) {
        return -1;
    }
    if (logSize == 0) {
        return 0;
    }

    readLen = len < logSize ? len : logSize;

    if (head < tail) { /* Case A */
        ret = memcpy_s(buf, len, logBuf + head, readLen);
        if (ret != EOK) {
            return -1;
        }
        g_dmesgInfo->logHead += readLen;
        g_dmesgInfo->logSize -= readLen;
    } else { /* Case B */
        if (readLen <= (g_logBufSize - head)) {
            ret = memcpy_s(buf, len, logBuf + head, readLen);
            if (ret != EOK) {
                return -1;
            }
            g_dmesgInfo->logHead += readLen;
            g_dmesgInfo->logSize -= readLen;
        } else {
            ret = memcpy_s(buf, len, logBuf + head, g_logBufSize - head);
            if (ret != EOK) {
                return -1;
            }

            ret = memcpy_s(buf + g_logBufSize - head, len - (g_logBufSize - head),
                           logBuf, readLen - (g_logBufSize - head));
            if (ret != EOK) {
                return -1;
            }
            g_dmesgInfo->logHead = readLen - (g_logBufSize - head);
            g_dmesgInfo->logSize -= readLen;
        }
    }
    return (INT32)readLen;
}
/// 把旧人账目移交给新人
STATIC INT32 OsCopyToNew(const VOID *addr, UINT32 size)
{
    UINT32 copyStart = 0;
    UINT32 copyLen;
    CHAR *temp = NULL;
    CHAR *newBuf = (CHAR *)addr + sizeof(DmesgInfo);
    UINT32 bufSize = size - sizeof(DmesgInfo);
    INT32 ret;
    UINT32 intSave;

    LOS_SpinLockSave(&g_dmesgSpin, &intSave);
    if (g_dmesgInfo->logSize == 0) {
        LOS_SpinUnlockRestore(&g_dmesgSpin, intSave);
        return 0;
    }

    temp = (CHAR *)malloc(g_dmesgInfo->logSize);
    if (temp == NULL) {
        LOS_SpinUnlockRestore(&g_dmesgSpin, intSave);
        return -1;
    }

    (VOID)memset_s(temp, g_dmesgInfo->logSize, 0, g_dmesgInfo->logSize);
    copyLen = ((bufSize < g_dmesgInfo->logSize) ? bufSize : g_dmesgInfo->logSize);
    if (bufSize < g_dmesgInfo->logSize) {
        copyStart = g_dmesgInfo->logSize - bufSize;
    }

    ret = OsDmesgRead(temp, g_dmesgInfo->logSize);
    if (ret <= 0) {
        goto FREE_OUT;
    }

    /* if new buf size smaller than logSize */
    ret = memcpy_s(newBuf, bufSize, temp + copyStart, copyLen);
    if (ret != EOK) {
        goto FREE_OUT;
    }
    LOS_SpinUnlockRestore(&g_dmesgSpin, intSave);
    free(temp);

    return (INT32)copyLen;

FREE_OUT:
    LOS_SpinUnlockRestore(&g_dmesgSpin, intSave);
    PRINT_ERR("%s,%d failed, err:%d!\n", __FUNCTION__, __LINE__, ret);
    free(temp);
    return -1;
}
/// 重置内存
STATIC UINT32 OsDmesgResetMem(const VOID *addr, UINT32 size)
{
    VOID *temp = NULL;
    INT32 copyLen;
    UINT32 intSave;

    if (size <= sizeof(DmesgInfo)) {
        return LOS_NOK;
    }

    LOS_SpinLockSave(&g_dmesgSpin, &intSave);
    temp = g_dmesgInfo;
    LOS_SpinUnlockRestore(&g_dmesgSpin, intSave);
    copyLen = OsCopyToNew(addr, size);
    if (copyLen < 0) {
        return LOS_NOK;
    }

    LOS_SpinLockSave(&g_dmesgSpin, &intSave);
    g_logBufSize = size - sizeof(DmesgInfo);
    g_dmesgInfo = (DmesgInfo *)addr;
    g_dmesgInfo->logBuf = (CHAR *)addr + sizeof(DmesgInfo);
    g_dmesgInfo->logSize = copyLen;
    g_dmesgInfo->logTail = ((copyLen == g_logBufSize) ? 0 : copyLen);
    g_dmesgInfo->logHead = 0;

    /* if old mem came from malloc */
    if (temp == g_mallocAddr) {
        goto FREE_OUT;
    }
    LOS_SpinUnlockRestore(&g_dmesgSpin, intSave);

    return LOS_OK;

FREE_OUT:
    g_mallocAddr = NULL;
    LOS_SpinUnlockRestore(&g_dmesgSpin, intSave);
    free(temp);
    return LOS_OK;
}
///调整缓冲区大小,如下五个步骤
STATIC UINT32 OsDmesgChangeSize(UINT32 size)
{
    VOID *temp = NULL;
    INT32 copyLen;
    CHAR *newString = NULL;
    UINT32 intSave;

    if (size == 0) {
        return LOS_NOK;
    }
	//1. 重新整一块新地方
    newString = (CHAR *)malloc(size + sizeof(DmesgInfo));
    if (newString == NULL) {//新人未找到,旧人得接着用
        return LOS_NOK;
    }

    LOS_SpinLockSave(&g_dmesgSpin, &intSave);
    temp = g_dmesgInfo;
    LOS_SpinUnlockRestore(&g_dmesgSpin, intSave);
	//2.把旧人账目移交给新人
    copyLen = OsCopyToNew(newString, size + sizeof(DmesgInfo));
    if (copyLen < 0) {
        goto ERR_OUT;
    }
	//3.以新换旧
    LOS_SpinLockSave(&g_dmesgSpin, &intSave);
    g_logBufSize = size;
    g_dmesgInfo = (DmesgInfo *)newString;
    g_dmesgInfo->logBuf = (CHAR *)newString + sizeof(DmesgInfo);
    g_dmesgInfo->logSize = copyLen;
    g_dmesgInfo->logTail = ((copyLen == g_logBufSize) ? 0 : copyLen);
    g_dmesgInfo->logHead = 0;
	//4. 有新欢了,释放旧人去找寻真爱
    if (temp == g_mallocAddr) {
        goto FREE_OUT;
    }
    g_mallocAddr = newString;//5. 正式和新人媾和
    LOS_SpinUnlockRestore(&g_dmesgSpin, intSave);

    return LOS_OK;
ERR_OUT:
    free(newString);
    return LOS_NOK;
FREE_OUT:
    g_mallocAddr = newString;
    LOS_SpinUnlockRestore(&g_dmesgSpin, intSave);
    free(temp);
    return LOS_OK;
}

UINT32 OsCheckConsoleLock(VOID)
{
    return g_consoleLock;
}

UINT32 OsCheckUartLock(VOID)
{
    return g_uartLock;
}
/// 初始化 dmesg
UINT32 OsDmesgInit(VOID)
{
    CHAR* buffer = NULL;

    buffer = (CHAR *)malloc(KERNEL_LOG_BUF_SIZE + sizeof(DmesgInfo));//总内存分 头 + 体两部分
    if (buffer == NULL) {
        return LOS_NOK;
    }
    g_mallocAddr = buffer;
    g_dmesgInfo = (DmesgInfo *)buffer;//全局变量
    g_dmesgInfo->logHead = 0;//读取开始位置 记录在头部
    g_dmesgInfo->logTail = 0;//写入开始位置 记录在头部
    g_dmesgInfo->logSize = 0;//日志已占用数量 记录在头部
    g_dmesgInfo->logBuf = buffer + sizeof(DmesgInfo);//身体部分开始位置
    g_logBufSize = KERNEL_LOG_BUF_SIZE;//身体部分总大小位置

    return LOS_OK;
}
/// 只记录一个字符
STATIC CHAR OsLogRecordChar(CHAR c)
{
    *(g_dmesgInfo->logBuf + g_dmesgInfo->logTail++) = c;

    if (g_dmesgInfo->logTail > BUF_MAX_INDEX) {
        g_dmesgInfo->logTail = 0;
    }

    if (g_dmesgInfo->logSize < g_logBufSize) {
        (g_dmesgInfo->logSize)++;
    } else {
        g_dmesgInfo->logHead = g_dmesgInfo->logTail;
    }
    return c;
}
/// 记录一个字符串
UINT32 OsLogRecordStr(const CHAR *str, UINT32 len)
{
    UINT32 i = 0;
    UINTPTR intSave;

    LOS_SpinLockSave(&g_dmesgSpin, &intSave);
    while (len--) {
        (VOID)OsLogRecordChar(str[i]);
        i++;
    }
    LOS_SpinUnlockRestore(&g_dmesgSpin, intSave);
    return i;
}

STATIC VOID OsBufFullWrite(const CHAR *dst, UINT32 logLen)
{
    UINT32 bufSize = g_logBufSize;
    UINT32 tail = g_dmesgInfo->logTail;
    CHAR *buf = g_dmesgInfo->logBuf;
    errno_t ret;

    if (!logLen || (dst == NULL)) {
        return;
    }
    if (logLen > bufSize) { /* full re-write */
        ret = memcpy_s(buf + tail, bufSize - tail, dst, bufSize - tail);
        if (ret != EOK) {
            PRINT_ERR("%s,%d memcpy_s failed, err:%d!\n", __FUNCTION__, __LINE__, ret);
            return;
        }
        ret = memcpy_s(buf, bufSize, dst + bufSize - tail, tail);
        if (ret != EOK) {
            PRINT_ERR("%s,%d memcpy_s failed, err:%d!\n", __FUNCTION__, __LINE__, ret);
            return;
        }

        OsBufFullWrite(dst + bufSize, logLen - bufSize);
    } else {
        if (logLen > (bufSize - tail)) { /* need cycle back to start */
            ret = memcpy_s(buf + tail, bufSize - tail, dst, bufSize - tail);
            if (ret != EOK) {
                PRINT_ERR("%s,%d memcpy_s failed, err:%d!\n", __FUNCTION__, __LINE__, ret);
                return;
            }
            ret = memcpy_s(buf, bufSize, dst + bufSize - tail, logLen - (bufSize - tail));
            if (ret != EOK) {
                PRINT_ERR("%s,%d memcpy_s failed, err:%d!\n", __FUNCTION__, __LINE__, ret);
                return;
            }

            g_dmesgInfo->logTail = logLen - (bufSize - tail);
            g_dmesgInfo->logHead = g_dmesgInfo->logTail;
        } else { /* no need cycle back to start */
            ret = memcpy_s(buf + tail, bufSize - tail, dst, logLen);
            if (ret != EOK) {
                PRINT_ERR("%s,%d memcpy_s failed, err:%d!\n", __FUNCTION__, __LINE__, ret);
                return;
            }
            g_dmesgInfo->logTail += logLen;
            if (g_dmesgInfo->logTail > BUF_MAX_INDEX) {
                g_dmesgInfo->logTail = 0;
            }
            g_dmesgInfo->logHead = g_dmesgInfo->logTail;
        }
    }
}
/// 从头写入 
STATIC VOID OsWriteTailToHead(const CHAR *dst, UINT32 logLen)
{
    UINT32 writeLen;
    UINT32 bufSize = g_logBufSize;
    UINT32 logSize = g_dmesgInfo->logSize;
    UINT32 tail = g_dmesgInfo->logTail;
    CHAR *buf = g_dmesgInfo->logBuf;
    errno_t ret;

    if ((!logLen) || (dst == NULL)) {
        return;
    }
    if (logLen > (bufSize - logSize)) { /* space-need > space-remain */
        writeLen = bufSize - logSize;
        ret = memcpy_s(buf + tail, bufSize - tail, dst, writeLen);
        if (ret != EOK) {
            PRINT_ERR("%s,%d memcpy_s failed, err:%d!\n", __FUNCTION__, __LINE__, ret);
            return;
        }

        g_dmesgInfo->logTail = g_dmesgInfo->logHead;
        g_dmesgInfo->logSize = g_logBufSize;
        OsBufFullWrite(dst + writeLen, logLen - writeLen);
    } else {
        ret = memcpy_s(buf + tail, bufSize - tail, dst, logLen);
        if (ret != EOK) {
            PRINT_ERR("%s,%d memcpy_s failed, err:%d!\n", __FUNCTION__, __LINE__, ret);
            return;
        }

        g_dmesgInfo->logTail += logLen;
        g_dmesgInfo->logSize += logLen;
    }
}
/// 从尾写入 
STATIC VOID OsWriteTailToEnd(const CHAR *dst, UINT32 logLen)
{
    UINT32 writeLen;
    UINT32 bufSize = g_logBufSize;
    UINT32 tail = g_dmesgInfo->logTail;
    CHAR *buf = g_dmesgInfo->logBuf;
    errno_t ret;

    if ((!logLen) || (dst == NULL)) {
        return;
    }
    if (logLen >= (bufSize - tail)) { /* need cycle to start ,then became B */
        writeLen = bufSize - tail;
        ret = memcpy_s(buf + tail, writeLen, dst, writeLen);
        if (ret != EOK) {
            PRINT_ERR("%s,%d memcpy_s failed, err:%d!\n", __FUNCTION__, __LINE__, ret);
            return;
        }

        g_dmesgInfo->logSize += writeLen;
        g_dmesgInfo->logTail = 0;
        if (g_dmesgInfo->logSize == g_logBufSize) { /* Tail = Head is 0 */
            OsBufFullWrite(dst + writeLen, logLen - writeLen);
        } else {
            OsWriteTailToHead(dst + writeLen, logLen - writeLen);
        }
    } else { /* just do serial copy */
        ret = memcpy_s(buf + tail, bufSize - tail, dst, logLen);
        if (ret != EOK) {
            PRINT_ERR("%s,%d memcpy_s failed, err:%d!\n", __FUNCTION__, __LINE__, ret);
            return;
        }

        g_dmesgInfo->logTail += logLen;
        g_dmesgInfo->logSize += logLen;
    }
}
/// 内存拷贝日志
INT32 OsLogMemcpyRecord(const CHAR *buf, UINT32 logLen)
{
    UINT32 intSave;

    LOS_SpinLockSave(&g_dmesgSpin, &intSave);
    if (OsCheckError()) {
        LOS_SpinUnlockRestore(&g_dmesgSpin, intSave);
        return -1;
    }
    if (g_dmesgInfo->logSize < g_logBufSize) {
        if (g_dmesgInfo->logHead <= g_dmesgInfo->logTail) {
            OsWriteTailToEnd(buf, logLen);
        } else {
            OsWriteTailToHead(buf, logLen);
        }
    } else {
        OsBufFullWrite(buf, logLen);
    }
    LOS_SpinUnlockRestore(&g_dmesgSpin, intSave);

    return LOS_OK;
}
/// 使用串口打印日志
VOID OsLogShow(VOID)
{
    UINT32 intSave;
    UINT32 index;
    UINT32 i = 0;
    CHAR *p = NULL;

    LOS_SpinLockSave(&g_dmesgSpin, &intSave);
    index = g_dmesgInfo->logHead;

    p = (CHAR *)malloc(g_dmesgInfo->logSize + 1);
    if (p == NULL) {
        LOS_SpinUnlockRestore(&g_dmesgSpin, intSave);
        return;
    }
    (VOID)memset_s(p, g_dmesgInfo->logSize + 1, 0, g_dmesgInfo->logSize + 1);

    while (i < g_dmesgInfo->logSize) {//一个一个字符拷贝
        *(p + i) = *(g_dmesgInfo->logBuf + index++);
        if (index > BUF_MAX_INDEX) {//循环buf,读到尾了得从头开始读
            index = 0;
        }
        i++;
        if (index == g_dmesgInfo->logTail) {//一直读到写入位置,才退出
            break;
        }
    }
    LOS_SpinUnlockRestore(&g_dmesgSpin, intSave);
    UartPuts(p, i, UART_WITH_LOCK);//串口输出
    free(p);//释放内存
}
/// 设置日志层级
STATIC INT32 OsDmesgLvSet(const CHAR *level)
{
    UINT32 levelNum, ret;
    CHAR *p = NULL;

    levelNum = strtoul(level, &p, 0);
    if (*p != 0) {
        PRINTK("dmesg: invalid option or parameter.\n");
        return -1;
    }

    ret = LOS_DmesgLvSet(levelNum);
    if (ret == LOS_OK) {
        PRINTK("Set current dmesg log level %s\n", g_levelString[g_dmesgLogLevel]);
        return LOS_OK;
    } else {
        PRINTK("current dmesg log level %s\n", g_levelString[g_dmesgLogLevel]);
        PRINTK("dmesg -l [num] can access as 0:EMG 1:COMMON 2:ERROR 3:WARN 4:INFO 5:DEBUG\n");
        return -1;
    }
}

STATIC INT32 OsDmesgMemSizeSet(const CHAR *size)
{
    UINT32 sizeVal;
    CHAR *p = NULL;

    sizeVal = strtoul(size, &p, 0);
    if (sizeVal > MAX_KERNEL_LOG_BUF_SIZE) {
        goto ERR_OUT;
    }

    if (!(LOS_DmesgMemSet(NULL, sizeVal))) {
        PRINTK("Set dmesg buf size %u success\n", sizeVal);
        return LOS_OK;
    } else {
        goto ERR_OUT;
    }

ERR_OUT:
    PRINTK("Set dmesg buf size %u fail\n", sizeVal);
    return LOS_NOK;
}
UINT32 OsDmesgLvGet(VOID)
{
    return g_dmesgLogLevel;
}

UINT32 LOS_DmesgLvSet(UINT32 level)
{
    if (level > 5) { /* 5: count of level */
        return LOS_NOK;
    }

    g_dmesgLogLevel = level;
    return LOS_OK;
}

VOID LOS_DmesgClear(VOID)
{
    UINT32 intSave;

    LOS_SpinLockSave(&g_dmesgSpin, &intSave);
    (VOID)memset_s(g_dmesgInfo->logBuf, g_logBufSize, 0, g_logBufSize);
    g_dmesgInfo->logHead = 0;
    g_dmesgInfo->logTail = 0;
    g_dmesgInfo->logSize = 0;
    LOS_SpinUnlockRestore(&g_dmesgSpin, intSave);
}
/// 设置dmesg缓存大小
UINT32 LOS_DmesgMemSet(const VOID *addr, UINT32 size)
{
    UINT32 ret = 0;

    if (addr == NULL) {
        ret = OsDmesgChangeSize(size);
    } else {
        ret = OsDmesgResetMem(addr, size);
    }
    return ret;
}
/// 读取 dmesg 消息
INT32 LOS_DmesgRead(CHAR *buf, UINT32 len)
{
    INT32 ret;
    UINT32 intSave;

    if (buf == NULL) {
        return -1;
    }
    if (len == 0) {
        return 0;
    }

    LOS_SpinLockSave(&g_dmesgSpin, &intSave);
    ret = OsDmesgRead(buf, len);
    LOS_SpinUnlockRestore(&g_dmesgSpin, intSave);
    return ret;
}

INT32 OsDmesgWrite2File(const CHAR *fullpath, const CHAR *buf, UINT32 logSize)
{
    INT32 ret;

    const CHAR *prefix = "/temp/";
    INT32 prefixLen = strlen(prefix);
    if (strncmp(fullpath, prefix, prefixLen) != 0) {
        return -1;
    }
    INT32 fd = open(fullpath, O_CREAT | O_RDWR | O_APPEND, 0644); /* 0644:file right */
    if (fd < 0) {
        return -1;
    }
    ret = write(fd, buf, logSize);
    (VOID)close(fd);
    return ret;
}

#ifdef LOSCFG_FS_VFS
/// 将dmesg 保存到文件中
INT32 LOS_DmesgToFile(const CHAR *filename)
{
    CHAR *fullpath = NULL;
    CHAR *buf = NULL;
    INT32 ret;
    CHAR *shellWorkingDirectory = OsShellGetWorkingDirectory();//获取工作路径
    UINT32 logSize, bufSize, head, tail, intSave;
    CHAR *logBuf = NULL;

    LOS_SpinLockSave(&g_dmesgSpin, &intSave);
    if (OsCheckError()) {
        LOS_SpinUnlockRestore(&g_dmesgSpin, intSave);
        return -1;
    }
    logSize = g_dmesgInfo->logSize;
    bufSize = g_logBufSize;
    head = g_dmesgInfo->logHead;
    tail = g_dmesgInfo->logTail;
    logBuf = g_dmesgInfo->logBuf;
    LOS_SpinUnlockRestore(&g_dmesgSpin, intSave);

    ret = vfs_normalize_path(shellWorkingDirectory, filename, &fullpath);//获取绝对路径
    if (ret != 0) {
        return -1;
    }

    buf = (CHAR *)malloc(logSize);
    if (buf == NULL) {
        goto ERR_OUT2;
    }

    if (head < tail) {
        ret = memcpy_s(buf, logSize, logBuf + head, logSize);
        if (ret != EOK) {
            goto ERR_OUT3;
        }
    } else {
        ret = memcpy_s(buf, logSize, logBuf + head, bufSize - head);
        if (ret != EOK) {
            goto ERR_OUT3;
        }
        ret = memcpy_s(buf + bufSize - head, logSize - (bufSize - head), logBuf, tail);
        if (ret != EOK) {
            goto ERR_OUT3;
        }
    }

    ret = OsDmesgWrite2File(fullpath, buf, logSize);//写文件
ERR_OUT3:
    free(buf);
ERR_OUT2:
    free(fullpath);
    return ret;
}
#else
INT32 LOS_DmesgToFile(CHAR *filename)
{
    (VOID)filename;
    PRINTK("File operation need VFS\n");
    return -1;
}
#endif

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
INT32 OsShellCmdDmesg(INT32 argc, const CHAR **argv)
{
    if (argc == 1) {
        PRINTK("\n");
        OsLogShow();
        return LOS_OK;
    } else if (argc == 2) { /* 2: count of parameters */
        if (argv == NULL) {
            goto ERR_OUT;
        }

        if (!strcmp(argv[1], "-c")) {//打印缓存区内容并清空缓存区
            PRINTK("\n");
            OsLogShow();//打印缓存区内容
            LOS_DmesgClear();
            return LOS_OK;
        } else if (!strcmp(argv[1], "-C")) {//清空缓存区。
            LOS_DmesgClear();
            return LOS_OK;
        } else if (!strcmp(argv[1], "-D")) {//关闭控制台打印。
            OsLockConsole();
            return LOS_OK;
        } else if (!strcmp(argv[1], "-E")) {///开启控制台打印。
            OsUnlockConsole();
            return LOS_OK;
        } else if (!strcmp(argv[1], "-L")) {//关闭串口打印
            OsLockUart();
            return LOS_OK;
        } else if (!strcmp(argv[1], "-U")) {//开启串口打印
            OsUnlockUart();
            return LOS_OK;
        }
    } else if (argc == 3) { /* 3: count of parameters */
        if (argv == NULL) {
            goto ERR_OUT;
        }

        if (!strcmp(argv[1], ">")) {//将缓存区内容写入文件
            if (LOS_DmesgToFile((CHAR *)argv[2]) < 0) { /* 2:index of parameters */
                PRINTK("Dmesg write log to %s fail \n", argv[2]); /* 2:index of parameters */
                return -1;
            } else {
                PRINTK("Dmesg write log to %s success \n", argv[2]); /* 2:index of parameters */
                return LOS_OK;
            }
        } else if (!strcmp(argv[1], "-l")) {//设置缓存等级
            return OsDmesgLvSet(argv[2]); /* 2:index of parameters */
        } else if (!strcmp(argv[1], "-s")) {//设置缓存区大小 size是要设置的大小
            return OsDmesgMemSizeSet(argv[2]); /* 2:index of parameters */
        }
    }

ERR_OUT:
    PRINTK("dmesg: invalid option or parameter.\n");
    return -1;
}

SHELLCMD_ENTRY(dmesg_shellcmd, CMD_TYPE_STD, "dmesg", XARGS, (CmdCallBackFunc)OsShellCmdDmesg);
/*
将扩展如下:
CmdItem dmesg_shellcmd __attribute__((section(".liteos.table.shellcmd.data"))) 
__attribute__((used)) = {CMD_TYPE_STD,"dmesg",XARGS,OsShellCmdDmesg}
*/
LOS_MODULE_INIT(OsDmesgInit, LOS_INIT_LEVEL_EARLIEST);//在非常早期调用

#endif
