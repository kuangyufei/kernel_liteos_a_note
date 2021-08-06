/*
 * Copyright (c) 2013-2019 Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020-2021 Huawei Device Co., Ltd. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this list of
 * conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list
 * of conditions and the following disclaimer in the documentation and/or other materials
 * provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors may be used
 * to endorse or promote products derived from this software without specific prior written
 * permission.
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

#include "It_test_IO.h"
#include "time.h"
#include "signal.h"
#include <poll.h>
#include <fcntl.h>
#include <unistd.h>
#include <strings.h>

#define BUF_LEN            20
#define MAX_SCAN_FDSET     10      /* define poll's listened FD set's size */
#define POLL_WAIT_TIMEOUT  10*1000 /* ms */

extern int ppoll(struct pollfd *fds, nfds_t nfds, const struct timespec *timeout, const sigset_t *sigmask);
void work_ppoll(int fd)
{
    int ret = 0;
    int i = 0;
    char recv_buf[BUF_LEN];
    sigset_t sigset;
    struct timespec t;
    struct pollfd scan_fdset[MAX_SCAN_FDSET];
    int scan_fdset_num = 10;
    int count = 5;

    bzero(recv_buf, BUF_LEN);
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGALRM);        /* add SIGALRM to sigset */
    sigaddset(&sigset, SIGUSR1);        /* add SIGUSR1 to sigset */
    bzero(&t, sizeof(struct timespec));
    t.tv_sec = 10;
    t.tv_nsec = 0;
    bzero(scan_fdset, sizeof(struct pollfd) * MAX_SCAN_FDSET);
    scan_fdset[0].fd = fd;

    /* attention:in the book《UNIX网络编程第一卷》P162 metions:POLLERR,POLLHUP,POLLNVAL\
       those error signals can not be set in events. */
    /* they will return in revents,while the proper condition happens. */
    scan_fdset[0].events = POLLOUT;  /* set the signal needed to be listened:POLLOUT/POLLIN */

    /* set other elements in the array as invalid. */
    for (i = 1; i < MAX_SCAN_FDSET; i++) {
        /* scan_fdset[i].fd = -1; */
        scan_fdset[i].fd = fd;
        scan_fdset[i].events = POLLOUT;  /* set the signal needed to be listened. */
    }
    /* scan_fdset_num = 1; */ /* 表示当前的scan_fdset[] 数组中只使用前面1 个元素存放需要监听的扫描符 */

    while (count--) {
       ret = ppoll(scan_fdset, scan_fdset_num, &t, &sigset);

       for (i = 0; i < MAX_SCAN_FDSET; i++) {
           if (scan_fdset[i].revents & POLLOUT) {
               TEST_PRINT("[INFO]%s:%d,%s,fd have signal!\n", __FILE__, __LINE__, __func__);
               ret = read(fd, recv_buf, BUF_LEN);
               if (-1 == ret) {
                   TEST_PRINT("[INFO]%s:%d,%s,read error!\n", __FILE__, __LINE__, __func__);
                   continue;
               }
               TEST_PRINT("[INFO]%s:%d,%s,recv_buf=%s\n", __FILE__, __LINE__, __func__, recv_buf);
           }
           TEST_PRINT("[INFO]%s:%d,%s,scan_fdset[i].revents=%d\n", __FILE__, __LINE__, __func__, scan_fdset[i].revents);
       }
    }
}

static UINT32 testcase(VOID)
{
    int fd;
    char *filename = FILEPATH_775;

    fd = open(filename, O_RDWR);
    TEST_PRINT("[INFO]%s:%d,%s,fd=%d\n", __FILE__, __LINE__, __func__, fd);
    work_ppoll(fd);

    return LOS_OK;
}

VOID IO_TEST_PPOLL_002(VOID)
{
    TEST_ADD_CASE(__FUNCTION__, testcase, TEST_LIB, TEST_LIBC, TEST_LEVEL1, TEST_FUNCTION);
}
