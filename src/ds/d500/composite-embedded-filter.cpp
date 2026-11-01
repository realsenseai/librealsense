// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "composite-embedded-filter.h"
#include "hdrd-embedded-filter.h"
#include <src/proc/temporal-embedded-filter.h>
#include <src/proc/decimation-embedded-filter.h>
#include <src/ds/composite-xu-option.h>

namespace librealsense {

template< class Base, rs2_embedded_filter_type Type >
composite_embedded_filter< Base, Type >::composite_embedded_filter(
    std::shared_ptr< composite_option_interface > option,
    rs2_composite_option_id option_id )
{
    this->register_composite_option( option_id, std::move( option ) );
}

// The only three composite-option embedded filters that exist today. Adding another one means
// adding its own explicit instantiation line here, never a hand-written subclass body.
template class composite_embedded_filter< temporal_embedded_filter, RS2_EMBEDDED_FILTER_TYPE_TEMPORAL >;
template class composite_embedded_filter< close_range_embedded_filter, RS2_EMBEDDED_FILTER_TYPE_CLOSE_RANGE >;
template class composite_embedded_filter< decimation_embedded_filter, RS2_EMBEDDED_FILTER_TYPE_DECIMATION >;

}  // namespace librealsense
