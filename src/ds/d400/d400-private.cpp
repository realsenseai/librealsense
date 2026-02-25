//// License: Apache 2.0. See LICENSE file in root directory.
//// Copyright(c) 2015 RealSense, Inc. All Rights Reserved.

#include "d400-private.h"

using namespace std;


namespace librealsense
{
    namespace ds
    {
        bool d400_try_fetch_usb_device(std::vector<platform::usb_device_info>& devices,
            const platform::uvc_device_info& info, platform::usb_device_info& result)
        {
            for (auto it = devices.begin(); it != devices.end(); ++it)
            {
                if (it->unique_id == info.unique_id)
                {
                    bool found = false;
                    result = *it;
                    switch (info.pid)
                    {
                    case RS_USB2_PID:
                    case RS400_PID:
                    case RS405U_PID:
                    case RS410_PID:
                    case RS416_PID:
                    case RS460_PID:
                    case RS430_PID:
                    case RS420_PID:
                    case RS421_PID:
                    case RS400_IMU_PID:
                        found = (result.mi == 3);
                        break;
                    case RS405_PID:
                    case RS430I_PID:
                        found = (result.mi == 4);
                        break;
                    case RS430_MM_PID:
                    case RS420_MM_PID:
                    case RS435I_PID:
                    case RS436_PID:
                    case RS455_PID:
                        found = (result.mi == 6);
                        break;
                    case RS415_PID:
                    case RS416_RGB_PID:
                    case RS435_RGB_PID:
                        found = (result.mi == 5);
                        break;
                    default:
                        throw not_implemented_exception(rsutils::string::from() << "USB device "
                            << std::hex << info.pid << ":" << info.vid << std::dec << " is not supported.");
                        break;
                    }

                    if (found)
                    {
                        devices.erase(it);
                        return true;
                    }
                }
            }
            return false;
        }

        std::vector<platform::uvc_device_info> filter_d400_device_by_capability(const std::vector<platform::uvc_device_info>& devices,
            ds_caps caps)
        {
            std::vector<platform::uvc_device_info> results;

            switch (caps)
            {
            case ds_caps::CAP_FISHEYE_SENSOR:
                std::copy_if(devices.begin(), devices.end(), std::back_inserter(results),
                    [](const platform::uvc_device_info& info)
                    {
                        return d400_fisheye_pid.find(info.pid) != d400_fisheye_pid.end();
                    });
                break;
            default:
                throw invalid_value_exception(rsutils::string::from()
                    << "Capability filters are not implemented for val "
                    << std::hex << caps << std::dec);
            }
            return results;
        }

        std::string extract_firmware_version_string( const std::vector< uint8_t > & fw_image )
        {
            auto version_offset = offsetof( platform::dfu_header, bcdDevice );
            if( fw_image.size() < ( version_offset + sizeof( size_t ) ) )
                throw std::runtime_error( "Firmware binary image might be corrupted - size is only: "
                                          + std::to_string( fw_image.size() ) );

            auto version = fw_image.data() + version_offset;
            uint8_t major = *( version + 3 );
            uint8_t minor = *( version + 2 );
            uint8_t patch = *( version + 1 );
            uint8_t build = *( version );

            return std::to_string( major ) + "." + std::to_string( minor ) + "." + std::to_string( patch ) + "."
                 + std::to_string( build );
        }

        rs2_intrinsics get_d400_intrinsic_by_resolution(const vector<uint8_t>& raw_data, d400_calibration_table_id table_id, uint32_t width, uint32_t height)
        {
            switch (table_id)
            {
            case d400_calibration_table_id::coefficients_table_id:
            {
                return get_d400_intrinsic_by_resolution_coefficients_table(raw_data, width, height);
            }
            case d400_calibration_table_id::fisheye_calibration_id:
            {
                return get_intrinsic_fisheye_table(raw_data, width, height);
            }
            case d400_calibration_table_id::rgb_calibration_id:
            {
                return get_d400_color_stream_intrinsic(raw_data, width, height);
            }
            default:
                throw invalid_value_exception(rsutils::string::from() << "Parsing Calibration table type " << static_cast<int>(table_id) << " is not supported");
            }
        }

        rs2_intrinsics get_d400_intrinsic_by_resolution_coefficients_table(const std::vector<uint8_t>& raw_data, uint32_t width, uint32_t height)
        {
            auto table = check_calib<ds::d400_coefficients_table>(raw_data);

#define intrinsics_string(res) #res << "\t" << array2str((float_4&)table->rect_params[res]) << endl

            LOG_DEBUG(endl
                << "baseline = " << table->baseline << " mm" << endl
                << "Rect params:  \t fX\t\t fY\t\t ppX\t\t ppY \n"
                << intrinsics_string(res_1920_1080)
                << intrinsics_string(res_1280_720)
                << intrinsics_string(res_640_480)
                << intrinsics_string(res_848_480)
                << intrinsics_string(res_424_240)
                << intrinsics_string(res_640_360)
                << intrinsics_string(res_320_240)
                << intrinsics_string(res_480_270)
                << intrinsics_string(res_1280_800)
                << intrinsics_string(res_960_540));

#undef intrinsics_string

            auto resolution = width_height_to_ds_rect_resolutions(width, height);

            if (width == 848 && height == 100) // Special 848x100 resolution is available in some firmware versions
                // for this resolution we use the same base-intrinsics as for 848x480 and adjust them later
            {
                resolution = width_height_to_ds_rect_resolutions(width, 480);
            }

            if (resolution < max_ds_rect_resolutions)
            {
                rs2_intrinsics intrinsics;
                intrinsics.width = resolutions_list[resolution].x;
                intrinsics.height = resolutions_list[resolution].y;

                auto rect_params = static_cast<const float4>(table->rect_params[resolution]);
                // DS5U - assume ideal intrinsic params
                if ((rect_params.x == rect_params.y) && (rect_params.z == rect_params.w))
                {
                    rect_params.x = rect_params.y = intrinsics.width * 1.5f;
                    rect_params.z = intrinsics.width * 0.5f;
                    rect_params.w = intrinsics.height * 0.5f;
                }
                intrinsics.fx = rect_params[0];
                intrinsics.fy = rect_params[1];
                intrinsics.ppx = rect_params[2];
                intrinsics.ppy = rect_params[3];
                intrinsics.model = RS2_DISTORTION_BROWN_CONRADY;
                memset(intrinsics.coeffs, 0, sizeof(intrinsics.coeffs));  // All coefficients are zeroed since rectified depth is defined as CS origin

                // In case of the special 848x100 resolution adjust the intrinsics
                if (width == 848 && height == 100)
                {
                    intrinsics.height = 100;
                    intrinsics.ppy -= 190;
                }

                return intrinsics;
            }
            else
            {
                // Intrinsics not found in the calibration table - use the generic calculation
                ds_rect_resolutions resolution = res_1920_1080;
                rs2_intrinsics intrinsics;
                intrinsics.width = width;
                intrinsics.height = height;

                auto rect_params = static_cast<const float4>(table->rect_params[resolution]);
                // DS5U - assume ideal intrinsic params
                if ((rect_params.x == rect_params.y) && (rect_params.z == rect_params.w))
                {
                    rect_params.x = rect_params.y = intrinsics.width * 1.5f;
                    rect_params.z = intrinsics.width * 0.5f;
                    rect_params.w = intrinsics.height * 0.5f;
                }

                // Special resolution for auto-calibration requires special treatment...
                if (width == 256 && height == 144)
                {
                    intrinsics.fx = rect_params[0];
                    intrinsics.fy = rect_params[1];
                    intrinsics.ppx = rect_params[2] - 832;
                    intrinsics.ppy = rect_params[3] - 468;
                }
                else
                {
                    intrinsics.fx = rect_params[0] * width / resolutions_list[resolution].x;
                    intrinsics.fy = rect_params[1] * height / resolutions_list[resolution].y;
                    intrinsics.ppx = rect_params[2] * width / resolutions_list[resolution].x;
                    intrinsics.ppy = rect_params[3] * height / resolutions_list[resolution].y;
                }

                intrinsics.model = RS2_DISTORTION_BROWN_CONRADY;
                memset(intrinsics.coeffs, 0, sizeof(intrinsics.coeffs));  // All coefficients are zeroed since rectified depth is defined as CS origin

                return intrinsics;
            }
        }

        pose get_d400_color_stream_extrinsic(const std::vector<uint8_t>& raw_data)
        {
            auto table = check_calib<d400_rgb_calibration_table>(raw_data);
            float3 trans_vector = table->translation_rect;
            float3x3 rect_rot_mat = table->rotation_matrix_rect;
            float trans_scale = -0.001f; // Convert units from mm to meter. Extrinsic of color is referenced to the Depth Sensor CS

            trans_vector.x *= trans_scale;
            trans_vector.y *= trans_scale;
            trans_vector.z *= trans_scale;

            return{ rect_rot_mat,trans_vector };
        }

        rs2_intrinsics get_d400_color_stream_intrinsic(const std::vector<uint8_t>& raw_data, uint32_t width, uint32_t height)
        {
            auto table = check_calib<ds::d400_rgb_calibration_table>(raw_data);

            // Compensate for aspect ratio as the normalized intrinsic is calculated with a single resolution
            float3x3 intrin = table->intrinsic;
            float calib_aspect_ratio = 9.f / 16.f; // shall be overwritten with the actual calib resolution

            if (table->calib_width && table->calib_height)
                calib_aspect_ratio = float(table->calib_height) / float(table->calib_width);
            else
            {
                LOG_WARNING("RGB Calibration resolution is not specified, using default 16/9 Aspect ratio");
            }

            // Compensate for aspect ratio
            float actual_aspect_ratio = height / (float)width;
            if (actual_aspect_ratio < calib_aspect_ratio)
            {
                intrin(1, 1) *= calib_aspect_ratio / actual_aspect_ratio;
                intrin(2, 1) *= calib_aspect_ratio / actual_aspect_ratio;
            }
            else
            {
                intrin(0, 0) *= actual_aspect_ratio / calib_aspect_ratio;
                intrin(2, 0) *= actual_aspect_ratio / calib_aspect_ratio;
            }

            // Calculate specific intrinsic parameters based on the normalized intrinsic and the sensor's resolution
            rs2_intrinsics calc_intrinsic{
                static_cast<int>(width),
                static_cast<int>(height),
                ((1 + intrin(2, 0)) * width) / 2.f,
                ((1 + intrin(2, 1)) * height) / 2.f,
                intrin(0, 0) * width / 2.f,
                intrin(1, 1) * height / 2.f,
                RS2_DISTORTION_BROWN_CONRADY
            };
            std::memcpy(calc_intrinsic.coeffs, table->distortion, sizeof(table->distortion));
            //LOG_DEBUG(endl << array2str((float_4&)(calc_intrinsic.fx, calc_intrinsic.fy, calc_intrinsic.ppx, calc_intrinsic.ppy)) << endl);

            static rs2_intrinsics ref{};
            if (memcmp(&calc_intrinsic, &ref, sizeof(rs2_intrinsics)))
            {
                LOG_DEBUG_THERMAL_LOOP("RGB Intrinsic: ScaleX, ScaleY = "
                    << std::setprecision(3) << intrin(0, 0) << ", " << intrin(1, 1)
                    << ". Fx,Fy = " << calc_intrinsic.fx << "," << calc_intrinsic.fy);
                ref = calc_intrinsic;
            }

            return calc_intrinsic;
        }

        //D405 needs special calculation because the ISP crops the full sensor image using non linear transformation.
        rs2_intrinsics get_d405_color_stream_intrinsic(const std::vector<uint8_t>& raw_data, uint32_t width, uint32_t height)
        {
            struct resolution
            {
                uint32_t width = 0;
                uint32_t height = 0;
            };

            // Convert normalized focal length and principal point to pixel units (K matrix format)
            auto normalized_k_to_pixels = [&]( float3x3 & k, resolution res )
            {
                if( res.width == 0 || res.height == 0 )
                    throw invalid_value_exception( rsutils::string::from() <<
                                                   "Unsupported resolution used (" << width << ", " << height << ")" );

                k( 0, 0 ) = k( 0, 0 ) * res.width / 2.f;      // fx
                k( 1, 1 ) = k( 1, 1 ) * res.height / 2.f;     // fy
                k( 2, 0 ) = ( k( 2, 0 ) + 1 ) * res.width / 2.f;  // ppx
                k( 2, 1 ) = ( k( 2, 1 ) + 1 ) * res.height / 2.f;  // ppy
            };

            // Scale focal length and principal point in pixel units from one resolution to another (K matrix format)
            auto scale_pixel_k = [&]( float3x3 & k, resolution in_res, resolution out_res)
            {
                if( in_res.width == 0 || in_res.height == 0 || out_res.width == 0 || out_res.height == 0 )
                    throw invalid_value_exception( rsutils::string::from() <<
                                                   "Unsupported resolution used (" << width << ", " << height << ")" );

                float scale_x = out_res.width / static_cast< float >( in_res.width );
                float scale_y = out_res.height / static_cast< float >( in_res.height );
                float scale = max( scale_x, scale_y );
                float shift_x = ( in_res.width * scale - out_res.width ) / 2.f;
                float shift_y = ( in_res.height * scale - out_res.height ) / 2.f;

                k( 0, 0 ) = k( 0, 0 ) * scale;  // fx
                k( 1, 1 ) = k( 1, 1 ) * scale;  // fy
                k( 2, 0 ) = k( 2, 0 ) * scale - shift_x;  // ppx
                k( 2, 1 ) = k( 2, 1 ) * scale - shift_y;  // ppy
            };

            auto table = check_calib< ds::d400_rgb_calibration_table >( raw_data );
            auto output_res = resolution{ width, height };
            auto calibration_res = resolution{ table->calib_width, table->calib_height };

            float3x3 k = table->intrinsic;
            if( width == 1280 && height == 720 )
                normalized_k_to_pixels( k, output_res );
            else if( width == 640 && height == 480 ) // 640x480 is 4:3 not 16:9 like other available resolutions, ISP handling is different.
            {
                auto raw_res = resolution{ 1280, 800 };
                // Extrapolate K to raw resolution
                float scale_y = calibration_res.height / static_cast< float >( raw_res.height );
                k( 1, 1 ) = k( 1, 1 ) * scale_y;  // fy
                k( 2, 1 ) = k( 2, 1 ) * scale_y;  // ppy
                normalized_k_to_pixels( k, raw_res );
                // Handle ISP scaling to 770x480
                auto scale_res = resolution{ 770, 480 };
                scale_pixel_k( k, raw_res, scale_res );
                // Handle ISP cropping to 640x480
                k( 2, 0 ) = k( 2, 0 ) - ( scale_res.width - output_res.width ) / 2;  // ppx
                k( 2, 1 ) = k( 2, 1 ) - ( scale_res.height - output_res.height ) / 2;  // ppy
            }
            else
            {
                normalized_k_to_pixels( k, calibration_res );
                scale_pixel_k( k, calibration_res, output_res );
            }

            // Convert k matrix format to rs2 format
            rs2_intrinsics rs2_intr{
                static_cast< int >( width ),
                static_cast< int >( height ),
                k( 2, 0 ),
                k( 2, 1 ),
                k( 0, 0 ),
                k( 1, 1 ),
                RS2_DISTORTION_BROWN_CONRADY
            };
            std::memcpy( rs2_intr.coeffs, table->distortion, sizeof( table->distortion ) );

            return rs2_intr;
        }

        // Parse intrinsics from newly added RECPARAMSGET command
        bool try_get_d400_intrinsic_by_resolution_new(const vector<uint8_t>& raw_data,
            uint32_t width, uint32_t height, rs2_intrinsics* result)
        {
            using namespace ds;
            auto count = raw_data.size() / sizeof(new_calibration_item);
            auto items = (new_calibration_item*)raw_data.data();
            for (int i = 0; i < count; i++)
            {
                auto&& item = items[i];
                if (item.width == width && item.height == height)
                {
                    result->width = width;
                    result->height = height;
                    result->ppx = item.ppx;
                    result->ppy = item.ppy;
                    result->fx = item.fx;
                    result->fy = item.fy;
                    result->model = RS2_DISTORTION_BROWN_CONRADY;
                    memset(result->coeffs, 0, sizeof(result->coeffs));  // All coefficients are zeroed since rectified depth is defined as CS origin

                    return true;
                }
            }
            return false;
        }

    } // librealsense::ds
} // namespace librealsense