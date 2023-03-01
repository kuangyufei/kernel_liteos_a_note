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

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define PROC_ROOTDIR_NAMELEN   5
#define PROC_INUSE             2
//定义自旋锁,说明 procfs不支持多线程访问
DEFINE_SPINLOCK(procfsLock);//定义profs 自旋锁
bool procfsInit = false;	//是否初始化了profs

static struct ProcFile g_procPf = {
    .fPos       = 0,
};

static struct ProcDirEntry g_procRootDirEntry = { //proc 根部 即 /proc
    .nameLen     = 5,
    .mode        = S_IFDIR | PROCFS_DEFAULT_MODE,
    .count       = ATOMIC_INIT(1),
    .procFileOps = NULL,//对根节点无操作需求
    .parent      = &g_procRootDirEntry,//自己当爹又当儿
    .name        = "/proc", 
    .subdir      = NULL,	//暂无,后续必有子
    .next        = NULL,	//暂无.后续必有续
    .pf          = &g_procPf,		//全局PRO文件	
    .type        = VNODE_TYPE_DIR,	//是个目录
};

int ProcMatch(unsigned int len, const char *name, struct ProcDirEntry *pn)
{
    if (len != pn->nameLen) {
        return 0;
    }
    return !strncmp(name, pn->name, len);
}

static struct ProcDirEntry *ProcFindNode(struct ProcDirEntry *parent, const char *name)
{
    struct ProcDirEntry *pn = NULL;
    int length;

    if ((parent == NULL) || (name == NULL)) {
        return pn;
    }
    length = strlen(name);

    for (pn = parent->subdir; pn != NULL; pn = pn->next) {
        if ((length == pn->nameLen) && strcmp(pn->name, name) == 0) {
            break;
        }
    }

    return pn;
}

/*
 * descrition: find the file's handle
 * path: the file of fullpath
 * return: the file of handle
 * add by ll
 */
struct ProcDirEntry *ProcFindEntry(const char *path)//查看内容项
{
    struct ProcDirEntry *pn = NULL;
    int isfoundsub;
    const char *next = NULL;
    unsigned int len;
    int leveltotal = 0;
    int levelcount = 0;
    const char *p = NULL;
    const char *name = path;

    while ((p = strchr(name, '/')) != NULL) {
        leveltotal++;
        name = p;
        name++;
    }
    if (leveltotal < 1) {
        return pn;
    }

    spin_lock(&procfsLock);

    pn = &g_procRootDirEntry;

    while ((pn != NULL) && (levelcount < leveltotal)) {
        levelcount++;
        isfoundsub = 0;
        while (pn != NULL) {
            next = strchr(path, '/');
            if (next == NULL) {
                while (pn != NULL) {
                    if (strcmp(path, pn->name) == 0) {
                        spin_unlock(&procfsLock);
                        return pn;
                    }
                    pn = pn->next;
                }
                pn = NULL;
                spin_unlock(&procfsLock);
                return pn;
            }

            len = next - path;
            if (pn == &g_procRootDirEntry) {
                if (levelcount == leveltotal) {
                    spin_unlock(&procfsLock);
                    return pn;
                }
                len = g_procRootDirEntry.nameLen;
            }
            if (ProcMatch(len, path, pn)) {
                isfoundsub = 1;
                path += len + 1;
                break;
            }

            pn = pn->next;
        }

        if ((isfoundsub == 1) && (pn != NULL)) {
            pn = pn->subdir;
        } else {
            pn = NULL;
            spin_unlock(&procfsLock);
            return pn;
        }
    }
    spin_unlock(&procfsLock);
    return NULL;
}
///检查 proc名称有效性
static int CheckProcName(const char *name, struct ProcDirEntry **parent, const char **lastName)
{
    struct ProcDirEntry *pn = *parent;
    const char *segment = name;
    const char *restName = NULL;
    int length;

    if (pn == NULL) {
        pn = &g_procRootDirEntry;
    }

    spin_lock(&procfsLock);

    restName = strchr(segment, '/');
    for (; restName != NULL; restName = strchr(segment, '/')) {
        length = restName - segment;
        for (pn = pn->subdir; pn != NULL; pn = pn->next) {
            if (ProcMatch(length, segment, pn)) {
                break;
            }
        }
        if (pn == NULL) {
            PRINT_ERR(" Error!No such name '%s'\n", name);
            spin_unlock(&procfsLock);
            return -ENOENT;
        }
        segment = restName;
        segment++;
    }
    *lastName = segment;
    *parent = pn;
    spin_unlock(&procfsLock);

    return 0;
}
///分配 proc 节点
static struct ProcDirEntry *ProcAllocNode(struct ProcDirEntry **parent, const char *name, mode_t mode)
{
    struct ProcDirEntry *pn = NULL;
    const char *lastName = NULL;
    int ret;

    if ((name == NULL) || (strlen(name) == 0) || (procfsInit == false)) {
        return pn;
    }

    if (CheckProcName(name, parent, &lastName) != 0) {
        return pn;
    }

    if (strlen(lastName) > NAME_MAX) {
        return pn;
    }

    if ((S_ISDIR((*parent)->mode) == 0) || (strchr(lastName, '/'))) {
        return pn;
    }
	//开始各种造初始数据
    pn = (struct ProcDirEntry *)malloc(sizeof(struct ProcDirEntry));//从内核分配内存
    if (pn == NULL) {
        return NULL;
    }

    if ((mode & S_IALLUGO) == 0) {
        mode |= S_IRUSR | S_IRGRP | S_IROTH;
    }

    (void)memset_s(pn, sizeof(struct ProcDirEntry), 0, sizeof(struct ProcDirEntry));//初始化内存
    pn->nameLen = strlen(lastName);
    pn->mode = mode;
    ret = memcpy_s(pn->name, sizeof(pn->name), lastName, strlen(lastName) + 1);
    if (ret != EOK) {
        free(pn);
        return NULL;
    }

    pn->pf = (struct ProcFile *)malloc(sizeof(struct ProcFile));
    if (pn->pf == NULL) {
        free(pn);
        return NULL;
    }
    (void)memset_s(pn->pf, sizeof(struct ProcFile), 0, sizeof(struct ProcFile));
    pn->pf->pPDE = pn;
    ret = memcpy_s(pn->pf->name, sizeof(pn->pf->name), pn->name, pn->nameLen + 1);
    if (ret != EOK) {
        free(pn->pf);
        free(pn);
        return NULL;
    }

    atomic_set(&pn->count, 1);//默认有一个引用
    spin_lock_init(&pn->pdeUnloadLock);
    return pn;
}

static int ProcAddNode(struct ProcDirEntry *parent, struct ProcDirEntry *pn)
{
    struct ProcDirEntry *temp = NULL;

    if (parent == NULL) {
        PRINT_ERR("%s(): parent is NULL", __FUNCTION__);
        return -EINVAL;
    }

    if (pn->parent != NULL) {
        PRINT_ERR("%s(): node already has a parent", __FUNCTION__);
        return -EINVAL;
    }

    if (S_ISDIR(parent->mode) == 0) {
        PRINT_ERR("%s(): parent is not a directory", __FUNCTION__);
        return -EINVAL;
    }

    spin_lock(&procfsLock);

    temp = ProcFindNode(parent, pn->name);
    if (temp != NULL) {
        PRINT_ERR("Error!ProcDirEntry '%s/%s' already registered\n", parent->name, pn->name);
        spin_unlock(&procfsLock);
        return -EEXIST;
    }

    pn->parent = parent;
    pn->next = parent->subdir;
    parent->subdir = pn;

    spin_unlock(&procfsLock);

    return 0;
}

void ProcDetachNode(struct ProcDirEntry *pn)
{
    struct ProcDirEntry *parent = pn->parent;
    struct ProcDirEntry **iter = NULL;

    if (parent == NULL) {
        PRINT_ERR("%s(): node has no parent", __FUNCTION__);
        return;
    }

    iter = &parent->subdir;
    while (*iter != NULL) {
        if (*iter == pn) {
            *iter = pn->next;
            break;
        }
        iter = &(*iter)->next;
    }
    pn->parent = NULL;
}
/// /proc文件系统创建目录的方式
static struct ProcDirEntry *ProcCreateDir(struct ProcDirEntry *parent, const char *name,
                                          const struct ProcFileOperations *procFileOps, mode_t mode)
{
    struct ProcDirEntry *pn = NULL;
    int ret;

    pn = ProcAllocNode(&parent, name, S_IFDIR | mode);//1.先创造一个数据项 ProcDirEntry
    if (pn == NULL) {
        return pn;
    }
    pn->procFileOps = procFileOps;//绑定数据项的文件系统
    pn->type = VNODE_TYPE_DIR;
    ret = ProcAddNode(parent, pn);
    if (ret != 0) {
        free(pn->pf);
        free(pn);
        return NULL;
    }

    return pn;
}
///创建文件项
static struct ProcDirEntry *ProcCreateFile(struct ProcDirEntry *parent, const char *name,
                                           const struct ProcFileOperations *procFileOps, mode_t mode)
{
    struct ProcDirEntry *pn = NULL;
    int ret;

    pn = ProcAllocNode(&parent, name, S_IFREG | mode);//分配一个节点,S_IFREG代表是文件类型
    if (pn == NULL) {
        return pn;
    }

    pn->procFileOps = procFileOps;//驱动程序
    pn->type = VNODE_TYPE_REG;	//文件类型
#ifdef LOSCFG_PROC_PROCESS_DIR
    if (S_ISLNK(mode)) {
        pn->type = VNODE_TYPE_VIR_LNK;
    }
#endif
    ret = ProcAddNode(parent, pn);
    if (ret != 0) {
        free(pn->pf);
        free(pn);
        return NULL;
    }

    return pn;
}
///创建 pro (目录/文件)项
struct ProcDirEntry *CreateProcEntry(const char *name, mode_t mode, struct ProcDirEntry *parent)
{
    struct ProcDirEntry *pde = NULL;

    if (S_ISDIR(mode)) {//目录模式 0
        pde = ProcCreateDir(parent, name, NULL, mode);//无驱动程序
    } else {
        pde = ProcCreateFile(parent, name, NULL, mode);//无驱动程序
    }
    return pde;
}

void ProcEntryClearVnode(struct ProcDirEntry *entry)
{
    struct Vnode *item = NULL;
    struct Vnode *nextItem = NULL;

    VnodeHold();
    LOS_DL_LIST_FOR_EACH_ENTRY_SAFE(item, nextItem, GetVnodeActiveList(), struct Vnode, actFreeEntry) {
        if ((struct ProcDirEntry *)item->data != entry) {
            continue;
        }

        if (VnodeFree(item) != LOS_OK) {
            PRINT_ERR("ProcEntryClearVnode free failed, entry: %s\n", entry->name);
        }
    }
    VnodeDrop();
    return;
}
///释放proc
static void FreeProcEntry(struct ProcDirEntry *entry)
{
    if (entry == NULL) {
        return;
    }

    if (entry->pf != NULL) {
        free(entry->pf);
        entry->pf = NULL;
    }
    if ((entry->dataType == PROC_DATA_FREE) && (entry->data != NULL)) {
        free(entry->data);
    }
    entry->data = NULL;
    free(entry);
}
///释放
void ProcFreeEntry(struct ProcDirEntry *pn)
{
    if (atomic_dec_and_test(&pn->count)) {
        FreeProcEntry(pn);
    }
}

void RemoveProcEntryTravalsal(struct ProcDirEntry *pn)
{
    if (pn == NULL) {
        return;
    }
    RemoveProcEntryTravalsal(pn->next);
    RemoveProcEntryTravalsal(pn->subdir);

    ProcEntryClearVnode(pn);

    ProcFreeEntry(pn);
}

void RemoveProcEntry(const char *name, struct ProcDirEntry *parent)
{
    struct ProcDirEntry *pn = NULL;
    const char *lastName = name;

    if ((name == NULL) || (strlen(name) == 0) || (procfsInit == false)) {
        return;
    }

    if (CheckProcName(name, &parent, &lastName) != 0) {
        return;
    }

    spin_lock(&procfsLock);

    pn = ProcFindNode(parent, lastName);
    if (pn == NULL) {
        PRINT_ERR("Error:name '%s' not found!\n", name);
        spin_unlock(&procfsLock);
        return;
    }
    ProcDetachNode(pn);

    spin_unlock(&procfsLock);

    RemoveProcEntryTravalsal(pn->subdir);

    ProcEntryClearVnode(pn);

    ProcFreeEntry(pn);
}

struct ProcDirEntry *ProcMkdirMode(const char *name, mode_t mode, struct ProcDirEntry *parent)
{
    return ProcCreateDir(parent, name, NULL, mode);
}

struct ProcDirEntry *ProcMkdir(const char *name, struct ProcDirEntry *parent)
{
    return ProcCreateDir(parent, name, NULL, 0);
}
///创建数据
struct ProcDirEntry *ProcCreateData(const char *name, mode_t mode, struct ProcDirEntry *parent,
                                    const struct ProcFileOperations *procFileOps, struct ProcDataParm *param)
{
    struct ProcDirEntry *pde = CreateProcEntry(name, mode, parent);
    if (pde != NULL) {
        if (procFileOps != NULL) {
            pde->procFileOps = procFileOps;
        }
        if (param != NULL) {
            pde->data = param->data;
            pde->dataType = param->dataType;
        }
    }
    return pde;
}

struct ProcDirEntry *ProcCreate(const char *name, mode_t mode, struct ProcDirEntry *parent,
                                const struct ProcFileOperations *procFileOps)
{
    return ProcCreateData(name, mode, parent, procFileOps, NULL);
}

int ProcStat(const char *file, struct ProcStat *buf)
{
    struct ProcDirEntry *pn = NULL;
    int len = sizeof(buf->name);
    int ret;

    pn = ProcFindEntry(file);
    if (pn == NULL) {
        return ENOENT;
    }
    ret = strncpy_s(buf->name, len, pn->name, len - 1);
    if (ret != EOK) {
        return ENAMETOOLONG;
    }
    buf->name[len - 1] = '\0';
    buf->stMode = pn->mode;
    buf->pPDE = pn;

    return 0;
}

static int GetNextDir(struct ProcDirEntry *pn, void *buf, size_t len)
{
    char *buff = (char *)buf;

    if (pn->pdirCurrent == NULL) {
        *buff = '\0';
        return -ENOENT;
    }
    int namelen = pn->pdirCurrent->nameLen;
    int ret = memcpy_s(buff, len, pn->pdirCurrent->name, namelen);
    if (ret != EOK) {
        return -ENAMETOOLONG;
    }

    pn->pdirCurrent = pn->pdirCurrent->next;
    pn->pf->fPos++;
    return ENOERR;
}
///打开 pro 
int ProcOpen(struct ProcFile *procFile)
{
    if (procFile == NULL) {
        return PROC_ERROR;
    }
    if (procFile->sbuf != NULL) {
        return OK;
    }

    struct SeqBuf *buf = LosBufCreat();//创建一个 seq buf
    if (buf == NULL) {
        return PROC_ERROR;
    }
    procFile->sbuf = buf;
    return OK;
}

static int ProcRead(struct ProcDirEntry *pde, char *buf, size_t len)
{
    if (pde == NULL || pde->pf == NULL) {
        return PROC_ERROR;
    }
    struct ProcFile *procFile = pde->pf;
    struct SeqBuf *sb = procFile->sbuf;

    if (sb->buf == NULL) {
        // only read once to build the storage buffer
        if (pde->procFileOps->read(sb, pde->data) != 0) {
            return PROC_ERROR;
        }
    }

    size_t realLen;
    loff_t pos = procFile->fPos;

    if ((pos >= sb->count) || (len == 0)) {
        /* there's no data or at the file tail. */
        realLen = 0;
    } else {
        realLen = MIN((sb->count - pos), MIN(len, INT_MAX));
        if (LOS_CopyFromKernel(buf, len, sb->buf + pos, realLen) != 0) {
            return PROC_ERROR;
        }

        procFile->fPos = pos + realLen;
    }

    return (ssize_t)realLen;
}

struct ProcDirEntry *OpenProcFile(const char *fileName, int flags, ...)
{
    struct ProcDirEntry *pn = ProcFindEntry(fileName);
    if (pn == NULL) {
        return NULL;
    }

    if (S_ISREG(pn->mode) && (pn->count != 1)) {
        return NULL;
    }

    pn->flags = (unsigned int)(pn->flags) | (unsigned int)flags;
    atomic_set(&pn->count, PROC_INUSE);
    if (ProcOpen(pn->pf) != OK) {
        return NULL;
    }
    if (S_ISREG(pn->mode) && (pn->procFileOps != NULL) && (pn->procFileOps->open != NULL)) {
        (void)pn->procFileOps->open((struct Vnode *)pn, pn->pf);
    }
    if (S_ISDIR(pn->mode)) {
        pn->pdirCurrent = pn->subdir;
        pn->pf->fPos = 0;
    }

    return pn;
}

int ReadProcFile(struct ProcDirEntry *pde, void *buf, size_t len)
{
    int result = -EPERM;

    if (pde == NULL) {
        return result;
    }
    if (S_ISREG(pde->mode)) {
        if ((pde->procFileOps != NULL) && (pde->procFileOps->read != NULL)) {
            result = ProcRead(pde, (char *)buf, len);
        }
    } else if (S_ISDIR(pde->mode)) {
        result = GetNextDir(pde, buf, len);
    }
    return result;
}
///写 proc 文件
int WriteProcFile(struct ProcDirEntry *pde, const void *buf, size_t len)
{
    int result = -EPERM;

    if (pde == NULL) {
        return result;
    }

    if (S_ISDIR(pde->mode)) {
        return -EISDIR;
    }

    spin_lock(&procfsLock);
    if ((pde->procFileOps != NULL) && (pde->procFileOps->write != NULL)) {
        result = pde->procFileOps->write(pde->pf, (const char *)buf, len, &(pde->pf->fPos));
    }
    spin_unlock(&procfsLock);
    return result;
}
/// seek proc 文件
loff_t LseekProcFile(struct ProcDirEntry *pde, loff_t offset, int whence)
{
    if (pde == NULL || pde->pf == NULL) {
        return PROC_ERROR;
    }

    struct ProcFile *procFile = pde->pf;

    loff_t result = -EINVAL;

    switch (whence) {
        case SEEK_CUR:
            result = procFile->fPos + offset;
            break;

        case SEEK_SET:
            result = offset;
            break;

        default:
            break;
    }

    if (result >= 0) {
        procFile->fPos = result;
    }

    return result;
}

int LseekDirProcFile(struct ProcDirEntry *pde, off_t *pos, int whence)
{
    /* Only allow SEEK_SET to zero */
    if ((whence != SEEK_SET) || (*pos != 0)) {
        return EINVAL;
    }
    pde->pdirCurrent = pde->subdir;
    pde->pf->fPos = 0;
    return ENOERR;
}

int CloseProcFile(struct ProcDirEntry *pde)
{
    int result = 0;

    if (pde == NULL) {
        return -EPERM;
    }
    pde->pf->fPos = 0;
    atomic_set(&pde->count, 1);
    if (S_ISDIR(pde->mode)) {
        pde->pdirCurrent = pde->subdir;
    }

    if ((pde->procFileOps != NULL) && (pde->procFileOps->release != NULL)) {
        result = pde->procFileOps->release((struct Vnode *)pde, pde->pf);
    }
    LosBufRelease(pde->pf->sbuf);
    pde->pf->sbuf = NULL;

    if (pde->parent == NULL) {
        FreeProcEntry(pde);
    }
    return result;
}

struct ProcDirEntry *GetProcRootEntry(void)
{
    return &g_procRootDirEntry;
}
