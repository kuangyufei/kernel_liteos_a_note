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
/*!
 * @file    trace.c
 * @brief 用户态
 * @link  @endlink
   @verbatim
    新增trace字符设备，位于"/dev/trace",通过对设备节点的read\write\ioctl，实现用户态trace的读写和控制：
	read: 用户态读取Trace记录数据
	write: 用户态事件写入
	ioctl: 用户态Trace控制操作，包括
		#define TRACE_IOC_MAGIC     'T'
		#define TRACE_START         _IO(TRACE_IOC_MAGIC, 1)
		#define TRACE_STOP          _IO(TRACE_IOC_MAGIC, 2)
		#define TRACE_RESET         _IO(TRACE_IOC_MAGIC, 3)
		#define TRACE_DUMP          _IO(TRACE_IOC_MAGIC, 4)
		#define TRACE_SET_MASK      _IO(TRACE_IOC_MAGIC, 5)
	分别对应Trace启动(LOS_TraceStart)、停止(LOS_TraceStop)、清除记录(LOS_TraceReset)、
	dump记录(LOS_TraceRecordDump)、设置事件过滤掩码(LOS_TraceEventMaskSet)	

	用户态开发流程
		通过在menuconfig配置"Driver->Enable TRACE DRIVER",开启Trace驱动。该配置仅在内核Enable Trace Feature后，
		才可在Driver的选项中可见。
		打开“/dev/trace”字符文件，进行读写和IOCTL操作；
		系统提供用户态的trace命令，该命令位于/bin目录下，cd bin 后可执行如下命令：
		./trace reset 清除Trace记录
		./trace mask num 设置Trace事件过滤掩码
		./trace start 启动Trace
		./trace stop 停止Trace
		./trace dump 0/1 格式化输出Trace记录
		./trace read nBytes 读取Trace记录
		./trace write type id params... 写用户态事件 如：./trace write 0x1 0x1001 0x2 0x3 则写入一条用户态事件，
		其事件类型为0x1, id为0x1001，参数有2个，分别是0x2和0x3.
		用户态命令行的典型使用方法如下：
		./trace start
		./trace write 0x1 0x1001 0x2 0x3
		./trace stop
		./trace dump 0
   @endverbatim
 * @version 
 * @author  weharmonyos.com | 鸿蒙研究站 | 每天死磕一点点
 * @date    2025-07-10
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdint.h>

/**
 * @file trace.c
 * @brief 系统跟踪工具实现，提供事件跟踪、控制和数据读写功能
 */

/**
 * @brief Trace模块IO控制命令魔数
 * @note 用于标识Trace设备的IO控制命令，采用大写字母'T'的ASCII值
 */
#define TRACE_IOC_MAGIC     'T'  // ASCII值：84，表示Trace设备的魔数

/**
 * @brief 启动跟踪命令
 * @note 通过ioctl调用启动事件跟踪功能
 */
#define TRACE_START         _IO(TRACE_IOC_MAGIC, 1)  // IO控制命令：启动跟踪

/**
 * @brief 停止跟踪命令
 * @note 通过ioctl调用停止事件跟踪功能
 */
#define TRACE_STOP          _IO(TRACE_IOC_MAGIC, 2)  // IO控制命令：停止跟踪

/**
 * @brief 重置跟踪命令
 * @note 通过ioctl调用清除跟踪记录缓冲区
 */
#define TRACE_RESET         _IO(TRACE_IOC_MAGIC, 3)  // IO控制命令：重置跟踪缓冲区

/**
 * @brief 转储跟踪数据命令
 * @note 通过ioctl调用格式化输出跟踪数据
 */
#define TRACE_DUMP          _IO(TRACE_IOC_MAGIC, 4)  // IO控制命令：转储跟踪数据

/**
 * @brief 设置跟踪掩码命令
 * @note 通过ioctl调用设置跟踪事件过滤掩码
 */
#define TRACE_SET_MASK      _IO(TRACE_IOC_MAGIC, 5)  // IO控制命令：设置跟踪掩码

/**
 * @brief 用户事件的最大参数数量
 * @note 定义用户自定义事件可携带的最大参数个数
 */
#define TRACE_USR_MAX_PARAMS 3  // 十进制值：3，表示最多支持3个参数

/**
 * @brief 用户事件信息结构体
 * @note 用于向跟踪系统提交用户自定义事件
 */
typedef struct {
    unsigned int eventType;       // 事件类型，用于标识不同的用户事件
    uintptr_t identity;           // 事件标识，通常表示事件发起者ID
    uintptr_t params[TRACE_USR_MAX_PARAMS];  // 事件参数数组，最多包含3个参数
} UsrEventInfo;

/**
 * @brief 打印trace命令的使用帮助信息
 * @note 当用户输入无效命令或需要帮助时调用此函数
 */
static void TraceUsage(void)
{
    printf("\nUsage: ./trace [start] Start to trace events.\n");          // 启动事件跟踪
    printf("\nUsage: ./trace [stop] Stop trace.\n");                    // 停止事件跟踪
    printf("\nUsage: ./trace [reset] Clear the trace record buffer.\n"); // 清除跟踪记录缓冲区
    printf("\nUsage: ./trace [dump 0/1] Format printf trace data,"
                "0/1 stands for whether to send data to studio for analysis.\n"); // 格式化输出跟踪数据，0/1表示是否发送到分析工具
    printf("\nUsage: ./trace [mask num] Set trace filter event mask.\n"); // 设置跟踪过滤事件掩码
    printf("\nUsage: ./trace [read nBytes] Read nBytes raw data from trace buffer.\n"); // 从跟踪缓冲区读取指定字节数的原始数据
    printf("\nUsage: ./trace [write type id params..] Write a user event, no more than 3 parameters.\n"); // 写入用户事件，最多3个参数
}

/**
 * @brief 从跟踪缓冲区读取原始数据并打印
 * @param fd 跟踪设备文件描述符
 * @param size 要读取的字节数
 * @note 读取的数据以十六进制格式打印，用于调试和原始数据分析
 */
static void TraceRead(int fd, size_t size)
{
    ssize_t i;          // 循环索引变量
    ssize_t len;        // 实际读取的字节数
    if (size == 0) {    // 检查读取大小是否为0
        return;         // 无效大小，直接返回
    }

    // 分配缓冲区用于存储读取的数据
    char *buffer = (char *)malloc(size);
    if (buffer == NULL) {  // 检查内存分配是否成功
        printf("Read buffer malloc failed.\n");  // 打印内存分配失败信息
        return;                                 // 分配失败，返回
    }

    // 从跟踪设备读取数据
    len = read(fd, buffer, size);
    // 循环打印每个字节的十六进制格式
    for (i = 0; i < len; i++) {
        printf("%02x ", buffer[i] & 0xFF);  // 确保无符号输出，格式化为两位十六进制数
    }
    printf("\n");  // 打印换行符，结束输出
    free(buffer);    // 释放分配的缓冲区内存
}

/**
 * @brief 向跟踪缓冲区写入用户自定义事件
 * @param fd 跟踪设备文件描述符
 * @param argc 命令行参数数量
 * @param argv 命令行参数数组
 * @note 事件参数通过命令行传入，最多支持3个参数
 */
static void TraceWrite(int fd, int argc, char **argv)
{
    int i;                      // 循环索引变量
    UsrEventInfo info = {0};    // 用户事件信息结构体，初始化为0
    // 解析事件类型参数（第3个命令行参数）
    info.eventType = strtoul(argv[2], NULL, 0); /* 2, argv number  */
    // 解析事件标识参数（第4个命令行参数）
    info.identity = strtoul(argv[3], NULL, 0); /* 3, argv number  */
    /* 4, argc -4 means user argv that does not contain argv[0]~argv[3] */
    // 计算参数数量，最多不超过TRACE_USR_MAX_PARAMS(3个)
    int paramNum = (argc - 4) > TRACE_USR_MAX_PARAMS ? TRACE_USR_MAX_PARAMS : (argc - 4);

    // 循环解析每个事件参数
    for (i = 0; i < paramNum; i++) {
        /* 4, argc -4 means user argv that does not contain argv[0]~argv[3] */
        info.params[i] = strtoul(argv[4 + i], NULL, 0);  // 解析第i个参数
    }
    // 将用户事件写入跟踪设备，忽略返回值
    (void)write(fd, &info, sizeof(UsrEventInfo));
}

/*!
 * @brief main	用户态示例代码功能如下:
   @verbatim
   	1. 打开trace字符设备。
	2. 设置事件掩码。
	3. 启动trace。
	4. 写trace事件。
	5. 停止trace。
	6. 格式化输出trace数据。
	7. 输出结果
		*******TraceInfo begin*******
		clockFreq = 50000000
		CurEvtIndex = 2
		Index   Time(cycles)      EventType      CurTask   Identity      params
		0       0x366d5e88        0xfffffff1     0x1       0x1           0x1          0x2       0x3
		1       0x366d74ae        0xfffffff2     0x1       0x2           0x1          0x2       0x3
		*******TraceInfo end*******
		示例代码中调用了2次write，对应产生2条Trace记录； 用户层事件的EventType高28位固定均为1（即0xfffffff0），
		仅低4位表示具体的事件类型。
   @endverbatim
 * @param argc	
 * @param argv	
 * @return	
 *
 * @see
 */
int main(int argc, char **argv)
{
    int fd = open("/dev/trace", O_RDWR);
    if (fd == -1) {
        printf("Trace open failed.\n");
        exit(EXIT_FAILURE);
    }

    if (argc == 1) {
        TraceUsage();
    } else if (argc == 2 && strcmp(argv[1], "start") == 0) { /* 2, argv num, no special meaning */
        ioctl(fd, TRACE_START, NULL);
    } else if (argc == 2 && strcmp(argv[1], "stop") == 0) { /* 2, argv num, no special meaning */
        ioctl(fd, TRACE_STOP, NULL);
    } else if (argc == 2 && strcmp(argv[1], "reset") == 0) { /* 2, argv num, no special meaning */
        ioctl(fd, TRACE_RESET, NULL);
    } else if (argc == 3 && strcmp(argv[1], "mask") == 0) { /* 3, argv num, no special meaning */
        size_t mask = strtoul(argv[2], NULL, 0);
        ioctl(fd, TRACE_SET_MASK, mask);
    } else if (argc == 3 && strcmp(argv[1], "dump") == 0) { /* 3, argv num, no special meaning */
        size_t flag = strtoul(argv[2], NULL, 0);
        ioctl(fd, TRACE_DUMP, flag);
    } else if (argc == 3 && strcmp(argv[1], "read") == 0) { /* 3, argv num, no special meaning */
        size_t size = strtoul(argv[2], NULL, 0);
        TraceRead(fd, size);
    } else if (argc >= 4 && strcmp(argv[1], "write") == 0) { /* 4, argv num, no special meaning */
        TraceWrite(fd, argc, argv);
    } else {
        printf("Unsupported trace command.\n");
        TraceUsage();
    }

    close(fd);
    return 0;
}
