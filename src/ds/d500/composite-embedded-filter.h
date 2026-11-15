// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

#include <librealsense2/h/rs_composite_option.h>

#include <memory>


namespace librealsense {

class composite_option_interface;

// Generic HKR/D5X5 composite-option embedded filter: registers ONE composite XU option under
// `option_id`, in this filter's OWN options container (via `Base`), NOT directly on
// d500_depth_sensor. The option itself is built by composite_xu_option::create() at the call
// site (features own the per-control desc); this template just hosts it under an option_id
// and gives it a filter-type identity.
template< class Base, rs2_embedded_filter_type Type >
class composite_embedded_filter : public Base
{
public:
    composite_embedded_filter( std::shared_ptr< composite_option_interface > option,
                                rs2_composite_option_id option_id );

    rs2_embedded_filter_type get_type() const override { return Type; }
};

}  // namespace librealsense
