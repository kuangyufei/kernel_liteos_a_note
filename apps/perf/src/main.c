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
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include "perf.h"
#include "perf_list.h"
#include "perf_stat.h"
#include "perf_record.h"

/**
 * @brief perf工具主函数，解析命令行参数并执行相应操作
 * @param[in] argc - 命令行参数数量
 * @param[in] argv - 命令行参数数组
 * @return 0表示成功，非0表示失败
 * @details 负责打开perf设备文件，根据用户输入的命令调用相应功能模块，并在结束时关闭设备
 */
int main(int argc, char **argv)
{
#define TWO_ARGS    2   // 命令行参数数量为2（含程序名）
#define THREE_ARGS  3   // 命令行参数数量为3（含程序名）
    // 打开perf设备文件，可读可写模式
    int fd = open("/dev/perf", O_RDWR);
    if (fd == -1) {                      // 设备打开失败检查
        printf("Perf open failed.\n");  // 输出错误信息
        exit(EXIT_FAILURE);              // 终止程序并返回失败状态
    }

    // 根据命令行参数数量和内容执行相应操作
    if (argc == 1) {                     // 无命令参数时
        PerfUsage();                     // 打印使用帮助
    } else if ((argc == TWO_ARGS) && strcmp(argv[1], "start") == 0) {  // 参数为"start"（无ID）
        PerfStart(fd, 0);                // 启动性能监控，默认区域ID=0
    } else if ((argc == THREE_ARGS) && strcmp(argv[1], "start") == 0) { // 参数为"start [id]"
        size_t id = strtoul(argv[THREE_ARGS - 1], NULL, 0);  // 将字符串ID转换为无符号整数
        PerfStart(fd, id);               // 启动指定区域ID的性能监控
    } else if ((argc == TWO_ARGS) && strcmp(argv[1], "stop") == 0) {   // 参数为"stop"
        PerfStop(fd);                    // 停止性能监控
    } else if ((argc == THREE_ARGS) && strcmp(argv[1], "read") == 0) {  // 参数为"read <nBytes>"
        size_t size = strtoul(argv[THREE_ARGS - 1], NULL, 0);  // 将字符串字节数转换为无符号整数
        if (size == 0) {                 // 检查请求读取的字节数是否为0
            goto EXIT;                   // 跳转到EXIT标签，避免无效读取
        }

        char *buf = (char *)malloc(size); // 分配指定大小的缓冲区
        if (buf != NULL) {               // 内存分配成功检查
            int len = PerfRead(fd, buf, size);  // 读取性能数据到缓冲区
            PerfPrintBuffer(buf, len);   // 打印读取到的原始数据
            free(buf);                   // 释放缓冲区内存
            buf = NULL;                  // 避免野指针
        }
    } else if ((argc == TWO_ARGS) && strcmp(argv[1], "list") == 0) {   // 参数为"list"
        PerfList();                      // 列出所有可用的性能事件
    } else if ((argc >= THREE_ARGS) && strcmp(argv[1], "stat") == 0) { // 参数为"stat [option] <command>"
        PerfStat(fd, argc, argv);        // 执行性能统计命令
    } else if ((argc >= THREE_ARGS) && strcmp(argv[1], "record") == 0) { // 参数为"record [option] <command>"
        PerfRecord(fd, argc, argv);      // 执行性能记录命令
    } else {                             // 不支持的命令
        printf("Unsupported perf command.\n");  // 输出不支持命令提示
        PerfUsage();                     // 打印使用帮助
    }

EXIT:                                    // 统一资源释放标签
    close(fd);                           // 关闭perf设备文件描述符
    return 0;                            // 返回成功状态
}