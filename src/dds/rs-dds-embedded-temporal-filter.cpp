// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2025 RealSense, Inc. All Rights Reserved.

#include "rs-dds-embedded-temporal-filter.h"
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

    rs_dds_embedded_temporal_filter::rs_dds_embedded_temporal_filter(const std::shared_ptr< realdds::dds_embedded_filter >& dds_embedded_filter,
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

    void rs_dds_embedded_temporal_filter::add_option(std::shared_ptr< realdds::dds_option > option)
    {
        rs2_option option_id;
        
        // Map DDS option names to standard RealSense option IDs
        if (option->get_name() == TOGGLE_OPTION_NAME)
        {
            option_id = RS2_OPTION_EMBEDDED_FILTER_ENABLED;
        }
        else if (option->get_name() == ALPHA_OPTION_NAME)
        {
            option_id = RS2_OPTION_FILTER_SMOOTH_ALPHA;
        }
        else if (option->get_name() == DELTA_OPTION_NAME)
        {
            option_id = RS2_OPTION_FILTER_SMOOTH_DELTA;
        }
        else if (option->get_name() == PERSISTENCY_OPTION_NAME)
        {
            option_id = RS2_OPTION_HOLES_FILL;
        }
        else
        {
            throw std::runtime_error("option '" + option->get_name() + "' not in this filter");
        }

        if (!is_valid(option_id))
        {
            LOG_ERROR("Option '" << option->get_name() << "' not found");
            throw librealsense::invalid_value_exception("Option '" + option->get_name() + "' not found");
        }

        if (get_option_handler(option_id))
            throw std::runtime_error("option '" + option->get_name() + "' already exists in sensor");

        // In below implementation:
        // - setting one option leads to
        //   * setting the new value for one option in the device,
        //   * setting the value also in the DDS filter
        // - getting one option leads to:
        //   * returning only the relevant option's value
        //   * the getting of the filter's options communicating with the device by DDS
        //     is not necessary, since the value is already automatically updated by the set action reply
        auto opt = std::make_shared< rs_dds_option >(
            option,
            [=]( json value )  // set_option cb for the filter's options
            {
                // create a proper option json with name and value
                json option_with_value = dds_option_to_name_and_value_json( option, value );
                // validate values
                validate_filter_option( option_with_value );
                // set updated options to the remote device
                _set_ef_cb( option_with_value );
                // Delegate to DDS filter
                _dds_ef->set_options( option_with_value );
            },
            [=]() -> json  // get_option cb for the filter's options
            { return option->get_value(); } );
        register_option( option_id, opt );
        _options_watcher.register_option( option_id, opt );
        
    }

    void rs_dds_embedded_temporal_filter::validate_filter_option(rsutils::json option_j) const
    {
        if (option_j.contains(TOGGLE_OPTION_NAME))
        {
            validate_toggle_option(option_j);
        }
        else if (option_j.contains(ALPHA_OPTION_NAME))
        {
            validate_alpha_option(option_j);
        }
        else if (option_j.contains(DELTA_OPTION_NAME))
        {
            validate_delta_option(option_j);
        }
        else if (option_j.contains(PERSISTENCY_OPTION_NAME))
        {
            validate_persistency_option(option_j);
        }
        else
        {
            throw std::runtime_error("Option json must contain a key matching one of the options name");
        }
        // Validation passed - parameter is valid
    }

    void rs_dds_embedded_temporal_filter::validate_toggle_option(rsutils::json opt_j) const
    {
        auto dds_toggle = find_dds_option_by_name(_dds_ef->get_options(), TOGGLE_OPTION_NAME);
        int32_t toggle_val = opt_j[TOGGLE_OPTION_NAME].get<int32_t>();

        // Check range using DDS option
        if (!dds_toggle->get_minimum_value().is_null() && toggle_val < dds_toggle->get_minimum_value().get<int32_t>())
        {
            throw std::invalid_argument("Toggle value " + std::to_string(toggle_val) +
                " is below minimum " + std::to_string(dds_toggle->get_minimum_value().get<int32_t>()));
        }
        if (!dds_toggle->get_maximum_value().is_null() && toggle_val > dds_toggle->get_maximum_value().get<int32_t>())
        {
            throw std::invalid_argument("Toggle value " + std::to_string(toggle_val) +
                " is above maximum " + std::to_string(dds_toggle->get_maximum_value().get<int32_t>()));
        }

        // Additional validation: Toggle should be 0 or 1
        if (toggle_val != 0 && toggle_val != 1)
        {
            throw std::runtime_error("Toggle shall be 0 for OFF or 1 for ON");
        }
    }

    void rs_dds_embedded_temporal_filter::validate_alpha_option(rsutils::json opt_j) const
    {
        auto dds_alpha = find_dds_option_by_name(_dds_ef->get_options(), ALPHA_OPTION_NAME);
        float alpha_val = opt_j[ALPHA_OPTION_NAME].get<float>();
        // Check range using DDS option
        if (!dds_alpha->get_minimum_value().is_null() && alpha_val < dds_alpha->get_minimum_value().get<float>())
        {
            throw std::invalid_argument("Alpha value " + std::to_string(alpha_val) +
                " is below minimum " + std::to_string(dds_alpha->get_minimum_value().get<float>()));
        }
        if (!dds_alpha->get_maximum_value().is_null() && alpha_val > dds_alpha->get_maximum_value().get<float>())
        {
            throw std::invalid_argument("Alpha value " + std::to_string(alpha_val) +
                " is above maximum " + std::to_string(dds_alpha->get_maximum_value().get<float>()));
        }
    }

    void rs_dds_embedded_temporal_filter::validate_delta_option(rsutils::json opt_j) const
    {
        auto dds_delta = find_dds_option_by_name(_dds_ef->get_options(), DELTA_OPTION_NAME);
        int32_t delta_val = opt_j[DELTA_OPTION_NAME].get<int32_t>();
        // Check range using DDS option
        if (!dds_delta->get_minimum_value().is_null() && delta_val < dds_delta->get_minimum_value().get<int32_t>())
        {
            throw std::invalid_argument("Delta value " + std::to_string(delta_val) +
                " is below minimum " + std::to_string(dds_delta->get_minimum_value().get<int32_t>()));
        }
        if (!dds_delta->get_maximum_value().is_null() && delta_val > dds_delta->get_maximum_value().get<int32_t>())
        {
            throw std::invalid_argument("Delta value " + std::to_string(delta_val) +
                " is above maximum " + std::to_string(dds_delta->get_maximum_value().get<int32_t>()));
        }
    }

    void rs_dds_embedded_temporal_filter::validate_persistency_option(rsutils::json opt_j) const
    {
        auto dds_persistency = find_dds_option_by_name(_dds_ef->get_options(), PERSISTENCY_OPTION_NAME);
        std::string persistency_val = opt_j[PERSISTENCY_OPTION_NAME].get<std::string>();
        // Check range not relevant for string
        // Checking that len is no more than max len to avoid DDS errors
        if( persistency_val.size() > PERSISTENCY_MAX_LEN )
        {
            throw std::invalid_argument("Persistency value " + (persistency_val) +
                " is too long ");
        }
    }

}  // namespace librealsense
