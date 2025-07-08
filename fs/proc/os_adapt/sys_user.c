/*
 * Copyright (c) 2023-2023 Huawei Device Co., Ltd. All rights reserved.
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

#include <sys/statfs.h>
#include <sys/mount.h>
#include "proc_fs.h"
#include "internal.h"
#include "los_process_pri.h"
#include "user_copy.h"
#include "los_memory.h"
#ifdef LOSCFG_KERNEL_CONTAINER  // 条件编译：启用内核容器功能时生效

/**
 * @brief 系统用户proc文件配置结构体
 * @details 定义/proc/sys/user目录下文件的属性和操作函数
 */
struct ProcSysUser {
    char         *name;          // 文件名
    mode_t       mode;           // 文件权限模式
    int          type;           // 容器类型
    const struct ProcFileOperations *fileOps;  // 文件操作函数集
};

/**
 * @brief 将用户空间数据复制到内核缓冲区
 * @param[in]  src   用户空间数据源地址
 * @param[in]  len   要复制的数据长度
 * @param[out] kbuf  输出的内核缓冲区指针
 * @return 成功返回0，内存分配失败返回ENOMEM，复制失败返回EFAULT
 */
static unsigned int MemUserCopy(const char *src, size_t len, char **kbuf)
{
    // 检查是否为用户空间地址范围
    if (LOS_IsUserAddressRange((VADDR_T)(UINTPTR)src, len)) {
        // 分配内核缓冲区（额外+1用于字符串结束符）
        char *kernelBuf = LOS_MemAlloc(m_aucSysMem1, len + 1);
        if (kernelBuf == NULL) {  // 内存分配失败
            return ENOMEM;       // 返回内存不足错误码
        }

        // 从用户空间复制数据到内核缓冲区
        if (LOS_ArchCopyFromUser(kernelBuf, src, len) != 0) {
            (VOID)LOS_MemFree(m_aucSysMem1, kernelBuf);  // 复制失败，释放已分配内存
            return EFAULT;                               // 返回地址错误码
        }
        kernelBuf[len] = '\0';  // 添加字符串结束符
        *kbuf = kernelBuf;       // 设置输出内核缓冲区指针
        return 0;                // 成功返回0
    }
    return 0;  // 非用户空间地址，直接返回成功
}

/**
 * @brief 解析用户输入的容器限制值（字符串转整数）
 * @param[in] pf    Proc文件结构体指针
 * @param[in] buf   用户输入的字符串缓冲区
 * @param[in] count 输入数据长度
 * @return 成功返回解析后的整数值，失败返回错误码
 */
static int GetContainerLimitValue(struct ProcFile *pf, const CHAR *buf, size_t count)
{
    int value;               // 解析后的数值结果
    char *kbuf = NULL;       // 内核缓冲区指针（用于用户空间数据复制）

    // 参数合法性检查（Proc文件、目录项、缓冲区、长度均需有效）
    if ((pf == NULL) || (pf->pPDE == NULL) || (buf == NULL) || (count <= 0)) {
        return -EINVAL;      // 参数无效，返回错误码
    }

    // 将用户空间数据复制到内核缓冲区
    unsigned ret = MemUserCopy(buf, count, &kbuf);
    if (ret != 0) {          // 复制失败
        return -ret;         // 返回错误码
    } else if ((ret == 0) && (kbuf != NULL)) {
        buf = (const char *)kbuf;  // 复制成功，使用内核缓冲区数据
    }

    // 检查输入是否为纯数字（防止非数值输入）
    if (strspn(buf, "0123456789") != count) {
        (void)LOS_MemFree(m_aucSysMem1, kbuf);  // 释放内核缓冲区
        return -EINVAL;                         // 输入非法，返回错误码
    }
    value = atoi(buf);  // 字符串转整数
    (void)LOS_MemFree(m_aucSysMem1, kbuf);      // 释放内核缓冲区
    return value;                               // 返回解析后的数值
}

/**
 * @brief 处理容器限制值写入请求（proc接口写回调）
 * @param[in] pf    Proc文件结构体指针
 * @param[in] buf   用户输入的限制值字符串
 * @param[in] size  输入数据长度
 * @param[in] ppos  文件偏移量（未使用）
 * @return 成功返回输入数据长度，失败返回-EINVAL
 */
static ssize_t ProcSysUserWrite(struct ProcFile *pf, const char *buf, size_t size, loff_t *ppos)
{
    (VOID)ppos;  // 未使用的参数，避免编译警告
    unsigned ret; // 操作结果

    // 解析用户输入的限制值
    int value = GetContainerLimitValue(pf, buf, size);
    if (value < 0) {       // 解析失败
        return -EINVAL;    // 返回错误码
    }

    // 从proc目录项获取容器类型（通过data字段传递）
    ContainerType type = (ContainerType)(uintptr_t)pf->pPDE->data;
    // 设置容器限制值
    ret = OsSetContainerLimit(type, value);
    if (ret != LOS_OK) {   // 设置失败
        return -EINVAL;    // 返回错误码
    }
    return size;           // 成功返回输入数据长度
}

/**
 * @brief 读取容器限制值和当前计数（proc接口读回调）
 * @param[in] seqBuf 序列缓冲区结构体指针，用于输出信息
 * @param[in] v      指向容器类型的指针（void*类型需强制转换）
 * @return 成功返回0，失败返回EINVAL
 */
static int ProcSysUserRead(struct SeqBuf *seqBuf, void *v)
{
    unsigned ret;  // 操作结果
    // 检查缓冲区和数据指针有效性
    if ((seqBuf == NULL) || (v == NULL)) {
        return EINVAL;    // 参数无效，返回错误码
    }

    // 将void*转换为容器类型
    ContainerType type = (ContainerType)(uintptr_t)v;
    // 获取容器限制值
    ret = OsGetContainerLimit(type);
    if (ret == OS_INVALID_VALUE) {  // 获取失败
        return EINVAL;              // 返回错误码
    }
    // 输出限制值和当前计数（格式：limit: 值
count: 值
）
    (void)LosBufPrintf(seqBuf, "\nlimit: %u\n", ret);
    (void)LosBufPrintf(seqBuf, "count: %u\n", OsGetContainerCount(type));
    return 0;                       // 成功返回0
}

/**
 * @brief 系统用户proc文件操作集合
 * @details 包含容器限制值的读/写回调函数
 */
static const struct ProcFileOperations SYS_USER_OPT = {
    .read = ProcSysUserRead,   // 读操作回调：读取限制值和计数
    .write = ProcSysUserWrite  // 写操作回调：设置限制值
};

/**
 * @brief 系统用户容器配置数组
 * @details 定义/proc/sys/user目录下的所有文件节点，根据编译选项决定启用哪些容器类型
 */
static struct ProcSysUser g_sysUser[] = {
#ifdef LOSCFG_MNT_CONTAINER  // 条件编译：启用挂载容器
    {
        .name = "max_mnt_container",       // 文件名：最大挂载容器数
        .mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH,  // 权限：用户读写、组读写、其他读
        .type = MNT_CONTAINER,              // 容器类型：挂载容器
        .fileOps = &SYS_USER_OPT            // 使用通用文件操作集
    },
#endif
#ifdef LOSCFG_PID_CONTAINER  // 条件编译：启用PID容器
    {
        .name = "max_pid_container",       // 文件名：最大PID容器数
        .mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH,  // 权限：用户读写、组读写、其他读
        .type = PID_CONTAINER,              // 容器类型：PID容器
        .fileOps = &SYS_USER_OPT            // 使用通用文件操作集
    },
#endif
#ifdef LOSCFG_USER_CONTAINER  // 条件编译：启用用户容器
    {
        .name = "max_user_container",      // 文件名：最大用户容器数
        .mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH,  // 权限：用户读写、组读写、其他读
        .type = USER_CONTAINER,             // 容器类型：用户容器
        .fileOps = &SYS_USER_OPT            // 使用通用文件操作集
    },
#endif
#ifdef LOSCFG_UTS_CONTAINER  // 条件编译：启用UTS容器
    {
        .name = "max_uts_container",       // 文件名：最大UTS容器数
        .mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH,  // 权限：用户读写、组读写、其他读
        .type = UTS_CONTAINER,              // 容器类型：UTS容器
        .fileOps = &SYS_USER_OPT            // 使用通用文件操作集
    },
#endif
#ifdef LOSCFG_UTS_CONTAINER  // 条件编译：启用时间容器（注意：此处类型可能应为TIME_CONTAINER，疑似代码重复定义）
    {
        .name = "max_time_container",      // 文件名：最大时间容器数
        .mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH,  // 权限：用户读写、组读写、其他读
        .type = UTS_CONTAINER,              // 容器类型：UTS容器（注意：此处可能应为TIME_CONTAINER）
        .fileOps = &SYS_USER_OPT            // 使用通用文件操作集
    },
#endif
#ifdef LOSCFG_IPC_CONTAINER  // 条件编译：启用IPC容器
    {
        .name = "max_ipc_container",       // 文件名：最大IPC容器数
        .mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH,  // 权限：用户读写、组读写、其他读
        .type = IPC_CONTAINER,              // 容器类型：IPC容器
        .fileOps = &SYS_USER_OPT            // 使用通用文件操作集
    },
#endif
#ifdef LOSCFG_NET_CONTAINER  // 条件编译：启用网络容器
    {
        .name = "max_net_container",       // 文件名：最大网络容器数
        .mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH,  // 权限：用户读写、组读写、其他读
        .type = NET_CONTAINER,              // 容器类型：网络容器
        .fileOps = &SYS_USER_OPT            // 使用通用文件操作集
    },
#endif
};

/**
 * @brief 创建系统用户容器相关的proc文件节点
 * @param[in] parent 父目录proc节点指针
 * @return 成功返回0，失败返回-1
 */
static int ProcCreateSysUser(struct ProcDirEntry *parent)
{
    struct ProcDataParm parm;  // proc数据参数结构体
    // 遍历所有系统用户容器配置
    for (int index = 0; index < (sizeof(g_sysUser) / sizeof(struct ProcSysUser)); index++) {
        struct ProcSysUser *sysUser = &g_sysUser[index];  // 获取当前配置项
        parm.data = (void *)(uintptr_t)sysUser->type;     // 设置容器类型为关联数据
        parm.dataType = PROC_DATA_STATIC;                 // 数据类型：静态数据
        // 创建proc文件节点
        struct ProcDirEntry *userFile = ProcCreateData(sysUser->name, sysUser->mode, parent, sysUser->fileOps, &parm);
        if (userFile == NULL) {  // 创建失败
            PRINT_ERR("create /proc/%s/%s error!\n", parent->name, sysUser->name);  // 输出错误信息
            return -1;                                                              // 返回失败
        }
    }
    return 0;  // 所有节点创建成功，返回0
}

/**
 * @brief /proc/sys/user目录权限模式
 * @details 目录类型 | 用户读写执行 | 组读执行 | 其他读执行
 */
#define PROC_SYS_USER_MODE (S_IFDIR | S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH)

/**
 * @brief 初始化/proc/sys/user目录及子节点
 * @details 创建/proc/sys目录和/proc/sys/user目录，并在user目录下创建各容器限制文件
 */
void ProcSysUserInit(void)
{
    // 创建/proc/sys目录
    struct ProcDirEntry *parentPDE = CreateProcEntry("sys", PROC_SYS_USER_MODE, NULL);
    if (parentPDE == NULL) {  // 创建失败
        return;               // 直接返回
    }
    // 在sys目录下创建user子目录
    struct ProcDirEntry *pde = CreateProcEntry("user", PROC_SYS_USER_MODE, parentPDE);
    if (pde == NULL) {        // 创建失败
        PRINT_ERR("create /proc/process error!\n");  // 输出错误信息（注意：错误信息中的路径应为/proc/sys/user）
        return;                                       // 返回
    }

    // 在user目录下创建各容器限制文件
    int ret = ProcCreateSysUser(pde);
    if (ret < 0) {  // 创建失败
        PRINT_ERR("Create proc sys user failed!\n");  // 输出错误信息
    }
    return;
}
#endif  // LOSCFG_KERNEL_CONTAINER