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

#include "console.h"
#include "fcntl.h"
#include "sys/ioctl.h"
#ifdef LOSCFG_FILE_MODE
#include "stdarg.h"
#endif
#include "unistd.h"
#include "securec.h"
#ifdef LOSCFG_SHELL_DMESG
#include "dmesg_pri.h"
#endif
#ifdef LOSCFG_SHELL
#include "shcmd.h"
#include "shell_pri.h"
#endif
#include "los_exc_pri.h"
#include "los_process_pri.h"
#include "los_sched_pri.h"
#include "user_copy.h"
#include "fs/driver.h"

#define EACH_CHAR 1
#define UART_IOC_MAGIC   'u'
#define UART_CFG_ATTR    _IOW(UART_IOC_MAGIC, 5, int)
#define UART_CFG_PRIVATE    _IOW(UART_IOC_MAGIC, 6, int)
/* 直接写在数据区的变量
#ifdef LOSCFG_QUICK_START
__attribute__ ((section(".data"))) UINT32 g_uart_fputc_en = 0;
#else
__attribute__ ((section(".data"))) UINT32 g_uart_fputc_en = 1;
#endif
*/
/**
 * @defgroup console_internal 控制台内部实现
 * @brief 控制台设备驱动适配、属性控制和任务管理的内部实现
 * @{ 
 */

/* 模块间变量声明 */
extern UINT32 g_uart_fputc_en;          /* UART输出使能标志 */
STATIC UINT32 ConsoleSendTask(UINTPTR param); /* 控制台发送任务声明 */

/**
 * @brief 任务-控制台ID映射数组
 * @details 记录每个任务对应的控制台ID，大小为系统最大任务数(LOSCFG_BASE_CORE_TSK_LIMIT)
 */
STATIC UINT8 g_taskConsoleIDArray[LOSCFG_BASE_CORE_TSK_LIMIT];
STATIC SPIN_LOCK_INIT(g_consoleSpin);          /* 控制台操作自旋锁 */
STATIC SPIN_LOCK_INIT(g_consoleWriteSpinLock); /* 控制台写操作自旋锁 */

/**
 * @defgroup console_const 控制台常量定义
 * @brief 控制台任务和状态相关常量
 * @{ 
 */
#define SHELL_ENTRYID_INVALID     0xFFFFFFFF  /* 无效的Shell入口任务ID */
#define SHELL_TASK_PRIORITY       9           /* Shell任务优先级(值越小优先级越高) */
#define CONSOLE_CIRBUF_EVENT      0x02U       /* 环形缓冲区事件标志 */
#define CONSOLE_SEND_TASK_EXIT    0x04U       /* 发送任务退出标志 */
#define CONSOLE_SEND_TASK_RUNNING 0x10U       /* 发送任务运行中标志 */
#define SHELL_ENTRY_NAME          "ShellEntry" /* Shell入口任务名称 */
#define SHELL_ENTRY_NAME_LEN      10          /* Shell入口任务名称长度 */
/** @} */ // console_const

CONSOLE_CB *g_console[CONSOLE_NUM];            /* 控制台控制块数组 */

/**
 * @brief 取两个数的最小值
 * @param a [IN] 比较值1
 * @param b [IN] 比较值2
 * @return 较小的数值
 */
#define MIN(a, b) ((a) < (b) ? (a) : (b))

/**
 * @brief 获取控制台设备的文件操作函数集
 * @details 获取UART驱动函数和/dev/console的文件指针，将UART驱动函数存储到filepOps，
 *          控制台设备文件指针存储到privFilep
 * @param filep [IN] 文件结构体指针
 * @param privFilep [OUT] 控制台设备文件指针的指针
 * @param filepOps [OUT] 文件操作函数集的指针
 * @return INT32 操作结果，成功返回ENOERR，失败返回错误码
 * @retval EINVAL 参数无效
 * @retval VFS_ERROR VFS操作错误
 */
INT32 GetFilepOps(const struct file *filep, struct file **privFilep, const struct file_operations_vfs **filepOps)
{
    INT32 ret;

    /* 参数有效性检查 */
    if ((filep == NULL) || (filep->f_vnode == NULL) || (filep->f_vnode->data == NULL)) {
        ret = EINVAL;
        goto ERROUT;
    }

    /* 通过i_private查找控制台设备的filep(当前为*privFilep) */
    struct drv_data *drv = (struct drv_data *)filep->f_vnode->data;
    *privFilep = (struct file *)drv->priv;
    
    /* 检查私有文件指针的有效性 */
    if (((*privFilep)->f_vnode == NULL) || ((*privFilep)->f_vnode->data == NULL)) {
        ret = EINVAL;
        goto ERROUT;
    }

    /* 通过u.i_opss查找UART驱动操作函数 */
    drv = (struct drv_data *)(*privFilep)->f_vnode->data;
    *filepOps = (const struct file_operations_vfs *)drv->ops;

    return ENOERR;
ERROUT:
    set_errno(ret);
    return VFS_ERROR;
}

/**
 * @brief 获取控制台终端属性
 * @param fd [IN] 文件描述符
 * @param termios [OUT] 终端属性结构体指针
 * @return INT32 操作结果，成功返回LOS_OK，失败返回错误码
 * @retval -EPERM 获取文件指针失败
 * @retval -EFAULT 控制台控制块无效
 */
INT32 ConsoleTcGetAttr(INT32 fd, struct termios *termios)
{
    struct file *filep = NULL;
    CONSOLE_CB *consoleCB = NULL;

    /* 获取文件指针 */
    INT32 ret = fs_getfilep(fd, &filep);
    if (ret < 0) {
        return -EPERM;
    }

    /* 获取控制台控制块 */
    consoleCB = (CONSOLE_CB *)filep->f_priv;
    if (consoleCB == NULL) {
        return -EFAULT;
    }

    /* 复制终端属性 */
    (VOID)memcpy_s(termios, sizeof(struct termios), &consoleCB->consoleTermios, sizeof(struct termios));
    return LOS_OK;
}

/**
 * @brief 设置控制台终端属性
 * @param fd [IN] 文件描述符
 * @param actions [IN] 属性生效时机(未使用)
 * @param termios [IN] 终端属性结构体指针
 * @return INT32 操作结果，成功返回LOS_OK，失败返回错误码
 * @retval -EPERM 获取文件指针失败
 * @retval -EFAULT 控制台控制块无效
 */
INT32 ConsoleTcSetAttr(INT32 fd, INT32 actions, const struct termios *termios)
{
    struct file *filep = NULL;
    CONSOLE_CB *consoleCB = NULL;

    (VOID)actions; /* 未使用的参数 */

    /* 获取文件指针 */
    INT32 ret = fs_getfilep(fd, &filep);
    if (ret < 0) {
        return -EPERM;
    }

    /* 获取控制台控制块 */
    consoleCB = (CONSOLE_CB *)filep->f_priv;
    if (consoleCB == NULL) {
        return -EFAULT;
    }

    /* 设置终端属性 */
    (VOID)memcpy_s(&consoleCB->consoleTermios, sizeof(struct termios), termios, sizeof(struct termios));
    return LOS_OK;
}

/**
 * @brief 获取控制台引用计数
 * @param consoleCB [IN] 控制台控制块指针
 * @return UINT32 当前引用计数值
 */
STATIC UINT32 ConsoleRefcountGet(const CONSOLE_CB *consoleCB)
{
    return consoleCB->refCount;
}

/**
 * @brief 设置控制台引用计数
 * @param consoleCB [IN] 控制台控制块指针
 * @param flag [IN] 增加/减少标志，TRUE表示增加，FALSE表示减少
 */
STATIC VOID ConsoleRefcountSet(CONSOLE_CB *consoleCB, BOOL flag)
{
    (consoleCB->refCount) += flag ? 1 : -1; // 根据标志增减引用计数
}

/**
 * @brief 判断控制台是否被占用
 * @param consoleCB [IN] 控制台控制块指针
 * @return BOOL 被占用返回TRUE，未被占用返回FALSE
 * @note 通过检查引用计数是否大于0判断占用状态
 */
BOOL IsConsoleOccupied(const CONSOLE_CB *consoleCB)
{
    return ConsoleRefcountGet(consoleCB);
}

/**
 * @brief 设置控制台为行捕获模式
 * @details 启用ICANON规范模式和ECHO回显功能，使控制台按行接收输入并回显字符
 * @param consoleCB [IN] 控制台控制块指针
 * @return INT32 操作结果，成功返回LOS_OK
 */
STATIC INT32 ConsoleCtrlCaptureLine(CONSOLE_CB *consoleCB)
{
    struct termios consoleTermios = {0};
    UINT32 intSave;

    /* 自旋锁保护终端属性操作 */
    LOS_SpinLockSave(&g_consoleSpin, &intSave);
    (VOID)ConsoleTcGetAttr(consoleCB->fd, &consoleTermios);
    consoleTermios.c_lflag |= ICANON | ECHO; // 启用规范模式和回显
    (VOID)ConsoleTcSetAttr(consoleCB->fd, 0, &consoleTermios);
    LOS_SpinUnlockRestore(&g_consoleSpin, intSave);

    return LOS_OK;
}

/** @} */ // console_internal

STATIC INT32 ConsoleCtrlCaptureChar(CONSOLE_CB *consoleCB)
{
    struct termios consoleTermios = {0};
    UINT32 intSave;

    LOS_SpinLockSave(&g_consoleSpin, &intSave);
    (VOID)ConsoleTcGetAttr(consoleCB->fd, &consoleTermios);
    consoleTermios.c_lflag &= ~(ICANON | ECHO);
    (VOID)ConsoleTcSetAttr(consoleCB->fd, 0, &consoleTermios);
    LOS_SpinUnlockRestore(&g_consoleSpin, intSave);

    return LOS_OK;
}

STATIC INT32 ConsoleCtrlRightsCapture(CONSOLE_CB *consoleCB)
{
    (VOID)LOS_SemPend(consoleCB->consoleSem, LOS_WAIT_FOREVER);
    if ((ConsoleRefcountGet(consoleCB) == 0) &&
        (OsCurrTaskGet()->taskID != consoleCB->shellEntryId)) {
        /* not 0:indicate that shellentry is in uart_read, suspend shellentry task directly */
        (VOID)LOS_TaskSuspend(consoleCB->shellEntryId);
    }
    ConsoleRefcountSet(consoleCB, TRUE);
    return LOS_OK;
}

STATIC INT32 ConsoleCtrlRightsRelease(CONSOLE_CB *consoleCB)
{
    if (ConsoleRefcountGet(consoleCB) == 0) {
        PRINT_ERR("console is free\n");
        (VOID)LOS_SemPost(consoleCB->consoleSem);
        return LOS_NOK;
    } else {
        ConsoleRefcountSet(consoleCB, FALSE);
        if ((ConsoleRefcountGet(consoleCB) == 0) &&
            (OsCurrTaskGet()->taskID != consoleCB->shellEntryId)) {
            (VOID)LOS_TaskResume(consoleCB->shellEntryId);
        }
    }
    (VOID)LOS_SemPost(consoleCB->consoleSem);
    return LOS_OK;
}

STATIC CONSOLE_CB *OsGetConsoleByDevice(const CHAR *deviceName)
{
    INT32 ret;
    struct Vnode *vnode = NULL;

    VnodeHold();
    ret = VnodeLookup(deviceName, &vnode, 0);
    VnodeDrop();
    if (ret < 0) {
        set_errno(EACCES);
        return NULL;
    }

    if (g_console[CONSOLE_SERIAL - 1]->devVnode == vnode) {
        return g_console[CONSOLE_SERIAL - 1];
    } else if (g_console[CONSOLE_TELNET - 1]->devVnode == vnode) {
        return g_console[CONSOLE_TELNET - 1];
    } else {
        set_errno(ENOENT);
        return NULL;
    }
}

STATIC INT32 OsGetConsoleID(const CHAR *deviceName)
{
    if ((deviceName != NULL) &&
        (strlen(deviceName) == strlen(SERIAL)) &&
        (!strncmp(deviceName, SERIAL, strlen(SERIAL)))) {
        return CONSOLE_SERIAL;
    }
#ifdef LOSCFG_NET_TELNET
    else if ((deviceName != NULL) &&
             (strlen(deviceName) == strlen(TELNET)) &&
             (!strncmp(deviceName, TELNET, strlen(TELNET)))) {
        return CONSOLE_TELNET;
    }
#endif
    return -1;
}

STATIC INT32 OsConsoleFullpathToID(const CHAR *fullpath)
{
#define CONSOLE_SERIAL_1 "/dev/console1"
#define CONSOLE_TELNET_2 "/dev/console2"

    size_t len;

    if (fullpath == NULL) {
        return -1;
    }

    len = strlen(fullpath);
    if ((len == strlen(CONSOLE_SERIAL_1)) &&
        (!strncmp(fullpath, CONSOLE_SERIAL_1, strlen(CONSOLE_SERIAL_1)))) {
        return CONSOLE_SERIAL;
    }
#ifdef LOSCFG_NET_TELNET
    else if ((len == strlen(CONSOLE_TELNET_2)) &&
             (!strncmp(fullpath, CONSOLE_TELNET_2, strlen(CONSOLE_TELNET_2)))) {
        return CONSOLE_TELNET;
    }
#endif
    return -1;
}

STATIC BOOL ConsoleFifoEmpty(const CONSOLE_CB *console)
{
    return console->fifoOut == console->fifoIn;
}

STATIC VOID ConsoleFifoClearup(CONSOLE_CB *console)
{
    console->fifoOut = 0;
    console->fifoIn = 0;
    (VOID)memset_s(console->fifo, CONSOLE_FIFO_SIZE, 0, CONSOLE_FIFO_SIZE);
}

STATIC VOID ConsoleFifoLenUpdate(CONSOLE_CB *console)
{
    console->currentLen = console->fifoIn - console->fifoOut;
}

STATIC INT32 ConsoleReadFifo(CHAR *buffer, CONSOLE_CB *console, size_t bufLen)
{
    INT32 ret;
    UINT32 readNum;

    readNum = MIN(bufLen, console->currentLen);
    ret = memcpy_s(buffer, bufLen, console->fifo + console->fifoOut, readNum);
    if (ret != EOK) {
        PRINTK("%s,%d memcpy_s failed\n", __FUNCTION__, __LINE__);
        return -1;
    }
    console->fifoOut += readNum;
    if (ConsoleFifoEmpty(console)) {
        ConsoleFifoClearup(console);
    }
    ConsoleFifoLenUpdate(console);
    return (INT32)readNum;
}

INT32 FilepOpen(struct file *filep, const struct file_operations_vfs *fops)
{
    INT32 ret;
    if (fops->open == NULL) {
        return -EFAULT;
    }

    /*
     * adopt uart open function to open filep (filep is
     * corresponding to filep of /dev/console)
     */
    ret = fops->open(filep);
    return (ret < 0) ? -EPERM : ret;
}

STATIC INLINE VOID UserEndOfRead(CONSOLE_CB *consoleCB, struct file *filep,
                                 const struct file_operations_vfs *fops)
{
    CHAR ch;
    if (consoleCB->consoleTermios.c_lflag & ECHO) {
        ch = '\r';
        (VOID)fops->write(filep, &ch, 1);
    }
    consoleCB->fifo[consoleCB->fifoIn++] = '\n';
    consoleCB->fifo[consoleCB->fifoIn] = '\0';
    consoleCB->currentLen = consoleCB->fifoIn;
}

enum {
    STAT_NORMAL_KEY,
    STAT_ESC_KEY,
    STAT_MULTI_KEY
};

STATIC INT32 UserShellCheckUDRL(const CHAR ch, INT32 *lastTokenType)
{
    INT32 ret = LOS_OK;
    if (ch == 0x1b) { /* 0x1b: ESC */
        *lastTokenType = STAT_ESC_KEY;
        return ret;
    } else if (ch == 0x5b) { /* 0x5b: first Key combination */
        if (*lastTokenType == STAT_ESC_KEY) {
            *lastTokenType = STAT_MULTI_KEY;
            return ret;
        }
    } else if (ch == 0x41) { /* up */
        if (*lastTokenType == STAT_MULTI_KEY) {
            *lastTokenType = STAT_NORMAL_KEY;
            return ret;
        }
    } else if (ch == 0x42) { /* down */
        if (*lastTokenType == STAT_MULTI_KEY) {
            *lastTokenType = STAT_NORMAL_KEY;
            return ret;
        }
    } else if (ch == 0x43) { /* right */
        if (*lastTokenType == STAT_MULTI_KEY) {
            *lastTokenType = STAT_NORMAL_KEY;
            return ret;
        }
    } else if (ch == 0x44) { /* left */
        if (*lastTokenType == STAT_MULTI_KEY) {
            *lastTokenType = STAT_NORMAL_KEY;
            return ret;
        }
    }
    return LOS_NOK;
}

STATIC INT32 IsNeedContinue(CONSOLE_CB *consoleCB, char ch, INT32 *lastTokenType)
{
    if (((ch == '\b') && (consoleCB->consoleTermios.c_lflag & ECHO) && (ConsoleFifoEmpty(consoleCB))) ||
        (UserShellCheckUDRL(ch, lastTokenType) == LOS_OK)) { /* parse the up/down/right/left key */
        return LOS_NOK;
    }

    return LOS_OK;
}

STATIC VOID EchoToTerminal(CONSOLE_CB *consoleCB, struct file *filep, const struct file_operations_vfs *fops, char ch)
{
    if (consoleCB->consoleTermios.c_lflag & ECHO) {
        if (ch == '\b') {
            (VOID)fops->write(filep, "\b \b", 3); // 3: length of "\b \b"
        } else {
            (VOID)fops->write(filep, &ch, EACH_CHAR);
        }
    }
}

STATIC VOID StoreReadChar(CONSOLE_CB *consoleCB, char ch, INT32 readcount)
{
    /* 3, store read char len need to minus \b  */
    if ((readcount == EACH_CHAR) && (consoleCB->fifoIn <= (CONSOLE_FIFO_SIZE - 3))) {
        if (ch == '\b') {
            if (!ConsoleFifoEmpty(consoleCB)) {
                consoleCB->fifo[--consoleCB->fifoIn] = '\0';
            }
        } else {
            consoleCB->fifo[consoleCB->fifoIn] = (UINT8)ch;
            consoleCB->fifoIn++;
        }
    }
}

VOID KillPgrp(UINT16 consoleId)
{
    if ((consoleId > CONSOLE_NUM) || (consoleId <= 0)) {
        return;
    }

    CONSOLE_CB *consoleCB = g_console[consoleId - 1];
    /* the default of consoleCB->pgrpId is -1, may not be set yet, avoid killing all processes */
    if (consoleCB->pgrpId < 0) {
        return;
    }
    (VOID)OsKillLock(consoleCB->pgrpId, SIGINT);
}

STATIC INT32 UserFilepRead(CONSOLE_CB *consoleCB, struct file *filep, const struct file_operations_vfs *fops,
                           CHAR *buffer, size_t bufLen)
{
    INT32 ret;
    INT32 needreturn = LOS_NOK;
    CHAR ch;
    INT32 lastTokenType = STAT_NORMAL_KEY;

    if (fops->read == NULL) {
        return -EFAULT;
    }

    /* Non-ICANON mode */
    if ((consoleCB->consoleTermios.c_lflag & ICANON) == 0) {
        ret = fops->read(filep, buffer, bufLen);
        return (ret < 0) ? -EPERM : ret;
    }
    /* ICANON mode: store data to console buffer,  read data and stored data into console fifo */
    if (consoleCB->currentLen == 0) {
        while (1) {
            ret = fops->read(filep, &ch, EACH_CHAR);
            if (ret <= 0) {
                return ret;
            }

            if (IsNeedContinue(consoleCB, ch, &lastTokenType))
                continue;

            switch (ch) {
                case '\r':
                    ch = '\n';
                case '\n':
                    EchoToTerminal(consoleCB, filep, fops, ch);
                    UserEndOfRead(consoleCB, filep, fops);
                    ret = ConsoleReadFifo(buffer, consoleCB, bufLen);

                    needreturn = LOS_OK;
                    break;
                case '\b':
                default:
                    EchoToTerminal(consoleCB, filep, fops, ch);
                    StoreReadChar(consoleCB, ch, ret);
                    break;
            }

            if (needreturn == LOS_OK)
                break;
        }
    } else {
        /* if data is already in console fifo, we return them immediately */
        ret = ConsoleReadFifo(buffer, consoleCB, bufLen);
    }

    return ret;
}

INT32 FilepRead(struct file *filep, const struct file_operations_vfs *fops, CHAR *buffer, size_t bufLen)
{
    INT32 ret;
    if (fops->read == NULL) {
        return -EFAULT;
    }
    /*
     * adopt uart read function to read data from filep
     * and write data to buffer (filep is
     * corresponding to filep of /dev/console)
     */
    ret = fops->read(filep, buffer, bufLen);
    return (ret < 0) ? -EPERM : ret;
}

INT32 FilepWrite(struct file *filep, const struct file_operations_vfs *fops, const CHAR *buffer, size_t bufLen)
{
    INT32 ret;
    if (fops->write == NULL) {
        return -EFAULT;
    }

    ret = fops->write(filep, buffer, bufLen);
    return (ret < 0) ? -EPERM : ret;
}

INT32 FilepClose(struct file *filep, const struct file_operations_vfs *fops)
{
    INT32 ret;
    if ((fops == NULL) || (fops->close == NULL)) {
        return -EFAULT;
    }

    /*
     * adopt uart close function to open filep (filep is
     * corresponding to filep of /dev/console)
     */
    ret = fops->close(filep);
    return ret < 0 ? -EPERM : ret;
}

INT32 FilepIoctl(struct file *filep, const struct file_operations_vfs *fops, INT32 cmd, unsigned long arg)
{
    INT32 ret;
    if (fops->ioctl == NULL) {
        return -EFAULT;
    }

    ret = fops->ioctl(filep, cmd, arg);
    return (ret < 0) ? -EPERM : ret;
}

INT32 FilepPoll(struct file *filep, const struct file_operations_vfs *fops, poll_table *fds)
{
    INT32 ret;
    if (fops->poll == NULL) {
        return -EFAULT;
    }

    /*
     * adopt uart poll function to poll filep (filep is
     * corresponding to filep of /dev/serial)
     */
    ret = fops->poll(filep, fds);
    return (ret < 0) ? -EPERM : ret;
}

STATIC INT32 ConsoleOpen(struct file *filep)
{
    INT32 ret;
    UINT32 consoleID;
    struct file *privFilep = NULL;
    const struct file_operations_vfs *fileOps = NULL;

    consoleID = (UINT32)OsConsoleFullpathToID(filep->f_path);
    if (consoleID == (UINT32)-1) {
        ret = EPERM;
        goto ERROUT;
    }
    filep->f_priv = g_console[consoleID - 1];

    ret = GetFilepOps(filep, &privFilep, &fileOps);
    if (ret != ENOERR) {
        ret = EINVAL;
        goto ERROUT;
    }
    ret = FilepOpen(privFilep, fileOps);
    if (ret < 0) {
        ret = EPERM;
        goto ERROUT;
    }
    return ENOERR;

ERROUT:
    set_errno(ret);
    return VFS_ERROR;
}

STATIC INT32 ConsoleClose(struct file *filep)
{
    INT32 ret;
    struct file *privFilep = NULL;
    const struct file_operations_vfs *fileOps = NULL;

    ret = GetFilepOps(filep, &privFilep, &fileOps);
    if (ret != ENOERR) {
        ret = EINVAL;
        goto ERROUT;
    }
    ret = FilepClose(privFilep, fileOps);
    if (ret < 0) {
        ret = EPERM;
        goto ERROUT;
    }

    return ENOERR;

ERROUT:
    set_errno(ret);
    return VFS_ERROR;
}

STATIC ssize_t DoRead(CONSOLE_CB *consoleCB, CHAR *buffer, size_t bufLen,
                      struct file *privFilep,
                      const struct file_operations_vfs *fileOps)
{
    INT32 ret;

#ifdef LOSCFG_SHELL
    if (OsCurrTaskGet()->taskID == consoleCB->shellEntryId) {
        ret = FilepRead(privFilep, fileOps, buffer, bufLen);
    } else {
#endif
        (VOID)ConsoleCtrlRightsCapture(consoleCB);
        ret = UserFilepRead(consoleCB, privFilep, fileOps, buffer, bufLen);
        (VOID)ConsoleCtrlRightsRelease(consoleCB);
#ifdef LOSCFG_SHELL
    }
#endif

    return ret;
}

STATIC ssize_t ConsoleRead(struct file *filep, CHAR *buffer, size_t bufLen)
{
    INT32 ret;
    struct file *privFilep = NULL;
    CONSOLE_CB *consoleCB = NULL;
    CHAR *sbuffer = NULL;
    BOOL userBuf = FALSE;
    const struct file_operations_vfs *fileOps = NULL;

    if ((buffer == NULL) || (bufLen == 0)) {
        ret = EINVAL;
        goto ERROUT;
    }

    if (bufLen > CONSOLE_FIFO_SIZE) {
        bufLen = CONSOLE_FIFO_SIZE;
    }

    userBuf = LOS_IsUserAddressRange((vaddr_t)(UINTPTR)buffer, bufLen);
    ret = GetFilepOps(filep, &privFilep, &fileOps);
    if (ret != ENOERR) {
        ret = -EINVAL;
        goto ERROUT;
    }
    consoleCB = (CONSOLE_CB *)filep->f_priv;
    if (consoleCB == NULL) {
        consoleCB = OsGetConsoleByTaskID(OsCurrTaskGet()->taskID);
        if (consoleCB == NULL) {
            return -EFAULT;
        }
    }

    /*
     * shell task use FilepRead function to get data,
     * user task use UserFilepRead to get data
     */
    sbuffer = userBuf ? LOS_MemAlloc(m_aucSysMem0, bufLen) : buffer;
    if (sbuffer == NULL) {
        ret = -ENOMEM;
        goto ERROUT;
    }

    ret = DoRead(consoleCB, sbuffer, bufLen, privFilep, fileOps);
    if (ret < 0) {
        goto ERROUT;
    }

    if (userBuf) {
        if (LOS_ArchCopyToUser(buffer, sbuffer, ret) != 0) {
            ret = -EFAULT;
            goto ERROUT;
        }

        LOS_MemFree(m_aucSysMem0, sbuffer);
    }

    return ret;

ERROUT:
    if ((userBuf) && (sbuffer != NULL)) {
        LOS_MemFree(m_aucSysMem0, sbuffer);
    }
    set_errno(-ret);
    return VFS_ERROR;
}

STATIC ssize_t DoWrite(CirBufSendCB *cirBufSendCB, CHAR *buffer, size_t bufLen)
{
    INT32 cnt;
    size_t written = 0;
    UINT32 intSave;

#ifdef LOSCFG_SHELL_DMESG
    (VOID)OsLogMemcpyRecord(buffer, bufLen);
    if (OsCheckConsoleLock()) {
        return 0;
    }
#endif

    LOS_SpinLockSave(&g_consoleWriteSpinLock, &intSave);
    while (written < (INT32)bufLen) {
        /* Transform for CR/LR mode */
        if ((buffer[written] == '\n') || (buffer[written] == '\r')) {
            (VOID)LOS_CirBufWrite(&cirBufSendCB->cirBufCB, "\r", 1);
        }

        cnt = LOS_CirBufWrite(&cirBufSendCB->cirBufCB, &buffer[written], 1);
        if (cnt <= 0) {
            break;
        }
        written += cnt;
    }
    LOS_SpinUnlockRestore(&g_consoleWriteSpinLock, intSave);

    /* Log is cached but not printed when a system exception occurs */
    if (OsGetSystemStatus() == OS_SYSTEM_NORMAL) {
        (VOID)LOS_EventWrite(&cirBufSendCB->sendEvent, CONSOLE_CIRBUF_EVENT);
    }

    return written;
}

STATIC ssize_t ConsoleWrite(struct file *filep, const CHAR *buffer, size_t bufLen)
{
    INT32 ret;
    CHAR *sbuffer = NULL;
    BOOL userBuf = FALSE;
    CirBufSendCB *cirBufSendCB = NULL;
    struct file *privFilep = NULL;
    const struct file_operations_vfs *fileOps = NULL;

    if ((buffer == NULL) || (bufLen == 0)) {
        ret = EINVAL;
        goto ERROUT;
    }

    if (bufLen > CONSOLE_FIFO_SIZE) {
        bufLen = CONSOLE_FIFO_SIZE;
    }

    userBuf = LOS_IsUserAddressRange((vaddr_t)(UINTPTR)buffer, bufLen);

    ret = GetFilepOps(filep, &privFilep, &fileOps);
    if ((ret != ENOERR) || (fileOps->write == NULL) || (filep->f_priv == NULL)) {
        ret = EINVAL;
        goto ERROUT;
    }
    cirBufSendCB = ((CONSOLE_CB *)filep->f_priv)->cirBufSendCB;

    /*
     * adopt uart open function to read data from buffer
     * and write data to filep (filep is
     * corresponding to filep of /dev/console)
     */
    sbuffer = userBuf ? LOS_MemAlloc(m_aucSysMem0, bufLen) : (CHAR *)buffer;
    if (sbuffer == NULL) {
        ret = ENOMEM;
        goto ERROUT;
    }

    if (userBuf && (LOS_ArchCopyFromUser(sbuffer, buffer, bufLen) != 0)) {
        ret = EFAULT;
        goto ERROUT;
    }
    ret = DoWrite(cirBufSendCB, sbuffer, bufLen);

    if (userBuf) {
        LOS_MemFree(m_aucSysMem0, sbuffer);
    }

    return ret;

ERROUT:
    if (userBuf && sbuffer != NULL) {
        LOS_MemFree(m_aucSysMem0, sbuffer);
    }

    set_errno(ret);
    return VFS_ERROR;
}

STATIC INT32 ConsoleSetSW(CONSOLE_CB *consoleCB, unsigned long arg)
{
    struct termios kerTermios;
    UINT32 intSave;

    if (LOS_ArchCopyFromUser(&kerTermios, (struct termios *)arg, sizeof(struct termios)) != 0) {
        return -EFAULT;
    }

    LOS_SpinLockSave(&g_consoleSpin, &intSave);
    (VOID)ConsoleTcSetAttr(consoleCB->fd, 0, &kerTermios);
    LOS_SpinUnlockRestore(&g_consoleSpin, intSave);
    return LOS_OK;
}

#define DEFAULT_WINDOW_SIZE_COL 80
#define DEFAULT_WINDOW_SIZE_ROW 24
STATIC INT32 ConsoleGetWinSize(unsigned long arg)
{
    struct winsize kws = {
        .ws_col = DEFAULT_WINDOW_SIZE_COL,
        .ws_row = DEFAULT_WINDOW_SIZE_ROW
    };

    return (LOS_CopyFromKernel((VOID *)arg, sizeof(struct winsize), &kws, sizeof(struct winsize)) != 0) ?
        -EFAULT : LOS_OK;
}

STATIC INT32 ConsoleGetTermios(unsigned long arg)
{
    struct file *filep = NULL;
    CONSOLE_CB *consoleCB = NULL;

    INT32 ret = fs_getfilep(0, &filep);
    if (ret < 0) {
        return -EPERM;
    }

    consoleCB = (CONSOLE_CB *)filep->f_priv;
    if (consoleCB == NULL) {
        return -EFAULT;
    }

    return (LOS_ArchCopyToUser((VOID *)arg, &consoleCB->consoleTermios, sizeof(struct termios)) != 0) ?
        -EFAULT : LOS_OK;
}

INT32 ConsoleSetPgrp(CONSOLE_CB *consoleCB, unsigned long arg)
{
    if (LOS_ArchCopyFromUser(&consoleCB->pgrpId, (INT32 *)(UINTPTR)arg, sizeof(INT32)) != 0) {
        return -EFAULT;
    }
    return LOS_OK;
}

INT32 ConsoleGetPgrp(CONSOLE_CB *consoleCB, unsigned long arg)
{
    return (LOS_ArchCopyToUser((VOID *)arg, &consoleCB->pgrpId, sizeof(INT32)) != 0) ? -EFAULT : LOS_OK;
}

STATIC INT32 ConsoleIoctl(struct file *filep, INT32 cmd, unsigned long arg)
{
    INT32 ret;
    struct file *privFilep = NULL;
    CONSOLE_CB *consoleCB = NULL;
    const struct file_operations_vfs *fileOps = NULL;

    ret = GetFilepOps(filep, &privFilep, &fileOps);
    if (ret != ENOERR) {
        ret = EINVAL;
        goto ERROUT;
    }

    if (fileOps->ioctl == NULL) {
        ret = EFAULT;
        goto ERROUT;
    }

    consoleCB = (CONSOLE_CB *)filep->f_priv;
    if (consoleCB == NULL) {
        ret = EINVAL;
        goto ERROUT;
    }

    switch (cmd) {
        case CONSOLE_CONTROL_RIGHTS_CAPTURE:
            ret = ConsoleCtrlRightsCapture(consoleCB);
            break;
        case CONSOLE_CONTROL_RIGHTS_RELEASE:
            ret = ConsoleCtrlRightsRelease(consoleCB);
            break;
        case CONSOLE_CONTROL_CAPTURE_LINE:
            ret = ConsoleCtrlCaptureLine(consoleCB);
            break;
        case CONSOLE_CONTROL_CAPTURE_CHAR:
            ret = ConsoleCtrlCaptureChar(consoleCB);
            break;
        case CONSOLE_CONTROL_REG_USERTASK:
            ret = ConsoleTaskReg(consoleCB->consoleID, arg);
            break;
        case TIOCGWINSZ:
            ret = ConsoleGetWinSize(arg);
            break;
        case TCSETSW:
            ret = ConsoleSetSW(consoleCB, arg);
            break;
        case TCGETS:
            ret = ConsoleGetTermios(arg);
            break;
        case TIOCGPGRP:
            ret = ConsoleGetPgrp(consoleCB, arg);
            break;
        case TIOCSPGRP:
            ret = ConsoleSetPgrp(consoleCB, arg);
            break;
        default:
            if ((cmd == UART_CFG_ATTR || cmd == UART_CFG_PRIVATE)
                && !LOS_IsUserAddress(arg)) {
                ret = EINVAL;
                goto ERROUT;
            }
            ret = fileOps->ioctl(privFilep, cmd, arg);
            break;
    }

    if (ret < 0) {
        ret = EPERM;
        goto ERROUT;
    }

    return ret;
ERROUT:
    set_errno(ret);
    return VFS_ERROR;
}

STATIC INT32 ConsolePoll(struct file *filep, poll_table *fds)
{
    INT32 ret;
    struct file *privFilep = NULL;
    const struct file_operations_vfs *fileOps = NULL;

    ret = GetFilepOps(filep, &privFilep, &fileOps);
    if (ret != ENOERR) {
        ret = EINVAL;
        goto ERROUT;
    }

    ret = FilepPoll(privFilep, fileOps, fds);
    if (ret < 0) {
        ret = EPERM;
        goto ERROUT;
    }
    return ret;

ERROUT:
    set_errno(ret);
    return VFS_ERROR;
}

/* console device driver function structure */
STATIC const struct file_operations_vfs g_consoleDevOps = {
    .open = ConsoleOpen,   /* open */
    .close = ConsoleClose, /* close */
    .read = ConsoleRead,   /* read */
    .write = ConsoleWrite, /* write */
    .seek = NULL,
    .ioctl = ConsoleIoctl,
    .mmap = NULL,
#ifndef CONFIG_DISABLE_POLL
    .poll = ConsolePoll,
#endif
};

STATIC VOID OsConsoleTermiosInit(CONSOLE_CB *consoleCB, const CHAR *deviceName)
{
    struct termios consoleTermios = {0};

    if ((deviceName != NULL) &&
        (strlen(deviceName) == strlen(SERIAL)) &&
        (!strncmp(deviceName, SERIAL, strlen(SERIAL)))) {
        consoleCB->isNonBlock = SetSerialBlock(consoleCB);

        /* set console to have a buffer for user */
        (VOID)ConsoleTcGetAttr(consoleCB->fd, &consoleTermios);
        consoleTermios.c_lflag |= ICANON | ECHO;
        consoleTermios.c_cc[VINTR] = 3; /* /003 for ^C */
        (VOID)ConsoleTcSetAttr(consoleCB->fd, 0, &consoleTermios);
    }
#ifdef LOSCFG_NET_TELNET
    else if ((deviceName != NULL) &&
             (strlen(deviceName) == strlen(TELNET)) &&
             (!strncmp(deviceName, TELNET, strlen(TELNET)))) {
        consoleCB->isNonBlock = SetTelnetBlock(consoleCB);
        /* set console to have a buffer for user */
        (VOID)ConsoleTcGetAttr(consoleCB->fd, &consoleTermios);
        consoleTermios.c_lflag |= ICANON | ECHO;
        consoleTermios.c_cc[VINTR] = 3; /* /003 for ^C */
        (VOID)ConsoleTcSetAttr(consoleCB->fd, 0, &consoleTermios);
    }
#endif
}

STATIC INT32 OsConsoleFileInit(CONSOLE_CB *consoleCB)
{
    INT32 ret;
    struct Vnode *vnode = NULL;
    struct file *filep = NULL;

    VnodeHold();
    ret = VnodeLookup(consoleCB->name, &vnode, 0);
    if (ret != LOS_OK) {
        ret = EACCES;
        goto ERROUT;
    }

    filep = files_allocate(vnode, O_RDWR, 0, consoleCB, FILE_START_FD);
    if (filep == NULL) {
        ret = EMFILE;
        goto ERROUT;
    }
    filep->ops = (struct file_operations_vfs *)((struct drv_data *)vnode->data)->ops;
    consoleCB->fd = filep->fd;

ERROUT:
    VnodeDrop();
    return ret;
}

/*
 * Initialized console control platform so that when we operate /dev/console
 * as if we are operating /dev/ttyS0 (uart0).
 */
STATIC INT32 OsConsoleDevInit(CONSOLE_CB *consoleCB, const CHAR *deviceName)
{
    INT32 ret;
    struct file *filep = NULL;
    struct Vnode *vnode = NULL;
    struct file_operations_vfs *devOps = NULL;

    /* allocate memory for filep,in order to unchange the value of filep */
    filep = (struct file *)LOS_MemAlloc(m_aucSysMem0, sizeof(struct file));
    if (filep == NULL) {
        ret = ENOMEM;
        goto ERROUT;
    }

    VnodeHold();
    ret = VnodeLookup(deviceName, &vnode, V_DUMMY);
    VnodeDrop(); // not correct, but can't fix perfectly here
    if (ret != LOS_OK) {
        ret = EACCES;
        PRINTK("!! can not find %s\n", consoleCB->name);
        goto ERROUT;
    }

    consoleCB->devVnode = vnode;

    /*
     * initialize the console filep which is associated with /dev/console,
     * assign the uart0 vnode of /dev/ttyS0 to console inod of /dev/console,
     * then we can operate console's filep as if we operate uart0 filep of
     * /dev/ttyS0.
     */
    (VOID)memset_s(filep, sizeof(struct file), 0, sizeof(struct file));
    filep->f_oflags = O_RDWR;
    filep->f_pos = 0;
    filep->f_vnode = vnode;
    filep->f_path = NULL;
    filep->f_priv = NULL;
    /*
     * Use filep to connect console and uart, we can find uart driver function through filep.
     * now we can operate /dev/console to operate /dev/ttyS0 through filep.
     */
    devOps = (struct file_operations_vfs *)((struct drv_data*)vnode->data)->ops;
    if (devOps != NULL && devOps->open != NULL) {
        (VOID)devOps->open(filep);
    } else {
        ret = ENOSYS;
        goto ERROUT;
    }

    ret = register_driver(consoleCB->name, &g_consoleDevOps, DEFFILEMODE, filep);
    if (ret != LOS_OK) {
        goto ERROUT;
    }

    return LOS_OK;

ERROUT:
     if (filep) {
        (VOID)LOS_MemFree(m_aucSysMem0, filep);
    }

    set_errno(ret);
    return LOS_NOK;
}

STATIC UINT32 OsConsoleDevDeinit(const CONSOLE_CB *consoleCB)
{
    return unregister_driver(consoleCB->name);
}

STATIC CirBufSendCB *ConsoleCirBufCreate(VOID)
{
    UINT32 ret;
    CHAR *fifo = NULL;
    CirBufSendCB *cirBufSendCB = NULL;
    CirBuf *cirBufCB = NULL;

    cirBufSendCB = (CirBufSendCB *)LOS_MemAlloc(m_aucSysMem0, sizeof(CirBufSendCB));
    if (cirBufSendCB == NULL) {
        return NULL;
    }
    (VOID)memset_s(cirBufSendCB, sizeof(CirBufSendCB), 0, sizeof(CirBufSendCB));

    fifo = (CHAR *)LOS_MemAlloc(m_aucSysMem0, CONSOLE_CIRCBUF_SIZE);
    if (fifo == NULL) {
        goto ERROR_WITH_SENDCB;
    }
    (VOID)memset_s(fifo, CONSOLE_CIRCBUF_SIZE, 0, CONSOLE_CIRCBUF_SIZE);

    cirBufCB = &cirBufSendCB->cirBufCB;
    ret = LOS_CirBufInit(cirBufCB, fifo, CONSOLE_CIRCBUF_SIZE);
    if (ret != LOS_OK) {
        goto ERROR_WITH_FIFO;
    }

    (VOID)LOS_EventInit(&cirBufSendCB->sendEvent);
    return cirBufSendCB;

ERROR_WITH_FIFO:
    (VOID)LOS_MemFree(m_aucSysMem0, cirBufCB->fifo);
ERROR_WITH_SENDCB:
    (VOID)LOS_MemFree(m_aucSysMem0, cirBufSendCB);
    return NULL;
}

STATIC VOID ConsoleCirBufDelete(CirBufSendCB *cirBufSendCB)
{
    CirBuf *cirBufCB = &cirBufSendCB->cirBufCB;

    (VOID)LOS_MemFree(m_aucSysMem0, cirBufCB->fifo);
    LOS_CirBufDeinit(cirBufCB);
    (VOID)LOS_EventDestroy(&cirBufSendCB->sendEvent);
    (VOID)LOS_MemFree(m_aucSysMem0, cirBufSendCB);
}

STATIC UINT32 OsConsoleBufInit(CONSOLE_CB *consoleCB)
{
    UINT32 ret;
    TSK_INIT_PARAM_S initParam = {0};

    consoleCB->cirBufSendCB = ConsoleCirBufCreate();
    if (consoleCB->cirBufSendCB == NULL) {
        return LOS_NOK;
    }

    initParam.pfnTaskEntry = (TSK_ENTRY_FUNC)ConsoleSendTask;
    initParam.usTaskPrio   = SHELL_TASK_PRIORITY;
    initParam.auwArgs[0]   = (UINTPTR)consoleCB;
    initParam.uwStackSize  = LOSCFG_BASE_CORE_TSK_DEFAULT_STACK_SIZE;
    initParam.pcName = (consoleCB->consoleID == CONSOLE_SERIAL) ? "SendToSer" : "SendToTelnet";
    initParam.uwResved     = LOS_TASK_STATUS_DETACHED;

    ret = LOS_TaskCreate(&consoleCB->sendTaskID, &initParam);
    if (ret != LOS_OK) {
        ConsoleCirBufDelete(consoleCB->cirBufSendCB);
        consoleCB->cirBufSendCB = NULL;
        return LOS_NOK;
    }
    (VOID)LOS_EventRead(&consoleCB->cirBufSendCB->sendEvent, CONSOLE_SEND_TASK_RUNNING,
                        LOS_WAITMODE_OR | LOS_WAITMODE_CLR, LOS_WAIT_FOREVER);

    return LOS_OK;
}

STATIC VOID OsConsoleBufDeinit(CONSOLE_CB *consoleCB)
{
    CirBufSendCB *cirBufSendCB = consoleCB->cirBufSendCB;

    consoleCB->cirBufSendCB = NULL;
    (VOID)LOS_EventWrite(&cirBufSendCB->sendEvent, CONSOLE_SEND_TASK_EXIT);
}

STATIC CONSOLE_CB *OsConsoleCBInit(UINT32 consoleID)
{
    CONSOLE_CB *consoleCB = (CONSOLE_CB *)LOS_MemAlloc((VOID *)m_aucSysMem0, sizeof(CONSOLE_CB));
    if (consoleCB == NULL) {
        return NULL;
    }
    (VOID)memset_s(consoleCB, sizeof(CONSOLE_CB), 0, sizeof(CONSOLE_CB));

    consoleCB->consoleID = consoleID;
    consoleCB->pgrpId = -1;
    consoleCB->shellEntryId = SHELL_ENTRYID_INVALID; /* initialize shellEntryId to an invalid value */
    consoleCB->name = LOS_MemAlloc((VOID *)m_aucSysMem0, CONSOLE_NAMELEN);
    if (consoleCB->name == NULL) {
        PRINT_ERR("consoleCB->name malloc failed\n");
        (VOID)LOS_MemFree((VOID *)m_aucSysMem0, consoleCB);
        return NULL;
    }
    return consoleCB;
}

STATIC VOID OsConsoleCBDeinit(CONSOLE_CB *consoleCB)
{
    (VOID)LOS_MemFree((VOID *)m_aucSysMem0, consoleCB->name);
    consoleCB->name = NULL;
    (VOID)LOS_MemFree((VOID *)m_aucSysMem0, consoleCB);
}

STATIC CONSOLE_CB *OsConsoleCreate(UINT32 consoleID, const CHAR *deviceName)
{
    INT32 ret;
    CONSOLE_CB *consoleCB = OsConsoleCBInit(consoleID);
    if (consoleCB == NULL) {
        PRINT_ERR("console malloc error.\n");
        return NULL;
    }

    ret = snprintf_s(consoleCB->name, CONSOLE_NAMELEN, CONSOLE_NAMELEN - 1,
                     "%s%u", CONSOLE, consoleCB->consoleID);
    if (ret == -1) {
        PRINT_ERR("consoleCB->name snprintf_s failed\n");
        goto ERR_WITH_NAME;
    }

    ret = (INT32)OsConsoleBufInit(consoleCB);
    if (ret != LOS_OK) {
        PRINT_ERR("console OsConsoleBufInit error. %d\n", ret);
        goto ERR_WITH_NAME;
    }

    ret = (INT32)LOS_SemCreate(1, &consoleCB->consoleSem);
    if (ret != LOS_OK) {
        PRINT_ERR("create sem for uart failed\n");
        goto ERR_WITH_BUF;
    }

    ret = OsConsoleDevInit(consoleCB, deviceName);
    if (ret != LOS_OK) {
        PRINT_ERR("console OsConsoleDevInit error. %d\n", ret);
        goto ERR_WITH_SEM;
    }

    ret = OsConsoleFileInit(consoleCB);
    if (ret != LOS_OK) {
        PRINT_ERR("console OsConsoleFileInit error. %d\n", ret);
        goto ERR_WITH_DEV;
    }

    OsConsoleTermiosInit(consoleCB, deviceName);
    return consoleCB;

ERR_WITH_DEV:
    ret = (INT32)OsConsoleDevDeinit(consoleCB);
    if (ret != LOS_OK) {
        PRINT_ERR("OsConsoleDevDeinit failed!\n");
    }
ERR_WITH_SEM:
    (VOID)LOS_SemDelete(consoleCB->consoleSem);
ERR_WITH_BUF:
    OsConsoleBufDeinit(consoleCB);
ERR_WITH_NAME:
    OsConsoleCBDeinit(consoleCB);
    return NULL;
}

STATIC UINT32 OsConsoleDelete(CONSOLE_CB *consoleCB)
{
    UINT32 ret;

    (VOID)files_close(consoleCB->fd);
    ret = OsConsoleDevDeinit(consoleCB);
    if (ret != LOS_OK) {
        PRINT_ERR("OsConsoleDevDeinit failed!\n");
    }
    OsConsoleBufDeinit((CONSOLE_CB *)consoleCB);
    (VOID)LOS_SemDelete(consoleCB->consoleSem);
    (VOID)LOS_MemFree(m_aucSysMem0, consoleCB->name);
    consoleCB->name = NULL;
    (VOID)LOS_MemFree(m_aucSysMem0, consoleCB);

    return ret;
}

/* Initialized system console and return stdinfd stdoutfd stderrfd */
INT32 system_console_init(const CHAR *deviceName)
{
#ifdef LOSCFG_SHELL
    UINT32 ret;
#endif
    INT32 consoleID;
    UINT32 intSave;
    CONSOLE_CB *consoleCB = NULL;

    consoleID = OsGetConsoleID(deviceName);
    if (consoleID == -1) {
        PRINT_ERR("device is full.\n");
        return VFS_ERROR;
    }

    consoleCB = OsConsoleCreate((UINT32)consoleID, deviceName);
    if (consoleCB == NULL) {
        PRINT_ERR("%s, %d\n", __FUNCTION__, __LINE__);
        return VFS_ERROR;
    }

    LOS_SpinLockSave(&g_consoleSpin, &intSave);
    g_console[consoleID - 1] = consoleCB;
    if (OsCurrTaskGet() != NULL) {
        g_taskConsoleIDArray[OsCurrTaskGet()->taskID] = (UINT8)consoleID;
    }
    LOS_SpinUnlockRestore(&g_consoleSpin, intSave);

#ifdef LOSCFG_SHELL
    ret = OsShellInit(consoleID);
    if (ret != LOS_OK) {
        PRINT_ERR("%s, %d\n", __FUNCTION__, __LINE__);
        LOS_SpinLockSave(&g_consoleSpin, &intSave);
        (VOID)OsConsoleDelete(consoleCB);
        g_console[consoleID - 1] = NULL;
        if (OsCurrTaskGet() != NULL) {
            g_taskConsoleIDArray[OsCurrTaskGet()->taskID] = 0;
        }
        LOS_SpinUnlockRestore(&g_consoleSpin, intSave);
        return VFS_ERROR;
    }
#endif

    return ENOERR;
}

INT32 system_console_deinit(const CHAR *deviceName)
{
    UINT32 ret;
    CONSOLE_CB *consoleCB = NULL;
    UINT32 taskIdx;
    LosTaskCB *taskCB = NULL;
    UINT32 intSave;

    consoleCB = OsGetConsoleByDevice(deviceName);
    if (consoleCB == NULL) {
        return VFS_ERROR;
    }

#ifdef LOSCFG_SHELL
    (VOID)OsShellDeinit((INT32)consoleCB->consoleID);
#endif

    LOS_SpinLockSave(&g_consoleSpin, &intSave);
    /* Redirect all tasks to serial as telnet was unavailable after deinitializing */
    for (taskIdx = 0; taskIdx < g_taskMaxNum; taskIdx++) {
        taskCB = ((LosTaskCB *)g_taskCBArray) + taskIdx;
        if (OsTaskIsUnused(taskCB)) {
            continue;
        } else {
            g_taskConsoleIDArray[taskCB->taskID] = CONSOLE_SERIAL;
        }
    }
    g_console[consoleCB->consoleID - 1] = NULL;
    LOS_SpinUnlockRestore(&g_consoleSpin, intSave);

    ret = OsConsoleDelete(consoleCB);
    if (ret != LOS_OK) {
        PRINT_ERR("%s, Failed to system_console_deinit\n", __FUNCTION__);
        return VFS_ERROR;
    }

    return ENOERR;
}

BOOL ConsoleEnable(VOID)
{
    INT32 consoleID;

    if (OsCurrTaskGet() != NULL) {
        consoleID = g_taskConsoleIDArray[OsCurrTaskGet()->taskID];
        if (g_uart_fputc_en == 0) {
            if ((g_console[CONSOLE_TELNET - 1] != NULL) && OsPreemptable()) {
                return TRUE;
            }
        }

        if (consoleID == 0) {
            return FALSE;
        } else if ((consoleID == CONSOLE_TELNET) || (consoleID == CONSOLE_SERIAL)) {
            return ((OsGetSystemStatus() == OS_SYSTEM_NORMAL) && !OsPreemptable()) ? FALSE : TRUE;
        }
#if defined (LOSCFG_DRIVERS_USB_SERIAL_GADGET) || defined (LOSCFG_DRIVERS_USB_ETH_SER_GADGET)
        else if ((SerialTypeGet() == SERIAL_TYPE_USBTTY_DEV) && (userial_mask_get() == 1)) {
            return TRUE;
        }
#endif
    }

    return FALSE;
}

BOOL IsShellEntryRunning(UINT32 shellEntryId)
{
    LosTaskCB *taskCB = NULL;
    if (shellEntryId == SHELL_ENTRYID_INVALID) {
        return FALSE;
    }
    taskCB = OsGetTaskCB(shellEntryId);
    return !OsTaskIsUnused(taskCB) &&
           (strlen(taskCB->taskName) == SHELL_ENTRY_NAME_LEN &&
            strncmp(taskCB->taskName, SHELL_ENTRY_NAME, SHELL_ENTRY_NAME_LEN) == 0);
}

INT32 ConsoleTaskReg(INT32 consoleID, UINT32 taskID)
{
    UINT32 intSave;

    LOS_SpinLockSave(&g_consoleSpin, &intSave);
    if (!IsShellEntryRunning(g_console[consoleID - 1]->shellEntryId)) {
        g_console[consoleID - 1]->shellEntryId = taskID;
        LOS_SpinUnlockRestore(&g_consoleSpin, intSave);
        (VOID)OsSetCurrProcessGroupID(OS_USER_ROOT_PROCESS_ID);
        return LOS_OK;
    }
    LOS_SpinUnlockRestore(&g_consoleSpin, intSave);
    return (g_console[consoleID - 1]->shellEntryId == taskID) ? LOS_OK : LOS_NOK;
}

BOOL SetSerialNonBlock(const CONSOLE_CB *consoleCB)
{
    if (consoleCB == NULL) {
        PRINT_ERR("%s: Input parameter is illegal\n", __FUNCTION__);
        return FALSE;
    }
    return ioctl(consoleCB->fd, CONSOLE_CMD_RD_BLOCK_SERIAL, CONSOLE_RD_NONBLOCK) == 0;
}

BOOL SetSerialBlock(const CONSOLE_CB *consoleCB)
{
    if (consoleCB == NULL) {
        PRINT_ERR("%s: Input parameter is illegal\n", __FUNCTION__);
        return TRUE;
    }
    return ioctl(consoleCB->fd, CONSOLE_CMD_RD_BLOCK_SERIAL, CONSOLE_RD_BLOCK) != 0;
}

BOOL SetTelnetNonBlock(const CONSOLE_CB *consoleCB)
{
    if (consoleCB == NULL) {
        PRINT_ERR("%s: Input parameter is illegal\n", __FUNCTION__);
        return FALSE;
    }
    return ioctl(consoleCB->fd, CONSOLE_CMD_RD_BLOCK_TELNET, CONSOLE_RD_NONBLOCK) == 0;
}

BOOL SetTelnetBlock(const CONSOLE_CB *consoleCB)
{
    if (consoleCB == NULL) {
        PRINT_ERR("%s: Input parameter is illegal\n", __FUNCTION__);
        return TRUE;
    }
    return ioctl(consoleCB->fd, CONSOLE_CMD_RD_BLOCK_TELNET, CONSOLE_RD_BLOCK) != 0;
}

BOOL is_nonblock(const CONSOLE_CB *consoleCB)
{
    if (consoleCB == NULL) {
        PRINT_ERR("%s: Input parameter is illegal\n", __FUNCTION__);
        return FALSE;
    }
    return consoleCB->isNonBlock;
}

INT32 ConsoleUpdateFd(VOID)
{
    INT32 consoleID;

    if (OsCurrTaskGet() != NULL) {
        consoleID = g_taskConsoleIDArray[(OsCurrTaskGet())->taskID];
    } else {
        return -1;
    }

    if (g_uart_fputc_en == 0) {
        if (g_console[CONSOLE_TELNET - 1] != NULL) {
            consoleID = CONSOLE_TELNET;
        }
    } else if (consoleID == 0) {
        if (g_console[CONSOLE_SERIAL - 1] != NULL) {
            consoleID = CONSOLE_SERIAL;
        } else if (g_console[CONSOLE_TELNET - 1] != NULL) {
            consoleID = CONSOLE_TELNET;
        } else {
            PRINTK("No console dev used.\n");
            return -1;
        }
    }

    return (g_console[consoleID - 1] != NULL) ? g_console[consoleID - 1]->fd : -1;
}

CONSOLE_CB *OsGetConsoleByID(INT32 consoleID)
{
    if (consoleID != CONSOLE_TELNET) {
        consoleID = CONSOLE_SERIAL;
    }
    return g_console[consoleID - 1];
}

CONSOLE_CB *OsGetConsoleByTaskID(UINT32 taskID)
{
    INT32 consoleID = g_taskConsoleIDArray[taskID];

    return OsGetConsoleByID(consoleID);
}

VOID OsSetConsoleID(UINT32 newTaskID, UINT32 curTaskID)
{
    if ((newTaskID >= LOSCFG_BASE_CORE_TSK_LIMIT) || (curTaskID >= LOSCFG_BASE_CORE_TSK_LIMIT)) {
        return;
    }

    g_taskConsoleIDArray[newTaskID] = g_taskConsoleIDArray[curTaskID];
}

STATIC ssize_t WriteToTerminal(const CONSOLE_CB *consoleCB, const CHAR *buffer, size_t bufLen)
{
    INT32 ret, fd;
    INT32 cnt = 0;
    struct file *privFilep = NULL;
    struct file *filep = NULL;
    const struct file_operations_vfs *fileOps = NULL;

    fd = consoleCB->fd;
    ret = fs_getfilep(fd, &filep);
    if (ret < 0) {
        ret = -EPERM;
        goto ERROUT;
    }
    ret = GetFilepOps(filep, &privFilep, &fileOps);
    if (ret != ENOERR) {
        ret = -EINVAL;
        goto ERROUT;
    }

    if ((fileOps == NULL) || (fileOps->write == NULL)) {
        ret = EFAULT;
        goto ERROUT;
    }
    (VOID)fileOps->write(privFilep, buffer, bufLen);

    return cnt;

ERROUT:
    set_errno(ret);
    return VFS_ERROR;
}

STATIC UINT32 ConsoleSendTask(UINTPTR param)
{
    CONSOLE_CB *consoleCB = (CONSOLE_CB *)param;
    CirBufSendCB *cirBufSendCB = consoleCB->cirBufSendCB;
    CirBuf *cirBufCB = &cirBufSendCB->cirBufCB;
    UINT32 ret, size;
    CHAR *buf = NULL;

    (VOID)LOS_EventWrite(&cirBufSendCB->sendEvent, CONSOLE_SEND_TASK_RUNNING);

    while (1) {
        ret = LOS_EventRead(&cirBufSendCB->sendEvent, CONSOLE_CIRBUF_EVENT | CONSOLE_SEND_TASK_EXIT,
                            LOS_WAITMODE_OR | LOS_WAITMODE_CLR, LOS_WAIT_FOREVER);
        if (ret == CONSOLE_CIRBUF_EVENT) {
            size =  LOS_CirBufUsedSize(cirBufCB);
            if (size == 0) {
                continue;
            }
            buf = (CHAR *)LOS_MemAlloc(m_aucSysMem1, size + 1);
            if (buf == NULL) {
                continue;
            }
            (VOID)memset_s(buf, size + 1, 0, size + 1);

            (VOID)LOS_CirBufRead(cirBufCB, buf, size);

            (VOID)WriteToTerminal(consoleCB, buf, size);
            (VOID)LOS_MemFree(m_aucSysMem1, buf);
        } else if (ret == CONSOLE_SEND_TASK_EXIT) {
            break;
        }
    }

    ConsoleCirBufDelete(cirBufSendCB);
    return LOS_OK;
}

#ifdef LOSCFG_KERNEL_SMP
VOID OsWaitConsoleSendTaskPend(UINT32 taskID)
{
    UINT32 i;
    CONSOLE_CB *console = NULL;
    LosTaskCB *taskCB = NULL;
    INT32 waitTime = 3000; /* 3000: 3 seconds */

    for (i = 0; i < CONSOLE_NUM; i++) {
        console = g_console[i];
        if (console == NULL) {
            continue;
        }

        if (OS_TID_CHECK_INVALID(console->sendTaskID)) {
            continue;
        }

        taskCB = OS_TCB_FROM_TID(console->sendTaskID);
        while ((waitTime > 0) && (taskCB->taskEvent == NULL) && (taskID != console->sendTaskID)) {
            LOS_Mdelay(1); /* 1: wait console task pend */
            --waitTime;
        }
    }
}

VOID OsWakeConsoleSendTask(VOID)
{
    UINT32 i;
    CONSOLE_CB *console = NULL;

    for (i = 0; i < CONSOLE_NUM; i++) {
        console = g_console[i];
        if (console == NULL) {
            continue;
        }

        if (console->cirBufSendCB != NULL) {
            (VOID)LOS_EventWrite(&console->cirBufSendCB->sendEvent, CONSOLE_CIRBUF_EVENT);
        }
    }
}
#endif

