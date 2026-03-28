// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2023 RealSense, Inc. All Rights Reserved.

#pragma once

#include <src/basics.h>
#include <librealsense2/hpp/rs_types.hpp>


namespace librealsense {


constexpr static char const * UNKNOWN_VALUE = "UNKNOWN";


// Require the last enumerator value to be in format of RS2_#####_COUNT
#define RS2_ENUM_HELPERS( TYPE, PREFIX ) RS2_ENUM_HELPERS_CUSTOMIZED( TYPE, 0, RS2_##PREFIX##_COUNT - 1, const char * )

// This macro can be used directly if needed to support enumerators with no RS2_#####_COUNT last value
#define RS2_ENUM_HELPERS_CUSTOMIZED( TYPE, FIRST, LAST, STRTYPE )                                                      \
    LRS_EXTENSION_API STRTYPE get_string( TYPE value );                                                                \
    inline bool is_valid( TYPE value ) { return value >= FIRST && value <= LAST; }                                     \
    inline std::ostream & operator<<( std::ostream & out, TYPE value )                                                 \
    {                                                                                                                  \
        if( is_valid( value ) )                                                                                        \
            return out << get_string( value );                                                                         \
        else                                                                                                           \
            return out << (int)value;                                                                                  \
    }                                                                                                                  \
    inline bool try_parse( const std::string & str, TYPE & res )                                                       \
    {                                                                                                                  \
        for( int i = FIRST; i <= LAST; i++ )                                                                           \
        {                                                                                                              \
            auto v = static_cast< TYPE >( i );                                                                         \
            if( str == get_string( v ) )                                                                               \
            {                                                                                                          \
                res = v;                                                                                               \
                return true;                                                                                           \
            }                                                                                                          \
        }                                                                                                              \
        return false;                                                                                                  \
    }

// For rs2_option, these make use of the registry
LRS_EXTENSION_API std::string const & get_string( rs2_option value );
bool is_valid( rs2_option value );
std::ostream & operator<<( std::ostream & out, rs2_option option );
bool try_parse( const std::string & option_name, rs2_option & result );

RS2_ENUM_HELPERS_CUSTOMIZED( rs2_option_type, 0, RS2_OPTION_TYPE_COUNT - 1, std::string const & )
RS2_ENUM_HELPERS( rs2_stream, STREAM )
LRS_EXTENSION_API char const * get_abbr_string( rs2_stream );
RS2_ENUM_HELPERS( rs2_format, FORMAT )
RS2_ENUM_HELPERS( rs2_distortion, DISTORTION )
RS2_ENUM_HELPERS( rs2_camera_info, CAMERA_INFO )
RS2_ENUM_HELPERS_CUSTOMIZED( rs2_frame_metadata_value, 0, RS2_FRAME_METADATA_COUNT - 1, std::string const & )
RS2_ENUM_HELPERS( rs2_timestamp_domain, TIMESTAMP_DOMAIN )
RS2_ENUM_HELPERS( rs2_calib_target_type, CALIB_TARGET )
RS2_ENUM_HELPERS( rs2_sr300_visual_preset, SR300_VISUAL_PRESET )
RS2_ENUM_HELPERS( rs2_extension, EXTENSION )
RS2_ENUM_HELPERS( rs2_exception_type, EXCEPTION_TYPE )
RS2_ENUM_HELPERS( rs2_log_severity, LOG_SEVERITY )
RS2_ENUM_HELPERS( rs2_notification_category, NOTIFICATION_CATEGORY )
RS2_ENUM_HELPERS( rs2_playback_status, PLAYBACK_STATUS )
RS2_ENUM_HELPERS( rs2_matchers, MATCHER )
RS2_ENUM_HELPERS( rs2_sensor_mode, SENSOR_MODE )
RS2_ENUM_HELPERS( rs2_l500_visual_preset, L500_VISUAL_PRESET )
RS2_ENUM_HELPERS( rs2_calibration_type, CALIBRATION_TYPE )
RS2_ENUM_HELPERS( rs2_eth_link_priority, LINK_PRIORITY )
RS2_ENUM_HELPERS_CUSTOMIZED( rs2_calibration_status,
                             RS2_CALIBRATION_STATUS_FIRST,
                             RS2_CALIBRATION_STATUS_LAST,
                             const char * )
RS2_ENUM_HELPERS_CUSTOMIZED( rs2_ambient_light,
                             RS2_AMBIENT_LIGHT_NO_AMBIENT,
                             RS2_AMBIENT_LIGHT_LOW_AMBIENT,
                             const char * )
RS2_ENUM_HELPERS_CUSTOMIZED( rs2_digital_gain, RS2_DIGITAL_GAIN_HIGH, RS2_DIGITAL_GAIN_LOW, const char * )
RS2_ENUM_HELPERS( rs2_host_perf_mode, HOST_PERF )
RS2_ENUM_HELPERS( rs2_emitter_frequency_mode, EMITTER_FREQUENCY )
RS2_ENUM_HELPERS( rs2_depth_auto_exposure_mode, DEPTH_AUTO_EXPOSURE )
RS2_ENUM_HELPERS( rs2_safety_mode, SAFETY_MODE )
RS2_ENUM_HELPERS( rs2_d500_intercam_sync_mode, D500_INTERCAM_SYNC )
RS2_ENUM_HELPERS( rs2_point_cloud_label, POINT_CLOUD_LABEL )
RS2_ENUM_HELPERS( rs2_calib_location, CALIB_LOCATION )
RS2_ENUM_HELPERS( rs2_embedded_filter_type, EMBEDDED_FILTER_TYPE )
RS2_ENUM_HELPERS( rs2_gyro_sensitivity, GYRO_SENSITIVITY )
RS2_ENUM_HELPERS( rs2_thread_category, THREAD_CATEGORY )


}  // namespace librealsense
