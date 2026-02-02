/* License: Apache 2.0. See LICENSE file in root directory.
Copyright(c) 2025 RealSense, Inc. All Rights Reserved. */

#include "pyrealsense2.h"
#include <librealsense2/hpp/rs_eth_config.hpp>
#include <rsutils/type/ip-address.h>
#include <sstream>

void init_eth_config(py::module &m) {

    // Found also in pyrsutils. Here to avoid dependency (need to import pyrsutils whenever importing pyrealsense2)
    // Using pyrsutils ip_address, not rs2_ip_address, because python must work on objects, not native type array.
    using rsutils::type::ip_address;
    py::class_< ip_address >( m, "ip_address" )
        .def( py::init<>() )
        .def( py::init< uint8_t, uint8_t, uint8_t, uint8_t >(), "b1", "b2", "b3", "b4" )
        .def( py::init< const uint8_t* >(), "b" )
        .def( "is_valid", &ip_address::is_valid )
        .def( "empty", &ip_address::empty )
        .def( "clear", &ip_address::clear )
        .def( "__str__", &ip_address::to_string )
        .def( "__eq__", &ip_address::operator== )
        .def( "__ne__", &ip_address::operator!= )
        .def( "get_components", []( const ip_address & self, uint8_t & b0, uint8_t & b1, uint8_t & b2, uint8_t & b3 ) { self.get_components( b0, b1, b2, b3 ); }, "Get IP address components" )
        .def( "get_components", []( const ip_address & self, uint8_t b[4] ) { self.get_components( b ); }, "Get IP address components" );

    /** rs_eth_config.hpp **/
    
    py::enum_< rs2_eth_link_priority >( m, "link_priority" )
        .value( "usb_only", RS2_LINK_PRIORITY_USB_ONLY )
        .value( "eth_only", RS2_LINK_PRIORITY_ETH_ONLY )
        .value( "eth_first", RS2_LINK_PRIORITY_ETH_FIRST )
        .value( "usb_first", RS2_LINK_PRIORITY_USB_FIRST )
        .value( "dynamic_eth_first", RS2_LINK_PRIORITY_DYNAMIC_ETH_FIRST )
        .value( "dynamic_usb_first", RS2_LINK_PRIORITY_DYNAMIC_USB_FIRST );

    // Bind the eth_config_device class
    py::class_< rs2::eth_config_device, rs2::device > eth_config_device( m, "eth_config_device",
                                                                         "Ethernet configuration extension for devices that support ethernet configuration");
    eth_config_device.def( py::init< rs2::device >(), "device"_a, "Create eth_config_device from regular device" )
        .def( "supports_eth_config", &rs2::eth_config_device::supports_eth_config, "Check if device supports ethernet configuration" )
        .def( "get_link_speed", &rs2::eth_config_device::get_link_speed, "Get Ethernet link speed, 0 if not linked" )
        .def( "get_link_priority", &rs2::eth_config_device::get_link_priority, "Get current link priority setting" )
        .def( "set_link_priority", &rs2::eth_config_device::set_link_priority, "Set link priority", "priority"_a )
        .def( "get_link_timeout", &rs2::eth_config_device::get_link_timeout, "Get current link timeout in milliseconds" )
        .def( "set_link_timeout", &rs2::eth_config_device::set_link_timeout, "Set link timeout in milliseconds", "timeout"_a )
        .def( "get_dds_domain", &rs2::eth_config_device::get_dds_domain, "Get current DDS domain (0-232)" )
        .def( "set_dds_domain", &rs2::eth_config_device::set_dds_domain, "Set DDS domain (0-232)", "domain"_a )
        .def( "get_ip_address", []( const rs2::eth_config_device & self )
        {
            rs2_ip_address configured_ip, actual_ip;
            self.get_ip_address( configured_ip, actual_ip );
            return py::make_tuple( ip_address( configured_ip ), ip_address (actual_ip ) );
        }, "Get current IP address. Returns tuple of (configured_ip, actual_ip)" )
        .def( "set_ip_address", []( const rs2::eth_config_device & self, const ip_address & ip )
        {
            rs2_ip_address rs2_ip;
            ip.get_components( rs2_ip );
            self.set_ip_address( rs2_ip );
        }, "Set IP address", "ip"_a )
        .def( "get_netmask", []( const rs2::eth_config_device & self )
        {
            rs2_ip_address configured_netmask, actual_netmask;
            self.get_netmask( configured_netmask, actual_netmask );
            return py::make_tuple( ip_address( configured_netmask ), ip_address( actual_netmask ) );
        }, "Get current network mask. Returns tuple of (configured_netmask, actual_netmask)" )
        .def( "set_netmask", []( const rs2::eth_config_device & self, const ip_address & netmask )
        {
            rs2_ip_address rs2_netmask;
            netmask.get_components( rs2_netmask );
            self.set_netmask( rs2_netmask );
        }, "Set network mask", "netmask"_a )
        .def( "get_gateway", []( const rs2::eth_config_device & self )
        {
            rs2_ip_address configured_gateway, actual_gateway;
            self.get_gateway( configured_gateway, actual_gateway );
            return py::make_tuple( ip_address( configured_gateway ), ip_address( actual_gateway ) );
        }, "Get current gateway address. Returns tuple of (configured_gateway, actual_gateway)" )
        .def( "set_gateway", []( const rs2::eth_config_device & self, const ip_address & gateway )
        {
            rs2_ip_address rs2_gateway;
            gateway.get_components( rs2_gateway );
            self.set_gateway( rs2_gateway );
        }, "Set gateway address", "gateway"_a )
        .def( "get_dhcp_config", []( const rs2::eth_config_device & self )
        {
            bool enabled;
            uint8_t timeout;
            self.get_dhcp_config( enabled, timeout );
            return py::make_tuple( enabled, timeout );
        }, "Get DHCP configuration. Returns tuple of (enabled, timeout)" )
        .def( "set_dhcp_config", []( const rs2::eth_config_device & self, bool enabled, uint8_t timeout )
        {
        self.set_dhcp_config( enabled, timeout );
        }, "Set DHCP configuration", "enabled"_a, "timeout"_a )
        .def( "get_mtu", &rs2::eth_config_device::get_mtu, "Get MTU (Maximum Transmission Unit) in bytes" )
        .def( "set_mtu", []( const rs2::eth_config_device & self, uint32_t mtu ) { self.set_mtu( mtu ); }, "Set MTU (Maximum Transmission Unit)", "mtu"_a )
        .def( "get_transmission_delay", &rs2::eth_config_device::get_transmission_delay, "Get transmission delay in microseconds" )
        .def( "set_transmission_delay", []( const rs2::eth_config_device & self, uint32_t delay ) { self.set_transmission_delay( delay ); }, "Set transmission delay", "delay"_a )
        .def( "get_udp_ttl", &rs2::eth_config_device::get_udp_ttl, "Get UDP TTL (Time To Live)" )
        .def( "set_udp_ttl", []( const rs2::eth_config_device & self, uint8_t ttl ) { self.set_udp_ttl( ttl ); }, "Set UDP TTL (Time To Live)", "ttl"_a )
        .def( "restore_defaults", &rs2::eth_config_device::restore_defaults, "Restores configuration to factory settings" );

    // Add utility functions as module-level functions
    m.def( "supports_eth_config", []( const rs2::device & device )
    {
        rs2_error * error = nullptr;
        int result = rs2_supports_eth_config( device.get().get(), &error );
        if( error )
        {
            std::string error_msg = rs2_get_error_message( error );
            rs2_free_error( error );
            throw rs2::error( error_msg );
        }
        return result != 0;
    }, "Check if device supports ethernet configuration", "device"_a );
    /** end rs_eth_config.hpp **/
}
