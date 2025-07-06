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
#include "shell.h"
#include "stdlib.h"
#include "time.h"
#include "los_typedef.h"
#include "sys/stat.h"
#include "securec.h"

#if defined(__LP64__)
#define  timeval64      timeval
#define  settimeofday64 settimeofday
#define  gettimeofday64 gettimeofday
#endif

#define  localtime64    localtime
#define  ctime64        ctime
#define  mktime64       mktime

#define  DATE_ERR_INFO      1
#define  DATE_HELP_INFO     0
#define  DATE_ERR           (-1)
#define  DATE_OK            0
#define  DATE_BASE_YEAR     1900
#define  LEAPYEAR(y) (((y) % 4) == 0 && (((y) % 100) != 0 || ((y) % 400) == 0))

// 月份天数表：[0]非闰年 [1]闰年，每行包含12个月的天数
STATIC const INT32 g_monLengths[2][12] = { /* 2: 2行(平年/闰年); 12: 12个月 */
    {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
    {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}
};

/**
 * @brief  复制tm结构体数据
 * @param  destTm [OUT] 目标tm结构体指针
 * @param  srcTm [IN] 源tm结构体指针，为NULL时目标结构体将被初始化为0
 * @return VOID
 */
STATIC VOID OsCopyTm(struct tm *destTm, const struct tm *srcTm)
{
    if (srcTm == NULL) {
        (VOID)memset_s(destTm, sizeof(struct tm), 0, sizeof(struct tm));  // 源为NULL时初始化目标结构体
    } else {
        // 逐个字段复制tm结构体成员
        destTm->tm_sec = srcTm->tm_sec;
        destTm->tm_min = srcTm->tm_min;
        destTm->tm_hour = srcTm->tm_hour;
        destTm->tm_mday = srcTm->tm_mday;
        destTm->tm_mon = srcTm->tm_mon;
        destTm->tm_year = srcTm->tm_year;
        destTm->tm_wday = srcTm->tm_wday;
        destTm->tm_yday = srcTm->tm_yday;
        destTm->tm_isdst = srcTm->tm_isdst;
        destTm->tm_gmtoff = srcTm->tm_gmtoff;
        destTm->tm_zone = srcTm->tm_zone;
    }
}

/**
 * @brief  显示date命令用法帮助或错误信息
 * @param  order [IN] 0表示显示完整帮助信息，非0表示显示错误提示
 * @return VOID
 */
STATIC VOID OsCmdUsageDate(INT32 order)
{
    if (order) {
        PRINTK("date: invalid option or parameter.\n");  // 无效选项或参数错误
        PRINTK("Try 'date --help' for more information.\n");
        return;
    }
    // 打印命令基本用法
    PRINTK("\nUsage: date [+FORMAT]\n");
    PRINTK("   or: date [-s] [YY/MM/DD] [hh:mm:ss]\n");
    PRINTK("Display the current time in the given FORMAT, or set the system date.\n");
    PRINTK("FORMAT controls the output.  Interpreted sequences are:\n");
    // 以下为格式说明，省略具体格式字符串注释（保持原有英文说明以便用户参考标准格式）
    PRINTK("  %%b     The abbreviated month name according to the current locale.\n");
    PRINTK("  %%B     The full month name according to the current locale.\n");
    PRINTK("  %%C     The century number (year/100) as a 2-digit integer. (SU)\n");
    PRINTK("  %%d     The day of the month as a decimal number (range 01 to 31).\n");
    PRINTK("  %%e     Like %%d, the day of the month as a decimal number, \n");
    PRINTK("         but a leading zero is replaced by a space.\n");
    PRINTK("  %%h     Equivalent to %%b.  (SU)\n");
    PRINTK("  %%H     The hour as a decimal number using a 24-hour clock (range 00 to 23).\n");
    PRINTK("  %%I     The  hour as a decimal number using a 12-hour clock (range 01 to 12).\n");
    PRINTK("  %%j     The day of the year as a decimal number (range 001 to 366).\n");
    PRINTK("  %%k     The hour (24-hour clock) as a decimal number (range  0  to  23); \n");
    PRINTK("         single digits are preceded by a blank.  (See also %H.)  (TZ)\n");
    PRINTK("  %%l     The  hour  (12-hour  clock) as a decimal number (range 1 to 12); \n");
    PRINTK("         single digits are preceded by a blank.  (See also %I.)  (TZ)\n");
    PRINTK("  %%m     The month as a decimal number (range 01 to 12).\n");
    PRINTK("  %%M     The minute as a decimal number (range 00 to 59).\n");
    PRINTK("  %%n     A newline character. (SU)\n");
    PRINTK("  %%p     Either "AM" or "PM" according to the given time  value, \n");
    PRINTK("         or the corresponding  strings  for the current locale.\n");
    PRINTK("         Noon is treated as "PM" and midnight as "AM".\n");
    PRINTK("  %%P     Like %%p but in lowercase: "am" or "pm" \n");
    PRINTK("         or a corresponding string for the current locale. (GNU)\n");
    PRINTK("  %%s     The number of seconds since the Epoch, that is,\n");
    PRINTK("         since 1970-01-01 00:00:00 UTC. (TZ)\n");
    PRINTK("  %%S     The second as a decimal number (range 00 to 60).\n");
    PRINTK("         (The range is up to 60 to allow for occasional leap seconds.)\n");
    PRINTK("  %%t     A tab character. (SU)\n");
    PRINTK("  %%y     The year as a decimal number without a century (range 00 to 99).\n");
    PRINTK("  %%Y     The year as a decimal number including the century.\n");
    PRINTK("  %%%%     A literal '%%' character.\n");
    // 打印使用示例
    PRINTK("\nExamples:\n");
    PRINTK("Set system date (2017-01-01)\n");
    PRINTK("$ date -s 20170101\n");
    PRINTK("Set system time (12:00:00)\n");
    PRINTK("$ date -s 12:00:00\n");
    PRINTK("Show the time with format Year-Month-Day\n");
    PRINTK("$ date +%%Y-%%m-%%d\n");
}

/**
 * @brief  将字符串转换为tm结构体时间
 * @param  str [IN] 时间字符串
 * @param  tm [OUT] 转换后的tm结构体指针
 * @return INT32 成功返回DATE_OK，失败返回DATE_ERR
 */
STATIC INT32 OsStrToTm(const CHAR *str, struct tm *tm)
{
    CHAR *ret = NULL;                                 // strptime返回值
    UINT32 strLen = strlen(str);                      // 时间字符串长度
    // 根据字符串长度和格式特征判断时间格式
    if (strLen == 8) { /* 8:时间格式字符串长度，如hh:mm:ss或yyyymmdd */
        if (str[2] == ':') { /* 2:特征字符索引 */
            ret = strptime(str, "%%H:%%M:%%S", tm);  // 解析时分秒格式(hh:mm:ss)
        } else {
            ret = strptime(str, "%%Y%%m%%d", tm);   // 解析年月日格式(yyyymmdd)
        }
    } else if (strLen == 10) { /* 10:时间格式字符串长度，如yyyy/mm/dd */
        ret = strptime(str, "%%Y/%%m/%%d", tm);       // 解析带斜杠的年月日格式
    } else if (strLen == 5) { /* 5:时间格式字符串长度，如hh:mm或mm/dd */
        if (str[2] == ':') { /* 2:特征字符索引 */
            ret = strptime(str, "%%H:%%M", tm);      // 解析时分格式(hh:mm)
        } else if (str[2] == '/') { /* 2:特征字符索引 */
            ret = strptime(str, "%%m/%%d", tm);      // 解析月日格式(mm/dd)
        }
    } else if (strLen == 7) { /* 7:时间格式字符串长度，如yyyy/mm */
        if (str[4] == '/') { /* 4:特征字符索引 */
            ret = strptime(str, "%%Y/%%m", tm);      // 解析年月格式(yyyy/mm)
        }
    }

    // 校验年份有效性（必须大于等于1970年）
    if (tm->tm_year < 70) { /* 70:年份起始值，tm_year=70表示1970年 */
        PRINTK("\nUsage: date -s set system time starting from 1970.\n");
        return DATE_ERR;
    }

    // 校验日期有效性（天数不能超过当月最大天数）
    if (tm->tm_mday > g_monLengths[(INT32)LEAPYEAR(tm->tm_year + DATE_BASE_YEAR)][tm->tm_mon]) {
        return DATE_ERR;
    }

    // 校验秒数有效性（0-59，设置时间时不允许闰秒）
    if ((tm->tm_sec < 0) || (tm->tm_sec > 59)) { /* 秒数范围(0-59) */
        return DATE_ERR;
    }
    return (ret == NULL) ? DATE_ERR : DATE_OK;       // 返回转换结果
}

/**
 * @brief  按指定格式打印当前时间
 * @param  formatStr [IN] 格式字符串，以'+'开头
 * @return INT32 成功返回DATE_OK，失败返回DATE_ERR
 */
STATIC INT32 OsFormatPrintTime(const CHAR *formatStr)
{
    CHAR timebuf[SHOW_MAX_LEN] = {0};                 // 格式化时间缓冲区
    struct tm *tm = NULL;                             // 时间结构体指针
    struct timeval64 nowTime = {0};                   // 当前时间

    if (strlen(formatStr) < 2) { /* 2:检查格式字符串最小长度(至少为+和一个格式字符) */
        OsCmdUsageDate(DATE_ERR_INFO);                // 格式字符串太短，显示错误
        return DATE_ERR;
    }

    if (gettimeofday64(&nowTime, NULL)) {             // 获取当前时间
        return DATE_ERR;
    }
    tm = localtime64(&nowTime.tv_sec);                // 转换为本地时间
    if (tm == NULL) {
        return DATE_ERR;
    }

    // 按格式字符串格式化时间并打印（跳过开头的'+'字符）
    if (strftime(timebuf, SHOW_MAX_LEN - 1, formatStr + 1, tm)) {
        PRINTK("%s\n", timebuf);
    } else {
        OsCmdUsageDate(DATE_ERR_INFO);                // 格式化失败，显示错误
        return DATE_ERR;
    }
    return DATE_OK;
}

/**
 * @brief  设置系统时间
 * @param  timeStr [IN] 时间字符串
 * @return INT32 成功返回DATE_OK，失败返回DATE_ERR
 */
STATIC INT32 OsDateSetTime(const CHAR *timeStr)
{
    struct tm tm = {0};                               // 时间结构体
    struct timeval64 nowTime = {0};                   // 当前时间
    struct timeval64 setTime = {0};                   // 要设置的时间

    if (gettimeofday64(&nowTime, NULL)) {             // 获取当前时间失败
        PRINTK("Setting time failed...\n");
        return DATE_ERR;
    }

    setTime.tv_usec = nowTime.tv_usec;                // 保留微秒部分
    OsCopyTm(&tm, localtime64(&nowTime.tv_sec));      // 复制当前时间到tm结构体

    if (OsStrToTm(timeStr, &tm)) {                    // 解析时间字符串
        OsCmdUsageDate(DATE_ERR_INFO);
        return DATE_ERR;
    }

    setTime.tv_sec = mktime64(&tm);                   // 将tm结构体转换为秒数

    if (settimeofday64(&setTime, NULL)) {             // 设置系统时间
        PRINTK("setting time failed...\n");
        return DATE_ERR;
    }

    return DATE_OK;
}

#ifdef  LOSCFG_FS_VFS
/**
 * @brief  查看文件修改时间
 * @param  filename [IN] 文件名
 * @return INT32 成功返回DATE_OK，失败返回DATE_ERR
 */
STATIC INT32 OsViewFileTime(const CHAR *filename)
{
#define BUFFER_SIZE 26 /* 缓冲区大小，与asctime_r接口使用的大小一致 */
    struct stat statBuf = {0};                        // 文件状态结构体
    CHAR *fullpath = NULL;                            // 规范化后的文件路径
    INT32 ret;                                        // 返回值
    CHAR buf[BUFFER_SIZE];                            // 时间字符串缓冲区
    CHAR *shellWorkingDirectory = OsShellGetWorkingDirectory();  // 获取shell工作目录

    // 规范化文件路径（工作目录+文件名）
    ret = vfs_normalize_path(shellWorkingDirectory, filename, &fullpath);
    if (ret < 0) {
        set_errno(-ret);
        perror("date error");
        return DATE_ERR;
    }

    if (stat(fullpath, &statBuf) != 0) {              // 获取文件状态
        OsCmdUsageDate(DATE_ERR_INFO);
        free(fullpath);                               // 释放路径内存
        return DATE_ERR;
    }
    PRINTK("%s\n", ctime_r(&(statBuf.st_mtim.tv_sec), buf));  // 打印修改时间
    free(fullpath);                                   // 释放路径内存
    return DATE_OK;
}
#endif

/**
 * @brief  shell日期命令主函数，处理日期显示和设置
 * @param  argc [IN] 参数个数
 * @param  argv [IN] 参数列表
 * @return INT32 成功返回DATE_OK，失败返回DATE_ERR
 */
INT32 OsShellCmdDate(INT32 argc, const CHAR **argv)
{
    struct timeval64 nowTime = {0};                   // 当前时间

    if (argc == 1) { /* 1:无参数，显示当前时间 */
        if (gettimeofday64(&nowTime, NULL)) {         // 获取当前时间
            return DATE_ERR;
        }
        PRINTK("%s\n", ctime64(&nowTime.tv_sec));    // 打印当前时间
        return DATE_OK;
    }

    if (argc == 2) { /* 2:一个参数，处理帮助或格式化显示 */
        if (argv == NULL) {
            OsCmdUsageDate(DATE_HELP_INFO);
            return DATE_ERR;
        }

        if (!(strcmp(argv[1], "--help"))) {          // 显示帮助信息
            OsCmdUsageDate(DATE_HELP_INFO);
            return DATE_OK;
        }
        if (!(strncmp(argv[1], "+", 1))) {           // 格式化显示时间
            return OsFormatPrintTime(argv[1]);
        }
    }

    if (argc > 2) { /* 2:多个参数，处理时间设置或文件时间查看 */
        if (argv == NULL) {
            OsCmdUsageDate(DATE_HELP_INFO);
            return DATE_ERR;
        }

        if (!(strcmp(argv[1], "-s"))) {              // 设置系统时间
            return OsDateSetTime(argv[2]); /* 2:参数索引，时间字符串 */
        } else if (!(strcmp(argv[1], "-r"))) {       // 查看文件修改时间
#ifdef  LOSCFG_FS_VFS
            return OsViewFileTime(argv[2]); /* 2:参数索引，文件名 */
#endif
        }
    }

    OsCmdUsageDate(DATE_ERR_INFO);                    // 参数无效，显示错误信息
    return DATE_OK;
}

SHELLCMD_ENTRY(date_shellcmd, CMD_TYPE_STD, "date", XARGS, (CmdCallBackFunc)OsShellCmdDate);
