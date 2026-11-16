// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "viewer-test-helpers.h"
#include <rsutils/os/special-folder.h>
#include <rsutils/string/from.h>
#include <fstream>


// Stream color RAW16, take a snapshot and verify it saves a grayscale PNG plus the raw data and metadata
VIEWER_TEST( "streaming", "snapshot_color_raw16" )
{
    auto & model = test.find_first_device_or_exit();

    std::shared_ptr< rs2::subdevice_model > sub;
    rs2::video_stream_profile raw;
    for( auto && s : model.subdevices )
        for( auto && p : s->profiles )
            if( ! sub && p.stream_type() == RS2_STREAM_COLOR && p.format() == RS2_FORMAT_RAW16 )
            {
                sub = s;
                raw = p.as< rs2::video_stream_profile >();
            }
    if( ! sub )
    {
        test.imgui->LogInfo( "no color RAW16 profile, skipping" );
        return;
    }

    test.expand_sensor_panel( model, sub );
    test.select_resolution( model, sub, rsutils::string::from() << raw.width() << " x " << raw.height() );
    test.collapse_sensor_panel( model, sub );
    auto & formats = sub->format_values[raw.unique_id()];
    sub->ui.selected_format_id[raw.unique_id()]
        = int( std::find( formats.begin(), formats.end(), RS2_FORMAT_RAW16 ) - formats.begin() );

    test.click_stream_toggle_on( model, sub );
    IM_CHECK( test.all_streams_alive() );
    test.sleep( 1.0f );

    std::string base = rsutils::os::get_special_folder( rsutils::os::special_folder::temp_folder ) + "/viewer-raw16-snapshot";
    for( auto && s : test.viewer_model.streams )
        if( s.second.profile.stream_type() == RS2_STREAM_COLOR )
            s.second.snapshot_frame( ( base + ".png" ).c_str(), test.viewer_model );

    test.click_stream_toggle_off( model, sub );
    test.sleep( 1.0f );

    // A RAW16 tile must reach the PNG writer as 1-byte grayscale (IHDR color type 0), not 2-byte gray+alpha (4)
    std::ifstream png( base + "_Color.png", std::ios::binary );
    char header[26] = {};
    IM_CHECK( png.read( header, sizeof( header ) ).good() );
    IM_CHECK_EQ( int( header[25] ), 0 );

    std::ifstream raw_file( base + "_Color.raw", std::ios::binary | std::ios::ate );
    IM_CHECK_EQ( static_cast< long long >( raw_file.tellg() ), static_cast< long long >( raw.width() ) * raw.height() * 2 );
    IM_CHECK( std::ifstream( base + "_Color_metadata.csv" ).good() );
}
