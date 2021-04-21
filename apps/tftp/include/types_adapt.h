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

#ifndef TYPES_ADAPT_H
#define TYPES_ADAPT_H

#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>

#define LWIP_TFTP 1
#define LOSCFG_NET_LWIP_SACK_TFTP 1
#define LOSCFG_SHELL_CMD_DEBUG 1

#define u8_t uint8_t
#define s8_t int8_t
#define u16_t uint16_t
#define s16_t int16_t
#define u32_t uint32_t
#define s32_t int32_t

#define X8_F  "02" PRIx8
#define U16_F PRIu16
#define S16_F PRId16
#define X16_F PRIx16
#define U32_F PRIu32
#define S32_F PRId32
#define X32_F PRIx32
#define SZT_F PRIuPTR

#define PRINTK(fmt, ...) printf(fmt, ##__VA_ARGS__)
#define LWIP_ASSERT(msg, expr) assert(expr)
#define LWIP_DEBUGF(module, msg) PRINTK msg

#define LOS_OK 0
#define LOS_NOK 1
#define ERR_OK 0
#define EOK 0

#define mem_malloc malloc
#define mem_free free
#define lwip_socket socket
#define lwip_ioctl ioctl
#define lwip_close close
#define lwip_bind bind
#define lwip_sendto sendto
#define lwip_recvfrom recvfrom

#define IPADDR_NONE INADDR_NONE
#define DEFFILEMODE (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH)

#define SHELLCMD_ENTRY(l, cmdType, cmdKey, paraNum, cmdHook)    \
int main(int argc, const char **argv)                           \
{                                                               \
    return (int)cmdHook(argc, argv);                            \
}

typedef u32_t (*CmdCallBackFunc)(u32_t argc, const char **argv);

#endif /* TYPES_ADAPT_H */
