/*
 * Copyright (c) 2023-2023 Huawei Device Co., Ltd. All rights reserved.
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

#ifdef LOSCFG_USER_CONTAINER
#include "los_user_container_pri.h"
#include "errno.h"
#include "ctype.h"
#include "los_config.h"
#include "los_memory.h"
#include "proc_fs.h"
#include "user_copy.h"
#include "los_seq_buf.h"
#include "capability_type.h"
#include "capability_api.h"
#include "internal.h"
// 默认溢出用户ID，当UID映射失败时使用  65534是Linux系统中nobody用户的典型UID
#define DEFAULT_OVERFLOWUID    65534
// 默认溢出组ID，当GID映射失败时使用  65534是Linux系统中nobody组的典型GID
#define DEFAULT_OVERFLOWGID    65534
// 用户容器层级最大值  限制容器嵌套深度
#define LEVEL_MAX 3
// 十六进制基数
#define HEX 16
// 八进制基数
#define OCT 8
// 十进制基数
#define DEC 10

// 当前用户容器数量  用于跟踪系统中创建的用户容器总数
UINT32 g_currentUserContainerNum = 0;

/**
 * @brief 创建用户容器
 * 
 * @param newCredentials 新凭证结构体指针，用于关联创建的用户容器
 * @param parentUserContainer 父用户容器指针，若为根容器则为NULL
 * @return UINT32 成功返回LOS_OK，失败返回错误码(EPERM/EINVAL/ENOMEM)
 */
UINT32 OsCreateUserContainer(Credentials *newCredentials, UserContainer *parentUserContainer)
{
    // 检查用户容器数量是否超过限制
    if (g_currentUserContainerNum >= OsGetContainerLimit(USER_CONTAINER)) {
        return EPERM;  // 容器数量超限，返回权限错误
    }

    // 检查父容器层级是否超过最大限制
    if ((parentUserContainer != NULL) && (parentUserContainer->level >= LEVEL_MAX)) {
        return EINVAL;  // 层级超限，返回参数无效错误
    }

    // 检查用户ID和组ID是否合法
    if ((newCredentials->euid < 0) || (newCredentials->egid < 0)) {
        return EINVAL;  // UID/GID为负数，返回参数无效错误
    }

    // 分配用户容器内存
    UserContainer *userContainer = LOS_MemAlloc(m_aucSysMem1, sizeof(UserContainer));
    if (userContainer == NULL) {
        return ENOMEM;  // 内存分配失败，返回内存不足错误
    }
    (VOID)memset_s(userContainer, sizeof(UserContainer), 0, sizeof(UserContainer));  // 初始化用户容器内存为0

    g_currentUserContainerNum++;  // 增加用户容器计数
    userContainer->containerID = OsAllocContainerID();  // 分配容器ID
    userContainer->parent = parentUserContainer;  // 设置父容器指针
    newCredentials->userContainer = userContainer;  // 关联凭证与用户容器
    if (parentUserContainer != NULL) {  // 非根容器初始化
        LOS_AtomicInc(&parentUserContainer->rc);  // 增加父容器引用计数
        LOS_AtomicSet(&userContainer->rc, 1);  // 设置当前容器引用计数为1
        userContainer->level = parentUserContainer->level + 1;  // 设置容器层级为父容器层级+1
        userContainer->owner = newCredentials->euid;  // 设置容器所有者UID
        userContainer->group = newCredentials->egid;  // 设置容器所有者GID
    } else {  // 根容器初始化
        LOS_AtomicSet(&userContainer->rc, 3); /* 3: 三个系统进程引用根容器 */
        userContainer->uidMap.extentCount = 1;  // UID映射范围数量为1
        userContainer->uidMap.extent[0].count = 4294967295U;  // UID映射数量(2^32-1)
        userContainer->gidMap.extentCount = 1;  // GID映射范围数量为1
        userContainer->gidMap.extent[0].count = 4294967295U;  // GID映射数量(2^32-1)
    }
    return LOS_OK;
}

/**
 * @brief 释放用户容器及其父容器（当引用计数为0时）
 * 
 * @param userContainer 需要释放的用户容器指针
 */
VOID FreeUserContainer(UserContainer *userContainer)
{
    UserContainer *parent = NULL;
    do {
        parent = userContainer->parent;  // 保存父容器指针
        (VOID)LOS_MemFree(m_aucSysMem1, userContainer);  // 释放当前用户容器内存
        userContainer->parent = NULL;  // 清空父容器指针，避免野指针
        userContainer = parent;  // 切换到父容器
        g_currentUserContainerNum--;  // 减少用户容器计数
        if (userContainer == NULL) {  // 父容器为空，退出循环
            break;
        }
        LOS_AtomicDec(&userContainer->rc);  // 减少父容器引用计数
    } while (LOS_AtomicRead(&userContainer->rc) <= 0);  // 当父容器引用计数为0时继续释放
}

/**
 * @brief 根据ID查找对应的UID/GID映射范围（基础函数）
 * 
 * @param extents 映射范围数量
 * @param map UID/GID映射结构体指针
 * @param id 需要查找的ID
 * @return UidGidExtent* 成功返回映射范围指针，失败返回NULL
 */
STATIC UidGidExtent *MapIdUpBase(UINT32 extents, UidGidMap *map, UINT32 id)
{
    UINT32 idx;
    UINT32 first;  // 映射范围起始ID
    UINT32 last;   // 映射范围结束ID

    // 遍历所有映射范围查找包含目标ID的范围
    for (idx = 0; idx < extents; idx++) {
        first = map->extent[idx].lowerFirst;  // 获取当前范围的起始ID
        last = first + map->extent[idx].count - 1;  // 计算当前范围的结束ID
        if ((id >= first) && (id <= last)) {  // 目标ID在当前范围内
            return &map->extent[idx];  // 返回找到的映射范围
        }
    }
    return NULL;  // 未找到映射范围
}

/**
 * @brief 将内部ID映射为外部ID
 * 
 * @param map UID/GID映射结构体指针
 * @param id 需要映射的内部ID
 * @return UINT32 成功返回映射后的外部ID，失败返回(UINT32)-1
 */
STATIC UINT32 MapIdUp(UidGidMap *map, UINT32 id)
{
    UINT32 extents = map->extentCount;  // 获取映射范围数量
    if (extents > UID_GID_MAP_MAX_EXTENTS) {  // 映射范围数量超过最大值
        return (UINT32)-1;  // 返回映射失败
    }

    // 查找ID对应的映射范围
    UidGidExtent *extent = MapIdUpBase(extents, map, id);
    if (extent != NULL) {  // 找到映射范围
        // 计算并返回映射后的ID
        return ((id - extent->lowerFirst) + extent->first);
    }

    return (UINT32)-1;  // 未找到映射范围，返回映射失败
}

/**
 * @brief 将内核UID转换为用户容器UID
 * 
 * @param userContainer 用户容器指针
 * @param kuid 内核UID
 * @return UINT32 转换后的用户容器UID
 */
UINT32 FromKuid(UserContainer *userContainer, UINT32 kuid)
{
    return MapIdUp(&userContainer->uidMap, kuid);  // 使用UID映射表进行转换
}

/**
 * @brief 将内核GID转换为用户容器GID
 * 
 * @param userContainer 用户容器指针
 * @param kgid 内核GID
 * @return UINT32 转换后的用户容器GID
 */
UINT32 FromKgid(UserContainer *userContainer, UINT32 kgid)
{
    return MapIdUp(&userContainer->gidMap, kgid);  // 使用GID映射表进行转换
}

/**
 * @brief 将内核UID安全转换为用户容器UID（带溢出处理）
 * 
 * @param userContainer 用户容器指针
 * @param kuid 内核UID
 * @return UINT32 转换后的用户容器UID，失败返回DEFAULT_OVERFLOWUID
 */
UINT32 OsFromKuidMunged(UserContainer *userContainer, UINT32 kuid)
{
    UINT32 uid = FromKuid(userContainer, kuid);  // 转换内核UID
    if (uid == (UINT32)-1) {  // 转换失败
        uid = DEFAULT_OVERFLOWUID;  // 使用默认溢出UID
    }
    return uid;
}

/**
 * @brief 将内核GID安全转换为用户容器GID（带溢出处理）
 * 
 * @param userContainer 用户容器指针
 * @param kgid 内核GID
 * @return UINT32 转换后的用户容器GID，失败返回DEFAULT_OVERFLOWGID
 */
UINT32 OsFromKgidMunged(UserContainer *userContainer, UINT32 kgid)
{
    UINT32 gid = FromKgid(userContainer, kgid);  // 转换内核GID
    if (gid == (UINT32)-1) {  // 转换失败
        gid = DEFAULT_OVERFLOWGID;  // 使用默认溢出GID
    }
    return gid;
}

/**
 * @brief 根据ID范围查找对应的UID/GID映射范围（基础函数）
 * 
 * @param extents 映射范围数量
 * @param map UID/GID映射结构体指针
 * @param id 需要查找的起始ID
 * @param count ID范围数量
 * @return UidGidExtent* 成功返回映射范围指针，失败返回NULL
 */
STATIC UidGidExtent *MapIdRangeDownBase(UINT32 extents, UidGidMap *map, UINT32 id, UINT32 count)
{
    UINT32 idx;
    UINT32 first;  // 映射范围起始ID
    UINT32 last;   // 映射范围结束ID
    UINT32 id2;

    id2 = id + count - 1;
    for (idx = 0; idx < extents; idx++) {
        first = map->extent[idx].first;  // 获取当前范围的起始ID
        last = first + map->extent[idx].count - 1;  // 计算当前范围的结束ID
        // 检查目标ID范围是否完全包含在当前映射范围内
        if ((id >= first && id <= last) && (id2 >= first && id2 <= last)) {
            return &map->extent[idx];  // 返回找到的映射范围
        }
    }
    return NULL;  // 未找到映射范围
}

/**
 * @brief 将外部ID范围映射为内部ID范围
 * 
 * @param map UID/GID映射结构体指针
 * @param id 外部起始ID
 * @param count ID数量
 * @return UINT32 成功返回映射后的内部起始ID，失败返回(UINT32)-1
 */
STATIC UINT32 MapIdRangeDown(UidGidMap *map, UINT32 id, UINT32 count)
{
    UINT32 extents = map->extentCount;  // 获取映射范围数量
    if (extents > UID_GID_MAP_MAX_EXTENTS) {  // 映射范围数量超过最大值
        return (UINT32)-1;  // 返回映射失败
    }

    // 查找ID范围对应的映射范围
    UidGidExtent *extent = MapIdRangeDownBase(extents, map, id, count);
    if (extent != NULL) {  // 找到映射范围
        // 计算并返回映射后的起始ID
        return ((id - extent->first) + extent->lowerFirst);
    }

    return (UINT32)-1;  // 未找到映射范围，返回映射失败
}

/**
 * @brief 将外部ID映射为内部ID
 * 
 * @param map UID/GID映射结构体指针
 * @param id 外部ID
 * @return UINT32 成功返回映射后的内部ID，失败返回(UINT32)-1
 */
STATIC UINT32 MapIdDown(UidGidMap *map, UINT32 id)
{
    return MapIdRangeDown(map, id, 1);  // 调用范围映射函数，数量为1
}

/**
 * @brief 将用户容器UID转换为内核UID
 * 
 * @param userContainer 用户容器指针
 * @param uid 用户容器UID
 * @return UINT32 转换后的内核UID
 */
UINT32 OsMakeKuid(UserContainer *userContainer, UINT32 uid)
{
    return MapIdDown(&userContainer->uidMap, uid);  // 使用UID映射表进行转换
}

/**
 * @brief 将用户容器GID转换为内核GID
 * 
 * @param userContainer 用户容器指针
 * @param gid 用户容器GID
 * @return UINT32 转换后的内核GID
 */
UINT32 OsMakeKgid(UserContainer *userContainer, UINT32 gid)
{
    return MapIdDown(&userContainer->gidMap, gid);  // 使用GID映射表进行转换
}

/**
 * @brief 向UID/GID映射表中插入新的映射范围
 * 
 * @param idMap UID/GID映射结构体指针
 * @param extent 要插入的映射范围结构体
 * @return INT32 成功返回0，失败返回-EINVAL
 */
STATIC INT32 InsertExtent(UidGidMap *idMap, UidGidExtent *extent)
{
    // 检查映射范围数量是否超过最大值
    if (idMap->extentCount > UID_GID_MAP_MAX_EXTENTS) {
        return -EINVAL;  // 返回参数无效错误
    }

    // 将新映射范围插入到映射表末尾
    UidGidExtent *dest = &idMap->extent[idMap->extentCount];
    *dest = *extent;  // 复制映射范围数据
    idMap->extentCount++;  // 增加映射范围计数

    return 0;  // 插入成功
}

/**
 * @brief 检查新映射范围是否与现有映射范围重叠
 * 
 * @param idMap UID/GID映射结构体指针
 * @param extent 要检查的新映射范围
 * @return BOOL 重叠返回TRUE，不重叠返回FALSE
 */
STATIC BOOL MappingsOverlap(UidGidMap *idMap, UidGidExtent *extent)
{
    UINT32 upperFirst = extent->first;  // 新范围的外部起始ID
    UINT32 lowerFirst = extent->lowerFirst;  // 新范围的内部起始ID
    UINT32 upperLast = upperFirst + extent->count - 1;  // 新范围的外部结束ID
    UINT32 lowerLast = lowerFirst + extent->count - 1;  // 新范围的内部结束ID
    INT32 idx;

    // 遍历所有现有映射范围检查重叠
    for (idx = 0; idx < idMap->extentCount; idx++) {
        if (idMap->extentCount > UID_GID_MAP_MAX_EXTENTS) {  // 映射范围数量超过最大值
            return TRUE;  // 视为重叠
        }
        UidGidExtent *prev = &idMap->extent[idx];  // 获取现有映射范围
        UINT32 prevUpperFirst = prev->first;  // 现有范围的外部起始ID
        UINT32 prevLowerFirst = prev->lowerFirst;  // 现有范围的内部起始ID
        UINT32 prevUpperLast = prevUpperFirst + prev->count - 1;  // 现有范围的外部结束ID
        UINT32 prevLowerLast = prevLowerFirst + prev->count - 1;  // 现有范围的内部结束ID

        // 检查外部ID范围是否重叠
        if ((prevUpperFirst <= upperLast) && (prevUpperLast >= upperFirst)) {
            return TRUE;  // 外部范围重叠
        }

        // 检查内部ID范围是否重叠
        if ((prevLowerFirst <= lowerLast) && (prevLowerLast >= lowerFirst)) {
            return TRUE;  // 内部范围重叠
        }
    }
    return FALSE;  // 无重叠
}

/**
 * @brief 跳过字符串中的空格字符
 * 
 * @param str 输入字符串指针
 * @return CHAR* 指向第一个非空格字符的指针
 */
STATIC CHAR *SkipSpaces(const CHAR *str)
{
    while (isspace(*str)) {  // 循环跳过所有空格字符
        ++str;
    }

    return (CHAR *)str;  // 返回非空格字符指针
}

/**
 * @brief 将字符串转换为无符号整数
 * 
 * @param str 输入字符串指针
 * @param endp 输出参数，指向转换后剩余字符串的指针
 * @param base 转换基数(0表示自动检测)
 * @return UINTPTR 转换后的无符号整数
 */
STATIC UINTPTR StrToUInt(const CHAR *str, CHAR **endp, UINT32 base)
{
    unsigned long result = 0;
    unsigned long value;

    if (*str == '0') {  // 处理以0开头的数字
        str++;
        // 检测十六进制(0x前缀)
        if ((*str == 'x') && isxdigit(str[1])) {
            base = HEX;  // 设置基数为十六进制
            str++;
        }
        if (!base) {  // 基数未设置且以0开头
            base = OCT;  // 默认使用八进制
        }
    }
    if (!base) {  // 基数仍未设置
        base = DEC;  // 默认使用十进制
    }
    // 转换数字字符
    while (isxdigit(*str) && (value = isdigit(*str) ? *str - '0' : (islower(*str) ?
        toupper(*str) : *str) - 'A' + DEC) < base) {
        result = result * base + value;  // 累加计算结果
        str++;
    }
    if (endp != NULL) {  // 设置剩余字符串指针
        *endp = (CHAR *)str;
    }
    return result;
}

/**
 * @brief 解析位置数据字符串为UID/GID映射范围
 * 
 * @param pos 位置数据字符串
 * @param extent 输出参数，存储解析后的映射范围
 * @return INT32 成功返回LOS_OK，失败返回-EINVAL
 */
STATIC INT32 ParsePosData(CHAR *pos, UidGidExtent *extent)
{
    INT32 ret = -EINVAL;
    pos = SkipSpaces(pos);  // 跳过前导空格
    // 解析外部起始ID
    extent->first = StrToUInt(pos, &pos, DEC);
    if (!isspace(*pos)) {  // 检查是否有分隔空格
        return ret;  // 格式错误
    }

    pos = SkipSpaces(pos);  // 跳过分隔空格
    // 解析内部起始ID
    extent->lowerFirst = StrToUInt(pos, &pos, DEC);
    if (!isspace(*pos)) {  // 检查是否有分隔空格
        return ret;  // 格式错误
    }

    pos = SkipSpaces(pos);  // 跳过分隔空格
    // 解析ID数量
    extent->count = StrToUInt(pos, &pos, DEC);
    if (*pos && !isspace(*pos)) {  // 检查是否有无效字符
        return ret;  // 格式错误
    }

    pos = SkipSpaces(pos);  // 跳过尾随空格
    if (*pos != '\0') {  // 检查是否到达字符串末尾
        return ret;  // 格式错误
    }
    return LOS_OK;  // 解析成功
}

/**
 * @brief 解析用户数据字符串为UID/GID映射表
 * 
 * @param kbuf 用户数据缓冲区
 * @param extent 临时存储解析出的映射范围
 * @param newMap 输出参数，存储解析后的映射表
 * @return INT32 成功返回0，失败返回-EINVAL
 */
STATIC INT32 ParseUserData(CHAR *kbuf, UidGidExtent *extent, UidGidMap *newMap)
{
    INT32 ret = -EINVAL;
    CHAR *pos = NULL;
    CHAR *nextLine = NULL;  // 下一行字符串指针

    // 按行解析用户数据
    for (pos = kbuf; pos != NULL; pos = nextLine) {
        nextLine = strchr(pos, '\n');  // 查找行结束符
        if (nextLine != NULL) {  // 处理行结束符
            *nextLine = '\0';  // 将换行符替换为字符串结束符
            nextLine++;  // 移动到下一行
            if (*nextLine == '\0') {  // 下一行是空行
                nextLine = NULL;  // 标记为无下一行
            }
        }

        // 解析当前行的位置数据
        if (ParsePosData(pos, extent) != LOS_OK) {
            return ret;  // 解析失败
        }

        // 检查解析出的ID是否有效
        if ((extent->first == (UINT32)-1) || (extent->lowerFirst == (UINT32)-1)) {
            return ret;  // ID无效
        }

        // 检查ID范围是否溢出
        if ((extent->first + extent->count) <= extent->first) {
            return ret;  // 外部ID范围溢出
        }

        if ((extent->lowerFirst + extent->count) <= extent->lowerFirst) {
            return ret;  // 内部ID范围溢出
        }

        // 检查映射范围是否重叠
        if (MappingsOverlap(newMap, extent)) {
            return ret;  // 映射范围重叠
        }

        // 检查是否还有空间插入新映射范围
        if ((newMap->extentCount + 1) == UID_GID_MAP_MAX_EXTENTS && (nextLine != NULL)) {
            return ret;  // 映射范围数量超限
        }

        // 插入新映射范围
        ret = InsertExtent(newMap, extent);
        if (ret < 0) {
            return ret;  // 插入失败
        }
        ret = 0;  // 标记当前行解析成功
    }

    if (newMap->extentCount == 0) {  // 未解析到任何映射范围
        return -EINVAL;  // 返回错误
    }

    return ret;  // 解析成功
}

/**
 * @brief 将新映射表的ID范围映射到父容器的ID空间
 * 
 * @param newMap 新映射表指针
 * @param parentMap 父容器映射表指针
 * @return INT32 成功返回0，失败返回-EPERM
 */
STATIC INT32 ParentMapIdRange(UidGidMap *newMap, UidGidMap *parentMap)
{
    UINT32 idx;
    INT32 ret = -EPERM;
    // 遍历新映射表的所有映射范围
    for (idx = 0; idx < newMap->extentCount; idx++) {
        if (newMap->extentCount > UID_GID_MAP_MAX_EXTENTS) {  // 映射范围数量超限
            return ret;  // 返回权限错误
        }

        UidGidExtent *extent = &newMap->extent[idx];  // 获取当前映射范围
        // 将新范围的内部ID映射到父容器的内部ID空间
        UINT32 lowerFirst = MapIdRangeDown(parentMap, extent->lowerFirst, extent->count);
        if (lowerFirst == (UINT32) -1) {  // 映射失败
            return ret;  // 返回权限错误
        }

        extent->lowerFirst = lowerFirst;  // 更新映射范围的内部起始ID
    }
    return 0;  // 映射成功
}

/**
 * @brief 写入用户容器的UID/GID映射表
 * 
 * @param fp 进程文件结构体指针
 * @param kbuf 输入数据缓冲区
 * @param count 数据长度
 * @param capSetid 权限检查标志
 * @param map 要写入的映射表指针
 * @param parentMap 父容器映射表指针
 * @return INT32 成功返回写入的字节数，失败返回-EPERM
 */
INT32 OsUserContainerMapWrite(struct ProcFile *fp, CHAR *kbuf, size_t count,
                              INT32 capSetid, UidGidMap *map, UidGidMap *parentMap)
{
    UidGidMap newMap = {0};  // 新映射表
    UidGidExtent extent;     // 临时映射范围
    INT32 ret;

    if (map->extentCount != 0) {  // 映射表已初始化
        return -EPERM;  // 不允许重复初始化，返回权限错误
    }

    if (!IsCapPermit(capSetid)) {  // 检查权限
        return -EPERM;  // 权限不足，返回权限错误
    }

    // 解析用户输入数据为映射表
    ret = ParseUserData(kbuf, &extent, &newMap);
    if (ret < 0) {
        return -EPERM;  // 解析失败，返回权限错误
    }

    // 将新映射表映射到父容器的ID空间
    ret = ParentMapIdRange(&newMap, parentMap);
    if (ret < 0) {
        return -EPERM;  // 映射失败，返回权限错误
    }

    // 将新映射表复制到目标映射表
    if (newMap.extentCount <= UID_GID_MAP_MAX_EXTENTS) {
        size_t mapSize = newMap.extentCount * sizeof(newMap.extent[0]);  // 计算映射范围数据大小
        ret = memcpy_s(map->extent, sizeof(map->extent), newMap.extent, mapSize);  // 复制映射范围数据
        if (ret != EOK) {  // 复制失败
            return -EPERM;  // 返回权限错误
        }
    }

    map->extentCount = newMap.extentCount;  // 更新映射范围数量
    return count;  // 返回写入的字节数
}

/**
 * @brief 获取用户容器数量
 * 
 * @return UINT32 当前用户容器总数
 */
UINT32 OsGetUserContainerCount(VOID)
{
    return g_currentUserContainerNum;  // 返回当前用户容器数量
}
#endif
