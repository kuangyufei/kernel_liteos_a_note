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

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/wait.h>

#ifdef LOSCFG_QUICK_START
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#define QUICKSTART_IOC_MAGIC    'T'
#define QUICKSTART_INITSTEP2    _IO(QUICKSTART_IOC_MAGIC, 0)
#define WAIT_FOR_SAMPLE         300000  // wait 300ms for sample
#endif
/**
 * @brief 系统初始化主函数
 * @param argc 参数数量
 * @param argv 参数数组
 * @return int 执行结果，0表示成功
 */
int main(int argc, char * const *argv)
{
    (void)argv;  // 未使用的参数
    int ret;     // 返回值变量
    pid_t gid;   // 进程组ID
    const char *shellPath = "/bin/mksh";  // shell程序路径

#ifdef LOSCFG_QUICK_START  // 快速启动配置
    const char *samplePath = "/dev/shm/sample_quickstart";  // 快速启动示例程序路径

    ret = fork();  // 创建子进程运行快速启动示例
    if (ret < 0) {  // 检查fork是否失败
        printf("Failed to fork for sample_quickstart\n");  // 输出fork失败信息
    } else if (ret == 0) {  // 子进程分支
        (void)execve(samplePath, NULL, NULL);  // 执行快速启动示例程序
        exit(0);  // 退出子进程
    }

    usleep(WAIT_FOR_SAMPLE);  // 等待示例程序执行

    int fd = open("/dev/quickstart", O_RDONLY);  // 打开快速启动设备
    if (fd != -1) {  // 检查设备是否打开成功
        ioctl(fd, QUICKSTART_INITSTEP2);  // 执行快速启动第二步初始化
        close(fd);  // 关闭设备文件描述符
    }
#endif
    ret = fork();  // 创建子进程运行shell
    if (ret < 0) {  // 检查fork是否失败
        printf("Failed to fork for shell\n");  // 输出fork失败信息
    } else if (ret == 0) {  // 子进程分支
        gid = getpgrp();  // 获取进程组ID
        if (gid < 0) {  // 检查获取进程组ID是否失败
            printf("get group id failed, pgrpid %d, errno %d\n", gid, errno);  // 输出错误信息
            exit(0);  // 退出子进程
        }
        ret = tcsetpgrp(STDIN_FILENO, gid);  // 设置控制终端进程组
        if (ret != 0) {  // 检查设置是否失败
            printf("tcsetpgrp failed, errno %d\n", errno);  // 输出错误信息
            exit(0);  // 退出子进程
        }
        (void)execve(shellPath, NULL, NULL);  // 执行shell程序
        exit(0);  // 退出子进程
    }

    while (1) {  // 无限循环等待子进程退出
        ret = waitpid(-1, 0, WNOHANG);  // 非阻塞等待任意子进程
        if (ret == 0) {  // 没有子进程退出
            sleep(1);  // 休眠1秒后重试
        }
    };
}
