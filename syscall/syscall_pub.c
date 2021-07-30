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
#include "syscall_pub.h"

int CheckRegion(const LosVmSpace *space, VADDR_T ptr, size_t len)
{
    LosVmMapRegion *region = LOS_RegionFind((LosVmSpace *)space, ptr);
    if (region == NULL) {
        return -1;
    }
    if (ptr + len <= region->range.base + region->range.size) {
        return 0;
    }
    return CheckRegion(space, region->range.base + region->range.size,
                       (ptr + len) - (region->range.base + region->range.size));
}

void *DupUserMem(const void *ptr, size_t len, int needCopy)
{
    void *p = LOS_MemAlloc(OS_SYS_MEM_ADDR, len);

    if (p == NULL) {
        set_errno(ENOMEM);
        return NULL;
    }

    if (needCopy && LOS_ArchCopyFromUser(p, ptr, len) != 0) {
        LOS_MemFree(OS_SYS_MEM_ADDR, p);
        set_errno(EFAULT);
        return NULL;
    }

    return p;
}

int GetFullpath(int fd, const char *path, char **fullpath)
{
    int ret = 0;
    char *pathRet = NULL;
    struct file *file = NULL;
    struct stat bufRet = {0};

    if (path != NULL) {
        ret = UserPathCopy(path, &pathRet);
        if (ret != 0) {
            goto OUT;
        }
    }

    if ((pathRet != NULL) && (*pathRet == '/')) {
        *fullpath = pathRet;
        pathRet = NULL;
    } else {
        if (fd != AT_FDCWD) {
            /* Process fd convert to system global fd */
            fd = GetAssociatedSystemFd(fd);
        }
        ret = fs_getfilep(fd, &file);
        if (file) {
            ret = stat(file->f_path, &bufRet);
            if (!ret) {
                if (!S_ISDIR(bufRet.st_mode)) {
                    set_errno(ENOTDIR);
                    ret = -ENOTDIR;
                    goto OUT;
                }
            }
        }
        ret = vfs_normalize_pathat(fd, pathRet, fullpath);
    }

OUT:
    PointerFree(pathRet);
    return ret;
}

int UserPathCopy(const char *userPath, char **pathBuf)
{
    int ret;

    *pathBuf = (char *)LOS_MemAlloc(OS_SYS_MEM_ADDR, PATH_MAX + 1);
    if (*pathBuf == NULL) {
        return -ENOMEM;
    }

    ret = LOS_StrncpyFromUser(*pathBuf, userPath, PATH_MAX + 1);
    if (ret < 0) {
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, *pathBuf);
        *pathBuf = NULL;
        return ret;
    } else if (ret > PATH_MAX) {
        (void)LOS_MemFree(OS_SYS_MEM_ADDR, *pathBuf);
        *pathBuf = NULL;
        return -ENAMETOOLONG;
    }
    (*pathBuf)[ret] = '\0';

    return 0;
}
