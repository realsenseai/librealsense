// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2024 RealSense, Inc. All Rights Reserved.

#include "dds-model.h"
#include "device-model.h"
#include "ux-window.h"
#include <rsutils/string/from.h>
#include <realsense_imgui.h>

#include <cstdio>

using namespace rs2;
using rsutils::type::ip_address;


dds_model::dds_model( rs2::device dev )
    : _device( dev )
    , _eth_device( _device )
    , _window_open( false )
    , _no_reset( false )
    , _dds_supported( false )
{
    if( check_DDS_support() )
    {
        load_eth_config_from_device();
        reset_to_current_device_values();
        _dds_supported = true;
    }
}

void dds_model::load_eth_config_from_device()
{
    _domain_current = _eth_device.get_dds_domain();
    _link_priority_current = _eth_device.get_link_priority();
    _link_timeout_current = _eth_device.get_link_timeout();
    _eth_device.get_dhcp_config( _dhcp_enabled_current, _dhcp_timeout_current );

    rs2_ip_address configured, actual;
    _eth_device.get_ip_address( configured, actual );
    _ip_current = configured;
    _eth_device.get_netmask( configured, actual );
    _netmask_current = configured;
    _eth_device.get_gateway( configured, actual );
    _gateway_current = configured;

    _mtu_current = _eth_device.get_mtu();
    _tx_delay_current = _eth_device.get_transmission_delay();
    _ttl_current = _eth_device.get_udp_ttl();

    _link_speed_read_only = _eth_device.get_link_speed();
}

void dds_model::save_eth_config_in_device( std::string & error_message, bool reset_to_default )
{
    try
    {
        if( reset_to_default )
        {
            _eth_device.restore_defaults();
        }
        else
        {
            _eth_device.set_dds_domain( _domain_to_set );
            _eth_device.set_link_priority( _link_priority_to_set );
            _eth_device.set_link_timeout( _link_timeout_to_set );
            _eth_device.set_dhcp_config( _dhcp_enabled_to_set, _dhcp_timeout_to_set);

            rs2_ip_address addr;
            _ip_to_set.get_components( addr );
            _eth_device.set_ip_address( addr);
            _netmask_to_set.get_components( addr );
            _eth_device.set_netmask( addr );
            _gateway_to_set.get_components( addr );
            _eth_device.set_gateway( addr );

            _eth_device.set_mtu( _mtu_to_set );
            _eth_device.set_transmission_delay( _tx_delay_to_set );
            _eth_device.set_udp_ttl( static_cast< uint8_t >( _ttl_to_set ) );
        }

        if( ! _no_reset )
        {
            close_window();
            _device.hardware_reset();
        }
    }
    catch( const std::exception & e )
    {
        error_message = rsutils::string::from() << "Failed to set Ethernet configuration: " << e.what();
        close_window();
    }
}

rs2::dds_model::priority dds_model::classify_priority( rs2_eth_link_priority pr ) const
{
    if( pr == RS2_LINK_PRIORITY_USB_ONLY || pr == RS2_LINK_PRIORITY_USB_FIRST )
        return priority::USB_FIRST;

    if( pr == RS2_LINK_PRIORITY_ETH_ONLY || pr == RS2_LINK_PRIORITY_ETH_FIRST )
        return priority::ETH_FIRST;

    return priority::DYNAMIC;
}

bool dds_model::check_DDS_support()
{
    return _eth_device.supports_eth_config();
}

void dds_model::reset_to_current_device_values()
{
    _domain_to_set = _domain_current;
    _link_priority_to_set = _link_priority_current;
    _link_timeout_to_set = _link_timeout_current;
    _dhcp_enabled_to_set = _dhcp_enabled_current;
    _dhcp_timeout_to_set = _dhcp_timeout_current;

    _ip_to_set = _ip_current;
    _netmask_to_set = _netmask_current;
    _gateway_to_set = _gateway_current;

    _mtu_to_set = _mtu_current;
    _tx_delay_to_set = _tx_delay_current;
    _ttl_to_set = _ttl_current;
}

bool dds_model::has_changed_values() const
{
    return _domain_to_set != _domain_current ||
        _link_priority_to_set != _link_priority_current || _link_timeout_to_set != _link_timeout_current ||
        _dhcp_enabled_to_set != _dhcp_enabled_current || _dhcp_timeout_to_set != _dhcp_timeout_current ||
        _ip_to_set != _ip_current || _netmask_to_set != _netmask_current || _gateway_to_set != _gateway_current ||
        _mtu_to_set != _mtu_current || _tx_delay_to_set != _tx_delay_current || _ttl_to_set != _ttl_current;
}

void rs2::dds_model::ip_input_text( std::string label, ip_address & ip ) const
{
    char buffer[16];
    std::string ip_str = ip.to_string();
    std::snprintf( buffer, sizeof( buffer ), "%s", ip_str.c_str() );
    std::string label_name = "##" + label;

    if( ImGui::InputText( label_name.c_str(), buffer, sizeof( buffer ) ) )
    {
        std::string new_ip_str( buffer );
        ip_address new_ip = ip_address( new_ip_str );
        if( new_ip.is_valid() )
        {
            ip = new_ip;
        }
        else
        {
            // Initialize the ImGui buffer with the current IP address in case of invalid input
            std::snprintf( buffer, sizeof( buffer ), "%s", ip.to_string().c_str() );
        }
    }
}

void dds_model::render_dds_config_window( ux_window & window, std::string & error_message )
{
    std::string serial = (_device.supports(RS2_CAMERA_INFO_SERIAL_NUMBER)) ? _device.get_info(RS2_CAMERA_INFO_SERIAL_NUMBER) : "Unknown";
    std::string window_name = rsutils::string::from() << "DDS Configuration" << "##" << serial;
    if( _window_open )
    {
        try
        {
            load_eth_config_from_device();
            reset_to_current_device_values();
            ImGui::OpenPopup( window_name.c_str() );
        }
        catch( std::exception e )
        {
            error_message = e.what();
        }
        _window_open = false;
    }

    // Calculate window position and size
    const float w = 620;
    const float h = 500;
    const float x0 = std::max( window.width() - w, 0.f ) / 2;
    const float y0 = std::max( window.height() - h, 0.f ) / 2;
    ImGui::SetNextWindowPos( { x0, y0 } );
    ImGui::SetNextWindowSize( { w, h } );

    auto flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse
               | ImGuiWindowFlags_NoSavedSettings;

    ImGui::PushStyleColor( ImGuiCol_PopupBg, sensor_bg );
    ImGui::PushStyleColor( ImGuiCol_TextSelectedBg, light_grey );
    ImGui::PushStyleColor( ImGuiCol_Text, light_grey );
    ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 5, 5 ) );
    ImGui::PushStyleVar( ImGuiStyleVar_WindowRounding, 1 );
    ImGui::PushStyleColor( ImGuiCol_Button, button_color );
    ImGui::PushStyleColor( ImGuiCol_ButtonHovered, button_color + 0.1f );
    ImGui::PushStyleColor( ImGuiCol_ButtonActive, button_color + 0.1f );


    if( ImGui::BeginPopupModal( window_name.c_str(), nullptr, flags ) )
    {
        if( error_message != "" )
            ImGui::CloseCurrentPopup();

        // Title
        const char * title_message = "DDS Configuration";
        ImVec2 title_size = ImGui::CalcTextSize( title_message );
        float title_x = ( w - title_size.x - 10 ) / 2.0f;
        ImGui::SetCursorPos( { title_x, 10.0f } );
        ImGui::PushFont( window.get_large_font() );
        ImGui::PushStyleColor( ImGuiCol_Text, white );
        ImGui::Text( "%s", title_message );
        ImGui::PopStyleColor();
        ImGui::PopFont();
        ImGui::Separator();
        ImGui::SetCursorPosY( ImGui::GetCursorPosY() + 15 );

        // Window button sizes
        float button_width = 115.0f;
        float spacing = 10.0f;
        float total_buttons_width = button_width * 3 + spacing * 2;
        float start_x = ( w - total_buttons_width ) / 2.0f;

        // Main Scrollable Section
        ImGui::BeginChild( "MainContent", ImVec2( w - 10, h - 100 ), true );
        ImGui::PushItemWidth( 150.0f );

        ImGui::Text( "Domain ID" );
        ImGui::SameLine();
        int tempDomain = static_cast< int >( _domain_to_set );
        if( ImGui::InputInt( "##Domain ID", &tempDomain ) )
        {
            if( tempDomain < 0 )
                tempDomain = 0;
            if( tempDomain > 232 )
                tempDomain = 232;
            _domain_to_set = static_cast< uint32_t >( tempDomain );
        }

        if( ImGui::CollapsingHeader( "Connection Priority" ) )
        {
            priority connection_priority = classify_priority( _link_priority_to_set );
            ImGui::Text( "Select connection priority:" );
            ImGui::RadioButton( "Ethernet First", reinterpret_cast< int * >( &connection_priority ), 0 );
            if( static_cast< int >( connection_priority ) == 0 )
            {
                ImGui::SameLine();
                ImGui::SetCursorPosX( ImGui::GetCursorPosX() + 50 );
                ImGui::Text( "Link Timeout [ms]" );
                ImGui::SameLine();
                int tempTimeout = static_cast< int >( _link_timeout_to_set );
                if( ImGui::InputInt( "##Link Timeout", &tempTimeout, 100 ) )
                {
                    if( tempTimeout < 2000 )
                        tempTimeout = 2000;
                    if( tempTimeout > 30000 )
                        tempTimeout = 30000;
                    _link_timeout_to_set = static_cast< uint32_t >( tempTimeout );
                }
            }
            ImGui::RadioButton( "USB First", reinterpret_cast< int * >( &connection_priority ), 1 );
            ImGui::RadioButton( "Dynamic Priority", reinterpret_cast< int * >( &connection_priority ), 2 );
            if( ImGui::IsItemHovered() )
            {
                window.link_hovered();
                RsImGui::CustomTooltip( "%s", "Try connection type from last power up, switch if changed" );
            }
            switch( connection_priority )
            {
            case ETH_FIRST:
                _link_priority_to_set = RS2_LINK_PRIORITY_ETH_FIRST;
                break;
            case USB_FIRST:
                _link_priority_to_set = RS2_LINK_PRIORITY_USB_FIRST;
                break;
            case DYNAMIC:
                // If link speed is not 0 than we are connected by Ethernet
                _link_priority_to_set = _link_speed_read_only ? RS2_LINK_PRIORITY_DYNAMIC_ETH_FIRST : RS2_LINK_PRIORITY_DYNAMIC_USB_FIRST;
                break;
            }
        }

        if( ImGui::CollapsingHeader( "Network Configuration" ) )
        {
            ImGui::Checkbox( "Enable DHCP", &_dhcp_enabled_to_set );
            if( ! _dhcp_enabled_to_set )
            {
                ImGui::Text( "Static IP Address" );
                ImGui::SameLine();
                float textbox_align = ImGui::GetCursorPosX();
                ip_input_text( "Static IP Address", _ip_to_set );
                ImGui::Text( "Subnet Mask" );
                ImGui::SameLine();
                ImGui::SetCursorPosX( textbox_align );
                bool maskStylePushed = false;
                ip_input_text( "Subnet Mask", _netmask_to_set );
                ImGui::Text( "Gateway" );
                ImGui::SameLine();
                ImGui::SetCursorPosX( textbox_align );
                ip_input_text( "Gateway", _gateway_to_set );
            }
            else
            {
                ImGui::Text( "DHCP Timeout [seconds]" );
                ImGui::SameLine();
                int tempTimeout = static_cast< int >( _dhcp_timeout_to_set );
                if( ImGui::InputInt( "##DHCP Timeout", &tempTimeout ) )
                {
                    if( tempTimeout > 255 )
                        tempTimeout = 255;
                    _dhcp_timeout_to_set = static_cast< uint8_t >( std::max( 0, tempTimeout ) );
                }
            }
        }

        if( ImGui::CollapsingHeader( "Traffic Shaping" ) )
        {
            ImGui::Text( "MTU [bytes]" );
            ImGui::SameLine();
            int temp_mtu = static_cast< int >( _mtu_to_set );
            if( ImGui::InputInt( "##MTU", &temp_mtu, 500 ) )
            {
                if( temp_mtu < 500 )
                    temp_mtu = 500;
                if( temp_mtu > 9000 )
                    temp_mtu = 9000;
                _mtu_to_set = static_cast< uint32_t >( temp_mtu );
            }

            ImGui::Text( "Transmission Delay [us]" );
            ImGui::SameLine();
            int temp_delay = static_cast< int >( _tx_delay_to_set );
            if( ImGui::InputInt( "##Transmission Delay", &temp_delay, 3 ) )
            {
                if( temp_delay < 0 )
                    temp_delay = 0;
                if( temp_delay > 144 )
                    temp_delay = 144;
                _tx_delay_to_set = static_cast< uint32_t >( temp_delay );
            }

            ImGui::Text( "UDP TTL [hops]" );
            ImGui::SameLine();
            int temp_ttl = static_cast< int >( _ttl_to_set );
            if( ImGui::InputInt( "##UDP TTL", &temp_ttl ) )
            {
                if( temp_ttl < 0 )
                    temp_ttl = 0;
                if( temp_ttl > 255 )
                    temp_ttl = 255;
                _ttl_to_set = static_cast< uint32_t >( temp_ttl );
            }
            ImGui::SameLine();
            ImGui::Text( "(0 means system default)" );
        }
        
        ImGui::Separator();
        ImGui::Checkbox( "No Reset after changes", &_no_reset );
        if( ImGui::Button( "Revert changes" ) )
        {
            reset_to_current_device_values();
        }
        if( ImGui::IsItemHovered() )
        {
            window.link_hovered();
            RsImGui::CustomTooltip( "%s", "Revert to current configuration values" );
        }

        ImGui::PopItemWidth();
        ImGui::EndChild();

        ImGui::SetCursorPosY( ImGui::GetCursorPosY() + 8 );

        ImGui::SetCursorPosX( start_x );

        if( ImGui::Button( "Cancel", ImVec2( button_width, 25 ) ) )
        {
            close_window();
        }
        if( ImGui::IsItemHovered() )
        {
            window.link_hovered();
            RsImGui::CustomTooltip( "%s", "Close without saving any changes" );
        }
        ImGui::SameLine();
        if( ImGui::Button( "Factory Reset", ImVec2( button_width, 25 ) ) )
        {
            save_eth_config_in_device( error_message, true );
            close_window();
        }
        if( ImGui::IsItemHovered() )
        {
            window.link_hovered();
            RsImGui::CustomTooltip( "%s", "Reset settings back to defult values" );
        }
        ImGui::SameLine();
        RsImGui::RsImButton(
            [&]()
            {
                if( ImGui::ButtonEx( "Apply", ImVec2( button_width, 25 ) ) )
                {
                    save_eth_config_in_device( error_message );
                    close_window();
                };
            },
            ! has_changed_values() );
        if( ImGui::IsItemHovered() )
        {
            window.link_hovered();
            RsImGui::CustomTooltip( "%s", "Apply changes" );
        }
        if( ImGui::BeginPopupModal( "No Changes Needed", NULL, ImGuiWindowFlags_AlwaysAutoResize ) )
        {
            ImGui::Text( "No changes were made to the configuration." );

            if( ImGui::Button( "OK", ImVec2( 100, 25 ) ) )
            {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleColor( 6 );
    ImGui::PopStyleVar( 2 );
}

void rs2::dds_model::open_dds_tool_window()
{
    _window_open = true;
}