// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.
#pragma once

#include <src/composite-option-interface.h>
#include <rsutils/json-fwd.h>

#include <functional>
#include <memory>
#include <vector>


namespace realdds {
class dds_embedded_filter;
class dds_option;
}  // namespace realdds


namespace librealsense {

    // DDS transport for the Improved Close Range Control (rs2_hdrd_control) composite option: maps the struct to the
    // FW's per-field DDS options of a realdds::dds_close_range_filter. Validation of the values against the streaming
    // profiles is left to hdrd_filter_feature, same as over USB.
    class rs_dds_close_range_control : public composite_option_interface
    {
    public:
        typedef std::function< void( rsutils::json value ) > set_embedded_filter_callback;

        rs_dds_close_range_control( std::shared_ptr< realdds::dds_embedded_filter > const & dds_embedded_filter,
                                    set_embedded_filter_callback set_embedded_filter_cb );

        std::vector< uint8_t > get_raw() const override;
        // Sends all fields in one set-filter request
        void set_raw( const void * data, size_t size ) override;
        std::vector< uint8_t > get_raw_range() const override;
        bool is_read_only() const override { return false; }
        bool is_enabled() const override { return true; }
        const char * get_description() const override;

    private:
        template< class Get >
        std::vector< uint8_t > pack( Get && get ) const;

        std::shared_ptr< realdds::dds_embedded_filter > _dds_ef;
        set_embedded_filter_callback _set_ef_cb;
        // DDS option backing each rs2_hdrd_control field, in wire order; null if the FW does not advertise it
        std::vector< std::shared_ptr< realdds::dds_option > > _fields;
    };

}  // namespace librealsense
