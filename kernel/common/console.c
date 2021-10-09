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
/* Inter-module variable */ //模块间变量
extern UINT32 g_uart_fputc_en;
STATIC UINT32 ConsoleSendTask(UINTPTR param);

STATIC UINT8 g_taskConsoleIDArray[LOSCFG_BASE_CORE_TSK_LIMIT];//task 控制台ID池,同步task数量,理论上每个task都可以有一个自己的控制台
STATIC SPIN_LOCK_INIT(g_consoleSpin);//初始化控制台自旋锁

#define SHELL_ENTRYID_INVALID     0xFFFFFFFF	//默认值,SHELL_ENTRYID 一般为 任务ID
#define SHELL_TASK_PRIORITY       9		//shell 的优先级为 9
#define CONSOLE_CIRBUF_EVENT      0x02U	//控制台循环buffer事件	
#define CONSOLE_SEND_TASK_EXIT    0x04U	//控制台发送任务退出事件
#define CONSOLE_SEND_TASK_RUNNING 0x10U	//控制台发送任务正在执行事件

CONSOLE_CB *g_console[CONSOLE_NUM];//控制台全局变量,控制台是共用的, 默认为 2个
#define MIN(a, b) ((a) < (b) ? (a) : (b))

/*
 * acquire uart driver function and filep of /dev/console,
 * then store uart driver function in *filepOps
 * and store filep of /dev/console in *privFilep.
 */
/*
* 获取/dev/console的uart驱动函数和filep，
* 然后将 uart 驱动函数存储在 *filepOps 中
* 并将 /dev/console 的文件存储在 *privFilep 中。
*/
INT32 GetFilepOps(const struct file *filep, struct file **privFilep, const struct file_operations_vfs **filepOps)
{
    INT32 ret;

    if ((filep == NULL) || (filep->f_vnode == NULL) || (filep->f_vnode->data == NULL)) {
        ret = EINVAL;
        goto ERROUT;
    }
	//通过 is_private 查找控制台设备的文件（现在是 *privFile）
    /* to find console device's filep(now it is *privFilep) throught i_private */
    struct drv_data *drv = (struct drv_data *)filep->f_vnode->data;
    *privFilep = (struct file *)drv->priv;// file
    if (((*privFilep)->f_vnode == NULL) || ((*privFilep)->f_vnode->data == NULL)) {
        ret = EINVAL;
        goto ERROUT;
    }

    /* to find uart driver operation function throutht u.i_opss */

    drv = (struct drv_data *)(*privFilep)->f_vnode->data;

    *filepOps = (const struct file_operations_vfs *)drv->ops;//拿到串口驱动程序

    return ENOERR;
ERROUT:
    set_errno(ret);
    return VFS_ERROR;
}
//获取控制台 模式值 
INT32 ConsoleTcGetAttr(INT32 fd, struct termios *termios)
{
    struct file *filep = NULL;
    CONSOLE_CB *consoleCB = NULL;

    INT32 ret = fs_getfilep(fd, &filep);
    if (ret < 0) {
        return -EPERM;
    }

    consoleCB = (CONSOLE_CB *)filep->f_priv;
    if (consoleCB == NULL) {
        return -EFAULT;
    }

    (VOID)memcpy_s(termios, sizeof(struct termios), &consoleCB->consoleTermios, sizeof(struct termios));
    return LOS_OK;
}
//设置控制台 模式值 
INT32 ConsoleTcSetAttr(INT32 fd, INT32 actions, const struct termios *termios)
{
    struct file *filep = NULL;
    CONSOLE_CB *consoleCB = NULL;

    (VOID)actions;

    INT32 ret = fs_getfilep(fd, &filep);
    if (ret < 0) {
        return -EPERM;
    }

    consoleCB = (CONSOLE_CB *)filep->f_priv;
    if (consoleCB == NULL) {
        return -EFAULT;
    }

    (VOID)memcpy_s(&consoleCB->consoleTermios, sizeof(struct termios), termios, sizeof(struct termios));
    return LOS_OK;
}

STATIC UINT32 ConsoleRefcountGet(const CONSOLE_CB *consoleCB)
{
    return consoleCB->refCount;
}
//设置控制台引用次数
STATIC VOID ConsoleRefcountSet(CONSOLE_CB *consoleCB, BOOL flag)
{
    if (flag == TRUE) {
        ++(consoleCB->refCount);
    } else {
        --(consoleCB->refCount);
    }
}
//控制台是否被占用
BOOL IsConsoleOccupied(const CONSOLE_CB *consoleCB)
{
    if (ConsoleRefcountGet(consoleCB) != FALSE) {
        return TRUE;
    } else {
        return FALSE;
    }
}

STATIC INT32 ConsoleCtrlCaptureLine(CONSOLE_CB *consoleCB)
{
    struct termios consoleTermios = {0};
    UINT32 intSave;

    LOS_SpinLockSave(&g_consoleSpin, &intSave);
    (VOID)ConsoleTcGetAttr(consoleCB->fd, &consoleTermios);
    consoleTermios.c_lflag |= ICANON | ECHO;
    (VOID)ConsoleTcSetAttr(consoleCB->fd, 0, &consoleTermios);
    LOS_SpinUnlockRestore(&g_consoleSpin, intSave);

    return LOS_OK;
}

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
//获取控制台ID,(/dev/serial = 1, /dev/telnet = 2)
STATIC INT32 OsGetConsoleID(const CHAR *deviceName)
{
    if ((deviceName != NULL) &&
        (strlen(deviceName) == strlen(SERIAL)) &&
        (!strncmp(deviceName, SERIAL, strlen(SERIAL)))) {
        return CONSOLE_SERIAL;//1 串口
    }
#ifdef LOSCFG_NET_TELNET
    else if ((deviceName != NULL) &&
             (strlen(deviceName) == strlen(TELNET)) &&
             (!strncmp(deviceName, TELNET, strlen(TELNET)))) {
        return CONSOLE_TELNET;//2 远程登录
    }
#endif
    return -1;
}
//通过路径找到控制台ID
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
    if (console->fifoOut == console->fifoIn) {
        return TRUE;
    }
    return FALSE;
}

STATIC VOID ConsoleFifoClearup(CONSOLE_CB *console)
{
    console->fifoOut = 0;
    console->fifoIn = 0;
    (VOID)memset_s(console->fifo, CONSOLE_FIFO_SIZE, 0, CONSOLE_FIFO_SIZE);
}
//控制台buf长度更新
STATIC VOID ConsoleFifoLenUpdate(CONSOLE_CB *console)
{
    console->currentLen = console->fifoIn - console->fifoOut;
}
//读取
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
//打开串口或远程登录
INT32 FilepOpen(struct file *filep, const struct file_operations_vfs *fops)
{
    INT32 ret;
    if (fops->open == NULL) {
        return -EFAULT;
    }
	//使用 uart open函数打开filep（filep是对应 /dev/console 的 filep)
    /*
     * adopt uart open function to open filep (filep is
     * corresponding to filep of /dev/console)
     */
    ret = fops->open(filep);
    if (ret < 0) {
        return -EPERM;
    }
    return ret;
}
//向控制台buf中写入结束字符
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
//根据VT终端标准 <ESC>[37m 为设置前景色
enum {
    STAT_NOMAL_KEY,	//普通按键
    STAT_ESC_KEY,	//控制按键,只有 ESC 是
    STAT_MULTI_KEY	//多个按键,只有 [ 是
};
//用户shell检查上下左右键
STATIC INT32 UserShellCheckUDRL(const CHAR ch, INT32 *lastTokenType)
{
    INT32 ret = LOS_OK;
    if (ch == 0x1b) { /* 0x1b: ESC */
        *lastTokenType = STAT_ESC_KEY;// vt 的控制字符
        return ret;
    } else if (ch == 0x5b) { /* 0x5b: first Key combination */
        if (*lastTokenType == STAT_ESC_KEY) { //遇到 <ESC>[
            *lastTokenType = STAT_MULTI_KEY;
            return ret;
        }
    } else if (ch == 0x41) { /* up */
        if (*lastTokenType == STAT_MULTI_KEY) {
            *lastTokenType = STAT_NOMAL_KEY;
            return ret;
        }
    } else if (ch == 0x42) { /* down */
        if (*lastTokenType == STAT_MULTI_KEY) {
            *lastTokenType = STAT_NOMAL_KEY;
            return ret;
        }
    } else if (ch == 0x43) { /* right */
        if (*lastTokenType == STAT_MULTI_KEY) {
            *lastTokenType = STAT_NOMAL_KEY;
            return ret;
        }
    } else if (ch == 0x44) { /* left */
        if (*lastTokenType == STAT_MULTI_KEY) {
            *lastTokenType = STAT_NOMAL_KEY;
            return ret;
        }
    }
    return LOS_NOK;
}
//是否需要继续
STATIC INT32 IsNeedContinue(CONSOLE_CB *consoleCB, char ch, INT32 *lastTokenType)
{
    if (((ch == '\b') && (consoleCB->consoleTermios.c_lflag & ECHO) && (ConsoleFifoEmpty(consoleCB))) ||
        (UserShellCheckUDRL(ch, lastTokenType) == LOS_OK)) { /* parse the up/down/right/left key */
        return LOS_NOK;
    }

    return LOS_OK;
}
//输出到终端
STATIC VOID EchoToTerminal(CONSOLE_CB *consoleCB, struct file *filep, const struct file_operations_vfs *fops, char ch)
{
    if (consoleCB->consoleTermios.c_lflag & ECHO) {
        if (ch == '\b') {//遇到回退字符
            (VOID)fops->write(filep, "\b \b", 3);//回退
        } else {
            (VOID)fops->write(filep, &ch, EACH_CHAR);//向终端写入字符
        }
    }
}
//存储读取的字符
STATIC VOID StoreReadChar(CONSOLE_CB *consoleCB, char ch, INT32 readcount)
{	//读取字符
    if ((readcount == EACH_CHAR) && (consoleCB->fifoIn <= (CONSOLE_FIFO_SIZE - 3))) {
        if (ch == '\b') {
            if (!ConsoleFifoEmpty(consoleCB)) {
                consoleCB->fifo[--consoleCB->fifoIn] = '\0';
            }
        } else {
            consoleCB->fifo[consoleCB->fifoIn] = (UINT8)ch;//将字符读入缓冲区
            consoleCB->fifoIn++;
        }
    }
}
//杀死进程组
VOID KillPgrp()
{
    INT32 consoleId;
    LosProcessCB *process = OsCurrProcessGet();//获取当前进程

    if ((process->consoleID > CONSOLE_NUM -1 ) || (process->consoleID < 0)) {
        return;
    }

    consoleId = process->consoleID;
    CONSOLE_CB *consoleCB = g_console[consoleId];
    (VOID)OsKillLock(consoleCB->pgrpId, SIGINT);//发送信号 SIGINT对应 键盘中断（ctrl + c）信号
}
//用户使用参数buffer将控制台的buf接走
STATIC INT32 UserFilepRead(CONSOLE_CB *consoleCB, struct file *filep, const struct file_operations_vfs *fops,
                           CHAR *buffer, size_t bufLen)
{
    INT32 ret;
    INT32 needreturn = LOS_NOK;
    CHAR ch;
    INT32 lastTokenType = STAT_NOMAL_KEY;

    if (fops->read == NULL) {
        return -EFAULT;
    }

    /* Non-ICANON mode */
    if ((consoleCB->consoleTermios.c_lflag & ICANON) == 0) {
        ret = fops->read(filep, buffer, bufLen);
        if (ret < 0) {
            return -EPERM;
        }
        return ret;
    }
    /* ICANON mode: store data to console buffer,  read data and stored data into console fifo */
    if (consoleCB->currentLen == 0) {//如果没有数据
        while (1) {//存储数据到控制台buf中
            ret = fops->read(filep, &ch, EACH_CHAR);//一个个字符读
            if (ret <= 0) {
                return ret;
            }

            if (IsNeedContinue(consoleCB, ch, &lastTokenType))//是否继续读
                continue;

            switch (ch) {
                case '\r':
                    ch = '\n';//回车换行
                case '\n':
                    EchoToTerminal(consoleCB, filep, fops, ch);//输出到终端
                    UserEndOfRead(consoleCB, filep, fops);//给控制台buf加上结束字符
                    ret = ConsoleReadFifo(buffer, consoleCB, bufLen);//读取控制台buf的信息到参数buffer中
                    needreturn = LOS_OK;//直接返回
                    break;
                case '\b':
                default:
                    EchoToTerminal(consoleCB, filep, fops, ch);//输出到终端
                    StoreReadChar(consoleCB, ch, ret);//将字符保存到控制台的buf中
                    break;
            }

            if (needreturn == LOS_OK)
                break;
        }
    } else {//如果数据准备好了,立即返回
        /* if data is already in console fifo, we returen them immediately */
        ret = ConsoleReadFifo(buffer, consoleCB, bufLen);//读取控制台buf的信息到参数buffer中
    }

    return ret;
}
//从串口或远程登录中读数据
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
    if (ret < 0) {
        return -EPERM;
    }
    return ret;
}
//写数据到串口或远程登录
INT32 FilepWrite(struct file *filep, const struct file_operations_vfs *fops, const CHAR *buffer, size_t bufLen)
{
    INT32 ret;
    if (fops->write == NULL) {
        return -EFAULT;
    }

    ret = fops->write(filep, buffer, bufLen);
    if (ret < 0) {
        return -EPERM;
    }
    return ret;
}
//关闭串口或远程登录
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
    if (ret < 0) {
        return -EPERM;
    }
    return ret;
}

INT32 FilepIoctl(struct file *filep, const struct file_operations_vfs *fops, INT32 cmd, unsigned long arg)
{
    INT32 ret;
    if (fops->ioctl == NULL) {
        return -EFAULT;
    }

    ret = fops->ioctl(filep, cmd, arg);
    if (ret < 0) {
        return -EPERM;
    }
    return ret;
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
    if (ret < 0) {
        return -EPERM;
    }
    return ret;
}
//对 file_operations_vfs->open 的实现函数,也就是说这是 打开控制台的实体函数.
STATIC INT32 ConsoleOpen(struct file *filep)
{
    INT32 ret;
    UINT32 consoleID;
    struct file *privFilep = NULL;
    const struct file_operations_vfs *fileOps = NULL;

    consoleID = (UINT32)OsConsoleFullpathToID(filep->f_path);//先找到控制台ID返回 (1,2,-1)
    if (consoleID == (UINT32)-1) {
        ret = EPERM;
        goto ERROUT;
    }
    filep->f_priv = g_console[consoleID - 1]; //f_priv 每种文件系统对应结构体都同,所以是void的类型,如一张白纸,画什么模块自己定

    ret = GetFilepOps(filep, &privFilep, &fileOps);//获取文件系统的驱动程序
    if (ret != ENOERR) {
        ret = EINVAL;
        goto ERROUT;
    }
    ret = FilepOpen(privFilep, fileOps);//打开文件,其实调用的是 串口底层驱动程序  
    if (ret < 0) {
        ret = EPERM;
        goto ERROUT;
    }
    return ENOERR;

ERROUT:
    set_errno(ret);
    return VFS_ERROR;
}
//关闭控制台
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
//用户任务从控制台读数据
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
	//检查buffer是否在用户区
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

    ret = DoRead(consoleCB, sbuffer, bufLen, privFilep, fileOps);//真正的读数据
    if (ret < 0) {
        goto ERROUT;
    }

    if (userBuf) {
        if (LOS_ArchCopyToUser(buffer, sbuffer, bufLen) != 0) {
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
    size_t writen = 0;
    size_t toWrite = bufLen;
    UINT32 intSave;

#ifdef LOSCFG_SHELL_DMESG
    (VOID)OsLogMemcpyRecord(buffer, bufLen);
    if (OsCheckConsoleLock()) {
        return 0;
    }
#endif
    LOS_CirBufLock(&cirBufSendCB->cirBufCB, &intSave);
    while (writen < (INT32)bufLen) {
        /* Transform for CR/LR mode */
        if ((buffer[writen] == '\n') || (buffer[writen] == '\r')) {
            (VOID)LOS_CirBufWrite(&cirBufSendCB->cirBufCB, "\r", 1);
        }

        cnt = LOS_CirBufWrite(&cirBufSendCB->cirBufCB, &buffer[writen], 1);
        if (cnt <= 0) {
            break;
        }
        toWrite -= cnt;
        writen += cnt;
    }
    LOS_CirBufUnlock(&cirBufSendCB->cirBufCB, intSave);
    /* Log is cached but not printed when a system exception occurs */
    if (OsGetSystemStatus() == OS_SYSTEM_NORMAL) {
        (VOID)LOS_EventWrite(&cirBufSendCB->sendEvent, CONSOLE_CIRBUF_EVENT);//发送数据事件
    }

    return writen;
}
//用户任务写数据到控制台
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
	//检测buffer是否在用户空间
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
    ret = DoWrite(cirBufSendCB, sbuffer, bufLen);//真正的写操作

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

    if (LOS_CopyFromKernel((VOID *)arg, sizeof(struct winsize), &kws, sizeof(struct winsize)) != 0) {
        return -EFAULT;
    }
    return LOS_OK;
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

    if(LOS_ArchCopyToUser((VOID *)arg, &consoleCB->consoleTermios, sizeof(struct termios)) != 0) {
        return -EFAULT;
    } else {
        return LOS_OK;
    }
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
    if (LOS_ArchCopyToUser((VOID *)arg, &consoleCB->pgrpId, sizeof(INT32)) != 0) {
        return -EFAULT;
    }
    return LOS_OK;
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

/* console device driver function structure */ //控制台设备驱动程序,对统一的vfs接口的实现
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
/*
termios 结构是在POSIX规范中定义的标准接口，它类似于系统V中的termio接口，
通过设置termios类型的数据结构中的值和使用一小组函数调用，你就可以对终端接口进行控制。
*/
STATIC VOID OsConsoleTermiosInit(CONSOLE_CB *consoleCB, const CHAR *deviceName)
{
    struct termios consoleTermios = {0};
//c_cc[VINTR] 默认对应的控制符是^C，作用是清空输入和输出队列的数据并且向tty设备的前台进程组中的
//每一个程序发送一个SIGINT信号，对SIGINT信号没有定义处理程序的进程会马上退出。
    if ((deviceName != NULL) &&
        (strlen(deviceName) == strlen(SERIAL)) &&
        (!strncmp(deviceName, SERIAL, strlen(SERIAL)))) {
        consoleCB->isNonBlock = SetSerialBlock(consoleCB);

        /* set console to have a buffer for user */
        (VOID)ConsoleTcGetAttr(consoleCB->fd, &consoleTermios);
        consoleTermios.c_lflag |= ICANON | ECHO;//控制模式标志 ICANON:使用标准输入模式 ECHO:显示输入字符
        consoleTermios.c_cc[VINTR] = 3; /* /003 for ^C */ //控制字符 
        (VOID)ConsoleTcSetAttr(consoleCB->fd, 0, &consoleTermios);
    }
#ifdef LOSCFG_NET_TELNET
    else if ((deviceName != NULL) &&
             (strlen(deviceName) == strlen(TELNET)) &&
             (!strncmp(deviceName, TELNET, strlen(TELNET)))) {
        consoleCB->isNonBlock = SetTelnetBlock(consoleCB);
        /* set console to have a buffer for user */
        (VOID)ConsoleTcGetAttr(consoleCB->fd, &consoleTermios);
        consoleTermios.c_lflag |= ICANON | ECHO;//控制模式标志 ICANON:使用标准输入模式 ECHO:显示输入字符
        consoleTermios.c_cc[VINTR] = 3; /* /003 for ^C */ //控制字符
        (VOID)ConsoleTcSetAttr(consoleCB->fd, 0, &consoleTermios);
    }
#endif
}
//控制台文件实例初始化
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
    filep->ops = (struct file_operations_vfs *)((struct drv_data *)vnode->data)->ops;//关联驱动程序
    consoleCB->fd = filep->fd;

ERROUT:
    VnodeDrop();
    return ret;
}

/*
 * Initialized console control platform so that when we operate /dev/console
 * as if we are operating /dev/ttyS0 (uart0).
 *///初始化控制台以此来控制平台，以便在操作/dev/console时，就好像我们在操作/dev/ttyS0（uart0）
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
    ret = VnodeLookup(deviceName, &vnode, V_DUMMY);	//找到对应 vnode节点
    VnodeDrop(); // not correct, but can't fix perfectly here
    if (ret != LOS_OK) {
        ret = EACCES;
        PRINTK("!! can not find %s\n", consoleCB->name);
        goto ERROUT;
    }

    consoleCB->devVnode = vnode;//关联vnode节点

    /*
     * initialize the console filep which is associated with /dev/console,
     * assign the uart0 inode of /dev/ttyS0 to console inod of /dev/console,
     * then we can operate console's filep as if we operate uart0 filep of
     * /dev/ttyS0.
     */
     /*
     初始化与/dev/console关联的console filep，将/dev/ttyS0的uart0 inode赋给
     /dev/console的console inod，就可以像操作/dev/ttyS0的uart0 filep一样操作控制台的filep
     */
    (VOID)memset_s(filep, sizeof(struct file), 0, sizeof(struct file));
    filep->f_oflags = O_RDWR;	//读写模式
    filep->f_pos = 0;	//偏移位置,默认为0
    filep->f_vnode = vnode;	//节点关联
    filep->f_path = NULL;	//
    filep->f_priv = NULL;	//
    /*
     * Use filep to connect console and uart, we can find uart driver function throught filep.
     * now we can operate /dev/console to operate /dev/ttyS0 through filep.
     */
     //使用filep连接控制台和uart，通过它可以找到uart驱动函数, 可以通过filep操作/dev/console
    devOps = (struct file_operations_vfs *)((struct drv_data*)vnode->data)->ops;//获取默认驱动程序
    if (devOps != NULL && devOps->open != NULL) {
        (VOID)devOps->open(filep);
    } else {
        ret = ENOSYS;
        goto ERROUT;
    }

    //更新驱动程序,其中再次对 达到操作/dev/ttyS0的目的
    ret = register_driver(consoleCB->name, &g_consoleDevOps, DEFFILEMODE, filep);//注册字符设备驱动程序
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
//控制台设备去初始化
STATIC UINT32 OsConsoleDevDeinit(const CONSOLE_CB *consoleCB)
{
    return unregister_driver(consoleCB->name);//注销驱动
}

//创建一个控制台循环buf
STATIC CirBufSendCB *ConsoleCirBufCreate(VOID)
{
    UINT32 ret;
    CHAR *fifo = NULL;
    CirBufSendCB *cirBufSendCB = NULL;
    CirBuf *cirBufCB = NULL;

    cirBufSendCB = (CirBufSendCB *)LOS_MemAlloc(m_aucSysMem0, sizeof(CirBufSendCB));//分配一个循环buf发送控制块
    if (cirBufSendCB == NULL) {
        return NULL;
    }
    (VOID)memset_s(cirBufSendCB, sizeof(CirBufSendCB), 0, sizeof(CirBufSendCB));

    fifo = (CHAR *)LOS_MemAlloc(m_aucSysMem0, CONSOLE_CIRCBUF_SIZE);//分配FIFO buf 1K 
    if (fifo == NULL) {
        goto ERROR_WITH_SENDCB;
    }
    (VOID)memset_s(fifo, CONSOLE_CIRCBUF_SIZE, 0, CONSOLE_CIRCBUF_SIZE);

    cirBufCB = &cirBufSendCB->cirBufCB;
    ret = LOS_CirBufInit(cirBufCB, fifo, CONSOLE_CIRCBUF_SIZE);//环形BUF初始化
    if (ret != LOS_OK) {
        goto ERROR_WITH_FIFO;
    }

    (VOID)LOS_EventInit(&cirBufSendCB->sendEvent);//事件初始化
    return cirBufSendCB;

ERROR_WITH_FIFO:
    (VOID)LOS_MemFree(m_aucSysMem0, cirBufCB->fifo);
ERROR_WITH_SENDCB:
    (VOID)LOS_MemFree(m_aucSysMem0, cirBufSendCB);
    return NULL;
}
//删除循环buf
STATIC VOID ConsoleCirBufDelete(CirBufSendCB *cirBufSendCB)
{
    CirBuf *cirBufCB = &cirBufSendCB->cirBufCB;

    (VOID)LOS_MemFree(m_aucSysMem0, cirBufCB->fifo);//释放内存 8K
    LOS_CirBufDeinit(cirBufCB);//清除初始化操作
    (VOID)LOS_EventDestroy(&cirBufSendCB->sendEvent);//销毁事件
    (VOID)LOS_MemFree(m_aucSysMem0, cirBufSendCB);//释放循环buf发送控制块
}
//控制台缓存初始化，创建一个 发送任务
STATIC UINT32 OsConsoleBufInit(CONSOLE_CB *consoleCB)
{
    UINT32 ret;
    TSK_INIT_PARAM_S initParam = {0};

    consoleCB->cirBufSendCB = ConsoleCirBufCreate();//创建控制台
    if (consoleCB->cirBufSendCB == NULL) {
        return LOS_NOK;
    }

    initParam.pfnTaskEntry = (TSK_ENTRY_FUNC)ConsoleSendTask;//控制台发送任务入口函数
    initParam.usTaskPrio   = SHELL_TASK_PRIORITY;	//优先级9
    initParam.auwArgs[0]   = (UINTPTR)consoleCB;	//入口函数的参数
    initParam.uwStackSize  = LOSCFG_BASE_CORE_TSK_DEFAULT_STACK_SIZE;	//16K
    if (consoleCB->consoleID == CONSOLE_SERIAL) {//控制台的两种方式
        initParam.pcName   = "SendToSer";	//发送数据到串口 
    } else {
        initParam.pcName   = "SendToTelnet";//发送数据到远程登录
    }
    initParam.uwResved     = LOS_TASK_STATUS_DETACHED; //使用任务分离模式

    ret = LOS_TaskCreate(&consoleCB->sendTaskID, &initParam);//创建task 并加入就绪队列，申请立即调度
    if (ret != LOS_OK) { //创建失败处理
        ConsoleCirBufDelete(consoleCB->cirBufSendCB);//释放循环buf
        consoleCB->cirBufSendCB = NULL;//置NULL
        return LOS_NOK;
    }//永久等待读取 CONSOLE_SEND_TASK_RUNNING 事件,CONSOLE_SEND_TASK_RUNNING 由 ConsoleSendTask 发出.
    (VOID)LOS_EventRead(&consoleCB->cirBufSendCB->sendEvent, CONSOLE_SEND_TASK_RUNNING,
                        LOS_WAITMODE_OR | LOS_WAITMODE_CLR, LOS_WAIT_FOREVER);

    return LOS_OK;
}
//控制台buf去初始化
STATIC VOID OsConsoleBufDeinit(CONSOLE_CB *consoleCB)
{
    CirBufSendCB *cirBufSendCB = consoleCB->cirBufSendCB;

    consoleCB->cirBufSendCB = NULL;
    (VOID)LOS_EventWrite(&cirBufSendCB->sendEvent, CONSOLE_SEND_TASK_EXIT);//写任务退出事件 ConsoleSendTask将会收到事件，退出死循环
}
//控制台描述符初始化
STATIC CONSOLE_CB *OsConsoleCBInit(UINT32 consoleID)
{
    CONSOLE_CB *consoleCB = (CONSOLE_CB *)LOS_MemAlloc((VOID *)m_aucSysMem0, sizeof(CONSOLE_CB));//内核空间分配控制台描述符
    if (consoleCB == NULL) {
        return NULL;
    }
    (VOID)memset_s(consoleCB, sizeof(CONSOLE_CB), 0, sizeof(CONSOLE_CB));//清0

    consoleCB->consoleID = consoleID;//记录控制台ID
    consoleCB->pgrpId = -1;
    consoleCB->shellEntryId = SHELL_ENTRYID_INVALID; /* initialize shellEntryId to an invalid value *///将shellEntryId初始化为无效值
    consoleCB->name = LOS_MemAlloc((VOID *)m_aucSysMem0, CONSOLE_NAMELEN);//控制台名称 不能多于16个字符
    if (consoleCB->name == NULL) {
        PRINT_ERR("consoleCB->name malloc failed\n");
        (VOID)LOS_MemFree((VOID *)m_aucSysMem0, consoleCB);
        return NULL;
    }
    return consoleCB;
}
//释放控制台描述符初始化时所占用的内核空间
STATIC VOID OsConsoleCBDeinit(CONSOLE_CB *consoleCB)
{
    (VOID)LOS_MemFree((VOID *)m_aucSysMem0, consoleCB->name);//释放控制台名称占用的内核内存
    consoleCB->name = NULL;
    (VOID)LOS_MemFree((VOID *)m_aucSysMem0, consoleCB);//释放控制台描述符所占用的内核内存
}
//创建一个控制台，这个函数的goto语句贼多
STATIC CONSOLE_CB *OsConsoleCreate(UINT32 consoleID, const CHAR *deviceName)
{
    INT32 ret;
    CONSOLE_CB *consoleCB = OsConsoleCBInit(consoleID);//初始化控制台
    if (consoleCB == NULL) {
        PRINT_ERR("console malloc error.\n");
        return NULL;
    }

    ret = snprintf_s(consoleCB->name, CONSOLE_NAMELEN, CONSOLE_NAMELEN - 1,
                     "%s%u", CONSOLE, consoleCB->consoleID);//通过printf方式得到name
    if (ret == -1) {
        PRINT_ERR("consoleCB->name snprintf_s failed\n");
        goto ERR_WITH_NAME;
    }

    ret = (INT32)OsConsoleBufInit(consoleCB);//控制台buf初始化,创建 ConsoleSendTask 任务
    if (ret != LOS_OK) {
        PRINT_ERR("console OsConsoleBufInit error. %d\n", ret);
        goto ERR_WITH_NAME;
    }

    ret = (INT32)LOS_SemCreate(1, &consoleCB->consoleSem);//创建控制台信号量
    if (ret != LOS_OK) {
        PRINT_ERR("creat sem for uart failed\n");
        goto ERR_WITH_BUF;
    }

    ret = OsConsoleDevInit(consoleCB, deviceName);//控制台设备初始化,注意这步要在 OsConsoleFileInit 的前面.
    if (ret != LOS_OK) {//先注册驱动程序
        PRINT_ERR("console OsConsoleDevInit error. %d\n", ret);
        goto ERR_WITH_SEM;
    }

    ret = OsConsoleFileInit(consoleCB);	//控制台文件初始化,其中file要绑定驱动程序
    if (ret != LOS_OK) {
        PRINT_ERR("console OsConsoleFileInit error. %d\n", ret);
        goto ERR_WITH_DEV;
    }

    OsConsoleTermiosInit(consoleCB, deviceName);//控制台术语/控制初始化
    return consoleCB;

ERR_WITH_DEV:
    ret = (INT32)OsConsoleDevDeinit(consoleCB);//控制台设备去初始化
    if (ret != LOS_OK) {
        PRINT_ERR("OsConsoleDevDeinit failed!\n");
    }
ERR_WITH_SEM:
    (VOID)LOS_SemDelete(consoleCB->consoleSem);//控制台信号量删除
ERR_WITH_BUF:
    OsConsoleBufDeinit(consoleCB);//控制台buf取消初始化
ERR_WITH_NAME:
    OsConsoleCBDeinit(consoleCB);//控制块取消初始化
    return NULL;
}
//删除控制台
STATIC UINT32 OsConsoleDelete(CONSOLE_CB *consoleCB)
{
    UINT32 ret;

    (VOID)files_close(consoleCB->fd);//回收系统文件句柄
    ret = OsConsoleDevDeinit(consoleCB);//注销驱动程序
    if (ret != LOS_OK) {
        PRINT_ERR("OsConsoleDevDeinit failed!\n");
    }
    OsConsoleBufDeinit((CONSOLE_CB *)consoleCB);//回收环形缓存区
    (VOID)LOS_SemDelete(consoleCB->consoleSem);//删除信号量
    (VOID)LOS_MemFree(m_aucSysMem0, consoleCB->name);//回收控制台名称,此名称占用内核内存
    consoleCB->name = NULL;
    (VOID)LOS_MemFree(m_aucSysMem0, consoleCB);//回收控制块本身占用的内存

    return ret;
}
//初始化系统控制台并返回 stdinfd stdoutfd stderrfd ，和system_console_deinit成对出现，像控制台的构造函数
/* Initialized system console and return stdinfd stdoutfd stderrfd */
INT32 system_console_init(const CHAR *deviceName)//deviceName: /dev/serial /dev/telnet
{
#ifdef LOSCFG_SHELL
    UINT32 ret;
#endif
    INT32 consoleID;
    UINT32 intSave;
    CONSOLE_CB *consoleCB = NULL;

    consoleID = OsGetConsoleID(deviceName);//获取控制台ID 返回[ CONSOLE_SERIAL(1) | CONSOLE_TELNET(2) | -1 ]三种结果
    if (consoleID == -1) {
        PRINT_ERR("device is full.\n");
        return VFS_ERROR;
    }

    consoleCB = OsConsoleCreate((UINT32)consoleID, deviceName);//创建一个控制台控制块
    if (consoleCB == NULL) {
        PRINT_ERR("%s, %d\n", __FUNCTION__, __LINE__);
        return VFS_ERROR;
    }

    LOS_SpinLockSave(&g_consoleSpin, &intSave);
    g_console[consoleID - 1] = consoleCB;//全局变量,  g_console最大值只有2 ,代表当前0,1指向哪个任务.
    if (OsCurrTaskGet() != NULL) {//当前task
        g_taskConsoleIDArray[OsCurrTaskGet()->taskID] = (UINT8)consoleID;//任务绑定控制台ID
    }
    LOS_SpinUnlockRestore(&g_consoleSpin, intSave);

#ifdef LOSCFG_SHELL //shell支持
    ret = OsShellInit(consoleID);//初始化shell
    if (ret != LOS_OK) {//初始化shell失败
        PRINT_ERR("%s, %d\n", __FUNCTION__, __LINE__);
        LOS_SpinLockSave(&g_consoleSpin, &intSave);
        (VOID)OsConsoleDelete(consoleCB);//删除控制台
        g_console[consoleID - 1] = NULL;
        g_taskConsoleIDArray[OsCurrTaskGet()->taskID] = 0;//表示当前任务还没有控制台。
        LOS_SpinUnlockRestore(&g_consoleSpin, intSave);
        return VFS_ERROR;
    }
#endif

    return ENOERR;
}
//控制台结束前的处理 和 system_console_init成对出现,像控制台的析构函数
INT32 system_console_deinit(const CHAR *deviceName)
{
    UINT32 ret;
    CONSOLE_CB *consoleCB = NULL;
    UINT32 taskIdx;
    LosTaskCB *taskCB = NULL;
    UINT32 intSave;

    consoleCB = OsGetConsoleByDevice(deviceName);//通过设备名称获取控制台描述符
    if (consoleCB == NULL) {
        return VFS_ERROR;
    }

#ifdef LOSCFG_SHELL
    (VOID)OsShellDeinit((INT32)consoleCB->consoleID);//shell结束前的处理，shell的析构函数
#endif

    LOS_SpinLockSave(&g_consoleSpin, &intSave);
    /* Redirect all tasks to serial as telnet was unavailable after deinitializing */
	//在远程登陆去初始化后变成无效时，将所有任务的控制台重定向到串口方式。
    for (taskIdx = 0; taskIdx < g_taskMaxNum; taskIdx++) {//这里是对所有的任务控制台方式设为串口化
        taskCB = ((LosTaskCB *)g_taskCBArray) + taskIdx;
        if (OsTaskIsUnused(taskCB)) {//任务还没被使用过
            continue;//继续
        } else {
            g_taskConsoleIDArray[taskCB->taskID] = CONSOLE_SERIAL;//任务对于的控制台变成串口方式
        }
    }
    g_console[consoleCB->consoleID - 1] = NULL;
    LOS_SpinUnlockRestore(&g_consoleSpin, intSave);

    ret = OsConsoleDelete(consoleCB);//删除控制台
    if (ret != LOS_OK) {
        PRINT_ERR("%s, Failed to system_console_deinit\n", __FUNCTION__);
        return VFS_ERROR;
    }

    return ENOERR;
}
//控制台使能
BOOL ConsoleEnable(VOID)
{
    INT32 consoleID;

    if (OsCurrTaskGet() != NULL) {//有当前任务的情况下
        consoleID = g_taskConsoleIDArray[OsCurrTaskGet()->taskID];//获取当前任务的控制台ID
        if (g_uart_fputc_en == 0) {
            if ((g_console[CONSOLE_TELNET - 1] != NULL) && OsPreemptable()) {
                return TRUE;
            } else {
                return FALSE;
            }
        }

        if (consoleID == 0) {
            return FALSE;
        } else if ((consoleID == CONSOLE_TELNET) || (consoleID == CONSOLE_SERIAL)) {
            if ((OsGetSystemStatus() == OS_SYSTEM_NORMAL) && !OsPreemptable()) {
                return FALSE;
            }
            return TRUE;
        }
#if defined (LOSCFG_DRIVERS_USB_SERIAL_GADGET) || defined (LOSCFG_DRIVERS_USB_ETH_SER_GADGET)
        else if ((SerialTypeGet() == SERIAL_TYPE_USBTTY_DEV) && (userial_mask_get() == 1)) {
            return TRUE;
        }
#endif
    }

    return FALSE;
}
//任务注册控制台,每个shell任务都有属于自己的控制台
INT32 ConsoleTaskReg(INT32 consoleID, UINT32 taskID)
{
    if (g_console[consoleID - 1]->shellEntryId == SHELL_ENTRYID_INVALID) {
        g_console[consoleID - 1]->shellEntryId = taskID;	//ID捆绑
        (VOID)OsSetCurrProcessGroupID(OsGetUserInitProcessID());// @notethinking 为何要在此处设置当前进程的组ID?
        return LOS_OK;
    }

    return LOS_NOK;
}
//无锁方式设置串口
BOOL SetSerialNonBlock(const CONSOLE_CB *consoleCB)
{
    INT32 ret;

    if (consoleCB == NULL) {
        PRINT_ERR("%s: Input parameter is illegal\n", __FUNCTION__);
        return FALSE;
    }
    ret = ioctl(consoleCB->fd, CONSOLE_CMD_RD_BLOCK_SERIAL, CONSOLE_RD_NONBLOCK);
    if (ret != 0) {
        return FALSE;
    }

    return TRUE;
}
//锁方式设置串口
BOOL SetSerialBlock(const CONSOLE_CB *consoleCB)
{
    INT32 ret;

    if (consoleCB == NULL) {
        PRINT_ERR("%s: Input parameter is illegal\n", __FUNCTION__);
        return TRUE;
    }
    ret = ioctl(consoleCB->fd, CONSOLE_CMD_RD_BLOCK_SERIAL, CONSOLE_RD_BLOCK);
    if (ret != 0) {
        return TRUE;
    }

    return FALSE;
}
//无锁方式设置远程登录
BOOL SetTelnetNonBlock(const CONSOLE_CB *consoleCB)
{
    INT32 ret;

    if (consoleCB == NULL) {
        PRINT_ERR("%s: Input parameter is illegal\n", __FUNCTION__);
        return FALSE;
    }
    ret = ioctl(consoleCB->fd, CONSOLE_CMD_RD_BLOCK_TELNET, CONSOLE_RD_NONBLOCK);
    if (ret != 0) {
        return FALSE;
    }
    return TRUE;
}
//锁方式设置远程登录
BOOL SetTelnetBlock(const CONSOLE_CB *consoleCB)
{
    INT32 ret;

    if (consoleCB == NULL) {
        PRINT_ERR("%s: Input parameter is illegal\n", __FUNCTION__);
        return TRUE;
    }
    ret = ioctl(consoleCB->fd, CONSOLE_CMD_RD_BLOCK_TELNET, CONSOLE_RD_BLOCK);
    if (ret != 0) {
        return TRUE;
    }
    return FALSE;
}

BOOL is_nonblock(const CONSOLE_CB *consoleCB)
{
    if (consoleCB == NULL) {
        PRINT_ERR("%s: Input parameter is illegal\n", __FUNCTION__);
        return FALSE;
    }
    return consoleCB->isNonBlock;
}
//控制台更新文件句柄
INT32 ConsoleUpdateFd(VOID)
{
    INT32 consoleID;

    if (OsCurrTaskGet() != NULL) {
        consoleID = g_taskConsoleIDArray[(OsCurrTaskGet())->taskID];//获取当前任务的控制台 (1,2,-1)
    } else {
        return -1;
    }

    if (g_uart_fputc_en == 0) {
        if (g_console[CONSOLE_TELNET - 1] != NULL) {
            consoleID = CONSOLE_TELNET;
        } else {
            return -1;
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

    if (g_console[consoleID - 1] == NULL) {
        return -1;
    }

    return g_console[consoleID - 1]->fd;
}
//获取参数控制台ID 获取对应的控制台控制块(描述符)
CONSOLE_CB *OsGetConsoleByID(INT32 consoleID)
{
    if (consoleID != CONSOLE_TELNET) {//只允许 1,2存在，> 3 时统统变成1
        consoleID = CONSOLE_SERIAL;
    }
    return g_console[consoleID - 1];
}
//获取参数任务的控制台控制块(描述符)
CONSOLE_CB *OsGetConsoleByTaskID(UINT32 taskID)
{
    INT32 consoleID = g_taskConsoleIDArray[taskID];

    return OsGetConsoleByID(consoleID);
}
//设置控制台ID
VOID OsSetConsoleID(UINT32 newTaskID, UINT32 curTaskID)
{
    if ((newTaskID >= LOSCFG_BASE_CORE_TSK_LIMIT) || (curTaskID >= LOSCFG_BASE_CORE_TSK_LIMIT)) {
        return;
    }

    g_taskConsoleIDArray[newTaskID] = g_taskConsoleIDArray[curTaskID];
}
//将buf内容写到终端设备
STATIC ssize_t WriteToTerminal(const CONSOLE_CB *consoleCB, const CHAR *buffer, size_t bufLen)
{
    INT32 ret, fd;
    INT32 cnt = 0;
    struct file *privFilep = NULL;
    struct file *filep = NULL;
    const struct file_operations_vfs *fileOps = NULL;

    fd = consoleCB->fd;//获取文件描述符
    ret = fs_getfilep(fd, &filep);//获取文件指针
    ret = GetFilepOps(filep, &privFilep, &fileOps);//获取文件的操作方法

    if ((fileOps == NULL) || (fileOps->write == NULL)) {
        ret = EFAULT;
        goto ERROUT;
    }
    (VOID)fileOps->write(privFilep, buffer, bufLen);//写入文件,控制台的本质就是一个文件，这里写入文件就是在终端设备上呈现出来

    return cnt;

ERROUT:
    set_errno(ret);
    return VFS_ERROR;
}
//控制台发送任务
STATIC UINT32 ConsoleSendTask(UINTPTR param)
{
    CONSOLE_CB *consoleCB = (CONSOLE_CB *)param;
    CirBufSendCB *cirBufSendCB = consoleCB->cirBufSendCB;
    CirBuf *cirBufCB = &cirBufSendCB->cirBufCB;
    UINT32 ret, size;
    UINT32 intSave;
    CHAR *buf = NULL;

    (VOID)LOS_EventWrite(&cirBufSendCB->sendEvent, CONSOLE_SEND_TASK_RUNNING);//发送一个控制台任务正在运行的事件

    while (1) {//读取 CONSOLE_CIRBUF_EVENT | CONSOLE_SEND_TASK_EXIT 这两个事件
        ret = LOS_EventRead(&cirBufSendCB->sendEvent, CONSOLE_CIRBUF_EVENT | CONSOLE_SEND_TASK_EXIT,
                            LOS_WAITMODE_OR | LOS_WAITMODE_CLR, LOS_WAIT_FOREVER);//读取循环buf或任务退出的事件
        if (ret == CONSOLE_CIRBUF_EVENT) {//控制台循环buf事件发生
            size =  LOS_CirBufUsedSize(cirBufCB);//循环buf使用大小
            if (size == 0) {
                continue;
            }
            buf = (CHAR *)LOS_MemAlloc(m_aucSysMem1, size + 1);//分配接收cirbuf的内存
            if (buf == NULL) {
                continue;
            }
            (VOID)memset_s(buf, size + 1, 0, size + 1);//清0

            LOS_CirBufLock(cirBufCB, &intSave);
            (VOID)LOS_CirBufRead(cirBufCB, buf, size);//读取循环cirBufCB至  buf
            LOS_CirBufUnlock(cirBufCB, intSave);

            (VOID)WriteToTerminal(consoleCB, buf, size);//将buf数据写到控制台终端设备
            (VOID)LOS_MemFree(m_aucSysMem1, buf);//清除buf
        } else if (ret == CONSOLE_SEND_TASK_EXIT) {//收到任务退出的事件, 由 OsConsoleBufDeinit 发出事件.
            break;//退出循环
        }
    }

    ConsoleCirBufDelete(cirBufSendCB);//删除循环buf,归还内存
    return LOS_OK;
}

#ifdef LOSCFG_KERNEL_SMP
VOID OsWaitConsoleSendTaskPend(UINT32 taskID)//等待控制台发送任务结束
{
    UINT32 i;
    CONSOLE_CB *console = NULL;
    LosTaskCB *taskCB = NULL;
    INT32 waitTime = 3000; /* 3000: 3 seconds */

    for (i = 0; i < CONSOLE_NUM; i++) {//轮询控制台
        console = g_console[i];
        if (console == NULL) {
            continue;
        }

        if (OS_TID_CHECK_INVALID(console->sendTaskID)) {
            continue;
        }

        taskCB = OS_TCB_FROM_TID(console->sendTaskID);
        while ((waitTime > 0) && (taskCB->taskEvent == NULL) && (taskID != console->sendTaskID)) {
            LOS_Mdelay(1); /* 1: wait console task pend */ //等待控制台任务阻塞
            --waitTime;
        }
    }
}
//唤醒控制台发送任务
VOID OsWakeConsoleSendTask(VOID)
{
    UINT32 i;
    CONSOLE_CB *console = NULL;

    for (i = 0; i < CONSOLE_NUM; i++) {//循环控制台数量，只有2个
        console = g_console[i];
        if (console == NULL) {
            continue;
        }

        if (console->cirBufSendCB != NULL) {//循环缓存描述符
            (VOID)LOS_EventWrite(&console->cirBufSendCB->sendEvent, CONSOLE_CIRBUF_EVENT);//写循环缓存区buf事件
        }
    }
}
#endif

