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

#ifndef _PROC_FS_H
#define _PROC_FS_H

#include <sys/types.h>
#include <sys/stat.h>

#include "los_config.h"

#ifdef LOSCFG_FS_PROC
#include "linux/spinlock.h"
#include "asm/atomic.h"
#include "fs/file.h"
#include "los_seq_buf.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif /* __cplusplus */
#endif /* __cplusplus */

typedef unsigned short fmode_t;
#define PROC_ERROR (-1)

/* 64bit hashes as llseek() offset (for directories) */
#define FMODE_64BITHASH   ((fmode_t)0x400)
/* 32bit hashes as llseek() offset (for directories) */
#define FMODE_32BITHASH   ((fmode_t)0x200)
/* File is opened using open(.., 3, ..) and is writeable only for ioctls
 *     (specialy hack for floppy.c)
 */
#define FMODE_WRITE_IOCTL ((fmode_t)0x100)
/* File is opened with O_EXCL (only set for block devices) */
#define FMODE_EXCL        ((fmode_t)0x80)
/* File is opened with O_NDELAY (only set for block devices) */
#define FMODE_NDELAY      ((fmode_t)0x40)
/* File is opened for execution with sys_execve / sys_uselib */
#define FMODE_EXEC        ((fmode_t)0x20)
/* file can be accessed using pwrite */
#define FMODE_PWRITE      ((fmode_t)0x10)
/* file can be accessed using pread */
#define FMODE_PREAD       ((fmode_t)0x8)
/* file is seekable */
#define FMODE_LSEEK       ((fmode_t)0x4)
/* file is open for writing */
#define FMODE_WRITE       ((fmode_t)0x2)
/* file is open for reading */
#define FMODE_READ        ((fmode_t)0x1)

struct ProcFile;

struct ProcFileOperations {
    char *name;
    ssize_t (*write)(struct ProcFile *pf, const char *buf, size_t count, loff_t *ppos);
    int (*open)(struct Vnode *vnode, struct ProcFile *pf);
    int (*release)(struct Vnode *vnode, struct ProcFile *pf);
    int (*read)(struct SeqBuf *m, void *v);
};

struct ProcDirEntry {
    mode_t mode;
    int flags;
    const struct ProcFileOperations *procFileOps;
    struct ProcFile *pf;
    struct ProcDirEntry *next, *parent, *subdir;
    void *data;
    atomic_t count; /* open file count */
    spinlock_t pdeUnloadLock;

    int nameLen;
    struct ProcDirEntry *pdirCurrent;
    char name[NAME_MAX];
    enum VnodeType type;
};

struct ProcFile {
    fmode_t fMode;
    spinlock_t fLock;
    atomic_t fCount;
    struct SeqBuf *sbuf;
    struct ProcDirEntry *pPDE;
    unsigned long long fVersion;
    loff_t fPos;
    char name[NAME_MAX];
};

struct ProcStat {
    mode_t stMode;
    struct ProcDirEntry *pPDE;
    char name[NAME_MAX];
};

struct ProcData {
    ssize_t size;
    loff_t fPos;
    char buf[1];
};

#define PROCDATA(n) (sizeof(struct ProcData) + (n))

#define S_IALLUGO (S_ISUID | S_ISGID | S_ISVTX | S_IRWXU | S_IRWXG | S_IRWXO)

/**
 * Interface for modules using proc below internal proc moudule;
 */
/**
 * @ingroup  procfs
 * @brief create a proc node
 *
 * @par Description:
 * This API is used to create the node by 'name' and parent vnode
 *
 * @attention
 * <ul>
 * <li>This interface should be called after system initialization.</li>
 * <li>The parameter name should be a valid string.</li>
 * </ul>
 *
 * @param  name    [IN] Type #const char * The name of the node to be created.
 * @param  mode    [IN] Type #mode_t the mode of create's node.
 * @param  parent  [IN] Type #struct ProcDirEntry * the parent node of the node to be created,
 * if pass NULL, default parent node is "/proc".
 *
 * @retval #NULL                Create failed.
 * @retval #ProcDirEntry*     Create successfully.
 * @par Dependency:
 * <ul><li>proc_fs.h: the header file that contains the API declaration.</li></ul>
 * @see
 *
 */
extern struct ProcDirEntry *CreateProcEntry(const char *name, mode_t mode, struct ProcDirEntry *parent);

/**
 * @ingroup  procfs
 * @brief remove a proc node
 *
 * @par Description:
 * This API is used to remove the node by 'name' and parent vnode
 *
 * @attention
 * <ul>
 * <li>This interface should be called after system initialization.</li>
 * <li>The parameter name should be a valid string.</li>
 * </ul>
 *
 * @param  name   [IN] Type #const char * The name of the node to be removed.
 * @param  parent [IN] Type #struct ProcDirEntry * the parent node of the node to be remove.
 *
 * @par Dependency:
 * <ul><li>proc_fs.h: the header file that contains the API declaration.</li></ul>
 * @see
 *
 */
extern void RemoveProcEntry(const char *name, struct ProcDirEntry *parent);

/**
 * @ingroup  procfs
 * @brief create a proc directory node
 *
 * @par Description:
 * This API is used to create the directory node by 'name' and parent vnode
 *
 * @attention
 * <ul>
 * <li>This interface should be called after system initialization.</li>
 * <li>The parameter name should be a valid string.</li>
 * </ul>
 *
 * @param  name   [IN] Type #const char * The name of the node directory to be created.
 * @param  parent [IN] Type #struct ProcDirEntry * the parent node of the directory node to be created,
 * if pass NULL, default parent node is "/proc".
 *
 * @retval #NULL               Create failed.
 * @retval #ProcDirEntry*    Create successfully.
 * @par Dependency:
 * <ul><li>proc_fs.h: the header file that contains the API declaration.</li></ul>
 * @see
 *
 */
extern struct ProcDirEntry *ProcMkdir(const char *name, struct ProcDirEntry *parent);

/**
 * @ingroup  procfs
 * @brief create a proc  node
 *
 * @par Description:
 * This API is used to create the node by 'name' and parent vnode,
 * And assignment operation function
 *
 * @attention
 * <ul>
 * <li>This interface should be called after system initialization.</li>
 * <li>The parameter name should be a valid string.</li>
 * </ul>
 *
 * @param  name      [IN] Type #const char * The name of the node to be created.
 * @param  mode      [IN] Type #mode_t the mode of create's node.
 * @param  parent    [IN] Type #struct ProcDirEntry * the parent node of the node to be created.
 * @param  procFops [IN] Type #const struct ProcFileOperations * operation function of the node.
 *
 * @retval #NULL               Create failed.
 * @retval #ProcDirEntry*    Create successfully.
 * @par Dependency:
 * <ul><li>proc_fs.h: the header file that contains the API declaration.</li></ul>
 * @see
 *
 */
extern struct ProcDirEntry *ProcCreate(const char *name, mode_t mode,
    struct ProcDirEntry *parent, const struct ProcFileOperations *procFops);

/**
 * @ingroup  procfs
 * @brief init proc fs
 *
 * @par Description:
 * This API is used to init proc fs.
 *
 * @attention
 * <ul>
 * <li>None.</li>
 * </ul>
 *
 * @param  NONE
 *
 * @retval NONE
 * @par Dependency:
 * <ul><li>proc_fs.h: the header file that contains the API declaration.</li></ul>
 * @see ProcFsInit
 *
 */
extern void ProcFsInit(void);

#ifdef __cplusplus
#if __cplusplus
}
#endif /* __cplusplus */
#endif /* __cplusplus */

#endif /* LOSCFG_FS_PROC */
#endif /* _PROC_FS_H */
