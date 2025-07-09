/*
 * Copyright (c) 2013-2019 Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020-2023 Huawei Device Co., Ltd. All rights reserved.
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

#include "proc_file.h"
#include <stdio.h>
#include <linux/errno.h>
#include <linux/module.h>
#include "internal.h"
#include "user_copy.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))  // 最小值宏定义，返回a和b中的较小值
#define PROC_ROOTDIR_NAMELEN   5  // proc根目录名称长度，值为5（对应"/proc"）
#define PROC_INUSE             2  // proc文件/目录正在使用的标记值

DEFINE_SPINLOCK(procfsLock);  // proc文件系统的自旋锁，用于同步对proc数据结构的访问
bool procfsInit = false;  // proc文件系统初始化标志，false表示未初始化

// proc文件结构体实例，用于根目录的文件操作
static struct ProcFile g_procPf = {
    .fPos       = 0,  // 文件当前位置偏移量，初始化为0
};

// proc根目录项结构体实例，代表/proc目录
static struct ProcDirEntry g_procRootDirEntry = {
    .nameLen     = 5,  // 名称长度，"/proc"为5个字符
    .mode        = S_IFDIR | PROCFS_DEFAULT_MODE,  // 文件模式：目录类型 | 默认权限
    .count       = ATOMIC_INIT(1),  // 引用计数，原子初始化值为1
    .procFileOps = NULL,  // 指向proc文件操作结构体的指针，初始化为NULL
    .parent      = &g_procRootDirEntry,  // 父目录项指针，根目录的父目录是自身
    .name        = "/proc",  // 目录名称
    .subdir      = NULL,  // 子目录链表头指针，初始化为NULL
    .next        = NULL,  // 同级目录项链表指针，初始化为NULL
    .pf          = &g_procPf,  // 指向对应的ProcFile结构体
    .type        = VNODE_TYPE_DIR,  // 节点类型：目录
};

/**
 * @brief 比较名称长度和内容是否匹配proc目录项
 * @param len 待比较名称的长度
 * @param name 待比较的名称字符串
 * @param pn 指向ProcDirEntry结构体的指针
 * @return 匹配返回非0值，不匹配返回0
 */
int ProcMatch(unsigned int len, const char *name, struct ProcDirEntry *pn)
{
    if (len != pn->nameLen) {  // 长度不匹配则直接返回0
        return 0;
    }
    return !strncmp(name, pn->name, len);  // 比较名称内容，相同返回1，不同返回0
}

/**
 * @brief 在父目录下查找指定名称的proc目录项
 * @param parent 父目录项指针
 * @param name 要查找的名称
 * @return 找到返回目录项指针，未找到返回NULL
 */
static struct ProcDirEntry *ProcFindNode(struct ProcDirEntry *parent, const char *name)
{
    struct ProcDirEntry *pn = NULL;  // 用于遍历的目录项指针
    int length;  // 名称长度

    if ((parent == NULL) || (name == NULL)) {  // 父目录或名称为空，直接返回NULL
        return pn;
    }
    length = strlen(name);  // 获取名称长度

    // 遍历父目录的子目录项链表
    for (pn = parent->subdir; pn != NULL; pn = pn->next) {
        // 长度和名称都匹配则找到目标
        if ((length == pn->nameLen) && strcmp(pn->name, name) == 0) {
            break;
        }
    }

    return pn;  // 返回找到的目录项或NULL
}

/*
 * description: find the file's handle
 * path: the file of fullpath
 * return: the file of handle
 * add by ll
 */
/**
 * @brief 根据完整路径查找proc文件/目录项
 * @param path 完整路径字符串
 * @return 找到返回目录项指针，未找到返回NULL
 */
struct ProcDirEntry *ProcFindEntry(const char *path)
{
    struct ProcDirEntry *pn = NULL;  // 结果目录项指针
    int isfoundsub;  // 是否找到子目录的标志
    const char *next = NULL;  // 指向下一个路径分隔符的指针
    unsigned int len;  // 当前路径段长度
    int leveltotal = 0;  // 路径总层级数
    int levelcount = 0;  // 当前遍历层级计数
    const char *p = NULL;  // 临时指针，用于计算层级
    const char *name = path;  // 路径起始指针

    // 计算路径中的层级总数（根据'/'分隔符数量）
    while ((p = strchr(name, '/')) != NULL) {
        leveltotal++;
        name = p;
        name++;
    }
    if (leveltotal < 1) {  // 层级小于1，无效路径，返回NULL
        return pn;
    }

    spin_lock(&procfsLock);  // 获取procfs自旋锁，保护目录项操作

    pn = &g_procRootDirEntry;  // 从根目录开始查找

    // 遍历每一层路径
    while ((pn != NULL) && (levelcount < leveltotal)) {
        levelcount++;  // 当前层级计数加1
        isfoundsub = 0;  // 重置子目录找到标志
        while (pn != NULL) {
            next = strchr(path, '/');  // 查找下一个'/'分隔符
            if (next == NULL) {  // 没有更多分隔符，检查当前名称
                while (pn != NULL) {
                    if (strcmp(path, pn->name) == 0) {  // 名称匹配
                        spin_unlock(&procfsLock);  // 释放锁
                        return pn;  // 返回找到的目录项
                    }
                    pn = pn->next;  // 遍历下一个同级目录项
                }
                pn = NULL;  // 未找到，置空
                spin_unlock(&procfsLock);  // 释放锁
                return pn;  // 返回NULL
            }

            len = next - path;  // 计算当前路径段长度
            if (pn == &g_procRootDirEntry) {  // 如果是根目录
                if (levelcount == leveltotal) {  // 已到达目标层级
                    spin_unlock(&procfsLock);  // 释放锁
                    return pn;  // 返回根目录项
                }
                len = g_procRootDirEntry.nameLen;  // 使用根目录名称长度
            }
            if (ProcMatch(len, path, pn)) {  // 匹配当前路径段
                isfoundsub = 1;  // 设置找到标志
                path += len + 1;  // 移动路径指针到下一段
                break;
            }

            pn = pn->next;  // 遍历下一个同级目录项
        }

        if ((isfoundsub == 1) && (pn != NULL)) {  // 找到子目录
            pn = pn->subdir;  // 进入子目录继续查找
        } else {  // 未找到
            pn = NULL;  // 置空
            spin_unlock(&procfsLock);  // 释放锁
            return pn;  // 返回NULL
        }
    }
    spin_unlock(&procfsLock);  // 释放锁
    return NULL;  // 返回找到的目录项或NULL
}

/**
 * @brief 检查proc路径名称的有效性并拆分父目录和最后一段名称
 * @param name 完整路径名称
 * @param parent 输入输出参数，指向父目录项指针
 * @param lastName 输出参数，指向最后一段名称的指针
 * @return 成功返回0，失败返回错误码
 */
static int CheckProcName(const char *name, struct ProcDirEntry **parent, const char **lastName)
{
    struct ProcDirEntry *pn = *parent;  // 当前目录项指针
    const char *segment = name;  // 当前路径段指针
    const char *restName = NULL;  // 剩余路径指针
    int length;  // 路径段长度

    if (pn == NULL) {  // 如果父目录为空，从根目录开始
        pn = &g_procRootDirEntry;
    }

    spin_lock(&procfsLock);  // 获取procfs自旋锁

    // 遍历路径中的每一段
    restName = strchr(segment, '/');
    for (; restName != NULL; restName = strchr(segment, '/')) {
        length = restName - segment;  // 计算当前段长度
        // 在当前目录的子目录中查找匹配的段
        for (pn = pn->subdir; pn != NULL; pn = pn->next) {
            if (ProcMatch(length, segment, pn)) {  // 匹配成功
                break;
            }
        }
        if (pn == NULL) {  // 未找到匹配的段
            PRINT_ERR(" Error!No such name '%s'\n", name);  // 打印错误信息
            spin_unlock(&procfsLock);  // 释放锁
            return -ENOENT;  // 返回不存在错误
        }
        segment = restName;  // 移动到下一段
        segment++;
    }
    *lastName = segment;  // 设置最后一段名称
    *parent = pn;  // 设置父目录
    spin_unlock(&procfsLock);  // 释放锁

    return 0;  // 成功返回0
}

/**
 * @brief 分配并初始化proc目录项
 * @param parent 输入输出参数，指向父目录项指针
 * @param name 完整路径名称
 * @param mode 文件模式（权限和类型）
 * @return 成功返回新目录项指针，失败返回NULL
 */
static struct ProcDirEntry *ProcAllocNode(struct ProcDirEntry **parent, const char *name, mode_t mode)
{
    struct ProcDirEntry *pn = NULL;  // 新目录项指针
    const char *lastName = NULL;  // 最后一段名称
    int ret;  // 函数返回值

    // 参数检查：名称为空或长度为0，或proc未初始化
    if ((name == NULL) || (strlen(name) == 0) || (procfsInit == false)) {
        return pn;
    }

    // 检查路径名称并获取父目录和最后一段名称
    if (CheckProcName(name, parent, &lastName) != 0) {
        return pn;
    }

    if (strlen(lastName) > NAME_MAX) {  // 最后一段名称长度超过最大值
        return pn;
    }

    // 父目录不是目录类型，或最后一段名称包含'/'
    if ((S_ISDIR((*parent)->mode) == 0) || (strchr(lastName, '/'))) {
        return pn;
    }

    // 分配ProcDirEntry结构体内存
    pn = (struct ProcDirEntry *)malloc(sizeof(struct ProcDirEntry));
    if (pn == NULL) {  // 内存分配失败
        return NULL;
    }

    // 如果未指定权限，默认设置为只读
    if ((mode & S_IALLUGO) == 0) {
        mode |= S_IRUSR | S_IRGRP | S_IROTH;  // 所有者、组、其他用户均有读权限
    }

    // 初始化目录项结构体
    (void)memset_s(pn, sizeof(struct ProcDirEntry), 0, sizeof(struct ProcDirEntry));
    pn->nameLen = strlen(lastName);  // 设置名称长度
    pn->mode = mode;  // 设置文件模式
    // 复制名称字符串
    ret = memcpy_s(pn->name, sizeof(pn->name), lastName, strlen(lastName) + 1);
    if (ret != EOK) {  // 复制失败
        free(pn);  // 释放已分配内存
        return NULL;
    }

    // 分配ProcFile结构体内存
    pn->pf = (struct ProcFile *)malloc(sizeof(struct ProcFile));
    if (pn->pf == NULL) {  // 内存分配失败
        free(pn);  // 释放目录项内存
        return NULL;
    }
    // 初始化ProcFile结构体
    (void)memset_s(pn->pf, sizeof(struct ProcFile), 0, sizeof(struct ProcFile));
    pn->pf->pPDE = pn;  // 设置指向所属目录项的指针
    // 复制名称到ProcFile
    ret = memcpy_s(pn->pf->name, sizeof(pn->pf->name), pn->name, pn->nameLen + 1);
    if (ret != EOK) {  // 复制失败
        free(pn->pf);  // 释放ProcFile内存
        free(pn);  // 释放目录项内存
        return NULL;
    }

    atomic_set(&pn->count, 1);  // 设置引用计数为1
    spin_lock_init(&pn->pdeUnloadLock);  // 初始化卸载锁
    return pn;  // 返回新分配的目录项
}

/**
 * @brief 将proc目录项添加到父目录的子目录链表
 * @param parent 父目录项指针
 * @param pn 要添加的目录项指针
 * @return 成功返回0，失败返回错误码
 */
static int ProcAddNode(struct ProcDirEntry *parent, struct ProcDirEntry *pn)
{
    struct ProcDirEntry *temp = NULL;  // 临时目录项指针，用于查找

    if (parent == NULL) {  // 父目录为空
        PRINT_ERR("%s(): parent is NULL", __FUNCTION__);  // 打印错误
        return -EINVAL;  // 返回无效参数错误
    }

    if (pn->parent != NULL) {  // 目录项已有父目录
        PRINT_ERR("%s(): node already has a parent", __FUNCTION__);  // 打印错误
        return -EINVAL;  // 返回无效参数错误
    }

    if (S_ISDIR(parent->mode) == 0) {  // 父目录不是目录类型
        PRINT_ERR("%s(): parent is not a directory", __FUNCTION__);  // 打印错误
        return -EINVAL;  // 返回无效参数错误
    }

    spin_lock(&procfsLock);  // 获取procfs自旋锁

    // 检查父目录下是否已存在同名目录项
    temp = ProcFindNode(parent, pn->name);
    if (temp != NULL) {  // 已存在
        PRINT_ERR("Error!ProcDirEntry '%s/%s' already registered\n", parent->name, pn->name);  // 打印错误
        spin_unlock(&procfsLock);  // 释放锁
        return -EEXIST;  // 返回已存在错误
    }

    pn->parent = parent;  // 设置父目录
    pn->next = parent->subdir;  // 将新目录项插入子目录链表头部
    parent->subdir = pn;

    spin_unlock(&procfsLock);  // 释放锁

    return 0;  // 成功返回0
}

/**
 * @brief 从父目录的子目录链表中分离指定的proc目录项
 * @param pn 要分离的目录项指针
 */
void ProcDetachNode(struct ProcDirEntry *pn)
{
    struct ProcDirEntry *parent = pn->parent;  // 父目录项指针
    struct ProcDirEntry **iter = NULL;  // 用于遍历子目录链表的二级指针

    if (parent == NULL) {  // 父目录为空
        PRINT_ERR("%s(): node has no parent", __FUNCTION__);  // 打印错误
        return;
    }

    iter = &parent->subdir;  // 从子目录链表头开始
    // 遍历子目录链表查找目标目录项
    while (*iter != NULL) {
        if (*iter == pn) {  // 找到目标
            *iter = pn->next;  // 从链表中移除
            break;
        }
        iter = &(*iter)->next;  // 移动到下一个节点
    }
    pn->parent = NULL;  // 清除父目录指针
}

/**
 * @brief 创建proc目录
 * @param parent 父目录项指针
 * @param name 目录名称
 * @param procFileOps 指向proc文件操作结构体的指针
 * @param mode 目录权限
 * @return 成功返回新目录项指针，失败返回NULL
 */
static struct ProcDirEntry *ProcCreateDir(struct ProcDirEntry *parent, const char *name,
                                          const struct ProcFileOperations *procFileOps, mode_t mode)
{
    struct ProcDirEntry *pn = NULL;  // 新目录项指针
    int ret;  // 函数返回值

    // 分配目录类型的目录项
    pn = ProcAllocNode(&parent, name, S_IFDIR | mode);
    if (pn == NULL) {  // 分配失败
        return pn;
    }
    pn->procFileOps = procFileOps;  // 设置文件操作指针
    pn->type = VNODE_TYPE_DIR;  // 设置节点类型为目录
    // 将新目录项添加到父目录
    ret = ProcAddNode(parent, pn);
    if (ret != 0) {  // 添加失败
        free(pn->pf);  // 释放ProcFile内存
        free(pn);  // 释放目录项内存
        return NULL;
    }

    return pn;  // 返回新创建的目录项
}

/**
 * @brief 创建proc文件
 * @param parent 父目录项指针
 * @param name 文件名称
 * @param procFileOps 指向proc文件操作结构体的指针
 * @param mode 文件权限和类型
 * @return 成功返回新目录项指针，失败返回NULL
 */
static struct ProcDirEntry *ProcCreateFile(struct ProcDirEntry *parent, const char *name,
                                           const struct ProcFileOperations *procFileOps, mode_t mode)
{
    struct ProcDirEntry *pn = NULL;  // 新目录项指针
    int ret;  // 函数返回值

    // 分配普通文件类型的目录项
    pn = ProcAllocNode(&parent, name, S_IFREG | mode);
    if (pn == NULL) {  // 分配失败
        return pn;
    }

    pn->procFileOps = procFileOps;  // 设置文件操作指针
    pn->type = VNODE_TYPE_REG;  // 设置节点类型为普通文件
#ifdef LOSCFG_PROC_PROCESS_DIR
    if (S_ISLNK(mode)) {  // 如果模式是符号链接
        pn->type = VNODE_TYPE_VIR_LNK;  // 设置节点类型为虚拟链接
    }
#endif
    // 将新文件目录项添加到父目录
    ret = ProcAddNode(parent, pn);
    if (ret != 0) {  // 添加失败
        free(pn->pf);  // 释放ProcFile内存
        free(pn);  // 释放目录项内存
        return NULL;
    }

    return pn;  // 返回新创建的文件目录项
}

/**
 * @brief 创建proc目录项（根据模式创建目录或文件）
 * @param name 名称
 * @param mode 文件模式（决定类型和权限）
 * @param parent 父目录项指针
 * @return 成功返回新目录项指针，失败返回NULL
 */
struct ProcDirEntry *CreateProcEntry(const char *name, mode_t mode, struct ProcDirEntry *parent)
{
    struct ProcDirEntry *pde = NULL;  // 新目录项指针

    if (S_ISDIR(mode)) {  // 如果是目录模式
        pde = ProcCreateDir(parent, name, NULL, mode);  // 创建目录
    } else {  // 否则创建文件
        pde = ProcCreateFile(parent, name, NULL, mode);
    }
    return pde;  // 返回新创建的目录项
}

/**
 * @brief 清除与指定proc目录项关联的Vnode节点
 * @param entry 指向ProcDirEntry结构体的指针
 */
void ProcEntryClearVnode(struct ProcDirEntry *entry)
{
    struct Vnode *item = NULL;  // 当前Vnode节点
    struct Vnode *nextItem = NULL;  // 下一个Vnode节点

    VnodeHold();  // 持有Vnode锁
    // 遍历活动Vnode链表
    LOS_DL_LIST_FOR_EACH_ENTRY_SAFE(item, nextItem, GetVnodeActiveList(), struct Vnode, actFreeEntry) {
        // 检查Vnode的数据是否指向当前proc目录项
        if ((struct ProcDirEntry *)item->data != entry) {
            continue;
        }

        if (VnodeFree(item) != LOS_OK) {  // 释放Vnode失败
            PRINT_ERR("ProcEntryClearVnode free failed, entry: %s\n", entry->name);  // 打印错误
        }
    }
    VnodeDrop();  // 释放Vnode锁
    return;
}

/**
 * @brief 释放proc目录项及其关联资源
 * @param entry 指向ProcDirEntry结构体的指针
 */
static void FreeProcEntry(struct ProcDirEntry *entry)
{
    if (entry == NULL) {  // 目录项为空，直接返回
        return;
    }

    if (entry->pf != NULL) {  // 如果ProcFile存在
        free(entry->pf);  // 释放ProcFile内存
        entry->pf = NULL;
    }
    // 如果数据类型是需要释放的，且数据不为空
    if ((entry->dataType == PROC_DATA_FREE) && (entry->data != NULL)) {
        free(entry->data);  // 释放数据内存
    }
    entry->data = NULL;
    free(entry);  // 释放目录项内存
}

/**
 * @brief 引用计数减1，当计数为0时释放proc目录项
 * @param pn 指向ProcDirEntry结构体的指针
 */
void ProcFreeEntry(struct ProcDirEntry *pn)
{
    // 原子减1并检查是否为0
    if (atomic_dec_and_test(&pn->count)) {
        FreeProcEntry(pn);  // 释放目录项
    }
}

/**
 * @brief 递归删除proc目录项及其所有子项
 * @param pn 指向ProcDirEntry结构体的指针
 */
void RemoveProcEntryTravalsal(struct ProcDirEntry *pn)
{
    if (pn == NULL) {  // 目录项为空，直接返回
        return;
    }
    RemoveProcEntryTravalsal(pn->next);  // 递归删除下一个同级目录项
    RemoveProcEntryTravalsal(pn->subdir);  // 递归删除子目录项

    ProcEntryClearVnode(pn);  // 清除关联的Vnode

    ProcFreeEntry(pn);  // 释放当前目录项
}

/**
 * @brief 从父目录中移除指定名称的proc目录项
 * @param name 要移除的目录项名称
 * @param parent 父目录项指针
 */
void RemoveProcEntry(const char *name, struct ProcDirEntry *parent)
{
    struct ProcDirEntry *pn = NULL;  // 要移除的目录项指针
    const char *lastName = name;  // 最后一段名称

    // 参数检查：名称为空或长度为0，或proc未初始化
    if ((name == NULL) || (strlen(name) == 0) || (procfsInit == false)) {
        return;
    }

    // 检查路径名称并获取父目录和最后一段名称
    if (CheckProcName(name, &parent, &lastName) != 0) {
        return;
    }

    spin_lock(&procfsLock);  // 获取procfs自旋锁

    pn = ProcFindNode(parent, lastName);  // 在父目录中查找目标
    if (pn == NULL) {  // 未找到
        PRINT_ERR("Error:name '%s' not found!\n", name);  // 打印错误
        spin_unlock(&procfsLock);  // 释放锁
        return;
    }
    ProcDetachNode(pn);  // 从父目录中分离目标目录项

    spin_unlock(&procfsLock);  // 释放锁

    RemoveProcEntryTravalsal(pn->subdir);  // 递归删除子目录项

    ProcEntryClearVnode(pn);  // 清除关联的Vnode

    ProcFreeEntry(pn);  // 释放目标目录项
}

/**
 * @brief 创建具有指定权限的proc目录
 * @param name 目录名称
 * @param mode 目录权限
 * @param parent 父目录项指针
 * @return 成功返回新目录项指针，失败返回NULL
 */
struct ProcDirEntry *ProcMkdirMode(const char *name, mode_t mode, struct ProcDirEntry *parent)
{
    return ProcCreateDir(parent, name, NULL, mode);  // 调用创建目录函数
}

/**
 * @brief 创建具有默认权限的proc目录
 * @param name 目录名称
 * @param parent 父目录项指针
 * @return 成功返回新目录项指针，失败返回NULL
 */
struct ProcDirEntry *ProcMkdir(const char *name, struct ProcDirEntry *parent)
{
    return ProcCreateDir(parent, name, NULL, 0);  // 调用创建目录函数，权限为0
}

/**
 * @brief 创建带有数据参数的proc文件/目录
 * @param name 名称
 * @param mode 模式（类型和权限）
 * @param parent 父目录项指针
 * @param procFileOps 文件操作结构体指针
 * @param param 数据参数结构体指针
 * @return 成功返回新目录项指针，失败返回NULL
 */
struct ProcDirEntry *ProcCreateData(const char *name, mode_t mode, struct ProcDirEntry *parent,
                                    const struct ProcFileOperations *procFileOps, struct ProcDataParm *param)
{
    // 创建基本目录项
    struct ProcDirEntry *pde = CreateProcEntry(name, mode, parent);
    if (pde != NULL) {
        if (procFileOps != NULL) {
            pde->procFileOps = procFileOps;  // 设置文件操作
        }
        if (param != NULL) {
            pde->data = param->data;  // 设置数据指针
            pde->dataType = param->dataType;  // 设置数据类型
        }
    }
    return pde;  // 返回新创建的目录项
}

/**
 * @brief 创建proc文件/目录
 * @param name 名称
 * @param mode 模式（类型和权限）
 * @param parent 父目录项指针
 * @param procFileOps 文件操作结构体指针
 * @return 成功返回新目录项指针，失败返回NULL
 */
struct ProcDirEntry *ProcCreate(const char *name, mode_t mode, struct ProcDirEntry *parent,
                                const struct ProcFileOperations *procFileOps)
{
    // 调用带数据参数的创建函数，数据参数为NULL
    return ProcCreateData(name, mode, parent, procFileOps, NULL);
}

/**
 * @brief 获取proc文件的状态信息
 * @param file 文件路径
 * @param buf 存储状态信息的缓冲区
 * @return 成功返回0，失败返回错误码
 */
int ProcStat(const char *file, struct ProcStat *buf)
{
    struct ProcDirEntry *pn = NULL;  // 文件目录项指针
    int len = sizeof(buf->name);  // 名称缓冲区长度
    int ret;  // 函数返回值

    pn = ProcFindEntry(file);  // 查找文件目录项
    if (pn == NULL) {  // 未找到
        return ENOENT;  // 返回不存在错误
    }
    // 复制名称到缓冲区
    ret = strncpy_s(buf->name, len, pn->name, len - 1);
    if (ret != EOK) {  // 复制失败
        return ENAMETOOLONG;  // 返回名称过长错误
    }
    buf->name[len - 1] = '\0';  // 确保字符串以null结尾
    buf->stMode = pn->mode;  // 设置文件模式
    buf->pPDE = pn;  // 设置指向目录项的指针

    return 0;  // 成功返回0
}

/**
 * @brief 获取目录中的下一个条目名称
 * @param pn 目录项指针
 * @param buf 存储名称的缓冲区
 * @param len 缓冲区长度
 * @return 成功返回0，失败返回错误码
 */
static int GetNextDir(struct ProcDirEntry *pn, void *buf, size_t len)
{
    char *buff = (char *)buf;  // 缓冲区指针

    if (pn->pdirCurrent == NULL) {  // 当前条目为空（已遍历完）
        *buff = '\0';  // 设置空字符串
        return -ENOENT;  // 返回不存在错误
    }
    int namelen = pn->pdirCurrent->nameLen;  // 获取当前条目的名称长度
    // 复制名称到缓冲区
    int ret = memcpy_s(buff, len, pn->pdirCurrent->name, namelen);
    if (ret != EOK) {  // 复制失败
        return -ENAMETOOLONG;  // 返回名称过长错误
    }

    pn->pdirCurrent = pn->pdirCurrent->next;  // 移动到下一个条目
    pn->pf->fPos++;  // 更新文件位置
    return ENOERR;  // 成功返回0
}

/**
 * @brief 打开proc文件，初始化缓冲区
 * @param procFile 指向ProcFile结构体的指针
 * @return 成功返回OK，失败返回PROC_ERROR
 */
int ProcOpen(struct ProcFile *procFile)
{
    if (procFile == NULL) {  // 文件结构体为空
        return PROC_ERROR;
    }
    if (procFile->sbuf != NULL) {  // 缓冲区已初始化
        return OK;
    }

    struct SeqBuf *buf = LosBufCreat();  // 创建序列缓冲区
    if (buf == NULL) {  // 创建失败
        return PROC_ERROR;
    }
    procFile->sbuf = buf;  // 设置缓冲区指针
    return OK;  // 成功返回OK
}

/**
 * @brief 读取proc文件内容
 * @param pde 目录项指针
 * @param buf 存储读取数据的缓冲区
 * @param len 要读取的长度
 * @return 成功返回读取的字节数，失败返回PROC_ERROR
 */
static int ProcRead(struct ProcDirEntry *pde, char *buf, size_t len)
{
    if (pde == NULL || pde->pf == NULL) {  // 目录项或文件结构体为空
        return PROC_ERROR;
    }
    struct ProcFile *procFile = pde->pf;  // 文件结构体指针
    struct SeqBuf *sb = procFile->sbuf;  // 序列缓冲区指针

    if (sb->buf == NULL) {  // 缓冲区未填充数据
        // 调用文件操作的read函数填充缓冲区（只读取一次）
        if (pde->procFileOps->read(sb, pde->data) != 0) {
            return PROC_ERROR;
        }
    }

    size_t realLen;  // 实际读取长度
    loff_t pos = procFile->fPos;  // 当前文件位置

    if ((pos >= sb->count) || (len == 0)) {  // 已到达文件末尾或读取长度为0
        /* there's no data or at the file tail. */
        realLen = 0;
    } else {  // 计算实际可读取长度
        realLen = MIN((sb->count - pos), MIN(len, INT_MAX));
        // 从内核缓冲区复制数据到用户缓冲区
        if (LOS_CopyFromKernel(buf, len, sb->buf + pos, realLen) != 0) {
            return PROC_ERROR;
        }

        procFile->fPos = pos + realLen;  // 更新文件位置
    }

    return (ssize_t)realLen;  // 返回实际读取长度
}

/**
 * @brief 打开proc文件
 * @param fileName 文件路径
 * @param flags 打开标志
 * @return 成功返回目录项指针，失败返回NULL
 */
struct ProcDirEntry *OpenProcFile(const char *fileName, int flags, ...)
{
    struct ProcDirEntry *pn = ProcFindEntry(fileName);  // 查找文件目录项
    if (pn == NULL) {  // 未找到
        return NULL;
    }

    // 如果是普通文件且引用计数不为1（已被打开）
    if (S_ISREG(pn->mode) && (pn->count != 1)) {
        return NULL;
    }

    pn->flags = (unsigned int)(pn->flags) | (unsigned int)flags;  // 设置标志
    atomic_set(&pn->count, PROC_INUSE);  // 设置引用计数为正在使用
    if (ProcOpen(pn->pf) != OK) {  // 打开文件失败
        return NULL;
    }
    // 如果是普通文件且有open操作函数
    if (S_ISREG(pn->mode) && (pn->procFileOps != NULL) && (pn->procFileOps->open != NULL)) {
        (void)pn->procFileOps->open((struct Vnode *)pn, pn->pf);  // 调用open操作
    }
    if (S_ISDIR(pn->mode)) {  // 如果是目录
        pn->pdirCurrent = pn->subdir;  // 初始化目录遍历指针
        pn->pf->fPos = 0;  // 重置文件位置
    }

    return pn;  // 返回目录项指针
}

/**
 * @brief 读取proc文件或目录内容
 * @param pde 目录项指针
 * @param buf 存储读取数据的缓冲区
 * @param len 要读取的长度
 * @return 成功返回读取的字节数或0，失败返回错误码
 */
int ReadProcFile(struct ProcDirEntry *pde, void *buf, size_t len)
{
    int result = -EPERM;  // 默认返回权限错误

    if (pde == NULL) {  // 目录项为空
        return result;
    }
    if (S_ISREG(pde->mode)) {  // 如果是普通文件
        // 检查是否有read操作函数
        if ((pde->procFileOps != NULL) && (pde->procFileOps->read != NULL)) {
            result = ProcRead(pde, (char *)buf, len);  // 调用读取函数
        }
    } else if (S_ISDIR(pde->mode)) {  // 如果是目录
        result = GetNextDir(pde, buf, len);  // 获取下一个目录条目
    }
    return result;  // 返回结果
}

/**
 * @brief 写入proc文件内容
 * @param pde 目录项指针
 * @param buf 包含写入数据的缓冲区
 * @param len 要写入的长度
 * @return 成功返回写入的字节数，失败返回错误码
 */
int WriteProcFile(struct ProcDirEntry *pde, const void *buf, size_t len)
{
    int result = -EPERM;  // 默认返回权限错误

    if (pde == NULL) {  // 目录项为空
        return result;
    }

    if (S_ISDIR(pde->mode)) {  // 不能写入目录
        return -EISDIR;  // 返回是目录错误
    }

    spin_lock(&procfsLock);  // 获取procfs自旋锁
    // 检查是否有write操作函数
    if ((pde->procFileOps != NULL) && (pde->procFileOps->write != NULL)) {
        // 调用write操作函数
        result = pde->procFileOps->write(pde->pf, (const char *)buf, len, &(pde->pf->fPos));
    }
    spin_unlock(&procfsLock);  // 释放锁
    return result;  // 返回结果
}

/**
 * @brief 调整proc文件的读写位置
 * @param pde 目录项指针
 * @param offset 偏移量
 * @param whence 基准位置（SEEK_CUR或SEEK_SET）
 * @return 成功返回新位置，失败返回PROC_ERROR
 */
loff_t LseekProcFile(struct ProcDirEntry *pde, loff_t offset, int whence)
{
    if (pde == NULL || pde->pf == NULL) {  // 目录项或文件结构体为空
        return PROC_ERROR;
    }

    struct ProcFile *procFile = pde->pf;  // 文件结构体指针

    loff_t result = -EINVAL;  // 默认返回无效参数错误

    switch (whence) {
        case SEEK_CUR:  // 从当前位置开始
            result = procFile->fPos + offset;
            break;

        case SEEK_SET:  // 从文件开头开始
            result = offset;
            break;

        default:  // 不支持其他基准
            break;
    }

    if (result >= 0) {
        procFile->fPos = result;
    }

    return result;  // 返回新位置
}

/**
 * @brief 调整proc目录的遍历位置
 * @param pde 目录项指针
 * @param pos 位置指针
 * @param whence 基准位置
 * @return 成功返回0，失败返回EINVAL
 */
int LseekDirProcFile(struct ProcDirEntry *pde, off_t *pos, int whence)
{
    /* Only allow SEEK_SET to zero */
    // 只支持SEEK_SET到位置0
    if ((whence != SEEK_SET) || (*pos != 0)) {
        return EINVAL;  // 返回无效参数错误
    }
    pde->pdirCurrent = pde->subdir;  // 重置目录遍历指针到开头
    pde->pf->fPos = 0;  // 重置文件位置
    return ENOERR;  // 成功返回0
}

/**
 * @brief 关闭proc文件
 * @param pde 目录项指针
 * @return 成功返回0，失败返回错误码
 */
int CloseProcFile(struct ProcDirEntry *pde)
{
    int result = 0;  // 默认成功

    if (pde == NULL) {  // 目录项为空
        return -EPERM;  // 返回权限错误
    }
    pde->pf->fPos = 0;  // 重置文件位置
    atomic_set(&pde->count, 1);  // 重置引用计数
    if (S_ISDIR(pde->mode)) {  // 如果是目录
        pde->pdirCurrent = pde->subdir;  // 重置目录遍历指针
    }

    // 如果有release操作函数
    if ((pde->procFileOps != NULL) && (pde->procFileOps->release != NULL)) {
        result = pde->procFileOps->release((struct Vnode *)pde, pde->pf);  // 调用release操作
    }
    LosBufRelease(pde->pf->sbuf);  // 释放序列缓冲区
    pde->pf->sbuf = NULL;  // 清除缓冲区指针

    if (pde->parent == NULL) {  // 如果没有父目录（根目录）
        FreeProcEntry(pde);  // 释放目录项
    }
    return result;  // 返回结果
}

/**
 * @brief 获取proc根目录项
 * @return 指向根目录项的指针
 */
struct ProcDirEntry *GetProcRootEntry(void)
{
    return &g_procRootDirEntry;  // 返回根目录项实例
}