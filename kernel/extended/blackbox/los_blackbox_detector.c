/*
 * Copyright (c) 2021-2021 Huawei Device Co., Ltd. All rights reserved.
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

/* ------------ includes ------------ */
#include "los_blackbox_detector.h"
#include "los_blackbox_common.h"
#ifdef LOSCFG_LIB_LIBC
#include "stdlib.h"
#include "unistd.h"
#endif

/* ------------ local macroes ------------ */
/* ------------ local prototypes ------------ */
/* ------------ local function declarations ------------ */
/* ------------ global function declarations ------------ */
/* ------------ local variables ------------ */
/* ------------ function definitions ------------ */
/**
 * @brief 通过文件路径上传事件数据
 * @param filePath 事件文件的路径字符串，不能为空
 * @return 成功返回0，失败返回-1
 */
int UploadEventByFile(const char *filePath)
{
    if (filePath == NULL) {  // 检查文件路径参数是否为空指针
        BBOX_PRINT_ERR("filePath is NULL\n");  // 打印参数为空错误信息
        return -1;  // 返回错误码-1表示失败
    }

    return 0;  // 返回0表示成功
}

/**
 * @brief 通过数据流上传事件数据
 * @param buf 数据缓冲区指针，不能为空
 * @param bufSize 数据缓冲区大小，必须大于0
 * @return 成功返回0，失败返回-1
 */
int UploadEventByStream(const char *buf, size_t bufSize)
{
    if (buf == NULL || bufSize == 0) {  // 检查缓冲区指针和大小是否有效
        BBOX_PRINT_ERR("buf: %p, bufSize: %u\n", buf, (UINT32)bufSize); // 打印缓冲区无效错误信息
        return -1;  // 返回错误码-1表示失败
    }

    return 0;  // 返回0表示成功
}