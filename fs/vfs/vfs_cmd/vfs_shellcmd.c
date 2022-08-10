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

#include "los_config.h"
#include "sys/mount.h"

#ifdef LOSCFG_SHELL

#include "los_typedef.h"
#include "shell.h"
#include "sys/stat.h"
#include "stdlib.h"
#include "unistd.h"
#include "fcntl.h"
#include "sys/statfs.h"
#include "stdio.h"
#include "pthread.h"

#include "shcmd.h"
#include "securec.h"
#include "show.h"
#include "los_syscall.h"

#include "los_process_pri.h"
#include <ctype.h>
#include "fs/fs_operation.h"

typedef enum {
  RM_RECURSIVER,	//递归删除
  RM_FILE,			//删除文件
  RM_DIR,			//删除目录
  CP_FILE,			//拷贝文件
  CP_COUNT			//拷贝数量
} wildcard_type;

#define ERROR_OUT_IF(condition, message_function, handler) \ //这种写法挺妙的
    do { \
        if (condition) { \
            message_function; \
            handler; \
        } \
    } while (0)

static inline void set_err(int errcode, const char *err_message)
{
  set_errno(errcode);
  perror(err_message);
}

int osShellCmdDoChdir(const char *path)
{
  char *fullpath = NULL;
  char *fullpath_bak = NULL;
  int ret;
  char *shell_working_directory = OsShellGetWorkingDirectory();
    if (shell_working_directory == NULL) {
        return -1;
    }

    if (path == NULL) {
        LOS_TaskLock();
        PRINTK("%s\n", shell_working_directory);
        LOS_TaskUnlock();

        return 0;
    }

    ERROR_OUT_IF(strlen(path) > PATH_MAX, set_err(ENOTDIR, "cd error"), return -1);

    ret = vfs_normalize_path(shell_working_directory, path, &fullpath);
    ERROR_OUT_IF(ret < 0, set_err(-ret, "cd error"), return -1);

    fullpath_bak = fullpath;
    ret = chdir(fullpath);
    if (ret < 0) {
        free(fullpath_bak);
        perror("cd");
        return -1;
    }

    /* copy full path to working directory */

    LOS_TaskLock();
    ret = strncpy_s(shell_working_directory, PATH_MAX, fullpath, strlen(fullpath));
    if (ret != EOK) {
        free(fullpath_bak);
        LOS_TaskUnlock();
        return -1;
    }
    LOS_TaskUnlock();
    /* release normalize directory path name */

    free(fullpath_bak);

    return 0;
}
/**
 * @brief 添加内置命令 ls
 * @verbatim
  命令功能
  ls命令用来显示当前目录的内容。

  命令格式
  ls [path]

  path为空时，显示当前目录的内容。
  path为无效文件名时，显示失败，提示：
  ls error: No such directory。
  path为有效目录路径时，会显示对应目录下的内容。

  使用指南
  ls命令显示当前目录的内容。
  ls可以显示文件的大小。
  proc下ls无法统计文件大小，显示为0。
 * @endverbatim
 * @param argc Shell命令中，参数个数。
 * @param argv 为指针数组，每个元素指向一个字符串，可以根据选择命令类型，决定是否要把命令关键字传入给注册函数。
 * @return int 
 */
int osShellCmdLs(int argc, const char **argv)
{
  char *fullpath = NULL;
  const char *filename = NULL;
  int ret;
  char *shell_working_directory = OsShellGetWorkingDirectory();//获取当前工作目录
    if (shell_working_directory == NULL) {
        return -1;
    }

  ERROR_OUT_IF(argc > 1, PRINTK("ls or ls [DIRECTORY]\n"), return -1);

  if (argc == 0) { //木有参数时 -> #ls 
      ls(shell_working_directory);//执行ls 当前工作目录
      return 0;
    }

  filename = argv[0];//有参数时 -> #ls ../harmony  or #ls /no such file or directory
  ret = vfs_normalize_path(shell_working_directory, filename, &fullpath);//获取全路径,注意这里带出来fullpath，而fullpath已经在内核空间
  ERROR_OUT_IF(ret < 0, set_err(-ret, "ls error"), return -1);

  ls(fullpath);//执行 ls 全路径
  free(fullpath);//释放全路径，为啥要释放，因为fullpath已经由内核空间分配

  return 0;
}

/**
 * @brief 
 * @verbatim
  命令功能
  cd命令用来改变当前目录。

  命令格式
  cd [path]

  未指定目录参数时，会跳转至根目录。
  cd后加路径名时，跳转至该路径。
  路径名以 /（斜杠）开头时，表示根目录。
  .（点）表示当前目录。
  ..（点点）表示父目录。
  cd ..
 * @endverbatim
 * @param argc 
 * @param argv 
 * @return int 
 */
int osShellCmdCd(int argc, const char **argv)
{
  if (argc == 0) { //没有参数时 #cd 
      (void)osShellCmdDoChdir("/");
      return 0;
    }

  (void)osShellCmdDoChdir(argv[0]);//#cd .. 带参数情况

  return 0;
}

#define CAT_BUF_SIZE  512
#define CAT_TASK_PRIORITY  10
#define CAT_TASK_STACK_SIZE  0x3000
pthread_mutex_t g_mutex_cat = PTHREAD_MUTEX_INITIALIZER;

/**
 * @brief 
 * @verbatim
cat用于显示文本文件的内容。

命令格式
cat [pathname]

使用指南
cat用于显示文本文件的内容。

使用实例
举例：cat harmony.txt
 * @endverbatim
 * @param arg 
 * @return int 
 */
int osShellCmdDoCatShow(UINTPTR arg) //shellcmd_cat 任务实现
{
  int ret = 0;
  char buf[CAT_BUF_SIZE];
  size_t size, written, toWrite;
  ssize_t cnt;
  char *fullpath = (char *)arg;
  FILE *ini = NULL;

  (void)pthread_mutex_lock(&g_mutex_cat);
  ini = fopen(fullpath, "r");//打开文件
    if (ini == NULL) {
        ret = -1;
        perror("cat error");
        goto out;
    }

    do {
      (void)memset_s(buf, sizeof(buf), 0, CAT_BUF_SIZE);
      size = fread(buf, 1, CAT_BUF_SIZE, ini); //读取文件内容
        if ((int)size < 0) {
            ret = -1;
            perror("cat error");
            goto out_with_fclose;
        }

        for (toWrite = size, written = 0; toWrite > 0;) {
          cnt = write(1, buf + written, toWrite);//stdout:1 标准输出文件
            if (cnt == 0) {
                /* avoid task-starvation */
                (void)LOS_TaskDelay(1);
                continue;
            } else if (cnt < 0) {
                perror("cat write error");
                break;
            }

          written += cnt;
          toWrite -= cnt;
        }
    }
  while (size > 0);

out_with_fclose:
  (void)fclose(ini);
out:
  free(fullpath);
  (void)pthread_mutex_unlock(&g_mutex_cat);
  return ret;
}
/*!
cat用于显示文本文件的内容。cat [pathname]
cat weharmony.txt
*/
int osShellCmdCat(int argc, const char **argv)
{
  char *fullpath = NULL;
  int ret;
  unsigned int ca_task;
  struct Vnode *vnode = NULL;
  TSK_INIT_PARAM_S init_param;
  char *shell_working_directory = OsShellGetWorkingDirectory();//显示当前目录 pwd
    if (shell_working_directory == NULL) {
        return -1;
    }

  ERROR_OUT_IF(argc != 1, PRINTK("cat [FILE]\n"), return -1);

  ret = vfs_normalize_path(shell_working_directory, argv[0], &fullpath);//由相对路径获取绝对路径
  ERROR_OUT_IF(ret < 0, set_err(-ret, "cat error"), return -1);

  VnodeHold();
  ret = VnodeLookup(fullpath, &vnode, O_RDONLY);
    if (ret != LOS_OK) {
        set_errno(-ret);
        perror("cat error");
        VnodeDrop();
        free(fullpath);
        return -1;
    }
    if (vnode->type != VNODE_TYPE_REG) {
        set_errno(EINVAL);
        perror("cat error");
        VnodeDrop();
        free(fullpath);
        return -1;
      }
  VnodeDrop();
  (void)memset_s(&init_param, sizeof(init_param), 0, sizeof(TSK_INIT_PARAM_S));
  init_param.pfnTaskEntry = (TSK_ENTRY_FUNC)osShellCmdDoCatShow;
  init_param.usTaskPrio   = CAT_TASK_PRIORITY;	//优先级10
  init_param.auwArgs[0]   = (UINTPTR)fullpath;	//入口参数
  init_param.uwStackSize  = CAT_TASK_STACK_SIZE;//内核栈大小
  init_param.pcName       = "shellcmd_cat";	//任务名称
  init_param.uwResved     = LOS_TASK_STATUS_DETACHED | OS_TASK_FLAG_SPECIFIES_PROCESS;
  init_param.processID    = 2; /* 2: kProcess */ //内核任务

  ret = (int)LOS_TaskCreate(&ca_task, &init_param);//创建任务显示cat内容

    if (ret != LOS_OK) {
        free(fullpath);
    }

  return ret;
}

static int nfs_mount_ref(const char *server_ip_and_path, const char *mount_path,
                         unsigned int uid, unsigned int gid) __attribute__((weakref("nfs_mount")));

static unsigned long get_mountflags(const char *options)
{
    unsigned long mountfalgs = 0;
    char *p;
    while ((options != NULL) && (p = strsep((char**)&options, ",")) != NULL) {
        if (strncmp(p, "ro", strlen("ro")) == 0) {
        mountfalgs |= MS_RDONLY;
        } else if (strncmp(p, "rw", strlen("rw")) == 0) {
            mountfalgs &= ~MS_RDONLY;
        } else if (strncmp(p, "nosuid", strlen("nosuid")) == 0) {
            mountfalgs |= MS_NOSUID;
        } else if (strncmp(p, "suid", strlen("suid")) == 0) {
            mountfalgs &= ~MS_NOSUID;
        } else {
            continue;
        }
    }

    return mountfalgs;
}
static inline void print_mount_usage(void)//mount 用法
{
  PRINTK("mount [DEVICE] [PATH] [NAME]\n");
}

/**
 * @brief
 * @verbatim
  命令功能
  mount命令用来将设备挂载到指定目录。
  命令格式
  mount <device> <path> <name> [uid gid]
  device 要挂载的设备（格式为设备所在路径）。系统拥有的设备。
  path 指定目录。用户必须具有指定目录中的执行（搜索）许可权。N/A
  name 文件系统的种类。 vfat, yaffs, jffs, ramfs, nfs，procfs, romfs.
  uid gid uid是指用户ID。 gid是指组ID。可选参数，缺省值uid:0，gid:0。

  使用指南
  mount后加需要挂载的设备信息、指定目录以及设备文件格式，就能成功挂载文件系统到指定目录。
  使用实例
  举例：mount /dev/mmcblk0p0 /bin/vs/sd vfat
 * @endverbatim 
 * @param argc 
 * @param argv 
 * @return int 
 */
int osShellCmdMount(int argc, const char **argv)
{
    int ret;
    char *fullpath = NULL;
    const char *filename = NULL;
    unsigned int gid, uid;
    char *data = NULL;
    char *filessystemtype = NULL;
    unsigned long mountfalgs;
    char *shell_working_directory = OsShellGetWorkingDirectory();
    if (shell_working_directory == NULL) {
        return -1;
    }

    ERROR_OUT_IF(argc < 3, print_mount_usage(), return OS_FAIL);

    if (strncmp(argv[0], "-t", 2) == 0 || strncmp(argv[0], "-o", 2) == 0) // 2: length of "-t"
    {
        if (argc < 4) { // 4: required number of parameters
            PRINTK("mount -t/-o [DEVICE] [PATH] [NAME]\n");
            return -1;
        }

        filename = argv[2]; // 2: index of file path
        ret = vfs_normalize_path(shell_working_directory, filename, &fullpath);
        ERROR_OUT_IF(ret < 0, set_err(-ret, "mount error"), return -1);

        if (strncmp(argv[3], "nfs", 3) == 0) { // 3: index of fs type
            if (argc <= 6) { // 6: arguments include uid or gid
                uid = ((argc >= 5) && (argv[4] != NULL)) ? (unsigned int)strtoul(argv[4], (char **)NULL, 0) : 0;
                gid = ((argc == 6) && (argv[5] != NULL)) ? (unsigned int)strtoul(argv[5], (char **)NULL, 0) : 0;
   
                if (nfs_mount_ref != NULL) {
                    ret = nfs_mount_ref(argv[1], fullpath, uid, gid);
                    if (ret != LOS_OK) {
                        PRINTK("mount -t [DEVICE] [PATH] [NAME]\n");
                    }
                } else {
                    PRINTK("can't find nfs_mount\n");
                }
                free(fullpath);
                return 0;
            }
        }

        filessystemtype = (argc >= 4) ? (char *)argv[3] : NULL; /* 4: specify fs type, 3: fs type */
        mountfalgs = (argc >= 5) ? get_mountflags((const char *)argv[4]) : 0; /* 4: usr option */
        data = (argc >= 6) ? (char *)argv[5] : NULL; /* 5: usr option data */

        if (strcmp(argv[1], "0") == 0) {
            ret = mount((const char *)NULL, fullpath, filessystemtype, mountfalgs, data);
        } else {
            ret = mount(argv[1], fullpath, filessystemtype, mountfalgs, data); /* 3: fs type */
        }
        if (ret != LOS_OK) {
            perror("mount error");
        } else {
            PRINTK("mount ok\n");
        }
    } else {
        filename = argv[1];
        ret = vfs_normalize_path(shell_working_directory, filename, &fullpath);
        ERROR_OUT_IF(ret < 0, set_err(-ret, "mount error"), return -1);

        if (strncmp(argv[2], "nfs", 3) == 0) { // 2: index of fs type, 3: length of "nfs"
            if (argc <= 5) { // 5: arguments include gid and uid
                uid = ((argc >= 4) && (argv[3] != NULL)) ? (unsigned int)strtoul(argv[3], (char **)NULL, 0) : 0;
                gid = ((argc == 5) && (argv[4] != NULL)) ? (unsigned int)strtoul(argv[4], (char **)NULL, 0) : 0;

                if (nfs_mount_ref != NULL) {
                    ret = nfs_mount_ref(argv[0], fullpath, uid, gid);
                    if (ret != LOS_OK) {
                        PRINTK("mount [DEVICE] [PATH] [NAME]\n");
                    }
                } else {
                    PRINTK("can't find nfs_mount\n");
                }
                free(fullpath);
                return 0;
            }

            print_mount_usage();
            free(fullpath);
            return 0;
        }

        mountfalgs = (argc >= 4) ? get_mountflags((const char *)argv[3]) : 0;  /* 3: usr option */
        data = (argc >= 5) ? (char *)argv[4] : NULL; /* 4: usr option data */

        if (strcmp(argv[0], "0") == 0) {
            ret = mount((const char *)NULL, fullpath, argv[2], mountfalgs, data);
        } else {
            ret = mount(argv[0], fullpath, argv[2], mountfalgs, data);  /* 2: fs type */
        }
        if (ret != LOS_OK) {
            perror("mount error");
        } else {
            PRINTK("mount ok\n");
        }
    }

    free(fullpath);
    return 0;
}

/**
 * @brief
 * @verbatim
  命令功能
  umount命令用来卸载指定文件系统。
  命令格式
  umount [dir]
  参数说明
  dir 需要卸载文件系统对应的目录。 系统已挂载的文件系统的目录。
  使用指南
  umount后加上需要卸载的指定文件系统的目录，即将指定文件系统卸载。
  使用实例
  举例：mount /bin/vs/sd
 * @endverbatim 
 * @param argc 
 * @param argv 
 * @return int 
 */
int osShellCmdUmount(int argc, const char **argv)
{
  int ret;
  const char *filename = NULL;
  char *fullpath = NULL;
  char *target_path = NULL;
  int cmp_num;
  char *work_path = NULL;
  char *shell_working_directory = OsShellGetWorkingDirectory();
    if (shell_working_directory == NULL) {
        return -1;
    }
  work_path = shell_working_directory;

  ERROR_OUT_IF(argc == 0, PRINTK("umount [PATH]\n"), return 0);

  filename = argv[0];
  ret = vfs_normalize_path(shell_working_directory, filename, &fullpath);
  ERROR_OUT_IF(ret < 0, set_err(-ret, "umount error"), return -1);

  target_path = fullpath;
  cmp_num = strlen(fullpath);
    ret = strncmp(work_path, target_path, cmp_num);
    if (ret == 0) {
        work_path += cmp_num;
        if (*work_path == '/' || *work_path == '\0') {
            set_errno(EBUSY);
            perror("umount error");
            free(fullpath);
            return -1;
        }
    }

    ret = umount(fullpath);
    free(fullpath);
    if (ret != LOS_OK) {
        perror("umount error");
        return 0;
    }

  PRINTK("umount ok\n");
  return 0;
}

/**
 * @brief
 * @verbatim
  命令功能
  mkdir命令用来创建一个目录。
  命令格式
  mkdir [directory]
  参数说明
  directory 需要创建的目录。
  使用指南
  mkdir后加所需要创建的目录名会在当前目录下创建目录。
  mkdir后加路径，再加上需要创建的目录名，即在指定目录下创建目录。

  使用实例
  举例：mkdir harmony
 * @endverbatim 
 * @param argc 
 * @param argv 
 * @return int 
 */
int osShellCmdMkdir(int argc, const char **argv)
{
  int ret;
  char *fullpath = NULL;
  const char *filename = NULL;
  char *shell_working_directory = OsShellGetWorkingDirectory();
    if (shell_working_directory == NULL) {
        return -1;
    }

  ERROR_OUT_IF(argc != 1, PRINTK("mkdir [DIRECTORY]\n"), return 0);

  filename = argv[0];
  ret = vfs_normalize_path(shell_working_directory, filename, &fullpath);
  ERROR_OUT_IF(ret < 0, set_err(-ret, "mkdir error"), return -1);

  ret = mkdir(fullpath, S_IRWXU | S_IRWXG | S_IRWXO);
    if (ret == -1) {
        perror("mkdir error");
    }
  free(fullpath);
  return 0;
}

/**
 * @brief
 * @verbatim
  命令功能
  pwd命令用来显示当前路径。
  命令格式
  无
  使用指南
  pwd 命令将当前目录的全路径名称（从根目录）写入标准输出。全部目录使用 / （斜线）分隔。
  第一个 / 表示根目录， 最后一个目录是当前目录。
 * @endverbatim 
 * @param argc 
 * @param argv 
 * @return int 
 */
int osShellCmdPwd(int argc, const char **argv)
{
  char buf[SHOW_MAX_LEN] = {0};
  DIR *dir = NULL;
  char *shell_working_directory = OsShellGetWorkingDirectory();//获取当前工作路径
    if (shell_working_directory == NULL) {
      return -1;
    }

  ERROR_OUT_IF(argc > 0, PRINTK("\nUsage: pwd\n"), return -1);

    dir = opendir(shell_working_directory);
    if (dir == NULL) {
        perror("pwd error");
        return -1;
    }

    LOS_TaskLock();
    if (strncpy_s(buf, SHOW_MAX_LEN, shell_working_directory, SHOW_MAX_LEN - 1) != EOK) {
        LOS_TaskUnlock();
        PRINTK("pwd error: strncpy_s error!\n");
        (void)closedir(dir);
        return -1;
    }
    LOS_TaskUnlock();

  PRINTK("%s\n", buf);
  (void)closedir(dir);
  return 0;
}

static inline void print_statfs_usage(void)
{
  PRINTK("Usage  :\n");
  PRINTK("    statfs <path>\n");
  PRINTK("    path  : Mounted file system path that requires query information\n");
  PRINTK("Example:\n");
  PRINTK("    statfs /ramfs\n");
}

/**
 * @brief
 * @verbatim
  命令功能
  statfs 命令用来打印文件系统的信息，如该文件系统类型、总大小、可用大小等信息。
  命令格式
  statfs [directory]
  参数说明
  directory 文件系统的路径。 必须是存在的文件系统，并且其支持statfs命令，当前支持的文件系统有：JFFS2，FAT，NFS。
  使用指南
  打印信息因文件系统而异。
  以nfs文件系统为例：	statfs /nfs
 * @endverbatim 
 * @param argc 
 * @param argv 
 * @return int 
 */
int osShellCmdStatfs(int argc, const char **argv)
{
  struct statfs sfs;
  int result;
  unsigned long long total_size, free_size;
  char *fullpath = NULL;
  const char *filename = NULL;
  char *shell_working_directory = OsShellGetWorkingDirectory();
    if (shell_working_directory == NULL) {
        return -1;
    }

  ERROR_OUT_IF(argc != 1, PRINTK("statfs failed! Invalid argument!\n"), return -1);

  (void)memset_s(&sfs, sizeof(sfs), 0, sizeof(sfs));

  filename = argv[0];
  result = vfs_normalize_path(shell_working_directory, filename, &fullpath);
  ERROR_OUT_IF(result < 0, set_err(-result, "statfs error"), return -1);

  result = statfs(fullpath, &sfs);//命令实现体
  free(fullpath);

    if (result != 0 || sfs.f_type == 0) {
        PRINTK("statfs failed! Invalid argument!\n");
      print_statfs_usage();
      return -1;
    }

  total_size  = (unsigned long long)sfs.f_bsize * sfs.f_blocks;
  free_size   = (unsigned long long)sfs.f_bsize * sfs.f_bfree;

  PRINTK("statfs got:\n f_type     = %d\n cluster_size   = %d\n", sfs.f_type, sfs.f_bsize);
  PRINTK(" total_clusters = %llu\n free_clusters  = %llu\n", sfs.f_blocks, sfs.f_bfree);
  PRINTK(" avail_clusters = %llu\n f_namelen    = %d\n", sfs.f_bavail, sfs.f_namelen);
  PRINTK("\n%s\n total size: %4llu Bytes\n free  size: %4llu Bytes\n", argv[0], total_size, free_size);

  return 0;
}

/**
 * @brief
 * @verbatim
  touch命令用来在指定的目录下创建一个不存在的空文件。
  touch命令操作已存在的文件会成功，不会更新时间戳。touch [filename]
  touch命令用来创建一个空文件，该文件可读写。
  使用touch命令一次只能创建一个文件。
 * @endverbatim 
 * @param argc 
 * @param argv 
 * @return int 
 */
int osShellCmdTouch(int argc, const char **argv)
{
  int ret;
  int fd = -1;
  char *fullpath = NULL;
  const char *filename = NULL;
  char *shell_working_directory = OsShellGetWorkingDirectory();
    if (shell_working_directory == NULL) {
        return -1;
    }

  ERROR_OUT_IF(argc != 1, PRINTK("touch [FILE]\n"), return -1);

  filename = argv[0];
  ret = vfs_normalize_path(shell_working_directory, filename, &fullpath);
  ERROR_OUT_IF(ret < 0, set_err(-ret, "touch error"), return -1);
  //如果没有就创建文件方式 
  fd = open(fullpath, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
  free(fullpath);
    if (fd == -1) {
        perror("touch error");
      return -1;
    }

  (void)close(fd);
  return 0;
}

#define CP_BUF_SIZE 4096
pthread_mutex_t g_mutex_cp = PTHREAD_MUTEX_INITIALIZER;

/**
 * @brief
 * @verbatim
  cp 拷贝文件，创建一份副本。
  cp [SOURCEFILE] [DESTFILE]
  使用指南
    同一路径下，源文件与目的文件不能重名。
    源文件必须存在，且不为目录。
    源文件路径支持“*”和“？”通配符，“*”代表任意多个字符，“？”代表任意单个字符。目的路径不支持通配符。当源路径可匹配多个文件时，目的路径必须为目录。
    目的路径为目录时，该目录必须存在。此时目的文件以源文件命名。
    目的路径为文件时，所在目录必须存在。此时拷贝文件的同时为副本重命名。
    目前不支持多文件拷贝。参数大于2个时，只对前2个参数进行操作。
    目的文件不存在时创建新文件，已存在则覆盖。
    拷贝系统重要资源时，会对系统造成死机等重大未知影响，如用于拷贝/dev/uartdev-0 文件时，会产生系统卡死现象。
  举例：cp weharmony.txt ./tmp/
 * @endverbatim 
 * @param argc 
 * @param argv 
 * @return int 
 */
static int os_shell_cmd_do_cp(const char *src_filepath, const char *dst_filename)
{
  int  ret;
  char *src_fullpath = NULL;
  char *dst_fullpath = NULL;
  const char *src_filename = NULL;
  char *dst_filepath = NULL;
  char *buf = NULL;
  const char *filename = NULL;
  ssize_t r_size, w_size;
  int src_fd = -1;
  int dst_fd = -1;
  struct stat stat_buf;
  mode_t src_mode;
    char *shell_working_directory = OsShellGetWorkingDirectory();
    if (shell_working_directory == NULL) {
        return -1;
    }

    buf = (char *)malloc(CP_BUF_SIZE);
    if (buf == NULL) {
      PRINTK("cp error: Out of memory!\n");
      return -1;
    }

  /* Get source fullpath. */

    ret = vfs_normalize_path(shell_working_directory, src_filepath, &src_fullpath);
    if (ret < 0) {
        set_errno(-ret);
        PRINTK("cp error: %s\n", strerror(errno));
        free(buf);
        return -1;
    }

    /* Is source path exist? */

    ret = stat(src_fullpath, &stat_buf);
    if (ret == -1) {
        PRINTK("cp %s error: %s\n", src_fullpath, strerror(errno));
        goto errout_with_srcpath;
    }
    src_mode = stat_buf.st_mode;
    /* Is source path a directory? */

    if (S_ISDIR(stat_buf.st_mode)) {
        PRINTK("cp %s error: Source file can't be a directory.\n", src_fullpath);
        goto errout_with_srcpath;
    }

    /* Get dest fullpath. */

    dst_fullpath = strdup(dst_filename);
    if (dst_fullpath == NULL) {
        PRINTK("cp error: Out of memory.\n");
        goto errout_with_srcpath;
    }

    /* Is dest path exist? */

    ret = stat(dst_fullpath, &stat_buf);
    if (ret == 0) {
        /* Is dest path a directory? */

        if (S_ISDIR(stat_buf.st_mode)) {
            /* Get source file name without '/'. */

            src_filename = src_filepath;
            while (1) {
                filename = strchr(src_filename, '/');
                if (filename == NULL) {
                    break;
                }
                src_filename = filename + 1;
            }

            /* Add the source file after dest path. */

            ret = vfs_normalize_path(dst_fullpath, src_filename, &dst_filepath);
            if (ret < 0) {
                set_errno(-ret);
                PRINTK("cp error. %s.\n", strerror(errno));
                goto errout_with_path;
            }
            free(dst_fullpath);
            dst_fullpath = dst_filepath;
        }
    }

    /* Is dest file same as source file? */

    if (strcmp(src_fullpath, dst_fullpath) == 0) {
        PRINTK("cp error: '%s' and '%s' are the same file\n", src_fullpath, dst_fullpath);
        goto errout_with_path;
    }

    /* Copy begins. */

    (void)pthread_mutex_lock(&g_mutex_cp);
    src_fd = open(src_fullpath, O_RDONLY);
    if (src_fd < 0) {
        PRINTK("cp error: can't open %s. %s.\n", src_fullpath, strerror(errno));
        goto errout_with_mutex;
    }

    dst_fd = open(dst_fullpath, O_CREAT | O_WRONLY | O_TRUNC, src_mode);
    if (dst_fd < 0) {
        PRINTK("cp error: can't create %s. %s.\n", dst_fullpath, strerror(errno));
        goto errout_with_srcfd;
    }

    do {
        (void)memset_s(buf, CP_BUF_SIZE, 0, CP_BUF_SIZE);
        r_size = read(src_fd, buf, CP_BUF_SIZE);
        if (r_size < 0) {
            PRINTK("cp %s %s failed. %s.\n", src_fullpath, dst_fullpath, strerror(errno));
            goto errout_with_fd;
        }
        w_size = write(dst_fd, buf, r_size);
        if (w_size != r_size) {
            PRINTK("cp %s %s failed. %s.\n", src_fullpath, dst_fullpath, strerror(errno));
            goto errout_with_fd;
        }
    } while (r_size == CP_BUF_SIZE);

    /* Release resource. */

  free(buf);
  free(src_fullpath);
  free(dst_fullpath);
  (void)close(src_fd);
  (void)close(dst_fd);
  (void)pthread_mutex_unlock(&g_mutex_cp);
  return LOS_OK;

errout_with_fd:
  (void)close(dst_fd);
errout_with_srcfd:
  (void)close(src_fd);
errout_with_mutex:
  (void)pthread_mutex_unlock(&g_mutex_cp);
errout_with_path:
  free(dst_fullpath);
errout_with_srcpath:
  free(src_fullpath);
  free(buf);
  return -1;
}

/* The separator and EOF for a directory fullpath: '/'and '\0' */

#define SEPARATOR_EOF_LEN 2

/**
 * @brief 
 * @verbatim
  rmdir命令用来删除一个目录。
  rmdir [dir]
  dir 需要删除目录的名称，删除目录必须为空，支持输入路径。
  使用指南
    rmdir命令只能用来删除目录。
    rmdir一次只能删除一个目录。
    rmdir只能删除空目录。

  举例：输入rmdir dir
 * @endverbatim
 * @param pathname 
 * @return int 
 */
static int os_shell_cmd_do_rmdir(const char *pathname)
{
  struct dirent *dirent = NULL;
  struct stat stat_info;
  DIR *d = NULL;
  char *fullpath = NULL;
  int ret;

    (void)memset_s(&stat_info, sizeof(stat_info), 0, sizeof(struct stat));
    if (stat(pathname, &stat_info) != 0) {
        return -1;
    }

    if (S_ISREG(stat_info.st_mode) || S_ISLNK(stat_info.st_mode)) {
        return remove(pathname);
    }
    d = opendir(pathname);
    if (d == NULL) {
        return -1;
    }
    while (1) {
        dirent = readdir(d);
        if (dirent == NULL) {
            break;
        }
        if (strcmp(dirent->d_name, "..") && strcmp(dirent->d_name, ".")) {
            size_t fullpath_buf_size = strlen(pathname) + strlen(dirent->d_name) + SEPARATOR_EOF_LEN;
            if (fullpath_buf_size <= 0) {
                PRINTK("buffer size is invalid!\n");
                (void)closedir(d);
                return -1;
            }
            fullpath = (char *)malloc(fullpath_buf_size);
            if (fullpath == NULL) {
                PRINTK("malloc failure!\n");
                (void)closedir(d);
                return -1;
            }
            ret = snprintf_s(fullpath, fullpath_buf_size, fullpath_buf_size - 1, "%s/%s", pathname, dirent->d_name);
            if (ret < 0) {
                PRINTK("name is too long!\n");
                free(fullpath);
                (void)closedir(d);
                return -1;
            }
          (void)os_shell_cmd_do_rmdir(fullpath);
          free(fullpath);
        }
    }
  (void)closedir(d);
  return rmdir(pathname);
}

/*  Wildcard matching operations  */
//确定路径中是否存在通配符
static int os_wildcard_match(const char *src, const char *filename)
{
    int ret;

    if (*src != '\0') {
        if (*filename == '*') {
            while ((*filename == '*') || (*filename == '?')) {
                filename++;
            }

            if (*filename == '\0') {
                return 0;
            }

            while (*src != '\0' && !(*src == *filename)) {
                src++;
            }

            if (*src == '\0') {
                return -1;
            }

            ret = os_wildcard_match(src, filename);

            while ((ret != 0) && (*(++src) != '\0')) {
                if (*src == *filename) {
                    ret = os_wildcard_match(src, filename);
                }
            }
            return ret;
        } else {
            if ((*src == *filename) || (*filename == '?')) {
                return os_wildcard_match(++src, ++filename);
            }
            return -1;
        }
    }

    while (*filename != '\0') {
        if (*filename != '*') {
            return -1;
        }
        filename++;
    }
    return 0;
}

/*   To determine whether a wildcard character exists in a path   */
//文件名是否存在通配符
static int os_is_containers_wildcard(const char *filename)
{
    while (*filename != '\0') {
        if ((*filename == '*') || (*filename == '?')) {
            return 1;
        }
        filename++;
    }
    return 0;
}

/*  Delete a matching file or directory  */
//删除匹配的文件或目录
static int os_wildcard_delete_file_or_dir(const char *fullpath, wildcard_type mark)
{
  int ret;

    switch (mark) {
      case RM_RECURSIVER://递归删除
        ret = os_shell_cmd_do_rmdir(fullpath);//删除目录，其中递归调用 rmdir
        break;
      case RM_FILE://删除文件
        ret = unlink(fullpath);
        break;
      case RM_DIR://删除目录
        ret = rmdir(fullpath);
        break;
      default:
        return VFS_ERROR;
    }
    if (ret == -1) {
      PRINTK("%s  ", fullpath);
      perror("rm/rmdir error!");
      return ret;
    }

  PRINTK("%s match successful!delete!\n", fullpath);
  return 0;
}

/*  Split the path with wildcard characters  */
//使用通配符拆分路径
static char* os_wildcard_split_path(char *fullpath, char **handle, char **wait)
{
  int n = 0;
  int a = 0;
  int b = 0;
  int len  = strlen(fullpath);

    for (n = 0; n < len; n++) {
        if (fullpath[n] == '/') {
            if (b != 0) {
                fullpath[n] = '\0';
                *wait = fullpath + n + 1;
                break;
            }
            a = n;
        } else if (fullpath[n] == '*' || fullpath[n] == '?') {
            b = n;
            fullpath[a] = '\0';
            if (a == 0) {
                *handle = fullpath + a + 1;
                continue;
            }
            *handle = fullpath + a + 1;
        }
    }
    return fullpath;
}

/*  Handling entry of the path with wildcard characters  */
//处理具有通配符的目录
static int os_wildcard_extract_directory(char *fullpath, void *dst, wildcard_type mark)
{
  char separator[] = "/";
  char src[PATH_MAX] = {0};
  struct dirent *dirent = NULL;
  char *f = NULL;
  char *s = NULL;
  char *t = NULL;
  int ret = 0;
  DIR *d = NULL;
  struct stat stat_buf;
  int deleteFlag = 0;

    f = os_wildcard_split_path(fullpath, &s, &t);

    if (s == NULL) {
        if (mark == CP_FILE) {
            ret = os_shell_cmd_do_cp(fullpath, dst);
        } else if (mark == CP_COUNT) {
            ret = stat(fullpath, &stat_buf);
            if (ret == 0 && (S_ISREG(stat_buf.st_mode) || S_ISLNK(stat_buf.st_mode))) {
                (*(int *)dst)++;
            }
        } else {
            ret = os_wildcard_delete_file_or_dir(fullpath, mark);
        }
        return ret;
    }

    d = (*f == '\0') ? opendir("/") : opendir(f);
    if (d == NULL) {
        perror("opendir error");
        return VFS_ERROR;
    }

    while (1) {
        dirent = readdir(d);
        if (dirent == NULL) {
            break;
        }

        ret = strcpy_s(src, PATH_MAX, f);
        if (ret != EOK) {
            goto closedir_out;
        }

        ret = os_wildcard_match(dirent->d_name, s);
        if (ret == 0) {
            ret = strcat_s(src, sizeof(src), separator);
            if (ret != EOK) {
                goto closedir_out;
            }
            ret = strcat_s(src, sizeof(src), dirent->d_name);
            if (ret != EOK) {
                goto closedir_out;
            }
            if (t == NULL) {
                if (mark == CP_FILE) {
                    ret = os_shell_cmd_do_cp(src, dst);
                } else if (mark == CP_COUNT) {
                    ret = stat(src, &stat_buf);
                    if (ret == 0 && (S_ISREG(stat_buf.st_mode) || S_ISLNK(stat_buf.st_mode))) {
                        (*(int *)dst)++;
                        if ((*(int *)dst) > 1) {
                            break;
                        }
                    }
                } else {
                    ret = os_wildcard_delete_file_or_dir(src, mark);
                    if (ret == 0) {
                        deleteFlag = 1;
                    }
                }
            } else {
                ret = strcat_s(src, sizeof(src), separator);
                if (ret != EOK) {
                    goto closedir_out;
                }
                ret = strcat_s(src, sizeof(src), t);
                if (ret != EOK) {
                    goto closedir_out;
                }
                ret = os_wildcard_extract_directory(src, dst, mark);
                if (mark == CP_COUNT && (*(int *)dst) > 1) {
                    break;
                }
            }
        }
    }
    (void)closedir(d);
    if (deleteFlag == 1) {
        ret = 0;
    }
  return ret;
closedir_out:
  (void)closedir(d);
  return VFS_ERROR;
}

/**
 * @brief
 * @verbatim
  命令功能
  拷贝文件，创建一份副本。

  命令格式
  cp [SOURCEFILE] [DESTFILE]

  SOURCEFILE - 源文件路径。- 目前只支持文件,不支持目录。
  DESTFILE - 目的文件路径。 - 支持目录以及文件。

  使用指南
  同一路径下，源文件与目的文件不能重名。
  源文件必须存在，且不为目录。
  源文件路径支持“*”和“？”通配符，“*”代表任意多个字符，“？”代表任意单个字符。目的路径不支持通配符。当源路径可匹配多个文件时，目的路径必须为目录。
  目的路径为目录时，该目录必须存在。此时目的文件以源文件命名。
  目的路径为文件时，所在目录必须存在。此时拷贝文件的同时为副本重命名。
  目前不支持多文件拷贝。参数大于2个时，只对前2个参数进行操作。
  目的文件不存在时创建新文件，已存在则覆盖。
  拷贝系统重要资源时，会对系统造成死机等重大未知影响，如用于拷贝/dev/uartdev-0 文件时，会产生系统卡死现象。

  使用实例
  举例：cp harmony.txt ./tmp/
 * @endverbatim 
 * @param argc 
 * @param argv 
 * @return int 
 */
int osShellCmdCp(int argc, const char **argv)
{
  int  ret;
  const char *src = NULL;
  const char *dst = NULL;
  char *src_fullpath = NULL;
  char *dst_fullpath = NULL;
  struct stat stat_buf;
  int count = 0;
  char *shell_working_directory = OsShellGetWorkingDirectory();//获取进程当前工作目录
    if (shell_working_directory == NULL) {
        return -1;
    }

  ERROR_OUT_IF(argc < 2, PRINTK("cp [SOURCEFILE] [DESTFILE]\n"), return -1);//参数必须要两个

  src = argv[0];
  dst = argv[1];

  /* Get source fullpath. */

  ret = vfs_normalize_path(shell_working_directory, src, &src_fullpath);//获取源路径的全路径
    if (ret < 0) {
      set_errno(-ret);
      PRINTK("cp error:%s\n", strerror(errno));
      return -1;
    }

  if (src[strlen(src) - 1] == '/') { //如果src最后一个字符是 '/'说明是个目录,src必须是个文件，目前只支持文件，不支持目录
      PRINTK("cp %s error: Source file can't be a directory.\n", src);
      goto errout_with_srcpath;
    }

  /* Get dest fullpath. */

  ret = vfs_normalize_path(shell_working_directory, dst, &dst_fullpath);
    if (ret < 0) {
      set_errno(-ret);
      PRINTK("cp error: can't open %s. %s\n", dst, strerror(errno));
      goto errout_with_srcpath;
    }

  /* Is dest path exist? *///目标路径是个目录吗？

    ret = stat(dst_fullpath, &stat_buf);
    if (ret < 0) {
        /* Is dest path a directory? */

        if (dst[strlen(dst) - 1] == '/') {
            PRINTK("cp error: %s, %s.\n", dst_fullpath, strerror(errno));
            goto errout_with_path;
        }
    } else {
        if ((S_ISREG(stat_buf.st_mode) || S_ISLNK(stat_buf.st_mode)) && dst[strlen(dst) - 1] == '/') {
            PRINTK("cp error: %s is not a directory.\n", dst_fullpath);
            goto errout_with_path;
        }
    }

    if (os_is_containers_wildcard(src_fullpath)) {
        if (ret < 0 || S_ISREG(stat_buf.st_mode) || S_ISLNK(stat_buf.st_mode)) {
            char *src_copy = strdup(src_fullpath);
            if (src_copy == NULL) {
                PRINTK("cp error : Out of memory.\n");
                goto errout_with_path;
            }
            (void)os_wildcard_extract_directory(src_copy, &count, CP_COUNT);
            free(src_copy);
            if (count > 1) {
                PRINTK("cp error : %s is not a directory.\n", dst_fullpath);
                goto errout_with_path;
            }
        }
        ret = os_wildcard_extract_directory(src_fullpath, dst_fullpath, CP_FILE);
    } else {
      ret = os_shell_cmd_do_cp(src_fullpath, dst_fullpath);
    }
  free(dst_fullpath);
  free(src_fullpath);
  return ret;

errout_with_path:
  free(dst_fullpath);
errout_with_srcpath:
  free(src_fullpath);
  return VFS_ERROR;
}

static inline void print_rm_usage(void)
{
  PRINTK("rm [FILE] or rm [-r/-R] [FILE]\n");
}

/**
 * @brief 
 * @verbatim rm命令用来删除文件或文件夹。rm [-r] [dirname / filename]
  rm命令一次只能删除一个文件或文件夹。
  rm -r命令可以删除非空目录。
  rm log1.txt ; rm -r sd 
   @endverbatim
 * @param argc 
 * @param argv 
 * @return int 
 */
int osShellCmdRm(int argc, const char **argv)
{
  int  ret = 0;
  char *fullpath = NULL;
  const char *filename = NULL;
  char *shell_working_directory = OsShellGetWorkingDirectory();
    if (shell_working_directory == NULL) {
        return -1;
    }

    ERROR_OUT_IF(argc != 1 && argc != 2, print_rm_usage(), return -1);

    if (argc == 2) { // 2: arguments include "-r" or "-R"
        ERROR_OUT_IF(strcmp(argv[0], "-r") != 0 && strcmp(argv[0], "-R") != 0, print_rm_usage(), return -1);

        filename = argv[1];
        ret = vfs_normalize_path(shell_working_directory, filename, &fullpath);
        ERROR_OUT_IF(ret < 0, set_err(-ret, "rm error"), return -1);

        if (os_is_containers_wildcard(fullpath)) {
            ret = os_wildcard_extract_directory(fullpath, NULL, RM_RECURSIVER);
        } else {
            ret = os_shell_cmd_do_rmdir(fullpath);
        }
    } else {
        filename = argv[0];
        ret = vfs_normalize_path(shell_working_directory, filename, &fullpath);
        ERROR_OUT_IF(ret < 0, set_err(-ret, "rm error"), return -1);

        if (os_is_containers_wildcard(fullpath)) {
            ret = os_wildcard_extract_directory(fullpath, NULL, RM_FILE);
        } else {
            ret = unlink(fullpath);
        }
    }
    if (ret == -1) {
        perror("rm error");
    }
    free(fullpath);
    return 0;
}

/**
 * @brief
 * @verbatim
  命令功能
  rmdir命令用来删除一个目录。

  命令格式
  rmdir [dir]

  参数说明
  参数		参数说明		取值范围
  dir		需要删除目录的名称，删除目录必须为空，支持输入路径。N/A

  使用指南
  rmdir命令只能用来删除目录。
  rmdir一次只能删除一个目录。
  rmdir只能删除空目录。
  使用实例
  举例：输入rmdir dir
 * @endverbatim 
 * @param argc 
 * @param argv 
 * @return int 
 */
int osShellCmdRmdir(int argc, const char **argv)
{
  int  ret;
  char *fullpath = NULL;
  const char *filename = NULL;
    char *shell_working_directory = OsShellGetWorkingDirectory();
    if (shell_working_directory == NULL) {
        return -1;
    }

    ERROR_OUT_IF(argc == 0, PRINTK("rmdir [DIRECTORY]\n"), return -1);

    filename = argv[0];
    ret = vfs_normalize_path(shell_working_directory, filename, &fullpath);
    ERROR_OUT_IF(ret < 0, set_err(-ret, "rmdir error"), return -1);

    if (os_is_containers_wildcard(fullpath)) {
        ret = os_wildcard_extract_directory(fullpath, NULL, RM_DIR);
    } else {
        ret = rmdir(fullpath);
    }
    if (ret == -1) {
        PRINTK("rmdir %s failed. Error: %s.\n", fullpath, strerror(errno));
    }
    free(fullpath);

  return 0;
}

/**
 * @brief
 * @verbatim
  命令功能
  sync命令用于同步缓存数据（文件系统的数据）到sd卡。

  命令格式
  sync

  参数说明
  无。

  使用指南
  sync命令用来刷新缓存，当没有sd卡插入时不进行操作。
  有sd卡插入时缓存信息会同步到sd卡，成功返回时无显示。
  使用实例
  举例：输入sync，有sd卡时同步到sd卡，无sd卡时不操作。
 * @endverbatim 
 * @param argc 
 * @param argv 
 * @return int 
 */
int osShellCmdSync(int argc, const char **argv)
{
  ERROR_OUT_IF(argc > 0, PRINTK("\nUsage: sync\n"), return -1);

  sync();
  return 0;
}

/**
 * @brief
 * @verbatim
  命令功能
  lsfd命令用来显示当前已经打开的系统文件描述符及对应的文件名。

  命令格式
  lsfd

  使用指南
  lsfd命令显示当前已经打开文件的fd号以及文件的名字。

  使用实例
  举例：输入lsfd,注意这里并不显示 (0 ~ 2)号
  OHOS #lsfd
    fd	filename
    3	/dev/console1
    4	/dev/spidev1.0
    5	/bin/init
    6	/bin/shell
 * @endverbatim 
 * @param argc 
 * @param argv 
 * @return int 
 */
int osShellCmdLsfd(int argc, const char **argv)
{
  ERROR_OUT_IF(argc > 0, PRINTK("\nUsage: lsfd\n"), return -1);

  lsfd();

  return 0;
}

int checkNum(const char *arg)
{
  int i = 0;
    if (arg == NULL) {
        return -1;
    }
    if (arg[0] == '-') {
        /* exclude the '-' */

        i = 1;
    }
    for (; arg[i] != 0; i++) {
        if (!isdigit(arg[i])) {
            return -1;
        }
    }
    return 0;
}

#ifdef LOSCFG_KERNEL_SYSCALL

/**
 * @brief
 * @verbatim
  shell su 用于变更为其他使用者的身份
  su [uid] [gid]
  su命令缺省切换到root用户，uid默认为0，gid为0。
  在su命令后的输入参数uid和gid就可以切换到该uid和gid的用户。
  输入参数超出范围时，会打印提醒输入正确范围参数。
 * @endverbatim 
 * @param argc 
 * @param argv 
 * @return int 
 */
int osShellCmdSu(int argc, const char **argv)
{
  int su_uid;
  int su_gid;

  if (argc == 0) { //无参时切换到root用户
      /* for su root */

      su_uid = 0;
      su_gid = 0;
    } else {
      ERROR_OUT_IF((argc != 2), PRINTK("su [uid_num] [gid_num]\n"), return -1);
      ERROR_OUT_IF((checkNum(argv[0]) != 0) || (checkNum(argv[1]) != 0), /* check argv is digit */
      PRINTK("check uid_num and gid_num is digit\n"), return -1);

      su_uid = atoi(argv[0]);//标准musl C库函数 字符串转数字
      su_gid = atoi(argv[1]);

      ERROR_OUT_IF((su_uid < 0) || (su_uid > 60000) || (su_gid < 0) ||
         (su_gid > 60000), PRINTK("uid_num or gid_num out of range!they should be [0~60000]\n"), return -1);
    }

  SysSetUserID(su_uid);
  SysSetGroupID(su_gid);
  return 0;
}
#endif

/**
 * @brief 
 * @verbatim
  shell chmod 用于修改文件操作权限。chmod [mode] [pathname]
  mode 文件或文件夹权限，用8进制表示对应User、Group、及Other（拥有者、群组、其他组）的权限。[0,777]
  pathname 文件路径。已存在的文件。
  chmod 777 weharmony.txt 暂不支持 chmod ugo+r file1.txt这种写法
 * @endverbatim
 * @param pathname 
 * @return int 
 */
int osShellCmdChmod(int argc, const char **argv)
{
  int i = 0;
  int mode = 0;
  int ret;
  char *fullpath = NULL;
  const char *filename = NULL;
  struct IATTR attr = {0};//IATTR是用来inode的属性 见于 ..\third_party\NuttX\include\nuttx\fs\fs.h
  char *shell_working_directory = NULL;
  const char *p = NULL;
#define MODE_BIT 3 /* 3 bits express 1 mode */

  ERROR_OUT_IF((argc != 2), PRINTK("Usage: chmod <MODE> [FILE]\n"), return -1);

  p = argv[0];
    while (p[i]) {
      if ((p[i] <= '7') && (p[i] >= '0')) { // 参数只能在 000 - 777 之间
          mode = ((uint)mode << MODE_BIT) | (uint)(p[i] - '0');
        } else {
          PRINTK("check the input <MODE>\n");//例如: chmod 807 是错误的
          return -1;
        }
      i++;
    }
  filename = argv[1];

  shell_working_directory = OsShellGetWorkingDirectory();
    if (shell_working_directory == NULL) {
        return -1;
    }
  ret = vfs_normalize_path(shell_working_directory, filename, &fullpath);
  ERROR_OUT_IF(ret < 0, set_err(-ret, "chmod error\n"), return -1);

  attr.attr_chg_mode = mode;
  attr.attr_chg_valid = CHG_MODE; /* change mode */
  ret = chattr(fullpath, &attr);
    if (ret < 0) {
      free(fullpath);
      PRINTK("chmod error! %s\n", strerror(errno));
      return ret;
    }

  free(fullpath);
  return 0;
}
/****************************************************************
shell chown 用于将指定文件的拥有者改为指定的用户或组。chown [owner] [group] [pathname]
owner 	文件拥有者。[0,0xFFFFFFFF]
group	文件群组。1、为空。2、[0,0xFFFFFFFF]
pathname	文件路径。已存在的文件。
在需要修改的文件名前加上文件拥有者和文件群组就可以分别修改该文件的拥有者和群组。
当owner或group值为-1时则表示对应的owner或group不修改。
group参数可以为空。
举例：chown 100 200 weharmony.txt
****************************************************************/
int osShellCmdChown(int argc, const char **argv)
{
  int ret;
  char *fullpath = NULL;
  const char *filename = NULL;
  struct IATTR attr;
  uid_t owner = -1;
  gid_t group = -1;
  attr.attr_chg_valid = 0;

  ERROR_OUT_IF(((argc != 2) && (argc != 3)), PRINTK("Usage: chown [OWNER] [GROUP] FILE\n"), return -1);
  if (argc == 2) { //只有二个参数解析 chown 100 weharmony.txt
      ERROR_OUT_IF((checkNum(argv[0]) != 0), PRINTK("check OWNER is digit\n"), return -1);
      owner = atoi(argv[0]);
      filename = argv[1];
    }
  if (argc == 3) { //有三个参数解析 chown 100 200 weharmony.txt
      ERROR_OUT_IF((checkNum(argv[0]) != 0), PRINTK("check OWNER is digit\n"), return -1);
      ERROR_OUT_IF((checkNum(argv[1]) != 0), PRINTK("check GROUP is digit\n"), return -1);
      owner = atoi(argv[0]);//第一个参数用于修改拥有者指定用户
      group = atoi(argv[1]);//第二个参数用于修改拥有者指定组
      filename = argv[2];
    }

  if (group != -1) { //-1代表不需要处理
      attr.attr_chg_gid = group;
      attr.attr_chg_valid |= CHG_GID;//贴上拥有组被修改过的标签
    }
    if (owner != -1) {
      attr.attr_chg_uid = owner;
      attr.attr_chg_valid |= CHG_UID;//贴上拥有者被修改过的标签
    }

  char *shell_working_directory = OsShellGetWorkingDirectory();
    if (shell_working_directory == NULL) {
      return -1;
    }
  ret = vfs_normalize_path(shell_working_directory, filename, &fullpath);
  ERROR_OUT_IF(ret < 0, set_err(-ret, "chown error\n"), return -1);

  ret = chattr(fullpath, &attr);//修改属性,在chattr中,参数attr将会和fullpath原有attr->attr_chg_valid 按|运算
    if (ret < 0) {
      free(fullpath);
      PRINTK("chown error! %s\n", strerror(errno));
      return ret;
   }

  free(fullpath);
  return 0;
}

/**
 * @brief 
 * @verbatim
  chgrp用于修改文件的群组。chgrp [group] [pathname]
  group 文件群组。[0,0xFFFFFFFF]
  pathname	文件路径。 已存在的文件。
  在需要修改的文件名前加上文件群组值就可以修改该文件的所属组。
  举例：chgrp 100 weharmony.txt
 * @endverbatim
 * @param pathname 
 * @return int 
 */
int osShellCmdChgrp(int argc, const char **argv)
{
  int ret;
  char *fullpath = NULL;
  const char *filename = NULL;
  struct IATTR attr;
  gid_t group;
  attr.attr_chg_valid = 0;
  ERROR_OUT_IF((argc != 2), PRINTK("Usage: chgrp GROUP FILE\n"), return -1);
  ERROR_OUT_IF((checkNum(argv[0]) != 0), PRINTK("check GROUP is digit\n"), return -1);
  group = atoi(argv[0]);
  filename = argv[1];

  if (group != -1) {//可以看出 chgrp 是 chown 命令的裁剪版
    attr.attr_chg_gid = group;
    attr.attr_chg_valid |= CHG_GID;
  }

  char *shell_working_directory = OsShellGetWorkingDirectory();
  if (shell_working_directory == NULL) {
    return -1;
  }
  ret = vfs_normalize_path(shell_working_directory, filename, &fullpath);
  ERROR_OUT_IF(ret < 0, set_err(-ret, "chmod error"), return -1);

  ret = chattr(fullpath, &attr);//修改属性
  if (ret < 0) {
    free(fullpath);
    PRINTK("chgrp error! %s\n", strerror(errno));
    return ret;
  }

  free(fullpath);
  return 0;
}

#ifdef LOSCFG_SHELL_CMD_DEBUG
SHELLCMD_ENTRY(lsfd_shellcmd, CMD_TYPE_EX, "lsfd", XARGS, (CmdCallBackFunc)osShellCmdLsfd);
SHELLCMD_ENTRY(statfs_shellcmd, CMD_TYPE_EX, "statfs", XARGS, (CmdCallBackFunc)osShellCmdStatfs);
SHELLCMD_ENTRY(touch_shellcmd, CMD_TYPE_EX, "touch", XARGS, (CmdCallBackFunc)osShellCmdTouch);
#ifdef LOSCFG_KERNEL_SYSCALL
SHELLCMD_ENTRY(su_shellcmd, CMD_TYPE_EX, "su", XARGS, (CmdCallBackFunc)osShellCmdSu);
#endif
#endif
SHELLCMD_ENTRY(sync_shellcmd, CMD_TYPE_EX, "sync", XARGS, (CmdCallBackFunc)osShellCmdSync);
SHELLCMD_ENTRY(ls_shellcmd, CMD_TYPE_EX, "ls", XARGS, (CmdCallBackFunc)osShellCmdLs);
SHELLCMD_ENTRY(pwd_shellcmd, CMD_TYPE_EX, "pwd", XARGS, (CmdCallBackFunc)osShellCmdPwd);
SHELLCMD_ENTRY(cd_shellcmd, CMD_TYPE_EX, "cd", XARGS, (CmdCallBackFunc)osShellCmdCd);
SHELLCMD_ENTRY(cat_shellcmd, CMD_TYPE_EX, "cat", XARGS, (CmdCallBackFunc)osShellCmdCat);
SHELLCMD_ENTRY(rm_shellcmd, CMD_TYPE_EX, "rm", XARGS, (CmdCallBackFunc)osShellCmdRm);
SHELLCMD_ENTRY(rmdir_shellcmd, CMD_TYPE_EX, "rmdir", XARGS, (CmdCallBackFunc)osShellCmdRmdir);
SHELLCMD_ENTRY(mkdir_shellcmd, CMD_TYPE_EX, "mkdir", XARGS, (CmdCallBackFunc)osShellCmdMkdir);
SHELLCMD_ENTRY(chmod_shellcmd, CMD_TYPE_EX, "chmod", XARGS, (CmdCallBackFunc)osShellCmdChmod);
SHELLCMD_ENTRY(chown_shellcmd, CMD_TYPE_EX, "chown", XARGS, (CmdCallBackFunc)osShellCmdChown);
SHELLCMD_ENTRY(chgrp_shellcmd, CMD_TYPE_EX, "chgrp", XARGS, (CmdCallBackFunc)osShellCmdChgrp);
SHELLCMD_ENTRY(mount_shellcmd, CMD_TYPE_EX, "mount", XARGS, (CmdCallBackFunc)osShellCmdMount);//#mount 命令静态注册方式
SHELLCMD_ENTRY(umount_shellcmd, CMD_TYPE_EX, "umount", XARGS, (CmdCallBackFunc)osShellCmdUmount);
SHELLCMD_ENTRY(cp_shellcmd, CMD_TYPE_EX, "cp", XARGS, (CmdCallBackFunc)osShellCmdCp);
#endif
