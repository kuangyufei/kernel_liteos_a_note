/*
 * Copyright (c) 2013-2019 Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020-2022 Huawei Device Co., Ltd. All rights reserved.
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

#include "los_config.h"
#include "sys/mount.h"

#ifdef LOSCFG_SHELL

#include "los_typedef.h"
#include "shell.h"
#include "sys/stat.h"
#include "stdlib.h"
#include "unistd.h"
#include "fcntl.h"
#include "sys/statfs.h"
#include "stdio.h"
#include "pthread.h"

#include "shcmd.h"
#include "securec.h"
#include "show.h"
#include "los_syscall.h"

#include "los_process_pri.h"
#include <ctype.h>
#include "fs/fs_operation.h"

// 文件通配符操作类型枚举定义
typedef enum {
    RM_RECURSIVER,  // 递归删除
    RM_FILE,        // 删除文件
    RM_DIR,         // 删除目录
    CP_FILE,        // 复制文件
    CP_COUNT        // 复制计数
} wildcard_type;  // 通配符类型枚举

// 条件错误处理宏定义，当条件满足时执行消息函数并处理
#define ERROR_OUT_IF(condition, message_function, handler) \
    do { \
        if (condition) { \
            message_function; \
            handler; \
        } \
    } while (0)

// 设置错误信息并输出
// 参数: errcode - 错误码, err_message - 错误消息
static inline void set_err(int errcode, const char *err_message)
{
    set_errno(errcode);  // 设置全局错误码
    perror(err_message); // 输出错误信息到标准错误流
}

// 执行目录切换操作
// 参数: path - 目标目录路径
// 返回值: 成功返回0，失败返回-1
int osShellCmdDoChdir(const char *path)
{
    char *fullpath = NULL;                  // 规范化后的完整路径
    char *fullpath_bak = NULL;              // 完整路径备份指针
    int ret;                                // 函数返回值
    char *shell_working_directory = OsShellGetWorkingDirectory();  // 获取当前shell工作目录
    if (shell_working_directory == NULL) {
        return -1;  // 获取工作目录失败
    }

    if (path == NULL) {  // 如果未提供路径参数
        LOS_TaskLock();  // 任务上锁
        PRINTK("%s\n", shell_working_directory);  // 打印当前工作目录
        LOS_TaskUnlock();  // 任务解锁

        return 0;  // 成功返回
    }

    // 检查路径长度是否超过最大限制
    ERROR_OUT_IF(strlen(path) > PATH_MAX, set_err(ENOTDIR, "cd error"), return -1);

    // 规范化路径（处理相对路径和特殊符号）
    ret = vfs_normalize_path(shell_working_directory, path, &fullpath);
    ERROR_OUT_IF(ret < 0, set_err(-ret, "cd error"), return -1);  // 路径规范化失败处理

    fullpath_bak = fullpath;  // 备份路径指针
    ret = chdir(fullpath);    // 执行目录切换
    if (ret < 0) {  // 切换失败
        free(fullpath_bak);   // 释放路径内存
        perror("cd");         // 输出错误信息
        return -1;            // 返回失败
    }

    /* copy full path to working directory */

    LOS_TaskLock();  // 任务上锁（保护共享资源）
    // 更新shell工作目录
    ret = strncpy_s(shell_working_directory, PATH_MAX, fullpath, strlen(fullpath));
    if (ret != EOK) {  // 字符串复制失败
        free(fullpath_bak);  // 释放路径内存
        LOS_TaskUnlock();    // 任务解锁
        return -1;           // 返回失败
    }
    LOS_TaskUnlock();  // 任务解锁
    /* release normalize directory path name */

    free(fullpath_bak);  // 释放路径内存

    return 0;  // 成功返回
}

// 实现ls命令功能，列出目录内容
// 参数: argc - 参数数量, argv - 参数数组
// 返回值: 成功返回0，失败返回-1
int osShellCmdLs(int argc, const char **argv)
{
    char *fullpath = NULL;                  // 规范化后的完整路径
    const char *filename = NULL;            // 目标文件名/目录名
    int ret;                                // 函数返回值
    char *shell_working_directory = OsShellGetWorkingDirectory();  // 获取当前shell工作目录
    if (shell_working_directory == NULL) {
        return -1;  // 获取工作目录失败
    }

    // 检查参数数量是否超过1个（ls命令最多接受一个目录参数）
    ERROR_OUT_IF(argc > 1, PRINTK("ls or ls [DIRECTORY]\n"), return -1);

    if (argc == 0) {  // 无参数时列出当前目录
        ls(shell_working_directory);
        return 0;    // 成功返回
    }

    filename = argv[0];  // 获取目标目录参数
    // 规范化路径
    ret = vfs_normalize_path(shell_working_directory, filename, &fullpath);
    ERROR_OUT_IF(ret < 0, set_err(-ret, "ls error"), return -1);  // 路径规范化失败处理

    ls(fullpath);       // 列出目标目录内容
    free(fullpath);     // 释放路径内存

    return 0;  // 成功返回
}

// 实现cd命令功能，提供用户接口
// 参数: argc - 参数数量, argv - 参数数组
// 返回值: 始终返回0
int osShellCmdCd(int argc, const char **argv)
{
    if (argc == 0) {  // 无参数时切换到根目录
        (void)osShellCmdDoChdir("/");
        return 0;
    }

    (void)osShellCmdDoChdir(argv[0]);  // 带参数时切换到指定目录

    return 0;  // 始终返回0（实际错误在osShellCmdDoChdir中处理）
}

#define CAT_BUF_SIZE  512               // cat命令缓冲区大小
#define CAT_TASK_PRIORITY  10           // cat命令任务优先级
#define CAT_TASK_STACK_SIZE  0x3000     // cat命令任务栈大小
pthread_mutex_t g_mutex_cat = PTHREAD_MUTEX_INITIALIZER;  // cat命令互斥锁

// cat命令文件内容显示任务函数
// 参数: arg - 文件路径指针
// 返回值: 成功返回0，失败返回-1
int osShellCmdDoCatShow(UINTPTR arg)
{
    int ret = 0;                         // 函数返回值
    char buf[CAT_BUF_SIZE];              // 读取缓冲区
    size_t size, written, toWrite;       // 读取大小、已写大小、待写大小
    ssize_t cnt;                         // 实际写入字节数
    char *fullpath = (char *)arg;        // 目标文件路径
    FILE *ini = NULL;                    // 文件指针

    (void)pthread_mutex_lock(&g_mutex_cat);  // 加互斥锁（确保线程安全）
    ini = fopen(fullpath, "r");             // 以只读方式打开文件
    if (ini == NULL) {  // 文件打开失败
        ret = -1;
        perror("cat error");  // 输出错误信息
        goto out;             // 跳转到清理流程
    }

    do {  // 循环读取文件内容
        (void)memset_s(buf, sizeof(buf), 0, CAT_BUF_SIZE);  // 清空缓冲区
        size = fread(buf, 1, CAT_BUF_SIZE, ini);            // 读取文件内容
        if ((int)size < 0) {  // 读取错误
            ret = -1;
            perror("cat error");
            goto out_with_fclose;  // 跳转到关闭文件流程
        }

        // 循环写入到标准输出（处理可能的部分写入）
        for (toWrite = size, written = 0; toWrite > 0;) {
            cnt = write(1, buf + written, toWrite);  // 写入标准输出
            if (cnt == 0) {  // 写入0字节，延迟后重试（避免任务饥饿）
                (void)LOS_TaskDelay(1);
                continue;
            } else if (cnt < 0) {  // 写入错误
                perror("cat write error");
                break;
            }

            written += cnt;  // 更新已写大小
            toWrite -= cnt;  // 更新待写大小
        }
    }
    while (size > 0);  // 直到读取到文件末尾

out_with_fclose:
    (void)fclose(ini);  // 关闭文件
out:
    free(fullpath);     // 释放路径内存
    (void)pthread_mutex_unlock(&g_mutex_cat);  // 释放互斥锁
    return ret;         // 返回结果
}

// 实现cat命令功能，创建显示任务
// 参数: argc - 参数数量, argv - 参数数组
// 返回值: 成功返回0，失败返回-1
int osShellCmdCat(int argc, const char **argv)
{
    char *fullpath = NULL;                  // 规范化后的完整路径
    int ret;                                // 函数返回值
    unsigned int ca_task;                   // 任务ID
    struct Vnode *vnode = NULL;             // vnode结构体指针
    TSK_INIT_PARAM_S init_param;            // 任务初始化参数
    char *shell_working_directory = OsShellGetWorkingDirectory();  // 获取当前shell工作目录
    if (shell_working_directory == NULL) {
        return -1;  // 获取工作目录失败
    }

    // 检查参数数量是否为1（cat命令需要一个文件参数）
    ERROR_OUT_IF(argc != 1, PRINTK("cat [FILE]\n"), return -1);

    // 规范化文件路径
    ret = vfs_normalize_path(shell_working_directory, argv[0], &fullpath);
    ERROR_OUT_IF(ret < 0, set_err(-ret, "cat error"), return -1);  // 路径规范化失败处理

    VnodeHold();  // 获取vnode哈希表锁
    // 查找文件vnode
    ret = VnodeLookup(fullpath, &vnode, O_RDONLY);
    if (ret != LOS_OK) {  // 查找失败
        set_errno(-ret);
        perror("cat error");
        VnodeDrop();      // 释放vnode哈希表锁
        free(fullpath);   // 释放路径内存
        return -1;
    }
    if (vnode->type != VNODE_TYPE_REG) {  // 检查是否为普通文件
        set_errno(EINVAL);
        perror("cat error");
        VnodeDrop();      // 释放vnode哈希表锁
        free(fullpath);   // 释放路径内存
        return -1;
    }
    VnodeDrop();  // 释放vnode哈希表锁
    // 初始化任务参数
    (void)memset_s(&init_param, sizeof(init_param), 0, sizeof(TSK_INIT_PARAM_S));
    init_param.pfnTaskEntry = (TSK_ENTRY_FUNC)osShellCmdDoCatShow;  // 任务入口函数
    init_param.usTaskPrio   = CAT_TASK_PRIORITY;                    // 任务优先级
    init_param.auwArgs[0]   = (UINTPTR)fullpath;                    // 传递文件路径参数
    init_param.uwStackSize  = CAT_TASK_STACK_SIZE;                  // 任务栈大小
    init_param.pcName       = "shellcmd_cat";                       // 任务名称
    init_param.uwResved     = LOS_TASK_STATUS_DETACHED | OS_TASK_FLAG_SPECIFIES_PROCESS;  // 任务属性（分离状态）

    // 创建cat任务
    ret = (int)LOS_TaskCreate(&ca_task, &init_param);
    if (ret != LOS_OK) {
        free(fullpath);  // 创建失败时释放路径内存
    }

    return ret;  // 返回创建结果
}

// NFS挂载函数弱引用（实际实现可能在其他模块）
// 参数: server_ip_and_path - 服务器IP和路径, mount_path - 挂载点路径, uid - 用户ID, gid - 组ID
// 返回值: 成功返回0，失败返回非0
static int nfs_mount_ref(const char *server_ip_and_path, const char *mount_path,
                         unsigned int uid, unsigned int gid) __attribute__((weakref("nfs_mount")));

// 解析挂载选项标志
// 参数: options - 挂载选项字符串
// 返回值: 解析后的挂载标志位
static unsigned long get_mountflags(const char *options)
{
    unsigned long mountfalgs = 0;  // 挂载标志位
    char *p;                       // 字符串分割指针
    // 分割选项字符串并解析每个选项
    while ((options != NULL) && (p = strsep((char**)&options, ",")) != NULL) {
        if (strncmp(p, "ro", strlen("ro")) == 0) {  // 只读模式
            mountfalgs |= MS_RDONLY;
        } else if (strncmp(p, "rw", strlen("rw")) == 0) {  // 读写模式
            mountfalgs &= ~MS_RDONLY;
        } else if (strncmp(p, "nosuid", strlen("nosuid")) == 0) {  // 禁止setuid
            mountfalgs |= MS_NOSUID;
        } else if (strncmp(p, "suid", strlen("suid")) == 0) {  // 允许setuid
            mountfalgs &= ~MS_NOSUID;
        } else {
            continue;  // 忽略未知选项
        }
    }

    return mountfalgs;  // 返回解析后的标志位
}

// 打印mount命令用法
static inline void print_mount_usage(void)
{
    PRINTK("mount [DEVICE] [PATH] [NAME]\n");
}

// 实现mount命令功能，挂载文件系统
// 参数: argc - 参数数量, argv - 参数数组
// 返回值: 成功返回0，失败返回-1
int osShellCmdMount(int argc, const char **argv)
{
    int ret;                                // 函数返回值
    char *fullpath = NULL;                  // 规范化后的挂载点路径
    const char *filename = NULL;            // 挂载点路径参数
    unsigned int gid, uid;                  // 用户组ID和用户ID
    char *data = NULL;                      // 挂载数据
    char *filessystemtype = NULL;           // 文件系统类型
    unsigned long mountfalgs;               // 挂载标志位
    char *shell_working_directory = OsShellGetWorkingDirectory();  // 获取当前shell工作目录
    if (shell_working_directory == NULL) {
        return -1;  // 获取工作目录失败
    }

    // 检查参数数量是否至少为3个
    ERROR_OUT_IF(argc < 3, print_mount_usage(), return OS_FAIL);

    // 处理带选项的mount命令（-t或-o）
    if (strncmp(argv[0], "-t", 2) == 0 || strncmp(argv[0], "-o", 2) == 0) // 2: "-t"字符串长度
    {
        if (argc < 4) { // 4: 带选项时至少需要4个参数
            PRINTK("mount -t/-o [DEVICE] [PATH] [NAME]\n");
            return -1;
        }

        filename = argv[2]; // 2: 文件路径参数索引
        // 规范化挂载点路径
        ret = vfs_normalize_path(shell_working_directory, filename, &fullpath);
        ERROR_OUT_IF(ret < 0, set_err(-ret, "mount error"), return -1);

        // 处理NFS文件系统挂载
        if (strncmp(argv[3], "nfs", 3) == 0) { // 3: 文件系统类型参数索引
            if (argc <= 6) { // 6: 包含uid或gid的参数数量
                // 解析uid和gid参数（默认值为0）
                uid = ((argc >= 5) && (argv[4] != NULL)) ? (unsigned int)strtoul(argv[4], (char **)NULL, 0) : 0;
                gid = ((argc == 6) && (argv[5] != NULL)) ? (unsigned int)strtoul(argv[5], (char **)NULL, 0) : 0;

                if (nfs_mount_ref != NULL) {  // 如果NFS挂载函数存在
                    ret = nfs_mount_ref(argv[1], fullpath, uid, gid);
                    if (ret != LOS_OK) {
                        PRINTK("mount -t [DEVICE] [PATH] [NAME]\n");
                    }
                } else {
                    PRINTK("can't find nfs_mount\n");  // NFS挂载函数未找到
                }
                free(fullpath);  // 释放路径内存
                return 0;
            }
        }

        // 解析文件系统类型、挂载标志和数据
        filessystemtype = (argc >= 4) ? (char *)argv[3] : NULL; /* 4: 指定文件系统类型的参数数量, 3: 文件系统类型参数索引 */
        mountfalgs = (argc >= 5) ? get_mountflags((const char *)argv[4]) : 0; /* 4: 用户选项参数索引 */
        data = (argc >= 6) ? (char *)argv[5] : NULL; /* 5: 用户数据参数索引, 6: 需要数据参数的总数量 */

        // 执行挂载操作（设备为"0"时表示无设备）
        if (strcmp(argv[1], "0") == 0) {
            ret = mount((const char *)NULL, fullpath, filessystemtype, mountfalgs, data);
        } else {
            ret = mount(argv[1], fullpath, filessystemtype, mountfalgs, data); /* 3: 文件系统类型 */
        }
        if (ret != LOS_OK) {
            perror("mount error");  // 挂载失败
        } else {
            PRINTK("mount ok\n");    // 挂载成功
        }
    } else {  // 处理不带选项的mount命令
        filename = argv[1];  // 挂载点路径参数
        // 规范化挂载点路径
        ret = vfs_normalize_path(shell_working_directory, filename, &fullpath);
        ERROR_OUT_IF(ret < 0, set_err(-ret, "mount error"), return -1);

        // 处理NFS文件系统挂载
        if (strncmp(argv[2], "nfs", 3) == 0) { // 2: 文件系统类型参数索引, 3: "nfs"字符串长度
            if (argc <= 5) { // 5: 包含gid和uid的参数数量
                // 解析uid和gid参数（默认值为0）
                uid = ((argc >= 4) && (argv[3] != NULL)) ? (unsigned int)strtoul(argv[3], (char **)NULL, 0) : 0;
                gid = ((argc == 5) && (argv[4] != NULL)) ? (unsigned int)strtoul(argv[4], (char **)NULL, 0) : 0;

                if (nfs_mount_ref != NULL) {  // 如果NFS挂载函数存在
                    ret = nfs_mount_ref(argv[0], fullpath, uid, gid);
                    if (ret != LOS_OK) {
                        PRINTK("mount [DEVICE] [PATH] [NAME]\n");
                    }
                } else {
                    PRINTK("can't find nfs_mount\n");  // NFS挂载函数未找到
                }
                free(fullpath);  // 释放路径内存
                return 0;
            }

            print_mount_usage();  // 参数数量错误，打印用法
            free(fullpath);       // 释放路径内存
            return 0;
        }

        // 解析挂载标志和数据
        mountfalgs = (argc >= 4) ? get_mountflags((const char *)argv[3]) : 0;  /* 3: 用户选项参数索引 */
        data = (argc >= 5) ? (char *)argv[4] : NULL; /* 4: 用户数据参数索引, 5: 需要数据参数的总数量 */

        // 执行挂载操作（设备为"0"时表示无设备）
        if (strcmp(argv[0], "0") == 0) {
            ret = mount((const char *)NULL, fullpath, argv[2], mountfalgs, data);
        } else {
            ret = mount(argv[0], fullpath, argv[2], mountfalgs, data);  /* 2: 文件系统类型参数索引 */
        }
        if (ret != LOS_OK) {
            perror("mount error");  // 挂载失败
        } else {
            PRINTK("mount ok\n");    // 挂载成功
        }
    }

    free(fullpath);  // 释放路径内存
    return 0;        // 返回结果
}

// 实现umount命令功能，卸载文件系统
// 参数: argc - 参数数量, argv - 参数数组
// 返回值: 成功返回0，失败返回-1
int osShellCmdUmount(int argc, const char **argv)
{
    int ret;                                // 函数返回值
    const char *filename = NULL;            // 卸载路径参数
    char *fullpath = NULL;                  // 规范化后的卸载路径
    char *target_path = NULL;               // 目标卸载路径
    int cmp_num;                            // 路径比较长度
    char *work_path = NULL;                 // 当前工作目录指针
    char *shell_working_directory = OsShellGetWorkingDirectory();  // 获取当前shell工作目录
    if (shell_working_directory == NULL) {
        return -1;  // 获取工作目录失败
    }
    work_path = shell_working_directory;

    // 检查参数数量是否为0（无参数时打印用法）
    ERROR_OUT_IF(argc == 0, PRINTK("umount [PATH]\n"), return 0);

    filename = argv[0];  // 获取卸载路径参数
    // 规范化卸载路径
    ret = vfs_normalize_path(shell_working_directory, filename, &fullpath);
    ERROR_OUT_IF(ret < 0, set_err(-ret, "umount error"), return -1);

    target_path = fullpath;
    cmp_num = strlen(fullpath);
    // 检查当前工作目录是否在目标卸载路径下
    ret = strncmp(work_path, target_path, cmp_num);
    if (ret == 0) {  // 工作目录在卸载路径下
        work_path += cmp_num;
        // 检查是否为直接子目录或当前目录
        if (*work_path == '/' || *work_path == '\0') {
            set_errno(EBUSY);  // 设置设备忙错误
            perror("umount error");
            free(fullpath);    // 释放路径内存
            return -1;
        }
    }

    ret = umount(fullpath);  // 执行卸载操作
    free(fullpath);          // 释放路径内存
    if (ret != LOS_OK) {
        perror("umount error");  // 卸载失败
        return 0;
    }

    PRINTK("umount ok\n");  // 卸载成功
    return 0;
}

// 实现mkdir命令功能，创建目录
// 参数: argc - 参数数量, argv - 参数数组
// 返回值: 成功返回0，失败返回-1
int osShellCmdMkdir(int argc, const char **argv)
{
    int ret;                                // 函数返回值
    char *fullpath = NULL;                  // 规范化后的目录路径
    const char *filename = NULL;            // 目录名参数
    char *shell_working_directory = OsShellGetWorkingDirectory();  // 获取当前shell工作目录
    if (shell_working_directory == NULL) {
        return -1;  // 获取工作目录失败
    }

    // 检查参数数量是否为1（mkdir命令需要一个目录参数）
    ERROR_OUT_IF(argc != 1, PRINTK("mkdir [DIRECTORY]\n"), return 0);

    filename = argv[0];  // 获取目录名参数
    // 规范化目录路径
    ret = vfs_normalize_path(shell_working_directory, filename, &fullpath);
    ERROR_OUT_IF(ret < 0, set_err(-ret, "mkdir error"), return -1);

    // 创建目录（权限：用户、组、其他都有读写执行权限）
    ret = mkdir(fullpath, S_IRWXU | S_IRWXG | S_IRWXO);
    if (ret == -1) {
        perror("mkdir error");  // 创建失败
    }
    free(fullpath);  // 释放路径内存
    return 0;        // 返回结果
}

// 实现pwd命令功能，打印当前工作目录
// 参数: argc - 参数数量, argv - 参数数组
// 返回值: 成功返回0，失败返回-1
int osShellCmdPwd(int argc, const char **argv)
{
    char buf[SHOW_MAX_LEN] = {0};           // 缓冲区
    DIR *dir = NULL;                        // 目录流指针
    char *shell_working_directory = OsShellGetWorkingDirectory();  // 获取当前shell工作目录
    if (shell_working_directory == NULL) {
        return -1;  // 获取工作目录失败
    }

    // 检查是否有参数（pwd命令不接受参数）
    ERROR_OUT_IF(argc > 0, PRINTK("\nUsage: pwd\n"), return -1);

    dir = opendir(shell_working_directory);  // 打开当前工作目录（验证目录存在性）
    if (dir == NULL) {
        perror("pwd error");  // 打开失败
        return -1;
    }

    LOS_TaskLock();  // 任务上锁（保护共享资源）
    // 复制工作目录到缓冲区
    if (strncpy_s(buf, SHOW_MAX_LEN, shell_working_directory, SHOW_MAX_LEN - 1) != EOK) {
        LOS_TaskUnlock();
        PRINTK("pwd error: strncpy_s error!\n");
        (void)closedir(dir);  // 关闭目录流
        return -1;
    }
    LOS_TaskUnlock();  // 任务解锁

    PRINTK("%s\n", buf);  // 打印工作目录
    (void)closedir(dir);  // 关闭目录流
    return 0;             // 成功返回
}
// 打印statfs命令用法说明
static inline void print_statfs_usage(void)
{
    PRINTK("Usage  :\n");
    PRINTK("    statfs <path>\n");
    PRINTK("    path  : Mounted file system path that requires query information\n");
    PRINTK("Example:\n");
    PRINTK("    statfs /ramfs\n");
}

// 实现statfs命令功能，查询文件系统信息
// 参数: argc - 参数数量, argv - 参数数组
// 返回值: 成功返回0，失败返回-1
int osShellCmdStatfs(int argc, const char **argv)
{
    struct statfs sfs;                      // 文件系统信息结构体
    int result;                             // 函数返回值
    unsigned long long total_size, free_size; // 总大小和空闲大小
    char *fullpath = NULL;                  // 规范化后的完整路径
    const char *filename = NULL;            // 目标路径参数
    char *shell_working_directory = OsShellGetWorkingDirectory();  // 获取当前shell工作目录
    if (shell_working_directory == NULL) {
        return -1;  // 获取工作目录失败
    }

    // 检查参数数量是否为1（statfs命令需要一个路径参数）
    ERROR_OUT_IF(argc != 1, PRINTK("statfs failed! Invalid argument!\n"), return -1);

    (void)memset_s(&sfs, sizeof(sfs), 0, sizeof(sfs));  // 清空文件系统信息结构体

    filename = argv[0];  // 获取目标路径参数
    // 规范化路径
    result = vfs_normalize_path(shell_working_directory, filename, &fullpath);
    ERROR_OUT_IF(result < 0, set_err(-result, "statfs error"), return -1);  // 路径规范化失败处理

    result = statfs(fullpath, &sfs);  // 获取文件系统信息
    free(fullpath);                   // 释放路径内存

    if (result != 0 || sfs.f_type == 0) {  // 检查获取结果是否有效
        PRINTK("statfs failed! Invalid argument!\n");
        print_statfs_usage();  // 打印用法说明
        return -1;
    }

    // 计算总大小和空闲大小（块大小 * 块数量）
    total_size  = (unsigned long long)sfs.f_bsize * sfs.f_blocks;
    free_size   = (unsigned long long)sfs.f_bsize * sfs.f_bfree;

    // 打印文件系统详细信息
    PRINTK("statfs got:\n f_type     = %d\n cluster_size   = %d\n", sfs.f_type, sfs.f_bsize);
    PRINTK(" total_clusters = %llu\n free_clusters  = %llu\n", sfs.f_blocks, sfs.f_bfree);
    PRINTK(" avail_clusters = %llu\n f_namelen    = %d\n", sfs.f_bavail, sfs.f_namelen);
    PRINTK("\n%s\n total size: %4llu Bytes\n free  size: %4llu Bytes\n", argv[0], total_size, free_size);

    return 0;  // 成功返回
}

// 实现touch命令功能，创建空文件或更新文件时间戳
// 参数: argc - 参数数量, argv - 参数数组
// 返回值: 成功返回0，失败返回-1
int osShellCmdTouch(int argc, const char **argv)
{
    int ret;                                // 函数返回值
    int fd = -1;                            // 文件描述符
    char *fullpath = NULL;                  // 规范化后的完整路径
    const char *filename = NULL;            // 目标文件名参数
    char *shell_working_directory = OsShellGetWorkingDirectory();  // 获取当前shell工作目录
    if (shell_working_directory == NULL) {
        return -1;  // 获取工作目录失败
    }

    // 检查参数数量是否为1（touch命令需要一个文件参数）
    ERROR_OUT_IF(argc != 1, PRINTK("touch [FILE]\n"), return -1);

    filename = argv[0];  // 获取目标文件名
    // 规范化文件路径
    ret = vfs_normalize_path(shell_working_directory, filename, &fullpath);
    ERROR_OUT_IF(ret < 0, set_err(-ret, "touch error"), return -1);  // 路径规范化失败处理

    // 打开文件（不存在则创建，存在则更新时间戳）
    fd = open(fullpath, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
    free(fullpath);  // 释放路径内存
    if (fd == -1) {  // 打开/创建文件失败
        perror("touch error");
        return -1;
    }

    (void)close(fd);  // 关闭文件
    return 0;         // 成功返回
}

#define CP_BUF_SIZE 4096  // cp命令缓冲区大小（4KB）
pthread_mutex_t g_mutex_cp = PTHREAD_MUTEX_INITIALIZER;  // cp命令互斥锁

// 实现文件复制功能的内部函数
// 参数: src_filepath - 源文件路径, dst_filename - 目标文件路径
// 返回值: 成功返回0，失败返回-1
static int os_shell_cmd_do_cp(const char *src_filepath, const char *dst_filename)
{
    int  ret;                               // 函数返回值
    char *src_fullpath = NULL;              // 源文件规范化路径
    char *dst_fullpath = NULL;              // 目标文件规范化路径
    const char *src_filename = NULL;        // 源文件名（不含路径）
    char *dst_filepath = NULL;              // 目标文件完整路径
    char *buf = NULL;                       // 复制缓冲区
    const char *filename = NULL;            // 临时文件名指针
    ssize_t r_size, w_size;                 // 读取/写入字节数
    int src_fd = -1;                        // 源文件描述符
    int dst_fd = -1;                        // 目标文件描述符
    struct stat stat_buf;                   // 文件状态结构体
    mode_t src_mode;                        // 源文件权限模式
    char *shell_working_directory = OsShellGetWorkingDirectory();  // 获取当前shell工作目录
    if (shell_working_directory == NULL) {
        return -1;  // 获取工作目录失败
    }

    buf = (char *)malloc(CP_BUF_SIZE);  // 分配复制缓冲区
    if (buf == NULL) {
        PRINTK("cp error: Out of memory!\n");  // 内存分配失败
        return -1;
    }

    /* Get source fullpath. */

    // 规范化源文件路径
    ret = vfs_normalize_path(shell_working_directory, src_filepath, &src_fullpath);
    if (ret < 0) {
        set_errno(-ret);
        PRINTK("cp error: %s\n", strerror(errno));
        free(buf);  // 释放缓冲区
        return -1;
    }

    /* Is source path exist? */

    // 获取源文件状态
    ret = stat(src_fullpath, &stat_buf);
    if (ret == -1) {
        PRINTK("cp %s error: %s\n", src_fullpath, strerror(errno));
        goto errout_with_srcpath;  // 跳转到源路径错误处理
    }
    src_mode = stat_buf.st_mode;  // 保存源文件权限
    /* Is source path a directory? */

    if (S_ISDIR(stat_buf.st_mode)) {  // 检查是否为目录
        PRINTK("cp %s error: Source file can't be a directory.\n", src_fullpath);
        goto errout_with_srcpath;  // 跳转到源路径错误处理
    }

    /* Get dest fullpath. */

    // 复制目标文件路径
    dst_fullpath = strdup(dst_filename);
    if (dst_fullpath == NULL) {
        PRINTK("cp error: Out of memory.\n");  // 内存分配失败
        goto errout_with_srcpath;  // 跳转到源路径错误处理
    }

    /* Is dest path exist? */

    // 检查目标路径是否存在
    ret = stat(dst_fullpath, &stat_buf);
    if (ret == 0) {
        /* Is dest path a directory? */

        if (S_ISDIR(stat_buf.st_mode)) {  // 如果目标是目录
            /* Get source file name without '/'. */

            // 提取源文件名（去掉路径部分）
            src_filename = src_filepath;
            while (1) {
                filename = strchr(src_filename, '/');
                if (filename == NULL) {
                    break;  // 找不到更多'/'，当前src_filename即为文件名
                }
                src_filename = filename + 1;  // 移动到'/'后的下一个字符
            }

            /* Add the source file after dest path. */

            // 构造目标文件完整路径（目录+源文件名）
            ret = vfs_normalize_path(dst_fullpath, src_filename, &dst_filepath);
            if (ret < 0) {
                set_errno(-ret);
                PRINTK("cp error. %s.\n", strerror(errno));
                goto errout_with_path;  // 跳转到路径错误处理
            }
            free(dst_fullpath);  // 释放旧的目标路径
            dst_fullpath = dst_filepath;  // 更新为新的目标路径
        }
    }

    /* Is dest file same as source file? */

    // 检查源文件和目标文件是否为同一文件
    if (strcmp(src_fullpath, dst_fullpath) == 0) {
        PRINTK("cp error: '%s' and '%s' are the same file\n", src_fullpath, dst_fullpath);
        goto errout_with_path;  // 跳转到路径错误处理
    }

    /* Copy begins. */

    (void)pthread_mutex_lock(&g_mutex_cp);  // 加互斥锁（确保线程安全）
    src_fd = open(src_fullpath, O_RDONLY);  // 以只读方式打开源文件
    if (src_fd < 0) {
        PRINTK("cp error: can't open %s. %s.\n", src_fullpath, strerror(errno));
        goto errout_with_mutex;  // 跳转到互斥锁错误处理
    }

    // 以创建、只写、截断方式打开目标文件，并保留源文件权限
    dst_fd = open(dst_fullpath, O_CREAT | O_WRONLY | O_TRUNC, src_mode);
    if (dst_fd < 0) {
        PRINTK("cp error: can't create %s. %s.\n", dst_fullpath, strerror(errno));
        goto errout_with_srcfd;  // 跳转到源文件描述符错误处理
    }

    // 循环读取并写入文件内容
    do {
        (void)memset_s(buf, CP_BUF_SIZE, 0, CP_BUF_SIZE);  // 清空缓冲区
        r_size = read(src_fd, buf, CP_BUF_SIZE);  // 读取源文件内容
        if (r_size < 0) {  // 读取错误
            PRINTK("cp %s %s failed. %s.\n", src_fullpath, dst_fullpath, strerror(errno));
            goto errout_with_fd;  // 跳转到文件描述符错误处理
        }
        w_size = write(dst_fd, buf, r_size);  // 写入目标文件
        if (w_size != r_size) {  // 写入字节数不匹配
            PRINTK("cp %s %s failed. %s.\n", src_fullpath, dst_fullpath, strerror(errno));
            goto errout_with_fd;  // 跳转到文件描述符错误处理
        }
    } while (r_size == CP_BUF_SIZE);  // 直到读取到文件末尾

    /* Release resource. */

    free(buf);  // 释放缓冲区
    free(src_fullpath);  // 释放源路径内存
    free(dst_fullpath);  // 释放目标路径内存
    (void)close(src_fd);  // 关闭源文件
    (void)close(dst_fd);  // 关闭目标文件
    (void)pthread_mutex_unlock(&g_mutex_cp);  // 释放互斥锁
    return LOS_OK;  // 成功返回

// 错误处理标签（使用goto集中处理错误）
errout_with_fd:
    (void)close(dst_fd);
errout_with_srcfd:
    (void)close(src_fd);
errout_with_mutex:
    (void)pthread_mutex_unlock(&g_mutex_cp);
errout_with_path:
    free(dst_fullpath);
errout_with_srcpath:
    free(src_fullpath);
    free(buf);
    return -1;
}

/* The separator and EOF for a directory fullpath: '/'and '\0' */
#define SEPARATOR_EOF_LEN 2  // 目录路径分隔符和结束符长度（'/'和'\0'）

// 递归删除目录及其内容的内部函数
// 参数: pathname - 目录路径
// 返回值: 成功返回0，失败返回-1
static int os_shell_cmd_do_rmdir(const char *pathname)
{
    struct dirent *dirent = NULL;  // 目录项结构体指针
    struct stat stat_info;         // 文件状态结构体
    DIR *d = NULL;                 // 目录流指针
    char *fullpath = NULL;         // 完整路径
    int ret;

    (void)memset_s(&stat_info, sizeof(stat_info), 0, sizeof(struct stat));
    if (stat(pathname, &stat_info) != 0) {  // 获取路径状态
        return -1;  // 获取失败
    }

    // 如果是普通文件或符号链接，直接删除
    if (S_ISREG(stat_info.st_mode) || S_ISLNK(stat_info.st_mode)) {
        return remove(pathname);
    }
    d = opendir(pathname);  // 打开目录
    if (d == NULL) {
        return -1;  // 打开失败
    }
    // 遍历目录项
    while (1) {
        dirent = readdir(d);
        if (dirent == NULL) {
            break;  // 遍历结束
        }
        // 跳过"."和".."目录
        if (strcmp(dirent->d_name, "..") && strcmp(dirent->d_name, ".")) {
            // 计算完整路径缓冲区大小
            size_t fullpath_buf_size = strlen(pathname) + strlen(dirent->d_name) + SEPARATOR_EOF_LEN;
            fullpath = (char *)malloc(fullpath_buf_size);  // 分配路径内存
            if (fullpath == NULL) {
                PRINTK("malloc failure!\n");
                (void)closedir(d);
                return -1;
            }
            // 构造完整路径
            ret = snprintf_s(fullpath, fullpath_buf_size, fullpath_buf_size - 1, "%s/%s", pathname, dirent->d_name);
            if (ret < 0) {
                PRINTK("name is too long!\n");
                free(fullpath);
                (void)closedir(d);
                return -1;
            }
            (void)os_shell_cmd_do_rmdir(fullpath);  // 递归删除子目录/文件
            free(fullpath);  // 释放路径内存
        }
    }
    (void)closedir(d);  // 关闭目录
    return rmdir(pathname);  // 删除空目录
}

/*  Wildcard matching operations  */
// 通配符匹配操作函数（支持*和?）
// 参数: src - 源字符串, filename - 带通配符的模式字符串
// 返回值: 匹配成功返回0，失败返回-1
static int os_wildcard_match(const char *src, const char *filename)
{
    int ret;

    if (*src != '\0') {  // 源字符串未结束
        if (*filename == '*') {  // 处理*通配符
            // 跳过连续的*和?
            while ((*filename == '*') || (*filename == '?')) {
                filename++;
            }

            if (*filename == '\0') {  // 模式字符串结束
                return 0;  // *匹配剩余所有字符，成功返回
            }

            // 找到源字符串中与模式字符匹配的位置
            while (*src != '\0' && !(*src == *filename)) {
                src++;
            }

            if (*src == '\0') {  // 源字符串结束但模式未结束
                return -1;  // 匹配失败
            }

            ret = os_wildcard_match(src, filename);  // 递归匹配后续字符

            // 如果当前位置不匹配，尝试源字符串下一个位置
            while ((ret != 0) && (*(++src) != '\0')) {
                if (*src == *filename) {
                    ret = os_wildcard_match(src, filename);
                }
            }
            return ret;
        } else {  // 处理普通字符或?通配符
            // 字符匹配或?通配符匹配
            if ((*src == *filename) || (*filename == '?')) {
                return os_wildcard_match(++src, ++filename);  // 递归匹配下一个字符
            }
            return -1;  // 字符不匹配
        }
    }

    // 源字符串已结束，检查模式字符串剩余部分
    while (*filename != '\0') {
        if (*filename != '*') {  // 剩余非*字符，匹配失败
            return -1;
        }
        filename++;  // 跳过剩余的*
    }
    return 0;  // 匹配成功
}

/*   To determine whether a wildcard character exists in a path   */
// 检查路径中是否包含通配符（*或?）
// 参数: filename - 路径字符串
// 返回值: 包含通配符返回1，否则返回0
static int os_is_containers_wildcard(const char *filename)
{
    while (*filename != '\0') {
        if ((*filename == '*') || (*filename == '?')) {  // 检查是否为通配符
            return 1;  // 找到通配符
        }
        filename++;
    }
    return 0;  // 未找到通配符
}

/*  Delete a matching file or directory  */
// 根据类型删除匹配的文件或目录
// 参数: fullpath - 文件/目录完整路径, mark - 删除类型（RM_RECURSIVER/RM_FILE/RM_DIR）
// 返回值: 成功返回0，失败返回-1
static int os_wildcard_delete_file_or_dir(const char *fullpath, wildcard_type mark)
{
    int ret;

    switch (mark) {  // 根据删除类型执行不同操作
        case RM_RECURSIVER:  // 递归删除目录及其内容
            ret = os_shell_cmd_do_rmdir(fullpath);
            break;
        case RM_FILE:  // 删除文件
            ret = unlink(fullpath);
            break;
        case RM_DIR:  // 删除空目录
            ret = rmdir(fullpath);
            break;
        default:  // 无效类型
            return VFS_ERROR;
    }
    if (ret == -1) {  // 删除失败
        PRINTK("%s  ", fullpath);
        perror("rm/rmdir error!");
        return ret;
    }

    PRINTK("%s match successful!delete!\n", fullpath);  // 删除成功
    return 0;
}

/*  Split the path with wildcard characters  */
/**
 * @brief 分割包含通配符的路径字符串
 * @param fullpath 完整路径字符串
 * @param handle 输出参数，指向通配符所在段的指针
 * @param wait 输出参数，指向通配符后续路径段的指针
 * @return 返回分割后的路径前缀部分
 */
static char* os_wildcard_split_path(char *fullpath, char **handle, char **wait)
{
    int n = 0;  // 循环计数器
    int a = 0;  // 记录最后一个 '/' 的位置
    int b = 0;  // 记录第一个通配符 '*' 或 '?' 的位置
    int len  = strlen(fullpath);  // 路径字符串长度

    for (n = 0; n < len; n++) {
        if (fullpath[n] == '/') {  // 遇到路径分隔符
            if (b != 0) {  // 如果已经找到通配符
                fullpath[n] = '\0';  // 在分隔符处截断字符串
                *wait = fullpath + n + 1;  // 设置后续路径段指针
                break;  // 退出循环
            }
            a = n;  // 更新最后一个 '/' 的位置
        } else if (fullpath[n] == '*' || fullpath[n] == '?') {  // 遇到通配符
            b = n;  // 记录通配符位置
            fullpath[a] = '\0';  // 在最近的 '/' 处截断字符串
            if (a == 0) {  // 如果 '/' 是路径起始位置
                *handle = fullpath + a + 1;  // 设置通配符段指针
                continue;  // 继续循环
            }
            *handle = fullpath + a + 1;  // 设置通配符段指针
        }
    }
    return fullpath;  // 返回路径前缀部分
}

/*  Handling entry of the path with wildcard characters  */

/**
 * @brief 处理包含通配符的路径条目
 * @param fullpath 完整路径字符串
 * @param dst 目标路径或计数指针
 * @param mark 操作标记，指定是复制文件、计数文件还是删除文件/目录
 * @return 成功返回0，失败返回VFS_ERROR
 */
static int os_wildcard_extract_directory(char *fullpath, void *dst, wildcard_type mark)
{
    char separator[] = "/";  // 路径分隔符
    char src[PATH_MAX] = {0};  // 源路径缓冲区
    struct dirent *dirent = NULL;  // 目录项指针
    char *f = NULL;  // 路径前缀
    char *s = NULL;  // 通配符段
    char *t = NULL;  // 后续路径段
    int ret = 0;  // 返回值
    DIR *d = NULL;  // 目录流指针
    struct stat stat_buf;  // 文件状态缓冲区
    int deleteFlag = 0;  // 删除成功标志

    f = os_wildcard_split_path(fullpath, &s, &t);  // 分割路径

    if (s == NULL) {  // 如果没有通配符段
        if (mark == CP_FILE) {  // 复制文件操作
            ret = os_shell_cmd_do_cp(fullpath, dst);  // 执行文件复制
        } else if (mark == CP_COUNT) {  // 文件计数操作
            ret = stat(fullpath, &stat_buf);  // 获取文件状态
            if (ret == 0 && (S_ISREG(stat_buf.st_mode) || S_ISLNK(stat_buf.st_mode))) {  // 如果是普通文件或符号链接
                (*(int *)dst)++;  // 增加计数
            }
        } else {  // 删除操作
            ret = os_wildcard_delete_file_or_dir(fullpath, mark);  // 执行删除
        }
        return ret;  // 返回结果
    }

    d = (*f == '\0') ? opendir("/") : opendir(f);  // 打开目录，默认为根目录
    if (d == NULL) {  // 打开目录失败
        perror("opendir error");  // 输出错误信息
        return VFS_ERROR;  // 返回错误
    }

    while (1) {  // 遍历目录
        dirent = readdir(d);  // 读取目录项
        if (dirent == NULL) {  // 读取完毕或出错
            break;  // 退出循环
        }

        ret = strcpy_s(src, PATH_MAX, f);  // 复制路径前缀到缓冲区
        if (ret != EOK) {  // 复制失败
            goto closedir_out;  // 跳转到关闭目录
        }

        ret = os_wildcard_match(dirent->d_name, s);  // 匹配通配符
        if (ret == 0) {  // 匹配成功
            ret = strcat_s(src, sizeof(src), separator);  // 添加路径分隔符
            if (ret != EOK) {  // 拼接失败
                goto closedir_out;  // 跳转到关闭目录
            }
            ret = strcat_s(src, sizeof(src), dirent->d_name);  // 添加目录项名称
            if (ret != EOK) {  // 拼接失败
                goto closedir_out;  // 跳转到关闭目录
            }
            if (t == NULL) {  // 如果没有后续路径段
                if (mark == CP_FILE) {  // 复制文件操作
                    ret = os_shell_cmd_do_cp(src, dst);  // 执行文件复制
                } else if (mark == CP_COUNT) {  // 文件计数操作
                    ret = stat(src, &stat_buf);  // 获取文件状态
                    if (ret == 0 && (S_ISREG(stat_buf.st_mode) || S_ISLNK(stat_buf.st_mode))) {  // 如果是普通文件或符号链接
                        (*(int *)dst)++;  // 增加计数
                        if ((*(int *)dst) > 1) {  // 如果计数超过1
                            break;  // 退出循环
                        }
                    }
                } else {  // 删除操作
                    ret = os_wildcard_delete_file_or_dir(src, mark);  // 执行删除
                    if (ret == 0) {  // 删除成功
                        deleteFlag = 1;  // 设置删除成功标志
                    }
                }
            } else {  // 如果有后续路径段
                ret = strcat_s(src, sizeof(src), separator);  // 添加路径分隔符
                if (ret != EOK) {  // 拼接失败
                    goto closedir_out;  // 跳转到关闭目录
                }
                ret = strcat_s(src, sizeof(src), t);  // 添加后续路径段
                if (ret != EOK) {  // 拼接失败
                    goto closedir_out;  // 跳转到关闭目录
                }
                ret = os_wildcard_extract_directory(src, dst, mark);  // 递归处理路径
                if (mark == CP_COUNT && (*(int *)dst) > 1) {  // 如果是计数操作且计数超过1
                    break;  // 退出循环
                }
            }
        }
    }
    (void)closedir(d);  // 关闭目录
    if (deleteFlag == 1) {  // 如果有删除成功
        ret = 0;  // 设置返回值为成功
    }
    return ret;  // 返回结果
closedir_out:  // 关闭目录标签
    (void)closedir(d);  // 关闭目录
    return VFS_ERROR;  // 返回错误
}

/**
 * @brief shell命令cp的实现函数
 * @param argc 参数个数
 * @param argv 参数数组
 * @return 成功返回0，失败返回-1或VFS_ERROR
 */
int osShellCmdCp(int argc, const char **argv)
{
    int  ret;  // 返回值
    const char *src = NULL;  // 源路径
    const char *dst = NULL;  // 目标路径
    char *src_fullpath = NULL;  // 规范化后的源路径
    char *dst_fullpath = NULL;  // 规范化后的目标路径
    struct stat stat_buf;  // 文件状态缓冲区
    int count = 0;  // 文件计数
    char *shell_working_directory = OsShellGetWorkingDirectory();  // 获取shell工作目录
    if (shell_working_directory == NULL) {  // 获取工作目录失败
        return -1;  // 返回错误
    }

    ERROR_OUT_IF(argc < 2, PRINTK("cp [SOURCEFILE] [DESTFILE]\n"), return -1);  // 检查参数个数

    src = argv[0];  // 源路径参数
    dst = argv[1];  // 目标路径参数

    /* Get source fullpath. */

    ret = vfs_normalize_path(shell_working_directory, src, &src_fullpath);  // 规范化源路径
    if (ret < 0) {  // 规范化失败
        set_errno(-ret);  // 设置错误号
        PRINTK("cp error:%s\n", strerror(errno));  // 输出错误信息
        return -1;  // 返回错误
    }

    if (src[strlen(src) - 1] == '/') {  // 源路径以 '/' 结尾
        PRINTK("cp %s error: Source file can't be a directory.\n", src);  // 输出错误信息
        goto errout_with_srcpath;  // 跳转到错误处理
    }

    /* Get dest fullpath. */

    ret = vfs_normalize_path(shell_working_directory, dst, &dst_fullpath);  // 规范化目标路径
    if (ret < 0) {  // 规范化失败
        set_errno(-ret);  // 设置错误号
        PRINTK("cp error: can't open %s. %s\n", dst, strerror(errno));  // 输出错误信息
        goto errout_with_srcpath;  // 跳转到错误处理
    }

    /* Is dest path exist? */

    ret = stat(dst_fullpath, &stat_buf);  // 获取目标路径状态
    if (ret < 0) {  // 目标路径不存在
        /* Is dest path a directory? */

        if (dst[strlen(dst) - 1] == '/') {  // 目标路径以 '/' 结尾
            PRINTK("cp error: %s, %s.\n", dst_fullpath, strerror(errno));  // 输出错误信息
            goto errout_with_path;  // 跳转到错误处理
        }
    } else {  // 目标路径存在
        if ((S_ISREG(stat_buf.st_mode) || S_ISLNK(stat_buf.st_mode)) && dst[strlen(dst) - 1] == '/') {  // 目标是文件但路径以 '/' 结尾
            PRINTK("cp error: %s is not a directory.\n", dst_fullpath);  // 输出错误信息
            goto errout_with_path;  // 跳转到错误处理
        }
    }

    if (os_is_containers_wildcard(src_fullpath)) {  // 源路径包含通配符
        if (ret < 0 || S_ISREG(stat_buf.st_mode) || S_ISLNK(stat_buf.st_mode)) {  // 目标不存在或为文件/链接
            char *src_copy = strdup(src_fullpath);  // 复制源路径
            if (src_copy == NULL) {  // 内存分配失败
                PRINTK("cp error : Out of memory.\n");  // 输出错误信息
                goto errout_with_path;  // 跳转到错误处理
            }
            (void)os_wildcard_extract_directory(src_copy, &count, CP_COUNT);  // 计算匹配文件数量
            free(src_copy);  // 释放内存
            if (count > 1) {  // 匹配文件超过1个
                PRINTK("cp error : %s is not a directory.\n", dst_fullpath);  // 输出错误信息
                goto errout_with_path;  // 跳转到错误处理
            }
        }
        ret = os_wildcard_extract_directory(src_fullpath, dst_fullpath, CP_FILE);  // 处理通配符复制
    } else {  // 源路径不包含通配符
        ret = os_shell_cmd_do_cp(src_fullpath, dst_fullpath);  // 直接复制文件
    }
    free(dst_fullpath);  // 释放目标路径内存
    free(src_fullpath);  // 释放源路径内存
    return ret;  // 返回结果

errout_with_path:  // 错误处理标签(释放目标路径)
    free(dst_fullpath);  // 释放目标路径内存
errout_with_srcpath:  // 错误处理标签(释放源路径)
    free(src_fullpath);  // 释放源路径内存
    return VFS_ERROR;  // 返回错误
}

/**
 * @brief 打印rm命令用法
 */
static inline void print_rm_usage(void)
{
    PRINTK("rm [FILE] or rm [-r/-R] [FILE]\n");  // 输出用法信息
}

/**
 * @brief shell命令rm的实现函数
 * @param argc 参数个数
 * @param argv 参数数组
 * @return 成功返回0，失败返回-1
 */
int osShellCmdRm(int argc, const char **argv)
{
    int  ret = 0;  // 返回值
    char *fullpath = NULL;  // 规范化后的路径
    const char *filename = NULL;  // 文件名参数
    char *shell_working_directory = OsShellGetWorkingDirectory();  // 获取shell工作目录
    if (shell_working_directory == NULL) {  // 获取工作目录失败
        return -1;  // 返回错误
    }

    ERROR_OUT_IF(argc != 1 && argc != 2, print_rm_usage(), return -1);  // 检查参数个数

    if (argc == 2) { // 2: arguments include "-r" or "-R"  // 参数包含递归选项
        ERROR_OUT_IF(strcmp(argv[0], "-r") != 0 && strcmp(argv[0], "-R") != 0, print_rm_usage(), return -1);  // 检查选项合法性

        filename = argv[1];  // 文件名参数
        ret = vfs_normalize_path(shell_working_directory, filename, &fullpath);  // 规范化路径
        ERROR_OUT_IF(ret < 0, set_err(-ret, "rm error"), return -1);  // 检查规范化结果

        if (os_is_containers_wildcard(fullpath)) {  // 路径包含通配符
            ret = os_wildcard_extract_directory(fullpath, NULL, RM_RECURSIVER);  // 递归删除匹配项
        } else {  // 路径不包含通配符
            ret = os_shell_cmd_do_rmdir(fullpath);  // 删除目录
        }
    } else {  // 参数不包含递归选项
        filename = argv[0];  // 文件名参数
        ret = vfs_normalize_path(shell_working_directory, filename, &fullpath);  // 规范化路径
        ERROR_OUT_IF(ret < 0, set_err(-ret, "rm error"), return -1);  // 检查规范化结果

        if (os_is_containers_wildcard(fullpath)) {  // 路径包含通配符
            ret = os_wildcard_extract_directory(fullpath, NULL, RM_FILE);  // 删除匹配文件
        } else {  // 路径不包含通配符
            ret = unlink(fullpath);  // 删除文件
        }
    }
    if (ret == -1) {  // 删除失败
        perror("rm error");  // 输出错误信息
    }
    free(fullpath);  // 释放路径内存
    return 0;  // 返回结果
}

/**
 * @brief shell命令rmdir的实现函数
 * @param argc 参数个数
 * @param argv 参数数组
 * @return 成功返回0，失败返回-1
 */
int osShellCmdRmdir(int argc, const char **argv)
{
    int  ret;  // 返回值
    char *fullpath = NULL;  // 规范化后的路径
    const char *filename = NULL;  // 目录名参数
    char *shell_working_directory = OsShellGetWorkingDirectory();  // 获取shell工作目录
    if (shell_working_directory == NULL) {  // 获取工作目录失败
        return -1;  // 返回错误
    }

    ERROR_OUT_IF(argc == 0, PRINTK("rmdir [DIRECTORY]\n"), return -1);  // 检查参数个数

    filename = argv[0];  // 目录名参数
    ret = vfs_normalize_path(shell_working_directory, filename, &fullpath);  // 规范化路径
    ERROR_OUT_IF(ret < 0, set_err(-ret, "rmdir error"), return -1);  // 检查规范化结果

    if (os_is_containers_wildcard(fullpath)) {  // 路径包含通配符
        ret = os_wildcard_extract_directory(fullpath, NULL, RM_DIR);  // 删除匹配目录
    } else {  // 路径不包含通配符
        ret = rmdir(fullpath);  // 删除目录
    }
    if (ret == -1) {  // 删除失败
        PRINTK("rmdir %s failed. Error: %s.\n", fullpath, strerror(errno));  // 输出错误信息
    }
    free(fullpath);  // 释放路径内存

    return 0;  // 返回结果
}

/**
 * @brief shell命令sync的实现函数
 * @param argc 参数个数
 * @param argv 参数数组
 * @return 成功返回0，失败返回-1
 */
int osShellCmdSync(int argc, const char **argv)
{
    ERROR_OUT_IF(argc > 0, PRINTK("\nUsage: sync\n"), return -1);  // 检查参数个数

    sync();  // 执行同步操作
    return 0;  // 返回成功
}

/**
 * @brief shell命令lsfd的实现函数
 * @param argc 参数个数
 * @param argv 参数数组
 * @return 成功返回0，失败返回-1
 */
int osShellCmdLsfd(int argc, const char **argv)
{
    ERROR_OUT_IF(argc > 0, PRINTK("\nUsage: lsfd\n"), return -1);  // 检查参数个数

    lsfd();  // 列出文件描述符

    return 0;  // 返回成功
}

/**
 * @brief 检查字符串是否为有效的数字
 * @param arg 待检查的字符串
 * @return 是数字返回0，否则返回-1
 */
int checkNum(const char *arg)
{
    int i = 0;  // 循环计数器
    if (arg == NULL) {  // 参数为空
        return -1;  // 返回错误
    }
    if (arg[0] == '-') {  // 负数情况
        /* exclude the '-' */

        i = 1;  // 跳过负号
    }
    for (; arg[i] != 0; i++) {  // 遍历字符串
        if (!isdigit(arg[i])) {  // 非数字字符
            return -1;  // 返回错误
        }
    }
    return 0;  // 返回成功
}

#ifdef LOSCFG_KERNEL_SYSCALL
/**
 * @brief shell命令su的实现函数
 * @param argc 参数个数
 * @param argv 参数数组
 * @return 成功返回0，失败返回-1
 */
int osShellCmdSu(int argc, const char **argv)
{
    int su_uid;  // 用户ID
    int su_gid;  // 组ID

    if (argc == 0) {  // 无参数，默认切换到root
        /* for su root */

        su_uid = 0;  // root用户ID
        su_gid = 0;  // root组ID
    } else {  // 有参数
        ERROR_OUT_IF((argc != 2), PRINTK("su [uid_num] [gid_num]\n"), return -1);  // 检查参数个数
        ERROR_OUT_IF((checkNum(argv[0]) != 0) || (checkNum(argv[1]) != 0), /* check argv is digit */
        PRINTK("check uid_num and gid_num is digit\n"), return -1);  // 检查参数是否为数字

        su_uid = atoi(argv[0]);  // 转换用户ID
        su_gid = atoi(argv[1]);  // 转换组ID

        ERROR_OUT_IF((su_uid < 0) || (su_uid > 60000) || (su_gid < 0) ||
            (su_gid > 60000), PRINTK("uid_num or gid_num out of range!they should be [0~60000]\n"), return -1);  // 检查ID范围
    }

    SysSetUserID(su_uid);  // 设置用户ID
    SysSetGroupID(su_gid);  // 设置组ID
    return 0;  // 返回成功
}
#endif

/**
 * @brief shell命令chmod的实现函数
 * @param argc 参数个数
 * @param argv 参数数组
 * @return 成功返回0，失败返回-1
 */
int osShellCmdChmod(int argc, const char **argv)
{
    int i = 0;  // 循环计数器
    int mode = 0;  // 文件权限模式
    int ret;  // 返回值
    char *fullpath = NULL;  // 规范化后的路径
    const char *filename = NULL;  // 文件名参数
    struct IATTR attr = {0};  // 文件属性结构体
    char *shell_working_directory = NULL;  // shell工作目录
    const char *p = NULL;  // 权限字符串指针
#define MODE_BIT 3 /* 3 bits express 1 mode */  // 权限位数量

    ERROR_OUT_IF((argc != 2), PRINTK("Usage: chmod <MODE> [FILE]\n"), return -1);  // 检查参数个数

    p = argv[0];  // 权限字符串
    while (p[i]) {  // 解析权限字符串
        if ((p[i] <= '7') && (p[i] >= '0')) {  // 合法的权限数字
            mode = ((uint)mode << MODE_BIT) | (uint)(p[i] - '0');  // 计算权限值
        } else {  // 非法字符
            PRINTK("check the input <MODE>\n");  // 输出错误信息
            return -1;  // 返回错误
        }
        i++;  // 移动到下一个字符
    }
    filename = argv[1];  // 文件名参数

    shell_working_directory = OsShellGetWorkingDirectory();  // 获取工作目录
    if (shell_working_directory == NULL) {  // 获取失败
        return -1;  // 返回错误
    }
    ret = vfs_normalize_path(shell_working_directory, filename, &fullpath);  // 规范化路径
    ERROR_OUT_IF(ret < 0, set_err(-ret, "chmod error\n"), return -1);  // 检查规范化结果

    attr.attr_chg_mode = mode;  // 设置新权限
    attr.attr_chg_valid = CHG_MODE; /* change mode */  // 指定修改权限
    ret = chattr(fullpath, &attr);  // 修改文件属性
    if (ret < 0) {  // 修改失败
        free(fullpath);  // 释放路径内存
        PRINTK("chmod error! %s\n", strerror(errno));  // 输出错误信息
        return ret;  // 返回错误
    }

    free(fullpath);  // 释放路径内存
    return 0;  // 返回成功
}

/**
 * @brief shell命令chown的实现函数
 * @param argc 参数个数
 * @param argv 参数数组
 * @return 成功返回0，失败返回-1
 */
int osShellCmdChown(int argc, const char **argv)
{
    int ret;  // 返回值
    char *fullpath = NULL;  // 规范化后的路径
    const char *filename = NULL;  // 文件名参数
    struct IATTR attr;  // 文件属性结构体
    uid_t owner = -1;  // 用户ID
    gid_t group = -1;  // 组ID
    attr.attr_chg_valid = 0;  // 属性修改标志

    ERROR_OUT_IF(((argc != 2) && (argc != 3)), PRINTK("Usage: chown [OWNER] [GROUP] FILE\n"), return -1);  // 检查参数个数
    if (argc == 2) { // 2: chown owner of file  // 修改用户ID
        ERROR_OUT_IF((checkNum(argv[0]) != 0), PRINTK("check OWNER is digit\n"), return -1);  // 检查用户ID是否为数字
        owner = atoi(argv[0]);  // 转换用户ID
        filename = argv[1];  // 文件名参数
    }
    if (argc == 3) { // 3: chown both owner and group  // 同时修改用户ID和组ID
        ERROR_OUT_IF((checkNum(argv[0]) != 0), PRINTK("check OWNER is digit\n"), return -1);  // 检查用户ID是否为数字
        ERROR_OUT_IF((checkNum(argv[1]) != 0), PRINTK("check GROUP is digit\n"), return -1);  // 检查组ID是否为数字
        owner = atoi(argv[0]);  // 转换用户ID
        group = atoi(argv[1]);  // 转换组ID
        filename = argv[2];  // 文件名参数
    }

    if (group != -1) {  // 如果指定了组ID
        attr.attr_chg_gid = group;  // 设置组ID
        attr.attr_chg_valid |= CHG_GID;  // 设置组ID修改标志
    }
    if (owner != -1) {  // 如果指定了用户ID
        attr.attr_chg_uid = owner;  // 设置用户ID
        attr.attr_chg_valid |= CHG_UID;  // 设置用户ID修改标志
    }

    char *shell_working_directory = OsShellGetWorkingDirectory();  // 获取工作目录
    if (shell_working_directory == NULL) {  // 获取失败
        return -1;  // 返回错误
    }
    ret = vfs_normalize_path(shell_working_directory, filename, &fullpath);  // 规范化路径
    ERROR_OUT_IF(ret < 0, set_err(-ret, "chown error\n"), return -1);  // 检查规范化结果

    ret = chattr(fullpath, &attr);  // 修改文件属性
    if (ret < 0) {  // 修改失败
        free(fullpath);  // 释放路径内存
        PRINTK("chown error! %s\n", strerror(errno));  // 输出错误信息
        return ret;  // 返回错误
    }

    free(fullpath);  // 释放路径内存
    return 0;  // 返回成功
}

/**
 * @brief shell命令chgrp的实现函数
 * @param argc 参数个数
 * @param argv 参数数组
 * @return 成功返回0，失败返回-1
 */
int osShellCmdChgrp(int argc, const char **argv)
{
    int ret;  // 返回值
    char *fullpath = NULL;  // 规范化后的路径
    const char *filename = NULL;  // 文件名参数
    struct IATTR attr;  // 文件属性结构体
    gid_t group;  // 组ID
    attr.attr_chg_valid = 0;  // 属性修改标志
    ERROR_OUT_IF((argc != 2), PRINTK("Usage: chgrp GROUP FILE\n"), return -1);  // 检查参数个数
    ERROR_OUT_IF((checkNum(argv[0]) != 0), PRINTK("check GROUP is digit\n"), return -1);  // 检查组ID是否为数字
    group = atoi(argv[0]);  // 转换组ID
    filename = argv[1];  // 文件名参数

    if (group != -1) {  // 如果指定了组ID
        attr.attr_chg_gid = group;  // 设置组ID
        attr.attr_chg_valid |= CHG_GID;  // 设置组ID修改标志
    }

    char *shell_working_directory = OsShellGetWorkingDirectory();  // 获取工作目录
    if (shell_working_directory == NULL) {  // 获取失败
        return -1;  // 返回错误
    }
    ret = vfs_normalize_path(shell_working_directory, filename, &fullpath);  // 规范化路径
    ERROR_OUT_IF(ret < 0, set_err(-ret, "chmod error"), return -1);  // 检查规范化结果

    ret = chattr(fullpath, &attr);  // 修改文件属性
    if (ret < 0) {  // 修改失败
        free(fullpath);  // 释放路径内存
        PRINTK("chgrp error! %s\n", strerror(errno));  // 输出错误信息
        return ret;  // 返回错误
    }

    free(fullpath);  // 释放路径内存
    return 0;  // 返回成功
}

#ifdef LOSCFG_SHELL_CMD_DEBUG
SHELLCMD_ENTRY(lsfd_shellcmd, CMD_TYPE_EX, "lsfd", XARGS, (CmdCallBackFunc)osShellCmdLsfd);  // 注册lsfd命令
SHELLCMD_ENTRY(statfs_shellcmd, CMD_TYPE_EX, "statfs", XARGS, (CmdCallBackFunc)osShellCmdStatfs);  // 注册statfs命令
SHELLCMD_ENTRY(touch_shellcmd, CMD_TYPE_EX, "touch", XARGS, (CmdCallBackFunc)osShellCmdTouch);  // 注册touch命令
#ifdef LOSCFG_KERNEL_SYSCALL
SHELLCMD_ENTRY(su_shellcmd, CMD_TYPE_EX, "su", XARGS, (CmdCallBackFunc)osShellCmdSu);  // 注册su命令
#endif
#endif
SHELLCMD_ENTRY(sync_shellcmd, CMD_TYPE_EX, "sync", XARGS, (CmdCallBackFunc)osShellCmdSync);  // 注册sync命令
SHELLCMD_ENTRY(ls_shellcmd, CMD_TYPE_EX, "ls", XARGS, (CmdCallBackFunc)osShellCmdLs);  // 注册ls命令
SHELLCMD_ENTRY(pwd_shellcmd, CMD_TYPE_EX, "pwd", XARGS, (CmdCallBackFunc)osShellCmdPwd);  // 注册pwd命令
SHELLCMD_ENTRY(cd_shellcmd, CMD_TYPE_EX, "cd", XARGS, (CmdCallBackFunc)osShellCmdCd);  // 注册cd命令
SHELLCMD_ENTRY(cat_shellcmd, CMD_TYPE_EX, "cat", XARGS, (CmdCallBackFunc)osShellCmdCat);  // 注册cat命令
SHELLCMD_ENTRY(rm_shellcmd, CMD_TYPE_EX, "rm", XARGS, (CmdCallBackFunc)osShellCmdRm);  // 注册rm命令
SHELLCMD_ENTRY(rmdir_shellcmd, CMD_TYPE_EX, "rmdir", XARGS, (CmdCallBackFunc)osShellCmdRmdir);  // 注册rmdir命令
SHELLCMD_ENTRY(mkdir_shellcmd, CMD_TYPE_EX, "mkdir", XARGS, (CmdCallBackFunc)osShellCmdMkdir);  // 注册mkdir命令
SHELLCMD_ENTRY(chmod_shellcmd, CMD_TYPE_EX, "chmod", XARGS, (CmdCallBackFunc)osShellCmdChmod);  // 注册chmod命令
SHELLCMD_ENTRY(chown_shellcmd, CMD_TYPE_EX, "chown", XARGS, (CmdCallBackFunc)osShellCmdChown);  // 注册chown命令
SHELLCMD_ENTRY(chgrp_shellcmd, CMD_TYPE_EX, "chgrp", XARGS, (CmdCallBackFunc)osShellCmdChgrp);  // 注册chgrp命令
SHELLCMD_ENTRY(mount_shellcmd, CMD_TYPE_EX, "mount", XARGS, (CmdCallBackFunc)osShellCmdMount);  // 注册mount命令
SHELLCMD_ENTRY(umount_shellcmd, CMD_TYPE_EX, "umount", XARGS, (CmdCallBackFunc)osShellCmdUmount);  // 注册umount命令
SHELLCMD_ENTRY(cp_shellcmd, CMD_TYPE_EX, "cp", XARGS, (CmdCallBackFunc)osShellCmdCp);  // 注册cp命令
#endif