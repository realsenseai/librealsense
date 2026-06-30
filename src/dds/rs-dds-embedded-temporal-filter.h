// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2025 RealSense, Inc. All Rights Reserved.
#pragma once

#include <rsutils/json-fwd.h>
#include <realdds/dds-embedded-filter.h>
#include <dds/rs-dds-embedded-filter.h>
#include <src/proc/temporal-embedded-filter.h>


namespace librealsense {

    // Class librealsense::rs_dds_embedded_temporal_filter:
    // A facade for a realdds::dds_embedded_temporal_filter exposing librealsense interface
    // handles librealsense embedded temporal filter specific logic and parameter validation
    // Communication to HW is delegated to realdds::dds_temporal_filter
    class rs_dds_embedded_temporal_filter
        : public rs_dds_embedded_filter
        , public temporal_embedded_filter
    {
    public:
        rs_dds_embedded_temporal_filter(const std::shared_ptr< realdds::dds_embedded_filter >& dds_embedded_filter,
            set_embedded_filter_callback set_embedded_filter_cb,
            query_embedded_filter_callback query_embedded_filter_cb);
        virtual ~rs_dds_embedded_temporal_filter() = default;

        // Override interface methods
        inline rs2_embedded_filter_type get_type() const override { return RS2_EMBEDDED_FILTER_TYPE_TEMPORAL; }

        // Override abstract class methods
        virtual void add_option(std::shared_ptr< realdds::dds_option > option) override;

    private:
        void validate_filter_option( rsutils::json const & option_j ) const;
        void validate_alpha_option( rsutils::json const & opt_j ) const;
        void validate_delta_option( rsutils::json const & opt_j ) const;
        void validate_persistency_option( rsutils::json const & opt_j ) const;

        // static constexpr avoids per-instance std::string allocation/copy.
        static constexpr const char * TOGGLE_OPTION_NAME      = "Toggle";
        static constexpr const char * ALPHA_OPTION_NAME       = "Alpha";
        static constexpr const char * DELTA_OPTION_NAME       = "Delta";
        static constexpr const char * PERSISTENCY_OPTION_NAME = "Persistency";
        static constexpr int32_t      PERSISTENCY_MAX_LEN     = 30;
    };

}  // namespace librealsense
