// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2024 RealSense, Inc. All Rights Reserved.
#pragma once

#include <librealsense2/rs.hpp>
#include <rsutils/type/ip-address.h>
#include <imgui.h>

#include <string>

namespace rs2
{
    class ux_window;

    class dds_model
    {

    public:
        dds_model( rs2::device dev );

        void render_dds_config_window( ux_window & window, std::string & error_message );

        void open_dds_tool_window();

        void close_window() { ImGui::CloseCurrentPopup(); }

        bool supports_DDS() const { return _dds_supported; }

    private:
        enum priority
        {
            ETH_FIRST,
            USB_FIRST,
            DYNAMIC
        };

        rs2::device _device;
        rs2::eth_config_device _eth_device;

        uint32_t _domain_current, _domain_to_set;
        rs2_eth_link_priority _link_priority_current, _link_priority_to_set;
        uint32_t _link_timeout_current, _link_timeout_to_set;
        bool _dhcp_enabled_current, _dhcp_enabled_to_set;
        uint8_t _dhcp_timeout_current, _dhcp_timeout_to_set;
        rsutils::type::ip_address _ip_current, _ip_to_set;
        rsutils::type::ip_address _netmask_current, _netmask_to_set;
        rsutils::type::ip_address _gateway_current, _gateway_to_set;
        uint32_t _mtu_current, _mtu_to_set;
        uint32_t _tx_delay_current, _tx_delay_to_set;
        uint32_t _ttl_current, _ttl_to_set;
        uint32_t _link_speed_read_only;

        bool _window_open;
        bool _no_reset;
        bool _dds_supported;

        void load_eth_config_from_device();
        void save_eth_config_in_device( std::string & error_message, bool reset_to_default = false );
        void ip_input_text( std::string label, rsutils::type::ip_address & ip ) const;
        priority classify_priority( rs2_eth_link_priority pr ) const;
        bool check_DDS_support();
        void reset_to_current_device_values();
        bool has_changed_values() const;
    };
}
