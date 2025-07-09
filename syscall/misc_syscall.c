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

#include "errno.h"
#include "sysinfo.h"
#include "sys/reboot.h"
#include "sys/resource.h"
#include "sys/times.h"
#include "sys/utsname.h"
#include "capability_type.h"
#include "capability_api.h"
#include "los_process_pri.h"
#include "los_strncpy_from_user.h"
#ifdef LOSCFG_SHELL
#include "shcmd.h"
#include "shmsg.h"
#endif
#include "user_copy.h"
#include "unistd.h"
#ifdef LOSCFG_UTS_CONTAINER
#define HOST_NAME_MAX_LEN 65  // 主机名最大长度（含终止符）

/**
 * @brief 设置系统主机名
 * @param name 用户空间传入的主机名字符串
 * @param len 主机名长度
 * @return 0表示成功，负数表示错误码
 * @note 仅在LOSCFG_UTS_CONTAINER配置开启时可用
 */
int SysSetHostName(const char *name, size_t len)
{
    int ret;                       // 函数返回值
    char tmpName[HOST_NAME_MAX_LEN]; // 临时缓冲区，存储从用户空间复制的主机名
    unsigned int intSave;          // 中断状态保存变量

    if (name == NULL) {            // 检查输入指针是否为空
        return -EFAULT;            // 返回内存访问错误
    }

    if ((len < 0) || (len > HOST_NAME_MAX_LEN)) { // 检查长度是否合法
        return -EINVAL;            // 返回参数无效错误
    }

    // 从用户空间复制主机名到内核缓冲区
    ret = LOS_ArchCopyFromUser(&tmpName, name, len);
    if (ret != 0) {
        return -EFAULT;            // 复制失败返回错误
    }

    SCHEDULER_LOCK(intSave);       // 关闭调度器，确保操作原子性
    struct utsname *currentUtsName = OsGetCurrUtsName(); // 获取当前UTS命名空间
    if (currentUtsName == NULL) {
        SCHEDULER_UNLOCK(intSave); // 解锁调度器
        return -EFAULT;
    }

    // 清空原有主机名字段
    (VOID)memset_s(currentUtsName->nodename, sizeof(currentUtsName->nodename), 0, sizeof(currentUtsName->nodename));
    // 复制新主机名到UTS结构
    ret = memcpy_s(currentUtsName->nodename, sizeof(currentUtsName->nodename), tmpName, len);
    if (ret != EOK) {
        SCHEDULER_UNLOCK(intSave); // 复制失败解锁并返回错误
        return -EFAULT;
    }
    SCHEDULER_UNLOCK(intSave);     // 恢复调度器
    return 0;
}
#endif

/**
 * @brief 获取系统UTS信息（内核名称、版本等）
 * @param name 用户空间结构体指针，用于存储UTS信息
 * @return 0表示成功，负数表示错误码
 */
int SysUname(struct utsname *name)
{
    int ret;                       // 函数返回值
    struct utsname tmpName;        // 内核临时UTS结构体
    // 从用户空间复制结构体（仅验证地址有效性）
    ret = LOS_ArchCopyFromUser(&tmpName, name, sizeof(struct utsname));
    if (ret != 0) {
        return -EFAULT;
    }
    ret = uname(&tmpName);         // 获取系统UTS信息
    if (ret < 0) {
        return ret;
    }
    // 将结果复制回用户空间
    ret = LOS_ArchCopyToUser(name, &tmpName, sizeof(struct utsname));
    if (ret != 0) {
        return -EFAULT;
    }
    return ret;
}

/**
 * @brief 获取系统内存信息
 * @param info 用户空间结构体指针，用于存储内存信息
 * @return 0表示成功，负数表示错误码
 */
int SysInfo(struct sysinfo *info)
{
    int ret;                       // 函数返回值
    struct sysinfo tmpInfo = { 0 }; // 内核临时sysinfo结构体

    // 填充内存信息（LiteOS简化实现）
    tmpInfo.totalram = LOS_MemPoolSizeGet(m_aucSysMem1); // 总内存大小
    tmpInfo.freeram = LOS_MemPoolSizeGet(m_aucSysMem1) - LOS_MemTotalUsedGet(m_aucSysMem1); // 空闲内存
    tmpInfo.sharedram = 0;         // 共享内存（未实现）
    tmpInfo.bufferram = 0;         // 缓冲区内存（未实现）
    tmpInfo.totalswap = 0;         // 交换区总量（未实现）
    tmpInfo.freeswap = 0;          // 空闲交换区（未实现）

    // 将结果复制回用户空间
    ret = LOS_ArchCopyToUser(info, &tmpInfo, sizeof(struct sysinfo));
    if (ret != 0) {
        return -EFAULT;
    }
    return 0;
}

/**
 * @brief 系统重启接口
 * @param magic 魔术数（未使用）
 * @param magic2 魔术数（未使用）
 * @param type 重启类型
 * @return 0表示成功，-EPERM无权限，-EFAULT不支持的类型
 */
int SysReboot(int magic, int magic2, int type)
{
    (void)magic;                   // 未使用参数
    (void)magic2;                  // 未使用参数
    if (!IsCapPermit(CAP_REBOOT)) { // 检查重启权限
        return -EPERM;
    }
    SystemRebootFunc rebootHook = OsGetRebootHook(); // 获取重启钩子函数
    if ((type == RB_AUTOBOOT) && (rebootHook != NULL)) {
        rebootHook();              // 执行重启操作
        return 0;
    }
    return -EFAULT;                // 不支持的重启类型
}

#ifdef LOSCFG_SHELL
/**
 * @brief 执行shell命令接口
 * @param msgName 命令名称
 * @param cmdString 命令参数
 * @return 0表示成功，负数表示错误码
 * @note 仅在LOSCFG_SHELL配置开启时可用
 */
int SysShellExec(const char *msgName, const char *cmdString)
{
    int ret;                       // 函数返回值
    unsigned int uintRet;          // 无符号返回值
    errno_t err;                   // 错误码
    CmdParsed cmdParsed;           // 命令解析结构体
    char msgNameDup[CMD_KEY_LEN + 1] = { 0 }; // 命令名称缓冲区
    char cmdStringDup[CMD_MAX_LEN + 1] = { 0 }; // 命令参数缓冲区

    if (!IsCapPermit(CAP_SHELL_EXEC)) { // 检查shell执行权限
        return -EPERM;
    }

    // 从用户空间复制命令名称
    ret = LOS_StrncpyFromUser(msgNameDup, msgName, CMD_KEY_LEN + 1);
    if (ret < 0) {
        return -EFAULT;
    } else if (ret > CMD_KEY_LEN) {
        return -ENAMETOOLONG;      // 名称过长
    }

    // 从用户空间复制命令参数
    ret = LOS_StrncpyFromUser(cmdStringDup, cmdString, CMD_MAX_LEN + 1);
    if (ret < 0) {
        return -EFAULT;
    } else if (ret > CMD_MAX_LEN) {
        return -ENAMETOOLONG;      // 参数过长
    }

    // 初始化命令解析结构体
    err = memset_s(&cmdParsed, sizeof(CmdParsed), 0, sizeof(CmdParsed));
    if (err != EOK) {
        return -EFAULT;
    }

    // 获取命令类型
    uintRet = ShellMsgTypeGet(&cmdParsed, msgNameDup);
    if (uintRet != LOS_OK) {
        PRINTK("%s:command not found\n", msgNameDup); // 命令未找到
        return -EFAULT;
    } else {
        (void)OsCmdExec(&cmdParsed, (char *)cmdStringDup); // 执行命令
    }

    return 0;
}
#endif

#define USEC_PER_SEC 1000000       // 每秒微秒数

/**
 * @brief 将时钟滴答数转换为timeval结构
 * @param time 输出的timeval结构体指针
 * @param clk 输入的时钟滴答数
 * @return 无
 */
static void ConvertClocks(struct timeval *time, clock_t clk)
{
    // 计算微秒部分（滴答数%每秒滴答数 -> 微秒）
    time->tv_usec = (clk % CLOCKS_PER_SEC) * USEC_PER_SEC / CLOCKS_PER_SEC;
    time->tv_sec = (clk) / CLOCKS_PER_SEC; // 计算秒部分
}

/**
 * @brief 获取进程资源使用情况
 * @param what 资源类型（RUSAGE_SELF/RUSAGE_CHILDREN）
 * @param ru 用户空间结构体指针，用于存储资源信息
 * @return 0表示成功，负数表示错误码
 */
int SysGetrusage(int what, struct rusage *ru)
{
    int ret;                       // 函数返回值
    struct tms time;               // 进程时间结构体
    clock_t usec, sec;             // 用户态和内核态时间
    struct rusage kru;             // 内核临时rusage结构体

    // 从用户空间复制结构体（仅验证地址有效性）
    ret = LOS_ArchCopyFromUser(&kru, ru, sizeof(struct rusage));
    if (ret != 0) {
        return -EFAULT;
    }

    if (times(&time) == -1) {      // 获取进程时间
        return -EFAULT;
    }

    switch (what) {
        case RUSAGE_SELF: {        // 当前进程资源
            usec = time.tms_utime; // 用户态时间
            sec = time.tms_stime;  // 内核态时间
            break;
        }
        case RUSAGE_CHILDREN: {    // 子进程资源
            usec = time.tms_cutime;// 子进程用户态时间
            sec = time.tms_cstime; // 子进程内核态时间
            break;
        }
        default:
            return -EINVAL;        // 无效参数
    }
    ConvertClocks(&kru.ru_utime, usec); // 转换用户时间
    ConvertClocks(&kru.ru_stime, sec);  // 转换系统时间

    // 将结果复制回用户空间
    ret = LOS_ArchCopyToUser(ru, &kru, sizeof(struct rusage));
    if (ret != 0) {
        return -EFAULT;
    }
    return 0;
}

/**
 * @brief 获取系统配置参数
 * @param name 配置参数名称
 * @return 参数值，-1表示错误
 */
long SysSysconf(int name)
{
    long ret;                      // 函数返回值

    ret = sysconf(name);           // 获取系统配置
    if (ret == -1) {
        return -get_errno();       // 返回错误码
    }

    return ret;
}

/**
 * @brief 获取资源限制
 * @param resource 资源类型
 * @param k_rlim 用户空间数组，用于存储资源限制（软限制/硬限制）
 * @return 0表示成功，负数表示错误码
 */
int SysUgetrlimit(int resource, unsigned long long k_rlim[2])
{
    int ret;                       // 函数返回值
    struct rlimit lim = { 0 };     // 资源限制结构体

    ret = getrlimit(resource, &lim); // 获取资源限制
    if (ret < 0) {
        return ret;
    }

    // 将结果复制回用户空间
    ret = LOS_ArchCopyToUser(k_rlim, &lim, sizeof(struct rlimit));
    if (ret != 0) {
        return -EFAULT;
    }

    return ret;
}

/**
 * @brief 设置资源限制
 * @param resource 资源类型
 * @param k_rlim 用户空间数组，存储新的资源限制（软限制/硬限制）
 * @return 0表示成功，负数表示错误码
 */
int SysSetrlimit(int resource, unsigned long long k_rlim[2])
{
    int ret;                       // 函数返回值
    struct rlimit lim;             // 资源限制结构体

    if(!IsCapPermit(CAP_CAPSET)) { // 检查权限
        return -EPERM;
    }

    // 从用户空间复制新的资源限制
    ret = LOS_ArchCopyFromUser(&lim, k_rlim, sizeof(struct rlimit));
    if (ret != 0) {
        return -EFAULT;
    }
    ret = setrlimit(resource, &lim); // 设置资源限制

    return ret;
}