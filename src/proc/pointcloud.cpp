// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 RealSense, Inc. All Rights Reserved.

#include "pointcloud.h"
#include "occlusion-filter.h"
#include <src/environment.h>
#include <src/core/depth-frame.h>
#include <src/option.h>
#include <src/device.h>
#include <src/stream.h>
#include <src/points.h>
#include <src/core/sensor-interface.h>
#include "device-calibration.h"

#include <librealsense2/rs.hpp>

#include <rsutils/string/from.h>

#ifdef RS2_USE_CUDA
#include "proc/cuda/cuda-pointcloud.h"
#include "rsutils/accelerators/gpu.h"
#endif
#ifdef __SSSE3__
#include "proc/sse/sse-pointcloud.h"
#endif
#include "proc/neon/neon-pointcloud.h"


namespace librealsense
{
    void pointcloud::preprocess()
    {
        _pre_compute_map_x.resize(_depth_intrinsics->width*_depth_intrinsics->height);
        _pre_compute_map_y.resize(_depth_intrinsics->width*_depth_intrinsics->height);

        for (int y = 0; y < _depth_intrinsics->height; ++y)
        {
            for (int x = 0; x < _depth_intrinsics->width; ++x)
            {
                const float pixel[] = { (float)x, (float)y };
                float point[3];

                const rs2_intrinsics & intrin = *_depth_intrinsics;
                rs2_deproject_pixel_to_point(point, &intrin, pixel, 1);

                _pre_compute_map_x[y*_depth_intrinsics->width + x] = point[0];
                _pre_compute_map_y[y*_depth_intrinsics->width + x] = point[1];
            }
        }
    }

    const float3 * pointcloud::depth_to_points(rs2::points output, 
        const rs2_intrinsics &depth_intrinsics, const rs2::depth_frame& depth_frame)
    {

        auto image = output.get_vertices();
        auto depth_scale = depth_frame.get_units();

        const uint16_t* depth = (const uint16_t*)depth_frame.get_data();
        float* points = (float*)image;

        for (int y = 0; y < depth_intrinsics.height; ++y)
        {
            for (int x = 0; x < depth_intrinsics.width; ++x)
            {
                float z = depth_scale * (*depth++);

                points[0] = z * _pre_compute_map_x[y*depth_intrinsics.width + x];
                points[1] = z * _pre_compute_map_y[y*depth_intrinsics.width + x];
                points[2] = z;

                points += 3;
            }
        }

        return (float3*)image;
    }

    float3 transform(const rs2_extrinsics *extrin, const float3 &point) { float3 p = {}; rs2_transform_point_to_point(&p.x, extrin, &point.x); return p; }
    float2 project(const rs2_intrinsics *intrin, const float3 & point) { float2 pixel = {}; rs2_project_point_to_pixel(&pixel.x, intrin, &point.x); return pixel; }
    float2 pixel_to_texcoord(const rs2_intrinsics *intrin, const float2 & pixel) { return{ pixel.x / (intrin->width), pixel.y / (intrin->height) }; }
    float2 project_to_texcoord(const rs2_intrinsics *intrin, const float3 & point) { return pixel_to_texcoord(intrin, project(intrin, point)); }

    void pointcloud::set_extrinsics()
    {
        if (_output_stream && _other_stream && !_extrinsics)
        {
            rs2_extrinsics ex;
            const rs2_stream_profile* ds = _output_stream;
            const rs2_stream_profile* os = _other_stream.get_profile();
            if (environment::get_instance().get_extrinsics_graph().try_fetch_extrinsics(
                *ds->profile, *os->profile, &ex))
            {
                _extrinsics = ex;
            }
        }
    }
    
    void pointcloud::inspect_depth_frame(const rs2::frame& depth)
    {
        if (!_output_stream || _depth_stream.get_profile().get() != depth.get_profile().get())
        {
            _output_stream = depth.get_profile().as<rs2::video_stream_profile>().clone(
                RS2_STREAM_DEPTH, depth.get_profile().stream_index(), RS2_FORMAT_XYZ32F);
            _depth_stream = depth;
            _depth_intrinsics = optional_value<rs2_intrinsics>();
            _depth_units = ((depth_frame*)depth.get())->get_units();
            _extrinsics = optional_value<rs2_extrinsics>();
        }

        bool found_depth_intrinsics = false;

        if (!_depth_intrinsics)
        {
            auto stream_profile = depth.get_profile();
            if (auto video = stream_profile.as<rs2::video_stream_profile>())
            {
                _depth_intrinsics = video.get_intrinsics();
                _pixels_map.resize(_depth_intrinsics->height*_depth_intrinsics->width);
                _occlusion_filter->set_depth_intrinsics(_depth_intrinsics.value());

                preprocess();

                found_depth_intrinsics = true;
            }
        }

        set_extrinsics();
    }

    template< class callback >
    rs2_calibration_change_callback_sptr create_calibration_change_callback_ptr( callback&& cb )
    {
        return {
            new rs2::calibration_change_callback< callback >( std::move( cb ) ),
            []( rs2_calibration_change_callback* p ) { p->release(); }
        };
    }

    void pointcloud::inspect_other_frame(const rs2::frame& other)
    {
        if (_stream_filter != _prev_stream_filter)
        {
            _prev_stream_filter = _stream_filter;

            if (!_registered_auto_calib_cb)
            {
                auto sensor = ((frame_interface*)other.get())->get_sensor();
                if (sensor)
                {
                    _registered_auto_calib_cb
                        = std::shared_ptr< pointcloud >( this, []( pointcloud * p ) {} );

                    auto dev = sensor->get_device().shared_from_this();
                    auto * d2r = dynamic_cast<calibration_change_device*>(dev.get());
                    if( d2r )
                        try
                        {
                            std::weak_ptr< pointcloud > wr{ _registered_auto_calib_cb };
                            auto fn = [=]( rs2_calibration_status status ) {
                                auto r = wr.lock();
                                if( ! r )
                                    // nobody there any more!
                                    return;
                                if( status == RS2_CALIBRATION_SUCCESSFUL )
                                {
                                    stream_profile_interface *ds = nullptr, *os = nullptr;
                                    for( size_t x = 0, N = dev->get_sensors_count(); x < N; ++x )
                                    {
                                        sensor_interface & s = dev->get_sensor( x );
                                        for( auto const & sp : s.get_active_streams() )
                                        {
                                            if(( sp->get_stream_type() == RS2_STREAM_COLOR ) &&
                                               ( _stream_filter.stream == RS2_STREAM_COLOR ))
                                            {
                                                auto vspi = As< video_stream_profile_interface >(
                                                    sp.get() );
                                                if( vspi )
                                                {
                                                    os = vspi;
                                                    _other_intrinsics = vspi->get_intrinsics();
                                                    _occlusion_filter->set_texel_intrinsics(
                                                        _other_intrinsics.value() );
                                                }
                                            }
                                            else if( sp->get_stream_type() == RS2_STREAM_DEPTH )
                                            {
                                                ds = sp.get();
                                            }
                                        }
                                    }
                                    if( ds && os )
                                    {
                                        rs2_extrinsics ex;
                                        if( environment::get_instance()
                                                .get_extrinsics_graph()
                                                .try_fetch_extrinsics( *ds, *os, &ex ) )
                                            _extrinsics = ex;
                                        else
                                            LOG_ERROR( "Failed to refresh extrinsics after calibration change" );
                                    }
                                }
                            };

                            d2r->register_calibration_change_callback(
                                create_calibration_change_callback_ptr( std::move( fn ) ) );
                        }
                        catch( const std::bad_weak_ptr & )
                        {
                            LOG_WARNING( "Device destroyed" );
                        }
                }
            }
        }

        if (_extrinsics.has_value() && other.get_profile().get() == _other_stream.get_profile().get())
            return;

        _other_stream = other;
        _other_intrinsics = optional_value<rs2_intrinsics>();
        _extrinsics = optional_value<rs2_extrinsics>();

        if (!_other_intrinsics)
        {
            auto stream_profile = _other_stream.get_profile();
            if (auto video = stream_profile.as<rs2::video_stream_profile>())
            {
                _other_intrinsics = video.get_intrinsics();
                _occlusion_filter->set_texel_intrinsics(_other_intrinsics.value());
            }
        }

        set_extrinsics();
    }

    void pointcloud::get_texture_map(rs2::points output, 
        const float3* points,
        const unsigned int width,
        const unsigned int height,
        const rs2_intrinsics &other_intrinsics,
        const rs2_extrinsics& extr,
        float2* pixels_ptr)
    {
        auto tex_ptr = (float2*)output.get_texture_coordinates();

        for (unsigned int y = 0; y < height; ++y)
        {
            for (unsigned int x = 0; x < width; ++x)
            {
                if (points->z)
                {
                    auto trans = transform(&extr, *points);
                    //auto tex_xy = project_to_texcoord(&mapped_intr, trans);
                    // Store intermediate results for poincloud filters
                    *pixels_ptr = project(&other_intrinsics, trans);
                    auto tex_xy = pixel_to_texcoord(&other_intrinsics, *pixels_ptr);

                    *tex_ptr = tex_xy;
                }
                else
                {
                    *tex_ptr = { 0.f, 0.f };
                    *pixels_ptr = { 0.f, 0.f };
                }
                ++points;
                ++tex_ptr;
                ++pixels_ptr;
            }
        }
    }

    rs2::points pointcloud::allocate_points(const rs2::frame_source& source, const rs2::frame& depth)
    {
        return source.allocate_points(_output_stream, depth);
    }

    rs2::frame pointcloud::process_depth_frame(const rs2::frame_source& source, const rs2::depth_frame& depth)
    {
        auto res = allocate_points(source, depth);
        auto pframe = (librealsense::points*)(res.get());
        const float3* points = depth_to_points(res, *_depth_intrinsics, depth);

        auto vid_frame = depth.as<rs2::video_frame>();

        // Pixels calculated in the mapped texture. Used in post-processing filters
        float2* pixels_ptr = _pixels_map.data();
        rs2_intrinsics mapped_intr;
        rs2_extrinsics extr;
        bool map_texture = false;
        {
            if (_extrinsics && _other_intrinsics)
            {
                mapped_intr = *_other_intrinsics;
                extr = *_extrinsics;
                map_texture = true;
            }
        }

        if (map_texture)
        {
            auto height = vid_frame.get_height();
            auto width = vid_frame.get_width();

            get_texture_map(res, points, width, height, mapped_intr, extr, pixels_ptr);

            if (run__occlusion_filter(extr))
            {
                if (_occlusion_filter->find_scanning_direction(extr) == vertical)
                {
                    _occlusion_filter->set_scanning(static_cast<uint8_t>(vertical));
                    _occlusion_filter->_depth_units = _depth_units;
                }
                _occlusion_filter->process(pframe->get_vertices(), pframe->get_texture_coordinates(), _pixels_map, depth);
            }
        }
        return res;
    }

    pointcloud::pointcloud()
        : pointcloud("Pointcloud")
    {}

    pointcloud::pointcloud(const char* name)
        : stream_filter_processing_block(name)
    {
        _occlusion_filter = std::make_shared<occlusion_filter>();

        auto occlusion_invalidation = std::make_shared<ptr_option<uint8_t>>(
            occlusion_none,
            occlusion_max - 1, 1,
            occlusion_monotonic_scan,
            (uint8_t*)&_occlusion_filter->_occlusion_filter,
            "Occlusion removal");

        // Passing shared_ptr to capture list generates circular dependency and a memleak
        auto occ_inv_weak = std::weak_ptr< ptr_option< uint8_t > >( occlusion_invalidation );
        occlusion_invalidation->on_set( [this, occ_inv_weak]( float val )
        {
            auto occ_inv_shared = occ_inv_weak.lock();
            if(!occ_inv_shared) return;

            if( ! occ_inv_shared->is_valid( val ) )
                throw invalid_value_exception( rsutils::string::from()
                                               << "Unsupported occlusion filtering mode requiested " << val
                                               << " is out of range." );

            _occlusion_filter->set_mode(static_cast<uint8_t>(val));

        });
        occlusion_invalidation->set_description(1.f, "Off");
        occlusion_invalidation->set_description(2.f, "On");
        register_option(RS2_OPTION_FILTER_MAGNITUDE, occlusion_invalidation);
    }

    bool pointcloud::should_process(const rs2::frame& frame)
    {
        if (!frame)
            return false;

        auto set = frame.as<rs2::frameset>();

        if (set)
        {
            //process composite frame only if it contains both a depth frame and the requested texture frame
            if (_stream_filter.stream == RS2_STREAM_ANY)
                return false;

            auto tex = set.first_or_default(_stream_filter.stream, _stream_filter.format);
            if (!tex)
                return false;
            auto depth = set.first_or_default(RS2_STREAM_DEPTH, RS2_FORMAT_Z16);
            if (!depth)
                return false;
        }
        else
        {
            auto p = frame.get_profile();
            if (p.stream_type() == RS2_STREAM_DEPTH && p.format() == RS2_FORMAT_Z16)
                return true;

            if (p.stream_type() == _stream_filter.stream && p.format() == _stream_filter.format && p.stream_index() == _stream_filter.index)
                return true;
            return false;

            //TODO: switch to this code when map_to api is removed
            //if (_stream_filter != RS2_STREAM_ANY)
            //    return false;
            //process single frame only if it is a depth frame
            //if (frame.get_profile().stream_type() != RS2_STREAM_DEPTH || frame.get_profile().format() != RS2_FORMAT_Z16)
            //    return false;
        }

        return true;
    }

    rs2::frame pointcloud::process_frame(const rs2::frame_source& source, const rs2::frame& f)
    {
        rs2::frame rv;
        if (auto composite = f.as<rs2::frameset>())
        {
            auto texture = composite.first(_stream_filter.stream);
            inspect_other_frame(texture);

            auto depth = composite.first(RS2_STREAM_DEPTH, RS2_FORMAT_Z16);
            inspect_depth_frame(depth);
            rv = process_depth_frame(source, depth);
        }
        else
        {
            if (f.is<rs2::depth_frame>())
            {
                inspect_depth_frame(f);
                rv = process_depth_frame(source, f);
            }
            if (f.get_profile().stream_type() == _stream_filter.stream && f.get_profile().format() == _stream_filter.format)
            {
                inspect_other_frame(f);
            }
        }
        return rv;
    }

    std::shared_ptr<pointcloud> pointcloud::create()
    {
        #ifdef RS2_USE_CUDA
        if (rsutils::rs2_is_gpu_available())
        {
            return std::make_shared<librealsense::pointcloud_cuda>();
        }
        #endif
        #ifdef __SSSE3__
            return std::make_shared<librealsense::pointcloud_sse>();
        #elif defined(__ARM_NEON)  && ! defined ANDROID
            return std::make_shared<librealsense::pointcloud_neon>();
        #else
            return std::make_shared<librealsense::pointcloud>();
        #endif
    }

    bool pointcloud::run__occlusion_filter(const rs2_extrinsics& extr)
    {
        return (_occlusion_filter->active() && !_occlusion_filter->is_same_sensor(extr));
    }
}
