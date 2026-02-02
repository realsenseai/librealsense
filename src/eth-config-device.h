// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2025 RealSense, Inc. All Rights Reserved.

#pragma once

#include "core/debug.h"
#include "core/extension.h"

#include <rsutils/type/eth-config.h>

namespace librealsense
{
    // Provides interface for ethernet configuration support.
    class eth_config_device
    {
    public:
        eth_config_device(){};
        virtual ~eth_config_device() = default;

        // debug_interface not set at constructor to avoid race conditions during device object construction list. Devices can set this at constructor body.
        void init( debug_interface * debug );

        virtual bool supports_ethernet_configuration();

        // Link configuration
        virtual uint32_t get_ethernet_link_speed() const;
        virtual rsutils::type::link_priority get_link_priority() const;
        virtual void set_link_priority( rsutils::type::link_priority priority );
        virtual uint32_t get_link_timeout() const;
        virtual void set_link_timeout( uint32_t timeout );

        // DDS configuration
        virtual uint32_t get_dds_domain_id() const;
        virtual void set_dds_domain_id( uint32_t domain );

        // IP configuration
        virtual void get_ip_address( rsutils::type::ip_address & configured_ip, rsutils::type::ip_address & actual_ip ) const;
        virtual void set_ip_address( const rsutils::type::ip_address & ip );
        virtual void get_netmask( rsutils::type::ip_address & configured_netmask, rsutils::type::ip_address & actual_netmask ) const;
        virtual void set_netmask( const rsutils::type::ip_address & netmask );
        virtual void get_gateway( rsutils::type::ip_address & configured_gateway, rsutils::type::ip_address & actual_gateway ) const;
        virtual void set_gateway( const rsutils::type::ip_address & gateway );

        // DHCP configuration
        virtual void get_dhcp_config( bool & enabled, uint8_t & timeout ) const;
        virtual void set_dhcp_config( bool enabled, uint8_t timeout );

        // Network configuration
        virtual uint32_t get_mtu() const;
        virtual void set_mtu( uint32_t mtu );
        virtual uint32_t get_transmission_delay() const;
        virtual void set_transmission_delay( uint32_t delay );
        virtual uint8_t get_udp_ttl() const;
        virtual void set_udp_ttl( uint8_t ttl );

        // Factory reset
        virtual void restore_defaults();

    private:
        debug_interface * _debug = nullptr;
        rsutils::type::eth_config _curr_config;
        bool _supported = false;

        rsutils::type::eth_config read_eth_config_from_device( bool default_values = false );
        void save_eth_config_to_device( const rsutils::type::eth_config & config, bool default_values = false );
        void validate_support() const;
    };

MAP_EXTENSION( RS2_EXTENSION_ETH_CONFIG, eth_config_device );

}  // namespace librealsense
