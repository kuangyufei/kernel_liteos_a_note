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

#include "time.h"
#include "stdint.h"
#include "stdio.h"
#include "sys/times.h"
#include "time_posix.h"
#include "unistd.h"
#ifdef LOSCFG_SECURITY_CAPABILITY
#include "capability_api.h"
#endif
#include "los_signal.h"
#ifdef LOSCFG_KERNEL_VDSO
#include "los_vdso.h"
#endif
#ifdef LOSCFG_SECURITY_VID
#include "vid_api.h"
#endif
#include "user_copy.h"
#include "los_process_pri.h"
#include "los_swtmr_pri.h"
#include "los_sys_pri.h"

#define CPUCLOCK_PERTHREAD_MASK 4
#define CPUCLOCK_ID_OFFSET 3

/*
 * Do a time package defined return. This requires the error code
 * to be placed in errno, and if it is non-zero, -1 returned as the
 * result of the function. This also gives us a place to put any
 * generic tidyup handling needed for things like signal delivery and
 * cancellation.
 */
#define TIME_RETURN(err) do { \
    INT32 retVal = 0;         \
    if ((err) != 0) {         \
        retVal = -1;          \
        errno = (err);        \
    }                         \
    return retVal;            \
} while (0)

#ifdef LOSCFG_AARCH64
/*
 * This two structures originally didn't exit,
 * they added by liteos to support 64bit interfaces on 32bit platform,
 * in 64bit platform, timeval64 define to timeval which is platform adaptive.
 */
#define timeval64 timeval
#define timespec64 timespec
#endif
/**
 * @brief   验证timeval结构体的有效性
 * @param   tv  指向timeval结构体的指针
 * @return  有效返回TRUE，无效返回FALSE
 */
STATIC INLINE BOOL ValidTimeval(const struct timeval *tv)
{
    /* 检查指针是否为空 */
    if (tv == NULL) {
        return FALSE;
    }

    /* 检查微秒数是否在合法范围内（0 <= tv_usec < 1000000）且秒数非负 */
    if ((tv->tv_usec < 0) || (tv->tv_usec >= OS_SYS_US_PER_SECOND) || (tv->tv_sec < 0)) {
        return FALSE;
    }

    return TRUE;
}

/**
 * @brief   验证64位timeval64结构体的有效性
 * @param   tv  指向timeval64结构体的指针
 * @return  有效返回TRUE，无效返回FALSE
 */
STATIC INLINE BOOL ValidTimeval64(const struct timeval64 *tv)
{
    /* 检查指针是否为空 */
    if (tv == NULL) {
        return FALSE;
    }

    /* 检查微秒数是否在合法范围内（0 <= tv_usec < 1000000）且秒数非负 */
    if ((tv->tv_usec < 0) || (tv->tv_usec >= OS_SYS_US_PER_SECOND) || (tv->tv_sec < 0)) {
        return FALSE;
    }

    return TRUE;
}

/**
 * @brief   验证软件定时器ID的有效性
 * @param   swtmrID 软件定时器ID
 * @return  有效返回TRUE，无效返回FALSE
 */
STATIC INLINE BOOL ValidTimerID(UINT16 swtmrID)
{
    /* 检查定时器ID是否超出最大范围 */
    if (swtmrID >= OS_SWTMR_MAX_TIMERID) {
        return FALSE;
    }

    /* 检查定时器的所有者是否为当前进程 */
    if (OS_SWT_FROM_SID(swtmrID)->uwOwnerPid != (UINTPTR)OsCurrProcessGet()) {
        return FALSE;
    }

    return TRUE;
}

STATIC SPIN_LOCK_INIT(g_timeSpin);               // 时间操作自旋锁
STATIC long long g_adjTimeLeft;                  /* adjtime的绝对值 */
STATIC INT32 g_adjDirection;                     /* 调整方向：1加速，0减速 */

/* 调整步长，每SCHED_CLOCK_INTETRVAL_TICKS个时钟周期的纳秒数 */
STATIC const long long g_adjPacement = (((LOSCFG_BASE_CORE_ADJ_PER_SECOND * SCHED_CLOCK_INTETRVAL_TICKS) /
                                        LOSCFG_BASE_CORE_TICK_PER_SECOND) * OS_SYS_NS_PER_US);

/* 来自连续调整（如adjtime）的累积时间增量 */
STATIC struct timespec64 g_accDeltaFromAdj;
/* 来自非连续调整（如settimeofday）的累积时间增量 */
STATIC struct timespec64 g_accDeltaFromSet;

/**
 * @brief   执行时间调整操作
 * @note    该函数会根据g_adjTimeLeft和g_adjDirection调整系统时间
 */
VOID OsAdjTime(VOID)
{
    UINT32 intSave;

    LOS_SpinLockSave(&g_timeSpin, &intSave);
    if (!g_adjTimeLeft) {                       // 如果没有剩余调整时间，直接返回
        LOS_SpinUnlockRestore(&g_timeSpin, intSave);
        return;
    }

    if (g_adjTimeLeft > g_adjPacement) {        // 剩余调整时间大于单次步长
        if (g_adjDirection) {                   // 加速调整
            if ((g_accDeltaFromAdj.tv_nsec + g_adjPacement) >= OS_SYS_NS_PER_SECOND) {
                g_accDeltaFromAdj.tv_sec++;    // 纳秒数溢出，秒数加1
                g_accDeltaFromAdj.tv_nsec  = (g_accDeltaFromAdj.tv_nsec + g_adjPacement) % OS_SYS_NS_PER_SECOND;
            } else {
                g_accDeltaFromAdj.tv_nsec  = g_accDeltaFromAdj.tv_nsec + g_adjPacement;
            }
        } else {                               // 减速调整
            if ((g_accDeltaFromAdj.tv_nsec - g_adjPacement) < 0) {
                g_accDeltaFromAdj.tv_sec--;    // 纳秒数不足，秒数减1
                g_accDeltaFromAdj.tv_nsec  = g_accDeltaFromAdj.tv_nsec - g_adjPacement + OS_SYS_NS_PER_SECOND;
            } else {
                g_accDeltaFromAdj.tv_nsec  = g_accDeltaFromAdj.tv_nsec - g_adjPacement;
            }
        }

        g_adjTimeLeft -= g_adjPacement;         // 减去已调整的步长
    } else {                                    // 剩余调整时间小于等于单次步长
        if (g_adjDirection) {                   // 加速调整
            if ((g_accDeltaFromAdj.tv_nsec + g_adjTimeLeft) >= OS_SYS_NS_PER_SECOND) {
                g_accDeltaFromAdj.tv_sec++;    // 纳秒数溢出，秒数加1
                g_accDeltaFromAdj.tv_nsec  = (g_accDeltaFromAdj.tv_nsec + g_adjTimeLeft) % OS_SYS_NS_PER_SECOND;
            } else {
                g_accDeltaFromAdj.tv_nsec  = g_accDeltaFromAdj.tv_nsec + g_adjTimeLeft;
            }
        } else {                               // 减速调整
            if ((g_accDeltaFromAdj.tv_nsec - g_adjTimeLeft) < 0) {
                g_accDeltaFromAdj.tv_sec--;    // 纳秒数不足，秒数减1
                g_accDeltaFromAdj.tv_nsec  = g_accDeltaFromAdj.tv_nsec - g_adjTimeLeft + OS_SYS_NS_PER_SECOND;
            } else {
                g_accDeltaFromAdj.tv_nsec  = g_accDeltaFromAdj.tv_nsec - g_adjTimeLeft;
            }
        }

        g_adjTimeLeft = 0;                      // 调整完成，剩余时间清零
    }
    LOS_SpinUnlockRestore(&g_timeSpin, intSave);
    return;
}

/*
 * Function: adjtime
 * Description:  校正时间以同步系统时钟
 * Input:     delta - 时钟需要调整的时间量
 * Output: oldDelta - 先前未完成的调整剩余时间
 * Return: 成功返回0，失败返回-1并设置errno
 */
int adjtime(const struct timeval *delta, struct timeval *oldDelta)
{
    UINT32 intSave;
    LOS_SpinLockSave(&g_timeSpin, &intSave);
    /* 返回先前未完成的调整剩余时间 */
    if (oldDelta != NULL) {
        if (g_adjDirection == 1) {
            oldDelta->tv_sec = g_adjTimeLeft / OS_SYS_NS_PER_SECOND;
            oldDelta->tv_usec = (g_adjTimeLeft % OS_SYS_NS_PER_SECOND) / OS_SYS_NS_PER_US;
        } else {
            oldDelta->tv_sec = -(g_adjTimeLeft / OS_SYS_NS_PER_SECOND);
            oldDelta->tv_usec = -((g_adjTimeLeft % OS_SYS_NS_PER_SECOND) / OS_SYS_NS_PER_US);
        }
    }

    if ((delta == NULL) || ((delta->tv_sec == 0) && (delta->tv_usec == 0))) { // 无需调整
        LOS_SpinUnlockRestore(&g_timeSpin, intSave);
        return 0;
    }

    if ((delta->tv_usec > OS_SYS_US_PER_SECOND) || (delta->tv_usec < -OS_SYS_US_PER_SECOND)) { // 微秒数超出范围
        LOS_SpinUnlockRestore(&g_timeSpin, intSave);
        TIME_RETURN(EINVAL);
    }

    /*
     * 2: 在glibc实现中，delta必须小于等于(INT_MAX / 1000000 - 2)且
     * 大于等于(INT_MIN / 1000000 + 2)
     */
    if ((delta->tv_sec < (INT_MIN / OS_SYS_US_PER_SECOND + 2)) ||
        (delta->tv_sec > (INT_MAX / OS_SYS_US_PER_SECOND + 2))) {
        LOS_SpinUnlockRestore(&g_timeSpin, intSave);
        TIME_RETURN(EINVAL);
    }

    g_adjTimeLeft = (INT64)delta->tv_sec * OS_SYS_NS_PER_SECOND + delta->tv_usec * OS_SYS_NS_PER_US; // 转换为纳秒
    if (g_adjTimeLeft > 0) {
        g_adjDirection = 1;                     // 正值表示加速
    } else {
        g_adjDirection = 0;                     // 负值表示减速
        g_adjTimeLeft = -g_adjTimeLeft;         // 取绝对值
    }

    LOS_SpinUnlockRestore(&g_timeSpin, intSave);
    return 0;
}

/**
 * @brief   两个timespec64结构体相加
 * @param   t1  第一个时间结构体
 * @param   t2  第二个时间结构体
 * @return  相加后的时间结构体
 */
STATIC INLINE struct timespec64 OsTimeSpecAdd(const struct timespec64 t1, const struct timespec64 t2)
{
    struct timespec64 ret = {0};

    ret.tv_sec = t1.tv_sec + t2.tv_sec;
    ret.tv_nsec = t1.tv_nsec + t2.tv_nsec;
    if (ret.tv_nsec >= OS_SYS_NS_PER_SECOND) {   // 纳秒数溢出
        ret.tv_sec += 1;
        ret.tv_nsec -= OS_SYS_NS_PER_SECOND;
    } else if (ret.tv_nsec < 0L) {              // 纳秒数为负
        ret.tv_sec -= 1;
        ret.tv_nsec += OS_SYS_NS_PER_SECOND;
    }

    return ret;
}

/**
 * @brief   两个timespec64结构体相减（t1 - t2）
 * @param   t1  被减数时间结构体
 * @param   t2  减数时间结构体
 * @return  相减后的时间结构体
 */
STATIC INLINE struct timespec64 OsTimeSpecSub(const struct timespec64 t1, const struct timespec64 t2)
{
    struct timespec64 ret = {0};

    ret.tv_sec = t1.tv_sec - t2.tv_sec;
    ret.tv_nsec = t1.tv_nsec - t2.tv_nsec;
    if (ret.tv_nsec < 0) {                      // 纳秒数为负，需要借位
        ret.tv_sec -= 1;
        ret.tv_nsec += OS_SYS_NS_PER_SECOND;
    }

    return ret;
}

/**
 * @brief   获取硬件当前时间
 * @param   hwTime  用于存储硬件时间的结构体指针
 */
STATIC VOID OsGetHwTime(struct timespec64 *hwTime)
{
    UINT64 nowNsec;

    nowNsec = LOS_CurrNanosec();                // 获取当前纳秒数
    hwTime->tv_sec = nowNsec / OS_SYS_NS_PER_SECOND; // 计算秒数
    hwTime->tv_nsec = nowNsec - hwTime->tv_sec * OS_SYS_NS_PER_SECOND; // 计算剩余纳秒数
}

/**
 * @brief   设置系统时间（内部实现）
 * @param   tv  指向timeval64结构体的时间值
 * @param   tz  时区信息（未使用）
 * @return  成功返回0，失败返回错误码
 */
STATIC INT32 OsSetTimeOfDay(const struct timeval64 *tv, const struct timezone *tz)
{
    UINT32 intSave;
    struct timespec64 setTime = {0};
    struct timespec64 hwTime = {0};
    struct timespec64 realTime = {0};
    struct timespec64 tmp = {0};

#ifdef LOSCFG_SECURITY_CAPABILITY
    if (!IsCapPermit(CAP_SET_TIMEOFDAY)) {      // 检查是否有设置时间的权限
        TIME_RETURN(EPERM);
    }
#endif

    (VOID)tz;                                   // 未使用时区参数
    OsGetHwTime(&hwTime);                       // 获取硬件时间
    setTime.tv_sec = tv->tv_sec;
    setTime.tv_nsec = tv->tv_usec * OS_SYS_NS_PER_US; // 转换微秒为纳秒

    LOS_SpinLockSave(&g_timeSpin, &intSave);
    /* 停止正在进行的连续调整 */
    if (g_adjTimeLeft) {
        g_adjTimeLeft = 0;
    }
    realTime = OsTimeSpecAdd(hwTime, g_accDeltaFromAdj); // 计算当前实际时间
    realTime = OsTimeSpecAdd(realTime, g_accDeltaFromSet);

    tmp = OsTimeSpecSub(setTime, realTime);     // 计算需要调整的时间差
    g_accDeltaFromSet = OsTimeSpecAdd(g_accDeltaFromSet, tmp); // 更新非连续调整增量

    LOS_SpinUnlockRestore(&g_timeSpin, intSave);

    return 0;
}

/**
 * @brief   设置系统时间
 * @param   tv  指向timeval结构体的时间值
 * @param   tz  时区信息
 * @return  成功返回0，失败返回-1并设置errno
 */
int settimeofday(const struct timeval *tv, const struct timezone *tz)
{
    struct timeval64 stTimeVal64 = {0};

    if (!ValidTimeval(tv)) {                    // 验证timeval结构体有效性
        TIME_RETURN(EINVAL);
    }

    stTimeVal64.tv_sec = tv->tv_sec;
    stTimeVal64.tv_usec = tv->tv_usec;

    return OsSetTimeOfDay(&stTimeVal64, tz);    // 调用内部实现函数
}

#ifndef LOSCFG_AARCH64
/**
 * @brief   64位版本的设置系统时间函数
 * @param   tv  指向timeval64结构体的时间值
 * @param   tz  时区信息
 * @return  成功返回0，失败返回-1并设置errno
 */
int settimeofday64(const struct timeval64 *tv, const struct timezone *tz)
{
    if (!ValidTimeval64(tv)) {                  // 验证timeval64结构体有效性
        TIME_RETURN(EINVAL);
    }

    return OsSetTimeOfDay(tv, tz);              // 调用内部实现函数
}
#endif

/**
 * @brief   设置本地时间（秒级精度）
 * @param   seconds 秒数时间值
 * @return  成功返回0，失败返回-1并设置errno
 */
int setlocalseconds(int seconds)
{
    struct timeval tv = {0};

    tv.tv_sec = seconds;                        // 设置秒数
    tv.tv_usec = 0;                             // 微秒数为0

    return settimeofday(&tv, NULL);             // 调用settimeofday设置时间
}

/**
 * @brief   获取系统时间（内部实现）
 * @param   tv  用于存储时间值的timeval64结构体指针
 * @param   tz  时区信息（未使用）
 * @return  成功返回0，失败返回错误码
 */
STATIC INT32 OsGetTimeOfDay(struct timeval64 *tv, struct timezone *tz)
{
    UINT32 intSave;

    (VOID)tz;                                   // 未使用时区参数
    struct timespec64 hwTime = {0};
    struct timespec64 realTime = {0};

    OsGetHwTime(&hwTime);                       // 获取硬件时间

    LOS_SpinLockSave(&g_timeSpin, &intSave);
    realTime = OsTimeSpecAdd(hwTime, g_accDeltaFromAdj); // 计算当前实际时间（硬件时间+调整增量）
    realTime = OsTimeSpecAdd(realTime, g_accDeltaFromSet);
    LOS_SpinUnlockRestore(&g_timeSpin, intSave);

    tv->tv_sec = realTime.tv_sec;               // 秒数
    tv->tv_usec = realTime.tv_nsec / OS_SYS_NS_PER_US; // 纳秒转换为微秒

    if (tv->tv_sec < 0) {                       // 秒数不能为负
        TIME_RETURN(EINVAL);
    }
    return 0;
}

#ifndef LOSCFG_AARCH64
/**
 * @brief   64位版本的获取系统时间函数
 * @param   tv  用于存储时间值的timeval64结构体指针
 * @param   tz  时区信息
 * @return  成功返回0，失败返回-1并设置errno
 */
int gettimeofday64(struct timeval64 *tv, struct timezone *tz)
{
    if (tv == NULL) {                           // 参数检查
        TIME_RETURN(EINVAL);
    }

    return OsGetTimeOfDay(tv, tz);              // 调用内部实现函数
}
#endif

#ifdef LOSCFG_LIBC_NEWLIB
int gettimeofday(struct timeval *tv, void *_tz)
#else
/**
 * @brief   获取系统时间
 * @param   tv  用于存储时间值的timeval结构体指针
 * @param   tz  时区信息
 * @return  成功返回0，失败返回-1并设置errno
 */
int gettimeofday(struct timeval *tv, struct timezone *tz)
#endif
{
    struct timeval64 stTimeVal64 = {0};
#ifdef LOSCFG_LIBC_NEWLIB
    struct timezone *tz = (struct timezone *)_tz;
#endif

    if (tv == NULL) {                           // 参数检查
        TIME_RETURN(EINVAL);
    }

    if (OsGetTimeOfDay(&stTimeVal64, tz) == -1) { // 获取64位时间
        return -1;
    }

#ifdef LOSCFG_AARCH64
    tv->tv_sec = stTimeVal64.tv_sec;
    tv->tv_usec = stTimeVal64.tv_usec;
#else
    if (stTimeVal64.tv_sec > (long long)LONG_MAX) { // 检查秒数是否超出32位范围
        return -1;
    }
    tv->tv_sec = (time_t)stTimeVal64.tv_sec;   // 转换为32位秒数
    tv->tv_usec = (suseconds_t)stTimeVal64.tv_usec; // 转换为32位微秒数
#endif

    return 0;
}

/**
 * @brief   设置时钟时间
 * @param   clockID 时钟ID（仅支持CLOCK_REALTIME）
 * @param   tp      指向timespec结构体的时间值
 * @return  成功返回0，失败返回-1并设置errno
 */
int clock_settime(clockid_t clockID, const struct timespec *tp)
{
    struct timeval tv = {0};

    switch (clockID) {
        case CLOCK_REALTIME:
            /* 当前仅支持实时时钟 */
            break;
        case CLOCK_MONOTONIC_COARSE:
        case CLOCK_REALTIME_COARSE:
        case CLOCK_MONOTONIC_RAW:
        case CLOCK_PROCESS_CPUTIME_ID:
        case CLOCK_BOOTTIME:
        case CLOCK_REALTIME_ALARM:
        case CLOCK_BOOTTIME_ALARM:
        case CLOCK_TAI:
        case CLOCK_THREAD_CPUTIME_ID:
            TIME_RETURN(ENOTSUP);               // 不支持的时钟类型
        case CLOCK_MONOTONIC:
        default:
            TIME_RETURN(EINVAL);                // 无效的时钟ID
    }

    if (!ValidTimeSpec(tp)) {                   // 验证timespec结构体有效性
        TIME_RETURN(EINVAL);
    }

#ifdef LOSCFG_SECURITY_CAPABILITY
    if (!IsCapPermit(CAP_CLOCK_SETTIME)) {      // 检查是否有设置时钟的权限
        TIME_RETURN(EPERM);
    }
#endif

    tv.tv_sec = tp->tv_sec;
    tv.tv_usec = tp->tv_nsec / OS_SYS_NS_PER_US; // 纳秒转换为微秒
    return settimeofday(&tv, NULL);             // 通过settimeofday设置时间
}

#ifdef LOSCFG_KERNEL_CPUP
/**
 * @brief   从时钟ID获取线程ID
 * @param   clockID 时钟ID
 * @return  线程ID
 */
inline UINT32 GetTidFromClockID(clockid_t clockID)
{
    // 在musl/src/thread/pthread_getcpuclockid.c中，clockid = (-tid - 1) * 8 + 6
    UINT32 tid = -(clockID - 6) / 8 - 1; // 从clockID反推tid的运算（6、8、1为固定参数）
    return tid;
}

/**
 * @brief   从时钟ID获取进程ID
 * @param   clockID 时钟ID
 * @return  进程ID
 */
inline const pid_t GetPidFromClockID(clockid_t clockID)
{
    // 在musl/src/time/clock_getcpuclockid.c中，clockid = (-pid - 1) * 8 + 2
    const pid_t pid = -(clockID - 2) / 8 - 1; // 从clockID反推pid的运算（2、8、1为固定参数）
    return pid;
}

/**
 * @brief   获取线程CPU时间
 * @param   clockID 时钟ID
 * @param   ats     用于存储CPU时间的timespec结构体指针
 * @return  成功返回0，失败返回错误码
 */
static int PthreadGetCputime(clockid_t clockID, struct timespec *ats)
{
    uint64_t runtime;
    UINT32 intSave;
    UINT32 tid = GetTidFromClockID(clockID);
    if (OS_TID_CHECK_INVALID(tid)) {            // 检查线程ID有效性
        return -EINVAL;
    }

    LosTaskCB *task = OsGetTaskCB(tid);

    if (OsCurrTaskGet()->processCB != task->processCB) { // 检查是否为同一进程
        return -EINVAL;
    }

    SCHEDULER_LOCK(intSave);
    runtime = task->taskCpup.allTime;           // 获取线程总运行时间
    SCHEDULER_UNLOCK(intSave);

    ats->tv_sec = runtime / OS_SYS_NS_PER_SECOND; // 转换为秒
    ats->tv_nsec = runtime % OS_SYS_NS_PER_SECOND; // 剩余纳秒

    return 0;
}

/**
 * @brief   获取进程CPU时间
 * @param   clockID 时钟ID
 * @param   ats     用于存储CPU时间的timespec结构体指针
 * @return  成功返回0，失败返回错误码
 */
static int ProcessGetCputime(clockid_t clockID, struct timespec *ats)
{
    UINT64 runtime;
    UINT32 intSave;
    const pid_t pid = GetPidFromClockID(clockID);
    LosProcessCB *spcb = NULL;

    if (OsProcessIDUserCheckInvalid(pid) || pid < 0) { // 检查进程ID有效性
        return -EINVAL;
    }

    spcb = OS_PCB_FROM_PID(pid);
    if (OsProcessIsUnused(spcb)) {               // 检查进程是否已使用
        return -EINVAL;
    }

    SCHEDULER_LOCK(intSave);
    if (spcb->processCpup == NULL) {            // 检查进程CPU时间结构体是否存在
        SCHEDULER_UNLOCK(intSave);
        return -EINVAL;
    }
    runtime = spcb->processCpup->allTime;       // 获取进程总运行时间
    SCHEDULER_UNLOCK(intSave);

    ats->tv_sec = runtime / OS_SYS_NS_PER_SECOND; // 转换为秒
    ats->tv_nsec = runtime % OS_SYS_NS_PER_SECOND; // 剩余纳秒

    return 0;
}

/**
 * @brief   获取CPU时间（线程或进程）
 * @param   clockID 时钟ID
 * @param   tp      用于存储CPU时间的timespec结构体指针
 * @return  成功返回0，失败返回错误码
 */
static int GetCputime(clockid_t clockID, struct timespec *tp)
{
    int ret;

    if (clockID >= 0) {                         // CPU时钟ID必须为负数
        return -EINVAL;
    }

    if ((UINT32)clockID & CPUCLOCK_PERTHREAD_MASK) { // 检查是否为线程时钟
        ret = PthreadGetCputime(clockID, tp);
    } else {
        ret = ProcessGetCputime(clockID, tp);  // 进程时钟
    }

    return ret;
}

/**
 * @brief   检查CPU时钟有效性
 * @param   clockID 时钟ID
 * @return  成功返回0，失败返回错误码
 */
static int CheckClock(const clockid_t clockID)
{
    int error = 0;
    const pid_t pid = GetPidFromClockID(clockID);

    if (!((UINT32)clockID & CPUCLOCK_PERTHREAD_MASK)) { // 进程时钟检查
        LosProcessCB *spcb = NULL;
        if (OsProcessIDUserCheckInvalid(pid) || pid < 0) {
            return -EINVAL;
        }
        spcb = OS_PCB_FROM_PID(pid);
        if (OsProcessIsUnused(spcb)) {
            error = -EINVAL;
        }
    } else {
        error = -EINVAL;                        // 线程时钟暂不支持
    }

    return error;
}

/**
 * @brief   获取CPU时钟分辨率
 * @param   clockID 时钟ID
 * @param   tp      用于存储分辨率的timespec结构体指针
 * @return  成功返回0，失败返回错误码
 */
static int CpuClockGetres(const clockid_t clockID, struct timespec *tp)
{
    if (clockID > 0) {                          // CPU时钟ID必须为负数
        return -EINVAL;
    }

    int error = CheckClock(clockID);            // 检查时钟有效性
    if (!error) {
        error = ProcessGetCputime(clockID, tp); // 获取进程CPU时间作为分辨率
    }

    return error;
}
#endif

/**
 * @brief   获取时钟时间
 * @param   clockID 时钟ID
 * @param   tp      用于存储时间的timespec结构体指针
 * @return  成功返回0，失败返回-1并设置errno
 */
int clock_gettime(clockid_t clockID, struct timespec *tp)
{
    UINT32 intSave;
    struct timespec64 tmp = {0};
    struct timespec64 hwTime = {0};

    if (clockID > MAX_CLOCKS) {                 // 检查时钟ID是否超出范围
        goto ERROUT;
    }

    if (tp == NULL) {                           // 参数检查
        goto ERROUT;
    }

    OsGetHwTime(&hwTime);                       // 获取硬件时间

    switch (clockID) {
        case CLOCK_MONOTONIC_RAW:               // 原始单调时钟（不受调整影响）
#ifdef LOSCFG_TIME_CONTAINER
            tmp = OsTimeSpecAdd(hwTime, CLOCK_MONOTONIC_TIME_BASE);
            tp->tv_sec = tmp.tv_sec;
            tp->tv_nsec = tmp.tv_nsec;
#else
            tp->tv_sec = hwTime.tv_sec;
            tp->tv_nsec = hwTime.tv_nsec;
#endif
            break;
        case CLOCK_MONOTONIC:                   // 单调时钟（受连续调整影响）
            LOS_SpinLockSave(&g_timeSpin, &intSave);
            tmp = OsTimeSpecAdd(hwTime, g_accDeltaFromAdj); // 硬件时间+连续调整增量
            LOS_SpinUnlockRestore(&g_timeSpin, intSave);
#ifdef LOSCFG_TIME_CONTAINER
            tmp = OsTimeSpecAdd(tmp, CLOCK_MONOTONIC_TIME_BASE);
#endif
            tp->tv_sec = tmp.tv_sec;
            tp->tv_nsec = tmp.tv_nsec;
            break;
        case CLOCK_REALTIME:                    // 实时时钟（受所有调整影响）
            LOS_SpinLockSave(&g_timeSpin, &intSave);
            tmp = OsTimeSpecAdd(hwTime, g_accDeltaFromAdj); // 硬件时间+连续调整增量
            tmp = OsTimeSpecAdd(tmp, g_accDeltaFromSet);    // +非连续调整增量
            LOS_SpinUnlockRestore(&g_timeSpin, intSave);
            tp->tv_sec = tmp.tv_sec;
            tp->tv_nsec = tmp.tv_nsec;
            break;
        case CLOCK_MONOTONIC_COARSE:
        case CLOCK_REALTIME_COARSE:
        case CLOCK_THREAD_CPUTIME_ID:
        case CLOCK_PROCESS_CPUTIME_ID:
        case CLOCK_BOOTTIME:
        case CLOCK_REALTIME_ALARM:
        case CLOCK_BOOTTIME_ALARM:
        case CLOCK_TAI:
            TIME_RETURN(ENOTSUP);               // 不支持的时钟类型
        default:
        {
#ifdef LOSCFG_KERNEL_CPUP
            int ret = GetCputime(clockID, tp);  // 获取CPU时间（线程/进程）
                TIME_RETURN(-ret);
#else
            TIME_RETURN(EINVAL);                // 无效的时钟ID
#endif
        }
    }

    return 0;

ERROUT:
    TIME_RETURN(EINVAL);
}

/**
 * @brief   获取时钟分辨率
 * @param   clockID 时钟ID
 * @param   tp      用于存储分辨率的timespec结构体指针
 * @return  成功返回0，失败返回-1并设置errno
 */
int clock_getres(clockid_t clockID, struct timespec *tp)
{
    if (tp == NULL) {                           // 参数检查
        TIME_RETURN(EINVAL);
    }

    switch (clockID) {
        case CLOCK_MONOTONIC_RAW:
        case CLOCK_MONOTONIC:
        case CLOCK_REALTIME:
            /* 可访问的RTC分辨率 */
            tp->tv_nsec = OS_SYS_NS_PER_US;     /* clock_gettime的精度为1微秒 */
            tp->tv_sec = 0;
            break;
        case CLOCK_MONOTONIC_COARSE:
        case CLOCK_REALTIME_COARSE:
            /* 粗略时钟分辨率，由vdso支持
             * clock_gettime的精度为1个时钟周期 */
            tp->tv_nsec = OS_SYS_NS_PER_SECOND / LOSCFG_BASE_CORE_TICK_PER_SECOND;
            tp->tv_sec = 0;
            break;
        case CLOCK_THREAD_CPUTIME_ID:
        case CLOCK_PROCESS_CPUTIME_ID:
        case CLOCK_BOOTTIME:
        case CLOCK_REALTIME_ALARM:
        case CLOCK_BOOTTIME_ALARM:
        case CLOCK_TAI:
            TIME_RETURN(ENOTSUP);               // 不支持的时钟类型
        default:
#ifdef LOSCFG_KERNEL_CPUP
            {                                   // CPU时钟分辨率
                int ret = CpuClockGetres(clockID, tp);
                TIME_RETURN(-ret);
            }
#else
            TIME_RETURN(EINVAL);                // 无效的时钟ID
#endif
    }

    TIME_RETURN(0);
}
/**
 * @brief 按指定时钟类型进行纳秒级睡眠
 * @param clk 时钟类型
 * @param flags 标志位（0表示相对时间，TIMER_ABSTIME表示绝对时间）
 * @param req 请求的睡眠时间
 * @param rem 剩余未睡眠的时间（未实现）
 * @return 成功返回0，失败返回错误码
 */
int clock_nanosleep(clockid_t clk, int flags, const struct timespec *req, struct timespec *rem)
{
    switch (clk) {
        case CLOCK_REALTIME:  // 实时时钟
            if (flags == 0) {  // 仅支持相对时间模式
                /* we only support the realtime clock currently */
                return nanosleep(req, rem);  // 调用nanosleep实现
            }
            /* fallthrough */
        case CLOCK_MONOTONIC_COARSE:  // 粗粒度单调时钟
        case CLOCK_REALTIME_COARSE:   // 粗粒度实时时钟
        case CLOCK_MONOTONIC_RAW:     // 原始单调时钟
        case CLOCK_MONOTONIC:         // 单调时钟
        case CLOCK_PROCESS_CPUTIME_ID:// 进程CPU时间时钟
        case CLOCK_BOOTTIME:          // 系统启动时间时钟
        case CLOCK_REALTIME_ALARM:    // 实时闹钟时钟
        case CLOCK_BOOTTIME_ALARM:    // 启动时间闹钟时钟
        case CLOCK_TAI:               // TAI时钟
            if (flags == 0 || flags == TIMER_ABSTIME) {  // 不支持这些时钟类型的睡眠
                TIME_RETURN(ENOTSUP);  // 返回不支持错误
            }
            /* fallthrough */
        case CLOCK_THREAD_CPUTIME_ID: // 线程CPU时间时钟
        default:  // 无效时钟类型
            TIME_RETURN(EINVAL);  // 返回参数无效错误
    }

    TIME_RETURN(0);  // 正常返回（理论上不会执行到此处）
}

/**
 * @brief 软件定时器回调参数结构体
 */
typedef struct {
    int sigev_signo;       // 信号编号
    pid_t pid;             // 进程ID
    unsigned int tid;      // 线程ID
    union sigval sigev_value;  // 信号值
} swtmr_proc_arg;

/**
 * @brief 软件定时器回调函数
 * @param tmrArg 定时器参数（swtmr_proc_arg类型指针）
 */
static VOID SwtmrProc(UINTPTR tmrArg)
{
#ifdef LOSCFG_KERNEL_VM  // 仅在启用VM配置时编译
    INT32 sig, ret;        // 信号编号和返回值
    UINT32 intSave;        // 中断保存标志
    pid_t pid;             // 进程ID
    siginfo_t info;        // 信号信息结构体
    LosTaskCB *stcb = NULL;  // 任务控制块指针

    swtmr_proc_arg *arg = (swtmr_proc_arg *)tmrArg;  // 转换参数类型
    OS_GOTO_EXIT_IF(arg == NULL, EINVAL);  // 参数校验：若参数为空则跳转到EXIT

    sig = arg->sigev_signo;  // 获取信号编号
    pid = arg->pid;          // 获取进程ID
    OS_GOTO_EXIT_IF(!GOOD_SIGNO(sig), EINVAL);  // 校验信号编号有效性

    /* Create the siginfo structure */
    info.si_signo = sig;                       // 设置信号编号
    info.si_code = SI_TIMER;                   // 设置信号代码为定时器
    info.si_value.sival_ptr = arg->sigev_value.sival_ptr;  // 设置信号值

    /* Send signals to threads or processes */
    if (arg->tid > 0) {  // 若指定了线程ID，则发送信号给线程
        /* Make sure that the para is valid */
        OS_GOTO_EXIT_IF(OS_TID_CHECK_INVALID(arg->tid), EINVAL);  // 校验线程ID有效性
        stcb = OsGetTaskCB(arg->tid);  // 获取线程控制块
        ret = OsUserProcessOperatePermissionsCheck(stcb, stcb->processCB);  // 权限检查
        OS_GOTO_EXIT_IF(ret != LOS_OK, -ret);  // 权限检查失败则退出

        /* Dispatch the signal to thread, bypassing normal task group thread
         * dispatch rules. */
        SCHEDULER_LOCK(intSave);  // 关闭调度器
        ret = OsTcbDispatch(stcb, &info);  // 向线程发送信号
        SCHEDULER_UNLOCK(intSave);  // 开启调度器
        OS_GOTO_EXIT_IF(ret != LOS_OK, -ret);  // 信号发送失败则退出
    } else {  // 否则发送信号给进程
        /* Make sure that the para is valid */
        OS_GOTO_EXIT_IF(pid <= 0 || OS_PID_CHECK_INVALID(pid), EINVAL);  // 校验进程ID有效性
        /* Dispatch the signal to process */
        SCHEDULER_LOCK(intSave);  // 关闭调度器
        OsDispatch(pid, &info, OS_USER_KILL_PERMISSION);  // 向进程发送信号
        SCHEDULER_UNLOCK(intSave);  // 开启调度器
    }
    return;  // 正常返回
EXIT:  // 错误处理标签
    PRINT_ERR("Dispatch signals failed!, ret: %d\r\n", ret);  // 打印错误信息
#endif
    return;  // 未启用VM配置时直接返回
}

/**
 * @brief 创建POSIX定时器
 * @param clockID 时钟类型（仅支持CLOCK_REALTIME）
 * @param evp 信号事件结构体
 * @param timerID 输出参数，返回创建的定时器ID
 * @return 成功返回0，失败返回-1并设置errno
 */
int timer_create(clockid_t clockID, struct sigevent *restrict evp, timer_t *restrict timerID)
{
    UINT32 ret;           // 返回值
    UINT16 swtmrID;       // 软件定时器ID
#ifdef LOSCFG_SECURITY_VID  // 若启用安全VID
    UINT16 vid;           // 虚拟ID
#endif

    if (!timerID || (clockID != CLOCK_REALTIME) || !evp) {  // 参数校验
        errno = EINVAL;   // 设置参数无效错误
        return -1;        // 返回失败
    }

    if ((evp->sigev_notify != SIGEV_THREAD) || evp->sigev_notify_attributes) {  // 仅支持线程通知方式
        errno = ENOTSUP;  // 设置不支持错误
        return -1;        // 返回失败
    }

    // 创建一次性软件定时器
    ret = LOS_SwtmrCreate(1, LOS_SWTMR_MODE_ONCE, (SWTMR_PROC_FUNC)evp->sigev_notify_function,
                          &swtmrID, (UINTPTR)evp->sigev_value.sival_ptr);
    if (ret != LOS_OK) {  // 定时器创建失败
        errno = (ret == LOS_ERRNO_SWTMR_MAXSIZE) ? EAGAIN : EINVAL;  // 设置错误码
        return -1;        // 返回失败
    }

#ifdef LOSCFG_SECURITY_VID  // 若启用安全VID
    vid = AddNodeByRid(swtmrID);  // 添加节点获取虚拟ID
    if (vid == MAX_INVALID_TIMER_VID) {  // 虚拟ID无效
        (VOID)LOS_SwtmrDelete(swtmrID);  // 删除已创建的定时器
        return -1;  // 返回失败
    }
    swtmrID = vid;  // 使用虚拟ID
#endif
    *timerID = (timer_t)(UINTPTR)swtmrID;  // 设置定时器ID
    return 0;  // 成功返回
}

/**
 * @brief 内部定时器创建函数
 * @param clockID 时钟类型（仅支持CLOCK_REALTIME）
 * @param evp 内核信号事件结构体
 * @param timerID 输出参数，返回创建的定时器ID
 * @return 成功返回0，失败返回-1并设置errno
 */
int OsTimerCreate(clockid_t clockID, struct ksigevent *evp, timer_t *timerID)
{
    UINT32 intSave;        // 中断保存标志
    UINT32 ret;            // 返回值
    UINT16 swtmrID;        // 软件定时器ID
    swtmr_proc_arg *arg = NULL;  // 定时器回调参数
    int signo;             // 信号编号
#ifdef LOSCFG_SECURITY_VID  // 若启用安全VID
    UINT16 vid;            // 虚拟ID
#endif

    signo = evp ? evp->sigev_signo : SIGALRM;  // 获取信号编号，默认为SIGALRM
    // 参数校验：时钟类型、定时器ID指针、信号编号范围
    if ((clockID != CLOCK_REALTIME) || (timerID == NULL) || (signo > SIGRTMAX) || (signo < 1)) {
        errno = EINVAL;   // 设置参数无效错误
        return -1;        // 返回失败
    }

    // 校验通知方式是否支持
    if (evp && (evp->sigev_notify != SIGEV_SIGNAL && evp->sigev_notify != SIGEV_THREAD_ID)) {
        errno = ENOTSUP;  // 设置不支持错误
        return -1;        // 返回失败
    }

    LOS_SpinLockSave(&g_timeSpin, &intSave);  // 加自旋锁
    // 分配定时器回调参数内存
    if ((arg = (swtmr_proc_arg *)malloc(sizeof(swtmr_proc_arg))) == NULL) {
        LOS_SpinUnlockRestore(&g_timeSpin, intSave);  // 解锁
        errno = ENOMEM;  // 设置内存不足错误
        return -1;       // 返回失败
    }

    arg->tid = evp ? evp->sigev_tid : 0;          // 设置线程ID
    arg->sigev_signo = signo;                     // 设置信号编号
    arg->pid = LOS_GetCurrProcessID();            // 获取当前进程ID
    arg->sigev_value.sival_ptr = evp ? evp->sigev_value.sival_ptr : NULL;  // 设置信号值
    // 创建一次性软件定时器，回调函数为SwtmrProc
    if ((ret = LOS_SwtmrCreate(1, LOS_SWTMR_MODE_ONCE, SwtmrProc, &swtmrID, (UINTPTR)arg)) != LOS_OK) {
        errno = (ret == LOS_ERRNO_SWTMR_MAXSIZE) ? EAGAIN : EINVAL;  // 设置错误码
        free(arg);  // 释放参数内存
        LOS_SpinUnlockRestore(&g_timeSpin, intSave);  // 解锁
        return -1;  // 返回失败
    }

#ifdef LOSCFG_SECURITY_VID  // 若启用安全VID
    if ((vid = AddNodeByRid(swtmrID)) == MAX_INVALID_TIMER_VID) {  // 获取虚拟ID失败
        free(arg);  // 释放参数内存
        (VOID)LOS_SwtmrDelete(swtmrID);  // 删除定时器
        LOS_SpinUnlockRestore(&g_timeSpin, intSave);  // 解锁
        return -1;  // 返回失败
    }
    swtmrID = vid;  // 使用虚拟ID
#endif
    *timerID = (timer_t)(UINTPTR)swtmrID;  // 设置定时器ID
    LOS_SpinUnlockRestore(&g_timeSpin, intSave);  // 解锁
    return 0;  // 成功返回
}

/**
 * @brief 删除POSIX定时器
 * @param timerID 要删除的定时器ID
 * @return 成功返回0，失败返回-1并设置errno
 */
int timer_delete(timer_t timerID)
{
    UINT16 swtmrID = (UINT16)(UINTPTR)timerID;  // 转换为软件定时器ID
    UINT32 intSave;        // 中断保存标志
    VOID *arg = NULL;      // 定时器参数
    UINTPTR swtmrProc;     // 定时器回调函数

#ifdef LOSCFG_SECURITY_VID  // 若启用安全VID
    swtmrID = GetRidByVid(swtmrID);  // 获取真实ID
#endif
    if (OS_INT_ACTIVE || !ValidTimerID(swtmrID)) {  // 中断中或定时器ID无效
        goto ERROUT;  // 跳转到错误处理
    }

    LOS_SpinLockSave(&g_timeSpin, &intSave);  // 加自旋锁
    arg = (VOID *)OS_SWT_FROM_SID(swtmrID)->uwArg;  // 获取定时器参数
    swtmrProc = (UINTPTR)OS_SWT_FROM_SID(swtmrID)->pfnHandler;  // 获取回调函数
    if (LOS_SwtmrDelete(swtmrID)) {  // 删除定时器失败
        LOS_SpinUnlockRestore(&g_timeSpin, intSave);  // 解锁
        goto ERROUT;  // 跳转到错误处理
    }
    // 如果是SwtmrProc回调且参数不为空，则释放参数内存
    if ((swtmrProc == (UINTPTR)SwtmrProc) && (arg != NULL)) {
        free(arg);  // 释放内存
    }
    LOS_SpinUnlockRestore(&g_timeSpin, intSave);  // 解锁

#ifdef LOSCFG_SECURITY_VID  // 若启用安全VID
    RemoveNodeByVid((UINT16)(UINTPTR)timerID);  // 移除虚拟ID节点
#endif
    return 0;  // 成功返回

ERROUT:  // 错误处理标签
    errno = EINVAL;  // 设置参数无效错误
    return -1;       // 返回失败
}

/**
 * @brief 设置定时器的超时时间和间隔
 * @param timerID 定时器ID
 * @param flags 标志位（未支持）
 * @param value 新的定时器参数（包含超时时间和间隔）
 * @param oldValue 输出参数，返回旧的定时器参数（未实现）
 * @return 成功返回0，失败返回-1并设置errno
 */
int timer_settime(timer_t timerID, int flags,
                  const struct itimerspec *value,   /* new value */
                  struct itimerspec *oldValue)      /* old value to return, always 0 */
{
    UINT16 swtmrID = (UINT16)(UINTPTR)timerID;  // 转换为软件定时器ID
    SWTMR_CTRL_S *swtmr = NULL;  // 软件定时器控制块
    UINT32 interval, expiry, ret;  // 间隔、超时时间（ ticks ）和返回值
    UINT32 intSave;  // 中断保存标志

    if (flags != 0) {  // 标志位不为0（未支持）
        /* flags not supported currently */
        errno = ENOSYS;  // 设置不支持系统调用错误
        return -1;       // 返回失败
    }

#ifdef LOSCFG_SECURITY_VID  // 若启用安全VID
    swtmrID = GetRidByVid(swtmrID);  // 获取真实ID
#endif
    // 参数校验：value不为空、不在中断中、定时器ID有效
    if ((value == NULL) || OS_INT_ACTIVE || !ValidTimerID(swtmrID)) {
        errno = EINVAL;  // 设置参数无效错误
        return -1;       // 返回失败
    }

    // 校验超时时间和间隔的有效性
    if (!ValidTimeSpec(&value->it_value) || !ValidTimeSpec(&value->it_interval)) {
        errno = EINVAL;  // 设置参数无效错误
        return -1;       // 返回失败
    }

    if (oldValue) {  // 若需要返回旧值
        (VOID)timer_gettime(timerID, oldValue);  // 获取旧值
    }

    swtmr = OS_SWT_FROM_SID(swtmrID);  // 获取定时器控制块
    ret = LOS_SwtmrStop(swtmr->usTimerID);  // 停止定时器
    // 若停止失败且不是因为定时器未启动
    if ((ret != LOS_OK) && (ret != LOS_ERRNO_SWTMR_NOT_STARTED)) {
        errno = EINVAL;  // 设置参数无效错误
        return -1;       // 返回失败
    }

    expiry = OsTimeSpec2Tick(&value->it_value);    // 将超时时间转换为ticks
    interval = OsTimeSpec2Tick(&value->it_interval);  // 将间隔转换为ticks

    LOS_SpinLockSave(&g_swtmrSpin, &intSave);  // 加自旋锁
    // 设置定时器模式：有间隔则为周期模式，否则为单次不自动删除模式
    swtmr->ucMode = interval ? LOS_SWTMR_MODE_OPP : LOS_SWTMR_MODE_NO_SELFDELETE;
    swtmr->uwExpiry = expiry + !!expiry; // PS: skip the first tick because it is NOT a full tick.
    swtmr->uwInterval = interval;  // 设置间隔
    swtmr->uwOverrun = 0;          // 重置溢出计数
    LOS_SpinUnlockRestore(&g_swtmrSpin, intSave);  // 解锁

    // 若超时时间为0（停止定时器）
    if ((value->it_value.tv_sec == 0) && (value->it_value.tv_nsec == 0)) {
        /*
         * 1) when expiry is 0, means timer should be stopped.
         * 2) If timer is ticking, stopping timer is already done before.
         * 3) If timer is created but not ticking, return 0 as well.
         */
        return 0;  // 直接返回成功
    }

    if (LOS_SwtmrStart(swtmr->usTimerID)) {  // 启动定时器失败
        errno = EINVAL;  // 设置参数无效错误
        return -1;       // 返回失败
    }

    return 0;  // 成功返回
}

/**
 * @brief 获取定时器的当前设置
 * @param timerID 定时器ID
 * @param value 输出参数，返回定时器当前参数
 * @return 成功返回0，失败返回-1并设置errno
 */
int timer_gettime(timer_t timerID, struct itimerspec *value)
{
    UINT32 tick = 0;       // 剩余ticks
    SWTMR_CTRL_S *swtmr = NULL;  // 软件定时器控制块
    UINT16 swtmrID = (UINT16)(UINTPTR)timerID;  // 转换为软件定时器ID
    UINT32 ret;  // 返回值

#ifdef LOSCFG_SECURITY_VID  // 若启用安全VID
    swtmrID = GetRidByVid(swtmrID);  // 获取真实ID
#endif
    if ((value == NULL) || !ValidTimerID(swtmrID)) {  // 参数校验
        errno = EINVAL;  // 设置参数无效错误
        return -1;       // 返回失败
    }

    swtmr = OS_SWT_FROM_SID(swtmrID);  // 获取定时器控制块

    /* get expire time */
    ret = LOS_SwtmrTimeGet(swtmr->usTimerID, &tick);  // 获取剩余ticks
    // 若获取失败且不是因为定时器未启动
    if ((ret != LOS_OK) && (ret != LOS_ERRNO_SWTMR_NOT_STARTED)) {
        errno = EINVAL;  // 设置参数无效错误
        return -1;       // 返回失败
    }

    OsTick2TimeSpec(&value->it_value, tick);  // 将剩余ticks转换为timespec
    // 根据定时器模式设置间隔（单次模式间隔为0）
    OsTick2TimeSpec(&value->it_interval, (swtmr->ucMode == LOS_SWTMR_MODE_ONCE) ? 0 : swtmr->uwInterval);
    return 0;  // 成功返回
}

/**
 * @brief 获取定时器溢出计数
 * @param timerID 定时器ID
 * @return 成功返回溢出计数，失败返回-1并设置errno
 */
int timer_getoverrun(timer_t timerID)
{
    UINT16 swtmrID = (UINT16)(UINTPTR)timerID;  // 转换为软件定时器ID
    SWTMR_CTRL_S *swtmr = NULL;  // 软件定时器控制块
    INT32 overRun;  // 溢出计数

#ifdef LOSCFG_SECURITY_VID  // 若启用安全VID
    swtmrID = GetRidByVid(swtmrID);  // 获取真实ID
#endif
    if (!ValidTimerID(swtmrID)) {  // 定时器ID无效
        errno = EINVAL;  // 设置参数无效错误
        return -1;       // 返回失败
    }

    swtmr = OS_SWT_FROM_SID(swtmrID);  // 获取定时器控制块
    if (swtmr->usTimerID >= OS_SWTMR_MAX_TIMERID) {  // 定时器ID超出范围
        errno = EINVAL;  // 设置参数无效错误
        return -1;       // 返回失败
    }

    overRun = (INT32)(swtmr->uwOverrun);  // 获取溢出计数
    // 溢出计数超过最大值则返回最大值
    return (overRun > DELAYTIMER_MAX) ? DELAYTIMER_MAX : overRun;
}

/**
 * @brief 内部纳秒睡眠实现函数
 * @param nanoseconds 要睡眠的纳秒数
 * @return 成功返回0，失败返回-1
 */
STATIC INT32 DoNanoSleep(UINT64 nanoseconds)
{
    UINT32 ret;  // 返回值

    ret = LOS_TaskDelay(OsNS2Tick(nanoseconds));  // 转换为ticks并延迟
    // 若成功或因任务不足无法切换（仍视为成功）
    if (ret == LOS_OK || ret == LOS_ERRNO_TSK_YIELD_NOT_ENOUGH_TASK) {
        return 0;  // 返回成功
    }
    return -1;  // 返回失败
}

#ifdef LOSCFG_LIBC_NEWLIB
int usleep(unsigned long useconds)  // newlib库版本声明
#else
int usleep(unsigned useconds)       // 标准版本声明
#endif
{
    return DoNanoSleep((UINT64)useconds * OS_SYS_NS_PER_US);  // 转换为纳秒并调用DoNanoSleep
}

/**
 * @brief 纳秒级睡眠函数
 * @param rqtp 请求的睡眠时间
 * @param rmtp 剩余未睡眠的时间（未实现）
 * @return 成功返回0，失败返回-1并设置errno
 */
int nanosleep(const struct timespec *rqtp, struct timespec *rmtp)
{
    UINT64 nanoseconds;  // 总纳秒数
    INT32 ret = -1;      // 返回值

    (VOID)rmtp;  // 未使用参数
    /* expire time */

    if (!ValidTimeSpec(rqtp)) {  // 校验睡眠时间有效性
        errno = EINVAL;  // 设置参数无效错误
        return ret;      // 返回失败
    }

    // 计算总纳秒数
    nanoseconds = (UINT64)rqtp->tv_sec * OS_SYS_NS_PER_SECOND + rqtp->tv_nsec;

    return DoNanoSleep(nanoseconds);  // 调用内部实现
}

/**
 * @brief 秒级睡眠函数
 * @param seconds 要睡眠的秒数
 * @return 成功返回0，失败返回-1
 */
unsigned int sleep(unsigned int seconds)
{
    return DoNanoSleep((UINT64)seconds * OS_SYS_NS_PER_SECOND);  // 转换为纳秒并调用DoNanoSleep
}

/**
 * @brief 计算两个时间的差值
 * @param time2 结束时间
 * @param time1 开始时间
 * @return 时间差（秒）
 */
double difftime(time_t time2, time_t time1)
{
    return (double)(time2 - time1);  // 返回两个时间的差值
}

/**
 * @brief 获取进程使用的CPU时间
 * @return CPU时间（时钟滴答数）
 */
clock_t clock(VOID)
{
    clock_t clockMsec;  // 时钟滴答数
    UINT64 nowNsec;     // 当前纳秒数

    nowNsec = LOS_CurrNanosec();  // 获取当前纳秒数
    // 转换为时钟滴答数（CLOCKS_PER_SEC为每秒滴答数）
    clockMsec = (clock_t)(nowNsec / (OS_SYS_NS_PER_SECOND / CLOCKS_PER_SEC));

    return clockMsec;  // 返回时钟滴答数
}

/**
 * @brief 获取进程和子进程的CPU时间（未实现）
 * @param buf tms结构体指针
 * @return -1并设置errno为ENOSYS
 */
clock_t times(struct tms *buf)
{
    clock_t clockTick = -1;  // 返回值

    (void)buf;  // 未使用参数
    set_errno(ENOSYS);  // 设置不支持系统调用错误

    return clockTick;  // 返回-1
}

/**
 * @brief 设置间隔定时器
 * @param which 定时器类型（仅支持ITIMER_REAL）
 * @param value 新的定时器参数
 * @param ovalue 输出参数，返回旧的定时器参数
 * @return 成功返回0，失败返回-1并设置errno
 */
int setitimer(int which, const struct itimerval *value, struct itimerval *ovalue)
{
    UINT32 intSave;  // 中断保存标志
    LosProcessCB *processCB = OsCurrProcessGet();  // 当前进程控制块
    timer_t timerID = 0;  // 定时器ID
    struct itimerspec spec;  // 转换后的timespec参数
    struct itimerspec ospec; // 旧的timespec参数
    int ret = LOS_OK;  // 返回值

    /* we only support the realtime clock timer currently */
    if (which != ITIMER_REAL || !value) {  // 参数校验
        set_errno(EINVAL);  // 设置参数无效错误
        return -1;          // 返回失败
    }

    /* To avoid creating an invalid timer after the timer has already been create */
    // 若进程定时器ID无效，则创建新定时器
    if (processCB->timerID == (timer_t)(UINTPTR)MAX_INVALID_TIMER_VID) {
        ret = OsTimerCreate(CLOCK_REALTIME, NULL, &timerID);  // 创建定时器
        if (ret != LOS_OK) {  // 创建失败
            return ret;       // 返回失败
        }
    }

    /* The initialization of this global timer must be in spinlock
     * OsTimerCreate cannot be located in spinlock. */
    SCHEDULER_LOCK(intSave);  // 关闭调度器
    if (processCB->timerID == (timer_t)(UINTPTR)MAX_INVALID_TIMER_VID) {
        processCB->timerID = timerID;  // 设置进程定时器ID
        SCHEDULER_UNLOCK(intSave);     // 开启调度器
    } else {
        SCHEDULER_UNLOCK(intSave);     // 开启调度器
        if (timerID) {  // 若已创建新定时器但进程已有有效定时器
            timer_delete(timerID);  // 删除新创建的定时器
        }
    }

    // 校验定时器参数有效性
    if (!ValidTimeval(&value->it_value) || !ValidTimeval(&value->it_interval)) {
        set_errno(EINVAL);  // 设置参数无效错误
        return -1;          // 返回失败
    }

    TIMEVAL_TO_TIMESPEC(&value->it_value, &spec.it_value);    // 转换超时时间格式
    TIMEVAL_TO_TIMESPEC(&value->it_interval, &spec.it_interval);  // 转换间隔格式

    // 设置定时器时间
    ret = timer_settime(processCB->timerID, 0, &spec, ovalue ? &ospec : NULL);
    if (ret == LOS_OK && ovalue) {  // 若成功且需要返回旧值
        TIMESPEC_TO_TIMEVAL(&ovalue->it_value, &ospec.it_value);    // 转换旧超时时间
        TIMESPEC_TO_TIMEVAL(&ovalue->it_interval, &ospec.it_interval);  // 转换旧间隔
    }

    return ret;  // 返回结果
}

/**
 * @brief 获取间隔定时器当前设置
 * @param which 定时器类型（仅支持ITIMER_REAL）
 * @param value 输出参数，返回定时器当前参数
 * @return 成功返回0，失败返回-1并设置errno
 */
int getitimer(int which, struct itimerval *value)
{
    LosProcessCB *processCB = OsCurrProcessGet();  // 当前进程控制块
    struct itimerspec spec = {};  // 临时timespec结构体

    int ret = LOS_OK;  // 返回值

    /* we only support the realtime clock timer currently */
    if (which != ITIMER_REAL || !value) {  // 参数校验
        set_errno(EINVAL);  // 设置参数无效错误
        return -1;          // 返回失败
    }

    // 若进程定时器ID有效，则获取定时器时间
    if (processCB->timerID != (timer_t)(UINTPTR)MAX_INVALID_TIMER_VID) {
        ret = timer_gettime(processCB->timerID, &spec);  // 获取定时器设置
    }

    if (ret == LOS_OK) {  // 若成功获取
        TIMESPEC_TO_TIMEVAL(&value->it_value, &spec.it_value);    // 转换超时时间格式
        TIMESPEC_TO_TIMEVAL(&value->it_interval, &spec.it_interval);  // 转换间隔格式
    }

    return ret;  // 返回结果
}

#ifdef LOSCFG_KERNEL_VDSO  // 若启用VDSO
/**
 * @brief 获取VDSO时间数据
 * @param vdsoDataPage VDSO数据页指针
 */
VOID OsVdsoTimeGet(VdsoDataPage *vdsoDataPage)
{
    UINT32 intSave;  // 中断保存标志
    struct timespec64 tmp = {0};  // 临时时间结构体
    struct timespec64 hwTime = {0};  // 硬件时间

    if (vdsoDataPage == NULL) {  // 参数校验
        return;  // 直接返回
    }

    OsGetHwTime(&hwTime);  // 获取硬件时间

    LOS_SpinLockSave(&g_timeSpin, &intSave);  // 加自旋锁
    tmp = OsTimeSpecAdd(hwTime, g_accDeltaFromAdj);  // 硬件时间加上调整增量
    vdsoDataPage->monoTimeSec = tmp.tv_sec;    // 设置单调时间秒数
    vdsoDataPage->monoTimeNsec = tmp.tv_nsec;  // 设置单调时间纳秒数

    tmp = OsTimeSpecAdd(tmp, g_accDeltaFromSet);  // 加上设置增量
    vdsoDataPage->realTimeSec = tmp.tv_sec;    // 设置实时时间秒数
    vdsoDataPage->realTimeNsec = tmp.tv_nsec;  // 设置实时时间纳秒数
    LOS_SpinUnlockRestore(&g_timeSpin, intSave);  // 解锁
}
#endif

/**
 * @brief 获取当前时间
 * @param t 输出参数，返回当前时间戳（秒）
 * @return 成功返回当前时间戳，失败返回OS_ERROR
 */
time_t time(time_t *t)
{
    struct timeval tp;  // 时间结构体
    int ret;            // 返回值

    /* Get the current time from the system */
    ret = gettimeofday(&tp, (struct timezone *)NULL);  // 获取当前时间
    if (ret == LOS_OK) {  // 成功获取
        /* Return the seconds since the epoch */
        if (t) {  // 若t不为空
            *t = tp.tv_sec;  // 设置输出参数
        }
        return tp.tv_sec;  // 返回秒数
    }
    return (time_t)OS_ERROR;  // 返回错误
}