/*
 * Copyright (c) 2013-2019 Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020-2023 Huawei Device Co., Ltd. All rights reserved.
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

#include "proc_fs.h"
#include "internal.h"
#include "stdio.h"
#include "sys/mount.h"
#include "sys/stat.h"
#include "los_init.h"

#ifdef LOSCFG_FS_PROC
// proc文件系统挂载点路径宏定义  
#define PROCFS_MOUNT_POINT  "/proc"  
// 挂载点路径长度宏定义（减去字符串结束符'\0'）  
#define PROCFS_MOUNT_POINT_SIZE (sizeof(PROCFS_MOUNT_POINT) - 1)  

/**  
 * @brief 初始化proc文件系统  
 * @details 创建/proc目录并挂载procfs文件系统，初始化各类proc节点  
 * @return 无返回值  
 */  
void ProcFsInit(void)  
{  
    int ret;  // 函数返回值变量，用于错误检查  

    // 创建/proc目录，使用默认权限模式  
    ret = mkdir(PROCFS_MOUNT_POINT, PROCFS_DEFAULT_MODE);  
    if (ret < 0) {  // 目录创建失败处理  
        PRINT_ERR("failed to mkdir %s, errno = %d\n", PROCFS_MOUNT_POINT, get_errno());  // 打印错误信息及错误码  
        return;  
    }  

    // 挂载procfs文件系统到/proc目录  
    ret = mount(NULL, PROCFS_MOUNT_POINT, "procfs", 0, NULL);  
    if (ret) {  // 挂载失败处理  
        PRINT_ERR("mount procfs err %d\n", ret);  // 打印挂载错误码  
        return;  
    }  

    // 初始化各类proc节点  
    ProcMountsInit();  // 初始化/proc/mounts节点  
#if defined(LOSCFG_SHELL_CMD_DEBUG) && defined(LOSCFG_KERNEL_VM)  
    ProcVmmInit();  // 当启用shell调试命令和内核虚拟内存时，初始化虚拟内存相关proc节点  
#endif  
    ProcProcessInit();  // 初始化进程相关proc节点(/proc/[pid]目录等)  
    ProcUptimeInit();  // 初始化系统运行时间节点(/proc/uptime)  
    ProcFsCacheInit();  // 初始化文件系统缓存信息节点(/proc/fs_cache)  
    ProcFdInit();  // 初始化文件描述符相关节点(/proc/fd)  
#ifdef LOSCFG_KERNEL_PM  
    ProcPmInit();  // 当启用电源管理时，初始化电源管理相关节点(/proc/power)  
#endif  
#ifdef LOSCFG_PROC_PROCESS_DIR  
    ProcSysMemInfoInit();  // 当启用进程目录时，初始化内存信息节点(/proc/meminfo)  
    ProcFileSysInit();  // 当启用进程目录时，初始化文件系统信息节点(/proc/filesystems)  
#endif  
#ifdef LOSCFG_KERNEL_PLIMITS  
    ProcLimitsInit();  // 当启用进程限制功能时，初始化限制器节点(/proc/plimits)  
#endif  
#ifdef LOSCFG_KERNEL_CONTAINER  
    ProcSysUserInit();  // 当启用容器功能时，初始化用户相关系统节点(/proc/sys/user)  
#endif  
}  

// 将ProcFsInit函数注册为内核扩展模块初始化函数，初始化级别为LOS_INIT_LEVEL_KMOD_EXTENDED  
LOS_MODULE_INIT(ProcFsInit, LOS_INIT_LEVEL_KMOD_EXTENDED);  

#endif  // 文件级条件编译结束  
