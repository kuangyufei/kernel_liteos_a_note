/*
 * Copyright (c) 2013-2019, Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020, Huawei Device Co., Ltd. All rights reserved.
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

#include "sys_config.h"
#ifdef LOSCFG_SHELL_DMESG
#include "dmesg_pri.h"
#include "show.h"
#include "shcmd.h"
#include "securec.h"
#include "unistd.h"
#include "stdlib.h"
#include "los_task.h"
#include "hisoc/uart.h"
#include "inode/inode.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

#define BUF_MAX_INDEX (g_logBufSize - 1)

LITE_OS_SEC_BSS STATIC SPIN_LOCK_INIT(g_dmesgSpin);//dmesg自旋锁初始化

STATIC DmesgInfo *g_dmesgInfo = NULL;	//缓存区描述符	
STATIC UINT32 g_logBufSize = 0;		//缓存buf大小,默认8K
STATIC VOID *g_mallocAddr = NULL;	//dmesg buf地址
STATIC UINT32 g_dmesgLogLevel = 3;	//默认日志等级 LOS_ERR_LEVEL = 3
STATIC UINT32 g_consoleLock = 0;	//控制台锁
STATIC UINT32 g_uartLock = 0;		//串口锁
STATIC const CHAR *g_levelString[] = {//缓存等级
    "EMG",
    "COMMON",
    "ERR",
    "WARN",
    "INFO",
    "DEBUG"
};

STATIC VOID OsLockConsole(VOID)
{
    g_consoleLock = 1;
}

STATIC VOID OsUnlockConsole(VOID)
{
    g_consoleLock = 0;
}

STATIC VOID OsLockUart(VOID)
{
    g_uartLock = 1;
}

STATIC VOID OsUnlockUart(VOID)
{
    g_uartLock = 0;
}

STATIC UINT32 OsCheckError(VOID)//检查缓冲区是否异常
{
    if (g_dmesgInfo == NULL) {
        return LOS_NOK;
    }

    if (g_dmesgInfo->logSize > g_logBufSize) {//超出缓冲区大小了
        return LOS_NOK;
    }

    if (((g_dmesgInfo->logSize == g_logBufSize) || (g_dmesgInfo->logSize == 0)) &&
        (g_dmesgInfo->logTail != g_dmesgInfo->logHead)) {
        return LOS_NOK;
    }

    return LOS_OK;
}
//读取日志缓冲区数据,buf带走数据
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

    readLen = len < logSize ? len : logSize;//最长只能读走g_dmesgInfo->logSize

    if (head < tail) { /* Case A */ //不用循环的情况 head 在 tail前
        ret = memcpy_s(buf, len, logBuf + head, readLen);//从head的位置一次性直接copy走
        if (ret != EOK) {
            return -1;
        }
        g_dmesgInfo->logHead += readLen;//head位置往前挪
        g_dmesgInfo->logSize -= readLen;//可读缓冲区大小减少
    } else { /* Case B */ 
        if (readLen <= (g_logBufSize - head)) {//可以读取连续的buf 
            ret = memcpy_s(buf, len, logBuf + head, readLen);//可以一次性读走
            if (ret != EOK) {
                return -1;
            }
            g_dmesgInfo->logHead += readLen;//head位置往前挪
            g_dmesgInfo->logSize -= readLen;//可读缓冲区大小减少
        } else {//这种情况需分两次读
            ret = memcpy_s(buf, len, logBuf + head, g_logBufSize - head);//先读到buf尾部
            if (ret != EOK) {
                return -1;
            }

            ret = memcpy_s(buf + g_logBufSize - head, len - (g_logBufSize - head),
                           logBuf, readLen - (g_logBufSize - head));//再从buf头开始读
            if (ret != EOK) {
                return -1;
            }
            g_dmesgInfo->logHead = readLen - (g_logBufSize - head);//重新
            g_dmesgInfo->logSize -= readLen;//可读缓冲区大小减少
        }
    }
    return (INT32)readLen;
}

STATIC INT32 OsCopyToNew(const VOID *addr, UINT32 size)
{
    UINT32 copyStart = 0;
    UINT32 copyLen;
    CHAR *temp = NULL;
    CHAR *newBuf = (CHAR *)addr + sizeof(DmesgInfo);
    UINT32 bufSize = size - sizeof(DmesgInfo);
    INT32 ret;

    if (g_dmesgInfo->logSize == 0) {
        return 0;
    }

    temp = (CHAR *)malloc(g_dmesgInfo->logSize);
    if (temp == NULL) {
        return -1;
    }

    (VOID)memset_s(temp, g_dmesgInfo->logSize, 0, g_dmesgInfo->logSize);
    copyLen = ((bufSize < g_dmesgInfo->logSize) ? bufSize : g_dmesgInfo->logSize);
    if (bufSize < g_dmesgInfo->logSize) {
        copyStart = g_dmesgInfo->logSize - bufSize;
    }

    ret = OsDmesgRead(temp, g_dmesgInfo->logSize);
    if (ret <= 0) {
        PRINT_ERR("%s,%d failed, err:%d!\n", __FUNCTION__, __LINE__, ret);
        free(temp);
        return -1;
    }

    /* if new buf size smaller than logSize */
    ret = memcpy_s(newBuf, bufSize, temp + copyStart, copyLen);
    if (ret != EOK) {
        PRINT_ERR("%s,%d memcpy_s failed, err:%d!\n", __FUNCTION__, __LINE__, ret);
        free(temp);
        return -1;
    }
    free(temp);

    return (INT32)copyLen;
}

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
    copyLen = OsCopyToNew(addr, size);
    if (copyLen < 0) {
        LOS_SpinUnlockRestore(&g_dmesgSpin, intSave);
        return LOS_NOK;
    }

    g_logBufSize = size - sizeof(DmesgInfo);
    g_dmesgInfo = (DmesgInfo *)addr;
    g_dmesgInfo->logBuf = (CHAR *)addr + sizeof(DmesgInfo);
    g_dmesgInfo->logSize = copyLen;
    g_dmesgInfo->logTail = ((copyLen == g_logBufSize) ? 0 : copyLen);
    g_dmesgInfo->logHead = 0;

    /* if old mem came from malloc */
    if (temp == g_mallocAddr) {
        g_mallocAddr = NULL;
        free(temp);
    }
    LOS_SpinUnlockRestore(&g_dmesgSpin, intSave);

    return LOS_OK;
}

STATIC UINT32 OsDmesgChangeSize(UINT32 size)
{
    VOID *temp = NULL;
    INT32 copyLen;
    CHAR *newString = NULL;
    UINT32 intSave;

    if (size == 0) {
        return LOS_NOK;
    }

    newString = (CHAR *)malloc(size + sizeof(DmesgInfo));
    if (newString == NULL) {
        return LOS_NOK;
    }

    LOS_SpinLockSave(&g_dmesgSpin, &intSave);
    temp = g_dmesgInfo;

    copyLen = OsCopyToNew(newString, size + sizeof(DmesgInfo));
    if (copyLen < 0) {
        LOS_SpinUnlockRestore(&g_dmesgSpin, intSave);
        free(newString);
        return LOS_NOK;
    }

    g_logBufSize = size;
    g_dmesgInfo = (DmesgInfo *)newString;
    g_dmesgInfo->logBuf = (CHAR *)newString + sizeof(DmesgInfo);
    g_dmesgInfo->logSize = copyLen;
    g_dmesgInfo->logTail = ((copyLen == g_logBufSize) ? 0 : copyLen);
    g_dmesgInfo->logHead = 0;

    if (temp == g_mallocAddr) {
        g_mallocAddr = NULL;
        free(temp);
    }
    g_mallocAddr = newString;
    LOS_SpinUnlockRestore(&g_dmesgSpin, intSave);

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
//初始化内核dmesg缓存区,即内核为日志开辟的一个8K缓冲区,dmesg的管理是一个buf的循环管理.
UINT32 OsDmesgInit(VOID)
{
    CHAR* buffer = NULL;

    buffer = (CHAR *)malloc(KERNEL_LOG_BUF_SIZE + sizeof(DmesgInfo));//注意这里用的是malloc分配,但鸿蒙做了适配调用了LOS_MemAlloc
    if (buffer == NULL) {
        return LOS_NOK;
    }
    g_mallocAddr = buffer;//@note_what 取名g_mallocAddr 用于记录日志缓冲区感觉怪怪的
    g_dmesgInfo = (DmesgInfo *)buffer;//整个buf的开头位置用于放置DmesgInfo
    g_dmesgInfo->logHead = 0;//头部日志索引位置,主要用于读操作
    g_dmesgInfo->logTail = 0;//尾部日志索引位置,主要用于写操作
    g_dmesgInfo->logSize = 0;//日志占用buf大小
    g_dmesgInfo->logBuf = buffer + sizeof(DmesgInfo);//日志内容紧跟在DmesgInfo之后
    g_logBufSize = KERNEL_LOG_BUF_SIZE;//buf大小 8K

    return LOS_OK;
}
//记录一个字符, 实现说明即使buf中写满了,还会继续写入,覆盖最早的数据.
STATIC CHAR OsLogRecordChar(CHAR c)
{
    *(g_dmesgInfo->logBuf + g_dmesgInfo->logTail++) = c;//将字符写在尾部位置 
	//注者更喜欢这样的写法 *g_dmesgInfo->logBuf[g_dmesgInfo->logTail++] = c;
    if (g_dmesgInfo->logTail > BUF_MAX_INDEX) {//写到尾了
        g_dmesgInfo->logTail = 0;//下次需从头开始了
    }

    if (g_dmesgInfo->logSize < g_logBufSize) {//如果可读buf大小未超总大小
        (g_dmesgInfo->logSize)++;//可读的buf大小 ++
    } else {//整个buf被写满了
        g_dmesgInfo->logHead = g_dmesgInfo->logTail;//头尾相等,读写索引在同一位置
    }
    return c;
}
//记录字符串
UINT32 OsLogRecordStr(const CHAR *str, UINT32 len)
{
    UINT32 i = 0;
    UINTPTR intSave;

    LOS_SpinLockSave(&g_dmesgSpin, &intSave);
    while (len--) {
        (VOID)OsLogRecordChar(str[i]);//一个一个字符写入
        i++;
    }
    LOS_SpinUnlockRestore(&g_dmesgSpin, intSave);
    return i;
}
//以覆盖的方式写buf,FULL的意思是 g_dmesgInfo->logHead = g_dmesgInfo->logTail;
STATIC VOID OsBufFullWrite(const CHAR *dst, UINT32 logLen)
{
    UINT32 bufSize = g_logBufSize;
    UINT32 tail = g_dmesgInfo->logTail;
    CHAR *buf = g_dmesgInfo->logBuf;
    errno_t ret;

    if (!logLen || (dst == NULL)) {
        return;
    }
    if (logLen > bufSize) { /* full re-write */ //日志长度大于缓存大小的时候怎么写? 答案是:一直覆盖
        ret = memcpy_s(buf + tail, bufSize - tail, dst, bufSize - tail);//先从tail位置写到buf末尾
        if (ret != EOK) {
            PRINT_ERR("%s,%d memcpy_s failed, err:%d!\n", __FUNCTION__, __LINE__, ret);
            return;
        }
        ret = memcpy_s(buf, bufSize, dst + bufSize - tail, tail);//再从buf头部写到tail位置
        if (ret != EOK) {
            PRINT_ERR("%s,%d memcpy_s failed, err:%d!\n", __FUNCTION__, __LINE__, ret);
            return;
        }

        OsBufFullWrite(dst + bufSize, logLen - bufSize);//哎呀! 用起递归来了哈. 
    } else {
        if (logLen > (bufSize - tail)) { /* need cycle back to start */ //需要循环回到开始
            ret = memcpy_s(buf + tail, bufSize - tail, dst, bufSize - tail); //先从tail位置写到buf末尾
            if (ret != EOK) {
                PRINT_ERR("%s,%d memcpy_s failed, err:%d!\n", __FUNCTION__, __LINE__, ret);
                return;
            }
            ret = memcpy_s(buf, bufSize, dst + bufSize - tail, logLen - (bufSize - tail));//再从buf头部写到剩余位置
            if (ret != EOK) {
                PRINT_ERR("%s,%d memcpy_s failed, err:%d!\n", __FUNCTION__, __LINE__, ret);
                return;
            }

            g_dmesgInfo->logTail = logLen - (bufSize - tail);//写入起始位置变了
            g_dmesgInfo->logHead = g_dmesgInfo->logTail;	//从tail位置开始读数据,因为即使有未读完的数据,此时也已经被dst覆盖了.
        } else { /* no need cycle back to start */ //不需要循环回到开始
            ret = memcpy_s(buf + tail, bufSize - tail, dst, logLen);//从tail位置写到buf末尾
            if (ret != EOK) {
                PRINT_ERR("%s,%d memcpy_s failed, err:%d!\n", __FUNCTION__, __LINE__, ret);
                return;
            }
            g_dmesgInfo->logTail += logLen;//
            if (g_dmesgInfo->logTail > BUF_MAX_INDEX) {//写到末尾了
                g_dmesgInfo->logTail = 0;//从头开始写
            }
            g_dmesgInfo->logHead = g_dmesgInfo->logTail;//从tail位置开始读数据,因为即使有未读完的数据,此时也已经被dst覆盖了.
        }
    }
}
//从读数据的位置开始写入
STATIC VOID OsWriteTailToHead(const CHAR *dst, UINT32 logLen)
{
    UINT32 writeLen = 0;
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

        g_dmesgInfo->logTail = g_dmesgInfo->logHead;	//读写位置一致
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
            OsWriteTailToHead(dst + writeLen, logLen - writeLen);//从读数据的位置开始写入
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
//dmesg中记录日志
INT32 OsLogMemcpyRecord(const CHAR *buf, UINT32 logLen)
{
    UINT32 intSave;

    LOS_SpinLockSave(&g_dmesgSpin, &intSave);
    if (OsCheckError()) {
        LOS_SpinUnlockRestore(&g_dmesgSpin, intSave);
        return -1;
    }
    if (g_dmesgInfo->logSize < g_logBufSize) {//小于缓冲区大小的情况将怎么写入数据?
        if (g_dmesgInfo->logHead <= g_dmesgInfo->logTail) {//说明head位置还有未读取的数据
            OsWriteTailToEnd(buf, logLen);//从写数据的位置开始写入
        } else {
            OsWriteTailToHead(buf, logLen);//从读数据的位置开始写入
        }
    } else {//超过缓冲区大小的情况
        OsBufFullWrite(buf, logLen);//全覆盖方式写buf
    }
    LOS_SpinUnlockRestore(&g_dmesgSpin, intSave);

    return LOS_OK;
}
//打印缓存区内容 shell dmesg 命令调用 
VOID OsLogShow(VOID)
{
    UINT32 intSave;
    UINT32 index;
    UINT32 i = 0;
    CHAR *p = NULL;

    LOS_SpinLockSave(&g_dmesgSpin, &intSave);
    index = g_dmesgInfo->logHead;	//logHead表示开始读的位置

    p = (CHAR *)malloc(g_dmesgInfo->logSize + 1);//申请日志长度内存用于拷贝内容
    if (p == NULL) {
        LOS_SpinUnlockRestore(&g_dmesgSpin, intSave);
        return;
    }
    (VOID)memset_s(p, g_dmesgInfo->logSize + 1, 0, g_dmesgInfo->logSize + 1);//内存块清0

    while (i < g_dmesgInfo->logSize) {
        *(p + i) = *(g_dmesgInfo->logBuf + index++);//一个一个字符复制
        if (index > BUF_MAX_INDEX) {//读到尾部
            index = 0;//从头开始读
        }
        i++;
        if (index == g_dmesgInfo->logTail) {//读到写入位置,说明读完了.
            break;
        }
    }
    LOS_SpinUnlockRestore(&g_dmesgSpin, intSave);
    UartPuts(p, i, UART_WITH_LOCK);//向串口发送数据
    free(p);//释放内存
}
//设置缓存等级,一共五级,默认第三级
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
//设置缓冲区大小, shell dmesg -s 16K 
STATIC INT32 OsDmesgMemSizeSet(const CHAR *size)
{
    UINT32 sizeVal;
    CHAR *p = NULL;

    sizeVal = strtoul(size, &p, 0);//C库函数将size所指向的字符串根据给定的base转换为一个无符号长整数（类型为 unsigned long int 型）
    if (sizeVal > MAX_KERNEL_LOG_BUF_SIZE) {//不能超过缓存区的上限,默认80K
        goto ERR_OUT;
    }

    if (!(LOS_DmesgMemSet(NULL, sizeVal))) {//调整缓存区大小
        PRINTK("Set dmesg buf size %u success\n", sizeVal);
        return LOS_OK;
    } else {
        goto ERR_OUT;
    }

ERR_OUT:
    PRINTK("Set dmesg buf size %u fail\n", sizeVal);
    return LOS_NOK;
}
//获取日志等级
UINT32 OsDmesgLvGet(VOID)
{
    return g_dmesgLogLevel;
}
//设置日志等级
UINT32 LOS_DmesgLvSet(UINT32 level)
{
    if (level > 5) { /* 5: count of level */
        return LOS_NOK;
    }

    g_dmesgLogLevel = level;
    return LOS_OK;
}
//清除缓存设置
VOID LOS_DmesgClear(VOID)
{
    UINT32 intSave;

    LOS_SpinLockSave(&g_dmesgSpin, &intSave);
    (VOID)memset_s(g_dmesgInfo->logBuf, g_logBufSize, 0, g_logBufSize);//缓存清0
    g_dmesgInfo->logHead = 0;
    g_dmesgInfo->logTail = 0;
    g_dmesgInfo->logSize = 0;
    LOS_SpinUnlockRestore(&g_dmesgSpin, intSave);
}

UINT32 LOS_DmesgMemSet(VOID *addr, UINT32 size)
{
    UINT32 ret = 0;

    if (addr == NULL) {
        ret = OsDmesgChangeSize(size);
    } else {
        ret = OsDmesgResetMem(addr, size);
    }
    return ret;
}
//LOS 开头为外部接口, 封装 OsDmesgRead
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
//将缓冲区内容写到指定文件
INT32 OsDmesgWrite2File(const CHAR *fullpath, const CHAR *buf, UINT32 logSize)
{
    INT32 ret;
	//见于 third_party\NuttX\fs\vfs\fs_open.c 
    INT32 fd = open(fullpath, O_CREAT | O_RDWR | O_APPEND, 0644); /* 0644:file right */ //可读可写 ,并以append的方式写文件
    if (fd < 0) { //fd必须大于0 ,并且 0,1,2 被控制台占用
        return -1;
    }
    ret = write(fd, buf, logSize);
    (VOID)close(fd);
    return ret;
}

#ifdef LOSCFG_FS_VFS //是否支持虚拟文件系统
INT32 LOS_DmesgToFile(const CHAR *filename)
{
    CHAR *fullpath = NULL;
    CHAR *buf = NULL;
    INT32 ret;
    CHAR *shellWorkingDirectory = OsShellGetWorkingDirtectory();
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

    ret = vfs_normalize_path(shellWorkingDirectory, filename, &fullpath);
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

    ret = OsDmesgWrite2File(fullpath, buf, logSize);
ERR_OUT3:
    free(buf);
ERR_OUT2:
    free(fullpath);
    return ret;
}
#else //不支持虚拟文件系统直接串口输出
INT32 LOS_DmesgToFile(CHAR *filename)
{
    (VOID)filename;
    PRINTK("File operation need VFS\n");
    return -1;
}
#endif

/****************************************************************
dmesg是一种程序，用于检测和控制内核环缓冲。程序用来帮助用户了解系统的启动信息。
Linux dmesg命令用于显示开机信息。鸿蒙沿用了linux dmesg命令
kernel会将开机信息存储在ring buffer中。您若是开机时来不及查看信息，可利用dmesg来查看。
开机信息亦保存在/var/log目录中，名称为dmesg的文件里

命令功能
dmesg命令用于控制内核dmesg缓存区。

命令格式
dmesg dmesg [-c/-C/-D/-E/-L/-U]
dmesg -s [size] dmesg -l [level dmesg > [fileA]

参数说明

参数	 参数说明 取值范围

-c 	打印缓存区内容并清空缓存区。N/A

-C 	清空缓存区。N/A

-D/-E	关闭/开启控制台打印。 N/A

-L/-U	关闭/开启串口打印。	N/A

-s size	设置缓存区大小 size是要设置的大小。 N/A

-l level	设置缓存等级。	0 - 5

> fileA	将缓存区内容写入文件。N/A

使用指南
该命令依赖于LOSCFG_SHELL_DMESG，使用时通过menuconfig在配置项中开启"Enable Shell dmesg"：
Debug ---> Enable a Debug Version ---> Enable Shell ---> Enable Shell dmesg
dmesg参数缺省时，默认打印缓存区内容。各“ - ”选项不能混合使用。
写入文件需确保已挂载文件系统。
关闭串口打印会影响shell使用，建议先连接telnet再尝试关闭串口。
举例：输入dmesg > /usr/dmesg.log。

****************************************************************/
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

        if (!strcmp(argv[1], "-c")) {//打印缓存区内容并清空缓存区。
            PRINTK("\n");
            OsLogShow();
            LOS_DmesgClear();
            return LOS_OK;
        } else if (!strcmp(argv[1], "-C")) {//清空缓存区
            LOS_DmesgClear();
            return LOS_OK;
        } else if (!strcmp(argv[1], "-D")) {//关闭控制台打印
            OsLockConsole();
            return LOS_OK;
        } else if (!strcmp(argv[1], "-E")) {//开启控制台打印
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

        if (!strcmp(argv[1], ">")) {
            if (LOS_DmesgToFile((CHAR *)argv[2]) < 0) { /* 2:index of parameters */
                PRINTK("Dmesg write log to %s fail \n", argv[2]); /* 2:index of parameters */
                return -1;
            } else {
                PRINTK("Dmesg write log to %s success \n", argv[2]); /* 2:index of parameters */
                return LOS_OK;
            }
        } else if (!strcmp(argv[1], "-l")) {
            return OsDmesgLvSet(argv[2]); /* 2:index of parameters */ //设置缓存等级,一共五级,默认第三级
        } else if (!strcmp(argv[1], "-s")) {
            return OsDmesgMemSizeSet(argv[2]); /* 2:index of parameters */ //设置缓冲区大小
        }
    }

ERR_OUT:
    PRINTK("dmesg: invalid option or parameter.\n");
    return -1;
}

SHELLCMD_ENTRY(dmesg_shellcmd, CMD_TYPE_STD, "dmesg", XARGS, (CmdCallBackFunc)OsShellCmdDmesg);//shell dmesg命令 静态注册方式

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */
#endif
