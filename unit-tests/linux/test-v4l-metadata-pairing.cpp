// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

//#test:donotrun:!linux

#include <unit-tests/catch.h>

#ifdef RS2_USE_V4L2_BACKEND
#include <src/linux/v4l-enumerator.h>
#include <linux/videodev2.h>

using namespace librealsense::platform;
using namespace librealsense::platform::v4l_enum;

namespace
{
node_info node( int number, const char * device, uint16_t mi, bool metadata, bool mipi = false )
{
    uvc_device_info info;
    info.id = "/dev/video" + std::to_string( number );
    info.unique_id = device;
    info.mi = mi;
    info.is_mipi = mipi;
    info.uvc_capabilities = metadata ? V4L2_CAP_META_CAPTURE : V4L2_CAP_VIDEO_CAPTURE;
    return { info, info.id };
}

void check_pairings( const std::vector< node_info > & input,
                     const std::vector< std::pair< std::string, std::string > > & expected )
{
    auto output = match_video_with_metadata_nodes( input );
    REQUIRE( output.size() == expected.size() );
    for( size_t i = 0; i < output.size(); ++i )
    {
        INFO( "video node: " << output[i].first.id );
        CHECK( output[i].first.id == expected[i].first );
        CHECK( output[i].second == expected[i].first );
        CHECK( output[i].first.metadata_node_id == expected[i].second );
        CHECK( output[i].first.has_metadata_node == ! expected[i].second.empty() );
    }
}
}

TEST_CASE( "V4L2 metadata stays with its USB device and interface", "[v4l2]" )
{
    SECTION( "adjacent nodes" )
    {
        check_pairings( { node( 0, "A", 0, false ), node( 1, "A", 0, true ) },
                        { { "/dev/video0", "/dev/video1" } } );
    }
    SECTION( "interleaved cameras after hotplug" )
    {
        check_pairings( { node( 16, "A", 0, false ), node( 23, "B", 0, false ),
                          node( 24, "A", 0, true ), node( 25, "B", 0, true ) },
                        { { "/dev/video16", "/dev/video24" }, { "/dev/video23", "/dev/video25" } } );
    }
    SECTION( "interleaved interfaces on one camera" )
    {
        check_pairings( { node( 0, "A", 0, false ), node( 1, "A", 3, false ),
                          node( 2, "A", 0, true ), node( 3, "A", 3, true ) },
                        { { "/dev/video0", "/dev/video2" }, { "/dev/video1", "/dev/video3" } } );
    }
    SECTION( "orphan metadata from another camera" )
    {
        check_pairings( { node( 0, "A", 0, false ), node( 1, "B", 0, true ) },
                        { { "/dev/video0", "" } } );
    }
    SECTION( "orphan metadata from another interface" )
    {
        check_pairings( { node( 0, "A", 0, false ), node( 1, "A", 3, true ) },
                        { { "/dev/video0", "" } } );
    }
    SECTION( "no video node" )
    {
        check_pairings( {}, {} );
        check_pairings( { node( 0, "A", 0, true ) }, {} );
        check_pairings( { node( 0, "MIPI", 3, true, true ) }, {} );
    }
    SECTION( "duplicate metadata does not overwrite the first pairing" )
    {
        check_pairings( { node( 0, "A", 0, false ), node( 1, "A", 0, true ), node( 2, "A", 0, true ) },
                        { { "/dev/video0", "/dev/video1" } } );
    }
    SECTION( "multiple streams on one USB interface" )
    {
        check_pairings( { node( 0, "A", 0, false ), node( 1, "A", 0, true ),
                          node( 2, "A", 0, false ), node( 3, "A", 0, true ) },
                        { { "/dev/video0", "/dev/video1" }, { "/dev/video2", "/dev/video3" } } );
    }
    SECTION( "MIPI retains adjacent pairing despite different mi values" )
    {
        check_pairings( { node( 0, "MIPI", 0, false, true ), node( 1, "MIPI", 3, true, true ) },
                        { { "/dev/video0", "/dev/video1" } } );
    }
}
#endif
