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
/**
 * @brief 配置控制台为字符捕获模式
 * @details 禁用规范模式和回显功能，使控制台能够捕获单个字符输入
 * @param[in] consoleCB 控制台控制块指针
 * @return 操作结果
 * @retval LOS_OK 成功
 * @note 该函数使用自旋锁保护控制台属性操作
 */
STATIC INT32 ConsoleCtrlCaptureChar(CONSOLE_CB *consoleCB)
{
    struct termios consoleTermios = {0};
    UINT32 intSave;  /* 中断状态保存变量 */

    LOS_SpinLockSave(&g_consoleSpin, &intSave);
    (VOID)ConsoleTcGetAttr(consoleCB->fd, &consoleTermios);
    consoleTermios.c_lflag &= ~(ICANON | ECHO);  /* 禁用规范模式和回显 */
    (VOID)ConsoleTcSetAttr(consoleCB->fd, 0, &consoleTermios);
    LOS_SpinUnlockRestore(&g_consoleSpin, intSave);

    return LOS_OK;
}

/**
 * @brief 获取控制台访问权限
 * @details 通过信号量实现控制台的互斥访问，必要时挂起shell任务
 * @param[in] consoleCB 控制台控制块指针
 * @return 操作结果
 * @retval LOS_OK 成功获取权限
 * @note 该函数会无限期等待信号量
 */
STATIC INT32 ConsoleCtrlRightsCapture(CONSOLE_CB *consoleCB)
{
    (VOID)LOS_SemPend(consoleCB->consoleSem, LOS_WAIT_FOREVER);
    if ((ConsoleRefcountGet(consoleCB) == 0) &&
        (OsCurrTaskGet()->taskID != consoleCB->shellEntryId)) {
        /* 非0表示shellentry正在uart_read中，直接挂起shellentry任务 */
        (VOID)LOS_TaskSuspend(consoleCB->shellEntryId);
    }
    ConsoleRefcountSet(consoleCB, TRUE);
    return LOS_OK;
}

/**
 * @brief 释放控制台访问权限
 * @details 释放控制台信号量，必要时恢复shell任务
 * @param[in] consoleCB 控制台控制块指针
 * @return 操作结果
 * @retval LOS_OK 成功释放权限
 * @retval LOS_NOK 控制台已处于空闲状态
 */
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

/**
 * @brief 通过设备名获取控制台控制块
 * @param[in] deviceName 设备名称
 * @return 控制台控制块指针
 * @retval 成功 获取到的控制台控制块指针
 * @retval NULL 获取失败，错误码通过errno设置
 * @see VnodeLookup
 */
STATIC CONSOLE_CB *OsGetConsoleByDevice(const CHAR *deviceName)
{
    INT32 ret;
    struct Vnode *vnode = NULL;  /* VFS节点指针 */

    VnodeHold();
    ret = VnodeLookup(deviceName, &vnode, 0);
    VnodeDrop();
    if (ret < 0) {
        set_errno(EACCES);  /* 权限被拒绝 */
        return NULL;
    }

    if (g_console[CONSOLE_SERIAL - 1]->devVnode == vnode) {
        return g_console[CONSOLE_SERIAL - 1];
    } else if (g_console[CONSOLE_TELNET - 1]->devVnode == vnode) {
        return g_console[CONSOLE_TELNET - 1];
    } else {
        set_errno(ENOENT);  /* 没有该文件或目录 */
        return NULL;
    }
}

/**
 * @brief 通过设备名获取控制台ID
 * @param[in] deviceName 设备名称
 * @return 控制台ID
 * @retval CONSOLE_SERIAL 串口控制台
 * @retval CONSOLE_TELNET Telnet控制台(LOSCFG_NET_TELNET启用时)
 * @retval -1 不支持的控制台类型
 */
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

/**
 * @brief 通过完整路径获取控制台ID
 * @param[in] fullpath 控制台设备完整路径
 * @return 控制台ID
 * @retval CONSOLE_SERIAL 串口控制台(/dev/console1)
 * @retval CONSOLE_TELNET Telnet控制台(/dev/console2, LOSCFG_NET_TELNET启用时)
 * @retval -1 不支持的控制台路径
 */
STATIC INT32 OsConsoleFullpathToID(const CHAR *fullpath)
{
#define CONSOLE_SERIAL_1 "/dev/console1"  /* 串口控制台设备路径 */
#define CONSOLE_TELNET_2 "/dev/console2" /* Telnet控制台设备路径 */

    size_t len;  /* 路径长度 */

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

/**
 * @brief 检查控制台FIFO是否为空
 * @param[in] console 控制台控制块指针
 * @return FIFO状态
 * @retval TRUE FIFO为空
 * @retval FALSE FIFO不为空
 */
STATIC BOOL ConsoleFifoEmpty(const CONSOLE_CB *console)
{
    return console->fifoOut == console->fifoIn;
}

/**
 * @brief 清空控制台FIFO缓冲区
 * @param[in] console 控制台控制块指针
 * @note 重置读写指针并清空缓冲区内容
 */
STATIC VOID ConsoleFifoClearup(CONSOLE_CB *console)
{
    console->fifoOut = 0;
    console->fifoIn = 0;
    (VOID)memset_s(console->fifo, CONSOLE_FIFO_SIZE, 0, CONSOLE_FIFO_SIZE);
}

/**
 * @brief 更新控制台FIFO当前长度
 * @param[in] console 控制台控制块指针
 * @note 当前长度 = 写入指针位置 - 读取指针位置
 */
STATIC VOID ConsoleFifoLenUpdate(CONSOLE_CB *console)
{
    console->currentLen = console->fifoIn - console->fifoOut;
}

/**
 * @brief 从控制台FIFO读取数据
 * @param[out] buffer 数据缓冲区
 * @param[in] console 控制台控制块指针
 * @param[in] bufLen 缓冲区长度
 * @return 实际读取字节数
 * @retval -1 读取失败
 * @retval 其他值 成功读取的字节数
 * @see memcpy_s
 */
STATIC INT32 ConsoleReadFifo(CHAR *buffer, CONSOLE_CB *console, size_t bufLen)
{
    INT32 ret;
    UINT32 readNum;  /* 读取字节数 */

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

/**
 * @brief 打开文件操作
 * @details 调用文件操作集中的open函数打开文件
 * @param[in] filep 文件结构体指针
 * @param[in] fops 文件操作集指针
 * @return 操作结果
 * @retval -EFAULT fops或fops->open为空
 * @retval -EPERM open操作失败
 * @retval 其他值 open操作返回值
 * @note 该函数主要用于打开控制台设备文件
 */
INT32 FilepOpen(struct file *filep, const struct file_operations_vfs *fops)
{
    INT32 ret;
    if (fops->open == NULL) {
        return -EFAULT;
    }

    /*
     * 采用uart open函数打开filep (filep对应/dev/console的filep)
     */
    ret = fops->open(filep);
    return (ret < 0) ? -EPERM : ret;
}

/**
 * @brief 处理用户输入结束
 * @details 添加换行符并更新FIFO长度，启用回显时输出回车符
 * @param[in] consoleCB 控制台控制块指针
 * @param[in] filep 文件结构体指针
 * @param[in] fops 文件操作集指针
 * @note 内联函数优化频繁调用场景
 */
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

/**
 * @brief 键盘输入状态枚举
 * @details 用于跟踪多字符组合键的解析状态
 */
enum {
    STAT_NORMAL_KEY,   /* 正常按键状态 */
    STAT_ESC_KEY,      /* ESC按键状态 */
    STAT_MULTI_KEY     /* 组合键状态 */
};

/**
 * @brief 检查上下左右方向键
 * @details 解析ESC序列组合键，识别上下左右方向键输入
 * @param[in] ch 当前字符
 * @param[in,out] lastTokenType 上一状态指针
 * @return 解析结果
 * @retval LOS_OK 成功识别方向键
 * @retval LOS_NOK 未识别或识别失败
 * @note 方向键ESC序列格式: ESC[方向键码
 *       0x1b: ESC键, 0x5b: '[', 0x41-0x44: 上下左右键
 */
STATIC INT32 UserShellCheckUDRL(const CHAR ch, INT32 *lastTokenType)
{
    INT32 ret = LOS_OK;
    if (ch == 0x1b) { /* 0x1b: ESC键 */
        *lastTokenType = STAT_ESC_KEY;
        return ret;
    } else if (ch == 0x5b) { /* 0x5b: 组合键前缀'[' */
        if (*lastTokenType == STAT_ESC_KEY) {
            *lastTokenType = STAT_MULTI_KEY;
            return ret;
        }
    } else if (ch == 0x41) { /* 上方向键 */
        if (*lastTokenType == STAT_MULTI_KEY) {
            *lastTokenType = STAT_NORMAL_KEY;
            return ret;
        }
    } else if (ch == 0x42) { /* 下方向键 */
        if (*lastTokenType == STAT_MULTI_KEY) {
            *lastTokenType = STAT_NORMAL_KEY;
            return ret;
        }
    } else if (ch == 0x43) { /* 右方向键 */
        if (*lastTokenType == STAT_MULTI_KEY) {
            *lastTokenType = STAT_NORMAL_KEY;
            return ret;
        }
    } else if (ch == 0x44) { /* 左方向键 */
        if (*lastTokenType == STAT_MULTI_KEY) {
            *lastTokenType = STAT_NORMAL_KEY;
            return ret;
        }
    }
    return LOS_NOK;
}
/**
 * @brief 判断是否需要继续处理输入字符
 * @details 检查退格键特殊情况或方向键，决定是否跳过当前字符处理
 * @param[in] consoleCB 控制台控制块指针
 * @param[in] ch 当前输入字符
 * @param[in,out] lastTokenType 上次按键类型指针（用于方向键解析）
 * @return INT32 - 处理结果：LOS_OK表示继续处理，LOS_NOK表示跳过当前字符
 * @note 当退格键按下且回显使能且缓冲区为空，或检测到方向键时返回LOS_NOK
 */
STATIC INT32 IsNeedContinue(CONSOLE_CB *consoleCB, char ch, INT32 *lastTokenType)
{
    // 退格键处理：回显使能且缓冲区为空时不处理 | 方向键解析（上/下/右/左）
    if (((ch == '\b') && (consoleCB->consoleTermios.c_lflag & ECHO) && (ConsoleFifoEmpty(consoleCB))) ||
        (UserShellCheckUDRL(ch, lastTokenType) == LOS_OK)) { /* 解析上下左右方向键 */
        return LOS_NOK; // 跳过当前字符处理
    }

    return LOS_OK; // 继续处理当前字符
}

/**
 * @brief 将字符回显到终端
 * @details 根据终端属性控制是否回显字符，退格键特殊处理（显示 序列）
 * @param[in] consoleCB 控制台控制块指针
 * @param[in] filep 文件结构体指针
 * @param[in] fops 文件操作函数集指针
 * @param[in] ch 待回显的字符
 * @note 仅当ECHO标志置位时执行回显操作
 * @see ECHO宏定义（终端回显使能标志）
 */
STATIC VOID EchoToTerminal(CONSOLE_CB *consoleCB, struct file *filep, const struct file_operations_vfs *fops, char ch)
{
    if (consoleCB->consoleTermios.c_lflag & ECHO) { // 检查终端回显使能标志
        if (ch == '\b') {
            (VOID)fops->write(filep, "\b \b", 3); // 3: "\b \b"序列长度（退格+空格+退格）
        } else {
            (VOID)fops->write(filep, &ch, EACH_CHAR); // EACH_CHAR: 每次写入1个字符
        }
    }
}

/**
 * @brief 存储读取到的字符到控制台缓冲区
 * @details 处理退格键（删除缓冲区最后字符）和普通字符（添加到缓冲区）
 * @param[in,out] consoleCB 控制台控制块指针
 * @param[in] ch 待存储的字符
 * @param[in] readcount 读取到的字符数（应等于EACH_CHAR）
 * @note 缓冲区预留3字节空间用于特殊字符处理
 * @see CONSOLE_FIFO_SIZE 控制台FIFO缓冲区大小宏定义
 */
STATIC VOID StoreReadChar(CONSOLE_CB *consoleCB, char ch, INT32 readcount)
{
    /* 3: 存储字符时需为退格键操作预留的空间 */
    if ((readcount == EACH_CHAR) && (consoleCB->fifoIn <= (CONSOLE_FIFO_SIZE - 3))) { // 检查缓冲区空间
        if (ch == '\b') {
            if (!ConsoleFifoEmpty(consoleCB)) { // 缓冲区非空时才执行删除
                consoleCB->fifo[--consoleCB->fifoIn] = '\0'; // 指针前移并清空当前位置
            }
        } else { // 普通字符：添加到缓冲区
            consoleCB->fifo[consoleCB->fifoIn] = (UINT8)ch; // 存储字符
            consoleCB->fifoIn++; // 移动写入指针
        }
    }
}

/**
 * @brief 终止指定控制台关联的进程组
 * @details 向控制台关联的进程组发送SIGINT信号，用于终端退出或中断处理
 * @param[in] consoleId 控制台ID（1~CONSOLE_NUM）
 * @note 1. 控制台ID无效（超出范围或0）时直接返回
 *       2. 进程组ID为默认值-1时不执行终止操作，避免误杀所有进程
 * @see CONSOLE_NUM 控制台最大数量宏定义
 * @see OsKillLock 带锁的进程终止函数
 */
VOID KillPgrp(UINT16 consoleId)
{
    if ((consoleId > CONSOLE_NUM) || (consoleId <= 0)) { // 检查控制台ID有效性
        return;
    }

    CONSOLE_CB *consoleCB = g_console[consoleId - 1]; // 获取控制台控制块（数组从0开始）
    /* consoleCB->pgrpId默认值为-1，可能尚未设置，避免终止所有进程 */
    if (consoleCB->pgrpId < 0) { // 进程组ID未设置
        return;
    }
    (VOID)OsKillLock(consoleCB->pgrpId, SIGINT); // 向进程组发送中断信号（SIGINT=2）
}

/**
 * @brief 用户模式下的文件读取实现
 * @details 处理规范模式（ICANON）和非规范模式的输入读取，支持行缓冲和特殊字符处理
 * @param[in,out] consoleCB 控制台控制块指针
 * @param[in] filep 文件结构体指针
 * @param[in] fops 文件操作函数集指针
 * @param[out] buffer 读取数据缓冲区
 * @param[in] bufLen 缓冲区长度
 * @return INT32 - 读取结果：成功返回读取字节数，失败返回错误码
 * @retval -EFAULT 文件操作函数集为空
 * @retval -EPERM 底层read调用失败
 * @note 1. 非规范模式直接调用底层read函数
 *       2. 规范模式下会进行行缓冲，直到换行符或EOF
 * @see ICANON 规范模式标志位
 * @see UserEndOfRead 行读取结束处理函数
 * @see ConsoleReadFifo 从FIFO缓冲区读取数据函数
 */
STATIC INT32 UserFilepRead(CONSOLE_CB *consoleCB, struct file *filep, const struct file_operations_vfs *fops,
                           CHAR *buffer, size_t bufLen)
{
    INT32 ret; // 函数返回值
    INT32 needreturn = LOS_NOK; // 是否需要返回标志：LOS_NOK-继续等待，LOS_OK-可以返回
    CHAR ch; // 当前读取的字符
    INT32 lastTokenType = STAT_NORMAL_KEY; // 上次按键类型：默认为普通键

    if (fops->read == NULL) { // 检查read函数是否存在
        return -EFAULT; // 返回函数不存在错误
    }

    /* 非规范模式（Non-ICANON mode）：无行缓冲，直接读取 */
    if ((consoleCB->consoleTermios.c_lflag & ICANON) == 0) { // ICANON标志未置位
        ret = fops->read(filep, buffer, bufLen); // 直接调用底层read
        return (ret < 0) ? -EPERM : ret; // 错误转换为-EPERM
    }
    /* 规范模式（ICANON mode）：行缓冲模式，数据存入控制台缓冲区 */
    if (consoleCB->currentLen == 0) { // 当前缓冲区为空，需要读取新数据
        while (1) { // 循环读取字符直到行结束
            ret = fops->read(filep, &ch, EACH_CHAR); // 每次读取1个字符
            if (ret <= 0) { // 读取失败或EOF
                return ret;
            }

            if (IsNeedContinue(consoleCB, ch, &lastTokenType)) // 判断是否需要跳过当前字符
                continue;

            switch (ch) { // 根据字符类型处理
                case '\r': // 回车符转换为换行符
                    ch = '\n';
                case '\n': // 换行符：行结束标志
                    EchoToTerminal(consoleCB, filep, fops, ch); // 回显换行符
                    UserEndOfRead(consoleCB, filep, fops); // 行读取结束处理
                    ret = ConsoleReadFifo(buffer, consoleCB, bufLen); // 从FIFO读取数据到缓冲区

                    needreturn = LOS_OK; // 设置返回标志
                    break;
                case '\b': // 退格键
                default: // 普通字符
                    EchoToTerminal(consoleCB, filep, fops, ch); // 回显字符
                    StoreReadChar(consoleCB, ch, ret); // 存储字符到FIFO
                    break;
            }

            if (needreturn == LOS_OK) // 满足返回条件时退出循环
                break;
        }
    } else { // 缓冲区已有数据，直接返回
        /* 若控制台FIFO已有数据，则立即返回 */
        ret = ConsoleReadFifo(buffer, consoleCB, bufLen); // 从FIFO读取数据
    }

    return ret; // 返回读取字节数或错误码
}

/**
 * @brief 文件读取适配函数
 * @details 适配控制台设备文件的读取操作，调用底层驱动的read函数
 * @param[in] filep 文件结构体指针（对应/dev/console设备）
 * @param[in] fops 文件操作函数集指针
 * @param[out] buffer 读取数据缓冲区
 * @param[in] bufLen 缓冲区长度
 * @return INT32 - 读取结果：成功返回读取字节数，失败返回-EPERM
 * @note 采用UART读取函数从filep读取数据并写入buffer
 * @see fops->read 底层设备驱动的read实现
 */
INT32 FilepRead(struct file *filep, const struct file_operations_vfs *fops, CHAR *buffer, size_t bufLen)
{
    INT32 ret; // 函数返回值
    if (fops->read == NULL) { // 检查read函数是否存在
        return -EFAULT; // 返回函数不存在错误
    }
    /*
     * 采用UART读取函数从filep读取数据
     * 并将数据写入buffer（filep对应/dev/console设备）
     */
    ret = fops->read(filep, buffer, bufLen); // 调用底层read函数
    return (ret < 0) ? -EPERM : ret; // 错误转换为-EPERM
}

/**
 * @brief 文件写入适配函数
 * @details 适配控制台设备文件的写入操作，调用底层驱动的write函数
 * @param[in] filep 文件结构体指针
 * @param[in] fops 文件操作函数集指针
 * @param[in] buffer 待写入数据缓冲区
 * @param[in] bufLen 写入数据长度
 * @return INT32 - 写入结果：成功返回写入字节数，失败返回-EPERM
 * @see fops->write 底层设备驱动的write实现
 */
INT32 FilepWrite(struct file *filep, const struct file_operations_vfs *fops, const CHAR *buffer, size_t bufLen)
{
    INT32 ret; // 函数返回值
    if (fops->write == NULL) { // 检查write函数是否存在
        return -EFAULT; // 返回函数不存在错误
    }

    ret = fops->write(filep, buffer, bufLen); // 调用底层write函数
    return (ret < 0) ? -EPERM : ret; // 错误转换为-EPERM
}

/**
 * @brief 文件关闭适配函数
 * @details 适配控制台设备文件的关闭操作，调用底层驱动的close函数
 * @param[in] filep 文件结构体指针（对应/dev/console设备）
 * @param[in] fops 文件操作函数集指针
 * @return INT32 - 关闭结果：成功返回0，失败返回-EPERM
 * @note 采用UART关闭函数关闭filep
 * @see fops->close 底层设备驱动的close实现
 */
INT32 FilepClose(struct file *filep, const struct file_operations_vfs *fops)
{
    INT32 ret; // 函数返回值
    if ((fops == NULL) || (fops->close == NULL)) { // 检查函数集和close函数是否存在
        return -EFAULT; // 返回函数不存在错误
    }

    /*
     * 采用UART关闭函数关闭filep（filep对应/dev/console设备）
     */
    ret = fops->close(filep); // 调用底层close函数
    return ret < 0 ? -EPERM : ret; // 错误转换为-EPERM
}

/**
 * @brief IO控制适配函数
 * @details 适配控制台设备文件的IO控制操作，调用底层驱动的ioctl函数
 * @param[in] filep 文件结构体指针
 * @param[in] fops 文件操作函数集指针
 * @param[in] cmd IO控制命令
 * @param[in] arg 命令参数
 * @return INT32 - 控制结果：成功返回命令执行结果，失败返回-EPERM
 * @see fops->ioctl 底层设备驱动的ioctl实现
 */
INT32 FilepIoctl(struct file *filep, const struct file_operations_vfs *fops, INT32 cmd, unsigned long arg)
{
    INT32 ret; // 函数返回值
    if (fops->ioctl == NULL) { // 检查ioctl函数是否存在
        return -EFAULT; // 返回函数不存在错误
    }

    ret = fops->ioctl(filep, cmd, arg); // 调用底层ioctl函数
    return (ret < 0) ? -EPERM : ret; // 错误转换为-EPERM
}
/**
 * @brief 文件轮询适配函数
 * @details 适配控制台设备文件的轮询操作，调用底层驱动的poll函数
 * @param[in] filep 文件结构体指针（对应/dev/serial设备）
 * @param[in] fops 文件操作函数集指针
 * @param[in] fds 轮询表指针
 * @return INT32 - 轮询结果：成功返回轮询事件掩码，失败返回-EPERM
 * @note 采用UART轮询函数对filep进行轮询
 * @see fops->poll 底层设备驱动的poll实现
 */
INT32 FilepPoll(struct file *filep, const struct file_operations_vfs *fops, poll_table *fds)
{
    INT32 ret; // 函数返回值
    if (fops->poll == NULL) { // 检查poll函数是否存在
        return -EFAULT; // 返回函数不存在错误
    }

    /*
     * 采用UART轮询函数对filep进行轮询（filep对应/dev/serial设备）
     */
    ret = fops->poll(filep, fds); // 调用底层poll函数
    return (ret < 0) ? -EPERM : ret; // 错误转换为-EPERM
}

/**
 * @brief 控制台设备打开函数
 * @details 实现控制台设备文件的打开操作，包括控制台ID验证和底层设备打开
 * @param[in,out] filep 文件结构体指针
 * @return INT32 - 打开结果：成功返回ENOERR，失败返回VFS_ERROR并设置errno
 * @retval ENOERR 成功
 * @retval EPERM 控制台ID无效或底层设备打开失败
 * @retval EINVAL 获取文件操作函数集失败
 * @note 1. 通过文件路径解析控制台ID
 *       2. 将控制台控制块指针存储到filep->f_priv
 *       3. 调用底层设备打开函数
 * @see OsConsoleFullpathToID 从路径获取控制台ID函数
 * @see GetFilepOps 获取文件操作函数集函数
 * @see FilepOpen 底层设备打开适配函数
 */
STATIC INT32 ConsoleOpen(struct file *filep)
{
    INT32 ret; // 函数返回值
    UINT32 consoleID; // 控制台ID
    struct file *privFilep = NULL; // 私有文件结构体指针
    const struct file_operations_vfs *fileOps = NULL; // 文件操作函数集指针

    consoleID = (UINT32)OsConsoleFullpathToID(filep->f_path); // 从路径解析控制台ID
    if (consoleID == (UINT32)-1) { // 控制台ID无效（-1表示解析失败）
        ret = EPERM; // 权限错误
        goto ERROUT; // 跳转到错误处理
    }
    filep->f_priv = g_console[consoleID - 1]; // 存储控制台控制块指针（数组从0开始）

    ret = GetFilepOps(filep, &privFilep, &fileOps); // 获取底层文件操作函数
    if (ret != ENOERR) { // 获取失败
        ret = EINVAL; // 参数无效
        goto ERROUT; // 跳转到错误处理
    }
    ret = FilepOpen(privFilep, fileOps); // 打开底层设备
    if (ret < 0) { // 打开失败
        ret = EPERM; // 权限错误
        goto ERROUT; // 跳转到错误处理
    }
    return ENOERR; // 成功返回

ERROUT: // 错误处理标签
    set_errno(ret); // 设置errno
    return VFS_ERROR; // 返回VFS错误
}

/**
 * @brief 控制台设备关闭函数
 * @details 实现控制台设备文件的关闭操作，释放相关资源
 * @param[in] filep 文件结构体指针
 * @return INT32 - 关闭结果：成功返回ENOERR，失败返回VFS_ERROR并设置errno
 * @retval ENOERR 成功
 * @retval EINVAL 获取文件操作函数集失败
 * @retval EPERM 底层设备关闭失败
 * @see GetFilepOps 获取文件操作函数集函数
 * @see FilepClose 底层设备关闭适配函数
 */
STATIC INT32 ConsoleClose(struct file *filep)
{
    INT32 ret; // 函数返回值
    struct file *privFilep = NULL; // 私有文件结构体指针
    const struct file_operations_vfs *fileOps = NULL; // 文件操作函数集指针

    ret = GetFilepOps(filep, &privFilep, &fileOps); // 获取底层文件操作函数
    if (ret != ENOERR) { // 获取失败
        ret = EINVAL; // 参数无效
        goto ERROUT; // 跳转到错误处理
    }
    ret = FilepClose(privFilep, fileOps); // 关闭底层设备
    if (ret < 0) { // 关闭失败
        ret = EPERM; // 权限错误
        goto ERROUT; // 跳转到错误处理
    }

    return ENOERR; // 成功返回

ERROUT: // 错误处理标签
    set_errno(ret); // 设置errno
    return VFS_ERROR; // 返回VFS错误
}

/**
 * @brief 控制台读操作实现函数
 * @details 根据任务类型（shell/用户）执行不同的读取逻辑，处理控制台访问权限
 * @param[in] consoleCB 控制台控制块指针
 * @param[out] buffer 读取数据缓冲区
 * @param[in] bufLen 缓冲区长度
 * @param[in] privFilep 私有文件结构体指针
 * @param[in] fileOps 文件操作函数集指针
 * @return ssize_t - 读取字节数，负数表示失败
 * @note 1. Shell任务直接调用FilepRead读取
 *       2. 用户任务需要获取控制台访问权限后调用UserFilepRead
 * @see FilepRead 底层文件读取适配函数
 * @see UserFilepRead 用户模式文件读取函数
 * @see ConsoleCtrlRightsCapture 获取控制台控制权函数
 * @see ConsoleCtrlRightsRelease 释放控制台控制权函数
 */
STATIC ssize_t DoRead(CONSOLE_CB *consoleCB, CHAR *buffer, size_t bufLen,
                      struct file *privFilep,
                      const struct file_operations_vfs *fileOps)
{
    INT32 ret; // 函数返回值

#ifdef LOSCFG_SHELL // 条件编译：如果启用Shell
    // 当前任务是控制台关联的Shell任务
    if (OsCurrTaskGet()->taskID == consoleCB->shellEntryId) {
        ret = FilepRead(privFilep, fileOps, buffer, bufLen); // 直接读取
    } else {
#endif
        (VOID)ConsoleCtrlRightsCapture(consoleCB); // 获取控制台控制权
        ret = UserFilepRead(consoleCB, privFilep, fileOps, buffer, bufLen); // 用户模式读取
        (VOID)ConsoleCtrlRightsRelease(consoleCB); // 释放控制台控制权
#ifdef LOSCFG_SHELL // 条件编译：如果启用Shell
    }
#endif

    return ret; // 返回读取字节数
}

/**
 * @brief 控制台设备读函数
 * @details 实现控制台设备文件的读操作，处理用户空间/内核空间缓冲区转换
 * @param[in] filep 文件结构体指针
 * @param[out] buffer 读取数据缓冲区
 * @param[in] bufLen 缓冲区长度
 * @return ssize_t - 读取字节数，VFS_ERROR表示失败
 * @note 1. 缓冲区长度超过CONSOLE_FIFO_SIZE时截断
 *       2. 用户空间缓冲区需要内核空间临时缓冲区中转
 *       3. 通过任务ID获取控制台控制块（如果未在filep中设置）
 * @see CONSOLE_FIFO_SIZE 控制台FIFO缓冲区大小宏定义
 * @see DoRead 读操作实现函数
 * @see LOS_IsUserAddressRange 判断用户空间地址函数
 * @see LOS_ArchCopyToUser 内核到用户空间数据拷贝函数
 */
STATIC ssize_t ConsoleRead(struct file *filep, CHAR *buffer, size_t bufLen)
{
    INT32 ret; // 函数返回值
    struct file *privFilep = NULL; // 私有文件结构体指针
    CONSOLE_CB *consoleCB = NULL; // 控制台控制块指针
    CHAR *sbuffer = NULL; // 临时缓冲区指针
    BOOL userBuf = FALSE; // 是否为用户空间缓冲区标志
    const struct file_operations_vfs *fileOps = NULL; // 文件操作函数集指针

    if ((buffer == NULL) || (bufLen == 0)) { // 无效参数检查
        ret = EINVAL; // 参数无效
        goto ERROUT; // 跳转到错误处理
    }

    if (bufLen > CONSOLE_FIFO_SIZE) { // 缓冲区长度限制
        bufLen = CONSOLE_FIFO_SIZE; // 截断为FIFO大小
    }

    // 判断缓冲区是否在用户空间
    userBuf = LOS_IsUserAddressRange((vaddr_t)(UINTPTR)buffer, bufLen);
    ret = GetFilepOps(filep, &privFilep, &fileOps); // 获取底层文件操作函数
    if (ret != ENOERR) { // 获取失败
        ret = -EINVAL; // 参数无效
        goto ERROUT; // 跳转到错误处理
    }
    consoleCB = (CONSOLE_CB *)filep->f_priv; // 从filep获取控制台控制块
    if (consoleCB == NULL) { // 如果未设置
        // 通过当前任务ID获取控制台控制块
        consoleCB = OsGetConsoleByTaskID(OsCurrTaskGet()->taskID);
        if (consoleCB == NULL) { // 获取失败
            return -EFAULT; // 返回错误
        }
    }

    /*
     * shell任务使用FilepRead函数获取数据，
     * 用户任务使用UserFilepRead函数获取数据
     */
    // 用户空间缓冲区需要分配内核临时缓冲区
    sbuffer = userBuf ? LOS_MemAlloc(m_aucSysMem0, bufLen) : buffer;
    if (sbuffer == NULL) { // 内存分配失败
        ret = -ENOMEM; // 内存不足
        goto ERROUT; // 跳转到错误处理
    }

    // 执行实际读取操作
    ret = DoRead(consoleCB, sbuffer, bufLen, privFilep, fileOps);
    if (ret < 0) { // 读取失败
        goto ERROUT; // 跳转到错误处理
    }

    if (userBuf) { // 用户空间缓冲区：拷贝数据并释放临时缓冲区
        // 将数据从内核缓冲区拷贝到用户缓冲区
        if (LOS_ArchCopyToUser(buffer, sbuffer, ret) != 0) {
            ret = -EFAULT; // 拷贝失败
            goto ERROUT; // 跳转到错误处理
        }

        LOS_MemFree(m_aucSysMem0, sbuffer); // 释放内核临时缓冲区
    }

    return ret; // 返回读取字节数

ERROUT: // 错误处理标签
    if ((userBuf) && (sbuffer != NULL)) { // 释放可能分配的内核缓冲区
        LOS_MemFree(m_aucSysMem0, sbuffer);
    }
    set_errno(-ret); // 设置errno
    return VFS_ERROR; // 返回VFS错误
}

/**
 * @brief 控制台写操作实现函数
 * @details 将数据写入循环缓冲区，处理CR/LF转换，支持系统异常时的日志缓存
 * @param[in] cirBufSendCB 循环缓冲区发送控制块指针
 * @param[in] buffer 待写入数据缓冲区
 * @param[in] bufLen 待写入数据长度
 * @return ssize_t - 实际写入字节数
 * @note 1. 支持CR/LF模式转换（自动添加回车符）
 *       2. 使用自旋锁保护循环缓冲区操作
 *       3. 系统异常时仅缓存日志不发送
 * @see LOS_CirBufWrite 循环缓冲区写入函数
 * @see CONSOLE_CIRBUF_EVENT 控制台循环缓冲区事件标志
 * @see OsGetSystemStatus 获取系统状态函数
 */
STATIC ssize_t DoWrite(CirBufSendCB *cirBufSendCB, CHAR *buffer, size_t bufLen)
{
    INT32 cnt; // 单次写入字节数
    size_t written = 0; // 总写入字节数
    UINT32 intSave; // 中断状态保存变量

#ifdef LOSCFG_SHELL_DMESG // 条件编译：如果启用Shell日志
    (VOID)OsLogMemcpyRecord(buffer, bufLen); // 记录日志到内存
    if (OsCheckConsoleLock()) { // 检查控制台锁定状态
        return 0; // 控制台锁定，返回0
    }
#endif

    // 获取自旋锁并保存中断状态
    LOS_SpinLockSave(&g_consoleWriteSpinLock, &intSave);
    while (written < (INT32)bufLen) { // 循环写入所有数据
        /* CR/LR模式转换 */
        if ((buffer[written] == '\n') || (buffer[written] == '\r')) {
            // 换行符或回车符前添加回车符
            (VOID)LOS_CirBufWrite(&cirBufSendCB->cirBufCB, "\r", 1);
        }

        // 写入一个字符到循环缓冲区
        cnt = LOS_CirBufWrite(&cirBufSendCB->cirBufCB, &buffer[written], 1);
        if (cnt <= 0) { // 写入失败（缓冲区满）
            break;
        }
        written += cnt; // 更新总写入字节数
    }
    LOS_SpinUnlockRestore(&g_consoleWriteSpinLock, intSave); // 恢复中断并释放自旋锁

    /* 系统异常时日志缓存但不打印 */
    if (OsGetSystemStatus() == OS_SYSTEM_NORMAL) { // 系统正常运行
        // 发送事件通知数据可发送
        (VOID)LOS_EventWrite(&cirBufSendCB->sendEvent, CONSOLE_CIRBUF_EVENT);
    }

    return written; // 返回实际写入字节数
}

/**
 * @brief 控制台设备写函数
 * @details 实现控制台设备文件的写操作，处理用户空间/内核空间缓冲区转换
 * @param[in] filep 文件结构体指针
 * @param[in] buffer 待写入数据缓冲区
 * @param[in] bufLen 待写入数据长度
 * @return ssize_t - 实际写入字节数，VFS_ERROR表示失败
 * @note 1. 缓冲区长度超过CONSOLE_FIFO_SIZE时截断
 *       2. 用户空间缓冲区需要内核空间临时缓冲区中转
 *       3. 从控制台控制块获取循环缓冲区发送控制块
 * @see CONSOLE_FIFO_SIZE 控制台FIFO缓冲区大小宏定义
 * @see DoWrite 写操作实现函数
 * @see LOS_IsUserAddressRange 判断用户空间地址函数
 * @see LOS_ArchCopyFromUser 用户到内核空间数据拷贝函数
 */
STATIC ssize_t ConsoleWrite(struct file *filep, const CHAR *buffer, size_t bufLen)
{
    INT32 ret; // 函数返回值
    CHAR *sbuffer = NULL; // 临时缓冲区指针
    BOOL userBuf = FALSE; // 是否为用户空间缓冲区标志
    CirBufSendCB *cirBufSendCB = NULL; // 循环缓冲区发送控制块指针
    struct file *privFilep = NULL; // 私有文件结构体指针
    const struct file_operations_vfs *fileOps = NULL; // 文件操作函数集指针

    if ((buffer == NULL) || (bufLen == 0)) { // 无效参数检查
        ret = EINVAL; // 参数无效
        goto ERROUT; // 跳转到错误处理
    }

    if (bufLen > CONSOLE_FIFO_SIZE) { // 缓冲区长度限制
        bufLen = CONSOLE_FIFO_SIZE; // 截断为FIFO大小
    }

    // 判断缓冲区是否在用户空间
    userBuf = LOS_IsUserAddressRange((vaddr_t)(UINTPTR)buffer, bufLen);

    ret = GetFilepOps(filep, &privFilep, &fileOps); // 获取底层文件操作函数
    // 检查获取结果、写函数是否存在以及私有数据是否有效
    if ((ret != ENOERR) || (fileOps->write == NULL) || (filep->f_priv == NULL)) {
        ret = EINVAL; // 参数无效
        goto ERROUT; // 跳转到错误处理
    }
    // 从控制台控制块获取循环缓冲区发送控制块
    cirBufSendCB = ((CONSOLE_CB *)filep->f_priv)->cirBufSendCB;

    /*
     * 采用UART打开函数从buffer读取数据
     * 并将数据写入filep（filep对应/dev/console设备）
     */
    // 用户空间缓冲区需要分配内核临时缓冲区
    sbuffer = userBuf ? LOS_MemAlloc(m_aucSysMem0, bufLen) : (CHAR *)buffer;
    if (sbuffer == NULL) { // 内存分配失败
        ret = ENOMEM; // 内存不足
        goto ERROUT; // 跳转到错误处理
    }

	// 用户空间缓冲区：从用户空间拷贝数据
    if (userBuf && (LOS_ArchCopyFromUser(sbuffer, buffer, bufLen) != 0)) {
            ret = EFAULT; // 拷贝失败
            goto ERROUT; // 跳转到错误处理
        }
    ret = DoWrite(cirBufSendCB, sbuffer, bufLen); // 执行实际写入操作

    if (userBuf) { // 用户空间缓冲区：释放内核临时缓冲区
        LOS_MemFree(m_aucSysMem0, sbuffer);
    }

    return ret; // 返回实际写入字节数

ERROUT: // 错误处理标签
    if (userBuf && sbuffer != NULL) { // 释放可能分配的内核缓冲区
        LOS_MemFree(m_aucSysMem0, sbuffer);
    }

    set_errno(ret); // 设置errno
    return VFS_ERROR; // 返回VFS错误
}
/**
 * @brief 设置终端属性（TCSETSW模式）
 * @details 从用户空间拷贝终端属性结构体，通过自旋锁保护设置终端属性
 * @param[in,out] consoleCB 控制台控制块指针
 * @param[in] arg 用户空间termos结构体指针
 * @return INT32 - 操作结果：成功返回LOS_OK，失败返回-EFAULT
 * @note TCSETSW模式：等待输出完成后再设置属性
 * @see ConsoleTcSetAttr 终端属性设置函数
 * @see LOS_ArchCopyFromUser 用户空间到内核空间数据拷贝函数
 */
STATIC INT32 ConsoleSetSW(CONSOLE_CB *consoleCB, unsigned long arg)
{
    struct termios kerTermios; // 内核空间终端属性结构体
    UINT32 intSave; // 中断状态保存变量

    // 从用户空间拷贝终端属性结构体
    if (LOS_ArchCopyFromUser(&kerTermios, (struct termios *)arg, sizeof(struct termios)) != 0) {
        return -EFAULT; // 拷贝失败返回错误
    }

    LOS_SpinLockSave(&g_consoleSpin, &intSave); // 获取自旋锁并保存中断状态
    (VOID)ConsoleTcSetAttr(consoleCB->fd, 0, &kerTermios); // 设置终端属性
    LOS_SpinUnlockRestore(&g_consoleSpin, intSave); // 恢复中断并释放自旋锁
    return LOS_OK; // 成功返回
}

/** @brief 默认窗口列数 */
#define DEFAULT_WINDOW_SIZE_COL 80
/** @brief 默认窗口行数 */
#define DEFAULT_WINDOW_SIZE_ROW 24
/**
 * @brief 获取窗口大小
 * @details 返回默认窗口大小（列数×行数），拷贝到用户空间
 * @param[in] arg 用户空间winsize结构体指针
 * @return INT32 - 操作结果：成功返回LOS_OK，失败返回-EFAULT
 * @note 默认窗口大小为80列×24行
 * @see LOS_CopyFromKernel 内核空间到用户空间数据拷贝函数
 */
STATIC INT32 ConsoleGetWinSize(unsigned long arg)
{
    struct winsize kws = { // 内核空间窗口大小结构体
        .ws_col = DEFAULT_WINDOW_SIZE_COL, // 列数：80
        .ws_row = DEFAULT_WINDOW_SIZE_ROW  // 行数：24
    };

    // 将内核窗口大小拷贝到用户空间
    return (LOS_CopyFromKernel((VOID *)arg, sizeof(struct winsize), &kws, sizeof(struct winsize)) != 0) ?
        -EFAULT : LOS_OK; // 拷贝失败返回-EFAULT，成功返回LOS_OK
}

/**
 * @brief 获取终端属性
 * @details 从标准输入设备获取控制台控制块，将终端属性拷贝到用户空间
 * @param[in] arg 用户空间termos结构体指针
 * @return INT32 - 操作结果：成功返回LOS_OK，失败返回-EFAULT或-EPERM
 * @see fs_getfilep 获取文件结构体函数
 * @see LOS_ArchCopyToUser 内核空间到用户空间数据拷贝函数
 */
STATIC INT32 ConsoleGetTermios(unsigned long arg)
{
    struct file *filep = NULL; // 文件结构体指针
    CONSOLE_CB *consoleCB = NULL; // 控制台控制块指针

    // 获取标准输入（文件描述符0）的文件结构体
    INT32 ret = fs_getfilep(0, &filep);
    if (ret < 0) {
        return -EPERM; // 获取失败返回权限错误
    }

    consoleCB = (CONSOLE_CB *)filep->f_priv; // 从文件结构体获取控制台控制块
    if (consoleCB == NULL) {
        return -EFAULT; // 控制台控制块无效返回错误
    }

    // 将终端属性拷贝到用户空间
    return (LOS_ArchCopyToUser((VOID *)arg, &consoleCB->consoleTermios, sizeof(struct termios)) != 0) ?
        -EFAULT : LOS_OK; // 拷贝失败返回-EFAULT，成功返回LOS_OK
}

/**
 * @brief 设置进程组ID
 * @details 从用户空间拷贝进程组ID到控制台控制块
 * @param[in,out] consoleCB 控制台控制块指针
 * @param[in] arg 用户空间进程组ID指针
 * @return INT32 - 操作结果：成功返回LOS_OK，失败返回-EFAULT
 * @see LOS_ArchCopyFromUser 用户空间到内核空间数据拷贝函数
 */
INT32 ConsoleSetPgrp(CONSOLE_CB *consoleCB, unsigned long arg)
{
    // 从用户空间拷贝进程组ID到控制台控制块
    if (LOS_ArchCopyFromUser(&consoleCB->pgrpId, (INT32 *)(UINTPTR)arg, sizeof(INT32)) != 0) {
        return -EFAULT; // 拷贝失败返回错误
    }
    return LOS_OK; // 成功返回
}

/**
 * @brief 获取进程组ID
 * @details 将控制台控制块中的进程组ID拷贝到用户空间
 * @param[in] consoleCB 控制台控制块指针
 * @param[in] arg 用户空间进程组ID指针
 * @return INT32 - 操作结果：成功返回LOS_OK，失败返回-EFAULT
 * @see LOS_ArchCopyToUser 内核空间到用户空间数据拷贝函数
 */
INT32 ConsoleGetPgrp(CONSOLE_CB *consoleCB, unsigned long arg)
{
    // 将进程组ID拷贝到用户空间
    return (LOS_ArchCopyToUser((VOID *)arg, &consoleCB->pgrpId, sizeof(INT32)) != 0) ? -EFAULT : LOS_OK;
}

/**
 * @brief 控制台IO控制函数
 * @details 处理各类控制台IO控制命令，包括权限控制、行/字符捕获、窗口大小、终端属性和进程组管理
 * @param[in] filep 文件结构体指针
 * @param[in] cmd IO控制命令
 * @param[in] arg 命令参数
 * @return INT32 - 命令执行结果：成功返回命令返回值，失败返回VFS_ERROR并设置errno
 * @retval EINVAL 参数无效或控制台控制块为空
 * @retval EFAULT 底层ioctl函数不存在
 * @retval EPERM 命令执行失败
 * @note 支持的主要命令：
 *       - CONSOLE_CONTROL_RIGHTS_CAPTURE/RELEASE：控制台权限获取/释放
 *       - CONSOLE_CONTROL_CAPTURE_LINE/CHAR：行/字符捕获模式设置
 *       - TIOCGWINSZ：获取窗口大小
 *       - TCSETSW/TCGETS：设置/获取终端属性
 *       - TIOCGPGRP/TIOCSPGRP：获取/设置进程组ID
 * @see GetFilepOps 获取文件操作函数集函数
 */
STATIC INT32 ConsoleIoctl(struct file *filep, INT32 cmd, unsigned long arg)
{
    INT32 ret; // 函数返回值
    struct file *privFilep = NULL; // 私有文件结构体指针
    CONSOLE_CB *consoleCB = NULL; // 控制台控制块指针
    const struct file_operations_vfs *fileOps = NULL; // 文件操作函数集指针

    ret = GetFilepOps(filep, &privFilep, &fileOps); // 获取底层文件操作函数
    if (ret != ENOERR) { // 获取失败
        ret = EINVAL; // 参数无效
        goto ERROUT; // 跳转到错误处理
    }

    if (fileOps->ioctl == NULL) { // 检查ioctl函数是否存在
        ret = EFAULT; // 函数不存在
        goto ERROUT; // 跳转到错误处理
    }

    consoleCB = (CONSOLE_CB *)filep->f_priv; // 从文件结构体获取控制台控制块
    if (consoleCB == NULL) { // 控制台控制块无效
        ret = EINVAL; // 参数无效
        goto ERROUT; // 跳转到错误处理
    }

    switch (cmd) { // 根据命令类型处理
        case CONSOLE_CONTROL_RIGHTS_CAPTURE: // 获取控制台控制权
            ret = ConsoleCtrlRightsCapture(consoleCB);
            break;
        case CONSOLE_CONTROL_RIGHTS_RELEASE: // 释放控制台控制权
            ret = ConsoleCtrlRightsRelease(consoleCB);
            break;
        case CONSOLE_CONTROL_CAPTURE_LINE: // 设置行捕获模式
            ret = ConsoleCtrlCaptureLine(consoleCB);
            break;
        case CONSOLE_CONTROL_CAPTURE_CHAR: // 设置字符捕获模式
            ret = ConsoleCtrlCaptureChar(consoleCB);
            break;
        case CONSOLE_CONTROL_REG_USERTASK: // 注册用户任务
            ret = ConsoleTaskReg(consoleCB->consoleID, arg);
            break;
        case TIOCGWINSZ: // 获取窗口大小
            ret = ConsoleGetWinSize(arg);
            break;
        case TCSETSW: // 设置终端属性（等待输出完成）
            ret = ConsoleSetSW(consoleCB, arg);
            break;
        case TCGETS: // 获取终端属性
            ret = ConsoleGetTermios(arg);
            break;
        case TIOCGPGRP: // 获取进程组ID
            ret = ConsoleGetPgrp(consoleCB, arg);
            break;
        case TIOCSPGRP: // 设置进程组ID
            ret = ConsoleSetPgrp(consoleCB, arg);
            break;
        default: // 其他命令：转发到底层驱动
            // UART配置命令需要检查参数地址是否在用户空间
            if ((cmd == UART_CFG_ATTR || cmd == UART_CFG_PRIVATE)
                && !LOS_IsUserAddress(arg)) {
                ret = EINVAL; // 参数不在用户空间，无效
                goto ERROUT; // 跳转到错误处理
            }
            ret = fileOps->ioctl(privFilep, cmd, arg); // 调用底层ioctl
            break;
    }

    if (ret < 0) { // 命令执行失败
        ret = EPERM; // 权限错误
        goto ERROUT; // 跳转到错误处理
    }

    return ret; // 返回命令执行结果
ERROUT: // 错误处理标签
    set_errno(ret); // 设置errno
    return VFS_ERROR; // 返回VFS错误
}

/**
 * @brief 控制台轮询函数
 * @details 实现控制台设备文件的轮询操作，转发到底层驱动的poll函数
 * @param[in] filep 文件结构体指针
 * @param[in] fds 轮询表指针
 * @return INT32 - 轮询结果：成功返回轮询事件掩码，失败返回VFS_ERROR并设置errno
 * @retval EINVAL 获取文件操作函数集失败
 * @retval EPERM 底层poll函数执行失败
 * @see GetFilepOps 获取文件操作函数集函数
 * @see FilepPoll 底层文件轮询适配函数
 */
STATIC INT32 ConsolePoll(struct file *filep, poll_table *fds)
{
    INT32 ret; // 函数返回值
    struct file *privFilep = NULL; // 私有文件结构体指针
    const struct file_operations_vfs *fileOps = NULL; // 文件操作函数集指针

    ret = GetFilepOps(filep, &privFilep, &fileOps); // 获取底层文件操作函数
    if (ret != ENOERR) { // 获取失败
        ret = EINVAL; // 参数无效
        goto ERROUT; // 跳转到错误处理
    }

    ret = FilepPoll(privFilep, fileOps, fds); // 调用底层poll函数
    if (ret < 0) { // 轮询失败
        ret = EPERM; // 权限错误
        goto ERROUT; // 跳转到错误处理
    }
    return ret; // 返回轮询结果

ERROUT: // 错误处理标签
    set_errno(ret); // 设置errno
    return VFS_ERROR; // 返回VFS错误
}
/* 控制台设备驱动函数结构体，定义控制台设备的文件操作接口 */
STATIC const struct file_operations_vfs g_consoleDevOps = {
    .open = ConsoleOpen,   /* 打开控制台设备 */
    .close = ConsoleClose, /* 关闭控制台设备 */
    .read = ConsoleRead,   /* 读取控制台数据 */
    .write = ConsoleWrite, /* 写入控制台数据 */
    .seek = NULL,          /* 不支持文件定位 */
    .ioctl = ConsoleIoctl, /* 控制台IO控制 */
    .mmap = NULL,          /* 不支持内存映射 */
#ifndef CONFIG_DISABLE_POLL
    .poll = ConsolePoll,   /* 控制台轮询操作 */
#endif
};

/**
 * @brief 初始化控制台终端属性
 * @param[in] consoleCB 控制台控制块指针
 * @param[in] deviceName 设备名称(如"/dev/ttyS0"或"telnet")
 * @details 根据设备类型设置终端属性，包括行缓冲、回显和中断字符
 */
STATIC VOID OsConsoleTermiosInit(CONSOLE_CB *consoleCB, const CHAR *deviceName)
{
    struct termios consoleTermios = {0};

    /* 判断是否为串口设备 */
    if ((deviceName != NULL) &&
        (strlen(deviceName) == strlen(SERIAL)) &&
        (!strncmp(deviceName, SERIAL, strlen(SERIAL)))) {
        consoleCB->isNonBlock = SetSerialBlock(consoleCB); /* 设置串口阻塞模式 */

        /* 配置控制台用户缓冲区 */
        (VOID)ConsoleTcGetAttr(consoleCB->fd, &consoleTermios);
        consoleTermios.c_lflag |= ICANON | ECHO; /* 启用行缓冲和回显 */
        consoleTermios.c_cc[VINTR] = 3; /* 设置中断字符为^C(ASCII码3) */
        (VOID)ConsoleTcSetAttr(consoleCB->fd, 0, &consoleTermios);
    }
#ifdef LOSCFG_NET_TELNET
    /* 判断是否为Telnet设备 */
    else if ((deviceName != NULL) &&
             (strlen(deviceName) == strlen(TELNET)) &&
             (!strncmp(deviceName, TELNET, strlen(TELNET)))) {
        consoleCB->isNonBlock = SetTelnetBlock(consoleCB); /* 设置Telnet阻塞模式 */
        /* 配置控制台用户缓冲区 */
        (VOID)ConsoleTcGetAttr(consoleCB->fd, &consoleTermios);
        consoleTermios.c_lflag |= ICANON | ECHO; /* 启用行缓冲和回显 */
        consoleTermios.c_cc[VINTR] = 3; /* 设置中断字符为^C(ASCII码3) */
        (VOID)ConsoleTcSetAttr(consoleCB->fd, 0, &consoleTermios);
    }
#endif
}

/**
 * @brief 初始化控制台文件结构
 * @param[in] consoleCB 控制台控制块指针
 * @return 成功返回LOS_OK，失败返回错误码
 * @retval EACCES 查找Vnode失败
 * @retval EMFILE 文件分配失败
 */
STATIC INT32 OsConsoleFileInit(CONSOLE_CB *consoleCB)
{
    INT32 ret;
    struct Vnode *vnode = NULL;       /* VFS节点指针 */
    struct file *filep = NULL;        /* 文件结构体指针 */

    VnodeHold();                      /* 持有Vnode锁 */
    ret = VnodeLookup(consoleCB->name, &vnode, 0); /* 查找控制台Vnode */
    if (ret != LOS_OK) {
        ret = EACCES;                 /* 查找失败，权限错误 */
        goto ERROUT;                  /* 跳转到错误处理 */
    }

    /* 分配文件结构体，关联Vnode和控制台控制块 */
    filep = files_allocate(vnode, O_RDWR, 0, consoleCB, FILE_START_FD);
    if (filep == NULL) {
        ret = EMFILE;                 /* 文件分配失败，打开文件过多 */
        goto ERROUT;                  /* 跳转到错误处理 */
    }
    /* 设置文件操作函数集，指向驱动操作接口 */
    filep->ops = (struct file_operations_vfs *)((struct drv_data *)vnode->data)->ops;
    consoleCB->fd = filep->fd;        /* 保存文件描述符 */

ERROUT:
    VnodeDrop();                      /* 释放Vnode锁 */
    return ret;
}

/**
 * @brief 初始化控制台设备平台
 * @param[in] consoleCB 控制台控制块指针
 * @param[in] deviceName 设备名称(如"/dev/ttyS0")
 * @return 成功返回LOS_OK，失败返回LOS_NOK
 * @details 将/dev/console关联到实际硬件设备(如UART0)，使控制台操作等同于操作硬件设备
 */
STATIC INT32 OsConsoleDevInit(CONSOLE_CB *consoleCB, const CHAR *deviceName)
{
    INT32 ret;
    struct file *filep = NULL;                /* 文件结构体指针 */
    struct Vnode *vnode = NULL;               /* VFS节点指针 */
    struct file_operations_vfs *devOps = NULL; /* 设备操作函数集 */

    /* 为filep分配内存，确保filep值不被意外修改 */
    filep = (struct file *)LOS_MemAlloc(m_aucSysMem0, sizeof(struct file));
    if (filep == NULL) {
        ret = ENOMEM;                         /* 内存分配失败 */
        goto ERROUT;
    }

    VnodeHold();
    ret = VnodeLookup(deviceName, &vnode, V_DUMMY); /* 查找硬件设备Vnode */
    VnodeDrop(); // 此处释放时机不完全正确，但目前无法完美修复
    if (ret != LOS_OK) {
        ret = EACCES;                         /* 设备查找失败 */
        PRINTK("!! can not find %s\n", consoleCB->name);
        goto ERROUT;
    }

    consoleCB->devVnode = vnode;              /* 保存设备Vnode指针 */

    /*
     * 初始化与/dev/console关联的filep，将/dev/ttyS0的uart0 vnode分配给
     * /dev/console的inode，使控制台filep操作等同于uart0 filep操作
     */
    (VOID)memset_s(filep, sizeof(struct file), 0, sizeof(struct file));
    filep->f_oflags = O_RDWR;                 /* 读写模式打开 */
    filep->f_pos = 0;                         /* 文件位置偏移量 */
    filep->f_vnode = vnode;                   /* 关联设备Vnode */
    filep->f_path = NULL;                     /* 路径信息 */
    filep->f_priv = NULL;                     /* 私有数据 */
    /*
     * 通过filep连接控制台和UART，通过filep查找UART驱动函数
     * 现在可以通过操作/dev/console来操作/dev/ttyS0
     */
    devOps = (struct file_operations_vfs *)((struct drv_data*)vnode->data)->ops;
    if (devOps != NULL && devOps->open != NULL) {
        (VOID)devOps->open(filep);            /* 打开硬件设备 */
    } else {
        ret = ENOSYS;                         /* 设备操作未实现 */
        goto ERROUT;
    }

    /* 注册控制台驱动，关联设备操作函数集 */
    ret = register_driver(consoleCB->name, &g_consoleDevOps, DEFFILEMODE, filep);
    if (ret != LOS_OK) {
        goto ERROUT;
    }

    return LOS_OK;

ERROUT:
     if (filep) {
        (VOID)LOS_MemFree(m_aucSysMem0, filep); /* 释放文件结构体内存 */
    }

    set_errno(ret);
    return LOS_NOK;
}

/**
 * @brief 控制台设备去初始化
 * @param[in] consoleCB 控制台控制块指针
 * @return 成功返回0，失败返回错误码
 */
STATIC UINT32 OsConsoleDevDeinit(const CONSOLE_CB *consoleCB)
{
    return unregister_driver(consoleCB->name); /* 注销控制台驱动 */
}

/**
 * @brief 创建控制台循环缓冲区
 * @return 成功返回CirBufSendCB指针，失败返回NULL
 * @details 分配并初始化循环缓冲区结构，包括缓冲区内存、循环缓冲控制块和事件
 */
STATIC CirBufSendCB *ConsoleCirBufCreate(VOID)
{
    UINT32 ret;
    CHAR *fifo = NULL;                        /* 缓冲区内存 */
    CirBufSendCB *cirBufSendCB = NULL;        /* 循环缓冲区发送控制块 */
    CirBuf *cirBufCB = NULL;                  /* 循环缓冲区控制块 */

    /* 分配循环缓冲区发送控制块内存 */
    cirBufSendCB = (CirBufSendCB *)LOS_MemAlloc(m_aucSysMem0, sizeof(CirBufSendCB));
    if (cirBufSendCB == NULL) {
        return NULL;
    }
    (VOID)memset_s(cirBufSendCB, sizeof(CirBufSendCB), 0, sizeof(CirBufSendCB));

    /* 分配循环缓冲区内存 */
    fifo = (CHAR *)LOS_MemAlloc(m_aucSysMem0, CONSOLE_CIRCBUF_SIZE);
    if (fifo == NULL) {
        goto ERROR_WITH_SENDCB;               /* 缓冲区内存分配失败 */
    }
    (VOID)memset_s(fifo, CONSOLE_CIRCBUF_SIZE, 0, CONSOLE_CIRCBUF_SIZE);

    cirBufCB = &cirBufSendCB->cirBufCB;
    /* 初始化循环缓冲区，设置缓冲区地址和大小 */
    ret = LOS_CirBufInit(cirBufCB, fifo, CONSOLE_CIRCBUF_SIZE);
    if (ret != LOS_OK) {
        goto ERROR_WITH_FIFO;                 /* 循环缓冲区初始化失败 */
    }

    (VOID)LOS_EventInit(&cirBufSendCB->sendEvent); /* 初始化发送事件 */
    return cirBufSendCB;

ERROR_WITH_FIFO:
    (VOID)LOS_MemFree(m_aucSysMem0, cirBufCB->fifo); /* 释放缓冲区内存 */
ERROR_WITH_SENDCB:
    (VOID)LOS_MemFree(m_aucSysMem0, cirBufSendCB);   /* 释放发送控制块内存 */
    return NULL;
}

/**
 * @brief 删除控制台循环缓冲区
 * @param[in] cirBufSendCB 循环缓冲区发送控制块指针
 * @details 释放循环缓冲区内存、销毁事件并释放控制块内存
 */
STATIC VOID ConsoleCirBufDelete(CirBufSendCB *cirBufSendCB)
{
    CirBuf *cirBufCB = &cirBufSendCB->cirBufCB;

    (VOID)LOS_MemFree(m_aucSysMem0, cirBufCB->fifo); /* 释放缓冲区内存 */
    LOS_CirBufDeinit(cirBufCB);                      /* 去初始化循环缓冲区 */
    (VOID)LOS_EventDestroy(&cirBufSendCB->sendEvent); /* 销毁发送事件 */
    (VOID)LOS_MemFree(m_aucSysMem0, cirBufSendCB);    /* 释放发送控制块内存 */
}

/**
 * @brief 初始化控制台缓冲区和发送任务
 * @param[in] consoleCB 控制台控制块指针
 * @return 成功返回LOS_OK，失败返回LOS_NOK
 * @details 创建循环缓冲区并启动控制台发送任务
 */
STATIC UINT32 OsConsoleBufInit(CONSOLE_CB *consoleCB)
{
    UINT32 ret;
    TSK_INIT_PARAM_S initParam = {0};         /* 任务初始化参数 */

    consoleCB->cirBufSendCB = ConsoleCirBufCreate(); /* 创建循环缓冲区 */
    if (consoleCB->cirBufSendCB == NULL) {
        return LOS_NOK;
    }

    /* 初始化发送任务参数 */
    initParam.pfnTaskEntry = (TSK_ENTRY_FUNC)ConsoleSendTask; /* 任务入口函数 */
    initParam.usTaskPrio   = SHELL_TASK_PRIORITY;             /* 任务优先级 */
    initParam.auwArgs[0]   = (UINTPTR)consoleCB;              /* 传递控制台控制块 */
    initParam.uwStackSize  = LOSCFG_BASE_CORE_TSK_DEFAULT_STACK_SIZE; /* 栈大小 */
    /* 根据控制台类型设置任务名称 */
    initParam.pcName = (consoleCB->consoleID == CONSOLE_SERIAL) ? "SendToSer" : "SendToTelnet";
    initParam.uwResved     = LOS_TASK_STATUS_DETACHED;        /* 分离任务属性 */

    /* 创建发送任务 */
    ret = LOS_TaskCreate(&consoleCB->sendTaskID, &initParam);
    if (ret != LOS_OK) {
        ConsoleCirBufDelete(consoleCB->cirBufSendCB); /* 创建失败，删除缓冲区 */
        consoleCB->cirBufSendCB = NULL;
        return LOS_NOK;
    }
    /* 等待发送任务启动完成 */
    (VOID)LOS_EventRead(&consoleCB->cirBufSendCB->sendEvent, CONSOLE_SEND_TASK_RUNNING,
                        LOS_WAITMODE_OR | LOS_WAITMODE_CLR, LOS_WAIT_FOREVER);

    return LOS_OK;
}

/**
 * @brief 控制台缓冲区去初始化
 * @param[in] consoleCB 控制台控制块指针
 * @details 通知发送任务退出并释放缓冲区资源
 */
STATIC VOID OsConsoleBufDeinit(CONSOLE_CB *consoleCB)
{
    CirBufSendCB *cirBufSendCB = consoleCB->cirBufSendCB;

    consoleCB->cirBufSendCB = NULL;
    /* 发送任务退出事件 */
    (VOID)LOS_EventWrite(&cirBufSendCB->sendEvent, CONSOLE_SEND_TASK_EXIT);
}
/**
 * @brief 初始化控制台控制块
 * @param[in] consoleID 控制台ID
 * @return 成功返回控制台控制块指针，失败返回NULL
 * @details 分配并初始化CONSOLE_CB结构体，设置初始控制台ID和名称
 */
STATIC CONSOLE_CB *OsConsoleCBInit(UINT32 consoleID)
{
    /* 分配控制台控制块内存 */
    CONSOLE_CB *consoleCB = (CONSOLE_CB *)LOS_MemAlloc((VOID *)m_aucSysMem0, sizeof(CONSOLE_CB));
    if (consoleCB == NULL) {
        return NULL; /* 内存分配失败 */
    }
    (VOID)memset_s(consoleCB, sizeof(CONSOLE_CB), 0, sizeof(CONSOLE_CB)); /* 初始化控制块内存 */

    consoleCB->consoleID = consoleID;                  /* 设置控制台ID */
    consoleCB->pgrpId = -1;                            /* 进程组ID初始化为-1(无效) */
    consoleCB->shellEntryId = SHELL_ENTRYID_INVALID;   /* 初始化shellEntryId为无效值 */
    /* 分配控制台名称内存 */
    consoleCB->name = LOS_MemAlloc((VOID *)m_aucSysMem0, CONSOLE_NAMELEN);
    if (consoleCB->name == NULL) {
        PRINT_ERR("consoleCB->name malloc failed\n");
        (VOID)LOS_MemFree((VOID *)m_aucSysMem0, consoleCB); /* 释放控制块内存 */
        return NULL;
    }
    return consoleCB;
}

/**
 * @brief 去初始化控制台控制块
 * @param[in] consoleCB 控制台控制块指针
 * @details 释放控制台名称内存和控制块内存
 */
STATIC VOID OsConsoleCBDeinit(CONSOLE_CB *consoleCB)
{
    (VOID)LOS_MemFree((VOID *)m_aucSysMem0, consoleCB->name); /* 释放名称内存 */
    consoleCB->name = NULL;
    (VOID)LOS_MemFree((VOID *)m_aucSysMem0, consoleCB);       /* 释放控制块内存 */
}

/**
 * @brief 创建控制台实例
 * @param[in] consoleID 控制台ID
 * @param[in] deviceName 设备名称(如"/dev/ttyS0")
 * @return 成功返回控制台控制块指针，失败返回NULL
 * @details 完整初始化控制台所需的所有资源，包括缓冲区、信号量、设备和文件
 */
STATIC CONSOLE_CB *OsConsoleCreate(UINT32 consoleID, const CHAR *deviceName)
{
    INT32 ret;
    CONSOLE_CB *consoleCB = OsConsoleCBInit(consoleID); /* 初始化控制块 */
    if (consoleCB == NULL) {
        PRINT_ERR("console malloc error.\n");
        return NULL;
    }

    /* 格式化控制台名称，如"/dev/console0" */
    ret = snprintf_s(consoleCB->name, CONSOLE_NAMELEN, CONSOLE_NAMELEN - 1,
                     "%s%u", CONSOLE, consoleCB->consoleID);
    if (ret == -1) {
        PRINT_ERR("consoleCB->name snprintf_s failed\n");
        goto ERR_WITH_NAME; /* 名称格式化失败，跳转到错误处理 */
    }

    /* 初始化控制台缓冲区 */
    ret = (INT32)OsConsoleBufInit(consoleCB);
    if (ret != LOS_OK) {
        PRINT_ERR("console OsConsoleBufInit error. %d\n", ret);
        goto ERR_WITH_NAME; /* 缓冲区初始化失败 */
    }

    /* 创建控制台信号量(初始值1，互斥访问) */
    ret = (INT32)LOS_SemCreate(1, &consoleCB->consoleSem);
    if (ret != LOS_OK) {
        PRINT_ERR("create sem for uart failed\n");
        goto ERR_WITH_BUF; /* 信号量创建失败 */
    }

    /* 初始化控制台设备 */
    ret = OsConsoleDevInit(consoleCB, deviceName);
    if (ret != LOS_OK) {
        PRINT_ERR("console OsConsoleDevInit error. %d\n", ret);
        goto ERR_WITH_SEM; /* 设备初始化失败 */
    }

    /* 初始化控制台文件 */
    ret = OsConsoleFileInit(consoleCB);
    if (ret != LOS_OK) {
        PRINT_ERR("console OsConsoleFileInit error. %d\n", ret);
        goto ERR_WITH_DEV; /* 文件初始化失败 */
    }

    OsConsoleTermiosInit(consoleCB, deviceName); /* 初始化终端属性 */
    return consoleCB;

ERR_WITH_DEV:
    ret = (INT32)OsConsoleDevDeinit(consoleCB); /* 去初始化设备 */
    if (ret != LOS_OK) {
        PRINT_ERR("OsConsoleDevDeinit failed!\n");
    }
ERR_WITH_SEM:
    (VOID)LOS_SemDelete(consoleCB->consoleSem); /* 删除信号量 */
ERR_WITH_BUF:
    OsConsoleBufDeinit(consoleCB); /* 去初始化缓冲区 */
ERR_WITH_NAME:
    OsConsoleCBDeinit(consoleCB); /* 去初始化控制块 */
    return NULL;
}

/**
 * @brief 删除控制台实例
 * @param[in] consoleCB 控制台控制块指针
 * @return 成功返回0，失败返回错误码
 * @details 关闭文件描述符并释放所有相关资源
 */
STATIC UINT32 OsConsoleDelete(CONSOLE_CB *consoleCB)
{
    UINT32 ret;

    (VOID)files_close(consoleCB->fd); /* 关闭文件描述符 */
    ret = OsConsoleDevDeinit(consoleCB); /* 去初始化设备 */
    if (ret != LOS_OK) {
        PRINT_ERR("OsConsoleDevDeinit failed!\n");
    }
    OsConsoleBufDeinit((CONSOLE_CB *)consoleCB); /* 去初始化缓冲区 */
    (VOID)LOS_SemDelete(consoleCB->consoleSem);  /* 删除信号量 */
    (VOID)LOS_MemFree(m_aucSysMem0, consoleCB->name); /* 释放名称内存 */
    consoleCB->name = NULL;
    (VOID)LOS_MemFree(m_aucSysMem0, consoleCB);       /* 释放控制块内存 */

    return ret;
}

/**
 * @brief 系统控制台初始化
 * @param[in] deviceName 设备名称(如"/dev/ttyS0")
 * @return 成功返回ENOERR，失败返回VFS_ERROR
 * @details 初始化系统控制台并返回标准输入/输出/错误文件描述符
 */
INT32 system_console_init(const CHAR *deviceName)
{
#ifdef LOSCFG_SHELL
    UINT32 ret;
#endif
    INT32 consoleID;          /* 控制台ID */
    UINT32 intSave;           /* 中断状态保存 */
    CONSOLE_CB *consoleCB = NULL; /* 控制台控制块指针 */

    consoleID = OsGetConsoleID(deviceName); /* 获取控制台ID */
    if (consoleID == -1) {
        PRINT_ERR("device is full.\n"); /* 设备数量已满 */
        return VFS_ERROR;
    }

    consoleCB = OsConsoleCreate((UINT32)consoleID, deviceName); /* 创建控制台 */
    if (consoleCB == NULL) {
        PRINT_ERR("%s, %d\n", __FUNCTION__, __LINE__);
        return VFS_ERROR;
    }

    LOS_SpinLockSave(&g_consoleSpin, &intSave); /* 获取自旋锁并禁止中断 */
    g_console[consoleID - 1] = consoleCB;       /* 将控制台添加到全局数组 */
    if (OsCurrTaskGet() != NULL) {
        /* 记录当前任务使用的控制台ID */
        g_taskConsoleIDArray[OsCurrTaskGet()->taskID] = (UINT8)consoleID;
    }
    LOS_SpinUnlockRestore(&g_consoleSpin, intSave); /* 释放自旋锁并恢复中断 */

#ifdef LOSCFG_SHELL
    ret = OsShellInit(consoleID); /* 初始化Shell */
    if (ret != LOS_OK) {
        PRINT_ERR("%s, %d\n", __FUNCTION__, __LINE__);
        LOS_SpinLockSave(&g_consoleSpin, &intSave);
        (VOID)OsConsoleDelete(consoleCB); /* 删除已创建的控制台 */
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

/**
 * @brief 系统控制台去初始化
 * @param[in] deviceName 设备名称
 * @return 成功返回ENOERR，失败返回VFS_ERROR
 * @details 释放控制台资源并将所有任务重定向到串口控制台
 */
INT32 system_console_deinit(const CHAR *deviceName)
{
    UINT32 ret;
    CONSOLE_CB *consoleCB = NULL;
    UINT32 taskIdx;           /* 任务索引 */
    LosTaskCB *taskCB = NULL; /* 任务控制块指针 */
    UINT32 intSave;           /* 中断状态保存 */

    consoleCB = OsGetConsoleByDevice(deviceName); /* 通过设备名获取控制台 */
    if (consoleCB == NULL) {
        return VFS_ERROR;
    }

#ifdef LOSCFG_SHELL
    (VOID)OsShellDeinit((INT32)consoleCB->consoleID); /* 去初始化Shell */
#endif

    LOS_SpinLockSave(&g_consoleSpin, &intSave); /* 获取自旋锁 */
    /* Telnet控制台不可用时，将所有任务重定向到串口控制台 */
    for (taskIdx = 0; taskIdx < g_taskMaxNum; taskIdx++) {
        taskCB = ((LosTaskCB *)g_taskCBArray) + taskIdx;
        if (OsTaskIsUnused(taskCB)) {
            continue; /* 跳过未使用的任务 */
        } else {
            g_taskConsoleIDArray[taskCB->taskID] = CONSOLE_SERIAL; /* 重定向到串口 */
        }
    }
    g_console[consoleCB->consoleID - 1] = NULL; /* 从全局数组移除控制台 */
    LOS_SpinUnlockRestore(&g_consoleSpin, intSave); /* 释放自旋锁 */

    ret = OsConsoleDelete(consoleCB); /* 删除控制台 */
    if (ret != LOS_OK) {
        PRINT_ERR("%s, Failed to system_console_deinit\n", __FUNCTION__);
        return VFS_ERROR;
    }

    return ENOERR;
}

/**
 * @brief 检查控制台是否启用
 * @return 启用返回TRUE，否则返回FALSE
 * @details 根据当前任务控制台ID、系统状态和编译选项判断控制台可用性
 */
BOOL ConsoleEnable(VOID)
{
    INT32 consoleID;

    if (OsCurrTaskGet() != NULL) {
        /* 获取当前任务的控制台ID */
        consoleID = g_taskConsoleIDArray[OsCurrTaskGet()->taskID];
        if (g_uart_fputc_en == 0) { /* UART输出禁用时 */
            /* Telnet控制台存在且系统可抢占时启用 */
            if ((g_console[CONSOLE_TELNET - 1] != NULL) && OsPreemptable()) {
                return TRUE;
            }
        }

        if (consoleID == 0) {
            return FALSE; /* 无效控制台ID */
        } else if ((consoleID == CONSOLE_TELNET) || (consoleID == CONSOLE_SERIAL)) {
            /* 系统正常运行且非抢占状态时禁用，其他情况启用 */
            return ((OsGetSystemStatus() == OS_SYSTEM_NORMAL) && !OsPreemptable()) ? FALSE : TRUE;
        }
#if defined (LOSCFG_DRIVERS_USB_SERIAL_GADGET) || defined (LOSCFG_DRIVERS_USB_ETH_SER_GADGET)
        /* USB串口设备且用户串口掩码为1时启用 */
        else if ((SerialTypeGet() == SERIAL_TYPE_USBTTY_DEV) && (userial_mask_get() == 1)) {
            return TRUE;
        }
#endif
    }

    return FALSE;
}
/**
 * @brief 检查Shell入口任务是否正在运行
 * @param[in] shellEntryId Shell入口任务ID
 * @return 运行中返回TRUE，否则返回FALSE
 * @details 通过任务ID获取任务控制块，检查任务是否有效且名称匹配Shell入口任务
 */
BOOL IsShellEntryRunning(UINT32 shellEntryId)
{
    LosTaskCB *taskCB = NULL;
    if (shellEntryId == SHELL_ENTRYID_INVALID) { /* 无效任务ID直接返回 */
        return FALSE;
    }
    taskCB = OsGetTaskCB(shellEntryId); /* 获取任务控制块 */
    /* 检查任务是否有效且名称匹配Shell入口任务 */
    return !OsTaskIsUnused(taskCB) &&
           (strlen(taskCB->taskName) == SHELL_ENTRY_NAME_LEN &&
            strncmp(taskCB->taskName, SHELL_ENTRY_NAME, SHELL_ENTRY_NAME_LEN) == 0);
}

/**
 * @brief 注册任务到控制台
 * @param[in] consoleID 控制台ID
 * @param[in] taskID 任务ID
 * @return 成功返回LOS_OK，失败返回LOS_NOK
 * @details 将任务与控制台关联，设置进程组ID为根进程组
 */
INT32 ConsoleTaskReg(INT32 consoleID, UINT32 taskID)
{
    UINT32 intSave; /* 中断状态保存变量 */

    LOS_SpinLockSave(&g_consoleSpin, &intSave); /* 获取自旋锁并禁止中断 */
    /* 如果当前Shell入口未运行，则注册新任务 */
    if (!IsShellEntryRunning(g_console[consoleID - 1]->shellEntryId)) {
        g_console[consoleID - 1]->shellEntryId = taskID; /* 保存任务ID到控制台控制块 */
        LOS_SpinUnlockRestore(&g_consoleSpin, intSave);   /* 释放自旋锁并恢复中断 */
        (VOID)OsSetCurrProcessGroupID(OS_USER_ROOT_PROCESS_ID); /* 设置当前进程组ID为根进程组 */
        return LOS_OK;
    }
    LOS_SpinUnlockRestore(&g_consoleSpin, intSave); /* 释放自旋锁并恢复中断 */
    /* 如果任务已注册则返回成功，否则失败 */
    return (g_console[consoleID - 1]->shellEntryId == taskID) ? LOS_OK : LOS_NOK;
}

/**
 * @brief 设置串口为非阻塞模式
 * @param[in] consoleCB 控制台控制块指针
 * @return 设置成功返回TRUE，失败返回FALSE
 * @details 通过IO控制命令CONSOLE_CMD_RD_BLOCK_SERIAL设置串口读取模式
 */
BOOL SetSerialNonBlock(const CONSOLE_CB *consoleCB)
{
    if (consoleCB == NULL) {
        PRINT_ERR("%s: Input parameter is illegal\n", __FUNCTION__);
        return FALSE;
    }
    /* 发送IO控制命令，设置串口为非阻塞模式(参数CONSOLE_RD_NONBLOCK) */
    return ioctl(consoleCB->fd, CONSOLE_CMD_RD_BLOCK_SERIAL, CONSOLE_RD_NONBLOCK) == 0;
}

/**
 * @brief 设置串口为阻塞模式
 * @param[in] consoleCB 控制台控制块指针
 * @return 设置成功返回TRUE，失败返回FALSE
 * @details 通过IO控制命令CONSOLE_CMD_RD_BLOCK_SERIAL设置串口读取模式
 */
BOOL SetSerialBlock(const CONSOLE_CB *consoleCB)
{
    if (consoleCB == NULL) {
        PRINT_ERR("%s: Input parameter is illegal\n", __FUNCTION__);
        return TRUE;
    }
    /* 发送IO控制命令，设置串口为阻塞模式(参数CONSOLE_RD_BLOCK) */
    return ioctl(consoleCB->fd, CONSOLE_CMD_RD_BLOCK_SERIAL, CONSOLE_RD_BLOCK) != 0;
}

/**
 * @brief 设置Telnet为非阻塞模式
 * @param[in] consoleCB 控制台控制块指针
 * @return 设置成功返回TRUE，失败返回FALSE
 * @details 通过IO控制命令CONSOLE_CMD_RD_BLOCK_TELNET设置Telnet读取模式
 */
BOOL SetTelnetNonBlock(const CONSOLE_CB *consoleCB)
{
    if (consoleCB == NULL) {
        PRINT_ERR("%s: Input parameter is illegal\n", __FUNCTION__);
        return FALSE;
    }
    /* 发送IO控制命令，设置Telnet为非阻塞模式(参数CONSOLE_RD_NONBLOCK) */
    return ioctl(consoleCB->fd, CONSOLE_CMD_RD_BLOCK_TELNET, CONSOLE_RD_NONBLOCK) == 0;
}

/**
 * @brief 设置Telnet为阻塞模式
 * @param[in] consoleCB 控制台控制块指针
 * @return 设置成功返回TRUE，失败返回FALSE
 * @details 通过IO控制命令CONSOLE_CMD_RD_BLOCK_TELNET设置Telnet读取模式
 */
BOOL SetTelnetBlock(const CONSOLE_CB *consoleCB)
{
    if (consoleCB == NULL) {
        PRINT_ERR("%s: Input parameter is illegal\n", __FUNCTION__);
        return TRUE;
    }
    /* 发送IO控制命令，设置Telnet为阻塞模式(参数CONSOLE_RD_BLOCK) */
    return ioctl(consoleCB->fd, CONSOLE_CMD_RD_BLOCK_TELNET, CONSOLE_RD_BLOCK) != 0;
}

/**
 * @brief 检查控制台是否为非阻塞模式
 * @param[in] consoleCB 控制台控制块指针
 * @return 非阻塞模式返回TRUE，否则返回FALSE
 */
BOOL is_nonblock(const CONSOLE_CB *consoleCB)
{
    if (consoleCB == NULL) {
        PRINT_ERR("%s: Input parameter is illegal\n", __FUNCTION__);
        return FALSE;
    }
    return consoleCB->isNonBlock; /* 返回控制台控制块中的非阻塞标志 */
}

/**
 * @brief 更新控制台文件描述符
 * @return 成功返回文件描述符，失败返回-1
 * @details 根据当前任务和系统配置选择合适的控制台设备
 */
INT32 ConsoleUpdateFd(VOID)
{
    INT32 consoleID; /* 控制台ID */

    if (OsCurrTaskGet() != NULL) {
        /* 获取当前任务关联的控制台ID */
        consoleID = g_taskConsoleIDArray[(OsCurrTaskGet())->taskID];
    } else {
        return -1; /* 无当前任务，返回错误 */
    }

    /* UART输出禁用时，优先使用Telnet控制台 */
    if (g_uart_fputc_en == 0) {
        if (g_console[CONSOLE_TELNET - 1] != NULL) {
            consoleID = CONSOLE_TELNET;
        }
    } else if (consoleID == 0) { /* 未设置控制台ID时自动选择 */
        if (g_console[CONSOLE_SERIAL - 1] != NULL) {
            consoleID = CONSOLE_SERIAL; /* 优先选择串口控制台 */
        } else if (g_console[CONSOLE_TELNET - 1] != NULL) {
            consoleID = CONSOLE_TELNET; /* 其次选择Telnet控制台 */
        } else {
            PRINTK("No console dev used.\n");
            return -1; /* 无可用控制台 */
        }
    }

    /* 返回选中控制台的文件描述符 */
    return (g_console[consoleID - 1] != NULL) ? g_console[consoleID - 1]->fd : -1;
}

/**
 * @brief 通过ID获取控制台控制块
 * @param[in] consoleID 控制台ID
 * @return 成功返回控制台控制块指针，失败返回NULL
 * @details 非Telnet控制台ID默认返回串口控制台
 */
CONSOLE_CB *OsGetConsoleByID(INT32 consoleID)
{
    /* 非Telnet控制台ID默认使用串口控制台 */
    if (consoleID != CONSOLE_TELNET) {
        consoleID = CONSOLE_SERIAL;
    }
    return g_console[consoleID - 1]; /* 返回控制台数组中对应的控制块 */
}

/**
 * @brief 通过任务ID获取控制台控制块
 * @param[in] taskID 任务ID
 * @return 成功返回控制台控制块指针，失败返回NULL
 */
CONSOLE_CB *OsGetConsoleByTaskID(UINT32 taskID)
{
    /* 获取任务关联的控制台ID */
    INT32 consoleID = g_taskConsoleIDArray[taskID];

    return OsGetConsoleByID(consoleID); /* 通过控制台ID获取控制块 */
}

/**
 * @brief 设置新任务的控制台ID
 * @param[in] newTaskID 新任务ID
 * @param[in] curTaskID 当前任务ID
 * @details 将新任务的控制台ID设置为与当前任务相同
 */
VOID OsSetConsoleID(UINT32 newTaskID, UINT32 curTaskID)
{
    /* 检查任务ID有效性 */
    if ((newTaskID >= LOSCFG_BASE_CORE_TSK_LIMIT) || (curTaskID >= LOSCFG_BASE_CORE_TSK_LIMIT)) {
        return;
    }

    /* 复制当前任务的控制台ID到新任务 */
    g_taskConsoleIDArray[newTaskID] = g_taskConsoleIDArray[curTaskID];
}

/**
 * @brief 向终端写入数据
 * @param[in] consoleCB 控制台控制块指针
 * @param[in] buffer 数据缓冲区
 * @param[in] bufLen 数据长度
 * @return 成功返回写入字节数，失败返回VFS_ERROR
 * @details 通过文件操作接口将数据写入底层终端设备
 */
STATIC ssize_t WriteToTerminal(const CONSOLE_CB *consoleCB, const CHAR *buffer, size_t bufLen)
{
    INT32 ret, fd;                  /* 返回值和文件描述符 */
    INT32 cnt = 0;                  /* 写入字节计数 */
    struct file *privFilep = NULL;  /* 私有文件结构体指针 */
    struct file *filep = NULL;      /* 文件结构体指针 */
    const struct file_operations_vfs *fileOps = NULL; /* 文件操作函数集 */

    fd = consoleCB->fd; /* 获取控制台文件描述符 */
    ret = fs_getfilep(fd, &filep); /* 获取文件结构体 */
    if (ret < 0) {
        ret = -EPERM; /* 权限错误 */
        goto ERROUT;
    }
    /* 获取文件操作函数集 */
    ret = GetFilepOps(filep, &privFilep, &fileOps);
    if (ret != ENOERR) {
        ret = -EINVAL; /* 无效参数 */
        goto ERROUT;
    }

    if ((fileOps == NULL) || (fileOps->write == NULL)) {
        ret = EFAULT; /* 错误地址 */
        goto ERROUT;
    }
    (VOID)fileOps->write(privFilep, buffer, bufLen); /* 调用底层写函数 */

    return cnt;

ERROUT:
    set_errno(ret); /* 设置错误码 */
    return VFS_ERROR;
}

/**
 * @brief 控制台发送任务
 * @param[in] param 控制台控制块指针
 * @return 成功返回LOS_OK
 * @details 从循环缓冲区读取数据并发送到终端，通过事件驱动机制实现
 */
STATIC UINT32 ConsoleSendTask(UINTPTR param)
{
    CONSOLE_CB *consoleCB = (CONSOLE_CB *)param; /* 控制台控制块 */
    CirBufSendCB *cirBufSendCB = consoleCB->cirBufSendCB; /* 循环缓冲区发送控制块 */
    CirBuf *cirBufCB = &cirBufSendCB->cirBufCB; /* 循环缓冲区控制块 */
    UINT32 ret, size; /* 返回值和数据大小 */
    CHAR *buf = NULL; /* 临时缓冲区 */

    /* 发送任务运行事件 */
    (VOID)LOS_EventWrite(&cirBufSendCB->sendEvent, CONSOLE_SEND_TASK_RUNNING);

    while (1) { /* 任务主循环 */
        /* 等待数据事件或退出事件 */
        ret = LOS_EventRead(&cirBufSendCB->sendEvent, CONSOLE_CIRBUF_EVENT | CONSOLE_SEND_TASK_EXIT,
                            LOS_WAITMODE_OR | LOS_WAITMODE_CLR, LOS_WAIT_FOREVER);
        if (ret == CONSOLE_CIRBUF_EVENT) { /* 数据事件：有数据需要发送 */
            size =  LOS_CirBufUsedSize(cirBufCB); /* 获取缓冲区数据大小 */
            if (size == 0) {
                continue; /* 无数据，继续等待 */
            }
            /* 分配临时缓冲区 */
            buf = (CHAR *)LOS_MemAlloc(m_aucSysMem1, size + 1);
            if (buf == NULL) {
                continue; /* 内存分配失败，继续等待 */
            }
            (VOID)memset_s(buf, size + 1, 0, size + 1); /* 初始化缓冲区 */

            (VOID)LOS_CirBufRead(cirBufCB, buf, size); /* 从循环缓冲区读取数据 */

            (VOID)WriteToTerminal(consoleCB, buf, size); /* 写入终端 */
            (VOID)LOS_MemFree(m_aucSysMem1, buf); /* 释放临时缓冲区 */
        } else if (ret == CONSOLE_SEND_TASK_EXIT) { /* 退出事件：终止任务 */
            break;
        }
    }

    ConsoleCirBufDelete(cirBufSendCB); /* 删除循环缓冲区 */
    return LOS_OK;
}

#ifdef LOSCFG_KERNEL_SMP
/**
 * @brief 等待控制台发送任务挂起(SMP多核模式)
 * @param[in] taskID 任务ID
 * @details 等待控制台发送任务进入挂起状态，超时时间3秒
 */
VOID OsWaitConsoleSendTaskPend(UINT32 taskID)
{
    UINT32 i;                       /* 循环索引 */
    CONSOLE_CB *console = NULL;     /* 控制台控制块指针 */
    LosTaskCB *taskCB = NULL;       /* 任务控制块指针 */
    INT32 waitTime = 3000; /* 3000: 3 seconds 超时时间(毫秒) */

    for (i = 0; i < CONSOLE_NUM; i++) { /* 遍历所有控制台 */
        console = g_console[i];
        if (console == NULL) {
            continue; /* 跳过未初始化的控制台 */
        }

        if (OS_TID_CHECK_INVALID(console->sendTaskID)) { /* 检查发送任务ID有效性 */
            continue;
        }

        taskCB = OS_TCB_FROM_TID(console->sendTaskID); /* 获取发送任务控制块 */
        /* 等待任务挂起或超时 */
        while ((waitTime > 0) && (taskCB->taskEvent == NULL) && (taskID != console->sendTaskID)) {
            LOS_Mdelay(1); /* 1: wait console task pend 等待1毫秒 */
            --waitTime;
        }
    }
}

/**
 * @brief 唤醒控制台发送任务(SMP多核模式)
 * @details 向所有控制台发送任务发送数据事件，唤醒等待中的任务
 */
VOID OsWakeConsoleSendTask(VOID)
{
    UINT32 i;                       /* 循环索引 */
    CONSOLE_CB *console = NULL;     /* 控制台控制块指针 */

    for (i = 0; i < CONSOLE_NUM; i++) { /* 遍历所有控制台 */
        console = g_console[i];
        if (console == NULL) {
            continue; /* 跳过未初始化的控制台 */
        }

        if (console->cirBufSendCB != NULL) {
            /* 发送数据事件，唤醒发送任务 */
            (VOID)LOS_EventWrite(&console->cirBufSendCB->sendEvent, CONSOLE_CIRBUF_EVENT);
        }
    }
}
#endif

