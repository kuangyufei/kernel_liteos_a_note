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

#ifndef _LWIP_PORTING_DHCP_H_
#define _LWIP_PORTING_DHCP_H_

#include <lwip/opt.h>

#if LWIP_DHCPS
#define DHCP_OPTION_IDX_SERVER_ID   DHCP_OPTION_IDX_SERVER_ID, \
                                    DHCP_OPTION_IDX_REQUESTED_IP
#endif
#include_next <lwip/dhcp.h>
#if LWIP_DHCPS
#undef DHCP_OPTION_IDX_SERVER_ID
#endif

#include <lwip/prot/dhcp.h>  // For DHCP_STATE_BOUND, DHCP_DISCOVER etc. by `mac/common/mac_data.c'

#ifdef __cplusplus
extern "C" {
#endif

#if LWIP_DHCPS
#define LWIP_HOOK_DHCP_PARSE_OPTION(netif, dhcp, state, msg, msg_type, option, len, pbuf, offset) \
        LWIP_UNUSED_ARG(msg); \
        break; \
    case (DHCP_OPTION_REQUESTED_IP): \
        LWIP_ERROR("len == 4", len == 4, return ERR_VAL); \
        decode_idx = DHCP_OPTION_IDX_REQUESTED_IP;
#endif

err_t dhcp_is_bound(struct netif *netif);

#ifdef __cplusplus
}
#endif

#endif /* _LWIP_PORTING_DHCP_H_ */
