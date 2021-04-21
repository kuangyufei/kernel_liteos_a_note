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
#include "fs/fs.h"
#include "fs/fs_operation.h"
#include "unistd.h"
#include "los_mux.h"
#include "los_list.h"
#include "los_atomic.h"
#include "los_vm_filemap.h"

#ifdef LOSCFG_KERNEL_VM

static struct file_map g_file_mapping = {0};

uint init_file_mapping()
{
    uint ret;

    LOS_ListInit(&g_file_mapping.head);

    ret = LOS_MuxInit(&g_file_mapping.lock, NULL);
    if (ret != LOS_OK) {
        PRINT_ERR("Create mutex for file map of page cache failed, (ret=%u)\n", ret);
    }

    return ret;
}

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

void add_mapping(struct file *filep, const char *fullpath)
{
    int path_len;
    status_t retval;
    struct file_map *fmap = NULL;
    struct page_mapping *mapping = NULL;

    if (filep == NULL || fullpath == NULL) {
        return;
    }

    (VOID)LOS_MuxLock(&g_file_mapping.lock, LOS_WAIT_FOREVER);
    mapping = find_mapping_nolock(fullpath);
    if (mapping) {
        LOS_AtomicInc(&mapping->ref);
        filep->f_mapping = mapping;
        mapping->host = filep;
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

    LOS_ListInit(&fmap->mapping.page_list);
    LOS_SpinInit(&fmap->mapping.list_lock);
    retval = LOS_MuxInit(&fmap->mapping.mux_lock, NULL);
    if (retval != LOS_OK) {
        PRINT_ERR("%s %d, Create mutex for mapping.mux_lock failed, status: %d\n", __FUNCTION__, __LINE__, retval);
        goto out;
    }

    LOS_ListTailInsert(&g_file_mapping.head, &fmap->head);

    filep->f_mapping = &fmap->mapping;
    filep->f_mapping->host = filep;

out:
    (VOID)LOS_MuxUnlock(&g_file_mapping.lock);
}

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
        filp->f_path = zalloc(len);
        strncpy_s(filp->f_path, strlen(new_path) + 1, new_path, len);
    }
    (VOID)LOS_MuxUnlock(&g_file_mapping.lock);
    (void)sem_post(&f_list->fl_sem);
    return LOS_OK;
}
#endif
