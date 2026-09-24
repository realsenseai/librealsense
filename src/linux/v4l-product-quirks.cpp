// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "v4l-product-quirks.h"

namespace librealsense
{
    bool is_d5xx_product_line( uint16_t pid )
    {
        return ( pid == 0x0B56 )                      // D555
            || ( pid == 0x0B6A ) || ( pid == 0x0B6B ) // D585 legacy / D585S
            || ( pid >= 0x0C01 && pid <= 0x0C08 );    // D535 / D585 2C+3C
    }

    bool is_d5xx_mapping_interface( uint16_t pid, uint16_t mi )
    {
        if( ! is_d5xx_product_line( pid ) )
            return false;
        return ( pid == 0x0B6B || pid == 0x0B6A ) ? ( mi == 13 ) : ( mi == 11 );
    }
}  // namespace librealsense
