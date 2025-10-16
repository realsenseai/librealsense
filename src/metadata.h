// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 Intel Corporation. All Rights Reserved.
#pragma once

#include "float3.h"
#include "platform/hid-device.h"
#include "platform/uvc-device.h"


// Metadata attributes provided by RS4xx Depth Cameras


#define REGISTER_MD_TYPE(A,B)\
    template<>\
    struct md_type_trait<A>\
    {\
        static const md_type type = B;\
    };


namespace librealsense
{
    template<class T>
    struct md_type_trait;

    // Metadata tables version
    const int META_DATA_INTEL_DEPTH_CONTROL_VERSION   = 0x1;
    // Intel Configuration:
    // version 2: gpioInputData added to md_configuration (with its flag)
    // version 3: sub_preset_info added to md_configuration (with its flag)
    const int META_DATA_INTEL_CONFIGURATION_VERSION   = 0x3; 
    const int META_DATA_INTEL_STAT_VERSION            = 0x1;
    const int META_DATA_INTEL_CAPTURE_TIMING_VERSION  = 0x1;

    /**\brief md_mode - enumerates the types of metadata modes(structs) supported */
    enum class md_type : uint32_t
    {
        META_DATA_INTEL_DEPTH_CONTROL_ID        = 0x80000000,
        META_DATA_INTEL_CAPTURE_TIMING_ID       = 0x80000001,
        META_DATA_INTEL_CONFIGURATION_ID        = 0x80000002,
        META_DATA_INTEL_STAT_ID                 = 0x80000003,
        META_DATA_INTEL_FISH_EYE_CONTROL_ID     = 0x80000004,
        META_DATA_INTEL_RGB_CONTROL_ID          = 0x80000005,
        META_DATA_INTEl_FE_FOV_MODEL_ID         = 0x80000006,
        META_DATA_INTEl_FE_CAMERA_EXTRINSICS_ID = 0x80000007,
        META_DATA_CAPTURE_STATS_ID              = 0x00000003,
        META_DATA_CAMERA_EXTRINSICS_ID          = 0x00000004,
        META_DATA_CAMERA_INTRINSICS_ID          = 0x00000005,
        META_DATA_INTEL_L500_CAPTURE_TIMING_ID  = 0x80000010,
        META_DATA_INTEL_L500_DEPTH_CONTROL_ID   = 0x80000012,
        META_DATA_CAMERA_DEBUG_ID               = 0x800000FF,
        META_DATA_HID_IMU_REPORT_ID             = 0x80001001,
        META_DATA_HID_CUSTOM_TEMP_REPORT_ID     = 0x80001002,
        META_DATA_MIPI_INTEL_DEPTH_ID           = 0x80010000,
        META_DATA_MIPI_INTEL_RGB_ID             = 0x80010001,
    };

    static const std::map<md_type, std::string> md_type_desc =
    {
        { md_type::META_DATA_INTEL_DEPTH_CONTROL_ID,        "Intel Depth Control"},
        { md_type::META_DATA_INTEL_CAPTURE_TIMING_ID,       "Intel Capture timing"},
        { md_type::META_DATA_INTEL_CONFIGURATION_ID,        "Intel Configuration"},
        { md_type::META_DATA_INTEL_STAT_ID,                 "Intel Statistics"},
        { md_type::META_DATA_INTEL_FISH_EYE_CONTROL_ID,     "Intel Fisheye Control"},
        { md_type::META_DATA_INTEL_RGB_CONTROL_ID,          "Intel RGB Control"},
        { md_type::META_DATA_INTEl_FE_FOV_MODEL_ID,         "Intel Fisheye FOV Model"},
        { md_type::META_DATA_CAPTURE_STATS_ID,              "Capture Statistics"},
        { md_type::META_DATA_CAMERA_EXTRINSICS_ID,          "Camera Extrinsic"},
        { md_type::META_DATA_CAMERA_INTRINSICS_ID,          "Camera Intrinsic"},
        { md_type::META_DATA_CAMERA_DEBUG_ID,               "Camera Debug"},
        { md_type::META_DATA_INTEL_L500_CAPTURE_TIMING_ID,  "Intel Capture timing"},
        { md_type::META_DATA_INTEL_L500_DEPTH_CONTROL_ID,   "Intel Depth Control"},
        { md_type::META_DATA_HID_IMU_REPORT_ID,             "HID IMU Report"},
        { md_type::META_DATA_HID_CUSTOM_TEMP_REPORT_ID,     "HID Custom Temperature Report"},
        { md_type::META_DATA_MIPI_INTEL_DEPTH_ID,           "Intel Mipi Depth Control"},
        { md_type::META_DATA_MIPI_INTEL_RGB_ID,             "Intel Mipi RGB Control"},
    };

    /**\brief md_capture_timing_attributes - enumerate the bit offset to check
     *  a specific attribute of md_capture_timing struct for validity.
     *  The enumeration includes up to 32 attributes, according to the size
     *  of flags parameter in all the defined structs */
    enum class md_capture_timing_attributes : uint32_t
    {
        frame_counter_attribute         = (1u << 0),
        sensor_timestamp_attribute      = (1u << 1),
        readout_time_attribute          = (1u << 2),
        exposure_attribute              = (1u << 3),
        frame_interval_attribute        = (1u << 4),
        pipe_latency_attribute          = (1u << 5),
    };

    /**\brief md_capture_stat_attributes - bit mask to find enabled attributes
     *  in md_capture_stats */
    enum class md_capture_stat_attributes : uint32_t
    {
        exposure_time_attribute         = (1u << 0),
        exposure_compensation_attribute = (1u << 1),
        iso_speed_attribute             = (1u << 2),
        focus_state_attribute           = (1u << 3),
        lens_posiiton_attribute         = (1u << 4),
        white_balance_attribute         = (1u << 5),
        flash_attribute                 = (1u << 6),
        flash_power_attribute           = (1u << 7),
        zoom_factor_attribute           = (1u << 8),
        scene_mode_attribute            = (1u << 9),
        sensor_framerate_attribute      = (1u << 10),
        hw_timestamp_attribute          = (1u << 11)
    };

    /**\brief md_depth_control_attributes - bit mask to find active attributes,
     *  md_depth_control struct */
    enum class md_depth_control_attributes : uint32_t
    {
        gain_attribute                  = (1u << 0),
        exposure_attribute              = (1u << 1),
        laser_pwr_attribute             = (1u << 2),
        ae_mode_attribute               = (1u << 3),
        exposure_priority_attribute     = (1u << 4),
        roi_attribute                   = (1u << 5),
        preset_attribute                = (1u << 6),
        emitter_mode_attribute          = (1u << 7),
        led_power_attribute             = (1u << 8)
    };
    
    /**\brief md_mipi_depth_control_attributes - bit mask to find active attributes,
     *  md_mipi_depth_control struct */
    enum class md_mipi_depth_control_attributes : uint32_t
    {
        hw_timestamp_attribute          = (1u << 0),
        optical_timestamp_attribute     = (1u << 1),
        exposure_time_attribute         = (1u << 2),
        manual_exposure_attribute       = (1u << 3),
        laser_power_attribute           = (1u << 4),
        trigger_attribute               = (1u << 5),
        projector_mode_attribute        = (1u << 6),
        preset_attribute                = (1u << 7),
        manual_gain_attribute           = (1u << 8),
        auto_exposure_mode_attribute    = (1u << 9),
        input_width_attribute           = (1u << 10),
        input_height_attribute          = (1u << 11),
        sub_preset_info_attribute       = (1u << 12),
        calibration_info_attribute      = (1u << 13), // shall be used to stire the frame counter in calibration routines
        crc_attribute                   = (1u << 14)
    };

    /**\brief md_mipi_rgb_control_attributes - bit mask to find active attributes,
     *  md_mipi_rgb_control struct */
    enum class md_mipi_rgb_control_attributes : uint32_t
    {
        hw_timestamp_attribute            = (1u << 0),
        brightness_attribute              = (1u << 1),
        contrast_attribute                = (1u << 2),
        saturation_attribute              = (1u << 3),
        sharpness_attribute               = (1u << 4),
        auto_white_balance_temp_attribute = (1u << 5),
        gamma_attribute                   = (1u << 6),
        manual_exposure_attribute         = (1u << 7),
        manual_white_balance_attribute    = (1u << 8),
        auto_exposure_mode_attribute      = (1u << 9),
        gain_attribute                    = (1u << 10),
        backlight_compensation_attribute  = (1u << 11),
        hue_attribute                     = (1u << 12),
        power_line_frequency_attribute    = (1u << 13),
        low_light_compensation_attribute  = (1u << 14),
        input_width_attribute             = (1u << 15),
        input_height_attribute            = (1u << 16),
        crc_attribute                     = (1u << 17)
    };

    /**\brief md_depth_control_attributes - bit mask to find active attributes,
     *  md_depth_control struct */
    enum class md_l500_depth_control_attributes : uint32_t
    {
        laser_power                     = (1u << 0),
        preset_id                       = (1u << 1),
        laser_power_mode                = (1u << 2),
    };
    /**\brief md_fisheye_control_attributes - bit mask to find active attributes,
     *  md_fisheye_control struct */
    enum class md_fisheye_control_attributes : uint32_t
    {
        gain_attribute                  = (1u << 0),
        exposure_attribute              = (1u << 1),
    };

    /**\brief md_rgb_control_attributes - bit mask to find active attributes,
    *  md_rgb_control struct */
    enum class md_rgb_control_attributes : uint32_t
    {
        brightness_attribute            = (1u << 0),
        contrast_attribute              = (1u << 1),
        saturation_attribute            = (1u << 2),
        sharpness_attribute             = (1u << 3),
        ae_mode_attribute               = (1u << 4),
        awb_temp_attribute              = (1u << 5),
        gain_attribute                  = (1u << 6),
        backlight_comp_attribute        = (1u << 7),
        gamma_attribute                 = (1u << 8),
        hue_attribute                   = (1u << 9),
        manual_exp_attribute            = (1u << 10),
        manual_wb_attribute             = (1u << 11),
        power_line_frequency_attribute  = (1u << 12),
        low_light_comp_attribute        = (1u << 13),
    };

    /**\brief md_configuration_attributes - bit mask to find active attributes,
     *  md_configuration struct */
    enum class md_configuration_attributes : uint32_t
    {
        hw_type_attribute               = (1u << 0),
        sku_id_attribute                = (1u << 1),
        cookie_attribute                = (1u << 2),
        format_attribute                = (1u << 3),
        width_attribute                 = (1u << 4),
        height_attribute                = (1u << 5),
        fps_attribute                   = (1u << 6),
        trigger_attribute               = (1u << 7),
        calibration_count_attribute     = (1u << 8),
        gpio_input_data_attribute       = (1u << 9),
        sub_preset_info_attribute       = (1u << 10)
    };

    /**\brief md_stat_attributes - bit mask to find active attributes,
     *  md_stat struct */
    enum class md_stat_attributes : uint32_t
    {
        left_sum_attribute              = (1u << 0),
        left_dark_count_attribute       = (1u << 1),
        left_bright_count_attribute     = (1u << 2),
        right_sum_attribute             = (1u << 3),
        right_dark_count_attribute      = (1u << 4),
        right_bright_count_attribute    = (1u << 5),
        red_frame_count_attribute       = (1u << 6),
        left_red_sum_attribute          = (1u << 7),
        left_greeen1_attribute          = (1u << 8),
        left_greeen2_attribute          = (1u << 9),
        left_blue_sum_attribute         = (1u << 10),
        right_red_sum_attribute         = (1u << 11),
        right_greeen1_attribute         = (1u << 12),
        right_greeen2_attribute         = (1u << 13),
        right_blue_sum_attribute        = (1u << 14),
    };

    /**\brief md_hid_imu_attributes - bit mask to designate the enabled attributed,
*  md_imu_report struct */
    enum md_hid_report_type : uint8_t
    {
        hid_report_unknown,
        hid_report_imu,
        hid_report_custom_temperature,
        hid_report_max,
    };

    /**\brief md_hid_imu_attributes - bit mask to designate the enabled attributed,
 *  md_imu_report struct */
    enum class md_hid_imu_attributes : uint8_t
    {
        custom_timestamp_attirbute  = (1u << 0),
        imu_counter_attribute       = (1u << 1),
        usb_counter_attribute       = (1u << 2)
    };

    inline md_hid_imu_attributes operator |(md_hid_imu_attributes l, md_hid_imu_attributes r)
    {
        return static_cast<md_hid_imu_attributes>(static_cast<uint8_t>(l) | static_cast<uint8_t>(r));
    }

    inline md_hid_imu_attributes operator |=(md_hid_imu_attributes l, md_hid_imu_attributes r)
    {
        return l = l | r;
    }

    /**\brief md_hid_imu_attributes - bit mask to designate the enabled attributed,
*  md_imu_report struct */
    enum class md_hid_custom_temp_attributes : uint8_t
    {
        source_id_attirbute         = (1u << 0),
        custom_timestamp_attirbute  = (1u << 1),
        imu_counter_attribute       = (1u << 2),
        usb_counter_attribute       = (1u << 3)
    };

#pragma pack(push, 1)
    /**\brief md_header - metadata header is a integral part of all rs4XX metadata objects */
    struct md_header
    {
        md_type     md_type_id;         // The type of the metadata struct
        uint32_t    md_size;            // Actual size of metadata struct without header
    };

    /**\brief md_mipi_depth_mode - properties associated with sensor configuration
     *  during video streaming. Corresponds to FW STMetaDataExtMipiDepthIR object*/
    struct md_mipi_depth_mode
    {
        md_header   header;
        uint8_t     version;  // maybe need to replace by uint8_t for version and 3 others for reserved
        uint16_t    calib_info;
        uint8_t     reserved;
        uint32_t    flags;              // Bit array to specify attributes that are valid
        uint32_t    hw_timestamp;
        uint32_t    optical_timestamp;   //In microsecond unit
        uint32_t    exposure_time;      //The exposure time in microsecond unit
        uint32_t    manual_exposure;
        uint16_t    laser_power;
        uint16_t    trigger;
        uint8_t     projector_mode;
        uint8_t     preset;
        uint8_t     manual_gain;
        uint8_t     auto_exposure_mode;
        uint16_t    input_width;
        uint16_t    input_height;
        uint32_t    sub_preset_info;
        uint32_t    crc;
    };
    constexpr uint8_t md_mipi_depth_mode_size = sizeof(md_mipi_depth_mode);
    REGISTER_MD_TYPE(md_mipi_depth_mode, md_type::META_DATA_MIPI_INTEL_DEPTH_ID)

    /**\brief md_mipi_rgb_mode - properties associated with sensor configuration
     *  during video streaming. Corresponds to FW STMetaDataExtMipiRgb object*/
    struct md_mipi_rgb_mode
    {
        md_header   header;
        uint32_t    version;  // maybe need to replace by uint8_t for version and 3 others for reserved
        uint32_t    flags;              // Bit array to specify attributes that are valid
        uint32_t    hw_timestamp;
        uint8_t     brightness;
        uint8_t     contrast;
        uint8_t     saturation;
        uint8_t     sharpness;
        uint16_t    auto_white_balance_temp;
        uint16_t    gamma;
        uint16_t    manual_exposure;
        uint16_t    manual_white_balance;
        uint8_t     auto_exposure_mode;
        uint8_t     gain;
        uint8_t     backlight_compensation;
        uint8_t     hue;
        uint8_t     power_line_frequency;
        uint8_t     low_light_compensation;
        uint16_t    input_width;
        uint16_t    input_height;
        uint8_t     reserved[6];
        uint32_t    crc;
    };
    constexpr uint8_t md_mipi_rgb_mode_size = sizeof(md_mipi_rgb_mode);
    REGISTER_MD_TYPE(md_mipi_rgb_mode, md_type::META_DATA_MIPI_INTEL_RGB_ID)

    /**\brief md_mipi_capture_timing - properties associated with sensor configuration
     *  during video streaming. Corresponds to FW STMetaDataIntelCaptureTiming object*/
    struct md_capture_timing
    {
        md_header   header;
        uint32_t    version;
        uint32_t    flags;              // Bit array to specify attributes that are valid
        uint32_t    frame_counter;
        uint32_t    sensor_timestamp;   //In microsecond unit
        uint32_t    readout_time;       //The readout time in microsecond unit
        uint32_t    exposure_time;      //The exposure time in microsecond unit
        uint32_t    frame_interval;     //The frame interval in microsecond unit
        uint32_t    pipe_latency;       //The latency between start of frame to frame ready in USB buffer
    };
    constexpr uint8_t md_capture_timing_size = sizeof(md_capture_timing);

    REGISTER_MD_TYPE(md_capture_timing, md_type::META_DATA_INTEL_CAPTURE_TIMING_ID)

    struct l500_md_capture_timing
    {
        md_header   header;
        uint32_t    version;
        uint32_t    flags;              // Bit array to specify attributes that are valid
        uint32_t    frame_counter;
        uint32_t    sensor_timestamp;   //In microsecond unit
        uint32_t    readout_time;       //The readout time in microsecond unit
        uint32_t    exposure_time;      //The exposure time in microsecond unit
        uint32_t    frame_interval;     //The frame interval in microsecond unit
        uint32_t    pipe_latency;       //The latency between start of frame to frame ready in USB buffer
    };
    REGISTER_MD_TYPE(l500_md_capture_timing, md_type::META_DATA_INTEL_L500_CAPTURE_TIMING_ID)

        /**\brief md_capture_stats - properties associated with optical sensor
         *  during video streaming. Corresponds to FW STMetaDataCaptureStats object*/
    struct md_capture_stats
    {
        md_header   header;
        uint32_t    flags;
        uint32_t    hw_timestamp;
        uint64_t    exposure_time;
        uint64_t    exposure_compensation_flags;
        int32_t     exposure_compensation_value;
        uint32_t    iso_speed;
        uint32_t    focus_state;
        uint32_t    lens_position;      // a.k.a Focus
        uint32_t    white_balance;
        uint32_t    flash;
        uint32_t    flash_power;
        uint32_t    zoom_factor;
        uint64_t    scene_mode;
        uint64_t    sensor_framerate;
    };

    REGISTER_MD_TYPE(md_capture_stats, md_type::META_DATA_CAPTURE_STATS_ID)

    /**\brief md_depth_control - depth data-related parameters.
     *  Corresponds to FW's STMetaDataIntelDepthControl object*/
    struct md_depth_control
    {
        md_header   header;
        uint32_t    version;
        uint32_t    flags;
        uint32_t    manual_gain;        //  Manual gain value
        uint32_t    manual_exposure;    //  Manual exposure
        uint32_t    laser_power;       //  Laser power value
        uint32_t    auto_exposure_mode; //  AE mode. When active handles both Exposure and Gain
        uint32_t    exposure_priority;
        uint32_t    exposure_roi_left;
        uint32_t    exposure_roi_right;
        uint32_t    exposure_roi_top;
        uint32_t    exposure_roi_bottom;
        uint32_t    preset;
        uint8_t     emitterMode;
        uint8_t     reserved;
        uint16_t    ledPower;
    };

    REGISTER_MD_TYPE(md_depth_control, md_type::META_DATA_INTEL_DEPTH_CONTROL_ID)

    /**\brief md_depth_control - depth data-related parameters.
     *  Corresponds to FW's STMetaDataIntelDepthControl object*/
        struct md_l500_depth_control
    {
        md_header   header;
        uint32_t    version;
        uint32_t    flags;
        uint32_t    laser_power;        //value between 1 to 12
        uint32_t    preset_id;
        uint32_t    laser_power_mode;     //Auto or Manual laser power
    };

    REGISTER_MD_TYPE(md_l500_depth_control, md_type::META_DATA_INTEL_L500_DEPTH_CONTROL_ID)

    /**\brief md_fisheye_control - fisheye-related parameters.
     *  Corresponds to FW's STMetaDataIntelFishEyeControl object*/
    struct md_fisheye_control
    {
        md_header   header;
        uint32_t    version;
        uint32_t    flags;
        uint32_t    manual_gain;        //  Manual gain value
        uint32_t    manual_exposure;    //  Manual exposure
    };

    REGISTER_MD_TYPE(md_fisheye_control, md_type::META_DATA_INTEL_FISH_EYE_CONTROL_ID)

    /**\brief md_rgb_control - Realtec RGB sensor attributes. */
    struct md_rgb_control
    {
        md_header   header;
        uint32_t    version;
        uint32_t    flags;
        uint32_t    brightness;
        uint32_t    contrast;
        uint32_t    saturation;
        uint32_t    sharpness;
        uint32_t    ae_mode;
        uint32_t    awb_temp;
        uint32_t    gain;
        uint32_t    backlight_comp;
        uint32_t    gamma;
        uint32_t    hue;
        uint32_t    manual_exp;
        uint32_t    manual_wb;
        uint32_t    power_line_frequency;
        uint32_t    low_light_comp;
    };

    REGISTER_MD_TYPE(md_rgb_control, md_type::META_DATA_INTEL_RGB_CONTROL_ID)

    /*struct md_sub_preset_info_fields
    {
        uint32_t  id : 4;
        uint32_t  num_of_items : 8;
        uint32_t  item_index : 8;
        uint32_t  iteration : 6;
        uint32_t  item_iteration : 6;
    };

    union md_sub_preset_info
    {
        uint32_t value;
        md_sub_preset_info_fields fields;
    };*/

    /**\brief md_configuration - device/stream configuration.
     *  Corresponds to FW's STMetaDataIntelConfiguration object*/
    struct md_configuration
    {
        md_header           header;
        uint32_t            version;
        uint32_t            flags;
        uint8_t             hw_type;    //  IVCAM2 , RS4xx, etc'
        uint8_t             sku_id;
        uint32_t            cookie;     /*  Place Holder enable FW to bundle cookie
                                            with control state and configuration.*/
        uint16_t            format;
        uint16_t            width;      //  Requested resolution
        uint16_t            height;
        uint16_t            fps;        //  Requested FPS
        uint16_t            trigger;    /*  Byte <0>  0 free-running
                                                      1 in sync
                                                      2 external trigger (depth only)
                                            Byte <1>  configured delay (depth only)*/
        uint16_t            calibration_count;
        uint8_t             gpioInputData;
        uint32_t            sub_preset_info;
        uint8_t             reserved[1];

        typedef enum sub_preset_bit_mask
        {
            SUB_PRESET_BIT_MASK_ID                      = 0xF,
            SUB_PRESET_BIT_MASK_SEQUENCE_SIZE           = 0xFF0,
            SUB_PRESET_BIT_MASK_SEQUENCE_ID             = 0xFF000,
            SUB_PRESET_BIT_MASK_SEQUENCE_ITERATION      = 0x3F00000,
            SUB_PRESET_BIT_MASK_SEQUENCE_ITEM_ITERATION = 0xFC000000
        }sub_preset_bit_mask;

        typedef enum sub_preset_bit_offset
        {
            SUB_PRESET_BIT_OFFSET_ID               = 0,
            SUB_PRESET_BIT_OFFSET_SEQUENCE_SIZE    = 4,
            SUB_PRESET_BIT_OFFSET_SEQUENCE_ID      = 12,
            SUB_PRESET_BIT_OFFSET_ITERATION        = 20,
            SUB_PRESET_BIT_OFFSET_ITEM_ITERATION   = 26
        }sub_preset_bit_offset;
    };

    REGISTER_MD_TYPE(md_configuration, md_type::META_DATA_INTEL_CONFIGURATION_ID)

    /**\brief md_intel_stat
     *  Corresponds to FW's STMetaDataIntelStat object*/
    struct md_intel_stat
    {
        md_header   header;
        uint32_t    version;
        uint32_t    flags;
        uint32_t    exposure_left_sum;
        uint32_t    exposure_left_dark_count;
        uint32_t    exposure_right_sum;
        uint32_t    exposure_right_dark_count;
        uint32_t    exposure_right_bright_count;
        uint32_t    rec_frame_count;
        uint32_t    left_red_sum;
        uint32_t    left_green1_sum;
        uint32_t    left_green2_sum;
        uint32_t    left_blue_sum;
        uint32_t    right_red_sum;
        uint32_t    right_green1_sum;
        uint32_t    right_green2_sum;
        uint32_t    right_blue_sum;
        uint32_t    reserved;
    };

    REGISTER_MD_TYPE(md_intel_stat, md_type::META_DATA_INTEL_STAT_ID)

    struct md_intrinsic_pinhole_cam_model
    {
        float2      focal_length;
        float2      principal_point;
    };

    /**\brief md_intrinsic_distortion_model - Distortion coefficients
     * of sensor instrinsic */
    struct md_intrinsic_distortion_model
    {
        float       radial_k1;
        float       radial_k2;
        float       radial_k3;
        float       tangential_p1;
        float       tangential_p2;
    };

    /**\brief md_pinhole_cam_intrinsic_model - Pinhole sensor's characteristics*/
    struct md_pinhole_cam_intrinsic_model
    {
        uint32_t                        width;
        uint32_t                        height;
        md_intrinsic_pinhole_cam_model  radial_k1;
        md_intrinsic_distortion_model   radial_k2;
    };

    const uint8_t INTRINSICS_MODEL_COUNT = 1;

    struct md_pinhole_camera_intrinsics
    {
        uint32_t                        intrinsic_model_count;
        md_pinhole_cam_intrinsic_model  intrinsic_models[INTRINSICS_MODEL_COUNT];
    };

    struct md_camera_intrinsic
    {
        md_header                       header;
        md_pinhole_camera_intrinsics    pinhole_cam_intrinsics;
    };

    REGISTER_MD_TYPE(md_camera_intrinsic, md_type::META_DATA_CAMERA_INTRINSICS_ID)

    const uint8_t UVC_GUID_SIZE = 16;
    struct md_extrinsic_calibrated_transform
    {
        uint8_t                         calibration_id[UVC_GUID_SIZE];
        float3                          position;
        float4                          orientation;    // quaternion representation
    };

    const uint8_t TRANSFORM_COUNT = 1;      // TODO requires explanation
    struct mf_camera_extrinsic
    {
        uint32_t                            transform_count;
        md_extrinsic_calibrated_transform   calibrated_transform[TRANSFORM_COUNT];
    };

    struct md_camera_extrinsic
    {
        md_header               header;
        mf_camera_extrinsic     camera_extrinsic;
    };

    REGISTER_MD_TYPE(md_camera_extrinsic, md_type::META_DATA_CAMERA_EXTRINSICS_ID)

    struct md_depth_y_normal_mode
    {
        md_capture_timing       intel_capture_timing;
        md_capture_stats        intel_capture_stats;
        md_depth_control        intel_depth_control;
        md_configuration        intel_configuration;
    };

    struct md_l500_depth
    {
        md_capture_timing       intel_capture_timing;
        md_capture_stats        intel_capture_stats;
        md_l500_depth_control   intel_depth_control;
        md_configuration        intel_configuration;
    };

    struct md_fisheye_normal_mode
    {
        md_capture_timing       intel_capture_timing;
        md_capture_stats        intel_capture_stats;
        md_fisheye_control      intel_fisheye_control;
        md_configuration        intel_configuration;
    };

    struct md_rgb_normal_mode
    {
        md_capture_timing       intel_capture_timing;
        md_capture_stats        intel_capture_stats;
        md_rgb_control          intel_rgb_control;
        md_configuration        intel_configuration;
    };

    struct md_calibration_mode
    {
        md_capture_timing       intel_capture_timing;
        md_camera_extrinsic     intel_camera_extrinsic;
        md_camera_intrinsic     intel_camera_intrinsic;
    };

    struct md_stat_mode
    {
        md_capture_timing       intel_capture_timing;
        md_capture_stats        intel_capture_stats;
        md_intel_stat           metadata_intel_stat;
        md_configuration        intel_configuration;
    };

    union md_depth_mode
    {
        md_depth_y_normal_mode  depth_y_mode;
        md_calibration_mode     calib_mode;
        md_stat_mode            stat_mode;
    };

    union md_fisheye_mode
    {
        md_fisheye_normal_mode  fisheye_mode;
        md_calibration_mode     calib_mode;
    };

    union md_rgb_mode
    {
        md_rgb_normal_mode      rgb_mode;
        md_calibration_mode     calib_mode;
    };

    /**\brief metadata_raw - aggrevative structure that represents all the possible
     * metadata struct types to be handled */
    union md_modes
    {
        md_depth_mode           depth_mode;
        md_fisheye_mode         fisheye_mode;
        md_rgb_mode             rgb_mode;
    };

    /**\brief metadata_raw - metadata structure
     *  layout as transmitted and received by backend */
    struct metadata_raw
    {
        platform::uvc_header    header;
        md_modes                mode;
    };
    constexpr int metadata_raw_mode_offset = sizeof(metadata_raw::header);

    /**\brief metadata_mipi_raw - metadata structure
     *  layout as transmitted and received by backend */
    struct metadata_mipi_depth_raw
    {
        platform::uvc_header_mipi    header;
        md_mipi_depth_mode           depth_mode;

        inline bool capture_valid() const
        {
            return ((header.header.length > platform::uvc_header_mipi_size) &&
                (depth_mode.header.md_size == md_mipi_depth_mode_size) &&
                (depth_mode.header.md_type_id == md_type::META_DATA_MIPI_INTEL_DEPTH_ID));
        }
    };
    REGISTER_MD_TYPE(metadata_mipi_depth_raw, md_type::META_DATA_MIPI_INTEL_DEPTH_ID)

    struct metadata_mipi_rgb_raw
    {
        platform::uvc_header_mipi    header;
        md_mipi_rgb_mode             rgb_mode;

        inline bool capture_valid() const
        {
            return ((header.header.length > platform::uvc_header_mipi_size) &&
                (rgb_mode.header.md_size == md_mipi_rgb_mode_size) &&
                (rgb_mode.header.md_type_id == md_type::META_DATA_MIPI_INTEL_RGB_ID));
        }
    };
    REGISTER_MD_TYPE(metadata_mipi_rgb_raw, md_type::META_DATA_MIPI_INTEL_RGB_ID)

    constexpr uint8_t metadata_raw_size = sizeof(metadata_raw);

    /**\brief metadata_intel_basic - a subset of the full metadata required to provide
     *    the essential sensor attributes only */
    struct metadata_intel_basic
    {
        platform::uvc_header   header;
        md_capture_timing payload;

        inline bool capture_valid() const
        {
            return ((header.length > platform::uvc_header_size) &&
                (payload.header.md_size == md_capture_timing_size) &&
                (payload.header.md_type_id == md_type::META_DATA_INTEL_CAPTURE_TIMING_ID));
        }
    };

    struct md_imu_report
    {
        md_header   header;
        uint8_t     flags;              // Bit array to specify attributes that are valid (limited to 7 fields)
        uint64_t    custom_timestamp;   // HW Timestamp
        uint8_t     imu_counter;        // IMU internal counter
        uint8_t     usb_counter;        // USB-layer internal counter
    };

    REGISTER_MD_TYPE(md_imu_report, md_type::META_DATA_HID_IMU_REPORT_ID)

    constexpr uint8_t metadata_imu_report_size = sizeof(md_imu_report);

    struct md_custom_tmp_report
    {
        md_header   header;
        uint8_t     flags;              // Bit array to specify attributes that are valid (limited to 7 fields)
        uint64_t    custom_timestamp;   // HW Timestamp
        uint8_t     imu_counter;        // IMU internal counter
        uint8_t     usb_counter;        // USB-layer internal counter
        uint8_t     source_id;
    };

    REGISTER_MD_TYPE(md_custom_tmp_report, md_type::META_DATA_HID_CUSTOM_TEMP_REPORT_ID)

    /**\brief md_hid_types - aggrevative structure that represents the supported HID
 * metadata struct types to be handled */
    union md_hid_report
    {
        md_imu_report           imu_report;
        md_custom_tmp_report    temperature_report;
    };

#pragma pack( push, 1 )
    struct hid_header
    {
        uint8_t length;       // HID report total size. Limited to 255
        uint8_t report_type;  // Curently supported: IMU/Custom Temperature
        uint64_t timestamp;   // Driver-produced/FW-based timestamp. Note that currently only the lower 32bit are used
    };
#pragma pack( pop )

    constexpr uint8_t hid_header_size = sizeof( hid_header );

    /**\brief metadata_hid_raw - HID metadata structure
 *  layout populated by backend */
    struct metadata_hid_raw
    {
        hid_header header;
        md_hid_report report_type;
    };

    constexpr uint8_t metadata_hid_raw_size = sizeof(metadata_hid_raw);

#pragma pack(pop)
}
