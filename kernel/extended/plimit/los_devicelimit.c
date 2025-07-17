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

#ifdef LOSCFG_KERNEL_DEV_PLIMIT
#include "los_seq_buf.h"
#include "los_bitmap.h"
#include "los_process_pri.h"
#include "los_devicelimit.h"
// 类型字符长度（1字节）  
#define TYPE_CHAR_LEN            (1)
// 设备名称前缀空格数（1个空格）  
#define DEVICE_NAME_PREFIX_SPACE (1)
// 设备访问权限最大长度（3个字符）  
#define DEVICE_ACCESS_MAXLEN     (3)
// 缓冲区分隔符判定阈值（5，用于空格判断）  
#define BUF_SEPARATOR            (5)

// 全局进程设备限制器实例  
STATIC ProcDevLimit *g_procDevLimit = NULL;

/**
 * @brief 设备限制器初始化函数
 * @param limit 设备限制器内存地址
 * @note 初始化设备限制器默认行为为允许访问，并初始化访问控制链表
 */
VOID OsDevLimitInit(UINTPTR limit)
{
    ProcDevLimit *deviceLimit = (ProcDevLimit *)limit;  // 类型转换为设备限制器指针  
    deviceLimit->behavior = DEVLIMIT_DEFAULT_ALLOW;    // 默认行为：允许所有设备访问  
    LOS_ListInit(&(deviceLimit->accessList));          // 初始化访问控制规则链表  
    g_procDevLimit = deviceLimit;                      // 保存全局设备限制器指针  
}

/**
 * @brief 分配设备限制器内存
 * @return 成功返回设备限制器指针，失败返回NULL
 * @note 使用系统内存池1分配内存，并初始化链表和默认行为
 */
VOID *OsDevLimitAlloc(VOID)
{
    // 分配ProcDevLimit结构体大小的内存  
    ProcDevLimit *plimit = (ProcDevLimit *)LOS_MemAlloc(m_aucSysMem1, sizeof(ProcDevLimit));
    if (plimit == NULL) {                              // 内存分配失败检查  
        return NULL;
    }
    (VOID)memset_s(plimit, sizeof(ProcDevLimit), 0, sizeof(ProcDevLimit));  // 清零结构体内存  
    LOS_ListInit(&(plimit->accessList));               // 初始化访问控制规则链表  
    plimit->behavior = DEVLIMIT_DEFAULT_NONE;          // 默认行为：无（需显式配置规则）  
    return (VOID *)plimit;
}

/**
 * @brief 释放设备访问控制规则链表
 * @param devLimit 设备限制器指针
 * @note 遍历并释放accessList中的所有DevAccessItem节点
 */
STATIC VOID DevAccessListDelete(ProcDevLimit *devLimit)
{
    DevAccessItem *delItem = NULL;
    DevAccessItem *tmpItem = NULL;
    // 安全遍历访问控制链表（防止删除节点时断链）  
    LOS_DL_LIST_FOR_EACH_ENTRY_SAFE(delItem, tmpItem, &devLimit->accessList, DevAccessItem, list) {
        LOS_ListDelete(&delItem->list);                 // 从链表中删除节点  
        (VOID)LOS_MemFree(m_aucSysMem1, (VOID *)delItem);  // 释放节点内存  
    }
}

/**
 * @brief 释放设备限制器内存
 * @param limit 设备限制器内存地址
 * @note 先释放访问控制链表，再释放限制器本身内存
 */
VOID OsDevLimitFree(UINTPTR limit)
{
    ProcDevLimit *devLimit = (ProcDevLimit *)limit;    // 类型转换为设备限制器指针  
    if (devLimit == NULL) {                            // 空指针检查  
        return;
    }

    DevAccessListDelete(devLimit);                     // 释放访问控制规则链表  
    (VOID)LOS_MemFree(m_aucSysMem1, devLimit);         // 释放设备限制器内存  
}

/**
 * @brief 复制设备访问控制规则
 * @param devLimitDest 目标限制器指针
 * @param devLimitSrc 源限制器指针
 * @return 成功返回LOS_OK，失败返回ENOMEM
 * @note 深拷贝源限制器的行为策略和所有访问控制规则
 */
STATIC UINT32 DevLimitCopyAccess(ProcDevLimit *devLimitDest, ProcDevLimit *devLimitSrc)
{
    DevAccessItem *tmpItem = NULL;
    INT32 itemSize = sizeof(DevAccessItem);            // DevAccessItem结构体大小  
    devLimitDest->behavior = devLimitSrc->behavior;    // 复制默认行为策略  
    // 遍历源限制器的访问控制链表  
    LOS_DL_LIST_FOR_EACH_ENTRY(tmpItem, &devLimitSrc->accessList, DevAccessItem, list) {
        // 为新规则项分配内存  
        DevAccessItem *newItem = (DevAccessItem *)LOS_MemAlloc(m_aucSysMem1, itemSize);
        if (newItem == NULL) {                         // 内存分配失败检查  
            return ENOMEM;
        }
        // 拷贝规则项内容（包括设备类型、权限和名称）  
        (VOID)memcpy_s(newItem, sizeof(DevAccessItem), tmpItem, sizeof(DevAccessItem));
        LOS_ListTailInsert(&devLimitDest->accessList, &newItem->list);  // 添加到目标链表尾部  
    }
    return LOS_OK;
}

/**
 * @brief 复制设备限制器
 * @param dest 目标限制器地址
 * @param src 源限制器地址
 * @note 复制访问控制规则并建立父子继承关系
 */
VOID OsDevLimitCopy(UINTPTR dest, UINTPTR src)
{
    ProcDevLimit *devLimitDest = (ProcDevLimit *)dest;
    ProcDevLimit *devLimitSrc = (ProcDevLimit *)src;
    (VOID)DevLimitCopyAccess(devLimitDest, devLimitSrc);  // 复制访问控制规则  
    devLimitDest->parent = (ProcDevLimit *)src;          // 设置父限制器指针（继承关系）  
}

/**
 * @brief 判断字符是否为空白字符
 * @param c 待判断字符
 * @return 是空白字符返回TRUE，否则返回FALSE
 * @note 空白字符包括空格和ASCII码小于\t+5的控制字符
 */
STATIC INLINE INT32 IsSpace(INT32 c)
{
    return (c == ' ' || (unsigned)c - '\t' < BUF_SEPARATOR);  // 空白字符判定逻辑  
}

/**
 * @brief 解析设备访问规则字符串
 * @param buf 输入字符串缓冲区
 * @param item 输出设备访问规则项
 * @return 成功返回LOS_OK，失败返回EINVAL
 * @note 格式：[a|b|c] [设备名] [r][w][m]，如"c /dev/tty rw"
 */
STATIC UINT32 ParseItemAccess(const CHAR *buf, DevAccessItem *item)
{
    switch (*buf) {                                    // 解析设备类型（首字符）  
        case 'a':
            item->type = DEVLIMIT_DEV_ALL;             // 所有设备类型  
            return LOS_OK;
        case 'b':
            item->type = DEVLIMIT_DEV_BLOCK;           // 块设备类型  
            break;
        case 'c':
            item->type = DEVLIMIT_DEV_CHAR;            // 字符设备类型  
            break;
        default:
            return EINVAL;                             // 无效设备类型  
    }
    buf += DEVICE_NAME_PREFIX_SPACE;                   // 跳过设备类型后的空格  
    if (!IsSpace(*buf)) {                              // 验证空格分隔符  
        return EINVAL;
    }
    buf += DEVICE_NAME_PREFIX_SPACE;                   // 跳过空格分隔符  

    // 解析设备名称（直到下一个空格）  
    for (INT32 count = 0; count < sizeof(item->name) - 1; count++) {
        if (IsSpace(*buf)) {                           // 遇到空格结束解析  
            break;
        }
        item->name[count] = *buf;                      // 复制设备名称字符  
        buf += TYPE_CHAR_LEN;                          // 移动到下一个字符  
    }
    if (!IsSpace(*buf)) {                              // 验证名称后的空格分隔符  
        return EINVAL;
    }

    buf += DEVICE_NAME_PREFIX_SPACE;                   // 跳过空格分隔符  
    // 解析访问权限（r:读, w:写, m:创建节点）  
    for (INT32 i = 0; i < DEVICE_ACCESS_MAXLEN; i++) {
        switch (*buf) {
            case 'r':
                item->access |= DEVLIMIT_ACC_READ;     // 设置读权限  
                break;
            case 'w':
                item->access |= DEVLIMIT_ACC_WRITE;    // 设置写权限  
                break;
            case 'm':
                item->access |= DEVLIMIT_ACC_MKNOD;    // 设置创建节点权限  
                break;
            case '\n':
            case '\0':
                i = DEVICE_ACCESS_MAXLEN;              // 结束解析  
                break;
            default:
                return EINVAL;                         // 无效权限字符  
        }
        buf += TYPE_CHAR_LEN;                          // 移动到下一个字符  
    }
    return LOS_OK;
}

/**
 * @brief 判断父限制器是否允许所有访问
 * @param parent 父限制器指针
 * @return 允许返回TRUE，否则返回FALSE
 * @note 父限制器为NULL时默认允许所有访问
 */
STATIC BOOL DevLimitMayAllowAll(ProcDevLimit *parent)
{
    if (parent == NULL) {                              // 无父限制器（根节点）  
        return TRUE;
    }
    return (parent->behavior == DEVLIMIT_DEFAULT_ALLOW);  // 父限制器默认允许  
}

/**
 * @brief 检查设备限制器是否有子进程
 * @param procLimitSet 进程限制器集合
 * @param devLimit 设备限制器指针
 * @return 有子进程返回TRUE，否则返回FALSE
 * @note 遍历子进程限制器集合，检查是否有继承设备限制的子进程
 */
STATIC BOOL DevLimitHasChildren(ProcLimitSet *procLimitSet, ProcDevLimit *devLimit)
{
    ProcLimitSet *parent = procLimitSet;
    ProcLimitSet *childProcLimitSet = NULL;
    if (devLimit == NULL) {                            // 空指针检查  
        return FALSE;
    }

    // 遍历进程限制器的子进程列表  
    LOS_DL_LIST_FOR_EACH_ENTRY(childProcLimitSet, &(procLimitSet->childList), ProcLimitSet, childList) {
        if (childProcLimitSet == NULL) {               // 跳过空节点  
            continue;
        }
        if (childProcLimitSet->parent != parent) {     // 验证父子关系  
            continue;
        }
        // 检查子进程是否启用了设备限制  
        if (!((childProcLimitSet->mask) & BIT(PROCESS_LIMITER_ID_DEV))) {
            continue;
        }
        return TRUE;                                  // 找到符合条件的子进程  
    }
    return FALSE;
}

/**
 * @brief 处理"全部允许/拒绝"访问规则
 * @param procLimitSet 进程限制器集合
 * @param devLimit 当前设备限制器
 * @param devParentLimit 父设备限制器
 * @param filetype 规则类型（DEVLIMIT_ALLOW/DEVLIMIT_DENY）
 * @return 成功返回LOS_OK，失败返回EINVAL/EPERM
 * @note 有子进程时不允许修改，ALLOW规则需父限制器允许继承
 */
STATIC UINT32 DealItemAllAccess(ProcLimitSet *procLimitSet, ProcDevLimit *devLimit,
                                ProcDevLimit *devParentLimit, INT32 filetype)
{
    switch (filetype) {
        case DEVLIMIT_ALLOW: {                         // 全部允许规则  
            if (DevLimitHasChildren(procLimitSet, devLimit)) {  // 有子进程时禁止修改  
                return EINVAL;
            }
            if (!DevLimitMayAllowAll(devParentLimit)) {  // 父限制器不允许继承  
                return EPERM;
            }
            DevAccessListDelete(devLimit);             // 清空现有访问规则  
            devLimit->behavior = DEVLIMIT_DEFAULT_ALLOW;  // 设置默认允许行为  
            if (devParentLimit == NULL) {              // 无父限制器时无需复制规则  
                break;
            }
            DevLimitCopyAccess(devLimit, devParentLimit);  // 复制父限制器的访问规则  
            break;
        }
        case DEVLIMIT_DENY: {                          // 全部拒绝规则  
            if (DevLimitHasChildren(procLimitSet, devLimit)) {  // 有子进程时禁止修改  
                return EINVAL;
            }
            DevAccessListDelete(devLimit);             // 清空现有访问规则  
            devLimit->behavior = DEVLIMIT_DEFAULT_DENY;   // 设置默认拒绝行为  
            break;
        }
        default:
            return EINVAL;                             // 无效规则类型  
    }
    return LOS_OK;
}
/**
 * @brief 部分匹配设备访问规则项
 * @param list 访问规则链表
 * @param item 待匹配规则项
 * @return 匹配成功返回TRUE，否则返回FALSE
 * @note 支持通配符'*'匹配任意设备名称，权限需完全包含
 */
STATIC BOOL DevLimitMatchItemPartial(LOS_DL_LIST *list, DevAccessItem *item)
{
    if ((list == NULL) || (item == NULL)) {  // 参数合法性检查  
        return FALSE;
    }
    if (LOS_ListEmpty(list)) {               // 空链表快速返回  
        return FALSE;
    }
    DevAccessItem *walk = NULL;
    // 遍历访问规则链表  
    LOS_DL_LIST_FOR_EACH_ENTRY(walk, list, DevAccessItem, list) {
        if (item->type != walk->type) {      // 设备类型不匹配则跳过  
            continue;
        }
        // 名称匹配规则：支持通配符'*'或完全匹配  
        if ((strcmp(walk->name, "*") != 0) && (strcmp(item->name, "*") != 0)
        && (strcmp(walk->name, item->name) != 0)) {
            continue;
        }
        // 权限检查：待匹配权限必须是规则权限的子集  
        if (!(item->access & ~(walk->access))) {
            return TRUE;
        }
    }
    return FALSE;
}

/**
 * @brief 检查父限制器是否允许删除规则项
 * @param devParentLimit 父设备限制器
 * @param item 待删除规则项
 * @return 允许删除返回TRUE，否则返回FALSE
 * @note 防止删除父限制器中存在的规则项
 */
STATIC BOOL DevLimitParentAllowsRmItem(ProcDevLimit *devParentLimit, DevAccessItem *item)
{
    if (devParentLimit == NULL) {            // 无父限制器时允许删除  
        return TRUE;
    }
     /* 确保不删除父限制器中已存在的规则项 */
    return !DevLimitMatchItemPartial(&devParentLimit->accessList, item);  // 检查父限制器规则  
}

/**
 * @brief 完全匹配设备访问规则项
 * @param list 访问规则链表
 * @param item 待匹配规则项
 * @return 匹配成功返回TRUE，否则返回FALSE
 * @note 不支持通配符'*'作为待匹配项名称，权限需完全包含
 */
STATIC BOOL DevLimitMatchItem(LOS_DL_LIST *list, DevAccessItem *item)
{
    if ((list == NULL) || (item == NULL)) {  // 参数合法性检查  
        return FALSE;
    }
    if (LOS_ListEmpty(list)) {               // 空链表快速返回  
        return FALSE;
    }
    DevAccessItem *walk = NULL;
    // 遍历访问规则链表  
    LOS_DL_LIST_FOR_EACH_ENTRY(walk, list, DevAccessItem, list) {
        if (item->type != walk->type) {      // 设备类型不匹配则跳过  
            continue;
        }
        // 名称匹配规则：支持通配符'*'或完全匹配  
        if ((strcmp(walk->name, "*") != 0) && (strcmp(walk->name, item->name) != 0)) {
            continue;
        }
        // 权限检查：待匹配权限必须是规则权限的子集  
        if (!(item->access & ~(walk->access))) {
            return TRUE;
        }
    }
    return FALSE;
}

/**
 * @brief 验证子限制器的新规则项权限
 * @param parent 父限制器
 * @param item 新规则项
 * @param currBehavior 当前限制器行为
 * @return 验证通过返回TRUE，否则返回FALSE
 * @note 确保子限制器权限不超过父限制器
 */
STATIC BOOL DevLimitVerifyNewItem(ProcDevLimit *parent, DevAccessItem *item, INT32 currBehavior)
{
    if (parent == NULL) {                    // 无父限制器时直接通过  
        return TRUE;
    }

    if (parent->behavior == DEVLIMIT_DEFAULT_ALLOW) {  // 父限制器默认允许  
        if (currBehavior == DEVLIMIT_DEFAULT_ALLOW) {  // 子限制器也默认允许  
            return TRUE;
        }
        // 检查是否与父限制器规则冲突  
        return !DevLimitMatchItemPartial(&parent->accessList, item);
    }
    // 父限制器默认拒绝时需显式匹配规则  
    return DevLimitMatchItem(&parent->accessList, item);
}

/**
 * @brief 检查父限制器是否允许添加规则项
 * @param devParentLimit 父设备限制器
 * @param item 待添加规则项
 * @param currBehavior 当前限制器行为
 * @return 允许添加返回TRUE，否则返回FALSE
 */
STATIC BOOL DevLimitParentAllowsAddItem(ProcDevLimit *devParentLimit, DevAccessItem *item, INT32 currBehavior)
{
    return DevLimitVerifyNewItem(devParentLimit, item, currBehavior);  // 复用验证逻辑  
}

/**
 * @brief 从访问规则链表中移除规则项
 * @param devLimit 设备限制器
 * @param item 待移除规则项
 * @note 仅移除指定类型、名称和权限的规则，权限完全清空时删除节点
 */
STATIC VOID DevLimitAccessListRm(ProcDevLimit *devLimit, DevAccessItem *item)
{
    if ((item == NULL) || (devLimit == NULL)) {  // 参数合法性检查  
        return;
    }
    DevAccessItem *walk, *tmp = NULL;
    // 安全遍历访问规则链表（防止删除节点时断链）  
    LOS_DL_LIST_FOR_EACH_ENTRY_SAFE(walk, tmp, &devLimit->accessList, DevAccessItem, list) {
        if (walk->type != item->type) {       // 设备类型不匹配则跳过  
            continue;
        }
        if (strcmp(walk->name, item->name) != 0) {  // 设备名称不匹配则跳过  
            continue;
        }
        walk->access &= ~item->access;        // 移除指定权限位  
        if (!walk->access) {                  // 权限位为空时删除节点  
            LOS_ListDelete(&walk->list);      // 从链表中删除节点  
            (VOID)LOS_MemFree(m_aucSysMem1, (VOID *)walk);  // 释放节点内存  
        }
    }
}

/**
 * @brief 向访问规则链表添加规则项
 * @param devLimit 设备限制器
 * @param item 待添加规则项
 * @return 成功返回LOS_OK，失败返回ENOMEM
 * @note 若存在同类型同名称规则则合并权限，否则添加新节点
 */
STATIC UINT32 DevLimitAccessListAdd(ProcDevLimit *devLimit, DevAccessItem *item)
{
    if ((item == NULL) || (devLimit == NULL)) {  // 参数合法性检查  
        return ENOMEM;
    }

    DevAccessItem *walk = NULL;
    // 分配新规则项内存  
    DevAccessItem *newItem = (DevAccessItem *)LOS_MemAlloc(m_aucSysMem1, sizeof(DevAccessItem));
    if (newItem == NULL) {                    // 内存分配失败  
        return ENOMEM;
    }
    (VOID)memcpy_s(newItem, sizeof(DevAccessItem), item, sizeof(DevAccessItem));  // 复制规则内容  
    // 遍历现有规则查找重复项  
    LOS_DL_LIST_FOR_EACH_ENTRY(walk, &devLimit->accessList, DevAccessItem, list) {
        if (walk->type != item->type) {       // 设备类型不匹配则跳过  
            continue;
        }
        if (strcmp(walk->name, item->name) != 0) {  // 设备名称不匹配则跳过  
            continue;
        }
        walk->access |= item->access;         // 合并权限位  
        (VOID)LOS_MemFree(m_aucSysMem1, (VOID *)newItem);  // 释放未使用的新节点  
        newItem = NULL;
    }

    if (newItem != NULL) {                    // 无重复项则添加新节点  
        LOS_ListTailInsert(&devLimit->accessList, &newItem->list);  // 添加到链表尾部  
    }
    return LOS_OK;
}

/**
 * @brief 重新验证活动规则项权限
 * @param devLimit 当前设备限制器
 * @param devParentLimit 父设备限制器
 * @note 移除所有父限制器不允许的规则项
 */
STATIC VOID DevLimitRevalidateActiveItems(ProcDevLimit *devLimit, ProcDevLimit *devParentLimit)
{
    if ((devLimit == NULL) || (devParentLimit == NULL)) {  // 参数合法性检查  
        return;
    }
    DevAccessItem *walK = NULL;
    DevAccessItem *tmp = NULL;
    // 安全遍历规则链表  
    LOS_DL_LIST_FOR_EACH_ENTRY_SAFE(walK, tmp, &devLimit->accessList, DevAccessItem, list) {
        // 检查父限制器是否允许该规则项  
        if (!DevLimitParentAllowsAddItem(devParentLimit, walK, devLimit->behavior)) {
            DevLimitAccessListRm(devLimit, walK);  // 移除不允许的规则项  
        }
    }
}

/**
 * @brief 将新规则项 propagate 到子限制器
 * @param procLimitSet 进程限制器集合
 * @param devLimit 当前设备限制器
 * @param item 新规则项
 * @return 成功返回LOS_OK，失败返回错误码
 * @note 根据父限制器行为更新子限制器规则
 */
STATIC UINT32 DevLimitPropagateItem(ProcLimitSet *procLimitSet, ProcDevLimit *devLimit, DevAccessItem *item)
{
    UINT32 ret = LOS_OK;
    ProcLimitSet *parent = procLimitSet;
    ProcLimitSet *childProcLimitSet = NULL;

    if ((procLimitSet == NULL) || (item == NULL)) {  // 参数合法性检查  
        return ENOMEM;
    }

    if (devLimit == NULL) {                   // 无当前限制器时直接返回  
        return LOS_OK;
    }

    // 遍历所有子进程限制器  
    LOS_DL_LIST_FOR_EACH_ENTRY(childProcLimitSet, &procLimitSet->childList, ProcLimitSet, childList) {
        if (childProcLimitSet == NULL) {      // 跳过空节点  
            continue;
        }
        if (childProcLimitSet->parent != parent) {  // 验证父子关系  
            continue;
        }
        // 检查子进程是否启用设备限制  
        if (!((childProcLimitSet->mask) & BIT(PROCESS_LIMITER_ID_DEV))) {
            continue;
        }
        // 获取子进程的设备限制器  
        ProcDevLimit *devLimitChild = (ProcDevLimit *)childProcLimitSet->limitsList[PROCESS_LIMITER_ID_DEV];
        // 父子限制器均为默认允许时添加规则，否则移除规则  
        if (devLimit->behavior == DEVLIMIT_DEFAULT_ALLOW &&
            devLimitChild->behavior == DEVLIMIT_DEFAULT_ALLOW) {
            ret = DevLimitAccessListAdd(devLimitChild, item);  // 添加规则到子限制器  
        } else {
            DevLimitAccessListRm(devLimitChild, item);  // 从子限制器移除规则  
        }
        // 重新验证子限制器规则  
        DevLimitRevalidateActiveItems(devLimitChild, (ProcDevLimit *)parent->limitsList[PROCESS_LIMITER_ID_DEV]);
    }
    return ret;
}

/**
 * @brief 更新设备访问规则
 * @param procLimitSet 进程限制器集合
 * @param buf 规则字符串缓冲区
 * @param filetype 规则类型（DEVLIMIT_ALLOW/DEVLIMIT_DENY）
 * @return 成功返回LOS_OK，失败返回错误码
 * @note 线程安全函数，内部使用调度器锁保护临界区
 */
STATIC UINT32 DevLimitUpdateAccess(ProcLimitSet *procLimitSet, const CHAR *buf, INT32 filetype)
{
    UINT32 ret;
    UINT32 intSave;
    DevAccessItem item = {0};

    SCHEDULER_LOCK(intSave);                 // 加调度器锁，确保线程安全  
    // 获取当前进程的设备限制器  
    ProcDevLimit *devLimit = (ProcDevLimit *)(procLimitSet->limitsList[PROCESS_LIMITER_ID_DEV]);
    ProcDevLimit *devParentLimit = devLimit->parent;  // 获取父限制器  

    ret = ParseItemAccess(buf, &item);       // 解析规则字符串  
    if (ret != LOS_OK) {                     // 解析失败处理  
        SCHEDULER_UNLOCK(intSave);           // 解锁调度器  
        return ret;
    }
    if (item.type == DEVLIMIT_DEV_ALL) {     // 处理"全部设备"规则  
        ret = DealItemAllAccess(procLimitSet, devLimit, devParentLimit, filetype);
        SCHEDULER_UNLOCK(intSave);           // 解锁调度器  
        return ret;
    }
    switch (filetype) {
        case DEVLIMIT_ALLOW: {               // 允许规则处理  
            if (devLimit->behavior == DEVLIMIT_DEFAULT_ALLOW) {  // 当前限制器默认允许  
                // 检查父限制器是否允许移除该规则  
                if (!DevLimitParentAllowsRmItem(devParentLimit, &item)) {
                    SCHEDULER_UNLOCK(intSave);  // 解锁调度器  
                    return EPERM;
                }
                DevLimitAccessListRm(devLimit, &item);  // 从允许列表移除规则  
                break;
            }
            // 检查父限制器是否允许添加该规则  
            if (!DevLimitParentAllowsAddItem(devParentLimit, &item, devLimit->behavior)) {
                SCHEDULER_UNLOCK(intSave);   // 解锁调度器  
                return EPERM;
            }
            ret = DevLimitAccessListAdd(devLimit, &item);  // 添加规则到允许列表  
            break;
        }
        case DEVLIMIT_DENY: {                // 拒绝规则处理  
            if (devLimit->behavior == DEVLIMIT_DEFAULT_DENY) {  // 当前限制器默认拒绝  
                DevLimitAccessListRm(devLimit, &item);  // 从拒绝列表移除规则  
            } else {
                ret = DevLimitAccessListAdd(devLimit, &item);  // 添加规则到拒绝列表  
            }
            // 更新子进程的访问列表  
            ret = DevLimitPropagateItem(procLimitSet, devLimit, &item);
            break;
        }
        default:
            ret = EINVAL;                    // 无效规则类型  
            break;
    }
    SCHEDULER_UNLOCK(intSave);               // 解锁调度器  
    return ret;
}


/**
 * @brief 写入允许访问规则
 * @param plimit 进程限制器集合
 * @param buf 规则字符串
 * @param size 字符串长度（未使用）
 * @return 成功返回LOS_OK，失败返回错误码
 */
UINT32 OsDevLimitWriteAllow(ProcLimitSet *plimit, const CHAR *buf, UINT32 size)
{
    (VOID)size;                              // 未使用参数  
    return DevLimitUpdateAccess(plimit, buf, DEVLIMIT_ALLOW);  // 调用通用更新接口  
}

/**
 * @brief 写入拒绝访问规则
 * @param plimit 进程限制器集合
 * @param buf 规则字符串
 * @param size 字符串长度（未使用）
 * @return 成功返回LOS_OK，失败返回错误码
 */
UINT32 OsDevLimitWriteDeny(ProcLimitSet *plimit, const CHAR *buf, UINT32 size)
{
    (VOID)size;                              // 未使用参数  
    return DevLimitUpdateAccess(plimit, buf, DEVLIMIT_DENY);  // 调用通用更新接口  
}

/**
 * @brief 将权限值转换为权限字符串
 * @param accArray 输出权限字符串缓冲区
 * @param access 权限值（DEVLIMIT_ACC_*组合）
 * @note 生成格式为"rwm"的权限字符串，如读权限为"r"，读写权限为"rw"
 */
STATIC VOID DevLimitItemSetAccess(CHAR *accArray, INT16 access)
{
    INT32 index = 0;
    (VOID)memset_s(accArray, ACCLEN, 0, ACCLEN);  // 清空输出缓冲区  
    if (access & DEVLIMIT_ACC_READ) {         // 读权限  
        accArray[index] = 'r';
        index++;
    }
    if (access & DEVLIMIT_ACC_WRITE) {        // 写权限  
        accArray[index] = 'w';
        index++;
    }
    if (access & DEVLIMIT_ACC_MKNOD) {        // 创建节点权限  
        accArray[index] = 'm';
        index++;
    }
}

/**
 * @brief 将设备类型转换为字符表示
 * @param type 设备类型（DEVLIMIT_DEV_*）
 * @return 类型字符（'a'=所有, 'c'=字符, 'b'=块, 'X'=无效）
 */
STATIC CHAR DevLimitItemTypeToChar(INT16 type)
{
    switch (type) {
        case DEVLIMIT_DEV_ALL:
            return 'a';                       // 所有设备类型  
        case DEVLIMIT_DEV_CHAR:
            return 'c';                       // 字符设备  
        case DEVLIMIT_DEV_BLOCK:
            return 'b';                       // 块设备  
        default:
            break;
    }
    return 'X';                              // 无效类型  
}

/**
 * @brief 显示设备限制规则
 * @param devLimit 设备限制器
 * @param seqBuf 输出缓冲区
 * @return 成功返回LOS_OK，失败返回EINVAL
 * @note 格式：[类型] [设备名] [权限]，如"c /dev/tty rw"
 */
UINT32 OsDevLimitShow(ProcDevLimit *devLimit, struct SeqBuf *seqBuf)
{
    DevAccessItem *item = NULL;
    CHAR acc[ACCLEN];
    UINT32 intSave;

    if ((devLimit == NULL) || (seqBuf == NULL)) {  // 参数合法性检查  
        return EINVAL;
    }

    SCHEDULER_LOCK(intSave);                 // 加调度器锁，确保线程安全  
    if (devLimit->behavior == DEVLIMIT_DEFAULT_ALLOW) {  // 默认允许时显示全部权限  
        DevLimitItemSetAccess(acc, DEVLIMIT_ACC_MASK);  // 设置全部权限字符串  
        SCHEDULER_UNLOCK(intSave);           // 解锁调度器  
        LosBufPrintf(seqBuf, "%c %s %s\n", DevLimitItemTypeToChar(DEVLIMIT_DEV_ALL), "*", acc);  // 输出规则  
        return LOS_OK;
    }
    // 遍历访问规则链表并输出  
    LOS_DL_LIST_FOR_EACH_ENTRY(item, &devLimit->accessList, DevAccessItem, list) {
        DevLimitItemSetAccess(acc, item->access);  // 转换权限为字符串  
        LosBufPrintf(seqBuf, "%c %s %s\n", DevLimitItemTypeToChar(item->type), item->name, acc);  // 输出规则  
    }
    SCHEDULER_UNLOCK(intSave);               // 解锁调度器  
    return LOS_OK;
}

/**
 * @brief 将VNode类型转换为设备限制类型
 * @param vnodeType VNode类型（VNODE_TYPE_*）
 * @return 设备类型（DEVLIMIT_DEV_BLOCK/CHAR/0）
 */
STATIC INLINE INT16 ConversionDevType(INT32 vnodeType)
{
    INT16 type = 0;
    if (vnodeType == VNODE_TYPE_BLK) {       // 块设备  
        type = DEVLIMIT_DEV_BLOCK;
    } else if (vnodeType == VNODE_TYPE_CHR) {  // 字符设备  
        type = DEVLIMIT_DEV_CHAR;
    }
    return type;
}

/**
 * @brief 将文件打开标志转换为设备访问权限
 * @param flags 文件打开标志（O_*）
 * @return 访问权限（DEVLIMIT_ACC_*组合）
 */
STATIC INLINE INT16 ConversionDevAccess(INT32 flags)
{
    INT16 access = 0;
    if ((flags & O_ACCMODE) == O_RDONLY) {   // 只读模式  
        access |= DEVLIMIT_ACC_READ;
    }
    if (flags & O_WRONLY) {                  // 只写模式  
        access |= DEVLIMIT_ACC_WRITE;
    }
    if (flags & O_RDWR) {                    // 读写模式  
        access |= DEVLIMIT_ACC_WRITE | DEVLIMIT_ACC_READ;
    }
    if (flags & O_CREAT) {                   // 创建文件标志  
        access |= DEVLIMIT_ACC_MKNOD;
    }
    return access;
}

/**
 * @brief 检查设备访问权限
 * @param vnodeType 设备VNode类型
 * @param pathName 设备路径名
 * @param flags 访问标志（O_*）
 * @return 允许访问返回LOS_OK，拒绝返回EPERM，参数错误返回EINVAL
 * @note 核心权限检查函数，根据当前进程的设备限制规则判断访问权限
 */
UINT32 OsDevLimitCheckPermission(INT32 vnodeType, const CHAR *pathName, INT32 flags)
{
    BOOL matched = FALSE;
    DevAccessItem item = {0};
    LosProcessCB *run = OsCurrProcessGet();  // 获取当前进程控制块  
    if ((run == NULL) || (run->plimits == NULL)) {  // 无进程限制器时允许访问  
        return LOS_OK;
    }

    if (pathName == NULL) {                  // 路径名不能为空  
        return EINVAL;
    }

    // 获取当前进程的设备限制器  
    ProcDevLimit *devLimit = (ProcDevLimit *)run->plimits->limitsList[PROCESS_LIMITER_ID_DEV];

    item.type = ConversionDevType(vnodeType);  // 转换设备类型  
    item.access = ConversionDevAccess(flags);  // 转换访问权限  
    LOS_ListInit(&(item.list));              // 初始化链表节点（未使用）  
    (VOID)strncpy_s(item.name, PATH_MAX, pathName, PATH_MAX);  // 复制设备路径名  

    // 根据限制器行为判断匹配逻辑  
    if (devLimit->behavior == DEVLIMIT_DEFAULT_ALLOW) {
        // 默认允许：检查是否在拒绝列表中  
        matched = !DevLimitMatchItemPartial(&devLimit->accessList, &item);
    } else {
        // 默认拒绝：检查是否在允许列表中  
        matched = DevLimitMatchItem(&devLimit->accessList, &item);
    }
    if (!matched) {                          // 未匹配到允许规则  
        return EPERM;
    }
    return LOS_OK;
}
#endif
