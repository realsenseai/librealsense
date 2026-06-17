// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2024 RealSense, Inc. All Rights Reserved.

#include "d500-dual-rgb.h"
#include "d500-info.h"
#include "environment.h"
#include "proc/color-formats-converter.h"  // m420_converter
#include <src/uvc-sensor.h>

#include <rsutils/type/fourcc.h>
using rs_fourcc = rsutils::type::fourcc;

#include <set>


namespace librealsense
{
    d500_dual_rgb::d500_dual_rgb( std::shared_ptr< const d500_info > const & dev_info )
        : d500_device( dev_info )
        , device( dev_info )
        , _color_stream_1( new stream( RS2_STREAM_COLOR, 1 ) )
        , _color_stream_2( new stream( RS2_STREAM_COLOR, 2 ) )
    {
        auto & depth_sensor = get_depth_sensor();
        auto raw_depth_sensor = get_raw_depth_sensor();

        // The two M420 RGB cameras arrive on separate pins (USB endpoints), each also advertising identical
        // {w,h,fps,format} M420. Distinguish the color pins from the stereo-imager pin (whose M420 is colored infrared
        // and must not become a color stream) by the YUY2 companion: color pins pair M420 with YUY2, while the
        // stereo-imager pin uses UYVY/Y8I. This holds across SKUs (the per-pin companions otherwise differ - D585 uses
        // MJPEG/NV12, D555 uses NV12/BYR2). Color pins are then mapped to Color 1 / 2 in ascending pin order.
        raw_depth_sensor->set_stream_id_resolver(
            []( const std::vector< platform::stream_profile > & all, const platform::stream_profile & p,
                rs2_stream & type, int & index )
            {
                if( p.format != rs_fourcc( 'M', '4', '2', '0' ) )
                    return;

                auto is_color_pin = [&all]( uint32_t pin )
                {
                    bool m420 = false, yuy2 = false;
                    for( auto & q : all )
                    {
                        if( q.pin_index != pin )
                            continue;
                        if( q.format == rs_fourcc( 'M', '4', '2', '0' ) ) m420 = true;
                        if( q.format == rs_fourcc( 'Y', 'U', 'Y', '2' ) ) yuy2 = true;
                    }
                    return m420 && yuy2;
                };

                if( ! is_color_pin( p.pin_index ) )
                    return;  // stereo-imager M420 stays infrared - no color converter, so it is not exposed

                // Rank this pin among all color pins (ascending pin order) -> Color 1, Color 2, ...
                std::set< uint32_t > pins, color_pins;
                for( auto & q : all )
                    pins.insert( q.pin_index );
                for( auto pin : pins )
                    if( is_color_pin( pin ) )
                        color_pins.insert( pin );

                int rank = 0;
                for( auto cp : color_pins )
                {
                    if( cp == p.pin_index )
                        break;
                    ++rank;
                }

                type = RS2_STREAM_COLOR;
                index = rank + 1;
            } );

        // Register converters from M420 to the four RGB formats supported by the SDK.
        for( auto target : { RS2_FORMAT_RGB8, RS2_FORMAT_RGBA8, RS2_FORMAT_BGR8, RS2_FORMAT_BGRA8 } )
        {
            depth_sensor.register_processing_block( { { RS2_FORMAT_M420, RS2_STREAM_COLOR } },
                                                      { { target, RS2_STREAM_COLOR, 1 }, { target, RS2_STREAM_COLOR, 2 } },
                                                      [target]() { return std::make_shared< m420_converter >( target ); } );
        }

        // The color profiles are produced by the depth sensor; hand it the stream objects so it can assign them
        // (matched by stream type + index) when it builds its profiles.
        auto & d500_depth = dynamic_cast< d500_depth_sensor & >( depth_sensor );
        d500_depth.add_stream( _color_stream_1 );
        d500_depth.add_stream( _color_stream_2 );

        // Each RGB stream comes from the same physical imager as its matching infrared stream so it shares that stream's extrinsics.
        auto & graph = environment::get_instance().get_extrinsics_graph();
        graph.register_same_extrinsics( *_left_ir_stream, *_color_stream_1 );
        graph.register_same_extrinsics( *_right_ir_stream, *_color_stream_2 );
        register_stream_to_extrinsic_group( *_color_stream_1, 0 );
        register_stream_to_extrinsic_group( *_color_stream_2, 0 );
    }
}
