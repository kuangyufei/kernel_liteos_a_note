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

#include "sys/types.h"
#include "sys/resource.h"
#include "unistd.h"
#include "stdio.h"
#include "pthread.h"
#include "sys/utsname.h"
#include "mqueue.h"
#include "semaphore.h"
#include "los_process_pri.h"
#include "los_hw.h"

/*
 * Supply some suitable values for constants that may not be present
 * in all configurations.
 */
#define SC_ENABLE                       1
#define SC_DISABLE                      (-1)

#define CONF_CASE_RETURN(name, val) \
    case (name):                    \
        return (val)

//uname命令用于显示当前操作系统的名称，版本创建时间，系统名称，版本信息等
int uname(struct utsname *name)
{
    INT32 ret;
    const char *cpuInfo = NULL;
    if (name == NULL) {
        return -EFAULT;
    }
    (VOID)strcpy_s(name->sysname, sizeof(name->sysname), KERNEL_NAME);
    (VOID)strcpy_s(name->nodename, sizeof(name->nodename), "hisilicon");
    ret = sprintf_s(name->version, sizeof(name->version), "%s %u.%u.%u.%u %s %s",
                     KERNEL_NAME, KERNEL_MAJOR, KERNEL_MINOR, KERNEL_PATCH, KERNEL_ITRE, __DATE__, __TIME__);
    if (ret < 0) {
        return -EIO;
    }

    cpuInfo = LOS_CpuInfo();
    (VOID)strcpy_s(name->machine, sizeof(name->machine), cpuInfo);
    ret = sprintf_s(name->release, sizeof(name->release), "%u.%u.%u.%u",
                     KERNEL_MAJOR, KERNEL_MINOR, KERNEL_PATCH, KERNEL_ITRE);
    if (ret < 0) {
        return -EIO;
    }

    name->domainname[0] = '\0';
    return 0;
}

long sysconf(int name)
{
    switch (name) {
        CONF_CASE_RETURN(_SC_AIO_LISTIO_MAX,                    SC_DISABLE);
        CONF_CASE_RETURN(_SC_AIO_MAX,                           SC_DISABLE);
        CONF_CASE_RETURN(_SC_AIO_PRIO_DELTA_MAX,                SC_DISABLE);
        CONF_CASE_RETURN(_SC_ARG_MAX,                           ARG_MAX);
        CONF_CASE_RETURN(_SC_ASYNCHRONOUS_IO,                   SC_DISABLE);
        CONF_CASE_RETURN(_SC_CHILD_MAX,                         CHILD_MAX);
        CONF_CASE_RETURN(_SC_CLK_TCK,                           SYS_CLK_TCK);
        CONF_CASE_RETURN(_SC_DELAYTIMER_MAX,                    DELAYTIMER_MAX);
        CONF_CASE_RETURN(_SC_FSYNC,                             SC_DISABLE);
        CONF_CASE_RETURN(_SC_GETGR_R_SIZE_MAX,                  GETGR_R_SIZE_MAX);
        CONF_CASE_RETURN(_SC_GETPW_R_SIZE_MAX,                  GETPW_R_SIZE_MAX);
        CONF_CASE_RETURN(_SC_JOB_CONTROL,                       SC_DISABLE);
        CONF_CASE_RETURN(_SC_LOGIN_NAME_MAX,                    LOGIN_NAME_MAX);
        CONF_CASE_RETURN(_SC_MAPPED_FILES,                      SC_DISABLE);
        CONF_CASE_RETURN(_SC_MEMLOCK,                           SC_DISABLE);
        CONF_CASE_RETURN(_SC_MEMLOCK_RANGE,                     SC_DISABLE);
        CONF_CASE_RETURN(_SC_MEMORY_PROTECTION,                 SC_DISABLE);
        CONF_CASE_RETURN(_SC_MESSAGE_PASSING,                   SC_DISABLE);
#ifdef LOSCFG_BASE_IPC_QUEUE
        CONF_CASE_RETURN(_SC_MQ_OPEN_MAX,                       MQ_OPEN_MAX);
        CONF_CASE_RETURN(_SC_MQ_PRIO_MAX,                       MQ_PRIO_MAX);
#endif
        CONF_CASE_RETURN(_SC_NGROUPS_MAX,                       NGROUPS_MAX);
        CONF_CASE_RETURN(_SC_OPEN_MAX,                          OPEN_MAX);
        CONF_CASE_RETURN(_SC_PAGESIZE,                          0x1000);
        CONF_CASE_RETURN(_SC_PRIORITIZED_IO,                    SC_DISABLE);
        CONF_CASE_RETURN(_SC_PRIORITY_SCHEDULING,               SC_DISABLE);
        CONF_CASE_RETURN(_SC_REALTIME_SIGNALS,                  SC_DISABLE);
        CONF_CASE_RETURN(_SC_RTSIG_MAX,                         RTSIG_MAX);
        CONF_CASE_RETURN(_SC_SAVED_IDS,                         SC_DISABLE);

#ifdef LOSCFG_BASE_IPC_SEM
        CONF_CASE_RETURN(_SC_SEMAPHORES,                        SC_ENABLE);
        CONF_CASE_RETURN(_SC_SEM_NSEMS_MAX,                     SEM_NSEMS_MAX);
        CONF_CASE_RETURN(_SC_SEM_VALUE_MAX,                     SEM_VALUE_MAX);
#endif

        CONF_CASE_RETURN(_SC_SHARED_MEMORY_OBJECTS,             SC_DISABLE);
        CONF_CASE_RETURN(_SC_SIGQUEUE_MAX,                      SIGQUEUE_MAX);
        CONF_CASE_RETURN(_SC_STREAM_MAX,                        STREAM_MAX);
        CONF_CASE_RETURN(_SC_SYNCHRONIZED_IO,                   SC_DISABLE);
        CONF_CASE_RETURN(_SC_THREADS,                           SC_ENABLE);
        CONF_CASE_RETURN(_SC_THREAD_ATTR_STACKADDR,             SC_ENABLE);
        CONF_CASE_RETURN(_SC_THREAD_ATTR_STACKSIZE,             PTHREAD_ATTR_STACKSIZE);
        CONF_CASE_RETURN(_SC_THREAD_DESTRUCTOR_ITERATIONS,      PTHREAD_DESTRUCTOR_ITERATIONS);
        CONF_CASE_RETURN(_SC_THREAD_KEYS_MAX,                   PTHREAD_KEYS_MAX);
        CONF_CASE_RETURN(_SC_THREAD_PRIO_INHERIT,               PTHREAD_PRIO_INHERIT);
        CONF_CASE_RETURN(_SC_THREAD_PRIO_PROTECT,               PTHREAD_PRIO_PROTECT);
        CONF_CASE_RETURN(_SC_THREAD_PRIORITY_SCHEDULING,        PTHREAD_PRIORITY_SCHEDULING);
        CONF_CASE_RETURN(_SC_THREAD_PROCESS_SHARED,             PTHREAD_PROCESS_SHARED);
        CONF_CASE_RETURN(_SC_THREAD_SAFE_FUNCTIONS,             SC_DISABLE);
        CONF_CASE_RETURN(_SC_THREAD_STACK_MIN,                  PTHREAD_STACK_MIN);
        CONF_CASE_RETURN(_SC_THREAD_THREADS_MAX,                PTHREAD_THREADS_MAX);
        CONF_CASE_RETURN(_SC_TIMERS,                            TIMERS);
        CONF_CASE_RETURN(_SC_TIMER_MAX,                         TIMER_MAX);
        CONF_CASE_RETURN(_SC_TTY_NAME_MAX,                      TTY_NAME_MAX);
        CONF_CASE_RETURN(_SC_TZNAME_MAX,                        TZNAME_MAX);
        CONF_CASE_RETURN(_SC_VERSION,                           POSIX_VERSION);

        default:
            set_errno(EINVAL);
            return -1;
    }
}

pid_t getpid(void)
{
    return ((LosTaskCB *)(OsCurrTaskGet()))->taskID;
}

int getrlimit(int resource, struct rlimit *rlim)
{
    LosProcessCB *pcb = OsCurrProcessGet();

    switch (resource) {
        case RLIMIT_NOFILE:
        case RLIMIT_FSIZE:
            break;
        default:
            return -EINVAL;
    }
    rlim->rlim_cur = pcb->pl_rlimit[resource].rlim_cur;
    rlim->rlim_max = pcb->pl_rlimit[resource].rlim_max;

    return 0;
}

#define FSIZE_RLIMIT 0XFFFFFFFF
int setrlimit(int resource, const struct rlimit *rlim)
{
    LosProcessCB *pcb = OsCurrProcessGet();

    if (rlim->rlim_cur > rlim->rlim_max) {
        return -EINVAL;
    }
    switch (resource) {
        case RLIMIT_NOFILE:
            if (rlim->rlim_max > NR_OPEN_DEFAULT) {
                return -EPERM;
            }
            break;
        case RLIMIT_FSIZE:
            if (rlim->rlim_max > FSIZE_RLIMIT) {
                return -EPERM;
            }
            break;
        default:
            return -EINVAL;
    }
    pcb->pl_rlimit[resource].rlim_cur = rlim->rlim_cur;
    pcb->pl_rlimit[resource].rlim_max = rlim->rlim_max;

    return 0;
}
