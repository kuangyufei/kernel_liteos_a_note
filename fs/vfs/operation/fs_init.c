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

#include "disk_pri.h"
#include "fcntl.h"
#include "fs/fs.h"
#include "fs/fs_operation.h"
#include "linux/spinlock.h"
#include "los_printf.h"
#include "fs/mount.h"
#include "fs/path_cache.h"
#include "sys/statfs.h"
#include "unistd.h"
#include "fs/vfs_util.h"
#include "fs/vnode.h"

void los_vfs_init(void)
{
    uint retval;
    static bool g_vfs_init = false;
    if (g_vfs_init) {
        return;
    }

#ifdef LOSCFG_FS_FAT_DISK
    spin_lock_init(&g_diskSpinlock);
    spin_lock_init(&g_diskFatBlockSpinlock);
#endif
    files_initialize();
    files_initlist(&tg_filelist);

    retval = VnodesInit();
    if (retval != LOS_OK) {
        PRINT_ERR("los_vfs_init VnodeInit failed error %d\n", retval);
        return;
    }

    retval = PathCacheInit();
    if (retval != LOS_OK) {
        PRINT_ERR("los_vfs_init PathCacheInit failed error %d\n", retval);
        return;
    }
    retval = VnodeHashInit();
    if (retval != LOS_OK) {
        PRINT_ERR("los_vfs_init VnodeHashInit failed error %d\n", retval);
        return;
    }

    retval = VnodeDevInit();
    if (retval != LOS_OK) {
        PRINT_ERR("los_vfs_init VnodeDevInit failed error %d\n", retval);
        return;
    }
 
#ifdef LOSCFG_KERNEL_VM
    retval = init_file_mapping();
    if (retval != LOS_OK) {
        PRINT_ERR("Page cache file map init failed\n");
        return;
    }
#endif
    g_vfs_init = true;
}
