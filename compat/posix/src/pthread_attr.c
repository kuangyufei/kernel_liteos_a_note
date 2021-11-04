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

#include "pthread.h"
#include "pprivate.h"
//https://docs.oracle.com/cd/E19253-01/819-7051/6n919hpac/index.html
//线程属性初始化
int pthread_attr_init(pthread_attr_t *attr)
{
    if (attr == NULL) {
        return EINVAL;
    }

    attr->detachstate                 = PTHREAD_CREATE_JOINABLE;//
    attr->schedpolicy                 = SCHED_RR;//抢占式调度
    attr->schedparam.sched_priority   = LOSCFG_BASE_CORE_TSK_DEFAULT_PRIO;//调度优先级
    attr->inheritsched                = PTHREAD_INHERIT_SCHED;//继承调度
    attr->scope                       = PTHREAD_SCOPE_PROCESS;//进程范围
    attr->stackaddr_set               = 0;	//暂未内核栈开始地址
    attr->stackaddr                   = NULL;//内核栈地址
    attr->stacksize_set               = 1;//已设置内核栈大小
    attr->stacksize                   = LOSCFG_BASE_CORE_TSK_DEFAULT_STACK_SIZE;//默认16K

#ifdef LOSCFG_KERNEL_SMP
    attr->cpuset.__bits[0] = 0;
#endif

    return ENOERR;
}

int pthread_attr_destroy(pthread_attr_t *attr)
{
    if (attr == NULL) {
        return EINVAL;
    }

    /* Nothing to do here... */
    return ENOERR;
}
///设置分离状态(分离和联合) 如果创建分离线程 (PTHREAD_CREATE_DETACHED)，则该线程一退出，便可重用其线程 ID 和其他资源。
int pthread_attr_setdetachstate(pthread_attr_t *attr, int detachState)
{
    if ((attr != NULL) && ((detachState == PTHREAD_CREATE_JOINABLE) || (detachState == PTHREAD_CREATE_DETACHED))) {
        attr->detachstate = (UINT32)detachState;
        return ENOERR;
    }

    return EINVAL;
}
///获取分离状态
int pthread_attr_getdetachstate(const pthread_attr_t *attr, int *detachState)
{
    if ((attr == NULL) || (detachState == NULL)) {
        return EINVAL;
    }

    *detachState = (int)attr->detachstate;

    return ENOERR;
}
///设置线程的竞争范围（PTHREAD_SCOPE_SYSTEM 或 PTHREAD_SCOPE_PROCESS）。 
//使用 PTHREAD_SCOPE_SYSTEM 时，此线程将与系统中的所有线程进行竞争。
//使用 PTHREAD_SCOPE_PROCESS 时，此线程将与进程中的其他线程进行竞争。
int pthread_attr_setscope(pthread_attr_t *attr, int scope)
{
    if (attr == NULL) {
        return EINVAL;
    }

    if (scope == PTHREAD_SCOPE_PROCESS) {
        attr->scope = (unsigned int)scope;
        return ENOERR;
    }

    if (scope == PTHREAD_SCOPE_SYSTEM) {
        return ENOTSUP;
    }

    return EINVAL;
}
///获取线程的竞争范围
int pthread_attr_getscope(const pthread_attr_t *attr, int *scope)
{
    if ((attr == NULL) || (scope == NULL)) {
        return EINVAL;
    }

    *scope = (int)attr->scope;

    return ENOERR;
}
///设置继承的调度策略, PTHREAD_INHERIT_SCHED 表示新建的线程将继承创建者线程中定义的调度策略。
//将忽略在 pthread_create() 调用中定义的所有调度属性。如果使用缺省值 PTHREAD_EXPLICIT_SCHED，则将使用 pthread_create() 调用中的属性
int pthread_attr_setinheritsched(pthread_attr_t *attr, int inherit)
{
    if ((attr != NULL) && ((inherit == PTHREAD_INHERIT_SCHED) || (inherit == PTHREAD_EXPLICIT_SCHED))) {
        attr->inheritsched = (UINT32)inherit;
        return ENOERR;
    }

    return EINVAL;
}
///获取继承的调度策略
int pthread_attr_getinheritsched(const pthread_attr_t *attr, int *inherit)
{
    if ((attr == NULL) || (inherit == NULL)) {
        return EINVAL;
    }

    *inherit = (int)attr->inheritsched;

    return ENOERR;
}
///设置调度策略,POSIX 标准指定 SCHED_FIFO（先入先出）、SCHED_RR（抢占）或 SCHED_OTHER（实现定义的方法）的调度策略属性。
int pthread_attr_setschedpolicy(pthread_attr_t *attr, int policy)
{
    if ((attr != NULL) && (policy == SCHED_RR)) {
        attr->schedpolicy = SCHED_RR;
        return ENOERR;
    }

    return EINVAL;
}
///获取调度策略
int pthread_attr_getschedpolicy(const pthread_attr_t *attr, int *policy)
{
    if ((attr == NULL) || (policy == NULL)) {
        return EINVAL;
    }

    *policy = (int)attr->schedpolicy;

    return ENOERR;
}
///设置线程属性对象的调度参数属性,调度参数是在 param 结构中定义的。仅支持优先级参数。新创建的线程使用此优先级运行。
int pthread_attr_setschedparam(pthread_attr_t *attr, const struct sched_param *param)
{
    if ((attr == NULL) || (param == NULL)) {
        return EINVAL;
    } else if ((param->sched_priority < 0) || (param->sched_priority > OS_TASK_PRIORITY_LOWEST)) {
        return ENOTSUP;
    }

    attr->schedparam = *param;

    return ENOERR;
}
///获取线程属性对象的调度参数属性
int pthread_attr_getschedparam(const pthread_attr_t *attr, struct sched_param *param)
{
    if ((attr == NULL) || (param == NULL)) {
        return EINVAL;
    }

    *param = attr->schedparam;

    return ENOERR;
}

/*
 * Set starting address of stack. Whether this is at the start or end of
 * the memory block allocated for the stack depends on whether the stack
 * grows up or down.
 */
//设置栈起始地址,在开始还是结束为堆栈分配的内存块取决于堆栈是向上增长还是向下增长。
int pthread_attr_setstackaddr(pthread_attr_t *attr, void *stackAddr)
{
    if (attr == NULL) {
        return EINVAL;
    }

    attr->stackaddr_set = 1;
    attr->stackaddr     = stackAddr;

    return ENOERR;
}
///获取栈起始地址
int pthread_attr_getstackaddr(const pthread_attr_t *attr, void **stackAddr)
{
    if (((attr != NULL) && (stackAddr != NULL)) && attr->stackaddr_set) {
        *stackAddr = attr->stackaddr;
        return ENOERR;
    }

    return EINVAL; /* Stack address not set, return EINVAL. */
}
///设置栈大小
int pthread_attr_setstacksize(pthread_attr_t *attr, size_t stackSize)
{
    /* Reject inadequate stack sizes *///拒绝不合适的堆栈尺寸
    if ((attr == NULL) || (stackSize < PTHREAD_STACK_MIN)) {//size 不应小于系统定义的最小栈大小
        return EINVAL;
    }

    attr->stacksize_set = 1;
    attr->stacksize     = stackSize;

    return ENOERR;
}
///获取栈大小
int pthread_attr_getstacksize(const pthread_attr_t *attr, size_t *stackSize)
{
    /* Reject attempts to get a stack size when one has not been set. */
    if ((attr == NULL) || (stackSize == NULL) || (!attr->stacksize_set)) {
        return EINVAL;
    }

    *stackSize = attr->stacksize;

    return ENOERR;
}

/*
 * Set the cpu affinity mask
 *///cpu集可以认为是一个掩码，每个设置的位都对应一个可以合法调度的 cpu，而未设置的位则对应一个不可调度的 CPU。
int pthread_attr_setaffinity_np(pthread_attr_t* attr, size_t cpusetsize, const cpu_set_t* cpuset)
{
#ifdef LOSCFG_KERNEL_SMP
    if (attr == NULL) {
        return EINVAL;
    }

    if ((cpuset == NULL) || (cpusetsize == 0)) {
        attr->cpuset.__bits[0] = 0;
        return ENOERR;
    }
///cpu_set_t这个结构体的理解类似于select中的fd_set，可以理解为cpu集，也是通过约定好的宏来进行清除、设置以及判断
    if ((cpusetsize != sizeof(cpu_set_t)) || (cpuset->__bits[0] > LOSCFG_KERNEL_CPU_MASK)) {
        return EINVAL;
    }

    attr->cpuset = *cpuset;
#endif

    return ENOERR;
}

/*
 * Get the cpu affinity mask
 */
int pthread_attr_getaffinity_np(const pthread_attr_t* attr, size_t cpusetsize, cpu_set_t* cpuset)
{
#ifdef LOSCFG_KERNEL_SMP
    if ((attr == NULL) || (cpuset == NULL) || (cpusetsize != sizeof(cpu_set_t))) {
        return EINVAL;
    }

    *cpuset = attr->cpuset;
#endif

    return ENOERR;
}

