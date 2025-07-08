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
/**
 * @brief 初始化线程属性对象
 * @param[in,out] attr 线程属性对象指针
 * @return 成功返回ENOERR，失败返回EINVAL
 * @note 初始化后属性值为默认值：可连接状态、RR调度策略、默认优先级、继承调度属性、进程作用域、默认栈大小
 */
int pthread_attr_init(pthread_attr_t *attr)
{
    if (attr == NULL) {  // 检查属性对象指针是否为空
        return EINVAL;   // 空指针返回参数无效错误
    }

    // 设置默认线程属性值
    attr->detachstate                 = PTHREAD_CREATE_JOINABLE;  // 默认可连接状态
    attr->schedpolicy                 = SCHED_RR;                 // 默认RR调度策略
    attr->schedparam.sched_priority   = LOSCFG_BASE_CORE_TSK_DEFAULT_PRIO;  // 默认优先级
    attr->inheritsched                = PTHREAD_INHERIT_SCHED;    // 默认继承调度属性
    attr->scope                       = PTHREAD_SCOPE_PROCESS;    // 默认进程作用域
    attr->stackaddr_set               = 0;                        // 未设置栈地址
    attr->stackaddr                   = NULL;                     // 栈地址为空
    attr->stacksize_set               = 1;                        // 已设置栈大小
    attr->stacksize                   = LOSCFG_BASE_CORE_TSK_DEFAULT_STACK_SIZE;  // 默认栈大小

#ifdef LOSCFG_KERNEL_SMP
    attr->cpuset.__bits[0] = 0;  // SMP配置下初始化CPU亲和性掩码
#endif

    return ENOERR;  // 初始化成功
}

/**
 * @brief 销毁线程属性对象
 * @param[in] attr 线程属性对象指针
 * @return 成功返回ENOERR，失败返回EINVAL
 * @note 本实现中无需释放资源，仅检查参数有效性
 */
int pthread_attr_destroy(pthread_attr_t *attr)
{
    if (attr == NULL) {  // 检查属性对象指针是否为空
        return EINVAL;   // 空指针返回参数无效错误
    }

    /* Nothing to do here... */  // 无需执行实际销毁操作
    return ENOERR;  // 销毁成功
}

/**
 * @brief 设置线程分离状态属性
 * @param[in,out] attr 线程属性对象指针
 * @param[in] detachState 分离状态：PTHREAD_CREATE_JOINABLE（可连接）或PTHREAD_CREATE_DETACHED（分离）
 * @return 成功返回ENOERR，失败返回EINVAL
 */
int pthread_attr_setdetachstate(pthread_attr_t *attr, int detachState)
{
    // 检查参数有效性并验证分离状态值
    if ((attr != NULL) && ((detachState == PTHREAD_CREATE_JOINABLE) || (detachState == PTHREAD_CREATE_DETACHED))) {
        attr->detachstate = (UINT32)detachState;  // 设置分离状态
        return ENOERR;                            // 设置成功
    }

    return EINVAL;  // 参数无效或分离状态值非法
}

/**
 * @brief 获取线程分离状态属性
 * @param[in] attr 线程属性对象指针
 * @param[out] detachState 分离状态输出指针
 * @return 成功返回ENOERR，失败返回EINVAL
 */
int pthread_attr_getdetachstate(const pthread_attr_t *attr, int *detachState)
{
    if ((attr == NULL) || (detachState == NULL)) {  // 检查输入输出指针是否为空
        return EINVAL;                               // 空指针返回参数无效错误
    }

    *detachState = (int)attr->detachstate;  // 获取分离状态值

    return ENOERR;  // 获取成功
}

/**
 * @brief 设置线程作用域属性
 * @param[in,out] attr 线程属性对象指针
 * @param[in] scope 作用域：PTHREAD_SCOPE_PROCESS（进程内）或PTHREAD_SCOPE_SYSTEM（系统级）
 * @return 成功返回ENOERR，不支持返回ENOTSUP，失败返回EINVAL
 * @note 仅支持PTHREAD_SCOPE_PROCESS作用域
 */
int pthread_attr_setscope(pthread_attr_t *attr, int scope)
{
    if (attr == NULL) {  // 检查属性对象指针是否为空
        return EINVAL;   // 空指针返回参数无效错误
    }

    if (scope == PTHREAD_SCOPE_PROCESS) {  // 进程内作用域
        attr->scope = (unsigned int)scope;  // 设置作用域
        return ENOERR;                      // 设置成功
    }

    if (scope == PTHREAD_SCOPE_SYSTEM) {  // 系统级作用域
        return ENOTSUP;                   // 不支持系统级作用域
    }

    return EINVAL;  // 作用域值非法
}

/**
 * @brief 获取线程作用域属性
 * @param[in] attr 线程属性对象指针
 * @param[out] scope 作用域输出指针
 * @return 成功返回ENOERR，失败返回EINVAL
 */
int pthread_attr_getscope(const pthread_attr_t *attr, int *scope)
{
    if ((attr == NULL) || (scope == NULL)) {  // 检查输入输出指针是否为空
        return EINVAL;                         // 空指针返回参数无效错误
    }

    *scope = (int)attr->scope;  // 获取作用域值

    return ENOERR;  // 获取成功
}

/**
 * @brief 设置线程调度继承属性
 * @param[in,out] attr 线程属性对象指针
 * @param[in] inherit 继承方式：PTHREAD_INHERIT_SCHED（继承）或PTHREAD_EXPLICIT_SCHED（显式）
 * @return 成功返回ENOERR，失败返回EINVAL
 */
int pthread_attr_setinheritsched(pthread_attr_t *attr, int inherit)
{
    // 检查参数有效性并验证继承方式
    if ((attr != NULL) && ((inherit == PTHREAD_INHERIT_SCHED) || (inherit == PTHREAD_EXPLICIT_SCHED))) {
        attr->inheritsched = (UINT32)inherit;  // 设置继承方式
        return ENOERR;                         // 设置成功
    }

    return EINVAL;  // 参数无效或继承方式非法
}

/**
 * @brief 获取线程调度继承属性
 * @param[in] attr 线程属性对象指针
 * @param[out] inherit 继承方式输出指针
 * @return 成功返回ENOERR，失败返回EINVAL
 */
int pthread_attr_getinheritsched(const pthread_attr_t *attr, int *inherit)
{
    if ((attr == NULL) || (inherit == NULL)) {  // 检查输入输出指针是否为空
        return EINVAL;                          // 空指针返回参数无效错误
    }

    *inherit = (int)attr->inheritsched;  // 获取继承方式

    return ENOERR;  // 获取成功
}

/**
 * @brief 设置线程调度策略
 * @param[in,out] attr 线程属性对象指针
 * @param[in] policy 调度策略：仅支持SCHED_RR（轮转调度）
 * @return 成功返回ENOERR，失败返回EINVAL
 * @note 目前仅实现SCHED_RR调度策略
 */
int pthread_attr_setschedpolicy(pthread_attr_t *attr, int policy)
{
    if ((attr != NULL) && (policy == SCHED_RR)) {  // 检查参数并验证为RR调度
        attr->schedpolicy = SCHED_RR;              // 设置RR调度策略
        return ENOERR;                             // 设置成功
    }

    return EINVAL;  // 参数无效或不支持的调度策略
}

/**
 * @brief 获取线程调度策略
 * @param[in] attr 线程属性对象指针
 * @param[out] policy 调度策略输出指针
 * @return 成功返回ENOERR，失败返回EINVAL
 */
int pthread_attr_getschedpolicy(const pthread_attr_t *attr, int *policy)
{
    if ((attr == NULL) || (policy == NULL)) {  // 检查输入输出指针是否为空
        return EINVAL;                          // 空指针返回参数无效错误
    }

    *policy = (int)attr->schedpolicy;  // 获取调度策略

    return ENOERR;  // 获取成功
}

/**
 * @brief 设置线程调度参数（优先级）
 * @param[in,out] attr 线程属性对象指针
 * @param[in] param 调度参数结构体，包含优先级信息
 * @return 成功返回ENOERR，不支持返回ENOTSUP，失败返回EINVAL
 * @note 优先级需在[0, OS_TASK_PRIORITY_LOWEST]范围内
 */
int pthread_attr_setschedparam(pthread_attr_t *attr, const struct sched_param *param)
{
    if ((attr == NULL) || (param == NULL)) {  // 检查参数指针是否为空
        return EINVAL;                         // 空指针返回参数无效错误
    } else if ((param->sched_priority < 0) || (param->sched_priority > OS_TASK_PRIORITY_LOWEST)) {  // 检查优先级范围
        return ENOTSUP;                                                                           // 优先级超出范围
    }

    attr->schedparam = *param;  // 设置调度参数

    return ENOERR;  // 设置成功
}

/**
 * @brief 获取线程调度参数（优先级）
 * @param[in] attr 线程属性对象指针
 * @param[out] param 调度参数结构体输出指针
 * @return 成功返回ENOERR，失败返回EINVAL
 */
int pthread_attr_getschedparam(const pthread_attr_t *attr, struct sched_param *param)
{
    if ((attr == NULL) || (param == NULL)) {  // 检查输入输出指针是否为空
        return EINVAL;                         // 空指针返回参数无效错误
    }

    *param = attr->schedparam;  // 获取调度参数

    return ENOERR;  // 获取成功
}

/**
 * @brief 设置线程栈地址
 * @param[in,out] attr 线程属性对象指针
 * @param[in] stackAddr 栈起始地址
 * @return 成功返回ENOERR，失败返回EINVAL
 * @note 栈地址的实际位置（栈顶/栈底）取决于栈增长方向（向上/向下）
 */
int pthread_attr_setstackaddr(pthread_attr_t *attr, void *stackAddr)
{
    if (attr == NULL) {  // 检查属性对象指针是否为空
        return EINVAL;   // 空指针返回参数无效错误
    }

    attr->stackaddr_set = 1;  // 标记栈地址已设置
    attr->stackaddr     = stackAddr;  // 设置栈地址

    return ENOERR;  // 设置成功
}

/**
 * @brief 获取线程栈地址
 * @param[in] attr 线程属性对象指针
 * @param[out] stackAddr 栈地址输出指针
 * @return 成功返回ENOERR，失败返回EINVAL
 * @note 仅当栈地址已设置（stackaddr_set=1）时返回成功
 */
int pthread_attr_getstackaddr(const pthread_attr_t *attr, void **stackAddr)
{
    // 检查参数有效性并确认栈地址已设置
    if (((attr != NULL) && (stackAddr != NULL)) && attr->stackaddr_set) {
        *stackAddr = attr->stackaddr;  // 获取栈地址
        return ENOERR;                 // 获取成功
    }

    return EINVAL;  // Stack address not set, return EINVAL.  // 栈地址未设置或参数无效
}

/**
 * @brief 设置线程栈大小
 * @param[in,out] attr 线程属性对象指针
 * @param[in] stackSize 栈大小（字节），需大于等于PTHREAD_STACK_MIN
 * @return 成功返回ENOERR，失败返回EINVAL
 * @note 拒绝过小的栈大小请求
 */
int pthread_attr_setstacksize(pthread_attr_t *attr, size_t stackSize)
{
    /* Reject inadequate stack sizes */  // 拒绝不充足的栈大小
    if ((attr == NULL) || (stackSize < PTHREAD_STACK_MIN)) {  // 检查参数和栈大小最小值
        return EINVAL;                                       // 参数无效或栈大小不足
    }

    attr->stacksize_set = 1;  // 标记栈大小已设置
    attr->stacksize     = stackSize;  // 设置栈大小

    return ENOERR;  // 设置成功
}

/**
 * @brief 获取线程栈大小
 * @param[in] attr 线程属性对象指针
 * @param[out] stackSize 栈大小输出指针
 * @return 成功返回ENOERR，失败返回EINVAL
 * @note 仅当栈大小已设置（stacksize_set=1）时返回成功
 */
int pthread_attr_getstacksize(const pthread_attr_t *attr, size_t *stackSize)
{
    /* Reject attempts to get a stack size when one has not been set. */  // 栈大小未设置时拒绝获取
    if ((attr == NULL) || (stackSize == NULL) || (!attr->stacksize_set)) {  // 检查参数和栈大小设置状态
        return EINVAL;                                                     // 参数无效或栈大小未设置
    }

    *stackSize = attr->stacksize;  // 获取栈大小

    return ENOERR;  // 获取成功
}

/**
 * @brief 设置线程CPU亲和性掩码
 * @param[in,out] attr 线程属性对象指针
 * @param[in] cpusetsize 掩码大小（字节）
 * @param[in] cpuset CPU亲和性掩码指针
 * @return 成功返回ENOERR，失败返回EINVAL
 * @note 仅在LOSCFG_KERNEL_SMP配置下有效
 */
int pthread_attr_setaffinity_np(pthread_attr_t* attr, size_t cpusetsize, const cpu_set_t* cpuset)
{
#ifdef LOSCFG_KERNEL_SMP  // SMP配置下才支持CPU亲和性
    if (attr == NULL) {  // 检查属性对象指针是否为空
        return EINVAL;   // 空指针返回参数无效错误
    }

    if ((cpuset == NULL) || (cpusetsize == 0)) {  // 空掩码或大小为0
        attr->cpuset.__bits[0] = 0;               // 清空亲和性掩码
        return ENOERR;                            // 设置成功
    }

    // 检查掩码大小和CPU范围有效性
    if ((cpusetsize != sizeof(cpu_set_t)) || (cpuset->__bits[0] > LOSCFG_KERNEL_CPU_MASK)) {
        return EINVAL;  // 掩码大小不匹配或CPU超出范围
    }

    attr->cpuset = *cpuset;  // 设置CPU亲和性掩码
#endif

    return ENOERR;  // 非SMP配置下直接返回成功
}

/**
 * @brief 获取线程CPU亲和性掩码
 * @param[in] attr 线程属性对象指针
 * @param[in] cpusetsize 掩码大小（字节）
 * @param[out] cpuset CPU亲和性掩码输出指针
 * @return 成功返回ENOERR，失败返回EINVAL
 * @note 仅在LOSCFG_KERNEL_SMP配置下有效
 */
int pthread_attr_getaffinity_np(const pthread_attr_t* attr, size_t cpusetsize, cpu_set_t* cpuset)
{
#ifdef LOSCFG_KERNEL_SMP  // SMP配置下才支持CPU亲和性
    if ((attr == NULL) || (cpuset == NULL) || (cpusetsize != sizeof(cpu_set_t))) {  // 检查参数有效性
        return EINVAL;                                                             // 参数无效
    }

    *cpuset = attr->cpuset;  // 获取CPU亲和性掩码
#endif

    return ENOERR;  // 非SMP配置下直接返回成功
}
