// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2024 RealSense, Inc. All Rights Reserved.
#pragma once

#include "eth-config-header.h"


namespace rsutils {
namespace type {


#pragma pack( push, 1 )

// The structure data for eth config info, version 3
// See table definition in HWMC v0.53 spec
struct eth_config_v3
{
    eth_config_header header;
    uint32_t link_check_timeout;  // The threshold to wait eth link(ms).
    uint8_t config_ip[4];         // static IP
    uint8_t config_netmask[4];    // static netmask
    uint8_t config_gateway[4];    // static gateway
    uint8_t actual_ip[4];         // actual IP when dhcp is ON, read-only
    uint8_t actual_netmask[4];    // actual netmask when dhcp is ON, read-only
    uint8_t actual_gateway[4];    // actual gateway when dhcp is ON, read-only
    uint32_t link_speed;          // Mbps read-only
    uint32_t mtu;                 // read-only
    uint8_t dhcp_on;              // 0=off
    uint8_t dhcp_timeout;         // The threshold to wait valid ip when DHCP is on(s).
    uint8_t domain_id;            // dds domain id
    uint8_t link_priority;        // device link priority. 0-USB_ONLY, 1-ETH_ONLY, 2-ETH_FIRST, 3 USB_FIRST
    uint8_t mac_address[6];       // read-only
    uint8_t reserved[2];          // needed to make size divisible by 4
};

static_assert( sizeof( eth_config_v3 ) % 4 == 0, "eth config v3 struct size must be divisible by 4" );

// The structure data for eth config info, version 4
// See table definition in HWMC v0.54 spec
// Changes from v3 - 2 reserved bytes defined as transmission_delay, 40 reserved bytes added.
struct eth_config_v4
{
    eth_config_header header;
    uint32_t link_check_timeout;  // The threshold to wait eth link(ms).
    uint8_t config_ip[4];         // static IP
    uint8_t config_netmask[4];    // static netmask
    uint8_t config_gateway[4];    // static gateway
    uint8_t actual_ip[4];         // actual IP when dhcp is ON, read-only
    uint8_t actual_netmask[4];    // actual netmask when dhcp is ON, read-only
    uint8_t actual_gateway[4];    // actual gateway when dhcp is ON, read-only
    uint32_t link_speed;          // Mbps read-only
    uint32_t mtu;                 // read-only
    uint8_t dhcp_on;              // 0=off
    uint8_t dhcp_timeout;         // The threshold to wait valid ip when DHCP is on(s).
    uint8_t domain_id;            // dds domain id
    uint8_t link_priority;        // device link priority. 0-USB_ONLY, 1-ETH_ONLY, 2-ETH_FIRST, 3 USB_FIRST
    uint8_t mac_address[6];       // read-only
    uint16_t transmission_delay;  // Inter-MTU transmission delay, microseconds.
    uint8_t reserved[40];         // For future growth, struct size should be multiplication of 4.
};

static_assert( sizeof( eth_config_v4 ) % 4 == 0, "eth config v4 struct size must be divisible by 4" );

// The structure data for eth config info, version 5
// See table definition in HWMC v0.54 spec
// Changes from v4 - 1 reserved byte defined as TTL value, link_check_timeout limits defined.
struct eth_config_v5
{
    eth_config_header header;
    uint32_t link_check_timeout;  // The threshold to wait eth link(ms).
    uint8_t config_ip[4];         // static IP
    uint8_t config_netmask[4];    // static netmask
    uint8_t config_gateway[4];    // static gateway
    uint8_t actual_ip[4];         // actual IP when dhcp is ON, read-only
    uint8_t actual_netmask[4];    // actual netmask when dhcp is ON, read-only
    uint8_t actual_gateway[4];    // actual gateway when dhcp is ON, read-only
    uint32_t link_speed;          // Mbps read-only
    uint32_t mtu;                 // read-only
    uint8_t dhcp_on;              // 0=off
    uint8_t dhcp_timeout;         // The threshold to wait valid ip when DHCP is on(s).
    uint8_t domain_id;            // dds domain id
    uint8_t link_priority;        // device link priority. 0-USB_ONLY, 1-ETH_ONLY, 2-ETH_FIRST, 3 USB_FIRST
    uint8_t mac_address[6];       // read-only
    uint16_t transmission_delay;  // Inter-MTU transmission delay, microseconds.
    uint8_t udp_ttl;              // UDP packet "time to live".
    uint8_t reserved[39];         // For future growth, struct size should be multiplication of 4.
};

static_assert( sizeof( eth_config_v5 ) % 4 == 0, "eth config v5 struct size must be divisible by 4" );

#pragma pack( pop )

}  // namespace type
}  // namespace rsutils
