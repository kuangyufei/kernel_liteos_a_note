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

#include "fcntl.h"
#include "linux/kernel.h"

static int MemOpen(struct file *filep)
{
    return 0;
}

static int MemClose(struct file *filep)
{
    return 0;
}

static ssize_t MemRead(FAR struct file *filep, FAR char *buffer, size_t buflen)
{
    return 0;
}

static ssize_t MemWrite(FAR struct file *filep, FAR const char *buffer, size_t buflen)
{
    return 0;
}

static ssize_t MemMap(FAR struct file *filep, FAR LosVmMapRegion *region)
{
#ifdef LOSCFG_KERNEL_VM
    size_t size = region->range.size;
    PADDR_T paddr = region->pgOff << PAGE_SHIFT;
    VADDR_T vaddr = region->range.base;
    LosVmSpace *space = LOS_SpaceGet(vaddr);

    if ((paddr >= SYS_MEM_BASE) && (paddr < SYS_MEM_END)) {
        return -EINVAL;
    }

    /* Peripheral register memory adds strongly ordered attributes */
    region->regionFlags |= VM_MAP_REGION_FLAG_STRONGLY_ORDERED;

    if (space == NULL) {
        return -EAGAIN;
    }
    if (LOS_ArchMmuMap(&space->archMmu, vaddr, paddr, size >> PAGE_SHIFT, region->regionFlags) <= 0) {
        return -EAGAIN;
    }
#else
    UNUSED(filep);
    UNUSED(region);
#endif
    return 0;
}

static const struct file_operations_vfs g_memDevOps = {
    MemOpen,  /* open */
    MemClose, /* close */
    MemRead,  /* read */
    MemWrite, /* write */
    NULL,      /* seek */
    NULL,      /* ioctl */
    MemMap,   /* mmap */
#ifndef CONFIG_DISABLE_POLL
    NULL,      /* poll */
#endif
    NULL,      /* unlink */
};

int DevMemRegister(void)
{
    return register_driver("/dev/mem", &g_memDevOps, 0666, 0); /* 0666: file mode */
}
