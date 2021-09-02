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

#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#ifdef LOSCFG_FS_VFS
#include <fs/fs.h>

off_t _lseek(int fd, off_t offset, int whence)
{
    int ret;
    struct file *filep = NULL;

    /* Get the file structure corresponding to the file descriptor. */
    ret = fs_getfilep(fd, &filep);
    if (ret < 0) {
        /* The errno value has already been set */
        return (off_t)-get_errno();
    }

    /* libc seekdir function should set the whence to SEEK_SET, so we can discard
     * the whence argument here */
    if (filep->f_oflags & O_DIRECTORY) {
        /* defensive coding */
        if (filep->f_dir == NULL) {
            return (off_t)-EINVAL;
        }
        if (offset == 0) {
            rewinddir(filep->f_dir);
        } else {
            seekdir(filep->f_dir, offset);
        }
        ret = telldir(filep->f_dir);
        if (ret < 0) {
            return (off_t)-get_errno();
        }
        return ret;
    }

    /* Then let file_seek do the real work */
    ret = file_seek(filep, offset, whence);
    if (ret < 0) {
        return -get_errno();
    }
    return ret;
}

off64_t _lseek64(int fd, int offsetHigh, int offsetLow, off64_t *result, int whence)
{
    off64_t ret;
    struct file *filep = NULL;
    off64_t offset = ((off64_t)offsetHigh << 32) + (uint)offsetLow; /* 32: offsetHigh is high 32 bits */

    /* Get the file structure corresponding to the file descriptor. */
    ret = fs_getfilep(fd, &filep);
    if (ret < 0) {
        /* The errno value has already been set */
        return (off64_t)-get_errno();
    }

    /* libc seekdir function should set the whence to SEEK_SET, so we can discard
     * the whence argument here */
    if (filep->f_oflags & O_DIRECTORY) {
        /* defensive coding */
        if (filep->f_dir == NULL) {
            return (off64_t)-EINVAL;
        }
        if (offsetLow == 0) {
            rewinddir(filep->f_dir);
        } else {
            seekdir(filep->f_dir, offsetLow);
        }
        ret = telldir(filep->f_dir);
        if (ret < 0) {
            return (off64_t)-get_errno();
        }
        goto out;
    }

    /* Then let file_seek do the real work */
    ret = file_seek64(filep, offset, whence);
    if (ret < 0) {
        return (off64_t)-get_errno();
    }

out:
    *result = ret;

    return 0;
}

#endif
