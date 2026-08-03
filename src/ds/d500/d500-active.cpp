// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2022 RealSense, Inc. All Rights Reserved.

#include <mutex>
#include <chrono>
#include <vector>
#include <iterator>
#include <cstddef>

#include "device.h"
#include "image.h"
#include "metadata-parser.h"

#include "d500-active.h"
#include "d500-private.h"
#include "d500-info.h"
#include "d500-options.h"
#include "ds/ds-options.h"
#include "ds/ds-timestamp.h"

namespace librealsense
{
    d500_active::d500_active( std::shared_ptr< const d500_info > const & dev_info )
    : device( dev_info )
    , d500_device( dev_info )
    {
        using namespace ds;

        _ds_active_common = std::make_shared<ds_active_common>(get_raw_depth_sensor(), get_depth_sensor(), this,
            _device_capabilities, _hw_monitor, _fw_version);

        _ds_active_common->register_options();

        // Emitter Always On (Laser Always On) - projector control common to all D500 active SKUs.
        // Newer 5x5 / D585 SKUs use the APM_STROBE opcodes (ds::d500_fw_cmd); D555 uses the legacy
        // LASERONCONST (ds::fw_cmd). uint8_t is the common underlying type — the two enums live in
        // different families and don't cross-convert.
        uint8_t emitter_get_opcode = d500_fw_cmd::APM_STROBE_GET;
        uint8_t emitter_set_opcode = d500_fw_cmd::APM_STROBE_SET;
        if( get_pid() == D555_PID )
        {
            emitter_get_opcode = fw_cmd::LASERONCONST;
            emitter_set_opcode = fw_cmd::LASERONCONST;
        }
        auto emitter_always_on_opt = std::make_shared<emitter_always_on_option>( _hw_monitor, emitter_get_opcode, emitter_set_opcode );
        get_depth_sensor().register_option( RS2_OPTION_EMITTER_ALWAYS_ON, emitter_always_on_opt );

        // Emitter on/off, supported on all D500 active SKUs but D585S, where the projector is owned by safety
        if( get_pid() != D585S_PID && ( _fw_version >= firmware_version( "7.58.40805.12940" ) ) )
        {
            auto alternating_emitter_opt = std::make_shared<alternating_emitter_option>( *_hw_monitor, true );

            std::vector<std::pair<std::shared_ptr<option>, std::string>> options_and_reasons = { std::make_pair(emitter_always_on_opt,
                    "Emitter ON/OFF cannot be set while Emitter always ON is enabled") };
            get_depth_sensor().register_option(RS2_OPTION_EMITTER_ON_OFF,
                std::make_shared<gated_option>(
                    alternating_emitter_opt,
                    options_and_reasons));
        }
    }
}
