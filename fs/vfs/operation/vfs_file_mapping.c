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

#include "fs/file.h"
#include "fs/file.h"
#include "fs/fs_operation.h"
#include "unistd.h"
#include "los_mux.h"
#include "los_list.h"
#include "los_atomic.h"
#include "los_vm_filemap.h"

#ifdef LOSCFG_KERNEL_VM

static struct file_map g_file_mapping = {0};//用于挂载所有文件的file_map

#if 0 
定义见于 ..\third_party\NuttX\include\nuttx\fs\fs.h
typedef volatile INT32 Atomic;
//page_mapping描述的是一个文件在内存中被映射了多少页,<文件,文件页的关系>
/* file mapped in VMM pages */
struct page_mapping {//记录文件页和文件关系的结构体,叫文件页映射
  LOS_DL_LIST                           page_list;    /* all pages */ //链表上挂的是属于该文件的所有FilePage，这些页的内容都来源同一个文件
  SPIN_LOCK_S                           list_lock;    /* lock protecting it */ //操作page_list的自旋锁
  LosMux                                mux_lock;     /* mutex lock */	//		//操作page_mapping的互斥量
  unsigned long                         nrpages;      /* number of total pages */ //page_list的节点数量	
  unsigned long                         flags;			//@note_why 全量代码中也没查到源码中对其操作	
  Atomic                                ref;          /* reference counting */	//引用次数(自增/自减),对应add_mapping/dec_mapping
  struct file                           *host;        /* owner of this mapping *///属于哪个文件的映射
};

/* map: full_path(owner) <-> mapping */ //叫文件映射
struct file_map { //为在内核层面文件在内存的身份证,每个需映射到内存的文件必须创建一个file_map，都挂到全局g_file_mapping链表上
  LOS_DL_LIST           head;		//链表节点,用于挂到g_file_mapping上
  LosMux                lock;         /* lock to protect this mapping */
  struct page_mapping   mapping;	//每个文件都有唯一的page_mapping标识其在内存的身份
  char                  *owner;     /* owner: full path of file *///文件全路径来标识唯一性
};

struct file //文件系统最重要的两个结构体之一,另一个是inode
{
  unsigned int         f_magicnum;  /* file magic number */ //文件魔法数字
  int                  f_oflags;    /* Open mode flags */	//打开模式标签
  FAR struct inode     *f_inode;    /* Driver interface */	//设备驱动程序
  loff_t               f_pos;       /* File position */		//文件的位置
  unsigned long        f_refcount;  /* reference count */	//被引用的数量,一个文件可被多个进程打开
  char                 *f_path;     /* File fullpath */		//全路径
  void                 *f_priv;     /* Per file driver private data */ //文件私有数据
  const char           *f_relpath;  /* realpath */			//真实路径
  struct page_mapping  *f_mapping;  /* mapping file to memory */ //与内存的映射 page-cache
  void                 *f_dir;      /* DIR struct for iterate the directory if open a directory */ //所在目录
};

#endif


/**************************************************************************************************
 初始化文件映射模块，
 file_map: 每个需映射到内存的文件必须创建一个 file_map，都挂到全局g_file_mapping链表上
 page_mapping: 记录的是<文件，文件页>的关系，一个文件在操作过程中被映射成了多少个页，
 file：是文件系统管理层面的概念
**************************************************************************************************/

uint init_file_mapping()
{
    uint ret;

    LOS_ListInit(&g_file_mapping.head);//初始化全局文件映射节点，所有文件的映射都将g_file_mapping.head挂在链表上

    ret = LOS_MuxInit(&g_file_mapping.lock, NULL);//初始化文件映射互斥锁
    if (ret != LOS_OK) {
        PRINT_ERR("Create mutex for file map of page cache failed, (ret=%u)\n", ret);
    }

    return ret;
}
///清除文件映射
static void clear_file_mapping(const struct page_mapping *mapping)
{
    unsigned int i = 3; /* file start fd */
    struct file *filp = NULL;

    while (i < CONFIG_NFILE_DESCRIPTORS) {
        filp = &tg_filelist.fl_files[i];
        if (filp->f_mapping == mapping) {
            filp->f_mapping = NULL;
        }
        i++;
    }
}
///以无锁的方式通过文件名查找文件映射并返回
static struct page_mapping *find_mapping_nolock(const char *fullpath)
{
    char *map_name = NULL;
    struct file_map *fmap = NULL;
    int name_len = strlen(fullpath);

    LOS_DL_LIST_FOR_EACH_ENTRY(fmap, &g_file_mapping.head, struct file_map, head) {
        map_name = fmap->rename ? fmap->rename: fmap->owner;
        if ((name_len == fmap->name_len)  && !strcmp(map_name, fullpath)) {
            return &fmap->mapping;
        }
    }

    return NULL;
}
/**************************************************************************************************
 增加一个文件映射，这个函数被do_open()函数调用，每打开一次文件就会调用一次
 注意不同的进程打开同一个文件，拿到的file是不一样的。
 https://blog.csdn.net/cywosp/article/details/38965239
**************************************************************************************************/
void add_mapping(struct file *filep, const char *fullpath)
{
    int path_len;
    status_t retval;
    struct file_map *fmap = NULL;
    struct page_mapping *mapping = NULL;

    if (filep == NULL || fullpath == NULL) {
        return;
    }

    (VOID)LOS_MuxLock(&g_file_mapping.lock, LOS_WAIT_FOREVER);//操作临界区，先拿锁
    mapping = find_mapping_nolock(fullpath);//是否已有文件映射
    if (mapping) {
        LOS_AtomicInc(&mapping->ref);//引用自增
        filep->f_mapping = mapping;//记录page_mapping的老板
        mapping->host = filep;//释放锁
        goto out;
    }

    path_len = strlen(fullpath);

    fmap = (struct file_map *)zalloc(sizeof(struct file_map) + path_len + 1);
    if (!fmap) {
        PRINT_WARN("%s %d, Mem alloc failed.\n", __FUNCTION__, __LINE__);
        goto out;
    }

    LOS_AtomicSet(&fmap->mapping.ref, 1);

    fmap->name_len = path_len;
    (void)strcpy_s(fmap->owner, path_len + 1, fullpath);

    LOS_ListInit(&fmap->mapping.page_list);//初始化文件映射的页表链表，上面将会挂这个文件映射的所有虚拟内存页
    LOS_SpinInit(&fmap->mapping.list_lock);//初始化文件映射的自旋锁
    retval = LOS_MuxInit(&fmap->mapping.mux_lock, NULL);//初始化文件映射的互斥锁
    if (retval != LOS_OK) {
        PRINT_ERR("%s %d, Create mutex for mapping.mux_lock failed, status: %d\n", __FUNCTION__, __LINE__, retval);
        goto out;
    }

    LOS_ListTailInsert(&g_file_mapping.head, &fmap->head);//将文件映射结点挂入全局链表

    filep->f_mapping = &fmap->mapping;//<file,file_map>之间互绑
    filep->f_mapping->host = filep;//<file,file_map>之间互绑，从此相亲相爱一家人

out:
    (VOID)LOS_MuxUnlock(&g_file_mapping.lock);
}

/******************************************************************************
 删除一个文件映射,需要有个三个地方删除才算断开了文件和内存的联系.
 
 以无锁的方式删除映射
******************************************************************************/

int remove_mapping_nolock(struct page_mapping *mapping)
{
    struct file_map *fmap = NULL;

    if (mapping == NULL) {
        set_errno(EINVAL);
        return EINVAL;
    }

    (VOID)LOS_MuxDestroy(&mapping->mux_lock);
    clear_file_mapping(mapping);
    OsFileCacheRemove(mapping);
    fmap = LOS_DL_LIST_ENTRY(mapping, struct file_map, mapping);
    LOS_ListDelete(&fmap->head);
    if (fmap->rename) {
        LOS_MemFree(m_aucSysMem0, fmap->rename);
    }
    LOS_MemFree(m_aucSysMem0, fmap);

    return OK;
}
///无锁方式引用递减，删除或关闭文件时 由 files_close_internal调用
void dec_mapping_nolock(struct page_mapping *mapping)
{
    if (mapping == NULL) {
        return;
    }

    (VOID)LOS_MuxLock(&g_file_mapping.lock, LOS_WAIT_FOREVER);
    if (LOS_AtomicRead(&mapping->ref) > 0) {
        LOS_AtomicDec(&mapping->ref);
    }

    if (LOS_AtomicRead(&mapping->ref) <= 0) {
        remove_mapping_nolock(mapping);
    } else {
        OsFileCacheFlush(mapping);
    }

    (VOID)LOS_MuxUnlock(&g_file_mapping.lock);
}
///已有锁的方式删除文件映射
int remove_mapping(const char *fullpath)
{
    int ret;
    struct filelist *f_list = NULL;
    struct page_mapping *mapping = NULL;

    f_list = &tg_filelist;
    ret = sem_wait(&f_list->fl_sem);
    if (ret < 0) {
        PRINTK("sem_wait error, ret=%d\n", ret);
        return VFS_ERROR;
    }

    (VOID)LOS_MuxLock(&g_file_mapping.lock, LOS_WAIT_FOREVER);

    mapping = find_mapping_nolock(fullpath);
    if (mapping) {
        ret = remove_mapping_nolock(mapping);
    }

    (VOID)LOS_MuxUnlock(&g_file_mapping.lock);

    (void)sem_post(&f_list->fl_sem);
    return OK;
}
///重命名映射
void rename_mapping(const char *src_path, const char *dst_path)
{
    int ret;
    char *tmp = NULL;
    int path_len;
    struct file_map *fmap = NULL;
    struct page_mapping *mapping = NULL;

    if (src_path == NULL || dst_path == NULL) {
        return;
    }

    path_len = strlen(dst_path);

    /* protect the whole list in case of this node been deleted just after we found it */

    (VOID)LOS_MuxLock(&g_file_mapping.lock, LOS_WAIT_FOREVER);

    mapping = find_mapping_nolock(src_path);
    if (!mapping) {
        goto out;
    }

    fmap = LOS_DL_LIST_ENTRY(mapping, struct file_map, mapping);

    tmp = LOS_MemAlloc(m_aucSysMem0, path_len + 1);
    if (!tmp) {
        PRINT_ERR("%s-%d: Mem alloc failed, path length(%d)\n", __FUNCTION__, __LINE__, path_len);
        goto out;
    }
    ret = strncpy_s(tmp, path_len, dst_path, strlen(dst_path));
    if (ret != 0) {
        (VOID)LOS_MemFree(m_aucSysMem0, tmp);
        goto out;
    }

    tmp[path_len] = '\0';
    fmap->rename = tmp;

out:
    (VOID)LOS_MuxUnlock(&g_file_mapping.lock);
}
///更新文件路径
int update_file_path(const char *old_path, const char *new_path)
{
    struct filelist *f_list = NULL;
    struct file *filp = NULL;
    int ret;

    f_list = &tg_filelist;
    ret = sem_wait(&f_list->fl_sem);
    if (ret < 0) {
        PRINTK("sem_wait error, ret=%d\n", ret);
        return VFS_ERROR;
    }

    (VOID)LOS_MuxLock(&g_file_mapping.lock, LOS_WAIT_FOREVER);
    for (int i = 3; i < CONFIG_NFILE_DESCRIPTORS; i++) {
        if (!get_bit(i)) {
            continue;
        }
        filp = &tg_filelist.fl_files[i];
        if (filp->f_path == NULL || strcmp(filp->f_path, old_path)) {
            continue;
        }
        int len = strlen(new_path) + 1;
        char *tmp_path = LOS_MemAlloc(m_aucSysMem0, len);
        if (tmp_path == NULL) {
            PRINT_ERR("%s-%d: Mem alloc failed, path length(%d)\n", __FUNCTION__, __LINE__, len);
            ret = VFS_ERROR;
            goto out;
        }
        ret = strncpy_s(tmp_path, strlen(new_path) + 1, new_path, len);
        if (ret != 0) {
            (VOID)LOS_MemFree(m_aucSysMem0, tmp_path);
            PRINT_ERR("%s-%d: strcpy failed.\n", __FUNCTION__, __LINE__);
            ret = VFS_ERROR;
            goto out;
        }
        free(filp->f_path);
        filp->f_path = tmp_path;
    }
    ret = LOS_OK;

out:
    (VOID)LOS_MuxUnlock(&g_file_mapping.lock);
    (void)sem_post(&f_list->fl_sem);
    return ret;
}

#ifdef LOSCFG_DEBUG_VERSION
struct file_map* GetFileMappingList(void)
{
    return &g_file_mapping;
}
#endif
#endif
