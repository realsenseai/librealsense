// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2019 RealSense, Inc. All Rights Reserved.

#include "ds/d400/d400-options.h"
#include "ds/d400/d400-motion.h"
#include "synthetic-stream.h"
#include "motion-transform.h"
#include "stream.h"
#include <src/platform/hid-data.h>
#include <src/core/frame-processor-callback.h>


namespace librealsense
{
    class converter_16_bit : public imu_to_librs_converter
    {
    public:
        converter_16_bit( double scale_factor ) : imu_to_librs_converter( scale_factor )
        {
        }

        void convert( uint8_t * const dest[], const uint8_t * source ) override
        {
            // The backend puts the data in a struct with 32 bit fields, data is valid at the lower 16 bits only.
            // Converting to int16_t before casting to float avoids incorrect handling of negative values and overflows.
            hid_data hid = *reinterpret_cast< const hid_data * >( source );
            hid.x = static_cast< int16_t >( hid.x );
            hid.y = static_cast< int16_t >( hid.y );
            hid.z = static_cast< int16_t >( hid.z );

            float3 res = float3{ float( hid.x ), float( hid.y ), float( hid.z ) } * float( _scale_factor );
            std::memcpy( dest[0], &res, sizeof( float3 ) );
        }
    };

    class converter_32_bit : public imu_to_librs_converter
    {
    public:
        converter_32_bit( double scale_factor ) : imu_to_librs_converter( scale_factor )
        {
        }

        void convert( uint8_t * const dest[], const uint8_t * source ) override
        {
            const hid_data * hid = reinterpret_cast< const hid_data * >( source );
            float3 res = float3{ float( hid->x ), float( hid->y ), float( hid->z ) } * float( _scale_factor );
            std::memcpy( dest[0], &res, sizeof( float3 ) );
        }
    };

    class converter_16_bit_mipi : public imu_to_librs_converter
    {
    public:
        converter_16_bit_mipi( double scale_factor ) : imu_to_librs_converter( scale_factor )
        {
        }

        void convert( uint8_t * const dest[], const uint8_t * source ) override
        {
            const hid_mipi_data * hid = reinterpret_cast< const hid_mipi_data * >( source );
            float3 res = float3{ float( hid->x ), float( hid->y ), float( hid->z ) } * float( _scale_factor );
            std::memcpy( dest[0], &res, sizeof( float3 ) );
        }
    };

    class converter_32_bit_mipi : public imu_to_librs_converter
    {
    public:
        converter_32_bit_mipi( double scale_factor ) : imu_to_librs_converter( scale_factor )
        {
        }

        void convert( uint8_t * const dest[], const uint8_t * source ) override
        {
            const hid_mipi_data_32 * hid = reinterpret_cast< const hid_mipi_data_32 * >( source );
            float3 res = float3{ float( hid->x ), float( hid->y ), float( hid->z ) } * float( _scale_factor );
            std::memcpy( dest[0], &res, sizeof( float3 ) );
        }
    };

    motion_transform::motion_transform( rs2_format target_format,
                                        rs2_stream target_stream,
                                        std::shared_ptr< mm_calib_handler > mm_calib,
                                        std::shared_ptr< enable_motion_correction > mm_correct_opt )
        : motion_transform( "Motion Transform", target_format, target_stream, mm_calib, mm_correct_opt )
    {
    }

    motion_transform::motion_transform( const char * name,
                                        rs2_format target_format,
                                        rs2_stream target_stream,
                                        std::shared_ptr< mm_calib_handler > mm_calib,
                                        std::shared_ptr< enable_motion_correction > mm_correct_opt )
        : functional_processing_block( name, target_format, target_stream, RS2_EXTENSION_MOTION_FRAME )
        , _mm_correct_opt( mm_correct_opt )
    {
        if (mm_calib)
        {
            _imu2depth_cs_alignment_matrix = (*mm_calib).imu_to_depth_alignment();
            if (_mm_correct_opt)
            {
                auto accel_intr = (*mm_calib).get_intrinsic(RS2_STREAM_ACCEL);
                auto gyro_intr = (*mm_calib).get_intrinsic(RS2_STREAM_GYRO);
                _accel_sensitivity  = accel_intr.sensitivity;
                _accel_bias         = accel_intr.bias;
                _gyro_sensitivity   = gyro_intr.sensitivity;
                _gyro_bias          = gyro_intr.bias;
            }
        }
        else
        {
            _imu2depth_cs_alignment_matrix = { {1,0,0},{0,1,0}, {0,0,1} };
        }
    }

    rs2::frame motion_transform::process_frame(const rs2::frame_source& source, const rs2::frame& f)
    {
        auto&& ret = functional_processing_block::process_frame(source, f);
        correct_motion(&ret);

        return ret;
    }

    void motion_transform::correct_motion_helper(float3* xyz, rs2_stream stream_type) const
    {
        // The IMU sensor orientation shall be aligned with depth sensor's coordinate system
        *xyz = _imu2depth_cs_alignment_matrix * (*xyz);

        // IMU calibration is done with data in depth sensor's coordinate system, so calibration parameters should be applied for motion correction
        // in the same coordinate system
        if (_mm_correct_opt)
        {
            if ((_mm_correct_opt->query() > 0.f)) // TBD resolve duality of is_enabled/is_active
            {
                if (stream_type == RS2_STREAM_ACCEL)
                    *xyz = (_accel_sensitivity * (*xyz)) - _accel_bias;

                if (stream_type == RS2_STREAM_GYRO)
                    *xyz = _gyro_sensitivity * (*xyz) - _gyro_bias;
            }
        }
    }
    void motion_transform::correct_motion(rs2::frame* f) const
    {
        auto xyz = (float3*)(f->get_data());

        correct_motion_helper(xyz, f->get_profile().stream_type());
    }

    void motion_to_accel_gyro::correct_motion(float3* xyz) const
    {
        correct_motion_helper(xyz, _accel_gyro_target_profile->get_stream_type());
    }

    motion_to_accel_gyro::motion_to_accel_gyro( std::shared_ptr< mm_calib_handler > mm_calib,
                                                std::shared_ptr< enable_motion_correction > mm_correct_opt,
                                                double gyro_scale_factor,
                                                bool high_accuracy )
        : motion_to_accel_gyro( "Accel_Gyro Transform", mm_calib, mm_correct_opt, gyro_scale_factor, high_accuracy )
    {}

    motion_to_accel_gyro::motion_to_accel_gyro( const char * name,
                                                std::shared_ptr< mm_calib_handler > mm_calib,
                                                std::shared_ptr< enable_motion_correction > mm_correct_opt,
                                                double gyro_scale_factor,
                                                bool high_accuracy )
        : motion_transform(name, RS2_FORMAT_MOTION_XYZ32F, RS2_STREAM_ANY, mm_calib, mm_correct_opt)
    {
        static constexpr float gravity = 9.80665f;  // Standard Gravitation Acceleration
        static constexpr double accelerator_scale_factor = 0.001 * gravity;

        if( high_accuracy )
        {
            _converter = std::make_unique< converter_32_bit_mipi >( deg2rad( gyro_scale_factor ) );
            _accel_converter = std::make_unique< converter_32_bit_mipi >( accelerator_scale_factor );
        }
        else
        {
            _converter = std::make_unique< converter_16_bit_mipi >( deg2rad( gyro_scale_factor ) );
            _accel_converter = std::make_unique< converter_16_bit_mipi >( accelerator_scale_factor );
        }
        configure_processing_callback();
    }

    void motion_to_accel_gyro::configure_processing_callback()
    {
        // define and set the frame processing callback
        auto process_callback = [&](frame_holder && frame, synthetic_source_interface* source)
        {
            auto profile = As<motion_stream_profile, stream_profile_interface>(frame.frame->get_stream());
            if (!profile)
            {
                LOG_ERROR("Failed configuring accel_gyro processing block: ", get_info(RS2_CAMERA_INFO_NAME));
                return;
            }

            if (profile.get() != _source_stream_profile.get())
            {
                _source_stream_profile = profile;
                // creating profile that fits the frame real stream type
                _accel_gyro_target_profile = profile->clone();
                _accel_gyro_target_profile->set_format(_target_format);
                _accel_gyro_target_profile->set_stream_index(0);
                //_accel_gyro_target_profile->set_unique_id(_target_profile_idx);
            }

            if (frame->get_frame_data()[0] == 1)
            {
                _accel_gyro_target_profile->set_stream_type(RS2_STREAM_ACCEL);
            }
            else if (frame->get_frame_data()[0] == 2)
            {
                _accel_gyro_target_profile->set_stream_type(RS2_STREAM_GYRO);
            }
            else
            {
                throw("motion_to_accel_gyro::configure_processing_callback - stream type not discovered");
            }

            frame_holder agf;
            agf = source->allocate_motion_frame(_accel_gyro_target_profile, frame);

            // process the frame
            uint8_t * frame_data[1];
            frame_data[0] = (uint8_t *)agf.frame->get_frame_data();
            process_function(frame_data, (const uint8_t *)frame->get_frame_data(), 0, 0, 0, 0);

            // correct the axes values according to the device's data
            correct_motion((float3*)(frame_data[0]));

            source->frame_ready(std::move(agf));
        };

        set_processing_callback( make_frame_processor_callback( std::move( process_callback ) ) );
    }

    void motion_to_accel_gyro::process_function( uint8_t * const dest[], const uint8_t * source, int, int, int, int )
    {
        // The mipi stream alternates between accel and gyro data.
        // '_accel_converter' processes accel,
        // while '_converter' processes gyro.
        if (source[0] == 1)//accel
        {
            _target_stream = RS2_STREAM_ACCEL;
            _accel_converter->convert( dest, source );
        }
        else if (source[0] == 2)//gyro
        {
            _target_stream = RS2_STREAM_GYRO;
            _converter->convert( dest, source );
        }
        else
        {
            throw("motion_to_accel_gyro::process_function - stream type not discovered");
        }

    }

    acceleration_transform::acceleration_transform( std::shared_ptr< mm_calib_handler > mm_calib,
                                                    std::shared_ptr< enable_motion_correction > mm_correct_opt,
                                                    bool high_accuracy )
        : acceleration_transform( "Acceleration Transform", mm_calib, mm_correct_opt, high_accuracy )
    {}

    acceleration_transform::acceleration_transform( const char * name,
                                                    std::shared_ptr< mm_calib_handler > mm_calib,
                                                    std::shared_ptr< enable_motion_correction > mm_correct_opt,
                                                    bool high_accuracy )
        : motion_transform( name, RS2_FORMAT_MOTION_XYZ32F, RS2_STREAM_ACCEL, mm_calib, mm_correct_opt )
    {
        static constexpr float gravity = 9.80665f;  // Standard Gravitation Acceleration
        static constexpr double accelerator_scale_factor = 0.001 * gravity;

        if( high_accuracy )
            _converter = std::make_unique< converter_32_bit >( accelerator_scale_factor );
        else
            _converter = std::make_unique< converter_16_bit >( accelerator_scale_factor );
    }

    void acceleration_transform::process_function( uint8_t * const dest[], const uint8_t * source, int, int, int, int )
    {
        _converter->convert( dest, source );
    }

    gyroscope_transform::gyroscope_transform( std::shared_ptr< mm_calib_handler > mm_calib,
                                              std::shared_ptr< enable_motion_correction > mm_correct_opt,
                                              double gyro_scale_factor, bool high_accuracy )
        : gyroscope_transform( "Gyroscope Transform", mm_calib, mm_correct_opt, gyro_scale_factor, high_accuracy )
    {}

    gyroscope_transform::gyroscope_transform( const char * name,
                                              std::shared_ptr< mm_calib_handler > mm_calib,
                                              std::shared_ptr< enable_motion_correction > mm_correct_opt,
                                              double gyro_scale_factor, bool high_accuracy )
        : motion_transform(name, RS2_FORMAT_MOTION_XYZ32F, RS2_STREAM_GYRO, mm_calib, mm_correct_opt)
    {
        if( high_accuracy )
            _converter = std::make_unique< converter_32_bit >( deg2rad( gyro_scale_factor ) );
        else
            _converter = std::make_unique< converter_16_bit >( deg2rad( gyro_scale_factor ) );
    }

    void gyroscope_transform::process_function( uint8_t * const dest[], const uint8_t * source, int, int, int, int )
    {
        _converter->convert( dest, source );
    }
}

