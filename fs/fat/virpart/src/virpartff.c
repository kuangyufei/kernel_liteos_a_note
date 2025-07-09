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

#include "ffconf.h"
#include "virpartff.h"
#include "string.h"
#include "diskio.h"

#ifdef LOSCFG_FS_FAT_VIRTUAL_PARTITION

#if FF_FS_REENTRANT
#if FF_USE_LFN == 1
#error Static LFN work area cannot be used at thread-safe configuration
#endif
// 线程安全模式下的文件系统访问锁定宏
// 功能：获取文件系统锁，若失败则返回超时错误
#define ENTER_FF(fs)        do { if (!lock_fs(fs)) return FR_TIMEOUT; } while (0)
// 线程安全模式下的文件系统访问解锁宏
// 功能：释放文件系统锁并返回结果
#define LEAVE_FF(fs, res)   do { unlock_fs(fs, res); return res; } while (0)
#else
// 非线程安全模式下的空宏定义
#define ENTER_FF(fs)
#define LEAVE_FF(fs, res) return (res)
#endif

// 声明文件系统卷数组外部引用
extern FATFS *FatFs[FF_VOLUMES];

/*
* follow_virentry: 虚拟分区路径解析函数
* 功能：比较路径的顶级段与虚拟分区条目，将文件系统对象替换为对应的子文件系统
* 参数：
*   obj - 文件系统对象ID指针
*   path - 文件路径字符串
* 返回值：
*   FR_OK      - 顶级段匹配某个虚拟分区条目，已替换为对应子文件系统
*   FR_DENIED  - 顶级段不匹配任何虚拟分区条目，文件系统保持不变；或虚拟分区功能未启用
*   FR_INT_ERR - 断言错误（子文件系统指针无效）
*/
FRESULT follow_virentry(FFOBJID *obj, const TCHAR *path)
{
    TCHAR keyword[FF_MAX_LFN + 1] = {0};  // 存储路径顶级段的关键字缓冲区
    FATFS *fs = obj->fs;                  // 当前文件系统对象
    INT len;                              // 路径段长度计数器
    UINT i;                               // 循环索引

    (void)memset_s(keyword, sizeof(keyword), 0, sizeof(keyword));
    // 提取路径中的第一个段（'/'、'\'或空字符前的部分）
    for (len = 0; *path != '/' && *path != '\\' && *path != '\0' && len < FF_MAX_LFN; path++, len++) {
        keyword[len] = *path;
    }

    // 路径段长度验证：空路径段或超过最大长度均无效
    if (len == 0 || len > _MAX_ENTRYLENGTH) {
        return FR_DENIED;
    }

    // 遍历所有虚拟分区条目，查找匹配的子文件系统
    for (i = 0; i < fs->vir_amount; i++) {
        if (!CHILDFS(fs, i)) {  // 检查子文件系统指针有效性
            return FR_INT_ERR;
        }
        // 比较子文件系统卷标与路径段
        if (memcmp((CHILDFS(fs, i))->namelabel, keyword, _MAX_ENTRYLENGTH + 1) == 0) {
            obj->fs = CHILDFS(fs, i);  // 匹配成功，替换为子文件系统
            return FR_OK;
        }
    }

    return FR_DENIED;  // 未找到匹配的虚拟分区
}

/*
* f_checkname: 文件名合法性检查函数
* 功能：验证文件路径名是否符合FAT文件系统命名规则
* 参数：
*   path - 待检查的文件路径字符串
* 返回值：
*   FR_OK           - 文件名合法
*   FR_INVALID_NAME - 文件名包含无效字符或长度超限
*/
FRESULT f_checkname(const TCHAR *path)
{
    FRESULT res;               // 函数返回结果
    DIR dj;                    // 目录对象
    FATFS fs;                  // 文件系统对象
    TCHAR *label = (TCHAR *)path;  // 路径字符指针
    DEF_NAMBUF                 // 定义文件名缓冲区宏

    (void)memset_s(&fs, sizeof(fs), 0, sizeof(fs));
    dj.obj.fs = &fs;
    INIT_NAMBUF(&fs);          // 初始化文件名缓冲区
    res = create_name(&dj, &path);  // 解析文件名
    // 检查文件名末尾是否为空格（FAT不允许文件名以空格结尾）
    if (res == FR_OK && dj.fn[11] == 0x20) {
        res = FR_INVALID_NAME;
        return res;
    }

    FREE_NAMBUF();             // 释放文件名缓冲区

    // 检查路径中是否包含无效字符或长度超限
    for (; *label != '\0'; label++) {
        if (label - path > _MAX_ENTRYLENGTH) {  // 路径长度超过最大允许值
            res = FR_INVALID_NAME;
            return res;
        }
        if (*label == '/' || *label == '\\') {  // 路径中包含目录分隔符
            res = FR_INVALID_NAME;
            return res;
        }
    }
    return res;  // 文件名验证通过
}

/*
* f_getfatfs: 文件系统卷获取函数
* 功能：根据卷号获取对应的FATFS文件系统对象
* 参数：
*   vol - 卷号（0-based索引）
* 返回值：
*   成功 - 返回对应卷的FATFS对象指针
*   失败 - 返回NULL（卷号无效）
*/
FATFS *f_getfatfs(int vol)
{
    FATFS *fs = NULL;
    if (vol < 0 || vol >= FF_VOLUMES) {  // 卷号范围检查
        fs = NULL;
    } else {
        fs = FatFs[vol];  // 从卷数组中获取对应文件系统
    }
    return fs;
}

/*
* FatfsCheckBoundParam: 簇边界参数检查函数（静态）
* 功能：验证簇号是否在当前虚拟分区的有效范围内
* 参数：
*   fs - FATFS文件系统对象指针
*   clust - 待检查的簇号
* 返回值：
*   FR_OK        - 参数有效
*   FR_INT_ERR   - 文件系统边界参数无效
*   FR_CHAIN_ERR - 簇号超出当前虚拟分区范围
*/
static FRESULT FatfsCheckBoundParam(FATFS *fs, DWORD clust)
{
    // 检查文件系统起始簇和总簇数是否有效
    if (fs->st_clst <= 2 || (fs->st_clst + fs->ct_clst) > fs->n_fatent) {
        return FR_INT_ERR;
    }
    // 检查簇号是否在有效范围内（FAT文件系统簇号从2开始）
    if (clust < 2 || clust > fs->n_fatent) {
        return FR_INT_ERR;
    }
    // 检查簇号是否在当前虚拟分区的边界内
    if (clust >= (fs->st_clst + fs->ct_clst) || clust < fs->st_clst) {
        return FR_CHAIN_ERR;
    }

    return FR_OK;
}

/*
* f_boundary: 簇链边界检查函数
* 功能：检查从指定簇开始的簇链是否仅包含在单个虚拟分区内
* 参数：
*   fs - FATFS文件系统对象指针
*   clust - 起始簇号
* 返回值：
*   FR_OK          - 簇链仅位于一个虚拟分区内
*   FR_CHAIN_ERR   - 簇链跨越多个虚拟分区
*   FR_INT_ERR     - 文件系统对象无效或簇号范围错误
*   FR_DISK_ERR    - 读取FAT表时发生磁盘错误
*   FR_INVAILD_FATFS - 文件系统类型不是FAT32
*/
FRESULT f_boundary(FATFS *fs, DWORD clust)
{
    FFOBJID obj;               // 文件系统对象ID
    FRESULT res;               // 函数返回结果
    obj.fs = fs;
    if (fs == NULL) {          // 文件系统对象有效性检查
        return FR_INT_ERR;
    }
    if (fs->fs_type != FS_FAT32) {  // 仅支持FAT32文件系统
        return FR_INVAILD_FATFS;
    }
    ENTER_FF(fs);              // 获取文件系统锁（线程安全模式）

    res = FatfsCheckBoundParam(fs, clust);  // 检查初始簇边界参数
    if (res != FR_OK) {
        LEAVE_FF(fs, res);     // 释放锁并返回错误
    }
    // 遍历整个簇链检查边界
    for (;;) {
        clust = get_fat(&obj, clust);  // 获取下一个簇号
        if (clust == 0xFFFFFFFF) {     // FAT表读取错误
            LEAVE_FF(fs, FR_DISK_ERR);
        }
        if (clust == 0x0FFFFFFF) {     // 已到达簇链末尾
            break;
        }
        // 检查簇号是否在有效范围内
        if (clust < 2 || clust >= fs->n_fatent) {
            LEAVE_FF(fs, FR_INT_ERR);
        }
        // 检查簇号是否超出当前虚拟分区边界
        if (clust >= (fs->st_clst + fs->ct_clst) || clust < fs->st_clst) {
            LEAVE_FF(fs, FR_CHAIN_ERR);
        }
    }

    LEAVE_FF(fs, FR_OK);       // 所有簇均在边界内，返回成功
}

/*
* f_disvirfs: 虚拟分区禁用函数
* 功能：禁用指定文件系统的虚拟分区功能，释放子文件系统资源
* 参数：
*   fs - FATFS文件系统对象指针（必须为父文件系统）
* 返回值：
*   FR_OK          - 成功禁用虚拟分区
*   FR_INVAILD_FATFS - 参数为子文件系统或文件系统对象无效
*   FR_INT_ERR     - 虚拟分区数量超出最大限制
*/
FRESULT f_disvirfs(FATFS *fs)
{
    if (ISCHILD(fs)) {         // 检查是否为子文件系统（不允许操作）
        return FR_INVAILD_FATFS;
    }

    if (fs->vir_amount > _MAX_VIRVOLUMES) {  // 虚拟分区数量有效性检查
        return FR_INT_ERR;
    }

    ENTER_FF(fs);              // 获取文件系统锁

    (void)f_unregvirfs(fs);    // 调用实际的卸载函数
    LEAVE_FF(fs, FR_OK);       // 释放锁并返回成功
}

/*
* f_unregvirfs: 虚拟分区卸载函数
* 功能：释放父文件系统关联的所有子文件系统资源
* 参数：
*   fs - FATFS文件系统对象指针（必须为父文件系统）
* 返回值：
*   FR_OK          - 成功卸载虚拟分区
*   FR_INVAILD_FATFS - 参数为子文件系统或文件系统对象无效
*/
FRESULT f_unregvirfs(FATFS *fs)
{
    UINT i;                    // 循环索引

    if (fs == NULL || ISCHILD(fs)) {  // 参数有效性检查
        return FR_INVAILD_FATFS;
    }

    fs->vir_avail = FS_VIRDISABLE;  // 标记虚拟分区功能为禁用
    // 如果存在子文件系统链表
    if (fs->child_fs != NULL) {
        // 遍历所有子文件系统并释放内存
        for (i = 0; i < fs->vir_amount; i++) {
            if (CHILDFS(fs, i) != NULL) {
                ff_memfree(CHILDFS(fs, i));
            }
        }
        // 释放子文件系统链表内存
        ff_memfree(fs->child_fs);
        fs->child_fs = NULL;
        fs->vir_amount = 0xFFFFFFFF;  // 重置虚拟分区数量
    }

    return FR_OK;
}

/*
* FatfsSetParentFs: 子文件系统初始化函数（静态）
* 功能：从父文件系统复制基本信息并初始化子文件系统特有参数
* 参数：
*   pfs - 子文件系统对象指针
*   fs  - 父文件系统对象指针
*/
static void FatfsSetParentFs(FATFS *pfs, FATFS *fs)
{
    pfs->fs_type = fs->fs_type;  // 复制文件系统类型（FAT12/FAT16/FAT32）
    pfs->pdrv = fs->pdrv;        // 复制物理驱动器号
    pfs->n_fats = fs->n_fats;    // 复制FAT表数量
    pfs->id = fs->id;            // 复制文件系统ID
    pfs->n_rootdir = fs->n_rootdir;  // 复制根目录项数
    pfs->csize = fs->csize;      // 复制每簇扇区数
#if FF_MAX_SS != FF_MIN_SS
    pfs->ssize = fs->ssize;      // 复制扇区大小（仅在扇区大小可变时）
#endif
    pfs->sobj = fs->sobj;        // 复制存储设备对象

#if FF_FS_RPATH != 0
    pfs->cdir = 0;               // 重置当前目录（相对路径支持）
#endif
    pfs->n_fatent = fs->n_fatent;  // 复制总簇数
    pfs->fsize = fs->fsize;      // 复制每FAT表扇区数
    pfs->volbase = fs->volbase;  // 复制卷起始扇区
    pfs->fatbase = fs->fatbase;  // 复制FAT表起始扇区
    pfs->dirbase = fs->dirbase;  // 复制根目录起始扇区
    pfs->database = fs->database;  // 复制数据区起始扇区
    pfs->last_clst = 0xFFFFFFFF;  // 标记最后分配的簇未更新
    pfs->free_clst = 0xFFFFFFFF;  // 标记空闲簇数未更新
    pfs->st_clst = 0xFFFFFFFF;   // 标记起始簇未设置
    pfs->ct_clst = 0xFFFFFFFF;   // 标记总簇数未设置
    pfs->vir_flag = FS_CHILD;    // 标记为子文件系统
    pfs->vir_avail = FS_VIRENABLE;  // 标记子文件系统可用
    pfs->parent_fs = (void *)fs;  // 链接到父文件系统
    pfs->child_fs = (void *)NULL;  // 子文件系统无下级节点
}

/*
* f_regvirfs: 虚拟分区注册函数
* 功能：为父文件系统创建并初始化子文件系统对象
* 参数：
*   fs - FATFS文件系统对象指针（必须为非子文件系统）
* 返回值：
*   FR_OK                - 成功注册虚拟分区
*   FR_INVAILD_FATFS     - 参数为子文件系统或文件系统对象无效
*   FR_INT_ERR           - 虚拟分区数量超出最大限制
*   FR_NOT_ENOUGH_CORE   - 内存不足，无法分配子文件系统资源
*/
FRESULT f_regvirfs(FATFS *fs)
{
    UINT i;                     // 循环索引
    FATFS *pfs = NULL;          // 子文件系统对象指针

    if (fs == NULL || ISCHILD(fs)) {  // 参数有效性检查
        return FR_INVAILD_FATFS;
    }

    if (fs->vir_amount > _MAX_VIRVOLUMES) {  // 检查虚拟分区数量是否超限
        return FR_INT_ERR;
    }

    fs->parent_fs = (void *)fs;  // 将父文件系统指针指向自身
    // 标记为父文件系统（未设置具体边界）
    fs->st_clst = 0xFFFFFFFF;
    fs->ct_clst = 0xFFFFFFFF;
    // 为子文件系统链表分配内存
    fs->child_fs = (void **)ff_memalloc(fs->vir_amount * sizeof(void *));
    if (fs->child_fs == NULL) {
        return FR_NOT_ENOUGH_CORE;  // 内存分配失败
    }
    fs->vir_avail = FS_VIRENABLE;  // 标记虚拟分区功能可用

    // 为每个虚拟分区创建并初始化子文件系统
    for (i = 0; i < fs->vir_amount; i++) {
        pfs = ff_memalloc(sizeof(FATFS));  // 分配子文件系统内存
        if (pfs == NULL) {  // 内存分配失败，回滚之前的分配
            goto ERROUT;
        }
        FatfsSetParentFs(pfs, fs);  // 初始化子文件系统信息
        *(fs->child_fs + i) = (void *)pfs;  // 添加到子文件系统链表
    }

    return FR_OK;  // 所有子文件系统创建成功
ERROUT:  // 错误处理：释放已分配的子文件系统内存
    while (i > 0) {
        --i;
        ff_memfree(*(fs->child_fs + i));
    }
    ff_memfree(fs->child_fs);
    fs->child_fs = NULL;

    return FR_NOT_ENOUGH_CORE;
}

/*
* FatfsCheckScanFatParam: FAT扫描参数检查函数（静态）
* 功能：验证子文件系统的FAT扫描参数是否有效
* 参数：
*   fs - FATFS文件系统对象指针
* 返回值：
*   FR_OK          - 参数有效
*   FR_INVAILD_FATFS - 文件系统对象无效、非FAT32类型或为父文件系统
*   FR_DENIED      - 文件系统为普通模式（非虚拟分区）
*/
static FRESULT FatfsCheckScanFatParam(FATFS *fs)
{
    if (fs == NULL) {
        return FR_INVAILD_FATFS;
    }

    if (ISNORMAL(fs)) {  // 普通文件系统不支持此操作
        return FR_DENIED;
    }

    if (fs->fs_type != FS_FAT32 || ISPARENT(fs)) {  // 仅子FAT32文件系统支持
        return FR_INVAILD_FATFS;
    }

    // 检查起始簇是否在有效范围内（FAT32簇号从2开始，3为第一个可用簇）
    if (fs->st_clst < 3 || fs->st_clst >= fs->n_fatent) {
        return FR_INVAILD_FATFS;
    }

    // 检查簇数量是否有效（不能为0且不能超过总簇数-3）
    if (fs->ct_clst == 0 || fs->ct_clst > (fs->n_fatent - 3)) {
        return FR_INVAILD_FATFS;
    }

    // 检查起始簇+簇数量是否超出总簇数或小于3
    if ((fs->st_clst + fs->ct_clst) > fs->n_fatent || (fs->st_clst + fs->ct_clst) < 3) {
        return FR_INVAILD_FATFS;
    }

    return FR_OK;
}

/*
* f_scanfat: FAT表扫描函数
* 功能：扫描子文件系统边界内的FAT表，更新空闲簇数和最后簇号
* 参数：
*   fs - FATFS文件系统对象指针（必须为子FAT32文件系统）
* 返回值：
*   FR_OK          - 扫描成功并更新参数
*   FR_INVAILD_FATFS - 文件系统参数无效
*   FR_DENIED      - 文件系统为普通模式
*   FR_DISK_ERR    - 读取FAT表时发生磁盘错误
*/
FRESULT f_scanfat(FATFS *fs)
{
    FRESULT res;               // 函数返回结果
    DWORD clst;                // 当前簇号
    DWORD link;                // 下一簇号
    FFOBJID obj;               // 文件系统对象ID

    res = FatfsCheckScanFatParam(fs);  // 检查扫描参数
    if (res != FR_OK) {
        return res;
    }

    ENTER_FF(fs);              // 获取文件系统锁
    res = FR_OK;
    obj.fs = fs;

    fs->free_clst = fs->ct_clst;  // 初始化空闲簇数为总簇数
    // 遍历子文件系统范围内的所有簇
    for (clst = fs->st_clst; clst < fs->st_clst + fs->ct_clst; clst++) {
        link = get_fat(&obj, clst);  // 读取FAT表项
        if (link == 0xFFFFFFFF) {    // FAT表读取错误
            LEAVE_FF(fs, FR_DISK_ERR);
        }
        if (link == 0) {             // 空闲簇（FAT表项为0）
            continue;
        }
        fs->free_clst--;  // 已使用簇，空闲簇数减1
    }
    fs->last_clst = fs->st_clst - 1;  // 初始化最后分配簇为起始簇前一个

    LEAVE_FF(fs, res);         // 释放锁并返回成功
}

/*
* FatfsCheckStart: 虚拟分区起始检查函数（静态）
* 功能：验证虚拟分区元数据与当前文件系统是否匹配
* 参数：
*   work - 工作缓冲区（存储虚拟分区元数据）
*   fs   - FATFS文件系统对象指针
*   vol  - 卷号
* 返回值：
*   FR_OK          - 元数据匹配
*   FR_NOVIRPART   - 元数据验证字符串不匹配（不是LITEOS虚拟分区）
*   FR_MODIFIED    - 元数据与当前文件系统信息不匹配
*/
static FRESULT FatfsCheckStart(BYTE *work, FATFS *fs, BYTE vol)
{
    DWORD startBaseSect, countBaseSect;

    countBaseSect = LD2PC(vol);  // 获取卷总扇区数
    startBaseSect = LD2PS(vol);  // 获取卷起始扇区

    // 检查验证字符串是否为"LITE"（ASCII码0x4C495445）
    if (ld_dword(work + VR_VertifyString) != 0x4C495445) {
        return FR_NOVIRPART;
    }
    // 检查文件系统类型是否匹配
    if (work[VR_PartitionFSType] != fs->fs_type) {
        return FR_MODIFIED;
    }
    // 检查卷起始扇区是否匹配
    if (ld_dword(work + VR_PartitionStSec) != startBaseSect) {
        return FR_MODIFIED;
    }
    // 检查卷总扇区数是否匹配
    if (ld_dword(work + VR_PartitionCtSec) != countBaseSect) {
        return FR_MODIFIED;
    }
    // 检查每簇扇区数是否匹配
    if (ld_word(work + VR_PartitionClstSz) != fs->csize) {
        return FR_MODIFIED;
    }
    // 检查总簇数是否匹配
    if (ld_dword(work + VR_PartitionCtClst) != fs->n_fatent) {
        return FR_MODIFIED;
    }
    // 检查虚拟分区数量是否超出最大限制
    if (work[VR_PartitionCnt] > _MAX_VIRVOLUMES) {
        return FR_MODIFIED;
    }

    return FR_OK;
}

/*
* FatfsCheckPercent: 虚拟分区百分比检查函数（静态）
* 功能：检查子文件系统的簇范围是否在父文件系统总簇数范围内
* 参数：
*   fs - 父FATFS文件系统对象指针
*   i  - 子文件系统索引
* 返回值：
*   FR_OK          - 簇范围有效
*   FR_MODIFIED    - 簇范围超出父文件系统总簇数
*/
static FRESULT FatfsCheckPercent(FATFS *fs, WORD i)
{
    // 子文件系统簇结束位置 = 起始簇 + 总簇数
    if ((CHILDFS(fs, i))->st_clst + (CHILDFS(fs, i))->ct_clst < fs->n_fatent) {
        // 未超出父文件系统总簇数，更新父文件系统剩余簇信息
        fs->st_clst = (CHILDFS(fs, i))->st_clst + (CHILDFS(fs, i))->ct_clst;
        fs->ct_clst = fs->n_fatent - ((CHILDFS(fs, i))->st_clst + (CHILDFS(fs, i))->ct_clst);
    } else if ((CHILDFS(fs, i))->st_clst + (CHILDFS(fs, i))->ct_clst == fs->n_fatent) {
        // 正好等于父文件系统总簇数，标记无剩余簇
        fs->st_clst = 0xFFFFFFFF;
        fs->ct_clst = 0xFFFFFFFF;
    } else {
        // 超出父文件系统总簇数，卸载虚拟分区
        (void)f_unregvirfs(fs);
        return FR_MODIFIED;
    }

    return FR_OK;
}

/*
* FatfsCheckPartClst: 虚拟分区簇连续性检查函数（静态）
* 功能：检查虚拟分区之间的簇是否连续且第一个分区从簇3开始
* 参数：
*   fs - 父FATFS文件系统对象指针
*   i  - 子文件系统索引
* 返回值：
*   FR_OK          - 簇连续性检查通过
*   FR_MODIFIED    - 簇不连续或第一个分区起始簇不是3
*/
static FRESULT FatfsCheckPartClst(FATFS *fs, WORD i)
{
    if (i == 0) {
        // 第一个虚拟分区必须从簇3开始（FAT32保留簇0-2）
        if ((CHILDFS(fs, i))->st_clst != 3) {
            (void)f_unregvirfs(fs);
            return FR_MODIFIED;
        }
    } else {
        // 检查当前分区是否紧跟前一个分区（无间隙）
        if ((CHILDFS(fs, i))->st_clst != (CHILDFS(fs, (i - 1))->st_clst + CHILDFS(fs, (i - 1))->ct_clst)) {
            (void)f_unregvirfs(fs);
            return FR_MODIFIED;
        }
    }

    return FR_OK;
}

/*
* FatfsSetChildClst: 子文件系统簇信息设置函数（静态）
* 功能：从虚拟分区元数据中读取并设置子文件系统的起始簇和总簇数
* 参数：
*   work - 工作缓冲区（存储虚拟分区元数据）
*   fs   - 父FATFS文件系统对象指针
*   i    - 子文件系统索引
*/
static void FatfsSetChildClst(BYTE *work, FATFS *fs, WORD i)
{
    // 从元数据读取起始簇并设置
    (CHILDFS(fs, i))->st_clst = ld_dword(work + VR_PARTITION + i * VR_ITEMSIZE + VR_StartClust);
    // 从元数据读取总簇数并设置
    (CHILDFS(fs, i))->ct_clst = ld_dword(work + VR_PARTITION + i * VR_ITEMSIZE + VR_CountClust);
}

/*
* f_ckvtlpt :
* Check the external SD virtual paritition sectors and read configure from it
*
* Acceptable Return Value:
* - FR_OK          : The external SD configure is complete, all info has been set to the
*                  each CHILD FATFS
* - FR_NOT_MATCHED : The virtual partition's configure does not matched as current setting
* - FR_MODIFIED    : The virtual partition's configure has been destroyed partly or completely
* - FR_NOVIRPART   : The external SD has not been apllied as virtual partition yet
*
* Others Return Value:
* - FR_INVAILD_FATFS : The FATFS object has error or the info in it has been occuried
* - FR_DENIED          : The virtual partition feature has been shut down by switcher
* - FR_INVALID_DRIVE   : The drive index is error
* - FR_DISK_ERR        : A Disk error happened
*/

/*
* f_checkvirpart: 虚拟分区验证函数
* 功能：检查并验证外部存储介质上的虚拟分区信息是否有效
* 参数：
*   fs   - FATFS文件系统对象指针（必须为父文件系统）
*   path - 文件路径字符串
*   vol  - 卷号
* 返回值：
*   FR_OK          - 虚拟分区信息有效
*   FR_INVAILD_FATFS - 文件系统对象无效或未初始化
*   FR_INT_ERR     - 尝试对非父文件系统操作
*   FR_NOT_ENOUGH_CORE - 内存分配失败
*   FR_DISK_ERR    - 磁盘读取错误
*   FR_NOVIRPART   - 未找到有效的虚拟分区标识
*   FR_MODIFIED    - 虚拟分区信息与当前设置不匹配
*/
FRESULT f_checkvirpart(FATFS *fs, const TCHAR *path, BYTE vol)
{
    FRESULT res;                     // 函数返回结果
    WORD i;                          // 循环索引
    DWORD virSect;                   // 虚拟分区元数据存储扇区
    DWORD tmp;                       // 临时变量
    BYTE pdrv;                       // 物理驱动器号
    BYTE *work = NULL;               // 工作缓冲区指针
    CHAR label[_MAX_ENTRYLENGTH + 1]; // 卷标缓冲区
    DWORD *labelTmp = NULL;          // 用于清除编译警告的临时指针

    // 检查文件系统对象有效性和磁盘初始化状态
    if (fs == NULL || (disk_status(fs->pdrv) & STA_NOINIT)) {
        return FR_INVAILD_FATFS; /* 对象无效 */
    }

    /* 锁定文件系统对象 */
    res = mount_volume(&path, &fs, FA_WRITE); /* 将文件系统信息更新到父文件系统 */
    if (res != FR_OK) {
        LEAVE_FF(fs, res);
    }

    // 不允许对子文件系统执行此操作
    if (ISCHILD(fs)) {
        LEAVE_FF(fs, FR_INT_ERR);
    }
    /* 数据将保存在最后一个保留扇区，即FAT表起始扇区的前一个扇区 */
    virSect = fs->fatbase - 1;

    pdrv = LD2PD(vol); /* 驱动器索引 */

    // 分配扇区大小的工作缓冲区
    work = (BYTE *)ff_memalloc(SS(fs));
    if (work == NULL) {
        LEAVE_FF(fs, FR_NOT_ENOUGH_CORE);
    }
    /* 检查并验证分区信息 */
    if (disk_read(pdrv, work, virSect, 1) != RES_OK) { // 读取虚拟分区元数据扇区
        res = FR_DISK_ERR;
        goto EXIT;
    } /* 加载VBR */

    // 验证虚拟分区元数据起始标识和基本信息
    res = FatfsCheckStart(work, fs, vol);
    if (res != FR_OK) {
        goto EXIT;
    }
    /* 检查虚拟分区数量是否与当前设置匹配 */
    fs->vir_amount = work[VR_PartitionCnt];
    res = f_regvirfs(fs); // 注册子文件系统
    if (res != FR_OK) {
        goto EXIT;
    }

    // 遍历所有可能的虚拟分区条目
    for (i = 0; i < _MAX_VIRVOLUMES; i++) {
        if (i < work[VR_PartitionCnt]) { // 当前条目为有效虚拟分区
            // 检查分区可用标志（必须为0x80）
            if (work[VR_PARTITION + i * VR_ITEMSIZE + VR_Available] != 0x80) {
                (void)f_unregvirfs(fs); // 卸载已注册的子文件系统
                res = FR_MODIFIED;
                goto EXIT;
            }
        } else { // 当前条目为无效虚拟分区
            // 检查分区可用标志（必须为0x00）
            if (work[VR_PARTITION + i * VR_ITEMSIZE + VR_Available] != 0x00) {
                (void)f_unregvirfs(fs); // 卸载已注册的子文件系统
                res = FR_MODIFIED;
                goto EXIT;
            }
            break; // 后续条目无需检查
        }

        (void)memset_s(label, sizeof(label), 0, sizeof(label));

        // 从元数据读取卷标（分4个DWORD读取）
        tmp = ld_dword(work + VR_PARTITION + i * VR_ITEMSIZE + VR_Entry + 0);
        labelTmp = (DWORD *)label;
        *labelTmp = tmp;
        tmp = ld_dword(work + VR_PARTITION + i * VR_ITEMSIZE + VR_Entry + 4);
        *((DWORD *)(label + 4)) = tmp;
        tmp = ld_dword(work + VR_PARTITION + i * VR_ITEMSIZE + VR_Entry + 8);
        *((DWORD *)(label + 8)) = tmp;
        tmp = ld_dword(work + VR_PARTITION + i * VR_ITEMSIZE + VR_Entry + 12);
        *((DWORD *)(label + 12)) = tmp;

        // 验证卷标合法性
        if (f_checkname(label) != FR_OK) {
            (void)f_unregvirfs(fs);
            res = FR_MODIFIED;
            goto EXIT;
        }
        // 将卷标复制到子文件系统
        (void)memcpy_s((CHILDFS(fs, i))->namelabel, _MAX_ENTRYLENGTH + 1, label, _MAX_ENTRYLENGTH + 1);

        // 设置子文件系统的起始簇和总簇数
        FatfsSetChildClst(work, fs, i);

        /* 外部SD卡设置已超出整个分区的簇数量 */
        if ((QWORD)(CHILDFS(fs, i))->st_clst + (QWORD)((CHILDFS(fs, i))->ct_clst) > (QWORD)fs->n_fatent) {
            (void)f_unregvirfs(fs);
            res = FR_MODIFIED;
            goto EXIT;
        }

        // 检查虚拟分区簇的连续性
        res = FatfsCheckPartClst(fs, i);
        if (res != FR_OK) {
            goto EXIT;
        }
        if (i == (work[VR_PartitionCnt] - 1)) { // 最后一个虚拟分区
            /*
             * 如果外部SD卡虚拟分区百分比超出当前虚拟分区百分比设置的误差容忍范围
             */
            res = FatfsCheckPercent(fs, i);
            if (res != FR_OK) {
                goto EXIT;
            }
        }
    }
EXIT:
    ff_memfree(work); // 释放工作缓冲区
    LEAVE_FF(fs, res);
}

/*
* FatfsClacPartInfo: 虚拟分区信息计算函数（静态）
* 功能：根据虚拟分区百分比计算子文件系统的起始簇和总簇数
* 参数：
*   fs         - FATFS文件系统对象指针（父文件系统）
*   virpartper - 累计虚拟分区百分比
*   i          - 当前虚拟分区索引
*/
static void FatfsClacPartInfo(FATFS *fs, DOUBLE virpartper, UINT i)
{
    if (i == 0) { // 第一个虚拟分区
        (CHILDFS(fs, i))->st_clst = 3; // 从簇3开始（FAT32保留簇0-2）
        // 计算总簇数 = (总可用簇数) * 分区百分比
        (CHILDFS(fs, i))->ct_clst = (DWORD)((fs->n_fatent - 3) *
                                            g_fatVirPart.virtualinfo.virpartpercent[i]);

        // 更新父文件系统的起始簇和剩余簇数
        fs->st_clst = (CHILDFS(fs, i))->st_clst + (CHILDFS(fs, i))->ct_clst;
        fs->ct_clst = fs->n_fatent - fs->st_clst;
    } else if (i != (fs->vir_amount - 1)) { // 中间虚拟分区
        // 起始簇 = 前一个分区的起始簇 + 前一个分区的总簇数
        (CHILDFS(fs, i))->st_clst = (CHILDFS(fs, (i - 1)))->st_clst + (CHILDFS(fs, (i - 1)))->ct_clst;
        // 计算总簇数 = (总可用簇数) * 分区百分比
        (CHILDFS(fs, i))->ct_clst = (DWORD)((fs->n_fatent - 3) *
                                            g_fatVirPart.virtualinfo.virpartpercent[i]);
    } else { // 最后一个虚拟分区
        // 起始簇 = 前一个分区的起始簇 + 前一个分区的总簇数
        (CHILDFS(fs, i))->st_clst = (CHILDFS(fs, (i - 1)))->st_clst + (CHILDFS(fs, (i - 1)))->ct_clst;
        // 检查累计百分比是否接近100%（考虑浮点误差）
        if (virpartper <= (1 + _FLOAT_ACC) && virpartper >= (1 - _FLOAT_ACC)) {
            // 使用所有剩余簇
            (CHILDFS(fs, i))->ct_clst = fs->n_fatent - (CHILDFS(fs, i))->st_clst;
            fs->st_clst = 0xFFFFFFFF; // 无剩余簇
            fs->ct_clst = 0xFFFFFFFF;
        } else {
            // 按百分比计算总簇数
            (CHILDFS(fs, i))->ct_clst = (DWORD)((fs->n_fatent - 3) *
                                                g_fatVirPart.virtualinfo.virpartpercent[i]);
            // 更新父文件系统的起始簇和剩余簇数
            fs->st_clst = (CHILDFS(fs, i))->st_clst + (CHILDFS(fs, i))->ct_clst;
            fs->ct_clst = fs->n_fatent - fs->st_clst;
        }
    }
}

/*
* f_makevirpart: 虚拟分区创建函数
* 功能：将当前虚拟分区设置应用到外部SD卡并初始化子文件系统
* 参数：
*   fs   - FATFS文件系统对象指针（必须为父文件系统）
*   path - 文件路径字符串
*   vol  - 卷号
* 返回值：
*   FR_OK          - 成功应用设置到SD卡和子文件系统
*   FR_INVAILD_FATFS - 文件系统对象无效或未初始化
*   FR_DENIED      - 虚拟分区功能已被禁用
*   FR_INVALID_DRIVE - 驱动器索引错误
*   FR_DISK_ERR    - 磁盘写入错误
*   FR_NOT_ENOUGH_CORE - 内存分配失败
*/
FRESULT f_makevirpart(FATFS *fs, const TCHAR *path, BYTE vol)
{
    FRESULT res;                     // 函数返回结果
    DWORD virSect;                   // 虚拟分区元数据存储扇区
    DWORD startBaseSect, countBaseSect; // 卷起始扇区和总扇区数
    DWORD tmp;                       // 临时变量
    CHAR label[_MAX_ENTRYLENGTH + 1]; // 卷标缓冲区
    DWORD *labelTmp = NULL;          // 用于清除编译警告的临时指针
    UINT i;                          // 循环索引
    BYTE pdrv;                       // 物理驱动器号
    BYTE *work = NULL;               // 工作缓冲区指针
    DOUBLE virpartper = 0.0;         // 累计虚拟分区百分比

    // 检查文件系统对象有效性和磁盘初始化状态
    if (fs == NULL || (disk_status(fs->pdrv) & STA_NOINIT)) {
        return FR_INVAILD_FATFS; /* 对象无效 */
    }

    /* 锁定文件系统对象 */
    res = mount_volume(&path, &fs, FA_WRITE); /* 将文件系统信息更新到父文件系统 */
    if (res != FR_OK) {
        LEAVE_FF(fs, res);
    }

    /* 仅在FAT32文件系统中可用 */
    if (ISCHILD(fs)) { // 不允许对子文件系统操作
        LEAVE_FF(fs, FR_INVAILD_FATFS);
    }
    /* 数据将保存在最后一个保留扇区，即FAT表起始扇区的前一个扇区 */
    virSect = fs->fatbase - 1;
    /* 查找与卷索引对应的文件系统索引 */
    pdrv = LD2PD(vol); /* 驱动器索引 */
    countBaseSect = LD2PC(vol); /* 卷总扇区数 */
    startBaseSect = LD2PS(vol); /* 卷起始扇区 */

    fs->vir_amount = g_fatVirPart.virtualinfo.virpartnum; // 设置虚拟分区数量
    res = f_regvirfs(fs); // 注册子文件系统
    if (res != FR_OK) {
        LEAVE_FF(fs, res);
    }

    // 分配扇区大小的工作缓冲区
    work = (BYTE *)ff_memalloc(SS(fs));
    if (work == NULL) {
        LEAVE_FF(fs, FR_NOT_ENOUGH_CORE);
    }
    /* 数据簇从簇#3开始到最后一个簇 */
    /* 簇#0 #1用于VBR、保留扇区和FAT表 */
    /* 簇#2用于根目录 */
    (void)memset_s(work, SS(fs), 0, SS(fs)); // 清空工作缓冲区

    // 遍历所有虚拟分区，设置卷标和簇信息
    for (i = 0; i < fs->vir_amount; i++) {
        /* 复制条目卷标并写入工作扇区缓冲区 */
        (void)memset_s(label, sizeof(label), 0, sizeof(label));
        (void)memcpy_s(label, _MAX_ENTRYLENGTH + 1, g_fatVirPart.virtualinfo.virpartname[i], _MAX_ENTRYLENGTH + 1);
        labelTmp = (DWORD *)label;
        tmp = *labelTmp;
        st_dword(work + VR_PARTITION + i * VR_ITEMSIZE + VR_Entry + 0, tmp);
        tmp = *((DWORD *)(label + 4));
        st_dword(work + VR_PARTITION + i * VR_ITEMSIZE + VR_Entry + 4, tmp);
        tmp = *((DWORD *)(label + 8));
        st_dword(work + VR_PARTITION + i * VR_ITEMSIZE + VR_Entry + 8, tmp);
        tmp = *((DWORD *)(label + 12));
        st_dword(work + VR_PARTITION + i * VR_ITEMSIZE + VR_Entry + 12, tmp);

        virpartper += g_fatVirPart.virtualinfo.virpartpercent[i]; // 累计分区百分比

        // 将卷标复制到子文件系统
        (void)memcpy_s((CHILDFS(fs, i))->namelabel, _MAX_ENTRYLENGTH + 1, g_fatVirPart.virtualinfo.virpartname[i],
                       _MAX_ENTRYLENGTH + 1);
        FatfsClacPartInfo(fs, virpartper, i); // 计算子分区簇信息
        (CHILDFS(fs, i))->last_clst = (CHILDFS(fs, i))->st_clst - 1; // 初始化最后分配簇
        work[VR_PARTITION + i * VR_ITEMSIZE + VR_Available] = 0x80; // 设置分区可用标志
    }

    /* 设置扇区数据 */
    work[VR_PartitionCnt] = fs->vir_amount; // 虚拟分区数量
    work[VR_PartitionFSType] = fs->fs_type; // 文件系统类型
    st_dword(work + VR_PartitionStSec, startBaseSect); // 卷起始扇区
    st_dword(work + VR_PartitionCtSec, countBaseSect); // 卷总扇区数
    st_word(work + VR_PartitionClstSz, fs->csize); // 每簇扇区数
    st_dword(work + VR_PartitionCtClst, fs->n_fatent); // 总簇数
    // 写入每个子分区的起始簇和总簇数
    for (i = 0; i < fs->vir_amount; i++) {
        st_dword(work + VR_PARTITION + i * VR_ITEMSIZE + VR_StartClust,
                 (CHILDFS(fs, i))->st_clst);
        st_dword(work + VR_PARTITION + i * VR_ITEMSIZE + VR_CountClust,
                 (CHILDFS(fs, i))->ct_clst);
    }

    /* 验证字符串"LITE"的ASCII码 */
    st_dword(work + VR_VertifyString, 0x4C495445);

    /* 写入数据区域 */
    if (disk_write(pdrv, work, virSect, 1) != RES_OK) { // 写入虚拟分区元数据
        (void)f_unregvirfs(fs); // 写入失败，卸载子文件系统
        res = FR_DISK_ERR;
    }

    ff_memfree(work); // 释放工作缓冲区
    LEAVE_FF(fs, res);
}

/*
* f_getvirfree: 虚拟分区空闲簇获取函数
* 功能：获取指定路径所在虚拟分区的空闲簇数和总簇数
* 参数：
*   path  - 文件路径字符串
*   nclst - 输出参数，接收空闲簇数
*   cclst - 输出参数，接收总簇数
* 返回值：
*   FR_OK          - 成功获取空闲簇信息
*   FR_DENIED      - 虚拟分区功能未启用
*   FR_INVAILD_FATFS - 文件系统对象无效
*   FR_DISK_ERR    - 读取FAT表时发生磁盘错误
*/
FRESULT f_getvirfree(const TCHAR *path, DWORD *nclst, DWORD *cclst)
{
    FATFS *fs = NULL;                // 文件系统对象指针
    FRESULT res;                     // 函数返回结果
    DWORD clst, link;                // 当前簇号和下一簇号
    DWORD nfree;                     // 总空闲簇数
    UINT i;                          // 循环索引
    DIR dj;                          // 目录对象

    /* 查找卷以更新全局FSINFO */
    res = mount_volume(&path, &fs, 0);
    if (res != FR_OK) {
        LEAVE_FF(fs, res);
    }

    /* 根据条目关键字决定是否将父文件系统替换为子文件系统 */
    dj.obj.fs = fs;
    if (ISVIRPART(fs)) { // 虚拟分区功能已启用
        /* 检查虚拟分区顶级目录，并匹配虚拟文件系统 */
        res = follow_virentry(&dj.obj, path);
        if (res == FR_INT_ERR) {
            LEAVE_FF(fs, res);
        }
        if (res == FR_OK) {
            fs = dj.obj.fs; // 切换到匹配的子文件系统
        }
    } else {
        /* 虚拟分区功能已关闭，拒绝此操作 */
        LEAVE_FF(fs, FR_DENIED);
    }

    /* 如果当前FATFS是子文件系统 */
    if (ISCHILD(fs)) {
        /* 如果子文件系统的空闲簇数无效，则扫描FAT表并更新 */
        if (fs->free_clst > fs->ct_clst) {
            dj.obj.fs = fs;
            fs->free_clst = fs->ct_clst; // 初始化为总簇数
            // 遍历所有簇，计算空闲簇数
            for (clst = fs->st_clst; clst < fs->st_clst + fs->ct_clst; clst++) {
                link = get_fat(&dj.obj, clst); // 读取FAT表项
                if (link == 0) { // 空闲簇（FAT表项为0）
                    continue;
                }
                fs->free_clst--; // 已使用簇，空闲簇数减1
            }
        }
        *nclst = fs->free_clst; // 输出空闲簇数
        *cclst = fs->ct_clst;   // 输出总簇数
        LEAVE_FF(fs, FR_OK);
    } else { // 当前为父文件系统
        nfree = 0;
        if (fs->ct_clst == 0xFFFFFFFF) { // 无可用簇
            LEAVE_FF(fs, FR_DENIED);
        }
        // 遍历所有子文件系统，计算总空闲簇数
        for (i = 0; i < fs->vir_amount; i++) {
            // 如果子文件系统的空闲簇数无效，则扫描FAT表并更新
            if (CHILDFS(fs, i)->free_clst > CHILDFS(fs, i)->ct_clst) {
                dj.obj.fs = CHILDFS(fs, i);
                CHILDFS(fs, i)->free_clst = CHILDFS(fs, i)->ct_clst; // 初始化为总簇数
                // 遍历子文件系统的所有簇
                for (clst = CHILDFS(fs, i)->st_clst; clst < CHILDFS(fs, i)->st_clst + CHILDFS(fs, i)->ct_clst; clst++) {
                    link = get_fat(&dj.obj, clst); // 读取FAT表项
                    if (link == 0) { // 空闲簇
                        continue;
                    }
                    CHILDFS(fs, i)->free_clst--; // 已使用簇，空闲簇数减1
                }
            }
            nfree += CHILDFS(fs, i)->free_clst; // 累加子分区空闲簇数
        }
        *nclst = fs->free_clst - nfree; // 父分区空闲簇数 = 总空闲簇数 - 子分区空闲簇数之和
        *cclst = fs->ct_clst;           // 输出父分区总簇数
        LEAVE_FF(fs, FR_OK);
    }
}

#endif
