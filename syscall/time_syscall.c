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

#include "errno.h"
#include "unistd.h"
#include "limits.h"
#include "utime.h"
#include "time.h"
#include "user_copy.h"
#include "sys/times.h"
#include "los_signal.h"
#include "los_memory.h"
#include "los_strncpy_from_user.h"
#include "time_posix.h"

#ifdef LOSCFG_FS_VFS
/**
 * @brief 系统调用：修改文件的访问和修改时间
 * @param path 文件路径
 * @param ptimes 指向utimbuf结构体的指针，包含新的访问和修改时间（为NULL时使用当前时间）
 * @return 成功返回0，失败返回-1并设置errno
 */
int SysUtime(const char *path, const struct utimbuf *ptimes)
{
    int ret;                          // 函数返回值
    char *spath = NULL;               // 内核空间中的文件路径缓冲区
    struct utimbuf sptimes;           // 内核空间中的utimbuf结构体副本

    if (path == NULL) {               // 检查路径参数是否为空
        errno = EINVAL;               // 设置参数无效错误
        return -EINVAL;               // 返回错误码
    }

    spath = LOS_MemAlloc(m_aucSysMem0, PATH_MAX + 1);  // 分配路径缓冲区内存
    if (spath == NULL) {              // 检查内存分配是否成功
        errno = ENOMEM;               // 设置内存不足错误
        return -ENOMEM;               // 返回错误码
    }

    // 从用户空间拷贝路径到内核空间
    ret = LOS_StrncpyFromUser(spath, path, PATH_MAX + 1);
    if (ret == -EFAULT) {             // 检查拷贝是否失败（用户空间地址无效）
        LOS_MemFree(m_aucSysMem0, spath);  // 释放已分配的内存
        return ret;                   // 返回错误码
    } else if (ret > PATH_MAX) {      // 检查路径长度是否超过最大值
        LOS_MemFree(m_aucSysMem0, spath);  // 释放已分配的内存
        PRINT_ERR("%s[%d], path exceeds maxlen: %d\n", __FUNCTION__, __LINE__, PATH_MAX);  // 打印错误信息
        return -ENAMETOOLONG;         // 返回路径过长错误
    }
    spath[ret] = '\0';                // 添加字符串结束符

    // 如果提供了时间参数，则从用户空间拷贝到内核空间
    if (ptimes && LOS_ArchCopyFromUser(&sptimes, ptimes, sizeof(struct utimbuf))) {
        LOS_MemFree(m_aucSysMem0, spath);  // 释放路径缓冲区内存
        errno = EFAULT;               // 设置内存访问错误
        return -EFAULT;               // 返回错误码
    }

    // 调用utime函数修改文件时间
    ret = utime(spath, ptimes ? &sptimes : NULL);
    if (ret < 0) {                    // 检查utime调用是否失败
        ret = -get_errno();           // 获取并转换错误码
    }

    LOS_MemFree(m_aucSysMem0, spath);  // 释放路径缓冲区内存

    return ret;                       // 返回结果
}
#endif

/**
 * @brief 系统调用：获取当前时间
 * @param tloc 指向time_t的指针，用于存储当前时间（为NULL时仅返回时间值）
 * @return 成功返回当前时间戳，失败返回-1并设置errno
 */
time_t SysTime(time_t *tloc)
{
    int ret;                          // 函数返回值
    time_t stloc;                     // 内核空间中的时间戳变量

    // 调用time函数获取当前时间
    ret = time(tloc ? &stloc : NULL);
    if (ret < 0) {                    // 检查time调用是否失败
        return -get_errno();          // 返回错误码
    }

    // 如果提供了tloc指针，则将结果拷贝到用户空间
    if (tloc && LOS_ArchCopyToUser(tloc, &stloc, sizeof(time_t))) {
        errno = EFAULT;               // 设置内存访问错误
        ret = -EFAULT;                // 设置返回错误码
    }

    return ret;                       // 返回时间戳或错误码
}

/**
 * @brief 系统调用：设置间隔定时器
 * @param which 定时器类型（ITIMER_REAL/ITIMER_VIRTUAL/ITIMER_PROF）
 * @param value 指向itimerval结构体的指针，包含新的定时器值
 * @param ovalue 指向itimerval结构体的指针，用于存储旧的定时器值（可为NULL）
 * @return 成功返回0，失败返回-1并设置errno
 */
int SysSetiTimer(int which, const struct itimerval *value, struct itimerval *ovalue)
{
    int ret;                          // 函数返回值
    struct itimerval svalue;          // 内核空间中的新定时器值
    struct itimerval sovalue = { 0 }; // 内核空间中的旧定时器值

    if (value == NULL) {              // 检查value参数是否为空
        errno = EINVAL;               // 设置参数无效错误
        return -EINVAL;               // 返回错误码
    }

    // 从用户空间拷贝新定时器值到内核空间
    if (LOS_ArchCopyFromUser(&svalue, value, sizeof(struct itimerval))) {
        errno = EFAULT;               // 设置内存访问错误
        return -EFAULT;               // 返回错误码
    }

    // 调用setitimer设置定时器
    ret = setitimer(which, &svalue, &sovalue);
    if (ret < 0) {                    // 检查setitimer调用是否失败
        return -get_errno();          // 返回错误码
    }

    // 如果提供了ovalue指针，则将旧定时器值拷贝到用户空间
    if (ovalue && LOS_ArchCopyToUser(ovalue, &sovalue, sizeof(struct itimerval))) {
        errno = EFAULT;               // 设置内存访问错误
        return -EFAULT;               // 返回错误码
    }

    return ret;                       // 返回成功
}

/**
 * @brief 系统调用：获取间隔定时器当前值
 * @param which 定时器类型（ITIMER_REAL/ITIMER_VIRTUAL/ITIMER_PROF）
 * @param value 指向itimerval结构体的指针，用于存储定时器当前值
 * @return 成功返回0，失败返回-1并设置errno
 */
int SysGetiTimer(int which, struct itimerval *value)
{
    int ret;                          // 函数返回值
    struct itimerval svalue = { 0 };  // 内核空间中的定时器值

    if (value == NULL) {              // 检查value参数是否为空
        errno = EINVAL;               // 设置参数无效错误
        return -EINVAL;               // 返回错误码
    }

    // 调用getitimer获取定时器值
    ret = getitimer(which, &svalue);
    if (ret < 0) {                    // 检查getitimer调用是否失败
        return -get_errno();          // 返回错误码
    }

    // 将定时器值拷贝到用户空间
    if (LOS_ArchCopyToUser(value, &svalue, sizeof(struct itimerval))) {
        errno = EFAULT;               // 设置内存访问错误
        return -EFAULT;               // 返回错误码
    }

    return ret;                       // 返回成功
}

/**
 * @brief 系统调用：创建定时器
 * @param clockID 时钟类型ID
 * @param evp 指向ksigevent结构体的指针，包含定时器到期时的通知方式
 * @param timerID 指向timer_t的指针，用于存储创建的定时器ID
 * @return 成功返回0，失败返回-1并设置errno
 */
int SysTimerCreate(clockid_t clockID, struct ksigevent *evp, timer_t *timerID)
{
    int ret;                          // 函数返回值
    timer_t stimerID;                 // 内核空间中的定时器ID
    struct ksigevent ksevp;           // 内核空间中的ksigevent结构体副本

    if (timerID == NULL) {            // 检查timerID参数是否为空
        errno = EINVAL;               // 设置参数无效错误
        return -EINVAL;               // 返回错误码
    }

    // 如果提供了evp指针，则从用户空间拷贝事件结构
    if (evp && LOS_ArchCopyFromUser(&ksevp, evp, sizeof(struct ksigevent))) {
        errno = EFAULT;               // 设置内存访问错误
        return -EFAULT;               // 返回错误码
    }

    // 调用内核函数创建定时器
    ret = OsTimerCreate(clockID, evp ? &ksevp : NULL, &stimerID);
    if (ret < 0) {                    // 检查创建是否失败
        return -get_errno();          // 返回错误码
    }

    // 将定时器ID拷贝到用户空间
    if (LOS_ArchCopyToUser(timerID, &stimerID, sizeof(timer_t))) {
        errno = EFAULT;               // 设置内存访问错误
        return -EFAULT;               // 返回错误码
    }

    return ret;                       // 返回成功
}

/**
 * @brief 系统调用：获取定时器当前值
 * @param timerID 定时器ID
 * @param value 指向itimerspec结构体的指针，用于存储定时器当前值
 * @return 成功返回0，失败返回-1并设置errno
 */
int SysTimerGettime(timer_t timerID, struct itimerspec *value)
{
    int ret;                          // 函数返回值
    struct itimerspec svalue = { 0 }; // 内核空间中的定时器值

    if (value == NULL) {              // 检查value参数是否为空
        errno = EINVAL;               // 设置参数无效错误
        return -EINVAL;               // 返回错误码
    }

    // 调用timer_gettime获取定时器值
    ret = timer_gettime(timerID, &svalue);
    if (ret < 0) {                    // 检查获取是否失败
        return -get_errno();          // 返回错误码
    }

    // 将定时器值拷贝到用户空间
    if (LOS_ArchCopyToUser(value, &svalue, sizeof(struct itimerspec))) {
        errno = EFAULT;               // 设置内存访问错误
        return -EFAULT;               // 返回错误码
    }

    return ret;                       // 返回成功
}

/**
 * @brief 系统调用：设置定时器值
 * @param timerID 定时器ID
 * @param flags 定时器标志（TIMER_ABSTIME表示绝对时间）
 * @param value 指向itimerspec结构体的指针，包含新的定时器值
 * @param oldValue 指向itimerspec结构体的指针，用于存储旧的定时器值（可为NULL）
 * @return 成功返回0，失败返回-1并设置errno
 */
int SysTimerSettime(timer_t timerID, int flags, const struct itimerspec *value, struct itimerspec *oldValue)
{
    int ret;                          // 函数返回值
    struct itimerspec svalue;         // 内核空间中的新定时器值
    struct itimerspec soldValue = { 0 };// 内核空间中的旧定时器值

    if (value == NULL) {              // 检查value参数是否为空
        errno = EINVAL;               // 设置参数无效错误
        return -EINVAL;               // 返回错误码
    }

    // 从用户空间拷贝新定时器值到内核空间
    if (LOS_ArchCopyFromUser(&svalue, value, sizeof(struct itimerspec))) {
        errno = EFAULT;               // 设置内存访问错误
        return -EFAULT;               // 返回错误码
    }

    // 调用timer_settime设置定时器
    ret = timer_settime(timerID, flags, &svalue, &soldValue);
    if (ret < 0) {                    // 检查设置是否失败
        return -get_errno();          // 返回错误码
    }

    // 如果提供了oldValue指针，则将旧定时器值拷贝到用户空间
    if (oldValue && LOS_ArchCopyToUser(oldValue, &soldValue, sizeof(struct itimerspec))) {
        errno = EFAULT;               // 设置内存访问错误
        return -EFAULT;               // 返回错误码
    }

    return ret;                       // 返回成功
}

/**
 * @brief 系统调用：获取定时器溢出次数
 * @param timerID 定时器ID
 * @return 成功返回溢出次数，失败返回-1并设置errno
 */
int SysTimerGetoverrun(timer_t timerID)
{
    int ret;                          // 函数返回值

    // 调用timer_getoverrun获取溢出次数
    ret = timer_getoverrun(timerID);
    if (ret < 0) {                    // 检查获取是否失败
        return -get_errno();          // 返回错误码
    }
    return ret;                       // 返回溢出次数
}

/**
 * @brief 系统调用：删除定时器
 * @param timerID 定时器ID
 * @return 成功返回0，失败返回-1并设置errno
 */
int SysTimerDelete(timer_t timerID)
{
    int ret;                          // 函数返回值

    // 调用timer_delete删除定时器
    ret = timer_delete(timerID);
    if (ret < 0) {                    // 检查删除是否失败
        return -get_errno();          // 返回错误码
    }
    return ret;                       // 返回成功
}

/**
 * @brief 系统调用：设置时钟时间
 * @param clockID 时钟类型ID
 * @param tp 指向timespec结构体的指针，包含新的时间值
 * @return 成功返回0，失败返回-1并设置errno
 */
int SysClockSettime(clockid_t clockID, const struct timespec *tp)
{
    int ret;                          // 函数返回值
    struct timespec stp;              // 内核空间中的时间结构体

    if (tp == NULL) {                 // 检查tp参数是否为空
        errno = EINVAL;               // 设置参数无效错误
        return -EINVAL;               // 返回错误码
    }

    // 从用户空间拷贝时间值到内核空间
    if (LOS_ArchCopyFromUser(&stp, tp, sizeof(struct timespec))) {
        errno = EFAULT;               // 设置内存访问错误
        return -EFAULT;               // 返回错误码
    }

    // 调用clock_settime设置时钟时间
    ret = clock_settime(clockID, &stp);
    if (ret < 0) {                    // 检查设置是否失败
        return -get_errno();          // 返回错误码
    }
    return ret;                       // 返回成功
}

/**
 * @brief 系统调用：获取时钟时间
 * @param clockID 时钟类型ID
 * @param tp 指向timespec结构体的指针，用于存储当前时间
 * @return 成功返回0，失败返回-1并设置errno
 */
int SysClockGettime(clockid_t clockID, struct timespec *tp)
{
    int ret;                          // 函数返回值
    struct timespec stp = { 0 };      // 内核空间中的时间结构体

    if (tp == NULL) {                 // 检查tp参数是否为空
        errno = EINVAL;               // 设置参数无效错误
        return -EINVAL;               // 返回错误码
    }

    // 调用clock_gettime获取时钟时间
    ret = clock_gettime(clockID, &stp);
    if (ret < 0) {                    // 检查获取是否失败
        return -get_errno();          // 返回错误码
    }

    // 将时间值拷贝到用户空间
    if (LOS_ArchCopyToUser(tp, &stp, sizeof(struct timespec))) {
        errno = EFAULT;               // 设置内存访问错误
        return -EFAULT;               // 返回错误码
    }

    return ret;                       // 返回成功
}

/**
 * @brief 系统调用：获取时钟分辨率
 * @param clockID 时钟类型ID
 * @param tp 指向timespec结构体的指针，用于存储时钟分辨率
 * @return 成功返回0，失败返回-1并设置errno
 */
int SysClockGetres(clockid_t clockID, struct timespec *tp)
{
    int ret;                          // 函数返回值
    struct timespec stp = { 0 };      // 内核空间中的时间结构体

    if (tp == NULL) {                 // 检查tp参数是否为空
        errno = EINVAL;               // 设置参数无效错误
        return -EINVAL;               // 返回错误码
    }

    // 调用clock_getres获取时钟分辨率
    ret = clock_getres(clockID, &stp);
    if (ret < 0) {                    // 检查获取是否失败
        return -get_errno();          // 返回错误码
    }

    // 将分辨率值拷贝到用户空间
    if (LOS_ArchCopyToUser(tp, &stp, sizeof(struct timespec))) {
        errno = EFAULT;               // 设置内存访问错误
        return -EFAULT;               // 返回错误码
    }

    return ret;                       // 返回成功
}

/**
 * @brief 系统调用：高精度睡眠（可指定时钟类型）
 * @param clk 时钟类型ID
 * @param flags 睡眠标志（TIMER_ABSTIME表示绝对时间）
 * @param req 指向timespec结构体的指针，包含请求睡眠的时间
 * @param rem 指向timespec结构体的指针，用于存储剩余未睡眠的时间（可为NULL）
 * @return 成功返回0，被信号中断返回-ERESTARTNOHAND，失败返回-1并设置errno
 */
int SysClockNanoSleep(clockid_t clk, int flags, const struct timespec *req, struct timespec *rem)
{
    int ret;                          // 函数返回值
    struct timespec sreq;             // 内核空间中的请求睡眠时间
    struct timespec srem = { 0 };     // 内核空间中的剩余睡眠时间

    // 检查请求时间参数是否有效并拷贝到内核空间
    if (!req || LOS_ArchCopyFromUser(&sreq, req, sizeof(struct timespec))) {
        errno = EFAULT;               // 设置内存访问错误
        return -EFAULT;               // 返回错误码
    }

    // 调用clock_nanosleep进行睡眠
    ret = clock_nanosleep(clk, flags, &sreq, rem ? &srem : NULL);
    if (ret < 0) {                    // 检查睡眠是否失败
        return -get_errno();          // 返回错误码
    }

    // 如果提供了rem指针，则将剩余时间拷贝到用户空间
    if (rem && LOS_ArchCopyToUser(rem, &srem, sizeof(struct timespec))) {
        errno = EFAULT;               // 设置内存访问错误
        return -EFAULT;               // 返回错误码
    }

    return ret;                       // 返回成功
}

/**
 * @brief 系统调用：高精度睡眠（使用CLOCK_REALTIME时钟）
 * @param rqtp 指向timespec结构体的指针，包含请求睡眠的时间
 * @param rmtp 指向timespec结构体的指针，用于存储剩余未睡眠的时间（可为NULL）
 * @return 成功返回0，被信号中断返回-ERESTARTNOHAND，失败返回-1并设置errno
 */
int SysNanoSleep(const struct timespec *rqtp, struct timespec *rmtp)
{
    int ret;                          // 函数返回值
    struct timespec srqtp;            // 内核空间中的请求睡眠时间
    struct timespec srmtp = { 0 };    // 内核空间中的剩余睡眠时间

    // 检查请求时间参数是否有效并拷贝到内核空间
    if (!rqtp || LOS_ArchCopyFromUser(&srqtp, rqtp, sizeof(struct timespec))) {
        errno = EFAULT;               // 设置内存访问错误
        return -EFAULT;               // 返回错误码
    }

    // 如果提供了剩余时间指针，则从用户空间拷贝初始值
    if (rmtp && LOS_ArchCopyFromUser(&srmtp, rmtp, sizeof(struct timespec))) {
        errno = EFAULT;               // 设置内存访问错误
        return -EFAULT;               // 返回错误码
    }

    // 调用nanosleep进行睡眠
    ret = nanosleep(&srqtp, rmtp ? &srmtp : NULL);
    if (ret < 0) {                    // 检查睡眠是否失败
        return -get_errno();          // 返回错误码
    }

    // 如果提供了rmtp指针，则将剩余时间拷贝到用户空间
    if (rmtp && LOS_ArchCopyToUser(rmtp, &srmtp, sizeof(struct timespec))) {
        errno = EFAULT;               // 设置内存访问错误
        return -EFAULT;               // 返回错误码
    }

    return ret;                       // 返回成功
}

/**
 * @brief 系统调用：获取进程时间
 * @param buf 指向tms结构体的指针，用于存储进程时间信息
 * @return 成功返回时钟滴答数，失败返回-1并设置errno
 */
clock_t SysTimes(struct tms *buf)
{
    clock_t ret;                      // 函数返回值
    struct tms sbuf = { 0 };          // 内核空间中的tms结构体

    if (buf == NULL) {                // 检查buf参数是否为空
        errno = EFAULT;               // 设置内存访问错误
        return -EFAULT;               // 返回错误码
    }
    // 调用times获取进程时间
    ret = times(&sbuf);
    if (ret == -1) {                  // 检查获取是否失败
        return -get_errno();          // 返回错误码
    }
    // 将进程时间信息拷贝到用户空间
    if (LOS_ArchCopyToUser(buf, &sbuf, sizeof(struct tms))) {
        errno = EFAULT;               // 设置内存访问错误
        return -EFAULT;               // 返回错误码
    }

    return ret;                       // 返回时钟滴答数
}

/**
 * @brief 系统调用：设置64位时钟时间
 * @param clockID 时钟类型ID
 * @param tp 指向timespec64结构体的指针，包含新的64位时间值
 * @return 成功返回0，失败返回-1并设置errno
 */
int SysClockSettime64(clockid_t clockID, const struct timespec64 *tp)
{
    int ret;                          // 函数返回值
    struct timespec t;                // 32位时间结构体
    struct timespec64 stp;            // 内核空间中的64位时间结构体

    if (tp == NULL) {                 // 检查tp参数是否为空
        errno = EINVAL;               // 设置参数无效错误
        return -EINVAL;               // 返回错误码
    }

    // 从用户空间拷贝64位时间值到内核空间
    if (LOS_ArchCopyFromUser(&stp, tp, sizeof(struct timespec64))) {
        errno = EFAULT;               // 设置内存访问错误
        return -EFAULT;               // 返回错误码
    }

    // 检查秒数是否超过32位无符号整数最大值
    if (stp.tv_sec > UINT32_MAX) {
        errno = ENOSYS;               // 设置系统不支持错误
        return -ENOSYS;               // 返回错误码
    }
    // 转换为32位时间结构体
    t.tv_sec = stp.tv_sec;
    t.tv_nsec = stp.tv_nsec;

    // 调用clock_settime设置时钟时间
    ret = clock_settime(clockID, &t);
    if (ret < 0) {                    // 检查设置是否失败
        return -get_errno();          // 返回错误码
    }
    return ret;                       // 返回成功
}

/**
 * @brief 系统调用：获取64位时钟时间
 * @param clockID 时钟类型ID
 * @param tp 指向timespec64结构体的指针，用于存储64位当前时间
 * @return 成功返回0，失败返回-1并设置errno
 */
int SysClockGettime64(clockid_t clockID, struct timespec64 *tp)
{
    int ret;                          // 函数返回值
    struct timespec t;                // 32位时间结构体
    struct timespec64 stp = { 0 };    // 内核空间中的64位时间结构体

    if (tp == NULL) {                 // 检查tp参数是否为空
        errno = EINVAL;               // 设置参数无效错误
        return -EINVAL;               // 返回错误码
    }

    // 调用clock_gettime获取32位时钟时间
    ret = clock_gettime(clockID, &t);
    if (ret < 0) {                    // 检查获取是否失败
        return -get_errno();          // 返回错误码
    }

    // 转换为64位时间结构体
    stp.tv_sec = t.tv_sec;
    stp.tv_nsec = t.tv_nsec;

    // 将64位时间值拷贝到用户空间
    if (LOS_ArchCopyToUser(tp, &stp, sizeof(struct timespec64))) {
        errno = EFAULT;               // 设置内存访问错误
        return -EFAULT;               // 返回错误码
    }

    return ret;                       // 返回成功
}

/**
 * @brief 系统调用：获取64位时钟分辨率
 * @param clockID 时钟类型ID
 * @param tp 指向timespec64结构体的指针，用于存储64位时钟分辨率
 * @return 成功返回0，失败返回-1并设置errno
 */
int SysClockGetres64(clockid_t clockID, struct timespec64 *tp)
{
    int ret;                          // 函数返回值
    struct timespec t;                // 32位时间结构体
    struct timespec64 stp = { 0 };    // 内核空间中的64位时间结构体

    if (tp == NULL) {                 // 检查tp参数是否为空
        errno = EINVAL;               // 设置参数无效错误
        return -EINVAL;               // 返回错误码
    }

    // 调用clock_getres获取32位时钟分辨率
    ret = clock_getres(clockID, &t);
    if (ret < 0) {                    // 检查获取是否失败
        return -get_errno();          // 返回错误码
    }

    // 转换为64位时间结构体
    stp.tv_sec = t.tv_sec;
    stp.tv_nsec = t.tv_nsec;

    // 将64位分辨率值拷贝到用户空间
    if (LOS_ArchCopyToUser(tp, &stp, sizeof(struct timespec64))) {
        errno = EFAULT;               // 设置内存访问错误
        return -EFAULT;               // 返回错误码
    }

    return ret;                       // 返回成功
}

/**
 * @brief 系统调用：64位高精度睡眠（可指定时钟类型）
 * @param clk 时钟类型ID
 * @param flags 睡眠标志（TIMER_ABSTIME表示绝对时间）
 * @param req 指向timespec64结构体的指针，包含请求睡眠的64位时间
 * @param rem 指向timespec64结构体的指针，用于存储剩余未睡眠的时间（可为NULL）
 * @return 成功返回0，被信号中断返回-ERESTARTNOHAND，失败返回-1并设置errno
 */
int SysClockNanoSleep64(clockid_t clk, int flags, const struct timespec64 *req, struct timespec64 *rem)
{
    int ret;                          // 函数返回值
    struct timespec rq;               // 32位请求睡眠时间
    struct timespec rm = { 0 };       // 32位剩余睡眠时间
    struct timespec64 sreq;           // 内核空间中的64位请求睡眠时间
    struct timespec64 srem = { 0 };   // 内核空间中的64位剩余睡眠时间

    // 检查请求时间参数是否有效并拷贝到内核空间
    if (!req || LOS_ArchCopyFromUser(&sreq, req, sizeof(struct timespec64))) {
        errno = EFAULT;               // 设置内存访问错误
        return -EFAULT;               // 返回错误码
    }

    // 转换64位时间到32位（秒数超过UINT32_MAX时截断）
    if (req != NULL) {
        rq.tv_sec = (sreq.tv_sec > UINT32_MAX) ? UINT32_MAX : sreq.tv_sec;
        rq.tv_nsec = sreq.tv_nsec;
    }

    // 调用clock_nanosleep进行睡眠
    ret = clock_nanosleep(clk, flags, &rq, rem ? &rm : NULL);
    if (ret < 0) {                    // 检查睡眠是否失败
        return -get_errno();          // 返回错误码
    }

    // 如果提供了rem指针，则转换剩余时间为64位并拷贝到用户空间
    if (rem != NULL) {
        srem.tv_sec = rm.tv_sec;
        srem.tv_nsec = rm.tv_nsec;
        if (LOS_ArchCopyToUser(rem, &srem, sizeof(struct timespec64))) {
            errno = EFAULT;           // 设置内存访问错误
            return -EFAULT;           // 返回错误码
        }
    }

    return ret;                       // 返回成功
}

/**
 * @brief 系统调用：获取64位定时器当前值
 * @param timerID 定时器ID
 * @param value 指向itimerspec64结构体的指针，用于存储64位定时器当前值
 * @return 成功返回0，失败返回-1并设置errno
 */
int SysTimerGettime64(timer_t timerID, struct itimerspec64 *value)
{
    int ret;                          // 函数返回值
    struct itimerspec val;            // 32位定时器值
    struct itimerspec64 svalue = { 0 };// 内核空间中的64位定时器值

    if (value == NULL) {              // 检查value参数是否为空
        errno = EINVAL;               // 设置参数无效错误
        return -EINVAL;               // 返回错误码
    }

    // 调用timer_gettime获取32位定时器值
    ret = timer_gettime(timerID, &val);
    if (ret < 0) {                    // 检查获取是否失败
        return -get_errno();          // 返回错误码
    }

    // 转换为64位定时器结构体
    svalue.it_interval.tv_sec = val.it_interval.tv_sec;
    svalue.it_interval.tv_nsec = val.it_interval.tv_nsec;
    svalue.it_value.tv_sec = val.it_value.tv_sec;
    svalue.it_value.tv_nsec = val.it_value.tv_nsec;

    // 将64位定时器值拷贝到用户空间
    if (LOS_ArchCopyToUser(value, &svalue, sizeof(struct itimerspec64))) {
        errno = EFAULT;               // 设置内存访问错误
        return -EFAULT;               // 返回错误码
    }

    return ret;                       // 返回成功
}

/**
 * @brief 系统调用：设置64位定时器值
 * @param timerID 定时器ID
 * @param flags 定时器标志（TIMER_ABSTIME表示绝对时间）
 * @param value 指向itimerspec64结构体的指针，包含新的64位定时器值
 * @param oldValue 指向itimerspec64结构体的指针，用于存储旧的64位定时器值（可为NULL）
 * @return 成功返回0，失败返回-1并设置errno
 */
int SysTimerSettime64(timer_t timerID, int flags, const struct itimerspec64 *value, struct itimerspec64 *oldValue)
{
    int ret;                          // 函数返回值
    struct itimerspec val;            // 32位新定时器值
    struct itimerspec oldVal;         // 32位旧定时器值
    struct itimerspec64 svalue;       // 内核空间中的64位新定时器值
    struct itimerspec64 soldValue;    // 内核空间中的64位旧定时器值

    if (value == NULL) {              // 检查value参数是否为空
        errno = EINVAL;               // 设置参数无效错误
        return -EINVAL;               // 返回错误码
    }

    // 从用户空间拷贝64位定时器值到内核空间
    if (LOS_ArchCopyFromUser(&svalue, value, sizeof(struct itimerspec64))) {
        errno = EFAULT;               // 设置内存访问错误
        return -EFAULT;               // 返回错误码
    }

    // 检查64位时间的秒数是否超过32位无符号整数最大值
    if (svalue.it_interval.tv_sec > UINT32_MAX || svalue.it_value.tv_sec > UINT32_MAX) {
        errno = ENOSYS;               // 设置系统不支持错误
        return -ENOSYS;               // 返回错误码
    }

    // 转换64位定时器值到32位
    val.it_interval.tv_sec = svalue.it_interval.tv_sec;
    val.it_interval.tv_nsec = svalue.it_interval.tv_nsec;
    val.it_value.tv_sec = svalue.it_value.tv_sec;
    val.it_value.tv_nsec = svalue.it_value.tv_nsec;

    // 调用timer_settime设置定时器
    ret = timer_settime(timerID, flags, &val, oldValue ? &oldVal : NULL);
    if (ret < 0) {                    // 检查设置是否失败
        return -get_errno();          // 返回错误码
    }

    // 如果提供了oldValue指针，则转换旧定时器值为64位并拷贝到用户空间
    if (oldValue != NULL) {
        (void)memset_s(&soldValue, sizeof(struct itimerspec64), 0, sizeof(struct itimerspec64));  // 清零结构体
        soldValue.it_interval.tv_sec = oldVal.it_interval.tv_sec;
        soldValue.it_interval.tv_nsec = oldVal.it_interval.tv_nsec;
        soldValue.it_value.tv_sec = oldVal.it_value.tv_sec;
        soldValue.it_value.tv_nsec = oldVal.it_value.tv_nsec;

        if (LOS_ArchCopyToUser(oldValue, &soldValue, sizeof(struct itimerspec64))) {
            errno = EFAULT;           // 设置内存访问错误
            return -EFAULT;           // 返回错误码
        }
    }

    return ret;                       // 返回成功
}