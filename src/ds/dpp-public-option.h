// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

// Boundary between the public settings structs of the HKR DPP composite options (rs2_decimation_filter_dpp_config,
// rs2_temporal_filter_dpp_config, ...) and the FW wire payload: dpp_header + 8 int32 param slots (38 bytes).
// A public struct holds only the active params, as contiguous int32 fields in slot order. The header, the unused
// slots and the packing never reach the user, so FW payload changes do not change the public API.

#pragma once

#include <src/composite-option-interface.h>
#include <src/ds/composite-xu-option.h>
#include <src/librealsense-exception.h>
#include <rsutils/string/from.h>
#include <rsutils/easylogging/easyloggingpp.h>
#include <src/ds/dpp-header.h>

#include <cstdint>
#include <cstring>
#include <memory>
#include <type_traits>
#include <vector>


namespace librealsense {

#pragma pack( push, 1 )
struct dpp_wire
{
    static constexpr size_t slot_count = 8;

    dpp_header header;
    int32_t params[slot_count];  // active ones first, the rest MUST be zero on SET
};
#pragma pack( pop )

static_assert( sizeof( dpp_wire ) == 38, "DPP wire payload must be 38 bytes" );


// Wraps the transport (XU or DDS) carrying the dpp_wire payload and exposes the public struct `Public` instead.
template< class Public >
class dpp_public_option : public composite_option_interface
{
    static_assert( std::is_standard_layout< Public >::value && sizeof( Public ) % sizeof( int32_t ) == 0
                       && sizeof( Public ) / sizeof( int32_t ) <= dpp_wire::slot_count,
                   "A DPP public struct is made of contiguous int32 settings only" );
    static constexpr uint8_t param_count = sizeof( Public ) / sizeof( int32_t );

public:
    dpp_public_option( std::shared_ptr< composite_option_interface > control, const dpp_control_desc & desc )
        : _control( std::move( control ) )
        , _desc( desc )
    {
        if( _desc.param_count != param_count )
            throw invalid_value_exception( "DPP control description does not match its public struct" );
    }

    std::vector< uint8_t > get_raw() const override { return to_public( _control->get_raw() ); }

    void set_raw( const void * data, size_t size ) override
    {
        if( ! data || size != sizeof( Public ) )
            throw invalid_value_exception( rsutils::string::from() << _desc_name() << " expects " << sizeof( Public )
                                                                   << " bytes, got " << size );
        // Carry back the header FW reported, so FW-owned flags are kept
        dpp_header header = default_header();
        try
        {
            auto raw = _control->get_raw();
            check_envelope_or_throw( raw );
            std::memcpy( &header, raw.data(), sizeof( header ) );
        }
        catch( const std::exception & e )
        {
            LOG_DEBUG( "DPP header unavailable, sending the default one: " << e.what() );
        }
        dpp_wire wire;
        std::memset( &wire, 0, sizeof( wire ) );
        wire.header = header;
        std::memcpy( wire.params, data, sizeof( Public ) );
        _control->set_raw( &wire, sizeof( wire ) );
    }

    // {min, max, step, def}, each a complete public struct
    std::vector< uint8_t > get_raw_range() const override
    {
        auto raw = _control->get_raw_range();
        if( raw.size() != 4 * sizeof( dpp_wire ) )
            throw invalid_value_exception( "DPP control returned an unexpected range payload size" );
        std::vector< uint8_t > range;
        for( size_t i = 0; i < 4; ++i )
        {
            auto bound = to_public( std::vector< uint8_t >( raw.begin() + i * sizeof( dpp_wire ),
                                                             raw.begin() + ( i + 1 ) * sizeof( dpp_wire ) ) );
            range.insert( range.end(), bound.begin(), bound.end() );
        }
        return range;
    }

    bool is_read_only() const override { return _control->is_read_only(); }
    bool is_enabled() const override { return _control->is_enabled(); }
    const char * get_description() const override { return _control->get_description(); }

private:
    const char * _desc_name() const { return _control->get_description(); }

    dpp_header default_header() const
    {
        dpp_header h;
        std::memset( &h, 0, sizeof( h ) );
        h.version = DPP_HEADER_CURRENT_VERSION;
        h.ctl_id = _desc.ctl_id;
        h.param_count = param_count;
        h.param_type = _desc.param_type;
        return h;
    }

    // The payload size, header version, control ID and param count must be this control's schema
    void check_envelope_or_throw( const std::vector< uint8_t > & raw ) const
    {
        if( raw.size() != sizeof( dpp_wire ) )
            throw invalid_value_exception( rsutils::string::from() << _desc_name() << " returned an unexpected payload size "
                                                                   << raw.size() );
        dpp_header hdr;
        std::memcpy( &hdr, raw.data(), sizeof( hdr ) );
        if( hdr.version != DPP_HEADER_CURRENT_VERSION || hdr.ctl_id != _desc.ctl_id || hdr.param_count != param_count )
            throw invalid_value_exception( rsutils::string::from()
                                           << _desc_name() << " returned an unsupported schema (version=" << (int)hdr.version
                                           << " ctl_id=" << (int)hdr.ctl_id << " param_count=" << (int)hdr.param_count << ")" );
    }

    std::vector< uint8_t > to_public( const std::vector< uint8_t > & raw ) const
    {
        check_envelope_or_throw( raw );
        std::vector< uint8_t > out( sizeof( Public ) );
        std::memcpy( out.data(), raw.data() + offsetof( dpp_wire, params ), sizeof( Public ) );
        return out;
    }

    std::shared_ptr< composite_option_interface > _control;
    dpp_control_desc _desc;
};

}  // namespace librealsense
