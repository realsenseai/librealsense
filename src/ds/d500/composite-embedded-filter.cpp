// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "composite-embedded-filter.h"
#include "hdrd-embedded-filter.h"
#include "ds/ds-private.h"
#include <src/proc/temporal-embedded-filter.h>
#include <src/proc/decimation-embedded-filter.h>
#include <src/ds/composite-xu-option.h>

namespace librealsense {

template< class Base, rs2_embedded_filter_type Type >
composite_embedded_filter< Base, Type >::composite_embedded_filter(
    std::weak_ptr< uvc_sensor > raw_depth_ep,
    uint8_t ctrl_id,
    uint32_t wire_size,
    rs2_composite_option_id option_id,
    std::string description )
{
    auto opt = std::make_shared< composite_xu_option >( raw_depth_ep, ds::depth_xu, ctrl_id, wire_size, std::move( description ) );
    this->register_composite_option( option_id, opt );
}

// The only three composite-option embedded filters that exist today. Adding another one means
// adding its own explicit instantiation line here, never a hand-written subclass body.
template class composite_embedded_filter< temporal_embedded_filter, RS2_EMBEDDED_FILTER_TYPE_TEMPORAL >;
template class composite_embedded_filter< close_range_embedded_filter, RS2_EMBEDDED_FILTER_TYPE_CLOSE_RANGE >;
template class composite_embedded_filter< decimation_embedded_filter, RS2_EMBEDDED_FILTER_TYPE_DECIMATION >;

}  // namespace librealsense
