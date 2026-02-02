// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2025 RealSense, Inc. All Rights Reserved.

#ifndef LIBREALSENSE_RS2_ETH_CONFIG_HPP
#define LIBREALSENSE_RS2_ETH_CONFIG_HPP

#include "rs_types.hpp"
#include "rs_device.hpp"
#include "../h/rs_eth_config.h"

#include <sstream>

namespace rs2
{
    /**
     * \brief Ethernet configuration extension for devices that support ethernet configuration
     */
    class eth_config_device : public device
    {
    public:
        eth_config_device() : device() {}

        eth_config_device( rs2::device d ) : device( d.get() )
        {
            rs2_error * e = nullptr;
            bool extendable = rs2_is_device_extendable_to( _dev.get(), RS2_EXTENSION_ETH_CONFIG, &e ) && ! e;
            if( ! extendable || ! rs2_supports_eth_config( _dev.get(), &e ) || e )
                _dev.reset();
            error::handle( e );
        }

        /**
         * Check if device supports ethernet configuration
         */
        bool supports_eth_config() const
        {
            if( ! _dev )
                return false; // Was reset in constructor because not supporting Ethernet configuration
            rs2_error * e = nullptr;
            auto result = rs2_supports_eth_config( _dev.get(), &e );
            error::handle( e );
            return result != 0;
        }

        /**
         * Get Ethernet link speed, 0 if not linked
         */
        uint32_t get_link_speed() const
        {
            rs2_error * e = nullptr;
            auto result = rs2_get_link_speed( _dev.get(), &e );
            error::handle( e );
            return result;
        }

        /**
         * Get current link priority setting
         */
        rs2_eth_link_priority get_link_priority() const
        {
            rs2_error * e = nullptr;
            auto result = rs2_get_link_priority( _dev.get(), &e );
            error::handle( e );
            return result;
        }

        /**
         * Set link priority
         */
        void set_link_priority( rs2_eth_link_priority priority ) const
        {
            rs2_error * e = nullptr;
            rs2_set_link_priority( _dev.get(), priority, &e );
            error::handle( e );
        }

        /**
         * Get current link timeout setting in milliseconds
         */
        uint32_t get_link_timeout() const
        {
            rs2_error * e = nullptr;
            auto result = rs2_get_link_timeout( _dev.get(), &e );
            error::handle( e );
            return result;
        }

        /**
         * Set link timeout in milliseconds
         */
        void set_link_timeout( uint32_t timeout ) const
        {
            rs2_error * e = nullptr;
            rs2_set_link_timeout( _dev.get(), timeout, &e );
            error::handle( e );
        }

        /**
         * Get current DDS domain (0-232)
         */
        unsigned int get_dds_domain() const
        {
            rs2_error * e = nullptr;
            auto result = rs2_get_dds_domain( _dev.get(), &e );
            error::handle( e );
            return result;
        }

        /**
         * Set DDS domain (0-232)
         */
        void set_dds_domain( unsigned int domain )
        {
            rs2_error * e = nullptr;
            rs2_set_dds_domain( _dev.get(), domain, &e );
            error::handle( e );
        }

        /**
         * Get current IP address
         */
        void get_ip_address( rs2_ip_address & configured_ip, rs2_ip_address & actual_ip) const
        {
            rs2_error * e = nullptr;
            rs2_get_ip_address( _dev.get(), configured_ip, actual_ip, &e );
            error::handle( e );
        }

        /**
         * Set IP address
         */
        void set_ip_address( const rs2_ip_address ip ) const
        {
            rs2_error * e = nullptr;
            rs2_set_ip_address( _dev.get(), ip, &e );
            error::handle( e );
        }

        /**
         * Get current network mask
         */
        void get_netmask( rs2_ip_address & configured_netmask, rs2_ip_address & actual_netmask ) const
        {
            rs2_error * e = nullptr;
            rs2_get_netmask( _dev.get(), configured_netmask, actual_netmask, &e );
            error::handle( e );
        }

        /**
         * Set network mask
         */
        void set_netmask( const rs2_ip_address netmask ) const
        {
            rs2_error * e = nullptr;
            rs2_set_netmask( _dev.get(), netmask, &e );
            error::handle( e );
        }

        /**
         * Get current gateway address
         */
        void get_gateway( rs2_ip_address & configured_gateway, rs2_ip_address & actual_gateway ) const
        {
            rs2_error * e = nullptr;
            rs2_get_gateway( _dev.get(), configured_gateway, actual_gateway, &e );
            error::handle( e );
        }

        /**
         * Set gateway address
         */
        void set_gateway( const rs2_ip_address gateway ) const
        {
            rs2_error * e = nullptr;
            rs2_set_gateway( _dev.get(), gateway, &e );
            error::handle( e );
        }

        /**
         * Get DHCP configuration
         * \param[out] enabled  DHCP enabled flag
         * \param[out] timeout  DHCP timeout threshold in seconds
         */
        void get_dhcp_config( bool & enabled, uint8_t & timeout ) const
        {
            rs2_error * e = nullptr;
            int enabled_as_int = 0;
            uint32_t timeout_as_uint = 0;
            rs2_get_dhcp_config( _dev.get(), &enabled_as_int, &timeout_as_uint, &e );
            enabled = enabled_as_int != 0;
            timeout = timeout_as_uint;
            error::handle( e );
        }

        /**
         * Set DHCP configuration
         * \param[in] enabled   DHCP enabled flag
         * \param[in] timeout   DHCP timeout threshold in seconds
         */
        void set_dhcp_config( bool enabled, uint8_t timeout ) const
        {
            rs2_error * e = nullptr;
            rs2_set_dhcp_config( _dev.get(), enabled ? 1 : 0, timeout, &e );
            error::handle( e );
        }

        /**
         * Get current MTU (Maximum Transmission Unit)
         */
        uint32_t get_mtu() const
        {
            rs2_error * e = nullptr;
            auto result = rs2_get_mtu( _dev.get(), &e );
            error::handle( e );
            return result;
        }

        /**
         * Set MTU (Maximum Transmission Unit)
         * \param mtu MTU in bytes (500-9000, in 500 byte steps)
         */
        void set_mtu( uint32_t mtu ) const
        {
            rs2_error * e = nullptr;
            rs2_set_mtu( _dev.get(), mtu, &e );
            error::handle( e );
        }

        /**
         * Get current transmission delay
         */
        uint32_t get_transmission_delay() const
        {
            rs2_error * e = nullptr;
            auto result = rs2_get_transmission_delay( _dev.get(), &e );
            error::handle( e );
            return result;
        }

        /**
         * Set transmission delay
         * \param delay Transmission delay in microseconds (0-144, in 3us steps)
         */
        void set_transmission_delay( uint32_t delay ) const
        {
            rs2_error * e = nullptr;
            rs2_set_transmission_delay( _dev.get(), delay, &e );
            error::handle( e );
        }

        /**
         * Get current UDP packet "time to live" (TTL)
         */
        uint8_t get_udp_ttl() const
        {
            rs2_error * e = nullptr;
            auto result = rs2_get_udp_ttl( _dev.get(), &e );
            error::handle( e );
            return static_cast<uint8_t>(result);
        }

        /**
         * Set UDP packet "time to live" (TTL)
         * \param ttl UDP TTL value (1-255)
         */
        void set_udp_ttl( uint8_t ttl ) const
        {
            rs2_error * e = nullptr;
            rs2_set_udp_ttl( _dev.get(), ttl, &e );
            error::handle( e );
        }

        /**
         * Restores configuration to factory settings
         */
        void restore_defaults()
        {
            rs2_error * e = nullptr;
            rs2_restore_default_eth_config( _dev.get(), &e );
            error::handle( e );
        }
    };

    inline std::ostream & operator<<( std::ostream & os, rs2_eth_link_priority priority )
    {
        const char * str = rs2_eth_link_priority_to_string( priority );
        return os << ( str ? str : "unknown" );
    }
}  // namespace rs2

#endif  // LIBREALSENSE_RS2_ETH_CONFIG_HPP
