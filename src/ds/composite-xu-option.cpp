// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "composite-xu-option.h"
#include <src/librealsense-exception.h>
#include <rsutils/string/from.h>
#include <librealsense2/h/rs_dpp_header.h>

#include <cstring>

namespace librealsense {

composite_xu_option::composite_xu_option( bool is_mipi,
                                          std::weak_ptr< uvc_sensor > ep,
                                          platform::extension_unit xu,
                                          uint8_t ctrl_id,
                                          uint32_t wire_size,
                                          std::string description,
                                          const dpp_control_desc & desc )
    : _ep( ep )
    , _xu( xu )
    , _ctrl_id( ctrl_id )
    , _wire_size( wire_size )
    , _description( description )
    , _is_mipi( is_mipi )
    , _desc( desc )
{
    // Validate the desc fits in the wire once, so the memcpy sites in synthesize_range can trust it.
    const size_t required = sizeof( dpp_header ) + _desc.param_count * sizeof( int32_t );
    if( _is_mipi && required > wire_size )
        throw invalid_value_exception( rsutils::string::from()
                                        << "composite_xu_option: dpp_control_desc (ctl_id=" << (int)_desc.ctl_id
                                        << ") needs " << required << " bytes but wire_size is only " << wire_size );
}

/* static */ std::shared_ptr< composite_xu_option >
composite_xu_option::create( bool is_mipi,
                             std::weak_ptr< uvc_sensor > ep,
                             platform::extension_unit xu,
                             uint8_t ctrl_id,
                             uint32_t wire_size,
                             std::string description,
                             const dpp_control_desc & desc )
{
    return std::make_shared< composite_xu_option >( is_mipi, std::move( ep ), xu, ctrl_id, wire_size,
                                                    std::move( description ), desc );
}

std::vector< uint8_t > composite_xu_option::get_raw() const
{
    auto ep = _ep.lock();
    if( ! ep )
        throw wrong_api_call_sequence_exception( "composite option is not available: sensor is not alive" );

    return ep->invoke_powered(
        [this]( platform::uvc_device & dev ) -> std::vector< uint8_t >
        {
            std::vector< uint8_t > data( _wire_size );
            if( ! dev.get_xu( _xu, _ctrl_id, data.data(), (int)_wire_size ) )
                throw invalid_value_exception( rsutils::string::from() << "get_xu(id=" << (int)_ctrl_id << ") failed!" );
            return data;
        } );
}

void composite_xu_option::set_raw( const void * data, size_t size )
{
    if( ! data )
        throw invalid_value_exception( "composite_xu_option::set_raw: data is null" );

    if( size != _wire_size )
        throw invalid_value_exception( rsutils::string::from()
                                        << "composite_xu_option::set_raw: data size " << size
                                        << " does not match this control's wire size " << _wire_size );

    auto ep = _ep.lock();
    if( ! ep )
        throw wrong_api_call_sequence_exception( "composite option is not available: sensor is not alive" );

    ep->invoke_powered(
        [this, data, size]( platform::uvc_device & dev )
        {
            if( ! dev.set_xu( _xu, _ctrl_id, reinterpret_cast< const uint8_t * >( data ), (int)size ) )
                throw invalid_value_exception( rsutils::string::from() << "set_xu(id=" << (int)_ctrl_id << ") failed!" );
        } );
}

std::vector< uint8_t > composite_xu_option::get_raw_range() const
{
    auto ep = _ep.lock();
    if( ! ep )
        throw wrong_api_call_sequence_exception( "composite option is not available: sensor is not alive" );

    auto uvc_range = _is_mipi ? synthesize_range()
                              : ep->invoke_powered( [this]( platform::uvc_device & dev )
                                                    { return dev.get_xu_range( _xu, _ctrl_id, (int)_wire_size ); } );

    if( uvc_range.min.size() != _wire_size || uvc_range.max.size() != _wire_size || uvc_range.step.size() != _wire_size
        || uvc_range.def.size() != _wire_size )
        throw invalid_value_exception( "composite_xu_option::get_raw_range: device returned an unexpected range payload size" );

    // Generic {min, max, step, def} packing - see composite_option_interface::get_raw_range()
    // for the convention this follows.
    std::vector< uint8_t > result;
    result.reserve( 4 * _wire_size );
    result.insert( result.end(), uvc_range.min.begin(), uvc_range.min.end() );
    result.insert( result.end(), uvc_range.max.begin(), uvc_range.max.end() );
    result.insert( result.end(), uvc_range.step.begin(), uvc_range.step.end() );
    result.insert( result.end(), uvc_range.def.begin(), uvc_range.def.end() );
    return result;
}

// GMSL cannot query ranges: V4L2's VIDIOC_QUERY_EXT_CTRL reports one aggregate scalar bound for an
// array control, and the FW register map exposes only the current value of each control. So the
// SDK keeps static per-slot tables in `_desc` and builds each bound as a full struct (header +
// active params + reserved zero-fill), the same layout USB gets from FW.
platform::control_range composite_xu_option::synthesize_range() const
{
    auto build = [this]( const int32_t * slots )
    {
        std::vector< uint8_t > v( _wire_size, 0 );
        dpp_header hdr{};
        hdr.version = DPP_HEADER_CURRENT_VERSION;
        hdr.flags = 0;
        hdr.ctl_id = _desc.ctl_id;
        hdr.param_count = _desc.param_count;
        hdr.param_type = _desc.param_type;
        std::memcpy( v.data(), &hdr, sizeof( dpp_header ) );
        std::memcpy( v.data() + sizeof( dpp_header ), slots, _desc.param_count * sizeof( int32_t ) );
        return v;
    };
    return platform::control_range( build( _desc.min ), build( _desc.max ), build( _desc.step ), build( _desc.def ) );
}

}  // namespace librealsense
