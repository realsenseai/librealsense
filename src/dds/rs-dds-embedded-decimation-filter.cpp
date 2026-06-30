// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2025 RealSense, Inc. All Rights Reserved.

#include "rs-dds-embedded-decimation-filter.h"
#include <stdexcept>
#include <string>
#include <cstring>
#include <algorithm>
#include <rsutils/json.h>
#include <src/core/options-registry.h>
#include <realdds/dds-option.h>
#include "rs-dds-option.h"

using rsutils::json;

namespace librealsense {

    rs_dds_embedded_decimation_filter::rs_dds_embedded_decimation_filter(const std::shared_ptr< realdds::dds_embedded_filter >& dds_embedded_filter,
        set_embedded_filter_callback set_embedded_filter_cb,
        query_embedded_filter_callback query_embedded_filter_cb)
        : rs_dds_embedded_filter(dds_embedded_filter, set_embedded_filter_cb, query_embedded_filter_cb)
    {
        // Initialize options by calling add_option for each DDS option
        for (auto& filter_option : _dds_ef->get_options())
        {
            add_option(filter_option);
        }
    }

    void rs_dds_embedded_decimation_filter::add_option(std::shared_ptr< realdds::dds_option > option)
    {
        rs2_option option_id;

        // Map DDS option names to standard RealSense option IDs
        if (option->get_name() == TOGGLE_OPTION_NAME)
        {
            option_id = RS2_OPTION_EMBEDDED_FILTER_ENABLED;
        }
        else if (option->get_name() == MAGNITUDE_OPTION_NAME)
        {
            option_id = RS2_OPTION_FILTER_MAGNITUDE;
        }
        else
        {
            throw std::runtime_error("option '" + option->get_name() + "' not in this filter");
        }

        if (get_option_handler(option_id))
            throw std::runtime_error("option '" + option->get_name() + "' already exists");

        // In below implementation:
        // The options setting is always only for only one option (toggle OR magnitude), since the API is set_option
        // - setting one option leads to
        //   * setting the new value for one option, and
        //   * sending also the other current values for the other filter's values
        // - getting one option leads to:
        //   * returning only the relevant option's value
        //   * the getting of the filter's options communicating with the device by DDS
        //     is not necessary, since the value is already automatically updated by the set action reply
        auto opt = std::make_shared< rs_dds_option >(
            option,
            [=](json value) // set_option cb for the filter's options
            {
                // create a proper option json with name and value
                json option_with_value = dds_option_to_name_and_value_json(option, value);
                // validate values
                validate_filter_option(option_with_value);
                // set updated options to the remote device
                _set_ef_cb(option_with_value);
                // Delegate to DDS filter
                _dds_ef->set_options(option_with_value);
            },
            [=]() -> json // get_option cb for the filter's options
            {
                return option->get_value();
            });
        register_option(option_id, opt);
        _options_watcher.register_option(option_id, opt);
    }

    void rs_dds_embedded_decimation_filter::validate_filter_option( rsutils::json const & option_j ) const
    {
        // Toggle: int with FW-advertised range [0..1] - rs_dds_option range check covers it.
        if( option_j.contains( MAGNITUDE_OPTION_NAME ) )
        {
            validate_magnitude_option( option_j );
        }
        else if( ! option_j.contains( TOGGLE_OPTION_NAME ) )
        {
            throw std::runtime_error( "Option json must contain a key matching one of the options name" );
        }
        // Validation passed - parameter is valid
    }

    void rs_dds_embedded_decimation_filter::validate_magnitude_option( rsutils::json const & opt_j ) const
    {
        auto dds_magnitude = find_dds_option_by_name(_dds_ef->get_options(), MAGNITUDE_OPTION_NAME);
        int32_t mag_val = opt_j[MAGNITUDE_OPTION_NAME].get<int32_t>();
        // Check range using DDS option
        if (!dds_magnitude->get_minimum_value().is_null() && mag_val < dds_magnitude->get_minimum_value().get<int32_t>())
        {
            throw std::invalid_argument("Magnitude value " + std::to_string(mag_val) +
                " is below minimum " + std::to_string(dds_magnitude->get_minimum_value().get<int32_t>()));
        }
        if (!dds_magnitude->get_maximum_value().is_null() && mag_val > dds_magnitude->get_maximum_value().get<int32_t>())
        {
            throw std::invalid_argument("Magnitude value " + std::to_string(mag_val) +
                " is above maximum " + std::to_string(dds_magnitude->get_maximum_value().get<int32_t>()));
        }
        // Additional validation: Decimation magnitude must be exactly 2 for depth sensor
        if (mag_val != DECIMATION_MAGNITUDE) {
            throw std::invalid_argument("Decimation filter magnitude must be " + std::to_string(DECIMATION_MAGNITUDE) + ". Received: " + std::to_string(mag_val));
        }
    }
}  // namespace librealsense
