/*
 * Copyright (c) 2021-2021 Huawei Device Co., Ltd. All rights reserved.
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

#include "fs/mount.h"
#include "fs/vfs_util.h"
#include "fs/path_cache.h"
#include "fs/vnode.h"
#ifdef LOSCFG_DRIVERS_RANDOM
#include "hisoc/random.h"
#else
#include "stdlib.h"
#endif

static LIST_HEAD *g_mountList = NULL;//装载链表,上面挂的是系统所有的装载设备
/*	在内核MountAlloc只被VnodeDevInit调用,但真实情况下它还将被系统调用 mount()调用
* int mount(const char *source, const char *target,
          const char *filesystemtype, unsigned long mountflags,
          const void *data)
  mount见于..\code-2.0-canary\third_party\NuttX\fs\mount\fs_mount.c
  vnodeBeCovered: /dev/mmcblk0 
*/
struct Mount* MountAlloc(struct Vnode* vnodeBeCovered, struct MountOps* fsop)
{
    struct Mount* mnt = (struct Mount*)zalloc(sizeof(struct Mount));//申请一个mount结构体内存,小内存分配用 zalloc
    if (mnt == NULL) {
        PRINT_ERR("MountAlloc failed no memory!\n");
        return NULL;
    }

    LOS_ListInit(&mnt->activeVnodeList);//初始化激活索引节点链表
    LOS_ListInit(&mnt->vnodeList);//初始化索引节点链表

    mnt->vnodeBeCovered = vnodeBeCovered;//设备将装载到vnodeBeCovered节点上
    vnodeBeCovered->newMount = mnt;//该节点不再是虚拟节点,而作为 设备结点
#ifdef LOSCFG_DRIVERS_RANDOM	//随机值	驱动模块
    HiRandomHwInit();//随机值初始化
    (VOID)HiRandomHwGetInteger(&mnt->hashseed);//用于生成哈希种子
    HiRandomHwDeinit();//随机值反初始化
#else
    mnt->hashseed = (uint32_t)random(); //随机生成哈子种子
#endif
    return mnt;
}
//获取装载链表
LIST_HEAD* GetMountList()
{
    if (g_mountList == NULL) {
        g_mountList = zalloc(sizeof(LIST_HEAD));//分配内存, 小内存分配用 zalloc
        if (g_mountList == NULL) {
            PRINT_ERR("init mount list failed, no memory.");
            return NULL;
        }
        LOS_ListInit(g_mountList);//初始化全局链表
    }
    return g_mountList;//所有文件系统的挂载信息
}
