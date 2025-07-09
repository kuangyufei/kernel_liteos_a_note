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

/* SYSCALL_HAND_DEF must be defined before including this file. */
/* SYSCALL_HAND_DEF(id, fun, rtype, narg); note if we have 64bit arg, narg should be ARG_NUM_7 */
#ifdef LOSCFG_FS_VFS  // 如果启用虚拟文件系统配置
SYSCALL_HAND_DEF(__NR_read, SysRead, ssize_t, ARG_NUM_3)  // 读取文件内容系统调用
SYSCALL_HAND_DEF(__NR_write, SysWrite, ssize_t, ARG_NUM_3)  // 写入文件内容系统调用
SYSCALL_HAND_DEF(__NR_open, SysOpen, int, ARG_NUM_7)  // 打开文件系统调用
SYSCALL_HAND_DEF(__NR_close, SysClose, int, ARG_NUM_1)  // 关闭文件系统调用
SYSCALL_HAND_DEF(__NR_creat, SysCreat, int, ARG_NUM_2)  // 创建文件系统调用
SYSCALL_HAND_DEF(__NR_link, SysLink, int, ARG_NUM_2)  // 创建硬链接系统调用
SYSCALL_HAND_DEF(__NR_readlink, SysReadlink, ssize_t, ARG_NUM_3)  // 读取符号链接内容系统调用
SYSCALL_HAND_DEF(__NR_symlink, SysSymlink, int, ARG_NUM_2)  // 创建符号链接系统调用
SYSCALL_HAND_DEF(__NR_unlink, SysUnlink, int, ARG_NUM_1)  // 删除文件系统调用

#ifdef LOSCFG_KERNEL_DYNLOAD  // 如果启用内核动态加载配置
SYSCALL_HAND_DEF(__NR_execve, SysExecve, int, ARG_NUM_3)  // 执行程序系统调用
#endif

SYSCALL_HAND_DEF(__NR_sysinfo, SysInfo, int, ARG_NUM_1)  // 获取系统信息系统调用
SYSCALL_HAND_DEF(__NR_fchdir, SysFchdir, int, ARG_NUM_1)  // 通过文件描述符改变当前目录系统调用
SYSCALL_HAND_DEF(__NR_chdir, SysChdir, int, ARG_NUM_1)  // 改变当前目录系统调用
SYSCALL_HAND_DEF(__NR_utimensat, SysUtimensat, int, ARG_NUM_4)  // 设置文件时间属性系统调用
SYSCALL_HAND_DEF(__NR_fchmodat, SysFchmodat, int, ARG_NUM_4)  // 通过文件描述符修改文件权限系统调用
SYSCALL_HAND_DEF(__NR_fchmod, SysFchmod, int, ARG_NUM_2)  // 修改文件描述符指向文件的权限系统调用
SYSCALL_HAND_DEF(__NR_utimensat, SysUtimensat, int, ARG_NUM_4)  // 设置文件时间属性系统调用(重复定义)
SYSCALL_HAND_DEF(__NR_chmod, SysChmod, int, ARG_NUM_2)  // 修改文件权限系统调用
SYSCALL_HAND_DEF(__NR_lseek, SysLseek, off_t, ARG_NUM_7)  // 文件指针定位系统调用 /* current only support 32bit max 4G file */
SYSCALL_HAND_DEF(__NR_mount, SysMount, int, ARG_NUM_5)  // 挂载文件系统系统调用
SYSCALL_HAND_DEF(__NR_umount, SysUmount, int, ARG_NUM_1)  // 卸载文件系统系统调用
SYSCALL_HAND_DEF(__NR_access, SysAccess, int, ARG_NUM_2)  // 检查文件访问权限系统调用
SYSCALL_HAND_DEF(__NR_faccessat, SysFaccessat, int, ARG_NUM_4)  // 通过文件描述符检查文件访问权限系统调用
SYSCALL_HAND_DEF(__NR_sync, SysSync, void, ARG_NUM_0)  // 同步文件系统缓存系统调用
SYSCALL_HAND_DEF(__NR_rename, SysRename, int, ARG_NUM_2)  // 重命名文件系统调用
SYSCALL_HAND_DEF(__NR_mkdir, SysMkdir, int, ARG_NUM_2)  // 创建目录系统调用
SYSCALL_HAND_DEF(__NR_rmdir, SysRmdir, int, ARG_NUM_1)  // 删除目录系统调用
SYSCALL_HAND_DEF(__NR_dup, SysDup, int, ARG_NUM_1)  // 复制文件描述符系统调用
#ifdef LOSCFG_KERNEL_PIPE  // 如果启用内核管道配置
SYSCALL_HAND_DEF(__NR_pipe, SysPipe, int, ARG_NUM_1)  // 创建管道系统调用
#endif
SYSCALL_HAND_DEF(__NR_umount2, SysUmount2, int, ARG_NUM_2)  // 带选项的卸载文件系统系统调用
SYSCALL_HAND_DEF(__NR_ioctl, SysIoctl, int, ARG_NUM_3)  // I/O控制操作系统调用
SYSCALL_HAND_DEF(__NR_fcntl, SysFcntl, int, ARG_NUM_3)  // 文件控制操作系统调用
SYSCALL_HAND_DEF(__NR_dup2, SysDup2, int, ARG_NUM_2)  // 复制文件描述符到指定编号系统调用
SYSCALL_HAND_DEF(__NR_truncate, SysTruncate, int, ARG_NUM_7)  // 截断文件长度系统调用
SYSCALL_HAND_DEF(__NR_ftruncate, SysFtruncate, int, ARG_NUM_7)  // 截断文件描述符指向文件的长度系统调用
SYSCALL_HAND_DEF(__NR_statfs, SysStatfs, int, ARG_NUM_2)  // 获取文件系统状态信息系统调用
SYSCALL_HAND_DEF(__NR_fstatfs, SysFstatfs, int, ARG_NUM_2)  // 获取文件描述符指向文件系统状态信息系统调用
SYSCALL_HAND_DEF(__NR_fstatfs64, SysFstatfs64, int, ARG_NUM_3)  // 64位获取文件系统状态信息系统调用
SYSCALL_HAND_DEF(__NR_stat, SysStat, int, ARG_NUM_2)  // 获取文件状态信息系统调用
SYSCALL_HAND_DEF(__NR_lstat, SysLstat, int, ARG_NUM_2)  // 获取符号链接文件状态信息系统调用
SYSCALL_HAND_DEF(__NR_fstat, SysFstat, int, ARG_NUM_2)  // 获取文件描述符指向文件状态信息系统调用
SYSCALL_HAND_DEF(__NR_fstatat64, SysFstatat64, int, ARG_NUM_4)  // 64位通过文件描述符获取文件状态信息系统调用
SYSCALL_HAND_DEF(__NR_fsync, SysFsync, int, ARG_NUM_1)  // 同步文件数据到磁盘系统调用
SYSCALL_HAND_DEF(__NR__llseek, SysLseek64, off64_t, ARG_NUM_5)  // 64位文件指针定位系统调用 /* current only support 32bit max 4G file */
SYSCALL_HAND_DEF(__NR__newselect, SysSelect, int, ARG_NUM_5)  // I/O多路复用select系统调用
SYSCALL_HAND_DEF(__NR_pselect6, SysPselect6, int, ARG_NUM_6)  // 带信号屏蔽的select系统调用
SYSCALL_HAND_DEF(__NR_readv, SysReadv, ssize_t, ARG_NUM_3)  // 向量读取系统调用
SYSCALL_HAND_DEF(__NR_writev, SysWritev, ssize_t, ARG_NUM_3)  // 向量写入系统调用
SYSCALL_HAND_DEF(__NR_poll, SysPoll, int, ARG_NUM_3)  // I/O多路复用poll系统调用
SYSCALL_HAND_DEF(__NR_ppoll, SysPpoll, int, ARG_NUM_5)  // 带信号屏蔽的poll系统调用
SYSCALL_HAND_DEF(__NR_prctl, SysPrctl, int, ARG_NUM_7)  // 进程控制操作系统调用
SYSCALL_HAND_DEF(__NR_pread64, SysPread64, ssize_t, ARG_NUM_7)  // 64位带偏移量读取系统调用
SYSCALL_HAND_DEF(__NR_pwrite64, SysPwrite64, ssize_t, ARG_NUM_7)  // 64位带偏移量写入系统调用
SYSCALL_HAND_DEF(__NR_epoll_create, SysEpollCreate, int, ARG_NUM_1)  // 创建epoll实例系统调用
SYSCALL_HAND_DEF(__NR_epoll_create1, SysEpollCreate1, int, ARG_NUM_1)  // 带标志创建epoll实例系统调用
SYSCALL_HAND_DEF(__NR_epoll_ctl, SysEpollCtl, int, ARG_NUM_4)  // 控制epoll事件系统调用
SYSCALL_HAND_DEF(__NR_epoll_wait, SysEpollWait, int, ARG_NUM_4)  // 等待epoll事件系统调用
SYSCALL_HAND_DEF(__NR_epoll_pwait, SysEpollPwait, int, ARG_NUM_5)  // 带信号屏蔽等待epoll事件系统调用
SYSCALL_HAND_DEF(__NR_getcwd, SysGetcwd, char *, ARG_NUM_2)  // 获取当前工作目录系统调用
SYSCALL_HAND_DEF(__NR_sendfile, SysSendFile, ssize_t, ARG_NUM_4)  // 文件发送系统调用
SYSCALL_HAND_DEF(__NR_truncate64, SysTruncate64, int, ARG_NUM_7)  // 64位截断文件长度系统调用
SYSCALL_HAND_DEF(__NR_ftruncate64, SysFtruncate64, int, ARG_NUM_7)  // 64位截断文件描述符指向文件长度系统调用
SYSCALL_HAND_DEF(__NR_stat64, SysStat, int, ARG_NUM_2)  // 64位获取文件状态信息系统调用
SYSCALL_HAND_DEF(__NR_lstat64, SysLstat, int, ARG_NUM_2)  // 64位获取符号链接文件状态信息系统调用
SYSCALL_HAND_DEF(__NR_fstat64, SysFstat64, int, ARG_NUM_2)  // 64位获取文件描述符指向文件状态信息系统调用
SYSCALL_HAND_DEF(__NR_fcntl64, SysFcntl64, int, ARG_NUM_3)  // 64位文件控制操作系统调用
SYSCALL_HAND_DEF(__NR_sendfile64, SysSendFile, ssize_t, ARG_NUM_4)  // 64位文件发送系统调用
SYSCALL_HAND_DEF(__NR_preadv, SysPreadv, ssize_t, ARG_NUM_7)  // 带偏移量的向量读取系统调用
SYSCALL_HAND_DEF(__NR_pwritev, SysPwritev, ssize_t, ARG_NUM_7)  // 带偏移量的向量写入系统调用
SYSCALL_HAND_DEF(__NR_fallocate, SysFallocate64, int, ARG_NUM_7)  // 文件空间预分配系统调用
SYSCALL_HAND_DEF(__NR_getdents64, SysGetdents64, int, ARG_NUM_3)  // 获取目录项系统调用

#ifdef LOSCFG_FS_FAT  // 如果启用FAT文件系统配置
SYSCALL_HAND_DEF(__NR_format, SysFormat, int, ARG_NUM_3)  // 格式化文件系统系统调用
#endif

SYSCALL_HAND_DEF(__NR_linkat, SysLinkat, int, ARG_NUM_5)  // 通过文件描述符创建硬链接系统调用
SYSCALL_HAND_DEF(__NR_symlinkat, SysSymlinkat, int, ARG_NUM_3)  // 通过文件描述符创建符号链接系统调用
SYSCALL_HAND_DEF(__NR_readlinkat, SysReadlinkat, ssize_t, ARG_NUM_4)  // 通过文件描述符读取符号链接内容系统调用
SYSCALL_HAND_DEF(__NR_unlinkat, SysUnlinkat, int, ARG_NUM_3)  // 通过文件描述符删除文件系统调用
SYSCALL_HAND_DEF(__NR_renameat, SysRenameat, int, ARG_NUM_4)  // 通过文件描述符重命名文件系统调用
SYSCALL_HAND_DEF(__NR_openat, SysOpenat, int, ARG_NUM_7)  // 通过文件描述符打开文件系统调用
SYSCALL_HAND_DEF(__NR_mkdirat, SysMkdirat, int, ARG_NUM_3)  // 通过文件描述符创建目录系统调用
SYSCALL_HAND_DEF(__NR_statfs64, SysStatfs64, int, ARG_NUM_3)  // 64位获取文件系统状态信息系统调用(重复定义)
#ifdef LOSCFG_DEBUG_VERSION  // 如果启用调试版本配置
SYSCALL_HAND_DEF(__NR_dumpmemory, LOS_DumpMemRegion, void, ARG_NUM_1)  // 内存区域转储系统调用
#endif
#ifdef LOSCFG_KERNEL_PIPE  // 如果启用内核管道配置
SYSCALL_HAND_DEF(__NR_mkfifo, SysMkFifo, int, ARG_NUM_2)  // 创建命名管道系统调用
#endif
SYSCALL_HAND_DEF(__NR_mqclose, SysMqClose, int, ARG_NUM_1)  // 关闭消息队列系统调用
SYSCALL_HAND_DEF(__NR_realpath, SysRealpath, char *, ARG_NUM_2)

#ifdef LOSCFG_SHELL  // 如果启用shell配置
SYSCALL_HAND_DEF(__NR_shellexec, SysShellExec, UINT32, ARG_NUM_2)  // shell执行命令系统调用
#endif
#endif  // LOSCFG_FS_VFS结束

SYSCALL_HAND_DEF(__NR_exit, SysThreadExit, void, ARG_NUM_1)  // 线程退出系统调用
SYSCALL_HAND_DEF(__NR_fork, SysFork, int, ARG_NUM_0)  // 创建进程系统调用
SYSCALL_HAND_DEF(__NR_vfork, SysVfork, int, ARG_NUM_0)  // 创建虚拟进程系统调用
SYSCALL_HAND_DEF(__NR_clone, SysClone, int, ARG_NUM_5)  // 克隆进程系统调用
SYSCALL_HAND_DEF(__NR_unshare, SysUnshare, int, ARG_NUM_1)  // 取消进程共享资源系统调用
SYSCALL_HAND_DEF(__NR_setns, SysSetns, int, ARG_NUM_2)  // 设置命名空间系统调用
SYSCALL_HAND_DEF(__NR_getpid, SysGetPID, unsigned int, ARG_NUM_0)  // 获取进程ID系统调用
SYSCALL_HAND_DEF(__NR_pause, SysPause, int, ARG_NUM_0)  // 进程暂停系统调用

SYSCALL_HAND_DEF(__NR_kill, SysKill, int, ARG_NUM_2)  // 发送信号给进程系统调用

SYSCALL_HAND_DEF(__NR_reboot, SysReboot, int, ARG_NUM_3)  // 系统重启系统调用
SYSCALL_HAND_DEF(__NR_times, SysTimes, clock_t, ARG_NUM_1)  // 获取进程时间系统调用
SYSCALL_HAND_DEF(__NR_brk, SysBrk, void *, ARG_NUM_1)  // 设置程序数据段结束地址系统调用
SYSCALL_HAND_DEF(__NR_setgid, SysSetGroupID, int, ARG_NUM_1)  // 设置组ID系统调用
SYSCALL_HAND_DEF(__NR_getgid, SysGetGroupID, int, ARG_NUM_0)  // 获取组ID系统调用
SYSCALL_HAND_DEF(__NR_setpgid, SysSetProcessGroupID, int, ARG_NUM_2)  // 设置进程组ID系统调用
SYSCALL_HAND_DEF(__NR_getppid, SysGetPPID, unsigned int, ARG_NUM_0)  // 获取父进程ID系统调用
SYSCALL_HAND_DEF(__NR_getpgrp, SysGetProcessGroupID, int, ARG_NUM_1)  // 获取进程组ID系统调用
SYSCALL_HAND_DEF(__NR_munmap, SysMunmap, int, ARG_NUM_2)  // 解除内存映射系统调用
SYSCALL_HAND_DEF(__NR_getpriority, SysGetProcessPriority, int, ARG_NUM_2)  // 获取进程优先级系统调用
SYSCALL_HAND_DEF(__NR_setpriority, SysSetProcessPriority, int, ARG_NUM_3)  // 设置进程优先级系统调用
SYSCALL_HAND_DEF(__NR_setitimer, SysSetiTimer, int, ARG_NUM_3)  // 设置间隔定时器系统调用
SYSCALL_HAND_DEF(__NR_getitimer, SysGetiTimer, int, ARG_NUM_2)  // 获取间隔定时器系统调用
SYSCALL_HAND_DEF(__NR_wait4, SysWait, int, ARG_NUM_4)  // 等待子进程退出系统调用
SYSCALL_HAND_DEF(__NR_waitid, SysWaitid, int, ARG_NUM_5)  // 等待指定子进程退出系统调用
SYSCALL_HAND_DEF(__NR_uname, SysUname, int, ARG_NUM_1)  // 获取系统信息结构体系统调用
#ifdef LOSCFG_UTS_CONTAINER  // 如果启用UTS命名空间容器配置
SYSCALL_HAND_DEF(__NR_sethostname, SysSetHostName, int, ARG_NUM_2)  // 设置主机名系统调用
#endif
SYSCALL_HAND_DEF(__NR_mprotect, SysMprotect, int, ARG_NUM_3)  // 设置内存保护属性系统调用
SYSCALL_HAND_DEF(__NR_getpgid, SysGetProcessGroupID, int, ARG_NUM_1)  // 获取进程组ID系统调用(重复定义)
SYSCALL_HAND_DEF(__NR_sched_setparam, SysSchedSetParam, int, ARG_NUM_3)  // 设置调度参数系统调用
SYSCALL_HAND_DEF(__NR_sched_getparam, SysSchedGetParam, int, ARG_NUM_3)  // 获取调度参数系统调用
SYSCALL_HAND_DEF(__NR_sched_setscheduler, SysSchedSetScheduler, int, ARG_NUM_4)  // 设置调度策略系统调用
SYSCALL_HAND_DEF(__NR_sched_getscheduler, SysSchedGetScheduler, int, ARG_NUM_2)  // 获取调度策略系统调用
SYSCALL_HAND_DEF(__NR_sched_yield, SysSchedYield, void, ARG_NUM_1)  // 放弃CPU调度系统调用
SYSCALL_HAND_DEF(__NR_sched_get_priority_max, SysSchedGetPriorityMax, int, ARG_NUM_1)  // 获取最大优先级系统调用
SYSCALL_HAND_DEF(__NR_sched_get_priority_min, SysSchedGetPriorityMin, int, ARG_NUM_1)  // 获取最小优先级系统调用
SYSCALL_HAND_DEF(__NR_sched_setaffinity, SysSchedSetAffinity, int, ARG_NUM_3)  // 设置CPU亲和性系统调用
SYSCALL_HAND_DEF(__NR_sched_getaffinity, SysSchedGetAffinity, int, ARG_NUM_3)  // 获取CPU亲和性系统调用
SYSCALL_HAND_DEF(__NR_sched_rr_get_interval, SysSchedRRGetInterval, int, ARG_NUM_2)  // 获取RR调度时间片系统调用
SYSCALL_HAND_DEF(__NR_nanosleep, SysNanoSleep, int, ARG_NUM_2)  // 纳秒级睡眠系统调用
SYSCALL_HAND_DEF(__NR_mremap, SysMremap, void *, ARG_NUM_5)  // 重新映射内存系统调用
SYSCALL_HAND_DEF(__NR_umask, SysUmask, mode_t, ARG_NUM_1)  // 设置文件创建掩码系统调用

SYSCALL_HAND_DEF(__NR_rt_sigaction, SysSigAction, int, ARG_NUM_4)  // 实时信号处理函数设置系统调用
SYSCALL_HAND_DEF(__NR_rt_sigprocmask, SysSigprocMask, int, ARG_NUM_4)  // 实时信号屏蔽系统调用
SYSCALL_HAND_DEF(__NR_rt_sigpending, SysSigPending, int, ARG_NUM_1)  // 获取未决实时信号系统调用
SYSCALL_HAND_DEF(__NR_rt_sigtimedwait, SysSigTimedWait, int, ARG_NUM_4)  // 带超时等待实时信号系统调用
SYSCALL_HAND_DEF(__NR_rt_sigsuspend, SysSigSuspend, int, ARG_NUM_1)  // 挂起等待实时信号系统调用

SYSCALL_HAND_DEF(__NR_fchownat, SysFchownat, int, ARG_NUM_5)  // 通过文件描述符修改文件所有者系统调用
SYSCALL_HAND_DEF(__NR_fchown32, SysFchown, int, ARG_NUM_3)  // 32位修改文件描述符指向文件所有者系统调用
SYSCALL_HAND_DEF(__NR_chown, SysChown, int, ARG_NUM_3)  // 修改文件所有者系统调用
SYSCALL_HAND_DEF(__NR_chown32, SysChown, int, ARG_NUM_3)  // 32位修改文件所有者系统调用(重复定义)
#ifdef LOSCFG_SECURITY_CAPABILITY  // 如果启用安全能力配置
SYSCALL_HAND_DEF(__NR_ohoscapget, SysCapGet, UINT32, ARG_NUM_2)  // 获取能力系统调用
SYSCALL_HAND_DEF(__NR_ohoscapset, SysCapSet, UINT32, ARG_NUM_1)  // 设置能力系统调用
#endif

SYSCALL_HAND_DEF(__NR_mmap2, SysMmap, void*, ARG_NUM_6)  // 内存映射系统调用
SYSCALL_HAND_DEF(__NR_getuid32, SysGetUserID, int, ARG_NUM_0)  // 32位获取用户ID系统调用
SYSCALL_HAND_DEF(__NR_getgid32, SysGetGroupID, unsigned int, ARG_NUM_0)  // 32位获取组ID系统调用
SYSCALL_HAND_DEF(__NR_geteuid32, SysGetEffUserID, int, ARG_NUM_0)  // 32位获取有效用户ID系统调用
SYSCALL_HAND_DEF(__NR_getegid32, SysGetEffGID, unsigned int, ARG_NUM_0)  // 32位获取有效组ID系统调用
SYSCALL_HAND_DEF(__NR_getresuid32, SysGetRealEffSaveUserID, int, ARG_NUM_3)  // 32位获取真实/有效/保存用户ID系统调用
SYSCALL_HAND_DEF(__NR_getresgid32, SysGetRealEffSaveGroupID, int, ARG_NUM_3)  // 32位获取真实/有效/保存组ID系统调用
SYSCALL_HAND_DEF(__NR_setresuid32, SysSetRealEffSaveUserID, int, ARG_NUM_3)  // 32位设置真实/有效/保存用户ID系统调用
SYSCALL_HAND_DEF(__NR_setresgid32, SysSetRealEffSaveGroupID, int, ARG_NUM_3)  // 32位设置真实/有效/保存组ID系统调用
SYSCALL_HAND_DEF(__NR_setreuid32, SysSetRealEffUserID, int, ARG_NUM_2)  // 32位设置真实和有效用户ID系统调用
SYSCALL_HAND_DEF(__NR_setregid32, SysSetRealEffGroupID, int, ARG_NUM_2)  // 32位设置真实和有效组ID系统调用
SYSCALL_HAND_DEF(__NR_setgroups32, SysSetGroups, int, ARG_NUM_2)  // 32位设置附加组ID列表系统调用
SYSCALL_HAND_DEF(__NR_getgroups32, SysGetGroups, int, ARG_NUM_2)  // 32位获取附加组ID列表系统调用
SYSCALL_HAND_DEF(__NR_setuid32, SysSetUserID, int, ARG_NUM_1)  // 32位设置用户ID系统调用
SYSCALL_HAND_DEF(__NR_setgid32, SysSetGroupID, int, ARG_NUM_1)  // 32位设置组ID系统调用(重复定义)

SYSCALL_HAND_DEF(__NR_gettid, SysGetTid, unsigned int, ARG_NUM_0)  // 获取线程ID系统调用

SYSCALL_HAND_DEF(__NR_tkill, SysPthreadKill, int, ARG_NUM_2)  // 向线程发送信号系统调用

SYSCALL_HAND_DEF(__NR_futex, SysFutex, int, ARG_NUM_4)  // 快速用户空间互斥锁系统调用
SYSCALL_HAND_DEF(__NR_exit_group, SysUserExitGroup, void, ARG_NUM_1)  // 退出进程组系统调用
SYSCALL_HAND_DEF(__NR_set_thread_area, SysSetThreadArea, int, ARG_NUM_1)  // 设置线程局部存储系统调用
SYSCALL_HAND_DEF(__NR_get_thread_area, SysGetThreadArea, char *, ARG_NUM_0)  // 获取线程局部存储系统调用
SYSCALL_HAND_DEF(__NR_timer_create, SysTimerCreate, int, ARG_NUM_3)  // 创建定时器系统调用
SYSCALL_HAND_DEF(__NR_timer_settime32, SysTimerSettime, int, ARG_NUM_4)  // 32位设置定时器时间系统调用
SYSCALL_HAND_DEF(__NR_timer_gettime32, SysTimerGettime, int, ARG_NUM_2)  // 32位获取定时器时间系统调用
SYSCALL_HAND_DEF(__NR_timer_getoverrun, SysTimerGetoverrun, int, ARG_NUM_1)  // 获取定时器溢出次数系统调用
SYSCALL_HAND_DEF(__NR_timer_delete, SysTimerDelete, int, ARG_NUM_1)  // 删除定时器系统调用
SYSCALL_HAND_DEF(__NR_clock_settime32, SysClockSettime, int, ARG_NUM_2)  // 32位设置时钟时间系统调用
SYSCALL_HAND_DEF(__NR_clock_gettime32, SysClockGettime, int, ARG_NUM_2)  // 32位获取时钟时间系统调用
SYSCALL_HAND_DEF(__NR_clock_getres_time32, SysClockGetres, int, ARG_NUM_2)  // 32位获取时钟分辨率系统调用
SYSCALL_HAND_DEF(__NR_clock_nanosleep_time32, SysClockNanoSleep, int, ARG_NUM_4)  // 32位按指定时钟纳秒睡眠系统调用
SYSCALL_HAND_DEF(__NR_mq_open, SysMqOpen, mqd_t, ARG_NUM_4)  // 打开消息队列系统调用
SYSCALL_HAND_DEF(__NR_mq_unlink, SysMqUnlink, int, ARG_NUM_1)  // 删除消息队列链接系统调用
SYSCALL_HAND_DEF(__NR_mq_timedsend, SysMqTimedSend, int, ARG_NUM_5)  // 带超时发送消息系统调用
SYSCALL_HAND_DEF(__NR_mq_timedreceive, SysMqTimedReceive, ssize_t, ARG_NUM_5)  // 带超时接收消息系统调用
SYSCALL_HAND_DEF(__NR_mq_notify, SysMqNotify, int, ARG_NUM_2)  // 消息队列通知系统调用
SYSCALL_HAND_DEF(__NR_mq_getsetattr, SysMqGetSetAttr, int, ARG_NUM_3)  // 获取/设置消息队列属性系统调用

#ifdef LOSCFG_NET_LWIP_SACK  // 如果启用LWIP SACK网络配置
SYSCALL_HAND_DEF(__NR_socket, SysSocket, int, ARG_NUM_3)  // 创建套接字系统调用
SYSCALL_HAND_DEF(__NR_bind, SysBind, int, ARG_NUM_3)  // 绑定套接字系统调用
SYSCALL_HAND_DEF(__NR_connect, SysConnect, int, ARG_NUM_3)  // 连接套接字系统调用
SYSCALL_HAND_DEF(__NR_listen, SysListen, int, ARG_NUM_2)  // 监听套接字系统调用
SYSCALL_HAND_DEF(__NR_accept, SysAccept, int, ARG_NUM_3)  // 接受连接系统调用
SYSCALL_HAND_DEF(__NR_getsockname, SysGetSockName, int, ARG_NUM_3)  // 获取套接字名称系统调用
SYSCALL_HAND_DEF(__NR_getpeername, SysGetPeerName, int, ARG_NUM_3)  // 获取对等方名称系统调用
SYSCALL_HAND_DEF(__NR_send, SysSend, ssize_t, ARG_NUM_4)  // 发送数据系统调用
SYSCALL_HAND_DEF(__NR_sendto, SysSendTo, ssize_t, ARG_NUM_6)  // 发送数据到指定地址系统调用
SYSCALL_HAND_DEF(__NR_recv, SysRecv, ssize_t, ARG_NUM_4)  // 接收数据系统调用
SYSCALL_HAND_DEF(__NR_recvfrom, SysRecvFrom, ssize_t, ARG_NUM_6)  // 从指定地址接收数据系统调用
SYSCALL_HAND_DEF(__NR_shutdown, SysShutdown, int, ARG_NUM_2)  // 关闭套接字连接系统调用
SYSCALL_HAND_DEF(__NR_setsockopt, SysSetSockOpt, int, ARG_NUM_5)  // 设置套接字选项系统调用
SYSCALL_HAND_DEF(__NR_getsockopt, SysGetSockOpt, int, ARG_NUM_5)  // 获取套接字选项系统调用
SYSCALL_HAND_DEF(__NR_sendmsg, SysSendMsg, ssize_t, ARG_NUM_3)  // 发送消息系统调用
SYSCALL_HAND_DEF(__NR_recvmsg, SysRecvMsg, ssize_t, ARG_NUM_3)  // 接收消息系统调用
#endif

#ifdef LOSCFG_KERNEL_SHM  // 如果启用内核共享内存配置
SYSCALL_HAND_DEF(__NR_shmat, SysShmAt, void *, ARG_NUM_3)  // 附加共享内存系统调用
SYSCALL_HAND_DEF(__NR_shmdt, SysShmDt, int, ARG_NUM_1)  // 分离共享内存系统调用
SYSCALL_HAND_DEF(__NR_shmget, SysShmGet, int, ARG_NUM_3)  // 获取共享内存系统调用
SYSCALL_HAND_DEF(__NR_shmctl, SysShmCtl, int, ARG_NUM_3)  // 控制共享内存系统调用
#endif

SYSCALL_HAND_DEF(__NR_statx, SysStatx, int, ARG_NUM_5)  // 扩展文件状态获取系统调用

#ifdef LOSCFG_CHROOT  // 如果启用chroot配置
SYSCALL_HAND_DEF(__NR_chroot, SysChroot, int, ARG_NUM_1)  // 改变根目录系统调用
#endif

/* LiteOS customized syscalls, not compatible with ARM EABI */  // LiteOS自定义系统调用，不兼容ARM EABI
SYSCALL_HAND_DEF(__NR_pthread_set_detach, SysUserThreadSetDetach, int, ARG_NUM_1)  // 设置线程分离状态系统调用
SYSCALL_HAND_DEF(__NR_pthread_join, SysThreadJoin, int, ARG_NUM_1)  // 连接线程系统调用
SYSCALL_HAND_DEF(__NR_pthread_deatch, SysUserThreadDetach, int, ARG_NUM_1)  // 分离线程系统调用(拼写错误，应为detach)
SYSCALL_HAND_DEF(__NR_create_user_thread, SysCreateUserThread, unsigned int, ARG_NUM_3)  // 创建用户线程系统调用
SYSCALL_HAND_DEF(__NR_getrusage, SysGetrusage, int, ARG_NUM_2)  // 获取资源使用情况系统调用
SYSCALL_HAND_DEF(__NR_sysconf, SysSysconf, long, ARG_NUM_1)  // 获取系统配置参数系统调用
SYSCALL_HAND_DEF(__NR_ugetrlimit, SysUgetrlimit, int, ARG_NUM_2)  // 获取资源限制系统调用
SYSCALL_HAND_DEF(__NR_setrlimit, SysSetrlimit, int, ARG_NUM_2)  // 设置资源限制系统调用