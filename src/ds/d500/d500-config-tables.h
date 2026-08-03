// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

#include "core/extension.h"
#include <librealsense2/h/rs_types.h>
#include <string>

namespace librealsense
{
    // Device-level access to the D500 flash configuration tables. All D500 cameras share the
    // same flash layout, so these live on the device rather than on the safety sensor (which
    // only D585S exposes).
    class d500_config_tables
    {
    public:
        virtual ~d500_config_tables() = default;
        virtual std::string get_safety_preset(int index) const = 0;
        virtual void set_safety_preset(int index, const std::string& sp_json_str) const = 0;
        virtual std::string get_safety_interface_config(rs2_calib_location loc) const = 0;
        virtual void set_safety_interface_config(const std::string& sic_json_str) const = 0;
        virtual std::string get_application_config() const = 0;
        virtual void set_application_config(const std::string& application_config_json_str) const = 0;

        // High-level access to individual exposed parameters, hiding table layout from the caller.
        // set: a flat JSON object of { "<param name>": <value>, ... }; each name is routed to its
        // table(s) and field(s), and each affected table is read-modified-written once.
        // get: returns a flat JSON object with the current value of every exposed parameter.
        virtual void set_parameters(const std::string& params_json_str) const = 0;
        virtual std::string get_parameters() const = 0;
    };
    MAP_EXTENSION(RS2_EXTENSION_D500_CONFIG_TABLES, librealsense::d500_config_tables);
}
