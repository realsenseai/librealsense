// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2024 RealSense, Inc. All Rights Reserved.

#include "core/options-registry.h"
#include "core/enum-helpers.h"

#include <rsutils/string/make-less-screamy.h>
#include <cassert>


#define STRX( X ) rsutils::string::make_less_screamy( #X )
#define STRCASE( T, X )                                                                                                \
    case RS2_##T##_##X: {                                                                                              \
        static const std::string s##T##_##X##_str = STRX( X );                                                         \
        return s##T##_##X##_str.c_str();                                                                               \
    }
#define STRARR( ARRAY, T, X ) ARRAY[RS2_##T##_##X] = STRX( X )


static std::string const unknown_value_str( librealsense::UNKNOWN_VALUE );


namespace librealsense {

   
const char * get_string( rs2_exception_type value )
{
#define CASE( X ) STRCASE( EXCEPTION_TYPE, X )
    switch( value )
    {
    CASE( UNKNOWN )
    CASE( CAMERA_DISCONNECTED )
    CASE( BACKEND )
    CASE( INVALID_VALUE )
    CASE( WRONG_API_CALL_SEQUENCE )
    CASE( NOT_IMPLEMENTED )
    CASE( DEVICE_IN_RECOVERY_MODE )
    CASE( IO )
    default:
        assert( ! is_valid( value ) );
        return UNKNOWN_VALUE;
    }
#undef CASE
}

const char * get_string( rs2_stream value )
{
#define CASE( X ) STRCASE( STREAM, X )
    switch( value )
    {
    CASE( ANY )
    CASE( DEPTH )
    CASE( COLOR )
    CASE( INFRARED )
    CASE( FISHEYE )
    CASE( GYRO )
    CASE( ACCEL )
    CASE( GPIO )
    CASE( POSE )
    CASE( CONFIDENCE )
    CASE( MOTION )
    CASE( SAFETY )
    CASE( OCCUPANCY )
    CASE( LABELED_POINT_CLOUD )
    default:
        assert( ! is_valid( value ) );
        return UNKNOWN_VALUE;
    }
#undef CASE
}

char const * get_abbr_string( rs2_stream value)
{
    switch( value )
    {
    case RS2_STREAM_ANY: return "Any";
    case RS2_STREAM_DEPTH: return "D";
    case RS2_STREAM_COLOR: return "C";
    case RS2_STREAM_INFRARED: return "IR";
    case RS2_STREAM_FISHEYE: return "FE";
    case RS2_STREAM_GYRO: return "G";
    case RS2_STREAM_ACCEL: return "A";
    case RS2_STREAM_GPIO: return "GPIO";
    case RS2_STREAM_POSE: return "P";
    case RS2_STREAM_CONFIDENCE: return "Conf";
    case RS2_STREAM_MOTION: return "M";
    case RS2_STREAM_SAFETY: return "S";
    case RS2_STREAM_OCCUPANCY: return "O";
    case RS2_STREAM_LABELED_POINT_CLOUD: return "LPC";
    default:
        assert( !is_valid( value ) );
        return "?";
    }
}


const char * get_string( rs2_sr300_visual_preset value )
{
#define CASE( X ) STRCASE( SR300_VISUAL_PRESET, X )
    switch( value )
    {
    CASE( SHORT_RANGE )
    CASE( LONG_RANGE )
    CASE( BACKGROUND_SEGMENTATION )
    CASE( GESTURE_RECOGNITION )
    CASE( OBJECT_SCANNING )
    CASE( FACE_ANALYTICS )
    CASE( FACE_LOGIN )
    CASE( GR_CURSOR )
    CASE( DEFAULT )
    CASE( MID_RANGE )
    CASE( IR_ONLY )
    default:
        assert( ! is_valid( value ) );
        return UNKNOWN_VALUE;
    }
#undef CASE
}

const char * get_string( rs2_sensor_mode value )
{
#define CASE( X ) STRCASE( SENSOR_MODE, X )
    switch( value )
    {
    CASE( VGA )
    CASE( XGA )
    CASE( QVGA )
    default:
        assert( ! is_valid( value ) );
        return UNKNOWN_VALUE;
    }
#undef CASE
}

const char * get_string( rs2_calibration_type type )
{
#define CASE( X ) STRCASE( CALIBRATION, X )
    switch( type )
    {
    CASE( AUTO_DEPTH_TO_RGB )
    CASE( MANUAL_DEPTH_TO_RGB )
    CASE( THERMAL )
    default:
        assert( ! is_valid( type ) );
        return UNKNOWN_VALUE;
    }
#undef CASE
}

const char * get_string( rs2_calibration_status value )
{
#define CASE( X ) STRCASE( CALIBRATION, X )
    switch( value )
    {
    CASE( TRIGGERED )
    CASE( SPECIAL_FRAME )
    CASE( STARTED )
    CASE( NOT_NEEDED )
    CASE( SUCCESSFUL )

    CASE( BAD_CONDITIONS )
    CASE( FAILED )
    CASE( SCENE_INVALID )
    CASE( BAD_RESULT )
    CASE( RETRY )
    default:
        assert( ! is_valid( value ) );
        return UNKNOWN_VALUE;
    }
#undef CASE
}

const char * get_string( rs2_ambient_light value )
{
#define CASE( X ) STRCASE( AMBIENT_LIGHT, X )
    switch( value )
    {
    CASE( NO_AMBIENT )
    CASE( LOW_AMBIENT )
    default:
        assert( ! is_valid( value ) );
        return UNKNOWN_VALUE;
    }
#undef CASE
}

const char * get_string( rs2_digital_gain value )
{
#define CASE( X ) STRCASE( DIGITAL_GAIN, X )
    switch( value )
    {
    CASE( HIGH )
    CASE( LOW )
    default:
        assert( ! is_valid( value ) );
        return UNKNOWN_VALUE;
    }
#undef CASE
}

const char * get_string( rs2_host_perf_mode value )
{
#define CASE( X ) STRCASE( HOST_PERF, X )
    switch( value )
    {
    CASE( DEFAULT )
    CASE( LOW )
    CASE( HIGH )
    default:
        assert( ! is_valid( value ) );
        return UNKNOWN_VALUE;
    }
#undef CASE
}

const char * get_string( rs2_emitter_frequency_mode mode )
{
#define CASE( X ) STRCASE( EMITTER_FREQUENCY, X )
    switch( mode )
    {
    CASE( 57_KHZ )
    CASE( 91_KHZ )
    default:
        assert( ! is_valid( mode ) );
        return UNKNOWN_VALUE;
    }
#undef CASE
}

const char * get_string( rs2_depth_auto_exposure_mode mode )
{
#define CASE( X ) STRCASE( DEPTH_AUTO_EXPOSURE, X )
    switch( mode )
    {
    CASE( REGULAR )
    CASE( ACCELERATED )
    default:
        assert( ! is_valid( mode ) );
        return UNKNOWN_VALUE;
    }
#undef CASE
}

const char * get_string( rs2_safety_mode mode )
{
#define CASE( X ) STRCASE( SAFETY_MODE, X )
    switch( mode )
    {
    CASE( RUN )
    CASE( STANDBY )
    CASE( SERVICE )
    default:
        assert( ! is_valid( mode ) );
        return UNKNOWN_VALUE;
    }
#undef CASE
}

const char * get_string( rs2_d500_intercam_sync_mode mode )
{
#define CASE( X ) STRCASE( D500_INTERCAM_SYNC, X )
    switch( mode )
    {
        CASE( NONE )
        CASE( RGB_MASTER )
        CASE( PWM_MASTER )
        CASE( EXTERNAL_MASTER )
    default:
        assert( ! is_valid( mode ) );
        return UNKNOWN_VALUE;
    }
#undef CASE
}

const char* get_string(rs2_point_cloud_label label)
{
#define CASE( X ) STRCASE( POINT_CLOUD_LABEL, X )
    switch (label)
    {
        CASE(UNKNOWN)
        CASE(UNDEFINED)
        CASE(INVALID)
        CASE(GROUND)
        CASE(NEAR_GROUND)
        CASE(OVERHEAD)
        CASE(ABOVE_CEILING_HEIGHT)
        CASE(GAP)
        CASE(MASKED)
        CASE(CLIFF)
        CASE(OBSTACLE)
        CASE(OBSTACLE_DANGER)
        CASE(OBSTACLE_WARNING)
    default:
        assert(!is_valid(label));
        return UNKNOWN_VALUE;
    }
#undef CASE
}


const char* get_string(rs2_calib_location calib_location)
{
#define CASE( X ) STRCASE( CALIB_LOCATION, X )
    switch (calib_location)
    {
        CASE(EEPROM)
        CASE(FLASH)
        CASE(RAM)
    default:
        assert(!is_valid(calib_location));
        return UNKNOWN_VALUE;
    }
#undef CASE
}

const char* get_string(rs2_embedded_filter_type embedded_filter_type)
{
#define CASE( X ) STRCASE( EMBEDDED_FILTER_TYPE, X )
    switch (embedded_filter_type)
    {
        CASE(DECIMATION)
        CASE(TEMPORAL)
    default:
        assert(!is_valid(embedded_filter_type));
        return UNKNOWN_VALUE;
    }
#undef CASE
}

const char * get_string( rs2_gyro_sensitivity value )
{
#define CASE( X ) STRCASE( GYRO_SENSITIVITY, X )
    switch( value )
    {
        CASE( 61_0_MILLI_DEG_SEC )
        CASE( 30_5_MILLI_DEG_SEC )
        CASE( 15_3_MILLI_DEG_SEC )
        CASE( 7_6_MILLI_DEG_SEC )
        CASE( 3_8_MILLI_DEG_SEC )
    default:
        assert( ! is_valid( value ) );
        return UNKNOWN_VALUE;
    }
#undef CASE
}

const char * get_string( rs2_extension value )
{
#define CASE( X ) STRCASE( EXTENSION, X )
    switch( value )
    {
    CASE( UNKNOWN )
    CASE( DEBUG )
    CASE( INFO )
    CASE( OPTIONS )
    CASE( MOTION )
    CASE( VIDEO )
    CASE( ROI )
    CASE( DEPTH_SENSOR )
    CASE( VIDEO_FRAME )
    CASE( MOTION_FRAME )
    CASE( COMPOSITE_FRAME )
    CASE( POINTS )
    CASE( DEPTH_FRAME )
    CASE( ADVANCED_MODE )
    CASE( RECORD )
    CASE( VIDEO_PROFILE )
    CASE( PLAYBACK )
    CASE( DEPTH_STEREO_SENSOR )
    CASE( DISPARITY_FRAME )
    CASE( MOTION_PROFILE )
    CASE( POSE_FRAME )
    CASE( POSE_PROFILE )
    CASE( TM2 )
    CASE( SOFTWARE_DEVICE )
    CASE( SOFTWARE_SENSOR )
    CASE( DECIMATION_FILTER )
    CASE( THRESHOLD_FILTER )
    CASE( DISPARITY_FILTER )
    CASE( SPATIAL_FILTER )
    CASE( TEMPORAL_FILTER )
    CASE( HOLE_FILLING_FILTER )
    CASE( ZERO_ORDER_FILTER )
    CASE( RECOMMENDED_FILTERS )
    CASE( POSE )
    CASE( POSE_SENSOR )
    CASE( WHEEL_ODOMETER )
    CASE( GLOBAL_TIMER )
    CASE( UPDATABLE )
    CASE( UPDATE_DEVICE )
    CASE( L500_DEPTH_SENSOR )
    CASE( TM2_SENSOR )
    CASE( AUTO_CALIBRATED_DEVICE )
    CASE( COLOR_SENSOR )
    CASE( MOTION_SENSOR )
    CASE( FISHEYE_SENSOR )
    CASE( DEPTH_HUFFMAN_DECODER ) // Deprecated
    CASE( SERIALIZABLE )
    CASE( FW_LOGGER )
    CASE( AUTO_CALIBRATION_FILTER )
    CASE( DEVICE_CALIBRATION )
    CASE( CALIBRATED_SENSOR )
    CASE( SEQUENCE_ID_FILTER )
    CASE( HDR_MERGE )
    CASE( MAX_USABLE_RANGE_SENSOR )
    CASE( DEBUG_STREAM_SENSOR )
    CASE( CALIBRATION_CHANGE_DEVICE )
    CASE( ROTATION_FILTER )
    CASE( SAFETY_SENSOR )
    CASE( DEPTH_MAPPING_SENSOR )
    CASE( LABELED_POINTS )
    CASE( ETH_CONFIG )
    CASE( SUPPORTED_EMBEDDED_FILTERS )
    CASE( DECIMATION_EMBEDDED_FILTER )
    CASE( TEMPORAL_EMBEDDED_FILTER )
    default:
        assert( ! is_valid( value ) );
        return UNKNOWN_VALUE;
    }
#undef CASE
}

const char * get_string( rs2_playback_status value )
{
#define CASE( X ) STRCASE( PLAYBACK_STATUS, X )
    switch( value )
    {
    CASE( UNKNOWN )
    CASE( STOPPED )
    CASE( PAUSED )
    CASE( PLAYING )
    default:
        assert( ! is_valid( value ) );
        return UNKNOWN_VALUE;
    }
#undef CASE
}

const char * get_string( rs2_log_severity value )
{
#define CASE( X ) STRCASE( LOG_SEVERITY, X )
    switch( value )
    {
    CASE( DEBUG )
    CASE( INFO )
    CASE( WARN )
    CASE( ERROR )
    CASE( FATAL )
    CASE( NONE )
    default:
        assert( ! is_valid( value ) );
        return UNKNOWN_VALUE;
    }
#undef CASE
}

std::string const & get_string_( rs2_option value )
{
    static auto str_array = []()
    {
        std::vector< std::string > arr( RS2_OPTION_COUNT );
#define CASE( X ) STRARR( arr, OPTION, X );
        CASE( BACKLIGHT_COMPENSATION )
        CASE( BRIGHTNESS )
        CASE( CONTRAST )
        CASE( EXPOSURE )
        CASE( GAIN )
        CASE( GAMMA )
        CASE( HUE )
        CASE( SATURATION )
        CASE( SHARPNESS )
        CASE( WHITE_BALANCE )
        CASE( ENABLE_AUTO_EXPOSURE )
        CASE( ENABLE_AUTO_WHITE_BALANCE )
        CASE( LASER_POWER )
        CASE( ACCURACY )
        CASE( MOTION_RANGE )
        CASE( FILTER_OPTION )
        CASE( CONFIDENCE_THRESHOLD )
        CASE( FRAMES_QUEUE_SIZE )
        CASE( VISUAL_PRESET )
        CASE( TOTAL_FRAME_DROPS )
        CASE( EMITTER_ENABLED )
        arr[RS2_OPTION_AUTO_EXPOSURE_MODE] = "Fisheye Auto Exposure Mode";
        CASE( POWER_LINE_FREQUENCY )
        CASE( ASIC_TEMPERATURE )
        CASE( ERROR_POLLING_ENABLED )
        CASE( PROJECTOR_TEMPERATURE )
        CASE( OUTPUT_TRIGGER_ENABLED )
        CASE( MOTION_MODULE_TEMPERATURE )
        CASE( DEPTH_UNITS )
        CASE( ENABLE_MOTION_CORRECTION )
        CASE( AUTO_EXPOSURE_PRIORITY )
        CASE( HISTOGRAM_EQUALIZATION_ENABLED )
        CASE( MIN_DISTANCE )
        CASE( MAX_DISTANCE )
        CASE( COLOR_SCHEME )
        CASE( TEXTURE_SOURCE )
        CASE( FILTER_MAGNITUDE )
        CASE( FILTER_SMOOTH_ALPHA )
        CASE( FILTER_SMOOTH_DELTA )
        CASE( STEREO_BASELINE )
        CASE( HOLES_FILL )
        CASE( AUTO_EXPOSURE_CONVERGE_STEP )
        CASE( INTER_CAM_SYNC_MODE )
        CASE( STREAM_FILTER )
        CASE( STREAM_FORMAT_FILTER )
        CASE( STREAM_INDEX_FILTER )
        CASE( EMITTER_ON_OFF )
        CASE( ZERO_ORDER_POINT_X )
        CASE( ZERO_ORDER_POINT_Y )
        arr[RS2_OPTION_LLD_TEMPERATURE] = "LDD temperature";
        CASE( MC_TEMPERATURE )
        CASE( MA_TEMPERATURE )
        CASE( APD_TEMPERATURE )
        CASE( HARDWARE_PRESET )
        CASE( GLOBAL_TIME_ENABLED )
        CASE( ENABLE_MAPPING )
        CASE( ENABLE_RELOCALIZATION )
        CASE( ENABLE_POSE_JUMPING )
        CASE( ENABLE_DYNAMIC_CALIBRATION )
        CASE( DEPTH_OFFSET )
        CASE( LED_POWER )
        CASE( ZERO_ORDER_ENABLED )
        CASE( ENABLE_MAP_PRESERVATION )
        CASE( FREEFALL_DETECTION_ENABLED )
        arr[RS2_OPTION_AVALANCHE_PHOTO_DIODE] = "Receiver Gain";
        CASE( POST_PROCESSING_SHARPENING )
        CASE( PRE_PROCESSING_SHARPENING )
        CASE( NOISE_FILTERING )
        CASE( INVALIDATION_BYPASS )
        // CASE(AMBIENT_LIGHT) // Deprecated - replaced by "DIGITAL_GAIN" option
        CASE( DIGITAL_GAIN )
        CASE( SENSOR_MODE )
        CASE( EMITTER_ALWAYS_ON )
        CASE( THERMAL_COMPENSATION )
        CASE( TRIGGER_CAMERA_ACCURACY_HEALTH )
        CASE( RESET_CAMERA_ACCURACY_HEALTH )
        CASE( HOST_PERFORMANCE )
        CASE( HDR_ENABLED )
        CASE( SEQUENCE_NAME )
        CASE( SEQUENCE_SIZE )
        CASE( SEQUENCE_ID )
        CASE( HUMIDITY_TEMPERATURE )
        CASE( ENABLE_MAX_USABLE_RANGE )
        arr[RS2_OPTION_ALTERNATE_IR] = "Alternate IR";
        CASE( NOISE_ESTIMATION )
        arr[RS2_OPTION_ENABLE_IR_REFLECTIVITY] = "Enable IR Reflectivity";
        CASE( AUTO_EXPOSURE_LIMIT )
        CASE( AUTO_GAIN_LIMIT )
        CASE( AUTO_RX_SENSITIVITY )
        CASE( TRANSMITTER_FREQUENCY )
        CASE( VERTICAL_BINNING )
        CASE( RECEIVER_SENSITIVITY )
        CASE( AUTO_EXPOSURE_LIMIT_TOGGLE )
        CASE( AUTO_GAIN_LIMIT_TOGGLE )
        CASE( EMITTER_FREQUENCY )
        arr[RS2_OPTION_DEPTH_AUTO_EXPOSURE_MODE] = "Auto Exposure Mode";
        CASE( OHM_TEMPERATURE )
        CASE( SOC_PVT_TEMPERATURE )
        CASE( GYRO_SENSITIVITY )
        CASE( ROTATION )
        arr[RS2_OPTION_REGION_OF_INTEREST] = "Region of Interest";
        CASE( SAFETY_PRESET_ACTIVE_INDEX )
        CASE( SAFETY_MODE )
        CASE( RGB_TNR_ENABLED )
        CASE( SAFETY_MCU_TEMPERATURE )
        CASE( LEFT_IR_TEMPERATURE )
        CASE( EMBEDDED_FILTER_ENABLED )
#undef CASE
        return arr;
    }();
    if( value >= 0 && value < RS2_OPTION_COUNT )
        return str_array[value];
    return unknown_value_str;
}

const char * get_string( rs2_eth_link_priority value )
{
#define CASE( X ) STRCASE( LINK_PRIORITY, X )
    switch( value )
    {
        CASE( USB_ONLY )
        CASE( ETH_ONLY )
        CASE( ETH_FIRST )
        CASE( USB_FIRST )
        CASE( DYNAMIC_ETH_FIRST )
        CASE( DYNAMIC_USB_FIRST )
    default:
        assert( ! is_valid( value ) );
        return UNKNOWN_VALUE;
    }
#undef CASE
}

std::string const & get_string( rs2_option const option )
{
    if( options_registry::is_option_registered( option ) )
        return options_registry::get_registered_option_name( option );
    return get_string_( option );
}


bool is_valid( rs2_option option )
{
    return options_registry::is_option_registered( option )
        || option >= 0 && option < RS2_OPTION_COUNT;
}


std::ostream & operator<<( std::ostream & out, rs2_option option )
{
    if( options_registry::is_option_registered( option ) )
        return out << options_registry::get_registered_option_name( option );
    if( option >= 0 && option < RS2_OPTION_COUNT )
        return out << get_string_( option );
    return out << (int)option;
}


bool try_parse( std::string const & option_name, rs2_option & res )
{
    auto const option = options_registry::find_option_by_name( option_name );
    if( RS2_OPTION_COUNT == option )
        return false;
    res = option;
    return true;
}


const char * get_string( rs2_format value )
{
#define CASE( X )                                                                                                      \
    case RS2_FORMAT_##X:                                                                                               \
        return #X;
    switch( value )
    {
    CASE( ANY )
    CASE( Z16 )
    CASE( DISPARITY16 )
    CASE( DISPARITY32 )
    CASE( XYZ32F )
    CASE( YUYV )
    CASE( RGB8 )
    CASE( BGR8 )
    CASE( RGBA8 )
    CASE( BGRA8 )
    CASE( Y8 )
    CASE( Y16 )
    CASE( RAW10 )
    CASE( RAW16 )
    CASE( RAW8 )
    CASE( UYVY )
    CASE( MOTION_RAW )
    CASE( MOTION_XYZ32F )
    CASE( COMBINED_MOTION )
    CASE( GPIO_RAW )
    CASE( 6DOF )
    CASE( Y10BPACK )
    CASE( DISTANCE )
    CASE( MJPEG )
    CASE( Y8I )
    CASE( Y12I )
    CASE( INZI )
    CASE( INVI )
    CASE( W10 )
    CASE( Z16H )
    CASE( FG )
    CASE( Y411 )
    CASE( Y16I )
    CASE( M420 )
    default:
        assert( ! is_valid( value ) );
        return UNKNOWN_VALUE;
    }
#undef CASE
}

const char * get_string( rs2_distortion value )
{
#define CASE( X ) STRCASE( DISTORTION, X )
    switch( value )
    {
    CASE( NONE )
    CASE( MODIFIED_BROWN_CONRADY )
    CASE( INVERSE_BROWN_CONRADY )
    CASE( FTHETA )
    CASE( BROWN_CONRADY )
    CASE( KANNALA_BRANDT4 )
    default:
        assert( ! is_valid( value ) );
        return UNKNOWN_VALUE;
    }
#undef CASE
}

const char * get_string( rs2_camera_info value )
{
#define CASE( X ) STRCASE( CAMERA_INFO, X )
    switch( value )
    {
    CASE( NAME )
    CASE( SERIAL_NUMBER )
    CASE( FIRMWARE_VERSION )
    CASE( RECOMMENDED_FIRMWARE_VERSION )
    CASE( PHYSICAL_PORT )
    CASE( DEBUG_OP_CODE )
    CASE( ADVANCED_MODE )
    CASE( PRODUCT_ID )
    CASE( CAMERA_LOCKED )
    CASE( PRODUCT_LINE )
    CASE( USB_TYPE_DESCRIPTOR )
    CASE( ASIC_SERIAL_NUMBER )
    CASE( FIRMWARE_UPDATE_ID )
    CASE( IP_ADDRESS )
    CASE( DFU_DEVICE_PATH )
    CASE( CONNECTION_TYPE )
    CASE( SMCU_FW_VERSION )
    CASE( IMU_TYPE )
    default:
        assert( ! is_valid( value ) );
        return UNKNOWN_VALUE;
    }
#undef CASE
}

std::string const & get_string( rs2_frame_metadata_value value )
{
    static auto str_array = []()
    {
        std::vector< std::string > arr( RS2_FRAME_METADATA_COUNT );
#define CASE( X ) STRARR( arr, FRAME_METADATA, X );
        CASE( FRAME_COUNTER )
        CASE( FRAME_TIMESTAMP )
        CASE( SENSOR_TIMESTAMP )
        CASE( ACTUAL_EXPOSURE )
        CASE( GAIN_LEVEL )
        CASE( AUTO_EXPOSURE )
        CASE( WHITE_BALANCE )
        CASE( TIME_OF_ARRIVAL )
        CASE( TEMPERATURE )
        CASE( BACKEND_TIMESTAMP )
        CASE( ACTUAL_FPS )
        CASE( FRAME_LASER_POWER )
        CASE( FRAME_LASER_POWER_MODE )
        CASE( EXPOSURE_PRIORITY )
        CASE( EXPOSURE_ROI_LEFT )
        CASE( EXPOSURE_ROI_RIGHT )
        CASE( EXPOSURE_ROI_TOP )
        CASE( EXPOSURE_ROI_BOTTOM )
        CASE( BRIGHTNESS )
        CASE( CONTRAST )
        CASE( SATURATION )
        CASE( SHARPNESS )
        CASE( AUTO_WHITE_BALANCE_TEMPERATURE )
        CASE( BACKLIGHT_COMPENSATION )
        CASE( GAMMA )
        CASE( HUE )
        CASE( MANUAL_WHITE_BALANCE )
        CASE( POWER_LINE_FREQUENCY )
        CASE( LOW_LIGHT_COMPENSATION )
        CASE( FRAME_EMITTER_MODE )
        CASE( FRAME_LED_POWER )
        CASE( RAW_FRAME_SIZE )
        CASE( GPIO_INPUT_DATA )
        CASE( SEQUENCE_NAME )
        CASE( SEQUENCE_ID )
        CASE( SEQUENCE_SIZE )
        CASE( TRIGGER )
        CASE( PRESET )
        CASE( INPUT_WIDTH )
        CASE( INPUT_HEIGHT )
        CASE( SUB_PRESET_INFO )
        CASE( CALIB_INFO )
        CASE( CRC )
        CASE( SAFETY_DEPTH_FRAME_COUNTER )
        CASE( SAFETY_LEVEL1 )
        CASE( SAFETY_LEVEL1_ORIGIN )
        CASE( SAFETY_LEVEL2 )
        CASE( SAFETY_LEVEL2_ORIGIN )
        CASE( SAFETY_LEVEL1_VERDICT )
        CASE( SAFETY_LEVEL2_VERDICT )
        CASE( SAFETY_OPERATIONAL_MODE )
        CASE( SAFETY_VISION_VERDICT )
        CASE( SAFETY_HARA_EVENTS )
        CASE( SAFETY_PRESET_INTEGRITY )
        CASE( SAFETY_PRESET_ID_SELECTED )
        CASE( SAFETY_PRESET_ID_USED )
        CASE( SAFETY_SIP_DEGRADATION_USED )
        CASE( SAFETY_SIP_GENERIC_METRICS_ACTIVATE )
        CASE( SAFETY_SIP_GENERIC_METRICS_STATE )
        CASE( SAFETY_SIP_GENERIC_METRICS_VALUE1 )
        CASE( SAFETY_SIP_GENERIC_METRICS_VALUE2 )
        CASE( SAFETY_SIP_GENERIC_METRICS_THRESHOLD1 )
        CASE( SAFETY_SIP_GENERIC_METRICS_THRESHOLD2 )
        CASE( SAFETY_ZERO_MONITORING_ENABLED )
        CASE( SAFETY_HARA_HISTORY_MODE )
        CASE( SAFETY_SOC_FUSA_EVENTS )
        CASE( SAFETY_SOC_FUSA_ACTION )
        CASE( SAFETY_SOC_L0_COUNTER )
        CASE( SAFETY_SOC_L0_RATE )
        CASE( SAFETY_SOC_L1_COUNTER )
        CASE( SAFETY_SOC_L1_RATE )
        CASE( SAFETY_SOC_GMT_STATUS )
        CASE( SAFETY_SOC_HKR_CRITICAL_ERROR_GPIO )
        CASE( SAFETY_SOC_MONITOR_L2_ERROR_TYPE )
        CASE( SAFETY_SOC_MONITOR_L3_ERROR_TYPE )
        CASE( SAFETY_SOC_SAFETY_AND_SECURITY )
        CASE( SAFETY_DEPTH_FRAME_TIMESTAMP )
        CASE( SAFETY_SMCU_PROCESSING_TIMESTAMP )
        CASE( SAFETY_PIPELINE_PROPAGATION_DELAY )
        CASE( SAFETY_SMCU_DEBUG_STATUS_BITMASK )
        CASE( SAFETY_SMCU_DEBUG_INFO_INTERNAL_STATE )
        CASE( SAFETY_SMCU_DEBUG_INFO_BIST_STATUS )
        CASE( SAFETY_NON_FUSA_GPIO_OUT )
        CASE( SAFETY_SMCU_HW_MONITOR_STATUS )
        CASE( SAFETY_SMCU_SW_MONITOR_STATUS )
        CASE( SAFETY_NON_FUSA_GPIO_IN )
        CASE( SAFETY_MB_FUSA_EVENT )
        CASE( SAFETY_MB_FUSA_ACTION )
        CASE( SAFETY_MB_STATUS )
        CASE( SAFETY_SMCU_LIVELINESS )
        CASE( SAFETY_SMCU_STATE )
        CASE( SAFETY_PRESET_ID )
        CASE( SENSOR_ANGLE_ROLL  )
        CASE( SENSOR_ANGLE_PITCH )
        CASE( DIAGNOSTIC_ZONE_MEDIAN_HEIGHT )
        CASE( FLOOR_DETECTION )
        CASE( DIAGNOSTIC_ZONE_FILL_RATE )
        CASE( DEPTH_FILL_RATE )
        CASE( DEPTH_STDEV )
        CASE( OCCUPANCY_GRID_ROWS)
        CASE( OCCUPANCY_GRID_COLUMNS )
        CASE( OCCUPANCY_CELL_SIZE )
        CASE( NUMBER_OF_3D_VERTICES )
        CASE( SAFETY_PRESET_ERROR_TYPE )
        CASE( SAFETY_PRESET_ERROR_PARAM_1 )
        CASE( SAFETY_PRESET_ERROR_PARAM_2 )
        CASE( DANGER_ZONE_POINT_0_X_CORD )
        CASE( DANGER_ZONE_POINT_0_Y_CORD )
        CASE( DANGER_ZONE_POINT_1_X_CORD )
        CASE( DANGER_ZONE_POINT_1_Y_CORD )
        CASE( DANGER_ZONE_POINT_2_X_CORD )
        CASE( DANGER_ZONE_POINT_2_Y_CORD )
        CASE( DANGER_ZONE_POINT_3_X_CORD )
        CASE( DANGER_ZONE_POINT_3_Y_CORD )
        CASE( WARNING_ZONE_POINT_0_X_CORD )
        CASE( WARNING_ZONE_POINT_0_Y_CORD )
        CASE( WARNING_ZONE_POINT_1_X_CORD )
        CASE( WARNING_ZONE_POINT_1_Y_CORD )
        CASE( WARNING_ZONE_POINT_2_X_CORD )
        CASE( WARNING_ZONE_POINT_2_Y_CORD )
        CASE( WARNING_ZONE_POINT_3_X_CORD )
        CASE( WARNING_ZONE_POINT_3_Y_CORD )
        CASE( DIAGNOSTIC_ZONE_POINT_0_X_CORD )
        CASE( DIAGNOSTIC_ZONE_POINT_0_Y_CORD )
        CASE( DIAGNOSTIC_ZONE_POINT_1_X_CORD )
        CASE( DIAGNOSTIC_ZONE_POINT_1_Y_CORD )
        CASE( DIAGNOSTIC_ZONE_POINT_2_X_CORD )
        CASE( DIAGNOSTIC_ZONE_POINT_2_Y_CORD )
        CASE( DIAGNOSTIC_ZONE_POINT_3_X_CORD )
        CASE( DIAGNOSTIC_ZONE_POINT_3_Y_CORD )
        CASE( EMBEDDED_FILTERS )
#undef CASE
            return arr;
    }();
    if( ! is_valid( value ) )
        return unknown_value_str;
    return str_array[value];
}

const char * get_string( rs2_timestamp_domain value )
{
#define CASE( X ) STRCASE( TIMESTAMP_DOMAIN, X )
    switch( value )
    {
    CASE( HARDWARE_CLOCK )
    CASE( SYSTEM_TIME )
    CASE( GLOBAL_TIME )
    default:
        assert( ! is_valid( value ) );
        return UNKNOWN_VALUE;
    }
#undef CASE
}

const char * get_string( rs2_calib_target_type value )
{
#define CASE( X ) STRCASE( CALIB_TARGET, X )
    switch( value )
    {
    CASE( RECT_GAUSSIAN_DOT_VERTICES )
    CASE( ROI_RECT_GAUSSIAN_DOT_VERTICES )
    CASE( POS_GAUSSIAN_DOT_VERTICES )
    default:
        assert( ! is_valid( value ) );
        return UNKNOWN_VALUE;
    }
#undef CASE
}

const char * get_string( rs2_notification_category value )
{
#define CASE( X ) STRCASE( NOTIFICATION_CATEGORY, X )
    switch( value )
    {
    CASE( FRAMES_TIMEOUT )
    CASE( FRAME_CORRUPTED )
    CASE( HARDWARE_ERROR )
    CASE( HARDWARE_EVENT )
    CASE( UNKNOWN_ERROR )
    CASE( FIRMWARE_UPDATE_RECOMMENDED )
    CASE( POSE_RELOCALIZATION )
    default:
        assert( ! is_valid( value ) );
        return UNKNOWN_VALUE;
    }
#undef CASE
}
const char * get_string( rs2_matchers value )
{
#define CASE( X ) STRCASE( MATCHER, X )
    switch( value )
    {
    CASE( DI )
    CASE( DI_C )
    CASE( DLR_C )
    CASE( DLR )
    CASE( DIC )
    CASE( DIC_C )
    CASE( DEFAULT )
    default:
        assert( ! is_valid( value ) );
        return UNKNOWN_VALUE;
    }

#undef CASE
}

const char * get_string( rs2_l500_visual_preset value )
{
#define CASE( X ) STRCASE( L500_VISUAL_PRESET, X )
    switch( value )
    {
    CASE( CUSTOM )
    CASE( DEFAULT )
    // CASE(NO_AMBIENT)
    case RS2_L500_VISUAL_PRESET_NO_AMBIENT:        return "No Ambient Light";
    // CASE(LOW_AMBIENT)
    case RS2_L500_VISUAL_PRESET_LOW_AMBIENT:       return "Low Ambient Light";
    CASE( MAX_RANGE )
    CASE( SHORT_RANGE )
    CASE( AUTOMATIC )
    default:
        assert( ! is_valid( value ) );
        return UNKNOWN_VALUE;
    }
#undef CASE
}

std::string const & get_string( rs2_option_type value )
{
    static auto str_array = []()
    {
        std::vector< std::string > arr( RS2_OPTION_TYPE_COUNT );
#define CASE( X ) STRARR( arr, OPTION_TYPE, X );
        CASE( INTEGER )
        CASE( FLOAT )
        CASE( STRING )
        CASE( BOOLEAN )
        CASE( RECT )
#undef CASE
            return arr;
    }();
    if( ! is_valid( value ) )
        return unknown_value_str;
    return str_array[value];
}


}  // namespace librealsense

const char * rs2_stream_to_string( rs2_stream stream ) { return librealsense::get_string( stream ); }
const char * rs2_format_to_string( rs2_format format ) { return librealsense::get_string( format ); }
const char * rs2_distortion_to_string( rs2_distortion distortion ) { return librealsense::get_string( distortion ); }

const char * rs2_option_to_string( rs2_option option )
{
    return librealsense::get_string( option ).c_str();
}

rs2_option rs2_option_from_string( char const * option_name )
{
    return option_name
        ? librealsense::options_registry::find_option_by_name( option_name )
        : RS2_OPTION_COUNT;
}

const char * rs2_option_type_to_string( rs2_option_type type ) { return librealsense::get_string( type ).c_str(); }
const char * rs2_camera_info_to_string( rs2_camera_info info ) { return librealsense::get_string( info ); }
const char * rs2_timestamp_domain_to_string( rs2_timestamp_domain info ) { return librealsense::get_string( info ); }
const char * rs2_notification_category_to_string( rs2_notification_category category ) { return librealsense::get_string( category ); }
const char * rs2_calib_target_type_to_string( rs2_calib_target_type type ) { return librealsense::get_string( type ); }
const char * rs2_sr300_visual_preset_to_string( rs2_sr300_visual_preset preset ) { return librealsense::get_string( preset ); }
const char * rs2_log_severity_to_string( rs2_log_severity severity ) { return librealsense::get_string( severity ); }
const char * rs2_exception_type_to_string( rs2_exception_type type ) { return librealsense::get_string( type ); }
const char * rs2_playback_status_to_string( rs2_playback_status status ) { return librealsense::get_string( status ); }
const char * rs2_extension_type_to_string( rs2_extension type ) { return librealsense::get_string( type ); }
const char * rs2_matchers_to_string( rs2_matchers matcher ) { return librealsense::get_string( matcher ); }
const char * rs2_frame_metadata_to_string( rs2_frame_metadata_value metadata ) { return librealsense::get_string( metadata ).c_str(); }
const char * rs2_extension_to_string( rs2_extension type ) { return rs2_extension_type_to_string( type ); }
const char * rs2_frame_metadata_value_to_string( rs2_frame_metadata_value metadata ) { return rs2_frame_metadata_to_string( metadata ); }
const char * rs2_l500_visual_preset_to_string( rs2_l500_visual_preset preset ) { return librealsense::get_string( preset ); }
const char * rs2_sensor_mode_to_string( rs2_sensor_mode mode ) { return librealsense::get_string( mode ); }
const char * rs2_ambient_light_to_string( rs2_ambient_light ambient ) { return librealsense::get_string( ambient ); }
const char * rs2_digital_gain_to_string( rs2_digital_gain gain ) { return librealsense::get_string( gain ); }
const char * rs2_cah_trigger_to_string( int mode ) { return "DEPRECATED as of 2.46"; }
const char * rs2_calibration_type_to_string( rs2_calibration_type type ) { return librealsense::get_string( type ); }
const char * rs2_calibration_status_to_string( rs2_calibration_status status ) { return librealsense::get_string( status ); }
const char * rs2_host_perf_mode_to_string( rs2_host_perf_mode mode ) { return librealsense::get_string( mode ); }
const char * rs2_emitter_frequency_mode_to_string( rs2_emitter_frequency_mode mode ) { return librealsense::get_string( mode ); }
const char * rs2_depth_auto_exposure_mode_to_string( rs2_depth_auto_exposure_mode mode ) { return librealsense::get_string( mode ); }
const char * rs2_safety_mode_to_string( rs2_safety_mode mode ) { return librealsense::get_string( mode ); }
const char * rs2_d500_intercam_sync_mode_to_string( rs2_d500_intercam_sync_mode mode ) { return librealsense::get_string( mode ); }
const char * rs2_point_cloud_label_to_string(rs2_point_cloud_label label) { return librealsense::get_string(label); }
const char * rs2_calib_location_to_string(rs2_calib_location calib_location) { return librealsense::get_string(calib_location); }
const char * rs2_embedded_filter_type_to_string(rs2_embedded_filter_type embedded_filter_type) { return librealsense::get_string(embedded_filter_type); }
const char * rs2_gyro_sensitivity_to_string( rs2_gyro_sensitivity mode ){return librealsense::get_string( mode );}
const char * rs2_eth_link_priority_to_string( rs2_eth_link_priority priority ){return librealsense::get_string( priority );}
