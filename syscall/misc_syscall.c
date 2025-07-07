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

#include "errno.h"
#include "sysinfo.h"
#include "sys/reboot.h"
#include "sys/resource.h"
#include "sys/times.h"
#include "sys/utsname.h"
#include "capability_type.h"
#include "capability_api.h"
#include "los_process_pri.h"
#include "los_strncpy_from_user.h"
#ifdef LOSCFG_SHELL
#include "shcmd.h"
#include "shmsg.h"
#endif
#include "user_copy.h"
#include "unistd.h"

#ifdef LOSCFG_UTS_CONTAINER
#define HOST_NAME_MAX_LEN 65
int SysSetHostName(const char *name, size_t len)
{
    int ret;
    char tmpName[HOST_NAME_MAX_LEN];
    unsigned int intSave;

    if (name == NULL) {
        return -EFAULT;
    }

    if ((len < 0) || (len > HOST_NAME_MAX_LEN)) {
        return -EINVAL;
    }

    ret = LOS_ArchCopyFromUser(&tmpName, name, len);
    if (ret != 0) {
        return -EFAULT;
    }

    SCHEDULER_LOCK(intSave);
    struct utsname *currentUtsName = OsGetCurrUtsName();
    if (currentUtsName == NULL) {
        SCHEDULER_UNLOCK(intSave);
        return -EFAULT;
    }

    (VOID)memset_s(currentUtsName->nodename, sizeof(currentUtsName->nodename), 0, sizeof(currentUtsName->nodename));
    ret = memcpy_s(currentUtsName->nodename, sizeof(currentUtsName->nodename), tmpName, len);
    if (ret != EOK) {
        SCHEDULER_UNLOCK(intSave);
        return -EFAULT;
    }
    SCHEDULER_UNLOCK(intSave);
    return 0;
}
#endif

int SysUname(struct utsname *name)
{
    int ret;
    struct utsname tmpName;
    ret = LOS_ArchCopyFromUser(&tmpName, name, sizeof(struct utsname));
    if (ret != 0) {
        return -EFAULT;
    }
    ret = uname(&tmpName);
    if (ret < 0) {
        return ret;
    }
    ret = LOS_ArchCopyToUser(name, &tmpName, sizeof(struct utsname));
    if (ret != 0) {
        return -EFAULT;
    }
    return ret;
}

int SysInfo(struct sysinfo *info)
{
    int ret;
    struct sysinfo tmpInfo = { 0 };

    tmpInfo.totalram = LOS_MemPoolSizeGet(m_aucSysMem1);
    tmpInfo.freeram = LOS_MemPoolSizeGet(m_aucSysMem1) - LOS_MemTotalUsedGet(m_aucSysMem1);
    tmpInfo.sharedram = 0;
    tmpInfo.bufferram = 0;
    tmpInfo.totalswap = 0;
    tmpInfo.freeswap = 0;

    ret = LOS_ArchCopyToUser(info, &tmpInfo, sizeof(struct sysinfo));
    if (ret != 0) {
        return -EFAULT;
    }
    return 0;
}

int SysReboot(int magic, int magic2, int type)
{
    (void)magic;
    (void)magic2;
    if (!IsCapPermit(CAP_REBOOT)) {
        return -EPERM;
    }
    SystemRebootFunc rebootHook = OsGetRebootHook();
    if ((type == RB_AUTOBOOT) && (rebootHook != NULL)) {
        rebootHook();
        return 0;
    }
    return -EFAULT;
}

#ifdef LOSCFG_SHELL
int SysShellExec(const char *msgName, const char *cmdString)
{
    int ret;
    unsigned int uintRet;
    errno_t err;
    CmdParsed cmdParsed;
    char msgNameDup[CMD_KEY_LEN + 1] = { 0 };
    char cmdStringDup[CMD_MAX_LEN + 1] = { 0 };

    if (!IsCapPermit(CAP_SHELL_EXEC)) {
        return -EPERM;
    }

    ret = LOS_StrncpyFromUser(msgNameDup, msgName, CMD_KEY_LEN + 1);
    if (ret < 0) {
        return -EFAULT;
    } else if (ret > CMD_KEY_LEN) {
        return -ENAMETOOLONG;
    }

    ret = LOS_StrncpyFromUser(cmdStringDup, cmdString, CMD_MAX_LEN + 1);
    if (ret < 0) {
        return -EFAULT;
    } else if (ret > CMD_MAX_LEN) {
        return -ENAMETOOLONG;
    }

    err = memset_s(&cmdParsed, sizeof(CmdParsed), 0, sizeof(CmdParsed));
    if (err != EOK) {
        return -EFAULT;
    }

    uintRet = ShellMsgTypeGet(&cmdParsed, msgNameDup);
    if (uintRet != LOS_OK) {
        PRINTK("%s:command not found\n", msgNameDup);
        return -EFAULT;
    } else {
        (void)OsCmdExec(&cmdParsed, (char *)cmdStringDup);
    }

    return 0;
}
#endif

#define USEC_PER_SEC 1000000

static void ConvertClocks(struct timeval *time, clock_t clk)
{
    time->tv_usec = (clk % CLOCKS_PER_SEC) * USEC_PER_SEC / CLOCKS_PER_SEC;
    time->tv_sec = (clk) / CLOCKS_PER_SEC;
}

int SysGetrusage(int what, struct rusage *ru)
{
    int ret;
    struct tms time;
    clock_t usec, sec;
    struct rusage kru;

    ret = LOS_ArchCopyFromUser(&kru, ru, sizeof(struct rusage));
    if (ret != 0) {
        return -EFAULT;
    }

    if (times(&time) == -1) {
        return -EFAULT;
    }

    switch (what) {
        case RUSAGE_SELF: {
            usec = time.tms_utime;
            sec = time.tms_stime;
            break;
        }
        case RUSAGE_CHILDREN: {
            usec = time.tms_cutime;
            sec = time.tms_cstime;
            break;
        }
        default:
            return -EINVAL;
    }
    ConvertClocks(&kru.ru_utime, usec);
    ConvertClocks(&kru.ru_stime, sec);

    ret = LOS_ArchCopyToUser(ru, &kru, sizeof(struct rusage));
    if (ret != 0) {
        return -EFAULT;
    }
    return 0;
}

long SysSysconf(int name)
{
    long ret;

    ret = sysconf(name);
    if (ret == -1) {
        return -get_errno();
    }

    return ret;
}

int SysUgetrlimit(int resource, unsigned long long k_rlim[2])
{
    int ret;
    struct rlimit lim = { 0 };

    ret = getrlimit(resource, &lim);
    if (ret < 0) {
        return ret;
    }

    ret = LOS_ArchCopyToUser(k_rlim, &lim, sizeof(struct rlimit));
    if (ret != 0) {
        return -EFAULT;
    }

    return ret;
}

int SysSetrlimit(int resource, unsigned long long k_rlim[2])
{
    int ret;
    struct rlimit lim;

	if(!IsCapPermit(CAP_CAPSET)) {
        return -EPERM;
    }

    ret = LOS_ArchCopyFromUser(&lim, k_rlim, sizeof(struct rlimit));
    if (ret != 0) {
        return -EFAULT;
    }
    ret = setrlimit(resource, &lim);

    return ret;
}
