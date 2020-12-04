/*
 * Copyright (c) 2013-2019, Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020, Huawei Device Co., Ltd. All rights reserved.
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

#ifndef CAPABILITY_TYPE_H
#define CAPABILITY_TYPE_H
	
// posix capabilities	//posix 接口能力范围
#define CAP_CHOWN                       0	//修改拥有者
#define CAP_DAC_EXECUTE                 1	//
#define CAP_DAC_WRITE                   2
#define CAP_DAC_READ_SEARCH             3
#define CAP_FOWNER                      4
#define CAP_KILL                        5	//kill
#define CAP_SETGID                      6	//设置用户组ID
#define CAP_SETUID                      7	//设置用户ID

// socket capabilities	//网络能力
#define CAP_NET_BIND_SERVICE            8	//绑定端口
#define CAP_NET_BROADCAST               9	//网络广播
#define CAP_NET_ADMIN                   10	//网络管理
#define CAP_NET_RAW                     11	//网络读写访问

// fs capabilities	//文件系统能力
#define CAP_FS_MOUNT                    12	//挂载
#define CAP_FS_FORMAT                   13	//格式化

// process capabilities	//进程调度能力，
#define CAP_SCHED_SETPRIORITY           14 //设置调度优先级

// time capabilities	//时间能力
#define CAP_SET_TIMEOFDAY               15	//重置系统时间
#define CAP_CLOCK_SETTIME               16	//设置时钟

// process capabilities	//进程能力
#define CAP_CAPSET                      17	//设置进程能力的能力

// reboot capability	//重新启动功能
#define CAP_REBOOT                      18	//重启系统
// self deined privileged syscalls	//自定义特权系统调用
#define CAP_SHELL_EXEC                  19	//自定义 shell 命令 
#endif