// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "d500-close-range-embedded-filter.h"
#include "ds/ds-private.h"
#include <src/librealsense-exception.h>

#include <cstdint>

namespace librealsense {

namespace {

// Depth XU control coordinates for the close-range filter.
constexpr uint8_t  XU_SELECTOR             = 0x14;
constexpr uint16_t CTL_ID                  = 0x0008;
constexpr uint8_t  DPPC_VERSION            = 0x01;
constexpr uint8_t  DPPC_FLAGS              = 0x01;
constexpr uint8_t  DPPC_PARAM_COUNT        = 4;
constexpr uint8_t  DPPC_PARAM_TYPE_INTEGER = 0x00;

// 38-byte wire payload, packed per the depth XU control format.
#pragma pack(push, 1)
struct dppc_ctl
{
    uint8_t  version;
    uint8_t  flags;
    uint16_t ctl_id;
    uint8_t  param_count;
    uint8_t  param_type;
    int32_t  params[8];  // [enable, downscale_ratio, disparity_shift, threshold, reserved...]
};
#pragma pack(pop)
static_assert( sizeof( dppc_ctl ) == 38, "dppc_ctl must be exactly 38 bytes" );

void stamp_header( dppc_ctl & p )
{
    p.version     = DPPC_VERSION;
    p.flags       = DPPC_FLAGS;
    p.ctl_id      = CTL_ID;
    p.param_count = DPPC_PARAM_COUNT;
    p.param_type  = DPPC_PARAM_TYPE_INTEGER;
}

}  // anonymous

close_range_xu_option::close_range_xu_option( std::weak_ptr< uvc_sensor > ep )
    : _ep( ep )
{
}

void close_range_xu_option::set( float value )
{
    if( value != 0.f && value != 1.f )
        throw invalid_value_exception( rsutils::string::from() << "Close Range Enable must be 0 or 1; got " << value );

    auto ep = _ep.lock();
    if( ! ep )
        throw invalid_value_exception( "Close Range: depth sensor not alive for set" );

    ep->invoke_powered(
        [this, value]( platform::uvc_device & dev )
        {
            // Read current payload so we preserve the device's current
            // downscale_ratio / disparity_shift / threshold values.
            dppc_ctl payload = {};
            if( ! dev.get_xu( ds::depth_xu,
                              XU_SELECTOR,
                              reinterpret_cast< uint8_t * >( &payload ),
                              sizeof( payload ) ) )
            {
                throw invalid_value_exception( "Close Range get_xu failed before set" );
            }

            stamp_header( payload );
            payload.params[0] = ( value != 0.f ) ? 1 : 0;

            if( ! dev.set_xu( ds::depth_xu,
                              XU_SELECTOR,
                              reinterpret_cast< uint8_t * >( &payload ),
                              sizeof( payload ) ) )
            {
                throw invalid_value_exception( "Close Range set_xu failed" );
            }

            _record( *this );
        } );
}

float close_range_xu_option::query() const
{
    auto ep = _ep.lock();
    if( ! ep )
        throw invalid_value_exception( "Close Range: depth sensor not alive for query" );

    return ep->invoke_powered(
        [this]( platform::uvc_device & dev ) -> float
        {
            dppc_ctl payload = {};
            if( ! dev.get_xu( ds::depth_xu,
                              XU_SELECTOR,
                              reinterpret_cast< uint8_t * >( &payload ),
                              sizeof( payload ) ) )
            {
                throw invalid_value_exception( "Close Range get_xu failed" );
            }
            return static_cast< float >( payload.params[0] );
        } );
}


d500_close_range_embedded_filter::d500_close_range_embedded_filter( std::weak_ptr< uvc_sensor > raw_depth_ep )
{
    auto opt = std::make_shared< close_range_xu_option >( raw_depth_ep );
    register_option( RS2_OPTION_EMBEDDED_FILTER_ENABLED, opt );
    _options_watcher.register_option( RS2_OPTION_EMBEDDED_FILTER_ENABLED, opt );
}

}  // namespace librealsense
