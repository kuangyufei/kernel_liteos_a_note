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

#include "vfs_zpfs.h"

#include <stdlib.h>
#include <unistd.h>

#include <fcntl.h>
#include <sys/statfs.h>

#include "fs/dirent_fs.h"
#include "inode/inode.h"
#include "internal.h"
#include "los_tables.h"

#ifdef LOSCFG_FS_ZPFS

/****************************************************************************
 * Name: CheckEntryExist
 *
 * Description:
 *   check if entry is exist or not.
 * Input Parameters:
 *   The path
 * Returned Value:
 *    true or false, it is true for the exist of path.
 ****************************************************************************/
bool CheckEntryExist(const char *entry)
{
    struct stat64 stat64Info;
    struct stat statInfo;

    if (stat64(entry, &stat64Info) == 0) {
        if (S_ISDIR(stat64Info.st_mode) != 0) {
            return true;
        } else {
            return false;
        }
    } else if (stat(entry, &statInfo) == 0) {
        if (S_ISDIR(statInfo.st_mode) != 0) {
            return true;
        } else {
            return false;
        }
    }

    return false;
}

/****************************************************************************
 * Name: GetZpfsConfig
 *
 * Description:
 *   get the zpfs configuration
 * Input Parameters:
 *   void
 * Returned Value:
 *    g_zpfsConfig  Save the patch gobal data.
 ****************************************************************************/
ZpfsConfig *GetZpfsConfig(const struct inode *inode)
{
    ZpfsConfig *zpfsCfg = (ZpfsConfig*)(inode->i_private);
    return zpfsCfg;
}

/****************************************************************************
 * Name: GetSourceList
 *
 * Description:
 *   Get the source path list.
 *   eg: /patch/etc:/patch/etc1:/patch/etc2
 * Input Parameters:
 *   source: /patch/etc:/patch/etc1:/patch/etc2
 *   sourcelist: output path arry
 *   num: output list num
 *
 * Returned Value:
 *    OK  The mounted folder parameter is right.
 *    EINVAL  The mounted folder parameter is wrong.
 *    ENOMEM  The memory is not enough
 ****************************************************************************/
static int GetSourceList(const char *source, char *sourceList[ZPFS_LEVELS], int *num)
{
    char *subSource = NULL;
    char *path = NULL;
    char *subPath = NULL;
    int index;
    int ret;

    subSource = strdup(source);
    if (subSource == NULL) {
        return -ENOMEM;
    }

    subPath = strtok_r(subSource, ":", &path);
    for (*num = 0; *num < ZPFS_LEVELS && subPath != NULL; (*num)++) {
        sourceList[*num] = strdup(subPath);
        if (sourceList[*num] == NULL) {
            ret = -ENOMEM;
            goto EXIT;
        }
        subPath = strtok_r(NULL, ":", &path);
    }

    if (subPath != NULL || *num == 0) {
        PRINTK("source path num %d error\n", *num);
        ret = -EINVAL;
        goto EXIT;
    }

    free(subSource);
    return OK;

EXIT:
    free(subSource);
    for (index = 0; index < *num; index++) {
        free(sourceList[index]);
        sourceList[index] = NULL;
    }
    *num = 0;
    return ret;
}


/****************************************************************************
 * Name: CheckInputParamsFormat
 *
 * Description:
 *   check the mounted folder format and validity. MAX three segment.
 *   eg: /patch/etc:/patch/etc1:/patch/etc2
 * Input Parameters:
 *   check the mounted folder. eg:
 *      source: /patch/etc:/patch/etc1:/patch/etc2
 *      target: /etc
 *
 * Returned Value:
 *    OK  The mounted folder parameter is right.
 *    EINVAL  The mounted folder parameter is wrong.
 *    ENOMEM  The memory is not enough
 ****************************************************************************/
static int CheckInputParamsFormat(const char *source, const char *target)
{
    char *sourceList[ZPFS_LEVELS] = {0};
    int num = 0;
    int ret;
    int index;
    bool isTargetValid = false;

    if (source == NULL || target == NULL) {
        return -EINVAL;
    }

    ret = GetSourceList(source, sourceList, &num);
    if (ret != OK) {
        goto EXIT;
    }

    for (index = 0; index < num; index++) {
        if (!CheckEntryExist(sourceList[index])) {
            ret = -ENOENT;
            goto EXIT;
        }
        /* target must same with one source path */
        if (strcmp(target, sourceList[index]) == 0) {
            isTargetValid = true;
        }
    }

    if (!isTargetValid) {
        ret = -EINVAL;
        goto EXIT;
    }
    ret = OK;
EXIT:
    for (index = 0; index < num; index++) {
        free(sourceList[index]);
    }
    return ret;
}

static void ZpfsFreeEntry(ZpfsEntry *zpfsEntry)
{
    if (zpfsEntry->mountedPath != NULL) {
        free(zpfsEntry->mountedPath);
        zpfsEntry->mountedPath = NULL;
    }
    if (zpfsEntry->mountedRelpath != NULL) {
        free(zpfsEntry->mountedRelpath);
        zpfsEntry->mountedRelpath = NULL;
    }
    zpfsEntry->mountedInode = NULL;
}

/****************************************************************************
 * Name: ZpfsFreeConfig
 *
 * Description:
 *   Umount the patch file system,
 *   Delete the global patch data and the mount pseudo point.
 *
 * Input Parameters:
 *   void
 * Returned Value:
 *   void
 ****************************************************************************/
void ZpfsFreeConfig(ZpfsConfig *zpfsCfg)
{
    struct inode *pInode = NULL;
    if (zpfsCfg == NULL) {
        return;
    }
    pInode = zpfsCfg->patchInode;
    if (pInode != NULL) {
        INODE_SET_TYPE(pInode, FSNODEFLAG_DELETED);
        inode_release(pInode);
        pInode = NULL;
    }

    for (int i = 0; i < zpfsCfg->entryCount; i++) {
        ZpfsFreeEntry(&(zpfsCfg->orgEntry[i]));
    }
    if (zpfsCfg->patchTarget != NULL) {
        free(zpfsCfg->patchTarget);
        zpfsCfg->patchTarget = NULL;
    }
    free(zpfsCfg);
    zpfsCfg = NULL;
}

/****************************************************************************
 * Name: IsTargetMounted
 * Description:
 *   The mount patch target path must be the mounted path.
 * Input Parameters:
 *   target:  The mount point
 * Returned Value:
 *   OK
 *   EINVAL  The mount patch target path is not the mounted path.
 ****************************************************************************/
static int IsTargetMounted(const char *target)
{
    struct inode *inode = NULL;
    char *path = NULL;
    struct statfs buf;

    if (!CheckEntryExist(target)) {
        return -EINVAL;
    }

    inode = inode_search((const char **)&target, (struct inode**)NULL,
        (struct inode**)NULL, (const char **)&path);
    if (inode == NULL || !INODE_IS_MOUNTPT(inode)) {
        PRINT_ERR("Can't to mount to this inode %s\n", target);
        return -EINVAL;
    }
    if ((inode->u.i_mops != NULL) && (inode->u.i_mops->statfs != NULL)) {
        if (inode->u.i_mops->statfs(inode, &buf) == OK) {
            if (buf.f_type == ZPFS_MAGIC) {
                return -EEXIST;
            }
        }
    }
    if (path == NULL || path[0] == '\0') {
        return -EEXIST;
    }
    return OK;
}

/****************************************************************************
 * Name: SaveZpfsParameter
 *
 * Description:
 *   When the patch file system is mounted, the patch gobal data is built.
 *
 * Input Parameters:
 *   source:  The mounted patch folder
 *   target:  The mount point
 * Returned Value:
 *    OK  Save the patch gobal data.
 *    ENOMEM  The memory is not enough.
 *    EBADMSG  There is no data.
 ****************************************************************************/
static int SaveZpfsParameter(const char *source, const char *target, struct ZpfsConfig *zpfsCfg)
{
    int ret;
    const char *path = NULL;
    int num = 0;
    int index;
    char *sourceList[ZPFS_LEVELS] = {0};

    /* save the mount point */
    zpfsCfg->patchTarget = strdup(target);
    if (zpfsCfg->patchTarget == NULL) {
        return -ENOMEM;
    }

    /* save the mount folder inode */
    zpfsCfg->patchTargetInode = inode_search((const char **)&target,
        (struct inode**)NULL, (struct inode**)NULL, &path);
    if (zpfsCfg->patchTargetInode == NULL) {
        return -EINVAL;
    }

    ret = GetSourceList(source, sourceList, &num);
    if (ret != OK) {
        return ret;
    }

    zpfsCfg->entryCount = num;
    for (index = 0; index < num; index++) {
        zpfsCfg->orgEntry[index].mountedPath = sourceList[index];

        /* save the mounted folder inode */
        zpfsCfg->orgEntry[index].mountedInode = inode_search((const char **)&sourceList[index],
            (struct inode**)NULL, (struct inode**)NULL, &path);
        if (zpfsCfg->orgEntry[index].mountedInode == NULL) {
            ret = -EINVAL;
            goto ERROR_PROCESS;
        }

        /* save the mounted relative path */
        zpfsCfg->orgEntry[index].mountedRelpath = (char*)path;
    }

    return OK;

ERROR_PROCESS:
    for (index = 0; index < num; index++) {
        free(sourceList[index]);
        sourceList[index] = NULL;
    }
    return ret;
}

/****************************************************************************
 * Name: ZpfsPrepare
 * Description:
 *   The main function of mount zpfs.
 * Input Parameters:
 *   source: The mounted parameter
 *   target: The mount path
 *   inodePtr: the new inode info of target
 *   force: It is true for zpfs
 * Returned Value:
 *   OK
 *   EINVAL  The parameter is wrong.
 *   ENOMEM  It is not enough memory.
 ****************************************************************************/
int ZpfsPrepare(const char *source, const char *target, struct inode **inodePtr, bool force)
{
    /* Check the mounted folder parameter */
    int ret = CheckInputParamsFormat(source, target);
    if (ret != OK) {
        PRINT_ERR("Parameter is err source:%s target:%s.\n", source, target);
        return ret;
    }

    /* The mount target path must be the mounted path */
    ret = IsTargetMounted(target);
    if (ret != OK) {
        PRINT_ERR("Can't to mount to this inode %s\n", target);
        return ret;
    }

    ZpfsConfig *zpfsCfg = (ZpfsConfig*) malloc(sizeof(struct ZpfsConfig));
    if (zpfsCfg == NULL) {
        PRINT_ERR("Memory is not enought.\n");
        return -ENOMEM;
    }
    (void)memset_s(zpfsCfg, sizeof(struct ZpfsConfig), 0, sizeof(struct ZpfsConfig));

    /* Save the patch global data */
    ret = SaveZpfsParameter(source, target, zpfsCfg);
    if (ret != OK) {
        ZpfsFreeConfig(zpfsCfg);
        PRINT_ERR("Memory is not enought.\n");
        return ret;
    }

    /* Create the patch inode */
    ret = inode_reserve_rootdir(target, inodePtr, force);
    if (ret != OK) {
        ZpfsFreeConfig(zpfsCfg);
        PRINT_ERR("failed to create mounted inode.\n");
        return ret;
    }
    zpfsCfg->patchInode = *inodePtr;
    (*inodePtr)->i_private = zpfsCfg;
    return ret;
}

/****************************************************************************
 * Name: ZpfsCleanUp
 * Description:
 *   Clean the configuration of zpfs.
 * Input Parameters:
 *   target: The mount path
 * Returned Value:
 *   void
 ****************************************************************************/
void ZpfsCleanUp(const void *node, const char *target)
{
    struct statfs buf;
    struct inode *inode = (struct inode *)node;
    if ((target == NULL) || (inode == NULL) ||
        (inode->u.i_mops == NULL) ||
        (inode->u.i_mops->statfs == NULL)) {
        return;
    }
    if (inode->u.i_mops->statfs(inode, &buf) == OK) {
        if (buf.f_type == ZPFS_MAGIC) {
            ZpfsConfig *zpfsCfg = inode->i_private;
            if (strcmp(zpfsCfg->patchTarget, target) == 0) {
                ZpfsFreeConfig(zpfsCfg);
            }
        }
    }
}

bool IsZpfsFileSystem(struct inode *inode)
{
    struct statfs buf;
    if (inode == NULL || inode->u.i_mops == NULL ||
        inode->u.i_mops->statfs == NULL) {
        return false;
    }
    if (inode->u.i_mops->statfs(inode, &buf) == OK) {
        if (buf.f_type == ZPFS_MAGIC) {
            return true;
        }
    }
    return false;
}

#endif // LOSCFG_FS_ZPFS
