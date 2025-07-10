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

#define _GNU_SOURCE

#include "unistd.h"
#include "shcmd.h"
#include "sherr.h"


/**
 * @brief 切换当前工作目录
 * @param tgtDir 目标目录路径字符串
 * @return 成功返回0(SH_OK)，失败返回-1(SH_ERROR)
 * @note 该函数会更新Shell的工作目录记录
 */
int Chdir(const char *tgtDir)
{
    int ret;  // 函数返回值，0表示成功，非0表示失败

    // 检查目标目录路径是否为空
    if (!tgtDir) {
        return SH_ERROR;  // 路径为空，返回错误
    }

    // 调用系统函数切换目录
    ret = chdir(tgtDir);
    // 如果切换目录成功，更新Shell的工作目录记录
    if (ret == 0) {
        // 保存新的工作目录到Shell控制块
        // 参数1:目标目录路径  参数2:路径长度(包含字符串结束符'\0')
        ret = OsShellSetWorkingDirectory(tgtDir, strlen(tgtDir) + 1); /* 1: the length of '\0' */
    }

    return ret;  // 返回操作结果
}

