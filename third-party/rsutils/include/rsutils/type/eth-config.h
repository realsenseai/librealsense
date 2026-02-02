// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2024 RealSense, Inc. All Rights Reserved.
#pragma once

#include "eth-config-header.h"

#include <rsutils/type/ip-address.h>

namespace rsutils {
namespace type {


enum class link_priority : uint8_t
{
    usb_only = 0,
    eth_only = 1,
    eth_first = 2,
    usb_first = 3,

    _dynamic_bit = 0x10,
    dynamic_eth_first = eth_first | _dynamic_bit,
    dynamic_usb_first = usb_first | _dynamic_bit
};
std::ostream & operator<<( std::ostream & os, link_priority );


struct ip_3
{
    rsutils::type::ip_address ip;
    rsutils::type::ip_address netmask;
    rsutils::type::ip_address gateway;

    operator bool() const { return ip.is_valid(); }

    bool operator==( ip_3 const & other ) const { return ip == other.ip && netmask == other.netmask && gateway == other.gateway; }
    bool operator!=( ip_3 const & other ) const { return ip != other.ip || netmask != other.netmask || gateway != other.gateway; }
};
std::ostream & operator<<( std::ostream &, ip_3 const & );


struct eth_config_v3;
struct eth_config_v4;
struct eth_config_v5;


struct eth_config
{
    eth_config_header header;
    std::string mac_address;
    ip_3 configured;
    ip_3 actual;
    struct dds_t
    {
        int domain_id;  // dds domain id
    } dds;
    struct link_t
    {
        unsigned mtu;      // bytes per packet
        unsigned speed;    // Mbps read-only; 0 if link is off
        unsigned timeout;  // The threshold to wait eth link [ms]
        link_priority priority;
    } link;
    struct dhcp_t
    {
        bool on;
        unsigned timeout;  // The threshold to wait valid ip when DHCP is on [s]
    } dhcp;
    unsigned transmission_delay; // Delay between packets [us]
    unsigned udp_ttl;            // UDP packet "time to live"

    eth_config() {}
    explicit eth_config( std::vector< uint8_t > const & hwm_response_without_code );
    eth_config( eth_config_v3 const & );
    eth_config( eth_config_v4 const & );
    eth_config( eth_config_v5 const & );

    bool operator==( eth_config const & ) const noexcept;
    bool operator!=( eth_config const & ) const noexcept;

    std::vector< uint8_t > build_command() const;
    void validate() const; // Check field integrity, throws if not valid
};

}  // namespace type
}  // namespace rsutils
