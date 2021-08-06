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

#ifndef _VIRPART_H
#define _VIRPART_H

#include "integer.h"
#include "disk.h"

#define _MAX_ENTRYLENGTH  16                                       /* MAX virtual partition name length */
#define _MAX_VIRVOLUMES   5                                        /* MAX virtual partition number */
typedef struct virtual_partition_info {
    char *devpartpath;                                             /* need set virtual partition, e.g. /dev/mmcblk0p0 */
    int  virpartnum;                                               /* virtual partition numbers, MAX number is 5 */
    double virpartpercent[_MAX_VIRVOLUMES];                        /* every virtual partition percent,e.g 0.6,0.3,0.1 */
    char virpartname[_MAX_VIRVOLUMES][_MAX_ENTRYLENGTH + 1];       /* every virtual partition name, MAX length is 16 */
} virpartinfo;

extern char g_devPartName[DISK_NAME + 1];

INT FatFsBindVirPart(void *handle, BYTE vol);

INT FatFsUnbindVirPart(void *handle);

INT FatFsMakeVirPart(void *handle, BYTE vol);

#endif /* _VIRPART_H */