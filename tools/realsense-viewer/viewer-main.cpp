// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "realsense-viewer.h"

#include <librealsense2/rs.hpp>
#include <iostream>

int main( int argc, const char ** argv ) try
{
    return run_viewer( argc, argv );
}
catch( const rs2::error & e )
{
    std::cerr << "RealSense error calling " << e.get_failed_function()
              << "(" << e.get_failed_args() << "):\n    " << e.what() << std::endl;
    return EXIT_FAILURE;
}
catch( const std::exception & e )
{
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
}
