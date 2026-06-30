// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2025 RealSense, Inc. All Rights Reserved.
#pragma once

#include <rsutils/json-fwd.h>
#include <realdds/dds-embedded-filter.h>
#include <dds/rs-dds-embedded-filter.h>
#include <src/proc/decimation-embedded-filter.h>


namespace librealsense {

    // Class librealsense::rs_dds_embedded_decimation_filter:
    // A facade for a realdds::dds_embedded_decimation_filter exposing librealsense interface
    // handles librealsense embedded decimation filter specific logic and parameter validation
    // Communication to HW is delegated to realdds::dds_decimation_filter
    class rs_dds_embedded_decimation_filter
        : public rs_dds_embedded_filter
        , public decimation_embedded_filter
    {
    public:
        rs_dds_embedded_decimation_filter(const std::shared_ptr< realdds::dds_embedded_filter >& dds_embedded_filter,
            set_embedded_filter_callback set_embedded_filter_cb,
            query_embedded_filter_callback query_embedded_filter_cb);
        virtual ~rs_dds_embedded_decimation_filter() = default;

        // Override interface methods
        inline rs2_embedded_filter_type get_type() const override { return RS2_EMBEDDED_FILTER_TYPE_DECIMATION; }

        // Override abstract class methods
        virtual void add_option(std::shared_ptr< realdds::dds_option > option) override;

    private:
        void validate_filter_option( rsutils::json const & option_j ) const;
        void validate_magnitude_option( rsutils::json const & opt_j ) const;

        // static constexpr avoids per-instance std::string allocation/copy.
        static constexpr const char * TOGGLE_OPTION_NAME    = "Toggle";
        static constexpr const char * MAGNITUDE_OPTION_NAME = "Magnitude";

        static constexpr int32_t DECIMATION_MAGNITUDE = 2; // Decimation magnitude must always be 2
    };

}  // namespace librealsense
