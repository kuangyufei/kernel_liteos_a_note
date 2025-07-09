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
#include "path_cache.h"
#include "vnode.h"
#ifdef LOSCFG_DRIVERS_RANDOM
#include "hisoc/random.h"
#else
#include "stdlib.h"
#endif
#ifdef LOSCFG_MNT_CONTAINER                          // 如果启用容器挂载功能
#include "los_mnt_container_pri.h"                  // 包含容器挂载私有头文件
static LIST_HEAD *g_mountCache = NULL;               // 挂载缓存列表指针
#else                                                // 否则(不启用容器挂载功能)
static LIST_HEAD *g_mountList = NULL;                // 挂载列表指针
#endif

/**
 * @brief 分配并初始化挂载结构体
 * @param vnodeBeCovered 被覆盖的虚拟节点
 * @param fsop 文件系统操作集
 * @return 成功返回挂载结构体指针，失败返回NULL
 */
struct Mount *MountAlloc(struct Vnode *vnodeBeCovered, struct MountOps *fsop)
{
    struct Mount *mnt = (struct Mount *)zalloc(sizeof(struct Mount));  // 分配挂载结构体内存
    if (mnt == NULL) {                                                // 检查内存分配是否失败
        PRINT_ERR("MountAlloc failed no memory!\n");                   // 输出内存分配失败错误信息
        return NULL;                                                  // 返回NULL表示失败
    }

    LOS_ListInit(&mnt->activeVnodeList);  // 初始化活跃虚拟节点链表
    LOS_ListInit(&mnt->vnodeList);        // 初始化虚拟节点链表

    mnt->vnodeBeCovered = vnodeBeCovered;  // 设置被覆盖的虚拟节点
    vnodeBeCovered->newMount = mnt;        // 将挂载结构体指针保存到虚拟节点
#ifdef LOSCFG_DRIVERS_RANDOM               // 如果启用随机数驱动
    HiRandomHwInit();                      // 初始化硬件随机数生成器
    (VOID)HiRandomHwGetInteger(&mnt->hashseed);  // 获取硬件随机数作为哈希种子
    HiRandomHwDeinit();                    // 反初始化硬件随机数生成器
#else                                      // 否则(不启用随机数驱动)
    mnt->hashseed = (uint32_t)random();    // 使用软件随机数作为哈希种子
#endif
    return mnt;                            // 返回初始化后的挂载结构体指针
}

#ifdef LOSCFG_MNT_CONTAINER                          // 如果启用容器挂载功能
/**
 * @brief 获取容器挂载列表
 * @return 容器挂载列表头指针
 */
LIST_HEAD *GetMountList(void)
{
    return GetContainerMntList();  // 返回容器挂载列表
}

/**
 * @brief 获取挂载缓存列表
 * @return 挂载缓存列表头指针
 */
LIST_HEAD *GetMountCache(void)
{
    if (g_mountCache == NULL) {                       // 如果挂载缓存列表未初始化
        g_mountCache = zalloc(sizeof(LIST_HEAD));     // 分配挂载缓存列表内存
        if (g_mountCache == NULL) {                   // 检查内存分配是否失败
            PRINT_ERR("init cache mount list failed, no memory.");  // 输出初始化失败错误信息
            return NULL;                              // 返回NULL表示失败
        }
        LOS_ListInit(g_mountCache);                   // 初始化挂载缓存列表
    }
    return g_mountCache;                              // 返回挂载缓存列表头指针
}
#else                                                // 否则(不启用容器挂载功能)
/**
 * @brief 获取挂载列表
 * @return 挂载列表头指针
 */
LIST_HEAD* GetMountList(void)
{
    if (g_mountList == NULL) {                        // 如果挂载列表未初始化
        g_mountList = zalloc(sizeof(LIST_HEAD));      // 分配挂载列表内存
        if (g_mountList == NULL) {                    // 检查内存分配是否失败
            PRINT_ERR("init mount list failed, no memory.");  // 输出初始化失败错误信息
            return NULL;                              // 返回NULL表示失败
        }
        LOS_ListInit(g_mountList);                    // 初始化挂载列表
    }
    return g_mountList;                               // 返回挂载列表头指针
}
#endif
