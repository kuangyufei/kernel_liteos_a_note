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

#ifndef LWIP_HDR_FIXME_H
#define LWIP_HDR_FIXME_H

#include "lwip/opt.h"
#include "netif/etharp.h"
#include "lwip/netif.h"

#define link_rx_drop cachehit
#define link_tx_drop cachehit
#define link_rx_overrun cachehit
#define link_tx_overrun cachehit

#define ip_rx_err cachehit
#define ip_tx_err cachehit
#define ip_rx_bytes cachehit
#define ip_tx_bytes cachehit

#define DUP_ARP_DETECT_TIME             2000      /* 2 seconds period */
#define NETCONN_PKT_RAW                 0x80
#define SYS_ARCH_ERROR                  0x7fffffffUL

#define LWIP_ENABLE_LOS_SHELL_CMD       LOSCFG_SHELL
#define LWIP_SHELL_CMD_PING_RETRY_TIMES 4
#define LWIP_SHELL_CMD_PING_TIMEOUT     2000
#define LWIP_MAX_UDP_RAW_SEND_SIZE      65332
#define LWIP_EXT_POLL_SUPPORT           LWIP_SOCKET_POLL

#define ip_addr_set_val(dest, src)  do { \
                                        IP_SET_TYPE_VAL(*dest, IP_GET_TYPE(src)); \
                                        if(IP_IS_V6_VAL(*(src))) { \
                                            ip6_addr_set(ip_2_ip6(dest), ip_2_ip6(src)); \
                                        } else { \
                                            ip4_addr_set(ip_2_ip4(dest), ip_2_ip4(src)); \
                                        } \
                                    } while (0)

#define ip_addr_ismulticast_val(ipaddr)  ((IP_IS_V6_VAL(*ipaddr)) ? \
                                          ip6_addr_ismulticast(ip_2_ip6(ipaddr)) : \
                                          ip4_addr_ismulticast(ip_2_ip4(ipaddr)))

#define ip_addr_isbroadcast_val(ipaddr, netif) ((IP_IS_V6_VAL(*ipaddr)) ? \
                                                0 : \
                                                ip4_addr_isbroadcast(ip_2_ip4(ipaddr), netif))

#define ip_addr_netcmp_val(addr1, addr2, mask) ((IP_IS_V6_VAL(*(addr1)) && IP_IS_V6_VAL(*(addr2))) ? \
                                                0 : \
                                                ip4_addr_netcmp(ip_2_ip4(addr1), ip_2_ip4(addr2), mask))

#define ip6_addr_isnone_val(ip6addr) (((ip6addr).addr[0] == 0xffffffffUL) && \
                                     ((ip6addr).addr[1] == 0xffffffffUL) && \
                                     ((ip6addr).addr[2] == 0xffffffffUL) && \
                                     ((ip6addr).addr[3] == 0xffffffffUL))

#define ip6_addr_isnone(ip6addr) (((ip6addr) == NULL) || ip6_addr_isnone_val(*(ip6addr)))

#define ipaddr_ntoa_unsafe(addr) ((IP_IS_V6_VAL(*addr)) ? ip6addr_ntoa(ip_2_ip6(addr)) : ip4addr_ntoa(ip_2_ip4(addr)))

#ifdef ip6_addr_cmp
#undef ip6_addr_cmp
#define ip6_addr_cmp(addr1, addr2) (((addr1)->addr[0] == (addr2)->addr[0]) && \
                                    ((addr1)->addr[1] == (addr2)->addr[1]) && \
                                    ((addr1)->addr[2] == (addr2)->addr[2]) && \
                                    ((addr1)->addr[3] == (addr2)->addr[3]))
#endif

err_t netif_dhcp_off(struct netif *netif);

err_t netif_do_rmv_ipv6_addr(struct netif *netif, void *arguments);

err_t netif_set_mtu(struct netif *netif, u16_t netif_mtu);

err_t netif_set_hwaddr(struct netif *netif, const unsigned char *hw_addr, int hw_len);

err_t etharp_update_arp_entry(struct netif *netif, const ip4_addr_t *ipaddr, struct eth_addr *ethaddr, u8_t flags);

err_t etharp_delete_arp_entry(struct netif *netif, ip4_addr_t *ipaddr);

err_t lwip_dns_setserver(u8_t numdns, ip_addr_t *dnsserver);

err_t lwip_dns_getserver(u8_t numdns, ip_addr_t *dnsserver);

#if PF_PKT_SUPPORT
extern struct raw_pcb *pkt_raw_pcbs;
#endif

#if LWIP_RAW
extern struct raw_pcb *raw_pcbs;
#endif

#endif /* LWIP_HDR_FIXME_H */
