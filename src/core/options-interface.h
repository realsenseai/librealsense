// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2023 RealSense, Inc. All Rights Reserved.

#pragma once

#include "option-interface.h"
#include "extension.h"

#include <librealsense2/h/rs_option.h>
#include <librealsense2/h/rs_composite_option.h>
#include <src/composite-option-interface.h>
#include <vector>
#include <string>
#include <rsutils/subscription.h>
#include <src/core/options-watcher.h>


namespace librealsense {


class options_interface : public recordable< options_interface >
{
public:
    virtual option & get_option( rs2_option id ) = 0;
    virtual const option & get_option( rs2_option id ) const = 0;
    virtual bool supports_option( rs2_option id ) const = 0;
    virtual std::vector< rs2_option > get_supported_options() const = 0;
    virtual std::string const & get_option_name( rs2_option ) const = 0;

    // Composite-option twin of the five methods above - a fully separate identity space, never
    // mixed with the scalar rs2_option accessors: get_supported_options() and
    // get_supported_composite_options() are two disjoint enumerations.
    virtual composite_option_interface & get_composite_option( rs2_composite_option_id id ) = 0;
    virtual const composite_option_interface & get_composite_option( rs2_composite_option_id id ) const = 0;
    virtual bool supports_composite_option( rs2_composite_option_id id ) const = 0;
    virtual std::vector< rs2_composite_option_id > get_supported_composite_options() const = 0;
    virtual std::string const & get_composite_option_name( rs2_composite_option_id ) const = 0;

    virtual ~options_interface() = default;
    virtual rsutils::subscription register_options_changed_callback(options_watcher::callback&& cb) = 0;

    // Composites are reported to the embedded filters that own them, not to the sensor
    virtual rsutils::subscription register_composite_options_changed_callback( options_watcher::composite_callback && )
    {
        return rsutils::subscription();
    }
};

MAP_EXTENSION( RS2_EXTENSION_OPTIONS, librealsense::options_interface );


}  // namespace librealsense
