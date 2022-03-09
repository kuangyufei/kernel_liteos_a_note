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

#include "los_mux.h"
#include "los_queue.h"
#include "los_sem.h"
#include "los_task.h"

//错误适配
int map_errno(UINT32 err)
{
    if (err == LOS_OK) {
        return ENOERR; //成功
    }
    switch (err) {
        case LOS_ERRNO_QUEUE_INVALID:
        case LOS_ERRNO_QUEUE_WRITE_PTR_NULL:
        case LOS_ERRNO_QUEUE_WRITESIZE_ISZERO:
        case LOS_ERRNO_QUEUE_SIZE_TOO_BIG:
        case LOS_ERRNO_QUEUE_CREAT_PTR_NULL:
        case LOS_ERRNO_QUEUE_PARA_ISZERO:
        case LOS_ERRNO_QUEUE_WRITE_SIZE_TOO_BIG:
            errno = EINVAL; //Invalid argument	无效的参数
            break;
        case LOS_ERRNO_QUEUE_ISFULL:
        case LOS_ERRNO_QUEUE_ISEMPTY:
            errno = EAGAIN;//资源暂时不可用
            break;
        case LOS_ERRNO_QUEUE_CREATE_NO_MEMORY:
            errno = ENOSPC;//	No space left on device	设备上没有空间
            break;
        case LOS_ERRNO_QUEUE_TIMEOUT:
            errno = ETIMEDOUT;
            break;
        case LOS_ERRNO_QUEUE_CB_UNAVAILABLE:
            errno = ENFILE;//File table overflow	打开太多的文件系统
            break;
        case LOS_ERRNO_QUEUE_READ_IN_INTERRUPT:
        case LOS_ERRNO_QUEUE_WRITE_IN_INTERRUPT:
            errno = EINTR;//Interrupted system call	系统调用被中断
            break;
        case LOS_ERRNO_TSK_ID_INVALID:
        case LOS_ERRNO_TSK_PTR_NULL:
        case LOS_ERRNO_TSK_NAME_EMPTY:
        case LOS_ERRNO_TSK_ENTRY_NULL:
        case LOS_ERRNO_TSK_PRIOR_ERROR:
        case LOS_ERRNO_TSK_STKSZ_TOO_LARGE:
        case LOS_ERRNO_TSK_STKSZ_TOO_SMALL:
        case LOS_ERRNO_TSK_NOT_CREATED:
        case LOS_ERRNO_TSK_CPU_AFFINITY_MASK_ERR:
        case OS_ERROR:
        case LOS_ERRNO_SEM_INVALID:
        case LOS_ERRNO_SEM_UNAVAILABLE:
            errno = EINVAL;
            break;
        case LOS_ERRNO_TSK_TCB_UNAVAILABLE:
        case LOS_ERRNO_TSK_MP_SYNC_RESOURCE:
        case LOS_ERRNO_SEM_ALL_BUSY:
            errno = ENOSPC;
            break;
        case LOS_ERRNO_TSK_NO_MEMORY:
        case LOS_ERRNO_SEM_OVERFLOW:
            errno = ENOMEM;
            break;
        case LOS_ERRNO_SEM_PENDED:
        case LOS_ERRNO_EVENT_SHOULD_NOT_DESTROY:
            errno = EBUSY;
            break;
        case LOS_ERRNO_SEM_PEND_IN_LOCK:
            errno = EPERM;
            break;
        case LOS_ERRNO_SEM_PEND_INTERR:
            errno = EINTR;
            break;
        case LOS_ERRNO_SEM_TIMEOUT:
            errno = ETIMEDOUT;
            break;
        default:
            errno = EINVAL;
            break;
    }
    return errno;
}

