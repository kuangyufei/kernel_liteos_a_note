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
 * This two structures originally did't exit,
 * they added by liteos to support 64bit interfaces on 32bit platform,
 * in 64bit platform, timeval64 define to timeval which is platform adaptive.
 */
#define timeval64 timeval
#define timespec64 timespec
#endif

STATIC INLINE BOOL ValidTimeval(const struct timeval *tv)
{
    /* Fail a NULL pointer */
    if (tv == NULL) {
        return FALSE;
    }

    /* Fail illegal microseconds values */
    if ((tv->tv_usec < 0) || (tv->tv_usec >= OS_SYS_US_PER_SECOND) || (tv->tv_sec < 0)) {
        return FALSE;
    }

    return TRUE;
}

STATIC INLINE BOOL ValidTimeval64(const struct timeval64 *tv)
{
    /* Fail a NULL pointer */
    if (tv == NULL) {
        return FALSE;
    }

    /* Fail illegal microseconds values */
    if ((tv->tv_usec < 0) || (tv->tv_usec >= OS_SYS_US_PER_SECOND) || (tv->tv_sec < 0)) {
        return FALSE;
    }

    return TRUE;
}

STATIC INLINE BOOL ValidTimerID(UINT16 swtmrID)
{
    /* check timer id */
    if (swtmrID >= OS_SWTMR_MAX_TIMERID) {
        return FALSE;
    }

    /* check owner of this timer */
    if (OS_SWT_FROM_SID(swtmrID)->uwOwnerPid != LOS_GetCurrProcessID()) {
        return FALSE;
    }

    return TRUE;
}

STATIC SPIN_LOCK_INIT(g_timeSpin);
STATIC long long g_adjTimeLeft; /* absolute value of adjtime */
STATIC INT32 g_adjDirection;    /* 1, speed up; 0, slow down; */

/* Adjust pacement, nanoseconds per SCHED_CLOCK_INTETRVAL_TICKS ticks */
STATIC const long long g_adjPacement = (((LOSCFG_BASE_CORE_ADJ_PER_SECOND * SCHED_CLOCK_INTETRVAL_TICKS) /
                                        LOSCFG_BASE_CORE_TICK_PER_SECOND) * OS_SYS_NS_PER_US);

/* accumulative time delta from continuous modify, such as adjtime */
STATIC struct timespec64 g_accDeltaFromAdj;
/* accumulative time delta from discontinuous modify, such as settimeofday */
STATIC struct timespec64 g_accDeltaFromSet;

VOID OsAdjTime(VOID)
{
    UINT32 intSave;

    LOS_SpinLockSave(&g_timeSpin, &intSave);
    if (!g_adjTimeLeft) {
        LOS_SpinUnlockRestore(&g_timeSpin, intSave);
        return;
    }

    if (g_adjTimeLeft > g_adjPacement) {
        if (g_adjDirection) {
            if ((g_accDeltaFromAdj.tv_nsec + g_adjPacement) >= OS_SYS_NS_PER_SECOND) {
                g_accDeltaFromAdj.tv_sec++;
                g_accDeltaFromAdj.tv_nsec  = (g_accDeltaFromAdj.tv_nsec + g_adjPacement) % OS_SYS_NS_PER_SECOND;
            } else {
                g_accDeltaFromAdj.tv_nsec  = g_accDeltaFromAdj.tv_nsec + g_adjPacement;
            }
        } else {
            if ((g_accDeltaFromAdj.tv_nsec - g_adjPacement) < 0) {
                g_accDeltaFromAdj.tv_sec--;
                g_accDeltaFromAdj.tv_nsec  = g_accDeltaFromAdj.tv_nsec - g_adjPacement + OS_SYS_NS_PER_SECOND;
            } else {
                g_accDeltaFromAdj.tv_nsec  = g_accDeltaFromAdj.tv_nsec - g_adjPacement;
            }
        }

        g_adjTimeLeft -= g_adjPacement;
    } else {
        if (g_adjDirection) {
            if ((g_accDeltaFromAdj.tv_nsec + g_adjTimeLeft) >= OS_SYS_NS_PER_SECOND) {
                g_accDeltaFromAdj.tv_sec++;
                g_accDeltaFromAdj.tv_nsec  = (g_accDeltaFromAdj.tv_nsec + g_adjTimeLeft) % OS_SYS_NS_PER_SECOND;
            } else {
                g_accDeltaFromAdj.tv_nsec  = g_accDeltaFromAdj.tv_nsec + g_adjTimeLeft;
            }
        } else {
            if ((g_accDeltaFromAdj.tv_nsec - g_adjTimeLeft) < 0) {
                g_accDeltaFromAdj.tv_sec--;
                g_accDeltaFromAdj.tv_nsec  = g_accDeltaFromAdj.tv_nsec - g_adjTimeLeft + OS_SYS_NS_PER_SECOND;
            } else {
                g_accDeltaFromAdj.tv_nsec  = g_accDeltaFromAdj.tv_nsec - g_adjTimeLeft;
            }
        }

        g_adjTimeLeft = 0;
    }
    LOS_SpinUnlockRestore(&g_timeSpin, intSave);
    return;
}

/*
 * Function: adjtime
 * Description:  correct the time to synchronize the system clock.
 * Input:     delta - The amount of time by which the clock is to be adjusted.
 * Output: oldDelta - the amount of time remaining from any previous adjustment that has not yet been completed.
 * Return: On success, returns 0.  On failure, -1 is returned, and errno is set to indicate the error.
 */
int adjtime(const struct timeval *delta, struct timeval *oldDelta)
{
    UINT32 intSave;
    LOS_SpinLockSave(&g_timeSpin, &intSave);
    /* return the amount of time remaining from any previous adjustment that has not yet been completed. */
    if (oldDelta != NULL) {
        if (g_adjDirection == 1) {
            oldDelta->tv_sec = g_adjTimeLeft / OS_SYS_NS_PER_SECOND;
            oldDelta->tv_usec = (g_adjTimeLeft % OS_SYS_NS_PER_SECOND) / OS_SYS_NS_PER_US;
        } else {
            oldDelta->tv_sec = -(g_adjTimeLeft / OS_SYS_NS_PER_SECOND);
            oldDelta->tv_usec = -((g_adjTimeLeft % OS_SYS_NS_PER_SECOND) / OS_SYS_NS_PER_US);
        }
    }

    if ((delta == NULL) || ((delta->tv_sec == 0) && (delta->tv_usec == 0))) {
        LOS_SpinUnlockRestore(&g_timeSpin, intSave);
        return 0;
    }

    if ((delta->tv_usec > OS_SYS_US_PER_SECOND) || (delta->tv_usec < -OS_SYS_US_PER_SECOND)) {
        LOS_SpinUnlockRestore(&g_timeSpin, intSave);
        TIME_RETURN(EINVAL);
    }

    /*
     * 2: in the glibc implementation, delta must be less than or equal to (INT_MAX / 1000000 - 2) and
     * greater than or equal to (INT_MIN / 1000000 + 2)
     */
    if ((delta->tv_sec < (INT_MIN / OS_SYS_US_PER_SECOND + 2)) ||
        (delta->tv_sec > (INT_MAX / OS_SYS_US_PER_SECOND + 2))) {
        LOS_SpinUnlockRestore(&g_timeSpin, intSave);
        TIME_RETURN(EINVAL);
    }

    g_adjTimeLeft = (INT64)delta->tv_sec * OS_SYS_NS_PER_SECOND + delta->tv_usec * OS_SYS_NS_PER_US;
    if (g_adjTimeLeft > 0) {
        g_adjDirection = 1;
    } else {
        g_adjDirection = 0;
        g_adjTimeLeft = -g_adjTimeLeft;
    }

    LOS_SpinUnlockRestore(&g_timeSpin, intSave);
    return 0;
}

STATIC INLINE struct timespec64 OsTimeSpecAdd(const struct timespec64 t1, const struct timespec64 t2)
{
    struct timespec64 ret = {0};

    ret.tv_sec = t1.tv_sec + t2.tv_sec;
    ret.tv_nsec = t1.tv_nsec + t2.tv_nsec;
    if (ret.tv_nsec >= OS_SYS_NS_PER_SECOND) {
        ret.tv_sec += 1;
        ret.tv_nsec -= OS_SYS_NS_PER_SECOND;
    } else if (ret.tv_nsec < 0L) {
        ret.tv_sec -= 1;
        ret.tv_nsec += OS_SYS_NS_PER_SECOND;
    }

    return ret;
}

STATIC INLINE struct timespec64 OsTimeSpecSub(const struct timespec64 t1, const struct timespec64 t2)
{
    struct timespec64 ret = {0};

    ret.tv_sec = t1.tv_sec - t2.tv_sec;
    ret.tv_nsec = t1.tv_nsec - t2.tv_nsec;
    if (ret.tv_nsec < 0) {
        ret.tv_sec -= 1;
        ret.tv_nsec += OS_SYS_NS_PER_SECOND;
    }

    return ret;
}

STATIC VOID OsGetHwTime(struct timespec64 *hwTime)
{
    UINT64 nowNsec;

    nowNsec = LOS_CurrNanosec();
    hwTime->tv_sec = nowNsec / OS_SYS_NS_PER_SECOND;
    hwTime->tv_nsec = nowNsec - hwTime->tv_sec * OS_SYS_NS_PER_SECOND;
}

STATIC INT32 OsSetTimeOfDay(const struct timeval64 *tv, const struct timezone *tz)
{
    UINT32 intSave;
    struct timespec64 setTime = {0};
    struct timespec64 hwTime = {0};
    struct timespec64 realTime = {0};
    struct timespec64 tmp = {0};

#ifdef LOSCFG_SECURITY_CAPABILITY
    if (!IsCapPermit(CAP_SET_TIMEOFDAY)) {
        TIME_RETURN(EPERM);
    }
#endif

    (VOID)tz;
    OsGetHwTime(&hwTime);
    setTime.tv_sec = tv->tv_sec;
    setTime.tv_nsec = tv->tv_usec * OS_SYS_NS_PER_US;

    LOS_SpinLockSave(&g_timeSpin, &intSave);
    /* stop on-going continuous adjusement */
    if (g_adjTimeLeft) {
        g_adjTimeLeft = 0;
    }
    realTime = OsTimeSpecAdd(hwTime, g_accDeltaFromAdj);
    realTime = OsTimeSpecAdd(realTime, g_accDeltaFromSet);

    tmp = OsTimeSpecSub(setTime, realTime);
    g_accDeltaFromSet = OsTimeSpecAdd(g_accDeltaFromSet, tmp);

    LOS_SpinUnlockRestore(&g_timeSpin, intSave);

    return 0;
}

int settimeofday(const struct timeval *tv, const struct timezone *tz)
{
    struct timeval64 stTimeVal64 = {0};

    if (!ValidTimeval(tv)) {
        TIME_RETURN(EINVAL);
    }

    stTimeVal64.tv_sec = tv->tv_sec;
    stTimeVal64.tv_usec = tv->tv_usec;

    return OsSetTimeOfDay(&stTimeVal64, tz);
}

#ifndef LOSCFG_AARCH64
int settimeofday64(const struct timeval64 *tv, const struct timezone *tz)
{
    if (!ValidTimeval64(tv)) {
        TIME_RETURN(EINVAL);
    }

    return OsSetTimeOfDay(tv, tz);
}
#endif

int setlocalseconds(int seconds)
{
    struct timeval tv = {0};

    tv.tv_sec = seconds;
    tv.tv_usec = 0;

    return settimeofday(&tv, NULL);
}

STATIC INT32 OsGetTimeOfDay(struct timeval64 *tv, struct timezone *tz)
{
    UINT32 intSave;

    (VOID)tz;
    struct timespec64 hwTime = {0};
    struct timespec64 realTime = {0};

    OsGetHwTime(&hwTime);

    LOS_SpinLockSave(&g_timeSpin, &intSave);
    realTime = OsTimeSpecAdd(hwTime, g_accDeltaFromAdj);
    realTime = OsTimeSpecAdd(realTime, g_accDeltaFromSet);
    LOS_SpinUnlockRestore(&g_timeSpin, intSave);

    tv->tv_sec = realTime.tv_sec;
    tv->tv_usec = realTime.tv_nsec / OS_SYS_NS_PER_US;

    if (tv->tv_sec < 0) {
        TIME_RETURN(EINVAL);
    }
    return 0;
}

#ifndef LOSCFG_AARCH64
int gettimeofday64(struct timeval64 *tv, struct timezone *tz)
{
    if (tv == NULL) {
        TIME_RETURN(EINVAL);
    }

    return OsGetTimeOfDay(tv, tz);
}
#endif

int gettimeofday(struct timeval *tv, struct timezone *tz)
{
    struct timeval64 stTimeVal64 = {0};

    if (tv == NULL) {
        TIME_RETURN(EINVAL);
    }

    if (OsGetTimeOfDay(&stTimeVal64, tz) == -1) {
        return -1;
    }

#ifdef LOSCFG_AARCH64
    tv->tv_sec = stTimeVal64.tv_sec;
    tv->tv_usec = stTimeVal64.tv_usec;
#else
    if (stTimeVal64.tv_sec > (long long)LONG_MAX) {
        return -1;
    }
    tv->tv_sec = (time_t)stTimeVal64.tv_sec;
    tv->tv_usec = (suseconds_t)stTimeVal64.tv_usec;
#endif

    return 0;
}

int clock_settime(clockid_t clockID, const struct timespec *tp)
{
    struct timeval tv = {0};

    switch (clockID) {
        case CLOCK_REALTIME:
            /* we only support the realtime clock currently */
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
            TIME_RETURN(ENOTSUP);
        case CLOCK_MONOTONIC:
        default:
            TIME_RETURN(EINVAL);
    }

    if (!ValidTimeSpec(tp)) {
        TIME_RETURN(EINVAL);
    }

#ifdef LOSCFG_SECURITY_CAPABILITY
    if (!IsCapPermit(CAP_CLOCK_SETTIME)) {
        TIME_RETURN(EPERM);
    }
#endif

    tv.tv_sec = tp->tv_sec;
    tv.tv_usec = tp->tv_nsec / OS_SYS_NS_PER_US;
    return settimeofday(&tv, NULL);
}

#ifdef LOSCFG_KERNEL_CPUP
inline UINT32 GetTidFromClockID(clockid_t clockID)
{
    // In musl/src/thread/pthread_getcpuclockid.c, we know 'clockid = (-tid - 1) * 8 + 6' 
    UINT32 tid = -(clockID - 6) / 8 - 1; // 6 8 1 inverse operation from clockID to tid
    return tid;
}

inline const pid_t GetPidFromClockID(clockid_t clockID)
{
    // In musl/src/time/clock_getcpuclockid.c, we know 'clockid = (-pid - 1) * 8 + 2'
    const pid_t pid = -(clockID - 2) / 8 - 1; // 2 8 1 inverse operation from clockID to pid
    return pid;
}

static int PthreadGetCputime(clockid_t clockID, struct timespec *ats)
{
    uint64_t runtime;
    UINT32 intSave;
    UINT32 tid = GetTidFromClockID(clockID);

    if (OS_TID_CHECK_INVALID(tid)) {
        return -EINVAL;
    }

    LosTaskCB *task = OsGetTaskCB(tid);

    if (OsCurrTaskGet()->processID != task->processID) {
        return -EINVAL;
    }

    SCHEDULER_LOCK(intSave);
    runtime = task->taskCpup.allTime;
    SCHEDULER_UNLOCK(intSave);

    ats->tv_sec = runtime / OS_SYS_NS_PER_SECOND;
    ats->tv_nsec = runtime % OS_SYS_NS_PER_SECOND;

    return 0;
}

static int ProcessGetCputime(clockid_t clockID, struct timespec *ats)
{
    UINT64 runtime;
    UINT32 intSave;
    const pid_t pid = GetPidFromClockID(clockID);
    LosProcessCB *spcb = NULL;

    if (OsProcessIDUserCheckInvalid(pid) || pid < 0) {
        return -EINVAL;
    }

    spcb = OS_PCB_FROM_PID(pid);
    if (OsProcessIsUnused(spcb)) {
        return -EINVAL;
    }

    SCHEDULER_LOCK(intSave);
    runtime = spcb->processCpup.allTime;
    SCHEDULER_UNLOCK(intSave);

    ats->tv_sec = runtime / OS_SYS_NS_PER_SECOND;
    ats->tv_nsec = runtime % OS_SYS_NS_PER_SECOND;

    return 0;
}

static int GetCputime(clockid_t clockID, struct timespec *tp)
{
    int ret;

    if (clockID >= 0) {
        return -EINVAL;
    }

    if ((UINT32)clockID & CPUCLOCK_PERTHREAD_MASK) {
        ret = PthreadGetCputime(clockID, tp);
    } else {
        ret = ProcessGetCputime(clockID, tp);
    }

    return ret;
}

static int CheckClock(const clockid_t clockID)
{
    int error = 0;
    const pid_t pid = GetPidFromClockID(clockID);

    if (!((UINT32)clockID & CPUCLOCK_PERTHREAD_MASK)) {
        LosProcessCB *spcb = NULL;
        if (OsProcessIDUserCheckInvalid(pid) || pid < 0) {
            return -EINVAL;
        }
        spcb = OS_PCB_FROM_PID(pid);
        if (OsProcessIsUnused(spcb)) {
            error = -EINVAL;
        }
    } else {
        error = -EINVAL;
    }

    return error;
}

static int CpuClockGetres(const clockid_t clockID, struct timespec *tp)
{
    if (clockID > 0) {
        return -EINVAL;
    }

    int error = CheckClock(clockID);
    if (!error) {
        error = ProcessGetCputime(clockID, tp);
    }

    return error;
}
#endif

int clock_gettime(clockid_t clockID, struct timespec *tp)
{
    UINT32 intSave;
    struct timespec64 tmp = {0};
    struct timespec64 hwTime = {0};

    if (clockID > MAX_CLOCKS) {
        goto ERROUT;
    }

    if (tp == NULL) {
        goto ERROUT;
    }

    OsGetHwTime(&hwTime);

    switch (clockID) {
        case CLOCK_MONOTONIC_RAW:
            tp->tv_sec = hwTime.tv_sec;
            tp->tv_nsec = hwTime.tv_nsec;
            break;
        case CLOCK_MONOTONIC:
            LOS_SpinLockSave(&g_timeSpin, &intSave);
            tmp = OsTimeSpecAdd(hwTime, g_accDeltaFromAdj);
            LOS_SpinUnlockRestore(&g_timeSpin, intSave);
            tp->tv_sec = tmp.tv_sec;
            tp->tv_nsec = tmp.tv_nsec;
            break;
        case CLOCK_REALTIME:
            LOS_SpinLockSave(&g_timeSpin, &intSave);
            tmp = OsTimeSpecAdd(hwTime, g_accDeltaFromAdj);
            tmp = OsTimeSpecAdd(tmp, g_accDeltaFromSet);
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
            TIME_RETURN(ENOTSUP);
        default:
        {
#ifdef LOSCFG_KERNEL_CPUP
            int ret = GetCputime(clockID, tp);
                TIME_RETURN(-ret);
#else
            TIME_RETURN(EINVAL);
#endif
        }
    }

    return 0;

    ERROUT:
    TIME_RETURN(EINVAL);
}

int clock_getres(clockid_t clockID, struct timespec *tp)
{
    if (tp == NULL) {
        TIME_RETURN(EINVAL);
    }

    switch (clockID) {
        case CLOCK_MONOTONIC_RAW:
        case CLOCK_MONOTONIC:
        case CLOCK_REALTIME:
            /* the accessable rtc resolution */
            tp->tv_nsec = OS_SYS_NS_PER_US; /* the precision of clock_gettime is 1us */
            tp->tv_sec = 0;
            break;
        case CLOCK_MONOTONIC_COARSE:
        case CLOCK_REALTIME_COARSE:
            /* the clock coarse resolution, supported by vdso.
             * the precision of clock_gettime is 1tick */
            tp->tv_nsec = OS_SYS_NS_PER_SECOND / LOSCFG_BASE_CORE_TICK_PER_SECOND;
            tp->tv_sec = 0;
            break;
        case CLOCK_THREAD_CPUTIME_ID:
        case CLOCK_PROCESS_CPUTIME_ID:
        case CLOCK_BOOTTIME:
        case CLOCK_REALTIME_ALARM:
        case CLOCK_BOOTTIME_ALARM:
        case CLOCK_TAI:
            TIME_RETURN(ENOTSUP);
        default:
#ifdef LOSCFG_KERNEL_CPUP
            {
                int ret = CpuClockGetres(clockID, tp);
                TIME_RETURN(-ret);
            }
#else
            TIME_RETURN(EINVAL);
#endif
    }

    TIME_RETURN(0);
}

int clock_nanosleep(clockid_t clk, int flags, const struct timespec *req, struct timespec *rem)
{
    switch (clk) {
        case CLOCK_REALTIME:
            if (flags == 0) {
                /* we only support the realtime clock currently */
                return nanosleep(req, rem);
            }
            /* fallthrough */
        case CLOCK_MONOTONIC_COARSE:
        case CLOCK_REALTIME_COARSE:
        case CLOCK_MONOTONIC_RAW:
        case CLOCK_MONOTONIC:
        case CLOCK_PROCESS_CPUTIME_ID:
        case CLOCK_BOOTTIME:
        case CLOCK_REALTIME_ALARM:
        case CLOCK_BOOTTIME_ALARM:
        case CLOCK_TAI:
            if (flags == 0 || flags == TIMER_ABSTIME) {
                TIME_RETURN(ENOTSUP);
            }
            /* fallthrough */
        case CLOCK_THREAD_CPUTIME_ID:
        default:
            TIME_RETURN(EINVAL);
    }

    TIME_RETURN(0);
}

typedef struct {
    int sigev_signo;
    pid_t pid;
    unsigned int tid;
    union sigval sigev_value;
} swtmr_proc_arg;

static VOID SwtmrProc(UINTPTR tmrArg)
{
    INT32 sig, ret;
    UINT32 intSave;
    pid_t pid;
    siginfo_t info;
    LosTaskCB *stcb = NULL;

    swtmr_proc_arg *arg = (swtmr_proc_arg *)tmrArg;
    OS_GOTO_EXIT_IF(arg == NULL, EINVAL);

    sig = arg->sigev_signo;
    pid = arg->pid;
    OS_GOTO_EXIT_IF(!GOOD_SIGNO(sig), EINVAL);

    /* Create the siginfo structure */
    info.si_signo = sig;
    info.si_code = SI_TIMER;
    info.si_value.sival_ptr = arg->sigev_value.sival_ptr;

    /* Send signals to threads or processes */
    if (arg->tid > 0) {
        /* Make sure that the para is valid */
        OS_GOTO_EXIT_IF(OS_TID_CHECK_INVALID(arg->tid), EINVAL);
        stcb = OsGetTaskCB(arg->tid);
        ret = OsUserProcessOperatePermissionsCheck(stcb, stcb->processID);
        OS_GOTO_EXIT_IF(ret != LOS_OK, -ret);

        /* Dispatch the signal to thread, bypassing normal task group thread
         * dispatch rules. */
        SCHEDULER_LOCK(intSave);
        ret = OsTcbDispatch(stcb, &info);
        SCHEDULER_UNLOCK(intSave);
        OS_GOTO_EXIT_IF(ret != LOS_OK, -ret);
    } else {
        /* Make sure that the para is valid */
        OS_GOTO_EXIT_IF(pid <= 0 || OS_PID_CHECK_INVALID(pid), EINVAL);
        /* Dispatch the signal to process */
        SCHEDULER_LOCK(intSave);
        OsDispatch(pid, &info, OS_USER_KILL_PERMISSION);
        SCHEDULER_UNLOCK(intSave);
    }
    return;
EXIT:
    PRINT_ERR("Dispatch signals failed!, ret: %d\r\n", ret);
    return;
}

int OsTimerCreate(clockid_t clockID, struct ksigevent *evp, timer_t *timerID)
{
    UINT32 ret;
    UINT16 swtmrID;
    swtmr_proc_arg *arg = NULL;
    int signo;
#ifdef LOSCFG_SECURITY_VID
    UINT16 vid;
#endif

    if ((clockID != CLOCK_REALTIME) || (timerID == NULL)) {
        errno = EINVAL;
        return -1;
    }

    signo = evp ? evp->sigev_signo : SIGALRM;
    if (signo > SIGRTMAX || signo < 1) {
        errno = EINVAL;
        return -1;
    }
    if (evp && (evp->sigev_notify != SIGEV_SIGNAL && evp->sigev_notify != SIGEV_THREAD_ID)) {
        errno = ENOTSUP;
        return -1;
    }

    arg = (swtmr_proc_arg *)malloc(sizeof(swtmr_proc_arg));
    if (arg == NULL) {
        errno = ENOMEM;
        return -1;
    }

    arg->tid = evp ? evp->sigev_tid : 0;
    arg->sigev_signo = signo;
    arg->pid = LOS_GetCurrProcessID();
    arg->sigev_value.sival_ptr = evp ? evp->sigev_value.sival_ptr : NULL;
    ret = LOS_SwtmrCreate(1, LOS_SWTMR_MODE_ONCE, SwtmrProc, &swtmrID, (UINTPTR)arg);
    if (ret != LOS_OK) {
        errno = (ret == LOS_ERRNO_SWTMR_MAXSIZE) ? EAGAIN : EINVAL;
        free(arg);
        return -1;
    }

#ifdef LOSCFG_SECURITY_VID
    vid = AddNodeByRid(swtmrID);
    if (vid == MAX_INVALID_TIMER_VID) {
        free(arg);
        (VOID)LOS_SwtmrDelete(swtmrID);
        return -1;
    }
    swtmrID = vid;
#endif
    *timerID = (timer_t)(UINTPTR)swtmrID;
    return 0;
}

int timer_delete(timer_t timerID)
{
    UINT16 swtmrID = (UINT16)(UINTPTR)timerID;
    VOID *arg = NULL;

#ifdef LOSCFG_SECURITY_VID
    swtmrID = GetRidByVid(swtmrID);
#endif
    if (OS_INT_ACTIVE || !ValidTimerID(swtmrID)) {
        goto ERROUT;
    }

    arg = (VOID *)OS_SWT_FROM_SID(swtmrID)->uwArg;
    if (LOS_SwtmrDelete(swtmrID)) {
        goto ERROUT;
    }
    if (arg != NULL) {
        free(arg);
    }

#ifdef LOSCFG_SECURITY_VID
    RemoveNodeByVid((UINT16)(UINTPTR)timerID);
#endif
    return 0;

ERROUT:
    errno = EINVAL;
    return -1;
}

int timer_settime(timer_t timerID, int flags,
                  const struct itimerspec *value,   /* new value */
                  struct itimerspec *oldValue)      /* old value to return, always 0 */
{
    UINT16 swtmrID = (UINT16)(UINTPTR)timerID;
    SWTMR_CTRL_S *swtmr = NULL;
    UINT32 interval, expiry, ret;
    UINT32 intSave;

    if (flags != 0) {
        /* flags not supported currently */
        errno = ENOSYS;
        return -1;
    }

#ifdef LOSCFG_SECURITY_VID
    swtmrID = GetRidByVid(swtmrID);
#endif
    if ((value == NULL) || OS_INT_ACTIVE || !ValidTimerID(swtmrID)) {
        errno = EINVAL;
        return -1;
    }

    if (!ValidTimeSpec(&value->it_value) || !ValidTimeSpec(&value->it_interval)) {
        errno = EINVAL;
        return -1;
    }

    if (oldValue) {
        (VOID)timer_gettime(timerID, oldValue);
    }

    swtmr = OS_SWT_FROM_SID(swtmrID);
    ret = LOS_SwtmrStop(swtmr->usTimerID);
    if ((ret != LOS_OK) && (ret != LOS_ERRNO_SWTMR_NOT_STARTED)) {
        errno = EINVAL;
        return -1;
    }

    expiry = OsTimeSpec2Tick(&value->it_value);
    interval = OsTimeSpec2Tick(&value->it_interval);

    LOS_SpinLockSave(&g_swtmrSpin, &intSave);
    swtmr->ucMode = interval ? LOS_SWTMR_MODE_OPP : LOS_SWTMR_MODE_NO_SELFDELETE;
    swtmr->uwExpiry = expiry + !!expiry; // PS: skip the first tick because it is NOT a full tick.
    swtmr->uwInterval = interval;
    swtmr->uwOverrun = 0;
    LOS_SpinUnlockRestore(&g_swtmrSpin, intSave);

    if ((value->it_value.tv_sec == 0) && (value->it_value.tv_nsec == 0)) {
        /*
         * 1) when expiry is 0, means timer should be stopped.
         * 2) If timer is ticking, stopping timer is already done before.
         * 3) If timer is created but not ticking, return 0 as well.
         */
        return 0;
    }

    if (LOS_SwtmrStart(swtmr->usTimerID)) {
        errno = EINVAL;
        return -1;
    }

    return 0;
}

int timer_gettime(timer_t timerID, struct itimerspec *value)
{
    UINT32 tick = 0;
    SWTMR_CTRL_S *swtmr = NULL;
    UINT16 swtmrID = (UINT16)(UINTPTR)timerID;
    UINT32 ret;

#ifdef LOSCFG_SECURITY_VID
    swtmrID = GetRidByVid(swtmrID);
#endif
    if ((value == NULL) || !ValidTimerID(swtmrID)) {
        errno = EINVAL;
        return -1;
    }

    swtmr = OS_SWT_FROM_SID(swtmrID);

    /* get expire time */
    ret = LOS_SwtmrTimeGet(swtmr->usTimerID, &tick);
    if ((ret != LOS_OK) && (ret != LOS_ERRNO_SWTMR_NOT_STARTED)) {
        errno = EINVAL;
        return -1;
    }

    OsTick2TimeSpec(&value->it_value, tick);
    OsTick2TimeSpec(&value->it_interval, (swtmr->ucMode == LOS_SWTMR_MODE_ONCE) ? 0 : swtmr->uwInterval);
    return 0;
}

int timer_getoverrun(timer_t timerID)
{
    UINT16 swtmrID = (UINT16)(UINTPTR)timerID;
    SWTMR_CTRL_S *swtmr = NULL;
    INT32 overRun;

#ifdef LOSCFG_SECURITY_VID
    swtmrID = GetRidByVid(swtmrID);
#endif
    if (!ValidTimerID(swtmrID)) {
        errno = EINVAL;
        return -1;
    }

    swtmr = OS_SWT_FROM_SID(swtmrID);
    if (swtmr->usTimerID >= OS_SWTMR_MAX_TIMERID) {
        errno = EINVAL;
        return -1;
    }

    overRun = (INT32)(swtmr->uwOverrun);
    return (overRun > DELAYTIMER_MAX) ? DELAYTIMER_MAX : overRun;
}

STATIC INT32 DoNanoSleep(UINT64 nanoseconds)
{
    UINT32 ret;

    ret = LOS_TaskDelay(OsNS2Tick(nanoseconds));
    if (ret == LOS_OK || ret == LOS_ERRNO_TSK_YIELD_NOT_ENOUGH_TASK) {
        return 0;
    }
    return -1;
}

int usleep(unsigned useconds)
{
    return DoNanoSleep((UINT64)useconds * OS_SYS_NS_PER_US);
}

int nanosleep(const struct timespec *rqtp, struct timespec *rmtp)
{
    UINT64 nanoseconds;
    INT32 ret = -1;

    (VOID)rmtp;
    /* expire time */

    if (!ValidTimeSpec(rqtp)) {
        errno = EINVAL;
        return ret;
    }

    nanoseconds = (UINT64)rqtp->tv_sec * OS_SYS_NS_PER_SECOND + rqtp->tv_nsec;

    return DoNanoSleep(nanoseconds);
}

unsigned int sleep(unsigned int seconds)
{
    return DoNanoSleep((UINT64)seconds * OS_SYS_NS_PER_SECOND);
}

double difftime(time_t time2, time_t time1)
{
    return (double)(time2 - time1);
}

clock_t clock(VOID)
{
    clock_t clockMsec;
    UINT64 nowNsec;

    nowNsec = LOS_CurrNanosec();
    clockMsec = (clock_t)(nowNsec / (OS_SYS_NS_PER_SECOND / CLOCKS_PER_SEC));

    return clockMsec;
}

clock_t times(struct tms *buf)
{
    clock_t clockTick = -1;

    (void)buf;
    set_errno(ENOSYS);

    return clockTick;
}

int setitimer(int which, const struct itimerval *value, struct itimerval *ovalue)
{
    UINT32 intSave;
    LosTaskCB *taskCB = OS_TCB_FROM_TID(LOS_CurTaskIDGet());
    LosProcessCB *processCB = OS_PCB_FROM_PID(taskCB->processID);
    timer_t timerID = 0;
    struct itimerspec spec;
    struct itimerspec ospec;
    int ret = LOS_OK;

    /* we only support the realtime clock timer currently */
    if (which != ITIMER_REAL || !value) {
        set_errno(EINVAL);
        return -1;
    }

    /* To avoid creating an invalid timer after the timer has already been create */
    if (processCB->timerID == (timer_t)(UINTPTR)MAX_INVALID_TIMER_VID) {
        ret = OsTimerCreate(CLOCK_REALTIME, NULL, &timerID);
        if (ret != LOS_OK) {
            return ret;
        }
    }

    /* The initialization of this global timer must be in spinlock
     * OsTimerCreate cannot be located in spinlock.
     */
    SCHEDULER_LOCK(intSave);
    if (processCB->timerID == (timer_t)(UINTPTR)MAX_INVALID_TIMER_VID) {
        processCB->timerID = timerID;
        SCHEDULER_UNLOCK(intSave);
    } else {
        SCHEDULER_UNLOCK(intSave);
        if (timerID) {
            timer_delete(timerID);
        }
    }

    if (!ValidTimeval(&value->it_value) || !ValidTimeval(&value->it_interval)) {
        set_errno(EINVAL);
        return -1;
    }

    TIMEVAL_TO_TIMESPEC(&value->it_value, &spec.it_value);
    TIMEVAL_TO_TIMESPEC(&value->it_interval, &spec.it_interval);

    ret = timer_settime(processCB->timerID, 0, &spec, ovalue ? &ospec : NULL);
    if (ret == LOS_OK && ovalue) {
        TIMESPEC_TO_TIMEVAL(&ovalue->it_value, &ospec.it_value);
        TIMESPEC_TO_TIMEVAL(&ovalue->it_interval, &ospec.it_interval);
    }

    return ret;
}

int getitimer(int which, struct itimerval *value)
{
    LosTaskCB *taskCB = OS_TCB_FROM_TID(LOS_CurTaskIDGet());
    LosProcessCB *processCB = OS_PCB_FROM_PID(taskCB->processID);
    struct itimerspec spec = {};

    int ret = LOS_OK;

    /* we only support the realtime clock timer currently */
    if (which != ITIMER_REAL || !value) {
        set_errno(EINVAL);
        return -1;
    }

    if (processCB->timerID != (timer_t)(UINTPTR)MAX_INVALID_TIMER_VID) {
        ret = timer_gettime(processCB->timerID, &spec);
    }

    if (ret == LOS_OK) {
        TIMESPEC_TO_TIMEVAL(&value->it_value, &spec.it_value);
        TIMESPEC_TO_TIMEVAL(&value->it_interval, &spec.it_interval);
    }

    return ret;
}

#ifdef LOSCFG_KERNEL_VDSO
VOID OsVdsoTimeGet(VdsoDataPage *vdsoDataPage)
{
    UINT32 intSave;
    struct timespec64 tmp = {0};
    struct timespec64 hwTime = {0};

    if (vdsoDataPage == NULL) {
        return;
    }

    OsGetHwTime(&hwTime);

    LOS_SpinLockSave(&g_timeSpin, &intSave);
    tmp = OsTimeSpecAdd(hwTime, g_accDeltaFromAdj);
    vdsoDataPage->monoTimeSec = tmp.tv_sec;
    vdsoDataPage->monoTimeNsec = tmp.tv_nsec;

    tmp = OsTimeSpecAdd(tmp, g_accDeltaFromSet);
    vdsoDataPage->realTimeSec = tmp.tv_sec;
    vdsoDataPage->realTimeNsec = tmp.tv_nsec;
    LOS_SpinUnlockRestore(&g_timeSpin, intSave);
}
#endif

time_t time(time_t *t)
{
    struct timeval tp;
    int ret;

    /* Get the current time from the system */
    ret = gettimeofday(&tp, (struct timezone *)NULL);
    if (ret == LOS_OK) {
        /* Return the seconds since the epoch */
        if (t) {
            *t = tp.tv_sec;
        }
        return tp.tv_sec;
    }
    return (time_t)OS_ERROR;
}
