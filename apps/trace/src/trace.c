/*!
 * @file    trace.c
 * @brief 用户态
 * @link section3.2 http://weharmonyos.com/openharmony/zh-cn/device-dev/kernel/kernel-small-debug-trace.html#section3.2 @endlink
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
 * @author  weharmonyos.com
 * @date    2021-11-22
 */
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
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdint.h>

//分别对应Trace启动(LOS_TraceStart)、停止(LOS_TraceStop)、清除记录(LOS_TraceReset)、
//dump记录(LOS_TraceRecordDump)、设置事件过滤掩码(LOS_TraceEventMaskSet)
#define TRACE_IOC_MAGIC     'T'
#define TRACE_START         _IO(TRACE_IOC_MAGIC, 1)
#define TRACE_STOP          _IO(TRACE_IOC_MAGIC, 2)
#define TRACE_RESET         _IO(TRACE_IOC_MAGIC, 3)
#define TRACE_DUMP          _IO(TRACE_IOC_MAGIC, 4)
#define TRACE_SET_MASK      _IO(TRACE_IOC_MAGIC, 5)

#define TRACE_USR_MAX_PARAMS 3
typedef struct {
    unsigned int eventType;
    uintptr_t identity;
    uintptr_t params[TRACE_USR_MAX_PARAMS];
} UsrEventInfo;

static void TraceUsage(void)
{
    printf("\nUsage: ./trace [start] Start to trace events.\n");
    printf("\nUsage: ./trace [stop] Stop trace.\n");
    printf("\nUsage: ./trace [reset] Clear the trace record buffer.\n");
    printf("\nUsage: ./trace [dump 0/1] Format printf trace data,"
                "0/1 stands for whether to send data to studio for analysis.\n");
    printf("\nUsage: ./trace [mask num] Set trace filter event mask.\n");
    printf("\nUsage: ./trace [read nBytes] Read nBytes raw data from trace buffer.\n");
    printf("\nUsage: ./trace [write type id params..] Write a user event, no more than 3 parameters.\n");
}

static void TraceRead(int fd, size_t size)
{
    ssize_t i;
    ssize_t len;
    char *buffer = (char *)malloc(size);
    if (buffer == NULL) {
        printf("Read buffer malloc failed.\n");
        return;
    }

    len = read(fd, buffer, size);
    for (i = 0; i < len; i++) {
        printf("%02x ", buffer[i] & 0xFF);
    }
    printf("\n");
    free(buffer);
}

static void TraceWrite(int fd, int argc, char **argv)
{
    int i;
    UsrEventInfo info = {0};
    info.eventType = strtoul(argv[2], NULL, 0);
    info.identity = strtoul(argv[3], NULL, 0);
    int paramNum = (argc - 4) > TRACE_USR_MAX_PARAMS ? TRACE_USR_MAX_PARAMS : (argc - 4);
    
    for (i = 0; i < paramNum; i++) {
        info.params[i] = strtoul(argv[4 + i], NULL, 0);
    }
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
    } else if (argc == 2 && strcmp(argv[1], "start") == 0) {
        ioctl(fd, TRACE_START, NULL);
    } else if (argc == 2 && strcmp(argv[1], "stop") == 0) {
        ioctl(fd, TRACE_STOP, NULL);
    } else if (argc == 2 && strcmp(argv[1], "reset") == 0) {
        ioctl(fd, TRACE_RESET, NULL);
    } else if (argc == 3 && strcmp(argv[1], "mask") == 0) {
        size_t mask = strtoul(argv[2], NULL, 0);
        ioctl(fd, TRACE_SET_MASK, mask);
    } else if (argc == 3 && strcmp(argv[1], "dump") == 0) {
        size_t flag = strtoul(argv[2], NULL, 0);
        ioctl(fd, TRACE_DUMP, flag);
    } else if (argc == 3 && strcmp(argv[1], "read") == 0) {
        size_t size = strtoul(argv[2], NULL, 0);
        TraceRead(fd, size);
    } else if (argc >= 4 && strcmp(argv[1], "write") == 0) {
        TraceWrite(fd, argc, argv);
    } else {
        printf("Unsupported trace command.\n");
        TraceUsage();
    }

    close(fd);
    return 0;
}
