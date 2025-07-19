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
#include "show.h"
#include "stdlib.h"
#include "unistd.h"
#include "dirent.h"
#include "securec.h"
// Shell初始化魔术标志，用于验证初始化有效性（0xABABABAB）
#define SHELL_INIT_MAGIC_FLAG 0xABABABAB
// Ctrl+C的ASCII码值（十进制3），用于中断信号处理
#define CTRL_C 0x03 /* 0x03: ctrl+c ASCII */

/**
 * @brief 释放命令解析结构中的参数内存
 * @param cmdParsed 命令解析结构指针，包含待释放的参数数组
 * @details 遍历参数数组，释放每个非空参数的内存并置空指针，防止内存泄漏
 */
static void OsFreeCmdPara(CmdParsed *cmdParsed)
{
    unsigned int i;  // 循环索引
    // 遍历所有参数，释放非空指针内存
    for (i = 0; i < cmdParsed->paramCnt; i++) {
        if ((cmdParsed->paramArray[i]) != NULL) {
            free((cmdParsed->paramArray[i]));  // 释放单个参数内存
            cmdParsed->paramArray[i] = NULL;   // 指针置空，避免野指针
        }
    }
}

/**
 * @brief 处理Tab补全时的字符串分离与解析
 * @param tabStr [in/out] 输入的Tab补全字符串，输出解析后的目标字符串
 * @param parsed [out] 命令解析结构，用于存储解析结果
 * @param tabStrLen [in] 输入字符串长度
 * @return SH_OK-成功，SH_ERROR-失败
 * @details 对输入字符串进行预处理（去空格、分离参数），提取需要补全的目标子串
 */
static int OsStrSeparateTabStrGet(const char **tabStr, CmdParsed *parsed, unsigned int tabStrLen)
{
    char *shiftStr = NULL;                  // 空格处理后的字符串
    // 分配临时缓冲区（SHOW_MAX_LEN << 1表示缓冲区大小翻倍，SHOW_MAX_LEN为最大显示长度）
    char *tempStr = (char *)malloc(SHOW_MAX_LEN << 1);
    if (tempStr == NULL) {
        return (int)SH_ERROR;               // 内存分配失败，返回错误
    }

    // 初始化临时缓冲区（全部置0）
    (void)memset_s(tempStr, SHOW_MAX_LEN << 1, 0, SHOW_MAX_LEN << 1);
    shiftStr = tempStr + SHOW_MAX_LEN;      // 后半部分用于存储处理后的字符串

    // 复制输入字符串到临时缓冲区前半部分（长度限制为SHOW_MAX_LEN-1）
    if (strncpy_s(tempStr, SHOW_MAX_LEN - 1, *tabStr, tabStrLen)) {
        free(tempStr);                      // 复制失败，释放内存
        return (int)SH_ERROR;
    }

    parsed->cmdType = CMD_TYPE_STD;         // 设置命令类型为标准命令

    /* 压缩无用或重复空格 */
    if (OsCmdKeyShift(tempStr, shiftStr, SHOW_MAX_LEN - 1)) {
        free(tempStr);                      // 空格处理失败，释放内存
        return (int)SH_ERROR;
    }

    /* 获取需要补全的字符串精确位置 */
    /* 根据末尾空格是否丢失区分不同情况 */
    // 如果处理后字符串为空，或原字符串末尾字符与处理后不同（表示末尾有空格被移除）
    if ((strlen(shiftStr) == 0) || (tempStr[strlen(tempStr) - 1] != shiftStr[strlen(shiftStr) - 1])) {
        *tabStr = "";                      // 无需要补全的字符串
    } else {
        // 解析处理后的字符串，提取参数
        if (OsCmdParse(shiftStr, parsed)) {
            free(tempStr);                  // 解析失败，释放内存
            return (int)SH_ERROR;
        }
        // 补全目标为最后一个参数
        *tabStr = parsed->paramArray[parsed->paramCnt - 1];
    }

    free(tempStr);                          // 释放临时缓冲区
    return SH_OK;
}

/**
 * @brief 获取Shell当前工作目录
 * @return 当前工作目录字符串指针
 * @details 从Shell控制块中获取当前工作目录，供路径解析使用
 */
char *OsShellGetWorkingDirectory(void)
{
    return OsGetShellCb()->shellWorkingDirectory;  // 返回控制块中的工作目录字段
}

/**
 * @brief 设置Shell当前工作目录
 * @param dir 目标目录路径
 * @param len 路径长度
 * @return SH_OK-成功，SH_NOK-失败
 * @details 将指定目录路径复制到Shell控制块，更新当前工作目录
 */
int OsShellSetWorkingDirectory(const char *dir, size_t len)
{
    if (dir == NULL) {
        return SH_NOK;                      // 目录路径为空，返回失败
    }

    // 复制目录路径到控制块（长度限制为工作目录缓冲区大小）
    int ret = strncpy_s(OsGetShellCb()->shellWorkingDirectory, sizeof(OsGetShellCb()->shellWorkingDirectory),
                        dir, len);
    if (ret != SH_OK) {
        return SH_NOK;                      // 复制失败，返回错误
    }
    return SH_OK;
}

/**
 * @brief 分离路径字符串为目录路径和目标名称
 * @param tabStr 输入的Tab补全字符串
 * @param strPath [out] 输出的目录路径缓冲区
 * @param nameLooking [out] 输出的目标名称缓冲区
 * @param tabStrLen 输入字符串长度
 * @return SH_OK-成功，SH_ERROR-失败
 * @details 结合当前工作目录解析输入路径，分离出基础目录和待查找的目标名称（用于Tab补全）
 */
static int OsStrSeparate(const char *tabStr, char *strPath, char *nameLooking, unsigned int tabStrLen)
{
    char *strEnd = NULL;                    // 路径中最后一个'/'的位置
    char *cutPos = NULL;                    // 路径切割位置
    CmdParsed parsed = {0};                 // 命令解析结构（初始化为全0）
    char *shellWorkingDirectory = OsShellGetWorkingDirectory();  // 获取当前工作目录
    int ret;

    // 预处理Tab字符串，提取需要补全的目标子串
    ret = OsStrSeparateTabStrGet(&tabStr, &parsed, tabStrLen);
    if (ret != SH_OK) {
        return ret;                         // 预处理失败，返回错误
    }

    /* 构建完整路径字符串 */
    // 如果不是绝对路径（不以'/'开头），则拼接当前工作目录
    if (*tabStr != '/') {
        // 复制当前工作目录到路径缓冲区
        if (strncpy_s(strPath, CMD_MAX_PATH, shellWorkingDirectory, CMD_MAX_PATH - 1)) {
            OsFreeCmdPara(&parsed);         // 复制失败，释放解析结构内存
            return (int)SH_ERROR;
        }
        // 如果工作目录不是根目录，添加路径分隔符'/'
        if (strcmp(shellWorkingDirectory, "/")) {
            if (strncat_s(strPath, CMD_MAX_PATH - 1, "/", CMD_MAX_PATH - strlen(strPath) - 1)) {
                OsFreeCmdPara(&parsed);     // 拼接失败，释放内存
                return (int)SH_ERROR;
            }
        }
    }

    // 拼接目标子串到路径缓冲区
    if (strncat_s(strPath, CMD_MAX_PATH - 1, tabStr, CMD_MAX_PATH - strlen(strPath) - 1)) {
        OsFreeCmdPara(&parsed);             // 拼接失败，释放内存
        return (int)SH_ERROR;
    }

    /* 按最后一个'/'分割字符串 */
    strEnd = strrchr(strPath, '/');         // 查找最后一个'/'
    if (strEnd != NULL) {
        // 提取'/'后的部分作为待查找名称
        if (strncpy_s(nameLooking, CMD_MAX_PATH, strEnd + 1, CMD_MAX_PATH - 1)) { /* get cmp str */
            OsFreeCmdPara(&parsed);         // 复制失败，释放内存
            return (int)SH_ERROR;
        }
    }

    // 截断路径，保留目录部分（将最后一个'/'后的字符置为结束符）
    cutPos = strrchr(strPath, '/');
    if (cutPos != NULL) {
        *(cutPos + 1) = '\0';
    }

    OsFreeCmdPara(&parsed);                 // 释放解析结构中的参数内存
    return SH_OK;
}

/**
 * @brief 分页显示时的用户输入控制
 * @return 1-继续显示，0-停止显示，SH_ERROR-错误
 * @details 处理用户在分页显示时的按键输入（q/Q/Ctrl+C停止，回车继续）
 */
static int OsShowPageInputControl(void)
{
    char readChar;                          // 读取的字符

    while (1) {
        // 从标准输入读取一个字符（阻塞操作）
        if (read(STDIN_FILENO, &readChar, 1) != 1) { /* get one char from stdin */
            printf("\n");
            return (int)SH_ERROR;           // 读取失败，返回错误
        }
        // 若输入q/Q/Ctrl+C，停止显示
        if ((readChar == 'q') || (readChar == 'Q') || (readChar == CTRL_C)) {
            printf("\n");
            return 0;
        } else if (readChar == '\r') {      // 回车继续显示
            // 清除--More--提示（退格并覆盖）
            printf("\b \b\b \b\b \b\b \b\b \b\b \b\b \b\b \b");
            return 1;
        }
    }
}

/**
 * @brief 分页显示控制逻辑
 * @param timesPrint 当前已打印次数
 * @param lineCap 每行可显示数量
 * @param count 总数量
 * @return 1-继续显示，0-停止显示，其他-错误
 * @details 根据已打印次数和屏幕容量决定是否分页，满屏时调用输入控制函数
 */
static int OsShowPageControl(unsigned int timesPrint, unsigned int lineCap, unsigned int count)
{
    // 判断是否需要换行（已打印次数是每行容量的倍数）
    if (NEED_NEW_LINE(timesPrint, lineCap)) {
        printf("\n");
        // 若屏幕已满且未显示完所有项，提示--More--并等待用户输入
        if (SCREEN_IS_FULL(timesPrint, lineCap) && (timesPrint < count)) {
            printf("--More--");
            return OsShowPageInputControl();
        }
    }
    return 1;                               // 继续显示
}

/**
 * @brief 大量结果时确认是否全部显示
 * @param count 结果总数
 * @return 1-显示全部，0-取消显示，SH_ERROR-错误
 * @details 当结果数量超过一屏时，询问用户是否显示全部（y/n确认）
 */
static int OsSurePrintAll(unsigned int count)
{
    char readChar = 0;                      // 读取的字符
    printf("\nDisplay all %u possibilities?(y/n)", count);
    while (1) {
        // 读取用户输入
        if (read(STDIN_FILENO, &readChar, 1) != 1) {
            return (int)SH_ERROR;           // 读取失败，返回错误
        }
        // 输入n/N/Ctrl+C，取消显示
        if ((readChar == 'n') || (readChar == 'N') || (readChar == CTRL_C)) {
            printf("\n");
            return 0;
        } else if ((readChar == 'y') || (readChar == 'Y') || (readChar == '\r')) {
            return 1;                       // 输入y/Y/回车，显示全部
        }
    }
}

/**
 * @brief 打印匹配的文件/目录列表
 * @param count 匹配项总数
 * @param strPath 目录路径
 * @param nameLooking 待匹配的名称前缀
 * @param printLen 每项显示长度
 * @return SH_OK-成功，SH_ERROR-失败
 * @details 打开目录，遍历并打印所有匹配前缀的项，支持分页显示和用户交互控制
 */
static int OsPrintMatchList(unsigned int count, const char *strPath, const char *nameLooking, unsigned int printLen)
{
    unsigned int timesPrint = 0;            // 已打印次数
    unsigned int lineCap;                   // 每行可显示项数
    int ret;                                // 返回值
    DIR *openDir = NULL;                    // 目录指针
    struct dirent *readDir = NULL;          // 目录项指针
    char formatChar[10] = {0}; /* 10:for formatChar length */  // 格式化字符串缓冲区

    // 限制打印长度不超过屏幕宽度（预留2字节）
    printLen = (printLen > (DEFAULT_SCREEN_WIDTH - 2)) ? (DEFAULT_SCREEN_WIDTH - 2) : printLen; /* 2:revered 2 bytes */
    // 计算每行可显示项数（每项占用printLen+2字节，2为分隔空格）
    lineCap = DEFAULT_SCREEN_WIDTH / (printLen + 2); /* 2:DEFAULT_SCREEN_WIDTH revered 2 bytes */
    // 构建格式化字符串（左对齐，固定长度）
    if (snprintf_s(formatChar, sizeof(formatChar) - 1, 7, "%%-%us  ", printLen) < 0) { /* 7:format-len */
        return (int)SH_ERROR;               // 格式化失败，返回错误
    }

    // 若结果数量超过一屏（每行容量×屏幕高度），询问用户是否显示全部
    if (count > (lineCap * DEFAULT_SCREEN_HEIGHT)) {
        ret = OsSurePrintAll(count);
        if (ret != 1) {
            return ret;                     // 用户取消，返回
        }
    }
    // 打开目标目录
    openDir = opendir(strPath);
    if (openDir == NULL) {
        return (int)SH_ERROR;               // 打开目录失败
    }

    printf("\n");
    // 遍历目录项
    for (readDir = readdir(openDir); readDir != NULL; readDir = readdir(openDir)) {
        // 跳过不匹配前缀的项
        if (strncmp(nameLooking, readDir->d_name, strlen(nameLooking)) != 0) {
            continue;
        }
        // 按格式化字符串打印目录项名称
        printf(formatChar, readDir->d_name);
        timesPrint++;
        // 分页控制
        ret = OsShowPageControl(timesPrint, lineCap, count);
        if (ret != 1) {
            if (closedir(openDir) < 0) {    // 关闭目录失败
                return (int)SH_ERROR;
            }
            return ret;                     // 返回用户控制结果
        }
    }

    printf("\n");
    // 关闭目录
    if (closedir(openDir) < 0) {
        return (int)SH_ERROR;               // 关闭目录失败
    }

    return SH_OK;
}

/**
 * @brief 比较两个字符串并截断不匹配部分
 * @param s1 源字符串1
 * @param s2 源字符串2（将被截断）
 * @param n 比较长度
 * @details 逐个字符比较s1和s2，在第一个不匹配处截断s2，并将剩余部分置0
 */
static void StrncmpCut(const char *s1, char *s2, size_t n)
{
    if ((n == 0) || (s1 == NULL) || (s2 == NULL)) {
        return;                             // 参数无效，直接返回
    }
    do {
        // 比较字符，若都不为空且相等则继续
        if (*s1 && *s2 && (*s1 == *s2)) {
            s1++;
            s2++;
        } else {
            break;                          // 遇到不匹配字符，跳出循环
        }
    } while (--n != 0);
    // 若还有剩余长度，将剩余部分置0
    if (n > 0) {
        /* NULL pad the remaining n-1 bytes */
        while (n-- != 0) {
            *s2++ = 0;
        }
    }
    return;
}

/**
 * @brief 补全命令行字符串
 * @param result 匹配的完整字符串
 * @param target 待补全的目标前缀
 * @param cmdKey 命令行缓冲区
 * @param len [in/out] 当前命令行长度，输出补全后的长度
 * @details 将result中超出target的部分追加到cmdKey，并更新长度（处理缓冲区溢出）
 */
static void OsCompleteStr(char *result, const char *target, char *cmdKey, unsigned int *len)
{
    // 计算需要补全的字符数（完整字符串长度 - 前缀长度）
    unsigned int size = strlen(result) - strlen(target);
    char *des = cmdKey + *len;              // 目标缓冲区位置
    char *src = result + strlen(target);    // 源字符串起始位置（跳过前缀）

    // 逐个字符追加补全内容
    while (size-- > 0) {
        printf("%c", *src);                 // 输出补全的字符（回显到终端）
        // 检查缓冲区是否已满（预留1字节给结束符）
        if (*len == (SHOW_MAX_LEN - 1)) {
            *des = '\0';                    // 缓冲区满，添加结束符
            break;
        }
        *des++ = *src++;                    // 复制字符到命令行缓冲区
        (*len)++;                           // 更新长度
    }
}

/**
 * @brief   在指定目录中查找与目标名称匹配的文件
 * @param   strPath      要打开的目录路径
 * @param   nameLooking  要匹配的文件名前缀
 * @param   strObj       输出参数，用于存储匹配到的文件名
 * @param   maxLen       输出参数，用于存储最长匹配文件名的长度
 * @return  匹配到的文件数量，失败返回SH_ERROR
 */
static int OsExecNameMatch(const char *strPath, const char *nameLooking, char *strObj, unsigned int *maxLen)
{
    int count = 0;                                 // 匹配到的文件计数
    DIR *openDir = NULL;                           // 目录指针
    struct dirent *readDir = NULL;                 // 目录项指针

    openDir = opendir(strPath);                    // 打开指定目录
    if (openDir == NULL) {                         // 目录打开失败检查
        return (int)SH_ERROR;                      // 返回错误码
    }

    // 遍历目录中的所有文件
    for (readDir = readdir(openDir); readDir != NULL; readDir = readdir(openDir)) {
        // 比较文件名前缀是否匹配
        if (strncmp(nameLooking, readDir->d_name, strlen(nameLooking)) != 0) {
            continue;                              // 不匹配则跳过当前文件
        }
        if (count == 0) {                          // 处理第一个匹配的文件
            // 复制文件名到输出缓冲区
            if (strncpy_s(strObj, CMD_MAX_PATH, readDir->d_name, CMD_MAX_PATH - 1)) {
                (void)closedir(openDir);           // 关闭目录
                return (int)SH_ERROR;              // 复制失败返回错误
            }
            *maxLen = strlen(readDir->d_name);     // 更新最长文件名长度
        } else {
            // 比较并截取匹配文件名的公共部分
            StrncmpCut(readDir->d_name, strObj, strlen(strObj));
            // 更新最长文件名长度
            if (strlen(readDir->d_name) > *maxLen) {
                *maxLen = strlen(readDir->d_name);
            }
        }
        count++;                                   // 增加匹配计数
    }

    if (closedir(openDir) < 0) {                   // 关闭目录并检查错误
        return (int)SH_ERROR;                      // 关闭失败返回错误
    }

    return count;                                  // 返回匹配文件数量
}

/**
 * @brief   处理Tab键补全文件路径
 * @param   cmdKey  命令行输入字符串
 * @param   len     输入字符串长度指针
 * @return  匹配到的文件数量，失败返回SH_ERROR
 */
static int OsTabMatchFile(char *cmdKey, unsigned int *len)
{
    unsigned int maxLen = 0;                       // 最长匹配文件名长度
    int count;                                     // 匹配文件计数
    char *strOutput = NULL;                        // 输出缓冲区指针
    char *strCmp = NULL;                           // 比较字符串指针
    // 分配内存：3个CMD_MAX_PATH大小，分别用于dirOpen、strOutput和strCmp
    char *dirOpen = (char *)malloc(CMD_MAX_PATH * 3); /* 3:dirOpen\strOutput\strCmp */
    if (dirOpen == NULL) {                         // 内存分配失败检查
        return (int)SH_ERROR;                      // 返回错误
    }

    // 初始化内存区域
    (void)memset_s(dirOpen, CMD_MAX_PATH * 3, 0, CMD_MAX_PATH * 3); /* 3:dirOpen\strOutput\strCmp */
    strOutput = dirOpen + CMD_MAX_PATH;            // 设置输出缓冲区地址
    strCmp = strOutput + CMD_MAX_PATH;             // 设置比较字符串地址

    // 分离目录路径和文件名
    if (OsStrSeparate(cmdKey, dirOpen, strCmp, *len)) {
        free(dirOpen);                             // 释放内存
        return (int)SH_ERROR;                      // 分离失败返回错误
    }

    // 在目录中查找匹配的文件
    count = OsExecNameMatch(dirOpen, strCmp, strOutput, &maxLen);
    /* 一个或多个匹配 */
    if (count >= 1) {
        // 补全命令行字符串
        OsCompleteStr(strOutput, strCmp, cmdKey, len);

        if (count == 1) {                          // 只有一个匹配项
            free(dirOpen);                         // 释放内存
            return 1;                              // 返回匹配数量
        }
        // 打印匹配列表
        if (OsPrintMatchList((unsigned int)count, dirOpen, strCmp, maxLen) == -1) {
            free(dirOpen);                         // 释放内存
            return (int)SH_ERROR;                  // 打印失败返回错误
        }
    }

    free(dirOpen);                                 // 释放内存
    return count;                                  // 返回匹配数量
}

/**
 * @brief   处理命令行字符串，清除无用空格
 * @details 包括：
 *          1) 清除未被引号标记区域的多余空格，将多个空格压缩为一个
 *          2) 清除第一个有效字符前的所有空格
 * @param   cmdKey  输入的命令行字符串
 * @param   cmdOut  输出的处理后字符串
 * @param   size    cmdOut缓冲区大小
 * @return  成功返回SH_OK，失败返回SH_ERROR
 */
unsigned int OsCmdKeyShift(const char *cmdKey, char *cmdOut, unsigned int size)
{
    char *output = NULL;                           // 临时输出缓冲区
    char *outputBak = NULL;                        // 输出缓冲区备份
    unsigned int len;                              // 字符串长度
    int ret;                                       // 函数返回值
    bool quotes = FALSE;                           // 引号匹配状态标志

    if ((cmdKey == NULL) || (cmdOut == NULL)) {    // 空指针检查
        return (unsigned int)SH_ERROR;             // 返回错误
    }

    len = strlen(cmdKey);                          // 获取输入字符串长度
    // 检查输入是否为空或长度超过缓冲区大小
    if ((*cmdKey == '\n') || (len >= size)) {
        return (unsigned int)SH_ERROR;             // 返回错误
    }
    output = (char *)malloc(len + 1);              // 分配临时缓冲区
    if (output == NULL) {                          // 内存分配失败检查
        printf("malloc failure in %s[%d]\n", __FUNCTION__, __LINE__);
        return (unsigned int)SH_ERROR;             // 返回错误
    }

    outputBak = output;                            // 备份缓冲区起始地址
    // 扫描命令行字符串，压缩多余空格并忽略无效字符
    for (; *cmdKey != '\0'; cmdKey++) {
        // 检测到双引号，切换匹配状态
        if (*(cmdKey) == '\"') {
            SWITCH_QUOTES_STATUS(quotes);         // 切换引号状态
        }
        // 在以下情况忽略当前字符：
        // 1) 引号匹配状态为FALSE（空格未被双引号标记）
        // 2) 当前字符是空格
        // 3) 下一个字符也是空格或已到达字符串末尾(\0)
        // 4) 无效字符，如单引号
        if ((*cmdKey == ' ') && ((*(cmdKey + 1) == ' ') || (*(cmdKey + 1) == '\0')) && QUOTES_STATUS_CLOSE(quotes)) {
            continue;                              // 跳过当前空格
        }
        if (*cmdKey == '\'') {                    // 忽略单引号
            continue;
        }
        *output = *cmdKey;                         // 复制有效字符到缓冲区
        output++;
    }
    *output = '\0';                               // 添加字符串结束符
    output = outputBak;                            // 恢复缓冲区起始地址
    len = strlen(output);                          // 获取处理后字符串长度
    // 清除缓冲区第一个字符为空格的情况
    if (*output == ' ') {
        output++;
        len--;
    }
    // 复制处理后的缓冲区到输出
    ret = strncpy_s(cmdOut, size, output, len);
    if (ret != SH_OK) {                            // 复制失败检查
        printf("%s,%d strncpy_s failed, err:%d!\n", __FUNCTION__, __LINE__, ret);
        free(outputBak);                           // 释放临时缓冲区
        return SH_ERROR;                           // 返回错误
    }
    cmdOut[len] = '\0';                           // 添加字符串结束符

    free(outputBak);                               // 释放临时缓冲区
    return SH_OK;                                  // 返回成功
}

/**
 * @brief   处理Tab键补全功能
 * @param   cmdKey  命令行输入字符串
 * @param   len     输入字符串长度指针
 * @return  匹配到的文件数量，失败返回SH_ERROR
 */
int OsTabCompletion(char *cmdKey, unsigned int *len)
{
    int count;                                     // 匹配文件计数

    if ((cmdKey == NULL) || (len == NULL)) {       // 空指针检查
        return (int)SH_ERROR;                      // 返回错误
    }

    count = OsTabMatchFile(cmdKey, len);           // 调用文件匹配函数

    return count;                                  // 返回匹配数量
}

/**
 * @brief   初始化Shell按键处理相关数据结构
 * @param   shellCB Shell控制块指针
 * @return  成功返回SH_OK，失败返回SH_ERROR
 */
unsigned int OsShellKeyInit(ShellCB *shellCB)
{
    CmdKeyLink *cmdKeyLink = NULL;                 // 命令键链表指针
    CmdKeyLink *cmdHistoryLink = NULL;             // 命令历史链表指针

    if (shellCB == NULL) {                         // 空指针检查
        return SH_ERROR;                           // 返回错误
    }

    cmdKeyLink = (CmdKeyLink *)malloc(sizeof(CmdKeyLink)); // 分配命令键链表内存
    if (cmdKeyLink == NULL) {                      // 内存分配失败检查
        printf("Shell CmdKeyLink memory alloc error!\n");
        return SH_ERROR;                           // 返回错误
    }
    cmdHistoryLink = (CmdKeyLink *)malloc(sizeof(CmdKeyLink)); // 分配命令历史链表内存
    if (cmdHistoryLink == NULL) {                  // 内存分配失败检查
        free(cmdKeyLink);                          // 释放已分配内存
        printf("Shell CmdHistoryLink memory alloc error!\n");
        return SH_ERROR;                           // 返回错误
    }

    cmdKeyLink->count = 0;                         // 初始化命令键计数
    SH_ListInit(&(cmdKeyLink->list));              // 初始化命令键链表
    shellCB->cmdKeyLink = (void *)cmdKeyLink;      // 保存命令键链表指针

    cmdHistoryLink->count = 0;                     // 初始化命令历史计数
    SH_ListInit(&(cmdHistoryLink->list));          // 初始化命令历史链表
    shellCB->cmdHistoryKeyLink = (void *)cmdHistoryLink; // 保存命令历史链表指针
    shellCB->cmdMaskKeyLink = (void *)cmdHistoryLink;   // 设置命令掩码链表指针
    return SH_OK;                                  // 返回成功
}

/**
 * @brief   释放命令键链表资源
 * @param   cmdKeyLink  命令键链表指针
 */
void OsShellKeyDeInit(CmdKeyLink *cmdKeyLink)
{
    CmdKeyLink *cmdtmp = NULL;                     // 临时命令键链表节点
    if (cmdKeyLink == NULL) {                      // 空指针检查
        return;
    }

    // 遍历并释放链表中的所有节点
    while (!SH_ListEmpty(&(cmdKeyLink->list))) {
        cmdtmp = SH_LIST_ENTRY(cmdKeyLink->list.pstNext, CmdKeyLink, list);
        SH_ListDelete(&cmdtmp->list);              // 从链表中删除节点
        free(cmdtmp);                              // 释放节点内存
    }

    cmdKeyLink->count = 0;                         // 重置计数
    free(cmdKeyLink);                              // 释放链表头内存
}

/**
 * @brief   将命令字符串添加到命令链表中
 * @param   string      要添加的命令字符串
 * @param   cmdKeyLink  命令链表指针
 */
void OsShellCmdPush(const char *string, CmdKeyLink *cmdKeyLink)
{
    CmdKeyLink *cmdNewNode = NULL;                 // 新命令节点指针
    unsigned int len;                              // 命令字符串长度

    // 检查输入字符串是否为空
    if ((string == NULL) || (strlen(string) == 0)) {
        return;
    }

    len = strlen(string);                          // 获取字符串长度
    // 分配新节点内存：节点结构大小 + 字符串长度 + 1(结束符)
    cmdNewNode = (CmdKeyLink *)malloc(sizeof(CmdKeyLink) + len + 1);
    if (cmdNewNode == NULL) {                      // 内存分配失败检查
        return;
    }

    // 初始化新节点内存
    (void)memset_s(cmdNewNode, sizeof(CmdKeyLink) + len + 1, 0, sizeof(CmdKeyLink) + len + 1);
    // 复制命令字符串到节点
    if (strncpy_s(cmdNewNode->cmdString, len + 1, string, len)) {
        free(cmdNewNode);                          // 复制失败，释放节点内存
        return;
    }

    // 将新节点插入链表尾部
    SH_ListTailInsert(&(cmdKeyLink->list), &(cmdNewNode->list));

    return;
}

/**
 * @brief   显示历史命令
 * @param   value       操作类型：CMD_KEY_UP(上)或CMD_KEY_DOWN(下)
 * @param   shellCB     Shell控制块指针
 */
void OsShellHistoryShow(unsigned int value, ShellCB *shellCB)
{
    CmdKeyLink *cmdtmp = NULL;                     // 临时命令节点指针
    CmdKeyLink *cmdNode = shellCB->cmdHistoryKeyLink; // 命令历史链表头
    CmdKeyLink *cmdMask = shellCB->cmdMaskKeyLink; // 当前命令掩码节点
    int ret;                                       // 函数返回值

    (void)pthread_mutex_lock(&shellCB->historyMutex); // 加锁保护历史命令
    if (value == CMD_KEY_DOWN) {                   // 向下浏览历史
        if (cmdMask == cmdNode) {                  // 已到达最新命令
            goto END;                              // 跳转到结束处理
        }

        // 获取下一个历史命令节点
        cmdtmp = SH_LIST_ENTRY(cmdMask->list.pstNext, CmdKeyLink, list);
        if (cmdtmp != cmdNode) {                   // 未到达链表头
            cmdMask = cmdtmp;                      // 更新当前节点
        } else {
            goto END;                              // 跳转到结束处理
        }
    } else if (value == CMD_KEY_UP) {              // 向上浏览历史
        // 获取上一个历史命令节点
        cmdtmp = SH_LIST_ENTRY(cmdMask->list.pstPrev, CmdKeyLink, list);
        if (cmdtmp != cmdNode) {                   // 未到达链表头
            cmdMask = cmdtmp;                      // 更新当前节点
        } else {
            goto END;                              // 跳转到结束处理
        }
    }

    // 清除当前命令行显示
    while (shellCB->shellBufOffset--) {
        printf("\b \b");                         // 退格并清除字符
    }
    printf("%s", cmdMask->cmdString);             // 打印历史命令
    shellCB->shellBufOffset = strlen(cmdMask->cmdString); // 更新缓冲区偏移
    // 清空命令缓冲区
    (void)memset_s(shellCB->shellBuf, SHOW_MAX_LEN, 0, SHOW_MAX_LEN);
    // 复制历史命令到缓冲区
    ret = memcpy_s(shellCB->shellBuf, SHOW_MAX_LEN, cmdMask->cmdString, shellCB->shellBufOffset);
    if (ret != SH_OK) {                            // 复制失败检查
        printf("%s, %d memcpy failed!\n", __FUNCTION__, __LINE__);
        goto END;                                  // 跳转到结束处理
    }
    shellCB->cmdMaskKeyLink = (void *)cmdMask;     // 更新当前命令掩码节点

END:
    (void)pthread_mutex_unlock(&shellCB->historyMutex); // 解锁
    return;
}

/**
 * @brief   执行命令（占位函数）
 * @param   cmdParsed   解析后的命令结构
 * @param   cmdStr      命令字符串
 * @return  成功返回SH_OK，失败返回SH_NOK
 */
unsigned int OsCmdExec(CmdParsed *cmdParsed, char *cmdStr)
{
    unsigned int ret = SH_OK;                      // 返回值，默认为成功
    if (cmdParsed && cmdStr) {                     // 如果参数有效（占位逻辑）
        ret = SH_NOK;                              // 设置返回失败
    }

    return ret;                                    // 返回结果
}