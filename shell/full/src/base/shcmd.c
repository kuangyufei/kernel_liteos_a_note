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

#include "shcmd.h"
#include "shell_pri.h"
#include "show.h"
#include "stdlib.h"
#include "unistd.h"
#include "dirent.h"
#include "securec.h"
#include "los_mux.h"
#include "los_memory.h"
#include "los_typedef.h"

#define SHELL_INIT_MAGIC_FLAG 0xABABABAB  // Shell初始化魔术标志
#define CTRL_C 0x03 /* 0x03: ctrl+c ASCII码值 */

STATIC CmdModInfo g_cmdInfo;  // 命令模块信息全局变量

LOS_HAL_TABLE_BEGIN(g_shellcmd, shellcmd);  // 声明shell命令表开始
LOS_HAL_TABLE_END(g_shellcmdEnd, shellcmd);  // 声明shell命令表结束

/**
 * @brief 获取命令模块信息
 * @return CmdModInfo* 命令模块信息指针
 */
CmdModInfo *OsCmdInfoGet(VOID)
{
    return &g_cmdInfo;  // 返回全局命令模块信息指针
}

/**
 * @brief 释放命令解析参数内存
 * @param cmdParsed 命令解析结构体指针
 */
STATIC VOID OsFreeCmdPara(CmdParsed *cmdParsed)
{
    UINT32 i;  // 循环计数器
    for (i = 0; i < cmdParsed->paramCnt; i++) {  // 遍历所有参数
        if ((cmdParsed->paramArray[i]) != NULL) {  // 如果参数不为空
            (VOID)LOS_MemFree(m_aucSysMem0, (cmdParsed->paramArray[i]));  // 释放参数内存
            cmdParsed->paramArray[i] = NULL;  // 置空指针防止野指针
        }
    }
}

/**
 * @brief 处理Tab补全字符串获取
 * @param tabStr 输入字符串指针的指针
 * @param parsed 命令解析结构体指针
 * @param tabStrLen 输入字符串长度
 * @return INT32 操作结果，LOS_OK表示成功，其他值表示失败
 */
STATIC INT32 OsStrSeparateTabStrGet(CHAR **tabStr, CmdParsed *parsed, UINT32 tabStrLen)
{
    CHAR *shiftStr = NULL;  // 处理后的字符串指针
    CHAR *tempStr = (CHAR *)LOS_MemAlloc(m_aucSysMem0, SHOW_MAX_LEN << 1);  // 分配临时内存 (SHOW_MAX_LEN * 2)
    if (tempStr == NULL) {  // 内存分配失败检查
        return (INT32)OS_ERROR;  // 返回错误
    }

    (VOID)memset_s(tempStr, SHOW_MAX_LEN << 1, 0, SHOW_MAX_LEN << 1);  // 初始化临时内存为0
    shiftStr = tempStr + SHOW_MAX_LEN;  // 设置shiftStr指向临时内存后半部分

    if (strncpy_s(tempStr, SHOW_MAX_LEN - 1, *tabStr, tabStrLen)) {  // 复制输入字符串到tempStr
        (VOID)LOS_MemFree(m_aucSysMem0, tempStr);  // 复制失败，释放内存
        return (INT32)OS_ERROR;  // 返回错误
    }

    parsed->cmdType = CMD_TYPE_STD;  // 设置命令类型为标准命令

    /* 去除无用或重复空格 */
    if (OsCmdKeyShift(tempStr, shiftStr, SHOW_MAX_LEN - 1)) {  // 调用字符串处理函数
        (VOID)LOS_MemFree(m_aucSysMem0, tempStr);  // 处理失败，释放内存
        return (INT32)OS_ERROR;  // 返回错误
    }

    /* 获取需要补全的字符串准确位置 */
    /* 根据末尾是否有空格处理不同情况 */
    if ((strlen(shiftStr) == 0) || (tempStr[strlen(tempStr) - 1] != shiftStr[strlen(shiftStr) - 1])) {
        *tabStr = "";  // 设置tabStr为空字符串
    } else {
        if (OsCmdTokenSplit(shiftStr, ' ', parsed)) {  // 分割字符串为令牌
            (VOID)LOS_MemFree(m_aucSysMem0, tempStr);  // 分割失败，释放内存
            return (INT32)OS_ERROR;  // 返回错误
        }
        *tabStr = parsed->paramArray[parsed->paramCnt - 1];  // 设置tabStr为最后一个参数
    }

    (VOID)LOS_MemFree(m_aucSysMem0, tempStr);  // 释放临时内存
    return LOS_OK;  // 返回成功
}

/**
 * @brief 分离路径和文件名
 * @param tabStr 输入字符串
 * @param strPath 输出路径缓冲区
 * @param nameLooking 输出文件名缓冲区
 * @param tabStrLen 输入字符串长度
 * @return INT32 操作结果，LOS_OK表示成功，其他值表示失败
 */
STATIC INT32 OsStrSeparate(CHAR *tabStr, CHAR *strPath, CHAR *nameLooking, UINT32 tabStrLen)
{
    CHAR *strEnd = NULL;  // 字符串结束指针
    CHAR *cutPos = NULL;  // 分割位置指针
    CmdParsed parsed = {0};  // 命令解析结构体初始化
    CHAR *shellWorkingDirectory = OsShellGetWorkingDirectory();  // 获取shell工作目录
    INT32 ret;  // 函数返回值

    ret = OsStrSeparateTabStrGet(&tabStr, &parsed, tabStrLen);  // 处理Tab补全字符串
    if (ret != LOS_OK) {  // 检查处理结果
        return ret;  // 返回错误
    }

    /* 获取完整路径字符串 */
    if (*tabStr != '/') {  // 如果不是绝对路径
        if (strncpy_s(strPath, CMD_MAX_PATH, shellWorkingDirectory, CMD_MAX_PATH - 1)) {  // 复制工作目录到路径
            OsFreeCmdPara(&parsed);  // 释放参数
            return (INT32)OS_ERROR;  // 返回错误
        }
        if (strcmp(shellWorkingDirectory, "/")) {  // 如果工作目录不是根目录
            if (strncat_s(strPath, CMD_MAX_PATH, "/", CMD_MAX_PATH - strlen(strPath) - 1)) {  // 添加路径分隔符
                OsFreeCmdPara(&parsed);  // 释放参数
                return (INT32)OS_ERROR;  // 返回错误
            }
        }
    }

    if (strncat_s(strPath, CMD_MAX_PATH, tabStr, CMD_MAX_PATH - strlen(strPath) - 1)) {  // 拼接路径
        OsFreeCmdPara(&parsed);  // 释放参数
        return (INT32)OS_ERROR;  // 返回错误
    }

    /* 按最后一个'/'分割字符串 */
    strEnd = strrchr(strPath, '/');  // 查找最后一个'/'
    cutPos = strEnd;  // 保存分割位置
    if (strEnd != NULL) {  // 如果找到'/'
        if (strncpy_s(nameLooking, CMD_MAX_PATH, strEnd + 1, CMD_MAX_PATH - 1)) { /* 获取比较字符串 */
            OsFreeCmdPara(&parsed);  // 释放参数
            return (INT32)OS_ERROR;  // 返回错误
        }
        *(cutPos + 1) = '\0';  // 在'/'后添加结束符
    }

    OsFreeCmdPara(&parsed);  // 释放参数
    return LOS_OK;  // 返回成功
}

/**
 * @brief 分页显示输入控制
 * @return INT32 操作结果，1表示继续，0表示退出，OS_ERROR表示错误
 */
STATIC INT32 OsShowPageInputControl(VOID)
{
    CHAR readChar;  // 读取的字符

    while (1) {  // 无限循环等待输入
        if (read(STDIN_FILENO, &readChar, 1) != 1) { /* 从标准输入读取一个字符 */
            PRINTK("\n");  // 输出换行
            return (INT32)OS_ERROR;  // 返回错误
        }
        if ((readChar == 'q') || (readChar == 'Q') || (readChar == CTRL_C)) {  // 如果是q/Q或Ctrl+C
            PRINTK("\n");  // 输出换行
            return 0;  // 返回退出
        } else if (readChar == '\r') {  // 如果是回车
            PRINTK("\b \b\b \b\b \b\b \b\b \b\b \b\b \b\b \b");  // 清除--More--提示
            return 1;  // 返回继续
        }
    }
}

/**
 * @brief 分页显示控制
 * @param timesPrint 已打印次数
 * @param lineCap 每行容量
 * @param count 总数量
 * @return INT32 操作结果，1表示继续，0表示退出，其他值表示错误
 */
STATIC INT32 OsShowPageControl(UINT32 timesPrint, UINT32 lineCap, UINT32 count)
{
    if (NEED_NEW_LINE(timesPrint, lineCap)) {  // 需要新行
        PRINTK("\n");  // 输出换行
        if (SCREEN_IS_FULL(timesPrint, lineCap) && (timesPrint < count)) {  // 如果屏幕已满且未打印完
            PRINTK("--More--");  // 显示--More--提示
            return OsShowPageInputControl();  // 调用输入控制
        }
    }
    return 1;  // 返回继续
}

/**
 * @brief 确认是否显示所有内容
 * @param count 项目数量
 * @return INT32 操作结果，1表示显示，0表示不显示，OS_ERROR表示错误
 */
STATIC INT32 OsSurePrintAll(UINT32 count)
{
    CHAR readChar = 0;  // 读取的字符
    PRINTK("\nDisplay all %u possibilities?(y/n)", count);  // 显示确认信息
    while (1) {  // 无限循环等待输入
        if (read(0, &readChar, 1) != 1) {  // 从标准输入读取一个字符
            return (INT32)OS_ERROR;  // 返回错误
        }
        if ((readChar == 'n') || (readChar == 'N') || (readChar == CTRL_C)) {  // 如果是n/N或Ctrl+C
            PRINTK("\n");  // 输出换行
            return 0;  // 返回不显示
        } else if ((readChar == 'y') || (readChar == 'Y') || (readChar == '\r')) {  // 如果是y/Y或回车
            return 1;  // 返回显示
        }
    }
}

/**
 * @brief 打印匹配列表
 * @param count 匹配数量
 * @param strPath 路径
 * @param nameLooking 查找的名称
 * @param printLen 打印长度
 * @return INT32 操作结果，LOS_OK表示成功，其他值表示失败
 */
STATIC INT32 OsPrintMatchList(UINT32 count, const CHAR *strPath, const CHAR *nameLooking, UINT32 printLen)
{
    UINT32 timesPrint = 0;  // 已打印次数
    UINT32 lineCap;  // 每行容量
    INT32 ret;  // 函数返回值
    DIR *openDir = NULL;  // 目录指针
    struct dirent *readDir = NULL;  // 目录项指针
    CHAR formatChar[10] = {0}; /* 10:格式字符串长度 */

    printLen = (printLen > (DEFAULT_SCREEN_WIDTH - 2)) ? (DEFAULT_SCREEN_WIDTH - 2) : printLen; /* 2:保留2字节 */
    lineCap = DEFAULT_SCREEN_WIDTH / (printLen + 2); /* 2:DEFAULT_SCREEN_WIDTH保留2字节 */
    if (snprintf_s(formatChar, sizeof(formatChar) - 1, 7, "%%-%us  ", printLen) < 0) { /* 7:格式长度 */
        return (INT32)OS_ERROR;  // 格式化字符串失败，返回错误
    }

    if (count > (lineCap * DEFAULT_SCREEN_HEIGHT)) {  // 如果数量超过一屏
        ret = OsSurePrintAll(count);  // 确认是否显示所有
        if (ret != 1) {  // 如果不显示所有
            return ret;  // 返回结果
        }
    }
    openDir = opendir(strPath);  // 打开目录
    if (openDir == NULL) {  // 打开目录失败检查
        return (INT32)OS_ERROR;  // 返回错误
    }

    PRINTK("\n");  // 输出换行
    for (readDir = readdir(openDir); readDir != NULL; readDir = readdir(openDir)) {  // 遍历目录
        if (strncmp(nameLooking, readDir->d_name, strlen(nameLooking)) != 0) {  // 比较名称
            continue;  // 不匹配则跳过
        }
        PRINTK(formatChar, readDir->d_name);  // 打印匹配的名称
        timesPrint++;  // 增加打印次数
        ret = OsShowPageControl(timesPrint, lineCap, count);  // 分页控制
        if (ret != 1) {  // 如果需要退出
            if (closedir(openDir) < 0) {  // 关闭目录
                return (INT32)OS_ERROR;  // 返回错误
            }
            return ret;  // 返回结果
        }
    }

    PRINTK("\n");  // 输出换行
    if (closedir(openDir) < 0) {  // 关闭目录
        return (INT32)OS_ERROR;  // 返回错误
    }

    return LOS_OK;  // 返回成功
}

/**
 * @brief 比较字符串并截断
 * @param s1 源字符串1
 * @param s2 源字符串2
 * @param n 比较长度
 */
STATIC VOID strncmp_cut(const CHAR *s1, CHAR *s2, size_t n)
{
    if ((n == 0) || (s1 == NULL) || (s2 == NULL)) {  // 参数检查
        return;  // 直接返回
    }
    do {
        if (*s1 && *s2 && (*s1 == *s2)) {  // 如果字符相等且不为结束符
            s1++;  // 移动s1指针
            s2++;  // 移动s2指针
        } else {
            break;  // 不相等则跳出循环
        }
    } while (--n != 0);  // 循环直到n为0
    if (n > 0) {  // 如果还有剩余长度
        /* 用NULL填充剩余的n-1字节 */
        while (n-- != 0)
            *s2++ = 0;  // 设置为结束符
    }
    return;  // 返回
}

/**
 * @brief 执行名称匹配
 * @param strPath 路径
 * @param nameLooking 查找的名称
 * @param strObj 输出匹配的对象
 * @param maxLen 输出最大长度
 * @return INT32 匹配数量，OS_ERROR表示失败
 */
STATIC INT32 OsExecNameMatch(const CHAR *strPath, const CHAR *nameLooking, CHAR *strObj, UINT32 *maxLen)
{
    INT32 count = 0;  // 匹配计数
    DIR *openDir = NULL;  // 目录指针
    struct dirent *readDir = NULL;  // 目录项指针

    openDir = opendir(strPath);  // 打开目录
    if (openDir == NULL) {  // 打开目录失败检查
        return (INT32)OS_ERROR;  // 返回错误
    }

    for (readDir = readdir(openDir); readDir != NULL; readDir = readdir(openDir)) {  // 遍历目录
        if (strncmp(nameLooking, readDir->d_name, strlen(nameLooking)) != 0) {  // 比较名称
            continue;  // 不匹配则跳过
        }
        if (count == 0) {  // 第一个匹配项
            if (strncpy_s(strObj, CMD_MAX_PATH, readDir->d_name, CMD_MAX_PATH - 1)) {  // 复制名称
                (VOID)closedir(openDir);  // 关闭目录
                return (INT32)OS_ERROR;  // 返回错误
            }
            *maxLen = strlen(readDir->d_name);  // 设置最大长度
        } else {  // 后续匹配项
            /* 比较并截断匹配名称的相同字符串 */
            strncmp_cut(readDir->d_name, strObj, strlen(strObj));  // 截断字符串
            if (strlen(readDir->d_name) > *maxLen) {  // 更新最大长度
                *maxLen = strlen(readDir->d_name);  // 设置新的最大长度
            }
        }
        count++;  // 增加计数
    }

    if (closedir(openDir) < 0) {  // 关闭目录
        return (INT32)OS_ERROR;  // 返回错误
    }

    return count;  // 返回匹配数量
}

/**
 * @brief 补全字符串
 * @param result 结果字符串
 * @param target 目标字符串
 * @param cmdKey 命令键
 * @param len 长度指针
 */
STATIC VOID OsCompleteStr(const CHAR *result, const CHAR *target, CHAR *cmdKey, UINT32 *len)
{
    UINT32 size = strlen(result) - strlen(target);  // 计算需要补全的大小
    CHAR *des = cmdKey + *len;  // 目标位置指针
    CHAR *src = (CHAR *)result + strlen(target);  // 源位置指针

    while (size-- > 0) {  // 循环补全字符
        PRINTK("%c", *src);  // 打印字符
        if (*len == (SHOW_MAX_LEN - 1)) {  // 检查长度是否达到最大值
            *des = '\0';  // 添加结束符
            break;  // 跳出循环
        }
        *des++ = *src++;  // 复制字符
        (*len)++;  // 增加长度
    }
}

/**
 * @brief Tab匹配命令
 * @param cmdKey 命令键
 * @param len 长度指针
 * @return INT32 匹配数量，负数表示错误
 */
STATIC INT32 OsTabMatchCmd(CHAR *cmdKey, UINT32 *len)
{
    INT32 count = 0;  // 匹配计数
    INT32 ret;  // 函数返回值
    CmdItemNode *cmdItemGuard = NULL;  // 命令项守卫指针
    CmdItemNode *curCmdItem = NULL;  // 当前命令项指针
    const CHAR *cmdMajor = (const CHAR *)cmdKey;  // 命令主串

    while (*cmdMajor == 0x20) { /* 去除左侧空格 */
        cmdMajor++;  // 移动指针
    }

    if (LOS_ListEmpty(&(g_cmdInfo.cmdList.list))) {  // 检查命令列表是否为空
        return (INT32)OS_ERROR;  // 返回错误
    }

    LOS_DL_LIST_FOR_EACH_ENTRY(curCmdItem, &(g_cmdInfo.cmdList.list), CmdItemNode, list) {  // 遍历命令列表
        if ((curCmdItem == NULL) || (curCmdItem->cmd == NULL)) {  // 检查命令项是否有效
            return -1;  // 返回错误
        }

        if (strncmp(cmdMajor, curCmdItem->cmd->cmdKey, strlen(cmdMajor)) > 0) {  // 比较命令键
            continue;  // 不匹配则跳过
        }

        if (strncmp(cmdMajor, curCmdItem->cmd->cmdKey, strlen(cmdMajor)) != 0) {  // 比较命令键
            break;  // 不匹配则跳出循环
        }

        if (count == 0) {  // 第一个匹配项
            cmdItemGuard = curCmdItem;  // 设置守卫指针
        }
        ++count;  // 增加计数
    }

    if (cmdItemGuard == NULL) {  // 没有匹配项
        return 0;  // 返回0
    }

    if (count == 1) {  // 只有一个匹配项
        OsCompleteStr(cmdItemGuard->cmd->cmdKey, cmdMajor, cmdKey, len);  // 补全字符串
    }

    ret = count;  // 保存计数
    if (count > 1) {  // 多个匹配项
        PRINTK("\n");  // 输出换行
        while (count--) {  // 遍历所有匹配项
            PRINTK("%s  ", cmdItemGuard->cmd->cmdKey);  // 打印命令键
            cmdItemGuard = LOS_DL_LIST_ENTRY(cmdItemGuard->list.pstNext, CmdItemNode, list);  // 移动到下一个
        }
        PRINTK("\n");  // 输出换行
    }

    return ret;  // 返回匹配数量
}

/**
 * @brief Tab匹配文件
 * @param cmdKey 命令键
 * @param len 长度指针
 * @return INT32 匹配数量，OS_ERROR表示失败
 */
STATIC INT32 OsTabMatchFile(CHAR *cmdKey, UINT32 *len)
{
    UINT32 maxLen = 0;  // 最大长度
    INT32 count;  // 匹配数量
    CHAR *strOutput = NULL;  // 输出字符串指针
    CHAR *strCmp = NULL;  // 比较字符串指针
    CHAR *dirOpen = (CHAR *)LOS_MemAlloc(m_aucSysMem0, CMD_MAX_PATH * 3); /* 3:dirOpen\strOutput\strCmp */
    if (dirOpen == NULL) {  // 内存分配失败检查
        return (INT32)OS_ERROR;  // 返回错误
    }

    (VOID)memset_s(dirOpen, CMD_MAX_PATH * 3, 0, CMD_MAX_PATH * 3); /* 3:dirOpen\strOutput\strCmp */
    strOutput = dirOpen + CMD_MAX_PATH;  // 设置输出字符串指针
    strCmp = strOutput + CMD_MAX_PATH;  // 设置比较字符串指针

    if (OsStrSeparate(cmdKey, dirOpen, strCmp, *len)) {  // 分离路径和文件名
        (VOID)LOS_MemFree(m_aucSysMem0, dirOpen);  // 释放内存
        return (INT32)OS_ERROR;  // 返回错误
    }

    count = OsExecNameMatch(dirOpen, strCmp, strOutput, &maxLen);  // 执行名称匹配
    /* 一个或多个匹配 */
    if (count >= 1) {  // 如果有匹配项
        OsCompleteStr(strOutput, strCmp, cmdKey, len);  // 补全字符串

        if (count == 1) {  // 只有一个匹配项
            (VOID)LOS_MemFree(m_aucSysMem0, dirOpen);  // 释放内存
            return 1;  // 返回1
        }
        if (OsPrintMatchList((UINT32)count, dirOpen, strCmp, maxLen) == -1) {  // 打印匹配列表
            (VOID)LOS_MemFree(m_aucSysMem0, dirOpen);  // 释放内存
            return (INT32)OS_ERROR;  // 返回错误
        }
    }

    (VOID)LOS_MemFree(m_aucSysMem0, dirOpen);  // 释放内存
    return count;  // 返回匹配数量
}

/**
 * @brief 处理命令字符串，清除无用空格
 * @param cmdKey 输入字符串
 * @param cmdOut 输出字符串
 * @param size 输出缓冲区大小
 * @return UINT32 操作结果，LOS_OK表示成功，其他值表示失败
 * @note 清除规则：
 *       1) 清除引号区域外的多余空格，将多个空格压缩为一个
 *       2) 清除第一个有效字符前的所有空格
 */
LITE_OS_SEC_TEXT_MINOR UINT32 OsCmdKeyShift(const CHAR *cmdKey, CHAR *cmdOut, UINT32 size)
{
    CHAR *output = NULL;  // 输出缓冲区指针
    CHAR *outputBak = NULL;  // 输出缓冲区备份指针
    UINT32 len;  // 字符串长度
    INT32 ret;  // 函数返回值
    BOOL quotes = FALSE;  // 引号状态标志

    if ((cmdKey == NULL) || (cmdOut == NULL)) {  // 参数检查
        return (UINT32)OS_ERROR;  // 返回错误
    }

    len = strlen(cmdKey);  // 获取输入字符串长度
    if (len >= size) {  // 检查长度是否超过输出缓冲区大小
        return (UINT32)OS_ERROR;  // 返回错误
    }
    output = (CHAR*)LOS_MemAlloc(m_aucSysMem0, len + 1);  // 分配输出缓冲区
    if (output == NULL) {  // 内存分配失败检查
        PRINTK("malloc failure in %s[%d]", __FUNCTION__, __LINE__);  // 打印错误信息
        return (UINT32)OS_ERROR;  // 返回错误
    }
    /* 备份output起始地址 */
    outputBak = output;  // 保存输出缓冲区起始地址
    /* 扫描cmdKey中的每个字符，压缩多余空格并忽略无效字符 */
    for (; *cmdKey != '\0'; cmdKey++) {  // 遍历输入字符串
        /* 检测到双引号，切换匹配状态 */
        if (*(cmdKey) == '\"') {  // 如果是双引号
            SWITCH_QUOTES_STATUS(quotes);  // 切换引号状态
        }
        /* 在以下情况忽略当前字符 */
        /* 1) 引号匹配状态为FALSE（表示空格不在双引号标记范围内） */
        /* 2) 当前字符是空格 */
        /* 3) 下一个字符也是空格，或者字符串已到达末尾(\0) */
        /* 4) 无效字符，如单引号 */
        if ((*cmdKey == ' ') && ((*(cmdKey + 1) == ' ') || (*(cmdKey + 1) == '\0')) && QUOTES_STATUS_CLOSE(quotes)) {
            continue;  // 忽略当前字符
        }
        if (*cmdKey == '\'') {  // 如果是单引号
            continue;  // 忽略当前字符
        }
        *output = *cmdKey;  // 复制字符到输出缓冲区
        output++;  // 移动输出指针
    }
    *output = '\0';  // 添加结束符
    /* 恢复output起始地址 */
    output = outputBak;  // 恢复输出缓冲区起始地址
    len = strlen(output);  // 获取处理后字符串长度
    /* 清除缓冲区第一个字符处的空格 */
    if (*outputBak == ' ') {  // 如果第一个字符是空格
        output++;  // 移动输出指针
        len--;  // 减少长度
    }
    /* 复制处理后的缓冲区 */
    ret = strncpy_s(cmdOut, size, output, len);  // 复制到输出缓冲区
    if (ret != EOK) {  // 检查复制结果
        PRINT_ERR("%s,%d strncpy_s failed, err:%d!\n", __FUNCTION__, __LINE__, ret);  // 打印错误信息
        (VOID)LOS_MemFree(m_aucSysMem0, output);  // 释放内存
        return OS_ERROR;  // 返回错误
    }
    cmdOut[len] = '\0';  // 添加结束符

    (VOID)LOS_MemFree(m_aucSysMem0, output);  // 释放内存

    return LOS_OK;  // 返回成功
}
/**
 * @brief 检查命令关键字合法性
 * @param cmdKey 命令关键字字符串
 * @return BOOL 合法性结果，TRUE表示合法，FALSE表示非法
 */
LITE_OS_SEC_TEXT_MINOR BOOL OsCmdKeyCheck(const CHAR *cmdKey)
{
    const CHAR *temp = cmdKey;  // 临时指针用于遍历字符串
    enum Stat {  // 状态枚举：无状态、数字状态、其他字符状态
        STAT_NONE,
        STAT_DIGIT,
        STAT_OTHER
    } state = STAT_NONE;  // 初始状态为无状态

    if (strlen(cmdKey) >= CMD_KEY_LEN) {  // 检查命令关键字长度是否超过限制
        return FALSE;  // 超过长度限制，返回非法
    }

    while (*temp != '\0') {  // 遍历命令关键字字符串
        // 检查字符是否为数字、字母、下划线或连字符
        if (!((*temp <= '9') && (*temp >= '0')) &&
            !((*temp <= 'z') && (*temp >= 'a')) &&
            !((*temp <= 'Z') && (*temp >= 'A')) &&
            (*temp != '_') && (*temp != '-')) {
            return FALSE;  // 包含非法字符，返回非法
        }

        if ((*temp >= '0') && (*temp <= '9')) {  // 如果是数字
            if (state == STAT_NONE) {  // 且当前状态为无状态
                state = STAT_DIGIT;  // 切换到数字状态
            }
        } else {  // 如果是字母、下划线或连字符
            state = STAT_OTHER;  // 切换到其他字符状态
        }

        temp++;  // 移动到下一个字符
    }

    if (state == STAT_DIGIT) {  // 如果最终状态为数字状态（命令关键字全为数字）
        return FALSE;  // 返回非法
    }

    return TRUE;  // 命令关键字合法，返回TRUE
}

/**
 * @brief Tab补全功能实现
 * @param cmdKey 命令关键字字符串
 * @param len 字符串长度指针
 * @return INT32 补全结果数量，负数表示错误
 */
LITE_OS_SEC_TEXT_MINOR INT32 OsTabCompletion(CHAR *cmdKey, UINT32 *len)
{
    INT32 count = 0;  // 补全结果计数
    CHAR *space = NULL;  // 空格指针
    CHAR *cmdMainStr = cmdKey;  // 命令主字符串指针

    if ((cmdKey == NULL) || (len == NULL)) {  // 参数检查
        return (INT32)OS_ERROR;  // 参数无效，返回错误
    }

    /* 去除左侧空格 */
    while (*cmdMainStr == 0x20) {  // 遍历空格字符
        cmdMainStr++;  // 移动指针跳过空格
    }

    /* 尝试在剩余字符串中查找空格 */
    space = strrchr(cmdMainStr, 0x20);  // 查找最后一个空格
    if ((space == NULL) && (*cmdMainStr != '\0')) {  // 如果没有空格且字符串非空
        count = OsTabMatchCmd(cmdKey, len);  // 尝试匹配命令
    }

    if (count == 0) {  // 如果命令匹配数量为0
        count = OsTabMatchFile(cmdKey, len);  // 尝试匹配文件
    }

    return count;  // 返回匹配数量
}

/**
 * @brief 按升序插入命令项到链表
 * @param cmd 要插入的命令项节点
 */
LITE_OS_SEC_TEXT_MINOR VOID OsCmdAscendingInsert(CmdItemNode *cmd)
{
    CmdItemNode *cmdItem = NULL;  // 当前命令项节点
    CmdItemNode *cmdNext = NULL;  // 下一个命令项节点

    if (cmd == NULL) {  // 参数检查
        return;  // 命令项为空，直接返回
    }

    // 从链表尾部开始遍历，寻找插入位置
    for (cmdItem = LOS_DL_LIST_ENTRY((&g_cmdInfo.cmdList.list)->pstPrev, CmdItemNode, list);
         &cmdItem->list != &(g_cmdInfo.cmdList.list);) {
        cmdNext = LOS_DL_LIST_ENTRY(cmdItem->list.pstPrev, CmdItemNode, list);  // 获取前一个节点
        if (&cmdNext->list != &(g_cmdInfo.cmdList.list)) {  // 如果不是头节点
            // 找到插入位置：当前节点关键字 >= 待插入关键字 且 前一个节点关键字 < 待插入关键字
            if ((strncmp(cmdItem->cmd->cmdKey, cmd->cmd->cmdKey, strlen(cmd->cmd->cmdKey)) >= 0) &&
                (strncmp(cmdNext->cmd->cmdKey, cmd->cmd->cmdKey, strlen(cmd->cmd->cmdKey)) < 0)) {
                LOS_ListTailInsert(&(cmdItem->list), &(cmd->list));  // 插入节点
                return;  // 插入完成，返回
            }
            cmdItem = cmdNext;  // 移动到前一个节点
        } else {  // 如果是头节点
            // 如果待插入关键字大于当前节点关键字
            if (strncmp(cmd->cmd->cmdKey, cmdItem->cmd->cmdKey, strlen(cmd->cmd->cmdKey)) > 0) {
                cmdItem = cmdNext;  // 移动到前一个节点（头节点）
            }
            break;  // 跳出循环
        }
    }

    LOS_ListTailInsert(&(cmdItem->list), &(cmd->list));  // 插入节点到链表尾部
}

/**
 * @brief 初始化Shell按键相关资源
 * @param shellCB Shell控制块指针
 * @return UINT32 操作结果，LOS_OK表示成功，其他值表示失败
 */
LITE_OS_SEC_TEXT_MINOR UINT32 OsShellKeyInit(ShellCB *shellCB)
{
    CmdKeyLink *cmdKeyLink = NULL;  // 命令关键字链表
    CmdKeyLink *cmdHistoryLink = NULL;  // 命令历史链表

    if (shellCB == NULL) {  // 参数检查
        return OS_ERROR;  // Shell控制块为空，返回错误
    }
    cmdKeyLink = (CmdKeyLink *)LOS_MemAlloc(m_aucSysMem0, sizeof(CmdKeyLink));  // 分配命令关键字链表内存
    if (cmdKeyLink == NULL) {  // 内存分配检查
        PRINT_ERR("Shell CmdKeyLink memory alloc error!\n");  // 打印错误信息
        return OS_ERROR;  // 返回错误
    }
    cmdHistoryLink = (CmdKeyLink *)LOS_MemAlloc(m_aucSysMem0, sizeof(CmdKeyLink));  // 分配命令历史链表内存
    if (cmdHistoryLink == NULL) {  // 内存分配检查
        (VOID)LOS_MemFree(m_aucSysMem0, cmdKeyLink);  // 释放已分配的命令关键字链表内存
        PRINT_ERR("Shell CmdHistoryLink memory alloc error!\n");  // 打印错误信息
        return OS_ERROR;  // 返回错误
    }

    cmdKeyLink->count = 0;  // 初始化计数为0
    LOS_ListInit(&(cmdKeyLink->list));  // 初始化链表
    shellCB->cmdKeyLink = (VOID *)cmdKeyLink;  // 设置Shell控制块中的命令关键字链表

    cmdHistoryLink->count = 0;  // 初始化计数为0
    LOS_ListInit(&(cmdHistoryLink->list));  // 初始化链表
    shellCB->cmdHistoryKeyLink = (VOID *)cmdHistoryLink;  // 设置Shell控制块中的命令历史链表
    shellCB->cmdMaskKeyLink = (VOID *)cmdHistoryLink;  // 设置Shell控制块中的命令掩码链表
    return LOS_OK;  // 返回成功
}

/**
 * @brief 反初始化命令关键字链表
 * @param cmdKeyLink 命令关键字链表指针
 */
LITE_OS_SEC_TEXT_MINOR VOID OsShellKeyDeInit(CmdKeyLink *cmdKeyLink)
{
    CmdKeyLink *cmdtmp = NULL;  // 临时命令关键字链表节点
    if (cmdKeyLink == NULL) {  // 参数检查
        return;  // 链表为空，直接返回
    }

    while (!LOS_ListEmpty(&(cmdKeyLink->list))) {  // 遍历链表直到为空
        cmdtmp = LOS_DL_LIST_ENTRY(cmdKeyLink->list.pstNext, CmdKeyLink, list);  // 获取下一个节点
        LOS_ListDelete(&cmdtmp->list);  // 从链表中删除节点
        (VOID)LOS_MemFree(m_aucSysMem0, cmdtmp);  // 释放节点内存
    }

    cmdKeyLink->count = 0;  // 重置计数
    (VOID)LOS_MemFree(m_aucSysMem0, cmdKeyLink);  // 释放链表内存
}

/**
 * @brief 注册系统命令到Shell
 * @return UINT32 操作结果，LOS_OK表示成功，其他值表示失败
 */
LITE_OS_SEC_TEXT_MINOR UINT32 OsShellSysCmdRegister(VOID)
{
    UINT32 i;  // 循环计数器
    UINT8 *cmdItemGroup = NULL;  // 命令项组指针
    // 计算命令项数量：(命令表结束地址 - 命令表开始地址) / 单个命令项大小
    UINT32 index = ((UINTPTR)(&g_shellcmdEnd) - (UINTPTR)(&g_shellcmd[0])) / sizeof(CmdItem);
    CmdItemNode *cmdItem = NULL;  // 命令项节点指针

    // 分配命令项组内存：命令项数量 * 单个命令项节点大小
    cmdItemGroup = (UINT8 *)LOS_MemAlloc(m_aucSysMem0, index * sizeof(CmdItemNode));
    if (cmdItemGroup == NULL) {  // 内存分配检查
        PRINT_ERR("[%s]System memory allocation failure!\n", __FUNCTION__);  // 打印错误信息
        return (UINT32)OS_ERROR;  // 返回错误
    }

    for (i = 0; i < index; ++i) {  // 遍历所有命令项
        cmdItem = (CmdItemNode *)(cmdItemGroup + i * sizeof(CmdItemNode));  // 获取当前命令项节点
        cmdItem->cmd = &g_shellcmd[i];  // 设置命令项
        OsCmdAscendingInsert(cmdItem);  // 按升序插入命令项到链表
    }
    g_cmdInfo.listNum += index;  // 更新命令列表数量
    return LOS_OK;  // 返回成功
}

/**
 * @brief 将命令字符串推入命令链表
 * @param string 命令字符串
 * @param cmdKeyLink 命令链表指针
 */
LITE_OS_SEC_TEXT_MINOR VOID OsShellCmdPush(const CHAR *string, CmdKeyLink *cmdKeyLink)
{
    CmdKeyLink *cmdNewNode = NULL;  // 新命令节点
    UINT32 len;  // 字符串长度

    if ((string == NULL) || (strlen(string) == 0)) {  // 参数检查
        return;  // 字符串为空，直接返回
    }

    len = strlen(string);  // 获取字符串长度
    // 分配新节点内存：命令链表节点大小 + 字符串长度 + 1（结束符）
    cmdNewNode = (CmdKeyLink *)LOS_MemAlloc(m_aucSysMem0, sizeof(CmdKeyLink) + len + 1);
    if (cmdNewNode == NULL) {  // 内存分配检查
        return;  // 分配失败，直接返回
    }

    // 初始化新节点内存
    (VOID)memset_s(cmdNewNode, sizeof(CmdKeyLink) + len + 1, 0, sizeof(CmdKeyLink) + len + 1);
    if (strncpy_s(cmdNewNode->cmdString, len + 1, string, len)) {  // 复制命令字符串到新节点
        (VOID)LOS_MemFree(m_aucSysMem0, cmdNewNode);  // 复制失败，释放内存
        return;  // 返回
    }

    LOS_ListTailInsert(&(cmdKeyLink->list), &(cmdNewNode->list));  // 将新节点插入链表尾部

    return;  // 返回
}

/**
 * @brief 显示命令历史记录
 * @param value 操作类型，CMD_KEY_UP表示上翻，CMD_KEY_DOWN表示下翻
 * @param shellCB Shell控制块指针
 */
LITE_OS_SEC_TEXT_MINOR VOID OsShellHistoryShow(UINT32 value, ShellCB *shellCB)
{
    CmdKeyLink *cmdtmp = NULL;  // 临时命令节点
    CmdKeyLink *cmdNode = shellCB->cmdHistoryKeyLink;  // 命令历史链表头节点
    CmdKeyLink *cmdMask = shellCB->cmdMaskKeyLink;  // 当前命令掩码节点
    errno_t ret;  // 函数返回值

    (VOID)pthread_mutex_lock(&shellCB->historyMutex);  // 加锁保护历史记录
    if (value == CMD_KEY_DOWN) {  // 如果是下翻操作
        if (cmdMask == cmdNode) {  // 如果当前节点是头节点
            goto END;  // 跳转到结束
        }

        // 获取下一个节点
        cmdtmp = LOS_DL_LIST_ENTRY(cmdMask->list.pstNext, CmdKeyLink, list);
        if (cmdtmp != cmdNode) {  // 如果下一个节点不是头节点
            cmdMask = cmdtmp;  // 更新当前节点为下一个节点
        } else {  // 如果下一个节点是头节点
            goto END;  // 跳转到结束
        }
    } else if (value == CMD_KEY_UP) {  // 如果是上翻操作
        // 获取上一个节点
        cmdtmp = LOS_DL_LIST_ENTRY(cmdMask->list.pstPrev, CmdKeyLink, list);
        if (cmdtmp != cmdNode) {  // 如果上一个节点不是头节点
            cmdMask = cmdtmp;  // 更新当前节点为上一个节点
        } else {  // 如果上一个节点是头节点
            goto END;  // 跳转到结束
        }
    }

    while (shellCB->shellBufOffset--) {  // 清除当前输入缓冲区
        PRINTK("\b \b");  // 退格并清除字符
    }
    PRINTK("%s", cmdMask->cmdString);  // 打印当前历史命令
    shellCB->shellBufOffset = strlen(cmdMask->cmdString);  // 更新缓冲区偏移
    (VOID)memset_s(shellCB->shellBuf, SHOW_MAX_LEN, 0, SHOW_MAX_LEN);  // 清空缓冲区
    // 复制历史命令到缓冲区
    ret = memcpy_s(shellCB->shellBuf, SHOW_MAX_LEN, cmdMask->cmdString, shellCB->shellBufOffset);
    if (ret != EOK) {  // 复制检查
        PRINT_ERR("%s, %d memcpy failed!\n", __FUNCTION__, __LINE__);  // 打印错误信息
        goto END;  // 跳转到结束
    }
    shellCB->cmdMaskKeyLink = (VOID *)cmdMask;  // 更新当前掩码节点

END:
    (VOID)pthread_mutex_unlock(&shellCB->historyMutex);  // 解锁
    return;  // 返回
}

/**
 * @brief 执行命令
 * @param cmdParsed 命令解析结构体指针
 * @param cmdStr 命令字符串
 * @return UINT32 执行结果，LOS_OK表示成功，其他值表示失败
 */
LITE_OS_SEC_TEXT_MINOR UINT32 OsCmdExec(CmdParsed *cmdParsed, CHAR *cmdStr)
{
    UINT32 ret;  // 函数返回值
    CmdCallBackFunc cmdHook = NULL;  // 命令回调函数
    CmdItemNode *curCmdItem = NULL;  // 当前命令项节点
    UINT32 i;  // 循环计数器
    const CHAR *cmdKey = NULL;  // 命令关键字

    if ((cmdParsed == NULL) || (cmdStr == NULL) || (strlen(cmdStr) == 0)) {  // 参数检查
        return (UINT32)OS_ERROR;  // 参数无效，返回错误
    }

    ret = OsCmdParse(cmdStr, cmdParsed);  // 解析命令字符串
    if (ret != LOS_OK) {  // 解析检查
        goto OUT;  // 跳转到清理
    }

    // 遍历命令列表查找匹配的命令
    LOS_DL_LIST_FOR_EACH_ENTRY(curCmdItem, &(g_cmdInfo.cmdList.list), CmdItemNode, list) {
        cmdKey = curCmdItem->cmd->cmdKey;  // 获取命令关键字
        // 检查命令类型和关键字是否匹配
        if ((cmdParsed->cmdType == curCmdItem->cmd->cmdType) &&
            (strlen(cmdKey) == strlen(cmdParsed->cmdKeyword)) &&
            (strncmp(cmdKey, (CHAR *)(cmdParsed->cmdKeyword), strlen(cmdKey)) == 0)) {
            cmdHook = curCmdItem->cmd->cmdHook;  // 获取命令回调函数
            break;  // 找到匹配命令，跳出循环
        }
    }

    ret = OS_ERROR;  // 默认返回错误
    if (cmdHook != NULL) {  // 如果找到命令回调函数
        // 调用命令回调函数，传入参数数量和参数数组
        ret = (cmdHook)(cmdParsed->paramCnt, (const CHAR **)cmdParsed->paramArray);
    }

OUT:
    for (i = 0; i < cmdParsed->paramCnt; i++) {  // 遍历参数数组
        if (cmdParsed->paramArray[i] != NULL) {  // 如果参数不为空
            (VOID)LOS_MemFree(m_aucSysMem0, cmdParsed->paramArray[i]);  // 释放参数内存
            cmdParsed->paramArray[i] = NULL;  // 置空指针防止野指针
        }
    }

    return (UINT32)ret;  // 返回执行结果
}

/**
 * @brief 初始化命令模块
 * @return UINT32 操作结果，LOS_OK表示成功，其他值表示失败
 */
LITE_OS_SEC_TEXT_MINOR UINT32 OsCmdInit(VOID)
{
    UINT32 ret;  // 函数返回值
    LOS_ListInit(&(g_cmdInfo.cmdList.list));  // 初始化命令列表
    g_cmdInfo.listNum = 0;  // 初始化命令数量为0
    g_cmdInfo.initMagicFlag = SHELL_INIT_MAGIC_FLAG;  // 设置初始化魔术标志
    ret = LOS_MuxInit(&g_cmdInfo.muxLock, NULL);  // 初始化互斥锁
    if (ret != LOS_OK) {  // 初始化检查
        PRINT_ERR("Create mutex for shell cmd info failed\n");  // 打印错误信息
        return OS_ERROR;  // 返回错误
    }
    return LOS_OK;  // 返回成功
}

/**
 * @brief 创建命令项
 * @param cmdType 命令类型
 * @param cmdKey 命令关键字
 * @param paraNum 参数数量
 * @param cmdProc 命令处理函数
 * @return UINT32 操作结果，LOS_OK表示成功，其他值表示失败
 */
STATIC UINT32 OsCmdItemCreate(CmdType cmdType, const CHAR *cmdKey, UINT32 paraNum, CmdCallBackFunc cmdProc)
{
    CmdItem *cmdItem = NULL;  // 命令项指针
    CmdItemNode *cmdItemNode = NULL;  // 命令项节点指针

    cmdItem = (CmdItem *)LOS_MemAlloc(m_aucSysMem0, sizeof(CmdItem));  // 分配命令项内存
    if (cmdItem == NULL) {  // 内存分配检查
        return OS_ERRNO_SHELL_CMDREG_MEMALLOC_ERROR;  // 返回内存分配错误
    }
    (VOID)memset_s(cmdItem, sizeof(CmdItem), '\0', sizeof(CmdItem));  // 初始化命令项内存

    // 分配命令项节点内存
    cmdItemNode = (CmdItemNode *)LOS_MemAlloc(m_aucSysMem0, sizeof(CmdItemNode));
    if (cmdItemNode == NULL) {  // 内存分配检查
        (VOID)LOS_MemFree(m_aucSysMem0, cmdItem);  // 释放命令项内存
        return OS_ERRNO_SHELL_CMDREG_MEMALLOC_ERROR;  // 返回内存分配错误
    }
    (VOID)memset_s(cmdItemNode, sizeof(CmdItemNode), '\0', sizeof(CmdItemNode));  // 初始化命令项节点内存
    cmdItemNode->cmd = cmdItem;  // 设置命令项节点关联的命令项
    cmdItemNode->cmd->cmdHook = cmdProc;  // 设置命令回调函数
    cmdItemNode->cmd->paraNum = paraNum;  // 设置参数数量
    cmdItemNode->cmd->cmdType = cmdType;  // 设置命令类型
    cmdItemNode->cmd->cmdKey = cmdKey;  // 设置命令关键字

    (VOID)LOS_MuxLock(&g_cmdInfo.muxLock, LOS_WAIT_FOREVER);  // 加锁保护命令列表
    OsCmdAscendingInsert(cmdItemNode);  // 按升序插入命令项节点到链表
    g_cmdInfo.listNum++;  // 增加命令列表数量
    (VOID)LOS_MuxUnlock(&g_cmdInfo.muxLock);  // 解锁

    return LOS_OK;  // 返回成功
}

/* 公开API */
/**
 * @brief 注册命令到Shell
 * @param cmdType 命令类型
 * @param cmdKey 命令关键字
 * @param paraNum 参数数量
 * @param cmdProc 命令处理函数
 * @return UINT32 操作结果，LOS_OK表示成功，其他值表示失败
 */
LITE_OS_SEC_TEXT_MINOR UINT32 osCmdReg(CmdType cmdType, const CHAR *cmdKey, UINT32 paraNum, CmdCallBackFunc cmdProc)
{
    CmdItemNode *cmdItemNode = NULL;  // 命令项节点指针

    (VOID)LOS_MuxLock(&g_cmdInfo.muxLock, LOS_WAIT_FOREVER);  // 加锁
    if (g_cmdInfo.initMagicFlag != SHELL_INIT_MAGIC_FLAG) {  // 检查命令模块是否已初始化
        (VOID)LOS_MuxUnlock(&g_cmdInfo.muxLock);  // 解锁
        PRINT_ERR("[%s] shell is not yet initialized!\n", __FUNCTION__);  // 打印错误信息
        return OS_ERRNO_SHELL_NOT_INIT;  // 返回未初始化错误
    }
    (VOID)LOS_MuxUnlock(&g_cmdInfo.muxLock);  // 解锁

    // 参数合法性检查
    if ((cmdProc == NULL) || (cmdKey == NULL) ||
        (cmdType >= CMD_TYPE_BUTT) || (strlen(cmdKey) >= CMD_KEY_LEN) || !strlen(cmdKey)) {
        return OS_ERRNO_SHELL_CMDREG_PARA_ERROR;  // 返回参数错误
    }

    if (paraNum > CMD_MAX_PARAS) {  // 检查参数数量是否超过最大值
        if (paraNum != XARGS) {  // 如果不是可变参数
            return OS_ERRNO_SHELL_CMDREG_PARA_ERROR;  // 返回参数错误
        }
    }

    if (OsCmdKeyCheck(cmdKey) != TRUE) {  // 检查命令关键字合法性
        return OS_ERRNO_SHELL_CMDREG_CMD_ERROR;  // 返回命令关键字错误
    }

    (VOID)LOS_MuxLock(&g_cmdInfo.muxLock, LOS_WAIT_FOREVER);  // 加锁
    // 遍历命令列表检查命令是否已存在
    LOS_DL_LIST_FOR_EACH_ENTRY(cmdItemNode, &(g_cmdInfo.cmdList.list), CmdItemNode, list) {
        if ((cmdType == cmdItemNode->cmd->cmdType) &&  // 命令类型匹配
            ((strlen(cmdKey) == strlen(cmdItemNode->cmd->cmdKey)) &&  // 关键字长度匹配
            (strncmp((CHAR *)(cmdItemNode->cmd->cmdKey), cmdKey, strlen(cmdKey)) == 0))) {  // 关键字内容匹配
            (VOID)LOS_MuxUnlock(&g_cmdInfo.muxLock);  // 解锁
            return OS_ERRNO_SHELL_CMDREG_CMD_EXIST;  // 返回命令已存在错误
        }
    }
    (VOID)LOS_MuxUnlock(&g_cmdInfo.muxLock);  // 解锁

    return OsCmdItemCreate(cmdType, cmdKey, paraNum, cmdProc);  // 创建并插入命令项
}
