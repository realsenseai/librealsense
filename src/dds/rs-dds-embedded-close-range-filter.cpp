// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "rs-dds-embedded-close-range-filter.h"
#include "rs-dds-option.h"
#include <realdds/dds-option.h>
#include <rsutils/json.h>
#include <stdexcept>
#include <string>

using rsutils::json;

namespace librealsense {

    rs_dds_embedded_close_range_filter::rs_dds_embedded_close_range_filter(const std::shared_ptr< realdds::dds_embedded_filter >& dds_embedded_filter,
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

    void rs_dds_embedded_close_range_filter::add_option(std::shared_ptr< realdds::dds_option > option)
    {
        rs2_option option_id;

        // Map FW-advertised DDS option names to RealSense option IDs.
        if (option->get_name() == ENABLE_OPTION_NAME)
        {
            option_id = RS2_OPTION_EMBEDDED_FILTER_ENABLED;
        }
        else if (option->get_name() == DOWNSCALE_RATIO_OPTION_NAME)
        {
            // FW advertises ratio as dds_enum_option with choices {"1","2","4"}.
            // rs_dds_option exposes enum options as float = choice index, range [0..2].
            option_id = RS2_OPTION_DOWNSCALE_RATIO;
        }
        else if (option->get_name() == DISPARITY_SHIFT_OPTION_NAME)
        {
            option_id = RS2_OPTION_DISPARITY_SHIFT;
        }
        else if (option->get_name() == THRESHOLD_OPTION_NAME)
        {
            option_id = RS2_OPTION_THRESHOLD;
        }
        else
        {
            throw std::runtime_error("option '" + option->get_name() + "' not in this filter");
        }

        if (get_option_handler(option_id))
            throw std::runtime_error("option '" + option->get_name() + "' already exists in filter");

        auto opt = std::make_shared< rs_dds_option >(
            option,
            [=]( json value )
            {
                json option_with_value = dds_option_to_name_and_value_json( option, value );
                validate_filter_option( option_with_value );
                _set_ef_cb( option_with_value );
                _dds_ef->set_options( option_with_value );
            },
            [=]() -> json
            { return option->get_value(); } );
        register_option( option_id, opt );
        _options_watcher.register_option( option_id, opt );
    }

    void rs_dds_embedded_close_range_filter::validate_filter_option( rsutils::json const & option_j ) const
    {
        // Enable: int with FW-advertised range [0..1] - rs_dds_option range check covers it, no extra host-side check needed.
        // Downscale ratio: dds_enum_option (choices "1"/"2"/"4") - rs_dds_option validates the choice index.
        if( option_j.contains( DISPARITY_SHIFT_OPTION_NAME ) )
        {
            validate_disparity_shift_option( option_j );
        }
        else if( option_j.contains( THRESHOLD_OPTION_NAME ) )
        {
            validate_threshold_option( option_j );
        }
        else if( ! option_j.contains( ENABLE_OPTION_NAME ) && ! option_j.contains( DOWNSCALE_RATIO_OPTION_NAME ) )
        {
            throw std::runtime_error( "Option json must contain a key matching one of the options name" );
        }
        // Validation passed - parameter is valid
    }

    void rs_dds_embedded_close_range_filter::validate_disparity_shift_option( rsutils::json const & opt_j ) const
    {
        auto dds_shift = find_dds_option_by_name(_dds_ef->get_options(), DISPARITY_SHIFT_OPTION_NAME);
        int32_t shift_val = opt_j[DISPARITY_SHIFT_OPTION_NAME].get<int32_t>();

        if (!dds_shift->get_minimum_value().is_null() && shift_val < dds_shift->get_minimum_value().get<int32_t>())
        {
            throw std::invalid_argument("Disparity Shift value " + std::to_string(shift_val) +
                " is below minimum " + std::to_string(dds_shift->get_minimum_value().get<int32_t>()));
        }
        if (!dds_shift->get_maximum_value().is_null() && shift_val > dds_shift->get_maximum_value().get<int32_t>())
        {
            throw std::invalid_argument("Disparity Shift value " + std::to_string(shift_val) +
                " is above maximum " + std::to_string(dds_shift->get_maximum_value().get<int32_t>()));
        }
    }

    void rs_dds_embedded_close_range_filter::validate_threshold_option( rsutils::json const & opt_j ) const
    {
        auto dds_thresh = find_dds_option_by_name(_dds_ef->get_options(), THRESHOLD_OPTION_NAME);
        int32_t thresh_val = opt_j[THRESHOLD_OPTION_NAME].get<int32_t>();

        if (!dds_thresh->get_minimum_value().is_null() && thresh_val < dds_thresh->get_minimum_value().get<int32_t>())
        {
            throw std::invalid_argument("Threshold value " + std::to_string(thresh_val) +
                " is below minimum " + std::to_string(dds_thresh->get_minimum_value().get<int32_t>()));
        }
        if (!dds_thresh->get_maximum_value().is_null() && thresh_val > dds_thresh->get_maximum_value().get<int32_t>())
        {
            throw std::invalid_argument("Threshold value " + std::to_string(thresh_val) +
                " is above maximum " + std::to_string(dds_thresh->get_maximum_value().get<int32_t>()));
        }
    }

}  // namespace librealsense
