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

#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#ifdef LOSCFG_FS_VFS
#include <fs/fs.h>
/**
 * @brief   移动文件读写指针
 * @param   fd      文件描述符
 * @param   offset  偏移量
 * @param   whence  偏移起始位置（SEEK_SET/SEEK_CUR/SEEK_END）
 * @return  成功时返回新的文件偏移量，失败时返回-errno
 */
off_t _lseek(int fd, off_t offset, int whence)
{
    int ret;                                 // 函数返回值
    struct file *filep = NULL;               // 文件结构体指针

    /* 获取文件描述符对应的文件结构体 */
    ret = fs_getfilep(fd, &filep);
    if (ret < 0) {
        /* errno值已在fs_getfilep中设置 */
        return (off_t)-get_errno();
    }

    /* libc的seekdir函数应将whence设为SEEK_SET，因此此处忽略whence参数 */
    if (filep->f_oflags & O_DIRECTORY) {     // 判断是否为目录文件
        /* 防御性编程：检查目录指针是否为空 */
        if (filep->f_dir == NULL) {
            return (off_t)-EINVAL;           // 目录指针为空，返回无效参数错误
        }
        if (offset == 0) {
            rewinddir(filep->f_dir);         // 偏移量为0时，重置目录流
        } else {
            seekdir(filep->f_dir, offset);   // 设置目录流的当前位置
        }
        ret = telldir(filep->f_dir);         // 获取目录流的当前位置
        if (ret < 0) {
            return (off_t)-get_errno();      // 获取位置失败，返回错误码
        }
        return ret;                          // 返回目录流当前位置
    }

    /* 调用file_seek执行实际的文件偏移操作 */
    ret = file_seek(filep, offset, whence);
    if (ret < 0) {
        return -get_errno();                 // 偏移失败，返回错误码
    }
    return ret;                              // 返回新的文件偏移量
}

/**
 * @brief   64位版本的移动文件读写指针函数
 * @param   fd          文件描述符
 * @param   offsetHigh  偏移量的高32位
 * @param   offsetLow   偏移量的低32位
 * @param   result      用于存储结果的指针
 * @param   whence      偏移起始位置（SEEK_SET/SEEK_CUR/SEEK_END）
 * @return  成功时返回0并将新偏移量存入result，失败时返回-errno
 */
off64_t _lseek64(int fd, int offsetHigh, int offsetLow, off64_t *result, int whence)
{
    off64_t ret;                             // 函数返回值
    struct file *filep = NULL;               // 文件结构体指针
    off64_t offset = ((off64_t)offsetHigh << 32) + (uint)offsetLow; /* 32: offsetHigh是高32位 */

    /* 获取文件描述符对应的文件结构体 */
    ret = fs_getfilep(fd, &filep);
    if (ret < 0) {
        /* errno值已在fs_getfilep中设置 */
        return (off64_t)-get_errno();
    }

    /* libc的seekdir函数应将whence设为SEEK_SET，因此此处忽略whence参数 */
    if (filep->f_oflags & O_DIRECTORY) {     // 判断是否为目录文件
        /* 防御性编程：检查目录指针是否为空 */
        if (filep->f_dir == NULL) {
            return (off64_t)-EINVAL;         // 目录指针为空，返回无效参数错误
        }
        if (offsetLow == 0) {
            rewinddir(filep->f_dir);         // 低32位偏移量为0时，重置目录流
        } else {
            seekdir(filep->f_dir, offsetLow); // 使用低32位偏移量设置目录流位置
        }
        ret = telldir(filep->f_dir);         // 获取目录流的当前位置
        if (ret < 0) {
            return (off64_t)-get_errno();    // 获取位置失败，返回错误码
        }
        goto out;                            // 跳转到结果处理
    }

    /* 调用file_seek64执行实际的64位文件偏移操作 */
    ret = file_seek64(filep, offset, whence);
    if (ret < 0) {
        return (off64_t)-get_errno();        // 偏移失败，返回错误码
    }

out:
    *result = ret;                           // 将结果存入输出参数

    return 0;                                // 成功返回0
}
#endif
