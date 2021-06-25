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
/*
capabilities 翻译为 权限(集)
Capabilities 机制是在 Linux 内核 2.2 之后引入的，原理很简单，就是将之前与超级用户 root（UID=0）
关联的特权细分为不同的功能组，Capabilites 作为线程（Linux 并不真正区分进程和线程）的属性存在，
每个功能组都可以独立启用和禁用。其本质上就是将内核调用分门别类，具有相似功能的内核调用被分到同一组中。
这样一来，权限检查的过程就变成了：在执行特权操作时，如果线程的有效身份不是 root，
就去检查其是否具有该特权操作所对应的 capabilities，并以此为依据，决定是否可以执行特权操作。
capability 作用在进程上,让用户态进程具有内核态进程的某些权限.
https://blog.csdn.net/alex_yangchuansheng/article/details/102796001
*/
#ifndef CAPABILITY_TYPE_H
#define CAPABILITY_TYPE_H

// posix capabilities
#define CAP_CHOWN                       0	//修改文件所有者的权限
#define CAP_DAC_EXECUTE                 1	//具有执行权限
#define CAP_DAC_WRITE                   2	//具有写权限
#define CAP_DAC_READ_SEARCH             3	//忽略文件读及目录搜索的 DAC 访问限制
#define CAP_FOWNER                      4	//忽略文件属主 ID 必须和进程用户 ID 相匹配的限制
#define CAP_KILL                        5	//允许向其他进程发生信号
#define CAP_SETGID                      6	//允许设置其他进程组ID
#define CAP_SETUID                      7	//允许设置其他进程用户ID

// socket capabilities
#define CAP_NET_BIND_SERVICE            8	//允许绑定端口权限
#define CAP_NET_BROADCAST               9	//允许广播
#define CAP_NET_ADMIN                   10
#define CAP_NET_RAW                     11

// fs capabilities
#define CAP_FS_MOUNT                    12	//允许挂载
#define CAP_FS_FORMAT                   13	//允许格式化

// process capabilities
#define CAP_SCHED_SETPRIORITY           14	//允许设置调度优先级

// time capabilities
#define CAP_SET_TIMEOFDAY               15	
#define CAP_CLOCK_SETTIME               16

// process capabilities
#define CAP_CAPSET                      17	//允许改变进程自身的权限集

// reboot capability
#define CAP_REBOOT                      18	//允许重新启动系统
// self deined privileged syscalls
#define CAP_SHELL_EXEC                  19	//自我定义的特权系统调用
#endif