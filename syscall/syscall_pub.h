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
#ifndef _SYSCALL_PUB_H
#define _SYSCALL_PUB_H

#include <stdlib.h>
#include "los_memory.h"
#include "los_vm_lock.h"
#include "los_vm_map.h"
#include "user_copy.h"
#include "fs/fs.h"
#include "fcntl.h"
#include "los_strncpy_from_user.h"

extern int CheckRegion(const LosVmSpace *space, VADDR_T ptr, size_t len);
extern void *DupUserMem(const void *ptr, size_t len, int needCopy);
extern int GetFullpath(int fd, const char *path, char **fullpath);
extern int UserPathCopy(const char *userPath, char **pathBuf);

#define CHECK_ASPACE(ptr, len, ...) \
    do { \
        if (ptr != NULL && len != 0) { \
            if (!LOS_IsUserAddressRange((VADDR_T)(UINTPTR)ptr, len)) { \
                set_errno(EFAULT); \
                __VA_ARGS__; \
                return -get_errno(); \
            } \
            LosVmSpace *__aspace = OsCurrProcessGet()->vmSpace; \
            (VOID)LOS_MuxAcquire(&__aspace->regionMux); \
            if (CheckRegion(__aspace, (VADDR_T)(UINTPTR)ptr, len) == -1) { \
                (VOID)LOS_MuxRelease(&__aspace->regionMux); \
                set_errno(EFAULT); \
                __VA_ARGS__; \
                return -get_errno(); \
            } \
            (VOID)LOS_MuxRelease(&__aspace->regionMux); \
        } \
    } while (0)

#define LEN(ptr) ((ptr) ? *(ptr) : 0)

#define DUP_FROM_USER_(ptr, size, copy, ...) \
    __typeof(ptr) ptr##bak = ptr; \
    if (ptr != NULL && (size) != 0) { \
        ptr = DupUserMem(ptr, size, copy); \
        if (ptr == NULL) { \
            ptr = ptr##bak; \
            __VA_ARGS__; \
            return -get_errno(); \
        } \
    }

/*
DUP_FROM_USER(ptr, size, ...) can not deal with "char *";
Please deal with the "char *" by function:UserPathCopy.
*/
#define DUP_FROM_USER(ptr, size, ...) \
    DUP_FROM_USER_(ptr, size, 1, ##__VA_ARGS__)

#define DUP_FROM_USER_NOCOPY(ptr, size, ...) \
    DUP_FROM_USER_(ptr, size, 0, ##__VA_ARGS__)

#define DUP_TO_USER(ptr, size, ...) \
    do { \
        if (ptr != NULL && (size) != 0) { \
            if (LOS_ArchCopyToUser(ptr##bak, ptr, size) != 0) { \
                set_errno(EFAULT); \
                __VA_ARGS__; \
                return -get_errno(); \
            } \
        } \
    } while (0)

#define FREE_DUP(ptr) \
    do { \
        if (ptr != ptr##bak) { \
            LOS_MemFree(OS_SYS_MEM_ADDR, (void*)ptr); \
            ptr = ptr##bak; \
        } \
    } while (0)

#define CPY_FROM_USER(ptr) \
    __typeof(*ptr) ptr##cpy = {0}, *ptr##bak = ptr; \
    if (ptr != NULL) { \
        if (LOS_ArchCopyFromUser((void*)&ptr##cpy, ptr, sizeof(*ptr)) != 0) { \
            set_errno(EFAULT); \
            return -get_errno(); \
        } \
        ptr = &ptr##cpy; \
    }

#define CPY_TO_USER(ptr, ...) \
    if (ptr != NULL) { \
        if (LOS_ArchCopyToUser(ptr##bak, ptr, sizeof(*ptr)) != 0) { \
            set_errno(EFAULT); \
            __VA_ARGS__; \
            return -get_errno(); \
        } \
    }

/** Macros for sendmsg and recvmsg */

#define CONST_CAST(ptr) ((__typeof(ptr##_NONCONST))ptr)

#define CHECK_FIELD_ASPACE(ptr, field, len) \
    do { \
        if (ptr != NULL) { \
            CHECK_ASPACE(ptr->field, len); \
        } \
    } while (0)

#define CHECK_ARRAY_FIELD_ASPACE(ptr, arr, arrlen, field, len, ...) \
    do { \
        if (ptr != NULL && ptr->arr != NULL) { \
            for (size_t i = 0; i < arrlen; i++) { \
                CHECK_ASPACE(ptr->arr[i].field, ptr->arr[i].len, ##__VA_ARGS__); \
            } \
        } \
    } while (0)

#define DUP_FIELD_FROM_USER_(ptr, field, size, copy, ...) \
    do { \
        if (ptr != NULL && ptr->field != NULL && (size) != 0) { \
            CONST_CAST(ptr)->field = DupUserMem(ptr->field, size, copy); \
            if (ptr->field == NULL) { \
                __VA_ARGS__; \
                return -get_errno(); \
            } \
        } \
    } while (0)

#define DUP_FIELD_FROM_USER(ptr, field, size, ...) \
    DUP_FIELD_FROM_USER_(ptr, field, size, 1, ##__VA_ARGS__)

#define DUP_FIELD_FROM_USER_NOCOPY(ptr, field, size, ...) \
    DUP_FIELD_FROM_USER_(ptr, field, size, 0, ##__VA_ARGS__)

/* backup the arr to ptr##arr */
#define DUP_ARRAY_FIELD_FROM_USER_(ext, ptr, arr, arrlen, field, len, ...) \
    __typeof(*ptr##_NONCONST) ptr##arr##cpy = ptr##cpybak, ptr##arr##cpybak = ptr##cpybak; \
    __typeof(ptr##_NONCONST) ptr##arr = ptr ? &ptr##arr##cpy : NULL, ptr##arr##_NONCONST = NULL; \
    DUP_FIELD_FROM_USER(ptr##arr, arr, arrlen * sizeof(ptr->arr[0]), ##__VA_ARGS__); \
    if (ptr != NULL && ptr->arr != NULL) { \
        size_t i = 0; \
        for (; i < arrlen; i++) { \
            DUP_FIELD_FROM_USER##ext(ptr, arr[i].field, ptr->arr[i].len, break); \
        } \
        if (i != arrlen) { \
            FREE_DUP_ARRAY_FIELD(ptr, arr, i, field); \
            __VA_ARGS__; \
            return -get_errno(); \
        } \
    }

#define DUP_ARRAY_FIELD_FROM_USER(ptr, arr, arrlen, field, len, ...) \
    DUP_ARRAY_FIELD_FROM_USER_(, ptr, arr, arrlen, field, len, ##__VA_ARGS__)

#define DUP_ARRAY_FIELD_FROM_USER_NOCOPY(ptr, arr, arrlen, field, len, ...) \
    DUP_ARRAY_FIELD_FROM_USER_(_NOCOPY, ptr, arr, arrlen, field, len, ##__VA_ARGS__)

#define FREE_DUP_FIELD(ptr, field) \
    do { \
        if (ptr != NULL && ptr->field != ptr##cpybak.field) { \
            LOS_MemFree(OS_SYS_MEM_ADDR, (void*)ptr->field); \
            CONST_CAST(ptr)->field = ptr##cpybak.field; \
        } \
    } while (0)

/* use and free the backuped arr in ptr##arr */
#define FREE_DUP_ARRAY_FIELD(ptr, arr, arrlen, field) \
    if (ptr != NULL && ptr->arr != NULL && arrlen != 0) { \
        __typeof(ptr##cpybak.arr) tmp = ptr##cpybak.arr; \
        ptr##cpybak.arr = ptr##arr->arr; \
        for (size_t j = 0; j < arrlen; j++) { \
            FREE_DUP_FIELD(ptr, arr[j].field); \
        } \
        ptr##cpybak.arr = tmp; \
    } \
    FREE_DUP_FIELD(ptr##arr, arr);

#define CPY_FROM_CONST_USER(NonConstType, ptr) \
    CPY_FROM_USER(ptr); \
    NonConstType *ptr##_NONCONST = NULL, ptr##cpybak = ptr##cpy; \
    (void)ptr##bak;

#define CPY_FROM_NONCONST_USER(ptr) \
    CPY_FROM_USER(ptr); \
    __typeof(*ptr) *ptr##_NONCONST = NULL, ptr##cpybak = ptr##cpy;

#define DUP_FIELD_TO_USER(ptr, field, size, ...) \
    do { \
        if (ptr != NULL && ptr->field != NULL && (size) != 0) { \
            if (LOS_ArchCopyToUser(ptr##cpybak.field, ptr->field, size) != 0 || \
                LOS_ArchCopyToUser(&ptr##bak->field, &ptr##cpybak.field, sizeof(__typeof(ptr##cpybak.field))) != 0) { \
                set_errno(EFAULT); \
                __VA_ARGS__; \
                return -get_errno(); \
            } \
        } \
    } while (0)

/* use the backuped arr from ptr##arr */
#define DUP_ARRAY_FIELD_TO_USER(ptr, arr, arrlen, field, len, ...) \
    if (ptr != NULL && ptr->arr != NULL) { \
        __typeof(ptr##cpybak.arr) tmp = ptr##cpybak.arr; \
        __typeof(ptr##bak) tmp2 = ptr##bak; \
        ptr##cpybak.arr = ptr##arr->arr; \
        ptr##arr->arr = tmp; \
        ptr##bak = ptr##arr; \
        for (size_t i = 0; i < arrlen; i++) { \
            DUP_FIELD_TO_USER(ptr, arr[i].field, ptr->arr[i].len, ##__VA_ARGS__); \
        } \
        ptr##bak = tmp2; \
        ptr##arr->arr = ptr##cpybak.arr; \
        ptr##cpybak.arr = tmp; \
    }

#define PointerFree(ptr) \
    do { \
        if (ptr != NULL) { \
            LOS_MemFree(OS_SYS_MEM_ADDR, (void*)ptr); \
        } \
    } while (0)
#endif
