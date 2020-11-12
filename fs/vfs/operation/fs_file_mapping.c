/*
 * Copyright (c) 2013-2019, Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020, Huawei Device Co., Ltd. All rights reserved.
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

#include "fs/fs.h"
#include "fs/fs_operation.h"
#include "fs_other.h"
#include "unistd.h"
#include "los_mux.h"
#include "los_list.h"
#include "los_atomic.h"
#include "los_vm_filemap.h"
/******************************************************************
定义见于 ..\third_party\NuttX\include\nuttx\fs\fs.h
typedef volatile INT32 Atomic;
struct page_mapping {
  LOS_DL_LIST                           page_list;    /* all pages * /
  SPIN_LOCK_S                           list_lock;    /* lock protecting it * /
  LosMux                                mux_lock;     /* mutex lock * /
  unsigned long                         nrpages;      /* number of total pages * /
  unsigned long                         flags;
  Atomic                                ref;          /* reference counting * /
  struct file                           *host;        /* owner of this mapping * /
};

/* map: full_path(owner) <-> mapping * /
struct file_map {
  LOS_DL_LIST           head;		//挂入文件链表的节点
  LosMux                lock;         /* lock to protect this mapping * /
  struct page_mapping   mapping;	//映射内容区
  char                  *owner;     /* owner: full path of file * /
};

struct file
{
  unsigned int         f_magicnum;  /* file magic number * /
  int                  f_oflags;    /* Open mode flags * /
  FAR struct inode     *f_inode;    /* Driver interface * /
  loff_t               f_pos;       /* File position * /
  unsigned long        f_refcount;  /* reference count * /
  char                 *f_path;     /* File fullpath * /
  void                 *f_priv;     /* Per file driver private data * /
  const char           *f_relpath;  /* realpath * /
  struct page_mapping  *f_mapping;  /* mapping file to memory * /
  void                 *f_dir;      /* DIR struct for iterate the directory if open a directory * /
};

******************************************************************/
static struct file_map g_file_mapping = {0};
//初始化文件映射
uint init_file_mapping()
{
    uint ret;

    LOS_ListInit(&g_file_mapping.head);//初始化文件映射的链表

    ret = LOS_MuxInit(&g_file_mapping.lock, NULL);//初始化文件映射互斥锁
    if (ret != LOS_OK) {
        PRINT_ERR("Create mutex for file map of page cache failed, (ret=%u)\n", ret);
    }

    return ret;
}
//通过文件名查找文件映射，不用锁的方式
static struct page_mapping *find_mapping_nolock(const char *fullpath)
{
    struct file_map *fmap = NULL;

    LOS_DL_LIST_FOR_EACH_ENTRY(fmap, &g_file_mapping.head, struct file_map, head) {//遍历文件映射链表
        if (!strcmp(fmap->owner, fullpath)) {//对比文件路径
            return &fmap->mapping;
        }
    }

    return NULL;
}
//添加一个文件映射
void add_mapping(struct file *filep, const char *fullpath)
{
    void *tmp = NULL;
    struct file_map *fmap = NULL;
    int fmap_len = sizeof(struct file_map);
    int path_len;
    struct page_mapping *mapping = NULL;
    status_t retval;

    if (filep == NULL || fullpath == NULL) {
        return;
    }

    (VOID)LOS_MuxLock(&g_file_mapping.lock, LOS_WAIT_FOREVER);//操作临界区，先拿锁

    path_len = strlen(fullpath) + 1;
    mapping = find_mapping_nolock(fullpath);//先看有米有映射过
    if (mapping) {//有映射过的情况
        LOS_AtomicInc(&mapping->ref);//引用自增
        filep->f_mapping = mapping;//赋值
        mapping->host = filep;
        (VOID)LOS_MuxUnlock(&g_file_mapping.lock);//释放锁
        return;//收工
    }

    (VOID)LOS_MuxUnlock(&g_file_mapping.lock);//没有映射过的情况下 先释放锁

    fmap = (struct file_map *)LOS_MemAlloc(m_aucSysMem0, fmap_len);//分配一个file_map

    /* page-cache as a optimization feature, just return when out of memory *///页面缓存作为一个优化功能，当内存不足时返回

    if (!fmap) {
        PRINT_WARN("%s-%d: Mem alloc failed. fmap length(%d)\n",
                   __FUNCTION__, __LINE__, fmap_len);
        return;
    }
    tmp = LOS_MemAlloc(m_aucSysMem0, path_len);//分配一块内存放文件路径，为什么要这么做？因为需要在内核区操作，而参数路径在用户区

    /* page-cache as a optimization feature, just return when out of memory */

    if (!tmp) {
        PRINT_WARN("%s-%d: Mem alloc failed. fmap length(%d), fmap(%p), path length(%d)\n",
                   __FUNCTION__, __LINE__, fmap_len, fmap, path_len);
        LOS_MemFree(m_aucSysMem0, fmap);
        return;
    }

    (void)memset_s(fmap, fmap_len, 0, fmap_len);//清0
    fmap->owner = tmp;//赋值，此时owner指向内核区
    LOS_AtomicSet(&fmap->mapping.ref, 1);//引用设为1
    (void)strcpy_s(fmap->owner, path_len, fullpath);//从用户区 拷贝到内核区

    LOS_ListInit(&fmap->mapping.page_list);//初始化文件映射的页表链表，上面将会挂这个文件映射的所有虚拟内存页
    LOS_SpinInit(&fmap->mapping.list_lock);//初始化文件映射的自旋锁
    retval = LOS_MuxInit(&fmap->mapping.mux_lock, NULL);//初始化文件映射的互斥锁
    if (retval != LOS_OK) {
        PRINT_ERR("%s %d, Create mutex for mapping.mux_lock failed, status: %d\n", __FUNCTION__, __LINE__, retval);
    }
    (VOID)LOS_MuxLock(&g_file_mapping.lock, LOS_WAIT_FOREVER);//先拿全局变量g_file_mapping的互斥锁
    LOS_ListTailInsert(&g_file_mapping.head, &fmap->head);//因为要将本次文件挂上去
    (VOID)LOS_MuxUnlock(&g_file_mapping.lock);//释放锁

    filep->f_mapping = &fmap->mapping;//映射互绑操作，都是自己人了，
    filep->f_mapping->host = filep;//文件互绑操作，亲上加亲了。

    return;
}
//查找文件映射
struct page_mapping *find_mapping(const char *fullpath)
{
    struct page_mapping *mapping = NULL;

    if (fullpath == NULL) {
        return NULL;
    }

    (VOID)LOS_MuxLock(&g_file_mapping.lock, LOS_WAIT_FOREVER);

    mapping = find_mapping_nolock(fullpath);//找去
    if (mapping) {//找到
        LOS_AtomicInc(&mapping->ref);//引用自增
    }

    (VOID)LOS_MuxUnlock(&g_file_mapping.lock);

    return mapping;
}
//引用递减
void dec_mapping(struct page_mapping *mapping)
{
    if (mapping == NULL) {
        return;
    }

    (VOID)LOS_MuxLock(&g_file_mapping.lock, LOS_WAIT_FOREVER);
    if (LOS_AtomicRead(&mapping->ref) > 0) {
        LOS_AtomicDec(&mapping->ref);//ref 递减
    }
    (VOID)LOS_MuxUnlock(&g_file_mapping.lock);
}
//清除文件映射，无锁方式
void clear_file_mapping_nolock(const struct page_mapping *mapping)
{
    unsigned int i = 3; /* file start fd */
    struct file *filp = NULL;

    while (i < CONFIG_NFILE_DESCRIPTORS) {//循环遍历查找
        filp = &tg_filelist.fl_files[i];//一个个来
        if (filp->f_mapping == mapping) {//发现一样
            filp->f_mapping = NULL;//直接NULL，注意这里并没有break，而是继续撸到最后，这里如果break了会怎样？ @note_thinking
        }
        i++;
    }
}
//删除一个映射
int remove_mapping_nolock(const char *fullpath, const struct file *ex_filp)
{
    int fd;
    struct file *filp = NULL;
    struct file_map *fmap = NULL;
    struct page_mapping *mapping = NULL;
    struct inode *node = NULL;

    if (fullpath == NULL) {
        set_errno(EINVAL);
        return EINVAL;
    }

    /* file start fd */

    for (fd = 3; fd < CONFIG_NFILE_DESCRIPTORS; fd++) {
        node = files_get_openfile(fd);//通过文件句柄获取inode
        if (node == NULL) {
            continue;
        }
        filp = &tg_filelist.fl_files[fd];//拿到文件

        /* ex_filp NULL: do not exclude any file, just matching the file name ; ex_filp not NULL: exclude it.
         * filp != ex_filp includes the two scenarios.
         */

        if (filp != ex_filp) {
            if (filp->f_path == NULL) {
                continue;
            }
            if ((strcmp(filp->f_path, fullpath) == 0)) {
                PRINT_WARN("%s is open(fd=%d), remove cache failed.\n", fullpath, fd);
                set_errno(EBUSY);
                return EBUSY;
            }
        }
    }

    (VOID)LOS_MuxLock(&g_file_mapping.lock, LOS_WAIT_FOREVER);

    mapping = find_mapping_nolock(fullpath);
    if (!mapping) {
        /* this scenario is a normal case */

        goto out;
    }

    (VOID)LOS_MuxDestroy(&mapping->mux_lock);
    clear_file_mapping_nolock(mapping);//清除自己
    OsFileCacheRemove(mapping);//从缓存中删除自己
    fmap = LOS_DL_LIST_ENTRY(mapping,
    struct file_map, mapping);//找到自己的inode
    LOS_ListDelete(&fmap->head);//从链表中摘除自己
    LOS_MemFree(m_aucSysMem0, fmap);

out:
    (VOID)LOS_MuxUnlock(&g_file_mapping.lock);

    return OK;
}
//删除文件映射
int remove_mapping(const char *fullpath, const struct file *ex_filp)
{
    int ret;
    struct filelist *f_list = NULL;

    f_list = &tg_filelist;
    ret = sem_wait(&f_list->fl_sem);//等待信号量
    if (ret < 0) {
        PRINTK("sem_wait error, ret=%d\n", ret);
        return VFS_ERROR;
    }

    ret = remove_mapping_nolock(fullpath, ex_filp);//调用删除实体

    (void)sem_post(&f_list->fl_sem);//发出信号量
    return OK;
}

void rename_mapping(const char *src_path, const char *dst_path)
{
    int ret;
    void *tmp = NULL;
    int path_len;
    struct file_map *fmap = NULL;
    struct page_mapping *mapping = NULL;

    if (src_path == NULL || dst_path == NULL) {
        return;
    }

    path_len = strlen(dst_path) + 1;

    /* protect the whole list in case of this node been deleted just after we found it */

    (VOID)LOS_MuxLock(&g_file_mapping.lock, LOS_WAIT_FOREVER);

    mapping = find_mapping_nolock(src_path);
    if (!mapping) {
        /* this scenario is a normal case */

        goto out;
    }

    fmap = LOS_DL_LIST_ENTRY(mapping,
    struct file_map, mapping);

    tmp = LOS_MemAlloc(m_aucSysMem0, path_len);
    if (!tmp) {
        /* in this extremly low-memory situation, un-referenced page caches can be recycled by Pagecache LRU */

        PRINT_ERR("%s-%d: Mem alloc failed, path length(%d)\n", __FUNCTION__, __LINE__, path_len);
        goto out;
    }
    ret = strcpy_s(tmp, path_len, dst_path);
    if (ret != 0) {
        (VOID)LOS_MemFree(m_aucSysMem0, tmp);
        goto out;
    }

    /* whole list is locked, so we don't protect this node here */

    (VOID)LOS_MemFree(m_aucSysMem0, fmap->owner);
    fmap->owner = tmp;

out:
    (VOID)LOS_MuxUnlock(&g_file_mapping.lock);
    return;
}

