// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2015 RealSense, Inc. All Rights Reserved.

#include "core/enum-helpers.h"
#include "core/notification.h"
#include "librealsense-exception.h"
#include <librealsense2/hpp/rs_processing.hpp>
#include <cmath>
#include <limits>
#include <ostream>


std::ostream & operator<<( std::ostream & out, rs2_extrinsics const & e )
{
    return out << "[ r[" << e.rotation[0] << "," << e.rotation[1] << "," << e.rotation[2] << "," << e.rotation[3] << ","
        << e.rotation[4] << "," << e.rotation[5] << "," << e.rotation[6] << "," << e.rotation[7] << ","
        << e.rotation[8] << "]  t[" << e.translation[0] << "," << e.translation[1] << "," << e.translation[2]
        << "] ]";
}


std::ostream & operator<<( std::ostream & out, rs2_intrinsics const & i )
{
    return out << "[ " << i.width << "x" << i.height << "  p[" << i.ppx << " " << i.ppy << "]"
        << "  f[" << i.fx << " " << i.fy << "]"
        << "  " << librealsense::get_string( i.model ) << " [" << i.coeffs[0] << " " << i.coeffs[1] << " "
        << i.coeffs[2] << " " << i.coeffs[3] << " " << i.coeffs[4] << "] ]";
}


LRS_EXTENSION_API bool operator==( const rs2_extrinsics & a, const rs2_extrinsics & b )
{
    for( int i = 0; i < 3; i++ )
        if( a.translation[i] != b.translation[i] )
            return false;
    for( int j = 0; j < 3; j++ )
        for( int i = 0; i < 3; i++ )
            if( std::fabs( a.rotation[j * 3 + i] - b.rotation[j * 3 + i] ) > std::numeric_limits< float >::epsilon() )
                return false;
    return true;
}


namespace librealsense
{
    // The extrinsics on the camera ("raw extrinsics") are in milimeters, but LRS works in meters
    // Additionally, LRS internal algorithms are
    // written with a transposed matrix in mind! (see rs2_transform_point_to_point)
    rs2_extrinsics to_raw_extrinsics(rs2_extrinsics extr)
    {
        float const meters_to_milimeters = 1000;
        extr.translation[0] *= meters_to_milimeters;
        extr.translation[1] *= meters_to_milimeters;
        extr.translation[2] *= meters_to_milimeters;

        std::swap(extr.rotation[1], extr.rotation[3]);
        std::swap(extr.rotation[2], extr.rotation[6]);
        std::swap(extr.rotation[5], extr.rotation[7]);
        return extr;
    }

    rs2_extrinsics from_raw_extrinsics(rs2_extrinsics extr)
    {
        float const milimeters_to_meters = 0.001f;
        extr.translation[0] *= milimeters_to_meters;
        extr.translation[1] *= milimeters_to_meters;
        extr.translation[2] *= milimeters_to_meters;

        std::swap(extr.rotation[1], extr.rotation[3]);
        std::swap(extr.rotation[2], extr.rotation[6]);
        std::swap(extr.rotation[5], extr.rotation[7]);
        return extr;
    }

    recoverable_exception::recoverable_exception(const std::string& msg,
        rs2_exception_type exception_type) noexcept
        : librealsense_exception(msg, exception_type)
    {
        // This is too intrusive: some throws are completely valid and even expected (options-watcher queries all
        // options, some of which may not be queryable) and this just dirties up the logs. It doesn't have the actual
        // exception type, either.
        //LOG_DEBUG("recoverable_exception: " << msg);
    }

}
