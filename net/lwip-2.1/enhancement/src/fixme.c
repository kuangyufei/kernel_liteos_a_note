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

#include "lwip/fixme.h"

#include <lwip/sys.h>
#include <lwip/snmp.h>
#include <lwip/etharp.h>
#include <lwip/netifapi.h>
#include <lwip/priv/api_msg.h>

#define NETIFAPI_VAR_REF(name)      API_VAR_REF(name)
#define NETIFAPI_VAR_DECLARE(name)  API_VAR_DECLARE(struct netifapi_msg, name)
#define NETIFAPI_VAR_ALLOC(name)    API_VAR_ALLOC(struct netifapi_msg, MEMP_NETIFAPI_MSG, name, ERR_MEM)
#define NETIFAPI_VAR_FREE(name)     API_VAR_FREE(MEMP_NETIFAPI_MSG, name)

#if LWIP_DHCP

#include <lwip/dhcp.h>

/*
 * Close DHCP and set static network.
 * @param netif a pre-allocated netif structure
 * @return ERR_OK, or ERR_VAL if failed.
 */
err_t netif_dhcp_off(struct netif *netif)
{
    ip_addr_t old_ipaddr;
    ip_addr_t old_netmask;
    ip_addr_t old_gateway;

    if (netif == NULL) {
        return ERR_VAL;
    }
    old_ipaddr = netif->ip_addr;
    old_netmask = netif->netmask;
    old_gateway = netif->gw;

    if (netif_dhcp_data(netif)) {
        (void)dhcp_release(netif);
        (void)dhcp_stop(netif);
        (void)dhcp_cleanup(netif);
        LWIP_DEBUGF(NETIF_DEBUG, ("DHCP is close;set static IP\n"));
    }

    ip_addr_set_val(&netif->ip_addr, &old_ipaddr);
    ip_addr_set_val(&netif->netmask, &old_netmask);
    ip_addr_set_val(&netif->gw, &old_gateway);
    (void)netif_set_up(netif);

    return ERR_OK;
}

err_t dhcp_is_bound(struct netif *netif)
{
    struct dhcp *dhcp = NULL;

    LWIP_ERROR("netif != NULL", (netif != NULL), return ERR_ARG);

    dhcp = netif_dhcp_data(netif);
    LWIP_ERROR("netif->dhcp != NULL", (dhcp != NULL), return ERR_ARG);

    if (dhcp->state == DHCP_STATE_BOUND) {
        return ERR_OK;
    } else {
        return ERR_INPROGRESS;
    }
}

#endif /* LWIP_DHCP */

#if LWIP_DHCPS

#include "lwip/dhcps.h"

static err_t netifapi_do_dhcps_start(struct tcpip_api_call_data *m)
{
    /* cast through void* to silence alignment warnings.
     * We know it works because the structs have been instantiated as struct netifapi_msg */
    err_t ret;
    struct netifapi_msg *msg = (struct netifapi_msg *)(void *)m;
    ret = dhcps_start(msg->netif, msg->msg.dhcp_start_params.start_ip, msg->msg.dhcp_start_params.ip_num);
    return ret;
}

err_t netifapi_dhcps_start(struct netif *netif, char *start_ip, u16_t ip_num)
{
    err_t err;
    NETIFAPI_VAR_DECLARE(msg);

    LWIP_ERROR("netifapi_dhcps_start : invalid arguments", (netif != NULL), return ERR_VAL);
    NETIFAPI_VAR_ALLOC(msg);

    NETIFAPI_VAR_REF(msg).netif = netif;
    NETIFAPI_VAR_REF(msg).msg.dhcp_start_params.start_ip = start_ip;
    NETIFAPI_VAR_REF(msg).msg.dhcp_start_params.ip_num = ip_num;

    err = tcpip_api_call(netifapi_do_dhcps_start, &API_VAR_REF(msg).call);

    NETIFAPI_VAR_FREE(msg);
    return err;
}

err_t netifapi_dhcps_stop(struct netif *netif)
{
    LWIP_ERROR("netifapi_dhcps_stop : invalid arguments", (netif != NULL), return ERR_VAL);

    return netifapi_netif_common(netif, dhcps_stop, NULL);
}

#endif /* LWIP_DHCPS */

/*
 * This function is for making sure that accept() should not block indefinetely
 * when removing IPv6 address used for accept() by using API[netifapi_netif_rmv_ip6_address].
 */
static void tcp_unlock_accept(ip6_addr_t *ipaddr)
{
    (void)ipaddr;
}

static void netif_ip6_addr_setinvalid(struct netif *netif, const ip6_addr_t *addr6)
{
    s8_t idx;
    LWIP_ERROR("netif_ip6_addr_set : invalid arguments", (netif != NULL), return);
    LWIP_ERROR("netif_ip6_addr_set : invalid arguments", (addr6 != NULL), return);

    idx = netif_get_ip6_addr_match(netif, addr6);
    if (idx < 0) {
        return;
    }

    netif_ip6_addr_set_state(netif, idx, IP6_ADDR_INVALID);
    return;
}

err_t netif_do_rmv_ipv6_addr(struct netif *netif, void *arguments)
{
    ip_addr_t *ipaddr = (ip_addr_t *)arguments;

    if (IP_IS_V6(ipaddr)) {
#if LWIP_TCP
        tcp_unlock_accept(ip_2_ip6(ipaddr));
#endif
        netif_ip6_addr_setinvalid(netif, ip_2_ip6(ipaddr));
    }
    return ERR_OK;
}

static err_t netif_do_rmv_ip6_address(struct tcpip_api_call_data *m)
{
    /* cast through void* to silence alignment warnings.
     * We know it works because the structs have been instantiated as struct netifapi_msg */
    struct netifapi_msg *msg = (struct netifapi_msg *)(void *)m;

    return netif_do_rmv_ipv6_addr(msg->netif, (void *)msg->msg.add.ipaddr);
}

void netifapi_netif_rmv_ip6_address(struct netif *netif, ip_addr_t *ipaddr)
{
    err_t err;
    if (netif == NULL) {
        return;
    }
    NETIFAPI_VAR_DECLARE(msg);
    NETIFAPI_VAR_ALLOC(msg);

    NETIFAPI_VAR_REF(msg).netif = netif;
    NETIFAPI_VAR_REF(msg).msg.add.ipaddr = (void *)ipaddr;

    err = tcpip_api_call(netif_do_rmv_ip6_address, &API_VAR_REF(msg).call);

    NETIFAPI_VAR_FREE(msg);
    (void)err;
}

static struct netif *netif_find_by_name(const char *name)
{
    struct netif *netif = NULL;

    LWIP_ASSERT_CORE_LOCKED();

    if (name == NULL) {
        return NULL;
    }

    NETIF_FOREACH(netif) {
        if (strcmp("lo", name) == 0 && (netif->name[0] == 'l' && netif->name[1] == 'o')) {
            LWIP_DEBUGF(NETIF_DEBUG, ("netif_find_by_name: found lo\n"));
            return netif;
        }

        if (strcmp(netif->full_name, name) == 0) {
            LWIP_DEBUGF(NETIF_DEBUG, ("netif_find_by_name: found %s\n", name));
            return netif;
        }
    }

    LWIP_DEBUGF(NETIF_DEBUG, ("netif_find_by_name: didn't find %s\n", name));
    return NULL;
}

static err_t netifapi_do_find_by_name(struct tcpip_api_call_data *m)
{
    /* cast through void* to silence alignment warnings.
     * We know it works because the structs have been instantiated as struct netifapi_msg */
    struct netifapi_msg *msg = (struct netifapi_msg *)(void *)m;

    msg->netif = netif_find_by_name(msg->msg.ifs.name);
    return ERR_OK;
}

struct netif *netifapi_netif_find_by_name(const char *name)
{
    struct netif *netif = NULL;
    NETIFAPI_VAR_DECLARE(msg);
    NETIFAPI_VAR_ALLOC(msg);

    NETIFAPI_VAR_REF(msg).netif = NULL;
#if LWIP_MPU_COMPATIBLE
    if (strncpy_s(NETIFAPI_VAR_REF(msg).msg.ifs.name, NETIF_NAMESIZE, name, NETIF_NAMESIZE - 1)) {
        NETIFAPI_VAR_FREE(msg);
        return netif;
    }
    NETIFAPI_VAR_REF(msg).msg.ifs.name[NETIF_NAMESIZE - 1] = '\0';
#else
    NETIFAPI_VAR_REF(msg).msg.ifs.name = (char *)name;
#endif /* LWIP_MPU_COMPATIBLE */

    (void)tcpip_api_call(netifapi_do_find_by_name, &API_VAR_REF(msg).call);

    netif = msg.netif;
    NETIFAPI_VAR_FREE(msg);
    return netif;
}

#define NETIF_MTU_MIN       1280
#ifndef IP_FRAG_MIN_MTU
#define IP_FRAG_MIN_MTU     68
#endif

err_t netif_set_mtu(struct netif *netif, u16_t netif_mtu)
{
    /*
     * As per RFC 791, "Every internet module must be able to forward a datagram of 68
     * octets without further fragmentation.  This is because an internet header
     * may be up to 60 octets, and the minimum fragment is 8 octets."
     */
    LWIP_ERROR("netif_set_mtu: invalid arguments", (netif != NULL), return ERR_VAL);

#if LWIP_IPV6
    LWIP_ERROR("netif_set_mtu: invalid arguments", (netif_mtu >= NETIF_MTU_MIN) && (netif_mtu <= IP_FRAG_MAX_MTU),
               return ERR_ARG);
#else
    LWIP_ERROR("netif_set_mtu: invalid arguments", (netif_mtu >= IP_FRAG_MIN_MTU) && (netif_mtu <= IP_FRAG_MAX_MTU),
             return ERR_ARG);
#endif

    netif->mtu = netif_mtu;
#if LWIP_IPV6 && LWIP_ND6_ALLOW_RA_UPDATES
    netif->mtu6 = netif_mtu;
#endif /* LWIP_IPV6 && LWIP_ND6_ALLOW_RA_UPDATES */

    LWIP_DEBUGF(NETIF_DEBUG | LWIP_DBG_STATE, ("netif: MTU of interface %s is changed to %d\n",
                netif_get_name(netif), netif->mtu));
    return ERR_OK;
}

err_t netif_set_hwaddr(struct netif *netif, const unsigned char *hw_addr, int hw_len)
{
    LWIP_ERROR("netif_set_hwaddr : invalid arguments", (netif != NULL), return ERR_VAL);

    LWIP_ERROR("netif_set_hwaddr : invalid arguments", (hw_addr != NULL), return ERR_VAL);

    LWIP_ERROR("netif_set_hwaddr: invalid arguments",
               ((unsigned int)hw_len == NETIF_MAX_HWADDR_LEN), return ERR_VAL);

    if (netif->drv_set_hwaddr == NULL) {
        return ERR_IF; // ERR_OPNOTSUPP;
    }

    if (netif->drv_set_hwaddr(netif, (u8_t *)hw_addr, hw_len) != ERR_OK) {
        return ERR_VAL;
    }

    if (memcpy_s(netif->hwaddr, NETIF_MAX_HWADDR_LEN, hw_addr, hw_len) != EOK) {
        LWIP_DEBUGF(NETIF_DEBUG, ("netif_set_hwaddr: memcpy_s error\n"));
        return ERR_VAL;
    }

    LWIP_DEBUGF(NETIF_DEBUG | LWIP_DBG_TRACE | LWIP_DBG_STATE,
                ("netif: HW address of interface %s set to %02X:%02X:%02X:%02X:%02X:%02X\n",
                 netif_get_name(netif),
                 netif->hwaddr[0], netif->hwaddr[1], netif->hwaddr[2],
                 netif->hwaddr[3], netif->hwaddr[4], netif->hwaddr[5]));

    return ERR_OK;
}

err_t etharp_update_arp_entry(struct netif *netif, const ip4_addr_t *ipaddr, struct eth_addr *ethaddr, u8_t flags)
{
    (void)netif;
    (void)ipaddr;
    (void)ethaddr;
    (void)flags;
    return 0;
}

err_t etharp_delete_arp_entry(struct netif *netif, ip4_addr_t *ipaddr)
{
    (void)netif;
    (void)ipaddr;
    return 0;
}


err_t lwip_dns_setserver(u8_t numdns, ip_addr_t *dnsserver)
{
    (void)numdns;
    (void)dnsserver;
    return 0;
}

err_t lwip_dns_getserver(u8_t numdns, ip_addr_t *dnsserver)
{
    (void)numdns;
    (void)dnsserver;
    return 0;
}

#if PF_PKT_SUPPORT
struct raw_pcb *pkt_raw_pcbs;
#endif

#if LWIP_RAW
struct raw_pcb *raw_pcbs; /* already defined in raw.c, but is static */
#endif

#if LWIP_ENABLE_IP_CONFLICT_SIGNAL
u32_t is_ip_conflict_signal = 0;
sys_sem_t ip_conflict_detect;
#endif

u32_t is_dup_detect_initialized = 0;
sys_sem_t dup_addr_detect;

#if LWIP_SNTP

#include <time.h>

int lwip_sntp_start(int server_num, char **sntp_server, struct timeval *time)
{
    (void)server_num;
    (void)sntp_server;
    (void)time;
    return 0;
}

#endif

const char *const tcp_state_str[] = {
    "CLOSED",
    "LISTEN",
    "SYN_SENT",
    "SYN_RCVD",
    "ESTABLISHED",
    "FIN_WAIT_1",
    "FIN_WAIT_2",
    "CLOSE_WAIT",
    "CLOSING",
    "LAST_ACK",
    "TIME_WAIT"
};

volatile int tcpip_init_finish = 1; // needed by api_shell.c

int ip6addr_aton(const char *cp, ip6_addr_t *addr)
{
    const int ipv6_blocks = 8;
    u16_t current_block_index = 0;
    u16_t current_block_value = 0;
    u16_t addr16[ipv6_blocks];
    u16_t *a16 = (u16_t *)addr->addr;
    int squash_pos = ipv6_blocks;
    int i;
    const char *s = cp;
    const char *ss = cp-1;

    for (; ; s++) {
        if (current_block_index >= ipv6_blocks) {
            return 0; // address too long
        }
        if (*s == 0) {
            if (s - ss == 1) {
                if (squash_pos != current_block_index) {
                    return 0; // empty address or address ends with a single ':'
                } // else address ends with one valid "::"
            } else {
                addr16[current_block_index++] = current_block_value;
            }
            break;
        } else if (*s == ':') {
            if (s - ss == 1) {
                if (s != cp || s[1] != ':') {
                    return 0; // address begins with a single ':' or contains ":::"
                } // else address begins with one valid "::"
            } else {
                addr16[current_block_index++] = current_block_value;
            }
            if (s[1] == ':') {
                if (squash_pos != ipv6_blocks) {
                    return 0; // more than one "::"
                }
                squash_pos = current_block_index;
                s++;
            }
            ss = s; // ss points to the recent ':' position
            current_block_value = 0;
        } else if (lwip_isxdigit(*s) && (s - ss) < 5) { // 4 hex-digits at most
            current_block_value = (current_block_value << 4) +
                ((u8_t)(*s) | ('a' - 'A')) - '0' - ('a' - '9' - 1) * (*s >= 'A');
#if LWIP_IPV4
        } else if (*s == '.' && current_block_index < ipv6_blocks - 1) {
            ip4_addr_t ip4;
            int ret = ip4addr_aton(ss+1, &ip4);
            if (!ret) {
                return 0;
            }
            ip4.addr = lwip_ntohl(ip4.addr);
            addr16[current_block_index++] = (u16_t)(ip4.addr >> 16);
            addr16[current_block_index++] = (u16_t)(ip4.addr);
            break;
#endif /* LWIP_IPV4 */
        } else {
            return 0; // unexpected char or too many digits
        }
    }

    if (squash_pos == ipv6_blocks && current_block_index != ipv6_blocks) {
        return 0; // address too short
    }
    if (squash_pos != ipv6_blocks && current_block_index == ipv6_blocks) {
        return 0; // unexpected "::" in address
    }

    for (i = 0; i < squash_pos; ++i) {
        a16[i] = lwip_htons(addr16[i]);
    }
    for (; i < ipv6_blocks - current_block_index + squash_pos; ++i) {
        a16[i] = 0;
    }
    for (; i < ipv6_blocks; ++i) {
        a16[i] = lwip_htons(addr16[i - ipv6_blocks + current_block_index]);
    }

    return 1;
}
