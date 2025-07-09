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

#include "virpart.h"
#include "errno.h"
#include "fatfs.h"
#include "errcode_fat.h"
#include "disk.h"
#include "fs/mount.h"

#ifdef LOSCFG_FS_FAT_CACHE
#include "bcache.h"
#endif

#include "virpartff.h"

#ifdef LOSCFG_FS_FAT_VIRTUAL_PARTITION
// 存储设备分区名称，长度为DISK_NAME+1，初始化为全0
char g_devPartName[DISK_NAME + 1] = {0};

/**
 * @brief 检查虚拟分区百分比参数是否有效
 * @param virtualinfo 指向虚拟分区信息结构体的指针
 * @return 0表示成功，-1表示失败
 * @note 每个分区百分比需在(0,_FLOAT_ACC)范围内，且总和需小于1
 */
static INT FatFsCheckPercent(virpartinfo *virtualinfo)
{
    double virtPercent = 0.0;  // 所有虚拟分区百分比总和
    INT partCount;             // 分区计数器

    // 遍历所有虚拟分区，检查单个分区百分比是否有效
    for (partCount = 0; partCount < virtualinfo->virpartnum; partCount++) {
        // 检查分区百分比是否在有效范围内(大于_FLOAT_ACC且小于1-_FLOAT_ACC)
        if (virtualinfo->virpartpercent[partCount] <= _FLOAT_ACC ||
            virtualinfo->virpartpercent[partCount] >= (1 - _FLOAT_ACC)) {
            set_errno(VIRERR_PARMPERCENTERR);  // 设置百分比错误码
            g_fatVirPart.isparamset = FALSE;   // 标记参数未设置
            return -1;                         // 返回错误
        }
        virtPercent += virtualinfo->virpartpercent[partCount];  // 累加分区百分比
    }

    // 检查分区百分比总和是否超过1(允许微小浮点误差)
    if (virtPercent >= (1 + _FLOAT_ACC)) {
        set_errno(VIRERR_PARMPERCENTERR);  // 设置百分比错误码
        g_fatVirPart.isparamset = FALSE;   // 标记参数未设置
        return -1;                         // 返回错误
    }

    return 0;  // 所有百分比检查通过
}

/**
 * @brief 检查虚拟分区名称参数是否有效
 * @param virtualinfo 指向虚拟分区信息结构体的指针
 * @return 0表示成功，-1表示失败
 * @note 名称需符合FAT文件系统命名规范且不重复
 */
static INT FatFsCheckName(virpartinfo *virtualinfo)
{
    INT partCount, loopCount, len;  // 分区计数器、循环计数器、名称长度
    FRESULT res;                    // FATFS函数返回值

    // 遍历所有虚拟分区，检查单个分区名称是否有效
    for (partCount = 0; partCount < virtualinfo->virpartnum; partCount++) {
        // 获取分区名称长度，最大不超过_MAX_ENTRYLENGTH
        len = ff_strnlen(virtualinfo->virpartname[partCount], _MAX_ENTRYLENGTH + 1);
        // 检查名称长度是否在有效范围内(大于0且小于等于_MAX_ENTRYLENGTH)
        if (len <= 0 || len > _MAX_ENTRYLENGTH) {
            set_errno(VIRERR_PARMNAMEERR);   // 设置名称错误码
            g_fatVirPart.isparamset = FALSE; // 标记参数未设置
            return -1;                       // 返回错误
        }

        // 检查名称是否符合FAT文件系统命名规范
        res = f_checkname(virtualinfo->virpartname[partCount]);
        if (res != FR_OK) {
            set_errno(VIRERR_PARMNAMEERR);   // 设置名称错误码
            g_fatVirPart.isparamset = FALSE; // 标记参数未设置
            return -1;                       // 返回错误
        }
    }

    // 检查所有分区名称是否存在重复
    for (partCount = 0; partCount < virtualinfo->virpartnum; partCount++) {
        for (loopCount = partCount + 1; loopCount < virtualinfo->virpartnum; loopCount++) {
            // 比较两个分区名称是否相同
            if ((memcmp(virtualinfo->virpartname[partCount], virtualinfo->virpartname[loopCount],
                        _MAX_ENTRYLENGTH)) == 0) {
                set_errno(VIRERR_PARMNAMEERR);   // 设置名称错误码
                g_fatVirPart.isparamset = FALSE; // 标记参数未设置
                return -1;                       // 返回错误
            }
        }
    }

    return 0;  // 所有名称检查通过
}

/**
 * @brief 设置虚拟分区参数
 * @param virtualinfo 虚拟分区信息结构体
 * @return 0表示成功，-1表示失败
 * @note 参数设置后会被锁定，需先解锁才能重新设置
 */
INT los_set_virpartparam(virpartinfo virtualinfo)
{
    INT ret;                  // 函数返回值
    UINT minSize;             // 最小长度
    UINT dstNameSize;         // 目标名称缓冲区大小
    char devHead[DISK_NAME + 1] = "/dev/";  // 设备路径前缀

    g_fatVirPart.virtualinfo.devpartpath = g_devPartName;  // 设置设备路径指针

    // 检查参数是否已被锁定
    if (g_fatVirPart.isparamset == TRUE) {
        set_errno(VIRERR_PARMLOCKED);  // 设置参数锁定错误码
        return -1;                     // 返回错误
    }

    g_fatVirPart.isparamset = TRUE;  // 锁定参数设置

    // 检查分区数量是否在有效范围内(1~_MAX_VIRVOLUMES)
    if (virtualinfo.virpartnum < 1 || virtualinfo.virpartnum > _MAX_VIRVOLUMES) {
        set_errno(VIRERR_PARMNUMERR);   // 设置分区数量错误码
        g_fatVirPart.isparamset = FALSE; // 解锁参数设置
        return -1;                       // 返回错误
    }

    ret = FatFsCheckPercent(&virtualinfo);  // 检查百分比参数
    if (ret != 0) {
        return ret;  // 返回百分比检查结果
    }

    ret = FatFsCheckName(&virtualinfo);  // 检查名称参数
    if (ret != 0) {
        return ret;  // 返回名称检查结果
    }

    // 检查设备路径长度是否合法(需长于/dev/前缀)
    if (strlen(virtualinfo.devpartpath) <= strlen(devHead)) {
        set_errno(VIRERR_PARMDEVERR);   // 设置设备路径错误码
        g_fatVirPart.isparamset = FALSE; // 解锁参数设置
        return -1;                       // 返回错误
    }

    // 检查设备路径是否以/dev/为前缀
    if (memcmp(virtualinfo.devpartpath, devHead, strlen(devHead)) != 0) {
        set_errno(VIRERR_PARMDEVERR);   // 设置设备路径错误码
        g_fatVirPart.isparamset = FALSE; // 解锁参数设置
        return -1;                       // 返回错误
    }

    dstNameSize = sizeof(g_devPartName);  // 获取目标缓冲区大小
    // 计算源和目标的最小长度
    minSize = dstNameSize < strlen(virtualinfo.devpartpath) ? dstNameSize : strlen(virtualinfo.devpartpath);
    // 安全复制设备路径
    ret = strncpy_s(g_fatVirPart.virtualinfo.devpartpath, dstNameSize, virtualinfo.devpartpath, minSize);
    if (ret != EOK) {
        set_errno(VIRERR_PARMNAMEERR);   // 设置名称错误码
        g_fatVirPart.isparamset = FALSE; // 解锁参数设置
        return -1;                       // 返回错误
    }
    g_fatVirPart.virtualinfo.devpartpath[dstNameSize - 1] = '\0';  // 确保字符串结束符

    // 复制分区名称数组
    (void)memcpy_s(g_fatVirPart.virtualinfo.virpartname, sizeof(g_fatVirPart.virtualinfo.virpartname),
                   virtualinfo.virpartname, sizeof(virtualinfo.virpartname));
    // 复制分区百分比数组
    (void)memcpy_s(g_fatVirPart.virtualinfo.virpartpercent, sizeof(g_fatVirPart.virtualinfo.virpartpercent),
                   virtualinfo.virpartpercent, sizeof(virtualinfo.virpartpercent));
    g_fatVirPart.virtualinfo.virpartnum = virtualinfo.virpartnum;  // 设置分区数量

    return 0;  // 参数设置成功
}

/**
 * @brief 禁用虚拟分区功能
 * @param handle 文件系统句柄(FATFS*类型)
 * @return 0表示成功，非0表示错误码
 */
static INT FatfsDisablePart(void *handle)
{
    FATFS *fs = (FATFS *)handle;  // 将句柄转换为FATFS指针
    return fatfs_2_vfs(f_disvirfs(fs));  // 调用底层禁用函数并转换返回值
}

/**
 * @brief 扫描FAT表并更新子文件系统信息
 * @param handle 文件系统句柄(FATFS*类型)
 * @return 0表示成功，非0表示错误码
 * @note 扫描每个子文件系统的FAT表，更新空闲簇和最后簇信息
 */
static INT FatfsScanFat(void *handle)
{
    FATFS *fat = (FATFS *)handle;  // 将句柄转换为FATFS指针
    UINT i;                        // 循环计数器
    INT ret = FR_OK;               // 函数返回值

    // 遍历所有子文件系统
    for (i = 0; i < fat->vir_amount; i++) {
        /* Assert error will not abort the scanning process */
        ret = f_scanfat(CHILDFS(fat, i));  // 扫描当前子文件系统的FAT表
        if (ret != FR_OK) {
            break;  // 扫描失败则退出循环
        }
    }

    return fatfs_2_vfs(ret);  // 转换并返回结果
}

/**
 * @brief 检查根目录是否为空
 * @param vol 卷号
 * @return FR_OK表示为空，FR_NOTCLEAR表示非空，其他值表示错误
 * @note 扫描根目录所有条目，不考虑虚拟分区条目
 */
static FRESULT FatfsScanClear(INT vol)
{
    FRESULT ret;                 // FATFS函数返回值
    DIR dir;                     // 目录对象
    FILINFO fno;                 // 文件信息结构体
    CHAR path[MAX_LFNAME_LENGTH]; // 路径缓冲区
    INT num;                     // 目录项计数
    INT res;                     // snprintf返回值

    /* num : for the amount of all item in root directory */
    num = 0;  // 初始化目录项计数器

    (void)memset_s(path, sizeof(path), 0, sizeof(path));  // 清空路径缓冲区

    // 构建根目录路径(格式:卷号:/)
    res = snprintf_s(path, MAX_LFNAME_LENGTH, MAX_LFNAME_LENGTH - 1, "%d:/", vol);
    if (res < 0) {
        return FR_INVALID_NAME;  // 路径构建失败
    }

    ret = f_opendir(&dir, path);  // 打开根目录
    if (ret != FR_OK) {
        return ret;  // 返回打开失败错误
    }

    /* Scan all entry for searching virtual partition directory and others in root directory */
    while (1) {
        (void)memset_s(&fno, sizeof(FILINFO), 0, sizeof(FILINFO));  // 清空文件信息
        ret = f_readdir(&dir, &fno);  // 读取下一个目录项
        /* Reach the end of directory, or the root direcotry is empty, abort the scanning operation */
        if (fno.fname[0] == 0x00 || fno.fname[0] == (TCHAR)0xFF) {
            break;  // 到达目录末尾或目录为空，退出循环
        }

        if (ret != FR_OK) {
            (void)f_closedir(&dir);  // 关闭目录
            return ret;              // 返回读取错误
        }
        num++;  // 增加目录项计数
    }

    /* Close the directory */
    ret = f_closedir(&dir);  // 关闭目录
    if ((ret == FR_OK) && (num != 0)) {
        return FR_NOTCLEAR;  // 目录非空
    }

    return ret;  // 返回结果(FR_OK表示目录为空)
}

/*
 * FatfsBuildEntry:
 * Scan virtual partition entry in root directory, and try to rebuild the
 * error virtual partition, according to the scanning result.
 * Acceptable Return Value:
 * - FR_OK          : The root directory is completely clean.
 * - FR_OCCUPIED    : The virtual partition entry has been occupied by the same name file.
 * - FR_CHAIN_ERR   : The virtual partition entry has been rebuilt along the invalid cluster
 *                    chain.
 * Others Return Value:
 * Followed the by the lower API
 */
/**
 * @brief 创建虚拟分区目录条目
 * @param fat FATFS文件系统对象指针
 * @param vol 卷号
 * @return FR_OK表示成功，其他值表示错误码
 * @note 为每个子文件系统创建根目录下的目录条目，并绑定到对应簇边界
 */
static FRESULT FatfsBuildEntry(FATFS *fat, INT vol)
{
    UINT i;                        // 子文件系统计数器
    CHAR path[MAX_LFNAME_LENGTH];  // 目录路径缓冲区
    FRESULT ret;                   // FATFS函数返回值
    DIR dir;                       // 目录对象
    INT res;                       // snprintf返回值

    // 遍历所有子文件系统
    for (i = 0; i < fat->vir_amount; i++) {
        // 构建子文件系统目录路径(格式:卷号:目录名)
        res = snprintf_s(path, MAX_LFNAME_LENGTH, MAX_LFNAME_LENGTH - 1, "%d:%s", vol, CHILDFS(fat, i)->namelabel);
        if (res < 0) {
            return FR_INVALID_NAME;  // 路径构建失败
        }

        ret = f_mkdir(path);  // 创建目录
        if (ret == FR_EXIST) {  // 目录已存在
            (void)memset_s(&dir, sizeof(dir), 0, sizeof(dir));  // 清空目录对象
            ret = f_opendir(&dir, path);  // 打开目录
            if (ret == FR_NO_DIR) {  // 目录不存在(与FR_EXIST矛盾，可能是文件占用)
                return FR_OCCUPIED;  // 返回路径被占用错误
            }
            if (ret == FR_OK) {  // 成功打开目录
                // 将子文件系统绑定到目录的起始簇
                ret = f_boundary(CHILDFS(fat, i), dir.obj.sclust);
                if (ret != FR_OK) {
                    (void)f_closedir(&dir);  // 关闭目录
                    return ret;              // 返回绑定失败错误
                }
            } else {
                return ret;  // 返回打开目录失败错误
            }
            ret = f_closedir(&dir);  // 关闭目录
            if (ret != FR_OK) {
                return ret;  // 返回关闭目录失败错误
            }
        } else if (ret != FR_OK) {  // 创建目录失败(非已存在错误)
            return ret;  // 返回创建目录错误
        }
    }

    return FR_OK;  // 所有目录条目创建成功
}

/**
 * @brief 解绑虚拟分区
 * @param handle 文件系统句柄(FATFS*类型)
 * @return 0表示成功，非0表示错误码
 * @note 注销子文件系统对象，释放相关资源
 */
INT FatFsUnbindVirPart(void *handle)
{
    INT ret;                       // 函数返回值
    FATFS *fat = (FATFS *)handle;  // 将句柄转换为FATFS指针
    ret = f_unregvirfs(fat);       // 注销虚拟文件系统
    return fatfs_2_vfs(ret);       // 转换返回值为VFS错误码
}

/**
 * @brief 绑定虚拟分区
 * @param handle 文件系统句柄(FATFS*类型)
 * @param vol 卷号
 * @return 0表示成功，非0表示错误码
 * @note 检测并使用保留扇区中的虚拟分区信息，如不存在则创建新分区
 */
INT FatFsBindVirPart(void *handle, BYTE vol)
{
    INT ret;                       // 函数返回值
    FATFS *fat = (FATFS *)handle;  // 将句柄转换为FATFS指针
    char path[MAX_LFNAME_LENGTH] = {0};  // 路径缓冲区

    if (fat == NULL) {  // 参数校验
        return -EINVAL;  // 句柄为空，返回无效参数错误
    }

    // 构建卷根目录路径(格式:卷号:/)
    ret = snprintf_s(path, sizeof(path), sizeof(path) - 1, "%u:/", vol);
    if (ret < 0) {  // 路径构建失败
        return -EINVAL;  // 返回无效参数错误
    }

    /* 检测保留扇区中的虚拟分区信息并尝试使用 */
    ret = f_checkvirpart(fat, path, vol);
    if (ret == FR_NOVIRPART) {  // 未找到虚拟分区信息
        /* 使用当前设置创建并重建虚拟分区条目 */
        /* 检查环境是否适合创建虚拟分区 */
        ret = FatfsScanClear(vol);  // 检查根目录是否为空
        if (ret != FR_OK) {  // 根目录非空或检查失败
            /* 文件操作出错 */
            (void)FatfsDisablePart(fat);  // 禁用虚拟分区
            return fatfs_2_vfs(ret);       // 返回错误码
        }
        /* 准备将SD卡适配为虚拟分区 */
        ret = f_makevirpart(fat, path, vol);  // 创建虚拟分区
        if (ret != FR_OK) {  // 创建失败
            (void)FatfsDisablePart(fat);  // 禁用虚拟分区
            return fatfs_2_vfs(ret);       // 返回错误码
        }
    } else if (ret != FR_OK) {  // 检测虚拟分区信息出错
        return fatfs_2_vfs(ret);  // 返回错误码
    }
    /* 尝试构建虚拟分区目录条目 */
    ret = FatfsBuildEntry(fat, vol);
    if (ret != FR_OK) {  // 构建失败
        (void)FatfsDisablePart(fat);  // 禁用虚拟分区
        return fatfs_2_vfs(ret);       // 返回错误码
    }
#ifdef LOSCFG_FS_FAT_CACHE  // 如果启用了FAT缓存
    los_part *part = NULL;  // 分区对象指针
    if (ret == FR_OK) {  // 前面操作成功
        part = get_part((int)fat->pdrv);  // 获取分区信息
        if (part != NULL) {  // 分区存在
            ret = OsSdSync(part->disk_id);  // 同步SD卡
            if (ret < 0) {
                ret = -EIO;  // 设置I/O错误
            }
        } else {
            return -ENODEV;  // 分区不存在错误
        }
    }
#endif
    if (ret == FR_OK) {  // 前面操作成功
        ret = FatfsScanFat(fat);  // 扫描FAT表更新信息
        if (ret != FR_OK) {  // 扫描失败
            (void)FatfsDisablePart(fat);  // 禁用虚拟分区
            return fatfs_2_vfs(ret);       // 返回错误码
        }
    }
    return fatfs_2_vfs(ret);  // 返回最终结果
}

/**
 * @brief 创建虚拟分区
 * @param handle 文件系统句柄(FATFS*类型)
 * @param vol 卷号
 * @return 0表示成功，非0表示错误码
 * @note 直接创建虚拟分区并构建目录条目，不检测保留扇区信息
 */
INT FatFsMakeVirPart(void *handle, BYTE vol)
{
    INT ret;                       // 函数返回值
    FATFS *fat = (FATFS *)handle;  // 将句柄转换为FATFS指针
    char path[MAX_LFNAME_LENGTH] = {0};  // 路径缓冲区

    if (fat == NULL) {  // 参数校验
        return -EINVAL;  // 句柄为空，返回无效参数错误
    }

    // 构建卷根目录路径(格式:卷号:/)
    ret = snprintf_s(path, sizeof(path), sizeof(path) - 1, "%u:/", vol);
    if (ret < 0) {  // 路径构建失败
        return -EINVAL;  // 返回无效参数错误
    }

    /* 准备将SD卡适配为虚拟分区 */
    ret = f_makevirpart(fat, path, vol);  // 创建虚拟分区
    if (ret != FR_OK) {  // 创建失败
        (void)FatfsDisablePart(fat);  // 禁用虚拟分区
        return fatfs_2_vfs(ret);       // 返回错误码
    }

    /* 尝试构建虚拟分区目录条目 */
    ret = FatfsBuildEntry(fat, vol);
    if (ret != FR_OK) {  // 构建失败
        (void)FatfsDisablePart(fat);  // 禁用虚拟分区
        return fatfs_2_vfs(ret);       // 返回错误码
    }

    return fatfs_2_vfs(ret);  // 返回成功
}

/**
 * @brief 获取虚拟分区文件系统统计信息
 * @param mountpt 挂载点Vnode指针
 * @param relpath 相对路径
 * @param buf statfs结构体指针，用于存储统计信息
 * @return 0表示成功，非0表示错误码
 * @note 获取指定虚拟分区的总簇数、空闲簇数等文件系统信息
 */
INT fatfs_virstatfs_internel(struct Vnode *mountpt, const char *relpath, struct statfs *buf)
{
    char drive[MAX_LFNAME_LENGTH];  // 驱动器路径
    DWORD freClust, allClust;       // 空闲簇数和总簇数
    FATFS *fat = NULL;              // FATFS对象指针
    INT result, vol;                // 结果和卷号

    fat = (FATFS *)(mountpt->originMount->data);  // 获取FATFS对象
    if (fat == NULL) {
        return -EINVAL;  // 对象为空，返回无效参数错误
    }

    if (fat->vir_flag != FS_PARENT) {  // 检查是否为父文件系统
        return -EINVAL;  // 不是父文件系统，返回错误
    }

    vol = fatfs_get_vol(fat);  // 获取卷号
    if (vol < 0 || vol > FF_VOLUMES) {  // 卷号越界检查
        return -ENOENT;  // 返回卷不存在错误
    }

    if (strlen(relpath) > MAX_LFNAME_LENGTH) {  // 路径长度检查
        return -EFAULT;  // 路径过长，返回错误
    }

    // 构建驱动器路径(格式:卷号:相对路径)
    result = snprintf_s(drive, sizeof(drive), sizeof(drive) - 1, "%d:%s", vol, relpath);
    if (result < 0) {  // 路径构建失败
        return -EINVAL;  // 返回无效参数错误
    }

    // 获取虚拟分区空闲簇数和总簇数
    result = f_getvirfree(drive, &freClust, &allClust);
    if (result != FR_OK) {  // 获取失败
        result = fatfs_2_vfs(result);  // 转换错误码
        goto EXIT;                     // 跳转到出口
    }

    // 清空statfs结构体
    (void)memset_s((void *)buf, sizeof(struct statfs), 0, sizeof(struct statfs));
    buf->f_type = MSDOS_SUPER_MAGIC;  // 文件系统类型(FAT)
    buf->f_bfree = freClust;          // 空闲簇数
    buf->f_bavail = freClust;         // 可用簇数
    buf->f_blocks = allClust;         // 总簇数
#if FF_MAX_SS != FF_MIN_SS  // 如果扇区大小可变
    buf->f_bsize = fat->ssize * fat->csize;  // 块大小=扇区大小×每簇扇区数
#else  // 扇区大小固定
    buf->f_bsize = FF_MIN_SS * fat->csize;   // 块大小=最小扇区大小×每簇扇区数
#endif
#if FF_USE_LFN  // 如果启用长文件名
    buf->f_namelen = FF_MAX_LFN; /* 最大文件名长度 */
#else  // 短文件名
    /*
     * 最大文件名长度,'8'是主文件名长度, '1'是分隔符, '3'是扩展名长度
     */
    buf->f_namelen = (8 + 1 + 3);
#endif

EXIT:  // 函数出口标签
    return result;  // 返回结果
}

#endif
