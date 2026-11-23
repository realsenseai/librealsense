// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2025 RealSense, Inc. All Rights Reserved.

#include <librealsense2/rs.hpp>

#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>

#include <common/cli.h>

// Helper function to convert link priority enum to readable string
std::string link_priority_to_string( rs2_eth_link_priority priority )
{
    const char * str = rs2_eth_link_priority_to_string( priority );
    return str ? str : "unknown";
}

// Helper function to format IP address for display
std::string format_ip_address( const rs2_ip_address & addr )
{
    if( ! addr[0] && ! addr[1] && ! addr[2] && ! addr[3] )
        return "N/A";
    return std::to_string( addr[0] ) + '.' + std::to_string( addr[1] ) + '.' + std::to_string( addr[2] ) + '.' + std::to_string( addr[3] );
}

// Function to get and display link speed
void print_link_speed( const rs2::eth_config_device & eth_device )
{
    try
    {
        int speed = eth_device.get_link_speed();

        std::cout << "Link Speed:    ";
        if( speed > 0 )
            std::cout << speed << " Mbps" << std::endl;
        else
            std::cout << "No link / Link down" << std::endl;
    }
    catch( const rs2::error & e )
    {
        std::cerr << "Error getting link speed: " << e.what() << std::endl;
    }
}

// Function to print current Ethernet configuration
void print_ethernet_config( const rs2::eth_config_device & eth_device )
{
    std::cout << "\n=== Current Ethernet Configuration ===" << std::endl;

    try
    {
        // Get link speed first
        print_link_speed( eth_device );

        // Get link priority
        auto priority = eth_device.get_link_priority();
        std::cout << "Link Priority: " << link_priority_to_string( priority ) << std::endl;

        // Get link timeout
        auto timeout = eth_device.get_link_timeout();
        std::cout << "Link Timeout:  " << timeout << " milliseconds" << std::endl;

        // Get IP configuration - now using the new API with both configured and actual values
        rs2_ip_address configured_ip, actual_ip;
        rs2_ip_address configured_netmask, actual_netmask;
        rs2_ip_address configured_gateway, actual_gateway;

        eth_device.get_ip_address( configured_ip, actual_ip );
        eth_device.get_netmask( configured_netmask, actual_netmask );
        eth_device.get_gateway( configured_gateway, actual_gateway );

        std::cout << "IP Address:    " << format_ip_address( configured_ip ) << " (actual: " << format_ip_address( actual_ip ) << ")" << std::endl;
        std::cout << "Network Mask:  " << format_ip_address( configured_netmask ) << " (actual: " << format_ip_address( actual_netmask ) << ")" << std::endl;
        std::cout << "Gateway:       " << format_ip_address( configured_gateway ) << " (actual: " << format_ip_address( actual_gateway ) << ")" << std::endl;

        // Get DHCP configuration
        bool dhcp_enabled;
        uint8_t dhcp_timeout;
        eth_device.get_dhcp_config( dhcp_enabled, dhcp_timeout );
        std::cout << "DHCP Enabled:  " << ( dhcp_enabled ? "Yes" : "No" ) << std::endl;
        std::cout << "DHCP Timeout:  " << std::to_string( dhcp_timeout ) << " seconds" << std::endl;

        // Get MTU
        auto mtu = eth_device.get_mtu();
        std::cout << "MTU:           " << mtu << " bytes" << std::endl;

        // Get transmission delay
        auto delay = eth_device.get_transmission_delay();
        std::cout << "TX Delay:      " << delay << " microseconds" << std::endl;
    }
    catch( const rs2::error & e )
    {
        std::cerr << "Error reading configuration: " << e.what() << std::endl;
    }
}

// Function to demonstrate changing Ethernet configuration
void demonstrate_config_changes( rs2::eth_config_device & eth_device )
{
    std::cout << "\n=== Demonstrating Configuration Changes ===" << std::endl;

    // Store original values
    rs2_eth_link_priority original_priority = RS2_LINK_PRIORITY_ETH_FIRST;
    bool original_dhcp_enabled = false;
    uint8_t original_dhcp_timeout = 0;

    try
    {
        // Store original values
        original_priority = eth_device.get_link_priority();
        eth_device.get_dhcp_config( original_dhcp_enabled, original_dhcp_timeout );

        std::cout << "\nDemonstrating link priority change..." << std::endl;

        rs2_eth_link_priority priority_to_set = original_priority == RS2_LINK_PRIORITY_ETH_FIRST ? RS2_LINK_PRIORITY_USB_FIRST : RS2_LINK_PRIORITY_ETH_FIRST;
        std::cout << "Setting link priority to " << link_priority_to_string( priority_to_set ) << std::endl;
        eth_device.set_link_priority( priority_to_set );
        // Verify the change
        auto new_priority = eth_device.get_link_priority();
        if( new_priority != priority_to_set )
            std::cerr << "Failed to set connection priority to " << link_priority_to_string( priority_to_set ) << std::endl;
        else
            std::cout << "Link priority changed from " << link_priority_to_string( original_priority ) << " to " << link_priority_to_string( new_priority ) << std::endl;

        std::cout << "\nDemonstrating DHCP timeout change..." << std::endl;

        uint8_t dhcp_timeout_to_set = original_dhcp_timeout == 60 ? 30 : 60;
        std::cout << "Setting DHCP timeout to " << std::to_string( dhcp_timeout_to_set ) << " seconds " << std::endl;
        eth_device.set_dhcp_config( original_dhcp_enabled, dhcp_timeout_to_set );
        // Verify the change
        bool new_dhcp_enabled;
        uint8_t new_dhcp_timeout;
        eth_device.get_dhcp_config( new_dhcp_enabled, new_dhcp_timeout );
        if( new_dhcp_timeout != dhcp_timeout_to_set )
            std::cerr << "Failed to set DHCP timeout to " << std::to_string( dhcp_timeout_to_set ) << " seconds" << std::endl;
        else
            std::cout << "DHCP timeout changed from " << std::to_string( original_dhcp_timeout ) << " to " << std::to_string( new_dhcp_timeout ) << " seconds" << std::endl;
    }
    catch( const rs2::error & e )
    {
        std::cerr << "Error during configuration changes: " << e.what() << std::endl;
    }

    try
    {
        std::cout << "\nRestoring original configuration..." << std::endl;

        // Restore original values
        eth_device.set_link_priority( original_priority );
        eth_device.set_dhcp_config( original_dhcp_enabled, original_dhcp_timeout );

        std::cout << "Configuration restored to original values" << std::endl;
    }
    catch( const rs2::error & e )
    {
        std::cerr << "Error while restoring configuration: " << e.what() << std::endl;
    }
}

int main( int argc, char * argv[] )
try
{
    rs2::rum::set_app_name( "rs-eth-config" );
    auto settings = rs2::cli( "Ethernet Configuration Example" ).process( argc, argv );

    std::cout << "RealSense Ethernet Configuration Example" << std::endl;
    std::cout << "=========================================" << std::endl;

    // Create RealSense context
    rs2::context ctx( settings.dump() );

    // Find devices
    auto devices = ctx.query_devices();
    if( devices.size() == 0 )
    {
        std::cerr << "No RealSense devices found!" << std::endl;
        return EXIT_FAILURE;
    }

    // Find a device that supports Ethernet configuration
    rs2::eth_config_device eth_device;
    bool found_eth_device = false;

    for( auto && dev : devices )
    {
        try
        {
            // Try to create an eth_config_device
            rs2::eth_config_device candidate( dev );

            // Check if it actually supports ethernet config
            if( candidate.supports_eth_config() )
            {
                eth_device = candidate;
                found_eth_device = true;

                std::cout << "\nFound Ethernet-capable device:" << std::endl;
                std::cout << "  Name: " << dev.get_info( RS2_CAMERA_INFO_NAME ) << std::endl;
                std::cout << "  Serial: " << dev.get_info( RS2_CAMERA_INFO_SERIAL_NUMBER ) << std::endl;
                if( dev.supports( RS2_CAMERA_INFO_FIRMWARE_VERSION ) )
                {
                    std::cout << "  FW Version: " << dev.get_info( RS2_CAMERA_INFO_FIRMWARE_VERSION ) << std::endl;
                }
                break;
            }
        }
        catch( const rs2::error & )
        {
            // This device doesn't support Ethernet configuration, continue searching
            continue;
        }
    }

    if( ! found_eth_device )
    {
        std::cerr << "No devices found that support Ethernet configuration!" << std::endl;
        std::cout << "\nNote: Ethernet configuration is only supported on devices with Ethernet capability."
                  << std::endl;
        return EXIT_FAILURE;
    }

    // Print current configuration
    print_ethernet_config( eth_device );

    // Demonstrate configuration changes
    // Not setting IP configuration as it might cause disconnection of network device
    // Not setting MTU or transmission delay as it is not supported by all FW versions
    demonstrate_config_changes( eth_device );

    return EXIT_SUCCESS;
}
catch( const rs2::error & e )
{
    std::cerr << "RealSense error calling " << e.get_failed_function() << "(" << e.get_failed_args() << "):\n    "
              << e.what() << std::endl;
    return EXIT_FAILURE;
}
catch( const std::exception & e )
{
    std::cerr << "Error: " << e.what() << std::endl;
    return EXIT_FAILURE;
}
