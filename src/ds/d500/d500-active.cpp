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

        // Emitter Always On (Laser Always On) - projector control on the D500 active SKUs, except the dual-RGB (2C)
        // ones where the strobe is FW controlled and keeping the laser on exposes its pattern on the color streams.
        // Newer 5x5 / D585 SKUs use the APM_STROBE opcodes; D555 uses the legacy LASERONCONST opcode.
        if( d500_dual_rgb_pids.find( get_pid() ) == d500_dual_rgb_pids.end() )
        {
            fw_cmd emitter_get_opcode = APM_STROBE_GET;
            fw_cmd emitter_set_opcode = APM_STROBE_SET;
            if( get_pid() == D555_PID )
            {
                emitter_get_opcode = LASERONCONST;
                emitter_set_opcode = LASERONCONST;
            }
            auto emitter_always_on_opt = std::make_shared<emitter_always_on_option>( _hw_monitor, emitter_get_opcode, emitter_set_opcode );
            get_depth_sensor().register_option( RS2_OPTION_EMITTER_ALWAYS_ON, emitter_always_on_opt );
        }
    }
}
