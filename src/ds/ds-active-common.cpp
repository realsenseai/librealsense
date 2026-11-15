// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2022 RealSense, Inc. All Rights Reserved.

#include "ds-active-common.h"
#include "d400/d400-private.h" // for RS_USB2_PID
#include "ds-options.h"

#include  <src/backend-device.h>

namespace librealsense
{
    using namespace ds;

    ds_active_common::ds_active_common( const std::shared_ptr< uvc_sensor > & raw_depth_ep,
                                        synthetic_sensor & depth_ep,
                                        backend_device * owner,
                                        ds_caps device_capabilities,
                                        std::shared_ptr< hw_monitor > hw_monitor,
                                        firmware_version fw_version )
        : _raw_depth_ep( raw_depth_ep )
        , _depth_ep( depth_ep )
        , _owner( owner )
        , _device_capabilities( device_capabilities )
        , _hw_monitor( hw_monitor )
        , _fw_version( fw_version )
    {
    }

    void ds_active_common::register_options()
    {
        //Projector's capacity is established based on actual HW capabilities
        auto pid = _owner->get_pid();
        if ((pid != RS_USB2_PID) && ((_device_capabilities & ds_caps::CAP_ACTIVE_PROJECTOR) == ds_caps::CAP_ACTIVE_PROJECTOR))
        {
            //EMITTER ENABLED OPTION
            auto emitter_enabled = std::make_shared<emitter_option>(_raw_depth_ep);

            //LASER POWER OPTION
            auto laser_power = std::make_shared<uvc_xu_option<uint16_t>>(_raw_depth_ep,
                depth_xu,
                DS5_LASER_POWER,
                "Manual laser power in mw. applicable only when laser power mode is set to Manual");

            auto laser_power_auto_disabling = std::make_shared<auto_disabling_control>(
                laser_power,
                emitter_enabled,
                std::vector<float>{0.f, 2.f}, 1.f);

            // Not gated by HDR: the HDR sub-preset locks only exposure and gain, FW applies laser controls during HDR
            _depth_ep.register_option(RS2_OPTION_EMITTER_ENABLED, emitter_enabled);
            _depth_ep.register_option(RS2_OPTION_LASER_POWER, laser_power_auto_disabling);
        }
        else
        {
            LOG_WARNING("Projector capacity is overrided and disabled by FW\nDevice PID = 0x" << std::hex << pid
                << std::dec << ", Capabilities Vector = [" << _device_capabilities << "]");
        }
    }
}
