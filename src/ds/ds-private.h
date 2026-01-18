// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2022 RealSense, Inc. All Rights Reserved.

#pragma once

#include <src/pose.h>
#include "fw-update/fw-update-unsigned.h"
#include <src/firmware-version.h>
#include <rsutils/string/from.h>
#include <rsutils/number/crc32.h>

#include <map>
#include <iomanip>
#include <string>

//#define DEBUG_THERMAL_LOOP
#ifdef DEBUG_THERMAL_LOOP
#define LOG_DEBUG_THERMAL_LOOP(...)   do { CLOG(WARNING   ,LIBREALSENSE_ELPP_ID) << __VA_ARGS__; } while(false)
#else
#define LOG_DEBUG_THERMAL_LOOP(...)
#endif //DEBUG_THERMAL_LOOP


namespace librealsense
{
    class context;
    typedef float float_4[4];

    template<typename T>
    constexpr size_t arr_size( T const & ) { return 1; }

    template<typename T, size_t sz>
    constexpr size_t arr_size( T( &arr )[sz] )
    {
        return sz * arr_size( arr[0] );
    }

    template<typename T>
    std::string array2str( T & data )
    {
        std::stringstream ss;
        for( auto i = 0; i < arr_size( data ); i++ )
            ss << " [" << i << "] = " << data[i] << "\t";
        return ss.str();
    }

    namespace ds
    {
        // DS5 depth XU identifiers
        const uint8_t DS5_HWMONITOR                         = 1;
        const uint8_t DS5_DEPTH_EMITTER_ENABLED             = 2;
        const uint8_t DS5_EXPOSURE                          = 3;
        const uint8_t DS5_LASER_POWER                       = 4;
        const uint8_t DS5_HARDWARE_PRESET                   = 6;
        const uint8_t DS5_ERROR_REPORTING                   = 7;
        const uint8_t DS5_EXT_TRIGGER                       = 8;
        const uint8_t DS5_ASIC_AND_PROJECTOR_TEMPERATURES   = 9;
        const uint8_t DS5_ENABLE_AUTO_WHITE_BALANCE         = 0xA;
        const uint8_t DS5_ENABLE_AUTO_EXPOSURE              = 0xB;
        const uint8_t DS5_LED_PWR                           = 0xE;
        const uint8_t DS5_THERMAL_COMPENSATION              = 0xF;
        const uint8_t DS5_EMITTER_FREQUENCY                 = 0x10;
        const uint8_t DS5_DEPTH_AUTO_EXPOSURE_MODE          = 0x11;

        // DS5 fisheye XU identifiers
        const uint8_t FISHEYE_EXPOSURE                      = 1;

        // subdevice[h] unit[fw], node[h] guid[fw]
        const platform::extension_unit depth_xu = { 0, 3, 2,
        { 0xC9606CCB, 0x594C, 0x4D25,{ 0xaf, 0x47, 0xcc, 0xc4, 0x96, 0x43, 0x59, 0x95 } } };

        const platform::extension_unit fisheye_xu = { 3, 12, 2,
        { 0xf6c3c3d1, 0x5cde, 0x4477,{ 0xad, 0xf0, 0x41, 0x33, 0xf5, 0x8d, 0xa6, 0xf4 } } };

        const uint32_t REGISTER_CLOCK_0 = 0x0001613c;

        const uint32_t FLASH_SIZE = 0x00200000;
        const uint32_t FLASH_SECTOR_SIZE = 0x1000;
        const uint32_t FLASH_RW_TABLE_OF_CONTENT_OFFSET = 0x0017FF80;
        const uint32_t FLASH_RO_TABLE_OF_CONTENT_OFFSET = 0x001FFE80;
        const uint32_t FLASH_INFO_HEADER_OFFSET = 0x001FFF00;

        flash_info get_flash_info(const std::vector<uint8_t>& flash_buffer);

        enum fw_cmd : uint8_t
        {
            MRD = 0x01,     // Read Register
            FRB = 0x09,     // Read from flash
            FWB = 0x0a,     // Write to flash <Parameter1 Name="StartIndex"> <Parameter2 Name="Size">
            FES = 0x0b,     // Erase flash sector <Parameter1 Name="Sector Index"> <Parameter2 Name="Number of Sectors">
            FEF = 0x0c,     // Erase flash full <Parameter1 Name="0xACE">
            FSRU = 0x0d,     // Flash status register unlock
            FPLOCK = 0x0e,     // Permanent lock on lower Quarter region of the flash
            GLD = 0x0f,     // Legacy get FW logs command
            GVD = 0x10,     // camera details
            GETINTCAL = 0x15,     // Read calibration table
            SETINTCAL = 0x16,     // Set Internal sub calibration table
            LOADINTCAL = 0x1D,     // Get Internal sub calibration table
            DFU = 0x1E,     // Enter to FW update mode
            HWRST = 0x20,     // hardware reset
            OBW = 0x29,     // OVT bypass write
            GTEMP  = 0x2A,     // D400: get ASIC temperature, with mipi device - D500: get all temperatures with one param
            SET_ADV = 0x2B,     // set advanced mode control
            GET_ADV = 0x2C,     // get advanced mode control
            EN_ADV = 0x2D,     // enable advanced mode
            UAMG = 0X30,     // get advanced mode status
            PFD = 0x3b,     // Disable power features <Parameter1 Name="0 - Disable, 1 - Enable" />
            SETAEROI = 0x44,     // set auto-exposure region of interest
            GETAEROI = 0x45,     // get auto-exposure region of interest
            MMER = 0x4F,     // MM EEPROM read ( from DS5 cache )
            CALIBRECALC = 0x51,     // Calibration recalc and update on the fly
            GET_EXTRINSICS = 0x53,     // get extrinsics
            CAL_RESTORE_DFLT = 0x61,     // Reset Depth/RGB calibration to factory settings
            SETINTCALNEW = 0x62,     // Set Internal sub calibration table
            SET_CAM_SYNC = 0x69,     // set Inter-cam HW sync mode [0-default, 1-master, 2-slave]
            GET_CAM_SYNC = 0x6A,     // fet Inter-cam HW sync mode
            SETRGBAEROI = 0x75,     // set RGB auto-exposure region of interest
            GETRGBAEROI = 0x76,     // get RGB auto-exposure region of interest
            SET_PWM_ON_OFF = 0x77,     // set emitter on and off mode
            GET_PWM_ON_OFF = 0x78,     // get emitter on and off mode
            ASIC_TEMP_MIPI  = 0x7A,     // get ASIC temperature - with mipi device
            SETSUBPRESET = 0x7B,     // Download sub-preset
            GETSUBPRESET = 0x7C,     // Upload the current sub-preset
            GETSUBPRESETID = 0x7D,     // Retrieve sub-preset's name
            RECPARAMSGET = 0x7E,     // Retrieve depth calibration table in new format (fw >= 5.11.12.100)
            LASERONCONST = 0x7F,     // Enable Laser On constantly (GS SKU Only)
            AUTO_CALIB = 0x80,      // auto calibration commands
            HKR_THERMAL_COMPENSATION = 0x84, // Control HKR thermal compensation
            GETAELIMITS = 0x89,   //Auto Exp/Gain Limit command FW version >= 5.13.0.200
            SETAELIMITS = 0x8A,   //Auto Exp/Gain Limit command FW version >= 5.13.0.200
            SAFETY_PRESET_READ = 0x94,  // Read safety preset from given index
            SAFETY_PRESET_WRITE = 0x95,   // Write safety preset to given index
            APM_STROBE_SET = 0x96,        // Control if Laser on constantly or pulse
            APM_STROBE_GET = 0x99,        // Query if Laser on constantly or pulse
            SET_HKR_CONFIG_TABLE = 0xA6, // HKR Set Internal sub calibration table
            GET_HKR_CONFIG_TABLE = 0xA7, // HKR Get Internal sub calibration table
            CALIBRESTOREEPROM = 0xA8, // HKR Store EEPROM Calibration
            RGB_TNR = 0xAA,      // RGB Temporal Noise Reduction
            GET_FW_LOGS = 0xB4, // Get FW logs extended format
            SET_CALIB_MODE = 0xB8,      // Set Calibration Mode
            GET_CALIB_STATUS = 0xB9      // Get Calibration Status
        };

#define TOSTRING(arg) #arg
#define VAR_ARG_STR(x) TOSTRING(x)
#define ENUM2STR(x) case(x):return VAR_ARG_STR(x);

        inline std::string fw_cmd2str(const fw_cmd state)
        {
            switch (state)
            {
                ENUM2STR(GLD);
                ENUM2STR(GVD);
                ENUM2STR(GETINTCAL);
                ENUM2STR(OBW);
                ENUM2STR(SET_ADV);
                ENUM2STR(GET_ADV);
                ENUM2STR(EN_ADV);
                ENUM2STR(UAMG);
                ENUM2STR(SETAEROI);
                ENUM2STR(GETAEROI);
                ENUM2STR(MMER);
                ENUM2STR(GET_EXTRINSICS);
                ENUM2STR(SET_CAM_SYNC);
                ENUM2STR(GET_CAM_SYNC);
                ENUM2STR(SETRGBAEROI);
                ENUM2STR(GETRGBAEROI);
                ENUM2STR(SET_PWM_ON_OFF);
                ENUM2STR(GET_PWM_ON_OFF);
                ENUM2STR(SETSUBPRESET);
                ENUM2STR(GETSUBPRESET);
                ENUM2STR(GETSUBPRESETID);
                ENUM2STR(GET_FW_LOGS);
            default:
              return ( rsutils::string::from() << "Unrecognized FW command " << state );
            }
        }

        const int etDepthTableControl = 9; // Identifier of the depth table control

        enum advanced_query_mode
        {
            GET_VAL = 0,
            GET_MIN = 1,
            GET_MAX = 2,
        };

        enum inter_cam_sync_mode
        {
            INTERCAM_SYNC_DEFAULT = 0,
            INTERCAM_SYNC_MASTER = 1,
            INTERCAM_SYNC_SLAVE = 2,
            INTERCAM_SYNC_FULL_SLAVE = 3,
            INTERCAM_SYNC_MAX = 260  // 4-258 are for Genlock with burst count of 1-255 frames for each trigger.
                                     // 259 for Sending two frame - First with laser ON, and the other with laser OFF.
                                     // 260 for Sending two frame - First with laser OFF, and the other with laser ON.
        };

        enum class ds_caps : uint16_t
        {
            CAP_UNDEFINED = 0,
            CAP_ACTIVE_PROJECTOR = (1u << 0),    //
            CAP_RGB_SENSOR = (1u << 1),    // Dedicated RGB sensor
            CAP_FISHEYE_SENSOR = (1u << 2),    // TM1
            CAP_IMU_SENSOR = (1u << 3),
            CAP_GLOBAL_SHUTTER = (1u << 4),
            CAP_ROLLING_SHUTTER = (1u << 5),
            CAP_BMI_055 = (1u << 6),
            CAP_BMI_085 = (1u << 7),
            CAP_INTERCAM_HW_SYNC = (1u << 8),
            CAP_IP65 = (1u << 9),
            CAP_IR_FILTER = (1u << 10),
            CAP_BMI_088 = (1u << 11),
            CAP_MAX
        };

        static const std::map<ds_caps, std::string> ds_capabilities_names = {
            { ds_caps::CAP_UNDEFINED,        "Undefined"         },
            { ds_caps::CAP_ACTIVE_PROJECTOR, "Active Projector"  },
            { ds_caps::CAP_RGB_SENSOR,       "RGB Sensor"        },
            { ds_caps::CAP_FISHEYE_SENSOR,   "Fisheye Sensor"    },
            { ds_caps::CAP_IMU_SENSOR,       "IMU Sensor"        },
            { ds_caps::CAP_GLOBAL_SHUTTER,   "Global Shutter"    },
            { ds_caps::CAP_ROLLING_SHUTTER,  "Rolling Shutter"   },
            { ds_caps::CAP_BMI_055,          "IMU BMI_055"       },
            { ds_caps::CAP_BMI_085,          "IMU BMI_085"       },
            { ds_caps::CAP_IP65,             "IP65 Sealed device"},
            { ds_caps::CAP_IR_FILTER,        "IR filter"         },
            { ds_caps::CAP_BMI_088,          "IMU BMI_088"       }
        };

        inline ds_caps operator &(const ds_caps lhs, const ds_caps rhs)
        {
            return static_cast<ds_caps>(static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs));
        }

        inline ds_caps operator |(const ds_caps lhs, const ds_caps rhs)
        {
            return static_cast<ds_caps>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
        }

        inline ds_caps& operator |=(ds_caps& lhs, ds_caps rhs)
        {
            return lhs = lhs | rhs;
        }

        inline bool operator &&(ds_caps l, ds_caps r)
        {
            return !!(static_cast<uint8_t>(l) & static_cast<uint8_t>(r));
        }

        inline std::ostream& operator <<(std::ostream& stream, const ds_caps& cap)
        {
            for (auto i : { ds_caps::CAP_ACTIVE_PROJECTOR,ds_caps::CAP_RGB_SENSOR,
                            ds_caps::CAP_FISHEYE_SENSOR,  ds_caps::CAP_IMU_SENSOR,
                            ds_caps::CAP_GLOBAL_SHUTTER,  ds_caps::CAP_ROLLING_SHUTTER,
                            ds_caps::CAP_BMI_055,         ds_caps::CAP_BMI_085,
                            ds_caps::CAP_IP65,            ds_caps::CAP_IR_FILTER })
            {
                if (i == (i & cap))
                    stream << ds_capabilities_names.at(i) << "/";
            }
            return stream;
        }

        const std::string DEPTH_STEREO = "Stereo Module";

#pragma pack(push, 1)
        struct table_header
        {
            big_endian<uint16_t>    version;        // major.minor. Big-endian
            uint16_t                table_type;     // ctCalibration
            uint32_t                table_size;     // full size including: TOC header + TOC + actual tables
            uint32_t                param;          // This field content is defined ny table type
            uint32_t                crc32;          // crc of all the actual table data excluding header/CRC

            std::string to_string() const;
        };

        // Note ds_rect_resolutions is used in struct d400_coefficients_table. Update with caution.
        enum ds_rect_resolutions : unsigned short
        {
            res_1920_1080,
            res_1280_720,
            res_640_480,
            res_848_480,
            res_640_360,
            res_424_240,
            res_320_240,
            res_480_270,
            res_1280_800,
            res_960_540,
            reserved_1,
            reserved_2,
            res_640_400,
            // Resolutions for DS5U
            res_576_576,
            res_720_720,
            res_1152_1152,
            max_ds_rect_resolutions
        };

        struct new_calibration_item
        {
            uint16_t width;
            uint16_t height;
            float  fx;
            float  fy;
            float  ppx;
            float  ppy;
        };
#pragma pack(pop)

        template<class T>
        const T* check_calib(const std::vector<uint8_t>& raw_data)
        {
            using namespace std;

            auto table = reinterpret_cast<const T*>(raw_data.data());
            auto header = reinterpret_cast<const table_header*>(raw_data.data());
            if (raw_data.size() < sizeof(table_header))
            {
                throw invalid_value_exception( rsutils::string::from()
                                               << "Calibration data invalid, buffer too small : expected "
                                               << sizeof( table_header ) << " , actual: " << raw_data.size() );
            }

            // Make sure the table size does not exceed the actual data we have!
            if( header->table_size + sizeof( table_header ) > raw_data.size() )
            {
                throw invalid_value_exception( rsutils::string::from()
                                               << "Calibration table size does not fit inside reply: expected "
                                               << ( raw_data.size() - sizeof( table_header ) ) << " but got "
                                               << header->table_size );
            }

            // verify the parsed table
            if (table->header.crc32 != rsutils::number::calc_crc32(raw_data.data() + sizeof(table_header), raw_data.size() - sizeof(table_header)))
            {
                throw invalid_value_exception("Calibration data CRC error, parsing aborted!");
            }

            //LOG_DEBUG("Loaded Valid Table: version [mjr.mnr]: 0x" <<
            //    hex << setfill('0') << setw(4) << header->version << dec
            //    << ", type " << header->table_type << ", size " << header->table_size
            //    << ", CRC: " << hex << header->crc32 << dec );
            return table;
        }

#pragma pack(push, 1)

        struct fisheye_extrinsics_table
        {
            table_header        header;
            int64_t             serial_mm;
            int64_t             serial_depth;
            float3x3            rotation;                   //  the fisheye rotation matrix
            float3              translation;                //  the fisheye translation vector
        };

        struct extrinsics_table
        {
            float3x3            rotation;
            float3              translation;
        };

        struct imu_intrinsics
        {
            float bias[3];
            float scale[3];
        };

        // Note that the intrinsic definition follows rs2_motion_device_intrinsic with different data layout
        struct imu_intrinsic
        {
            float3x3    sensitivity;
            float3      bias;
            float3      noise_variances;  /**< Variance of noise for X, Y, and Z axis */
            float3      bias_variances;   /**< Variance of bias for X, Y, and Z axis */
        };

        struct fisheye_calibration_table
        {
            table_header        header;
            float               intrinsics_model;           //  1 - Brown, 2 - FOV, 3 - Kannala Brandt
            float3x3            intrinsic;                  //  FishEye intrinsic matrix, normalize by [-1 1]
            float               distortion[5];              //  FishEye forward distortion parameters, F-theta model
            extrinsics_table    fisheye_to_imu;             //  FishEye rotation matrix and translation vector in IMU CS
            uint8_t             reserved[28];
        };

        constexpr size_t fisheye_calibration_table_size = sizeof(fisheye_calibration_table);

        struct imu_calibration_table
        {
            table_header        header;
            float               rmax;
            extrinsics_table    imu_to_imu;
            imu_intrinsics      accel_intrinsics;
            imu_intrinsics      gyro_intrinsics;
            uint8_t             reserved[64];
        };

        constexpr size_t imu_calibration_table_size = sizeof(imu_calibration_table);

        struct tm1_module_info
        {
            table_header        header;
            uint8_t             serial_num[8];              // 2 bytes reserved + 6 data (0000xxxxxxxxxxxx)
            uint8_t             optic_module_mm[4];
            uint8_t             ta[10];
            uint32_t            board_num;                  // SKU id
            uint32_t            board_rev;                  // 0
            uint8_t             reserved[34];               // Align to 64 byte ???
        };

        constexpr size_t tm1_module_info_size = sizeof(tm1_module_info);

        struct tm1_calib_model
        {
            table_header                header;
            float                       calibration_model_flag;     //  1 - Brown, 2 - FOV, 3 - Kannala Brandt ???????
            fisheye_calibration_table   fe_calibration;
            float                       temperature;
            uint8_t                     reserved[20];
        };

        constexpr size_t tm1_calib_model_size = sizeof(tm1_calib_model);

        struct tm1_serial_num_table
        {
            table_header                header;
            uint8_t                     serial_num[8];              // 2 bytes reserved + 6 data  12 digits in (0000xxxxxxxxxxxx) format
            uint8_t                     reserved[8];
        };

        constexpr size_t tm1_serial_num_table_size = sizeof(tm1_serial_num_table);

        struct tm1_calibration_table
        {
            table_header                header;
            tm1_calib_model             calib_model;
            imu_calibration_table       imu_calib_table;
            tm1_serial_num_table        serial_num_table;
        };

        constexpr size_t tm1_calibration_table_size = sizeof(tm1_calibration_table);

        // TM1 ver 0.51
        struct tm1_eeprom
        {
            table_header            header;
            tm1_module_info         module_info;
            tm1_calibration_table   calibration_table;
        };

        constexpr size_t tm1_eeprom_size = sizeof(tm1_eeprom);

        struct dm_v2_imu_intrinsic
        {
            float3x3            sensitivity;
            float3              bias;
        };

        struct dm_v2_calibration_table
        {
            table_header            header;
            uint8_t                 extrinsic_valid;
            uint8_t                 intrinsic_valid;
            uint8_t                 reserved[2];
            extrinsics_table        depth_to_imu;       // The extrinsic parameters of IMU persented in Depth sensor's CS
            dm_v2_imu_intrinsic     accel_intrinsic;
            dm_v2_imu_intrinsic     gyro_intrinsic;
            uint8_t                 reserved1[96];
        };

        constexpr size_t dm_v2_calibration_table_size = sizeof(dm_v2_calibration_table);

        struct dm_v2_calib_info
        {
            table_header            header;
            dm_v2_calibration_table dm_v2_calib_table;
            tm1_serial_num_table    serial_num_table;
        };

        constexpr size_t dm_v2_calib_info_size = sizeof(dm_v2_calib_info);

        // Depth Module V2 IMU EEPROM ver 0.52
        struct dm_v2_eeprom
        {
            table_header            header;
            dm_v2_calib_info        module_info;
        };

        constexpr size_t dm_v2_eeprom_size = sizeof(dm_v2_eeprom);

        union eeprom_imu_table {
            tm1_eeprom      tm1_table;
            dm_v2_eeprom    dm_v2_table;
        };

        constexpr size_t eeprom_imu_table_size = sizeof(eeprom_imu_table);

        enum imu_eeprom_id : uint16_t
        {
            dm_v2_eeprom_id = 0x0101,   // The pack alignment is Big-endian
            tm1_eeprom_id = 0x0002,
            l500_eeprom_id = 0x0105
        };

        struct depth_table_control
        {
            uint32_t depth_units;
            int32_t depth_clamp_min;
            int32_t depth_clamp_max;
            int32_t disparity_multiplier;
            int32_t disparity_shift;
        };

        inline rs2_motion_device_intrinsic create_motion_intrinsics(imu_intrinsic data)
        {
            rs2_motion_device_intrinsic result{};

            for (int i = 0; i < 3; i++)
            {
                for (int j = 0; j < 3; j++)
                    result.data[i][j] = data.sensitivity(i, j);

                result.data[i][3] = data.bias[i];
                result.bias_variances[i] = data.bias_variances[i];
                result.noise_variances[i] = data.noise_variances[i];
            }
            return result;
        }

#pragma pack(pop)
        namespace d400_gvd
        {
        enum d400_gvd_offsets {
            // Keep sorted
            gvd_version_offset = 2,
            camera_fw_version_offset = 12,
            is_camera_locked_offset = 25,
            module_serial_offset = 48,
            module_asic_serial_offset = 64,
            fisheye_sensor_lb = 112,
            fisheye_sensor_hb = 113,
            imu_acc_chip_id = 124,
            ip65_sealed_offset = 161,
            ir_filter_offset = 164,
            depth_sensor_type = 166,
            active_projector = 170,
            rgb_sensor = 174,
            imu_sensor = 178,
            motion_module_fw_version_offset = 212
        };
        }  // namespace d400_gvd        

        const uint8_t I2C_IMU_BMI055_ID_ACC = 0xfa;
        const uint8_t I2C_IMU_BMI085_ID_ACC = 0x1f;
        const uint8_t I2C_IMU_BMI088_ID_ACC = 0x1e;

        enum gvd_fields_size
        {
            // Keep sorted
            module_serial_size = 6
        };
        

        static std::map< ds_rect_resolutions, int2> resolutions_list = {
            { res_320_240,{ 320, 240 } },
            { res_424_240,{ 424, 240 } },
            { res_480_270,{ 480, 270 } },
            { res_640_360,{ 640, 360 } },
            { res_640_400,{ 640, 400 } },
            { res_640_480,{ 640, 480 } },
            { res_848_480,{ 848, 480 } },
            { res_960_540,{ 960, 540 } },
            { res_1280_720,{ 1280, 720 } },
            { res_1280_800,{ 1280, 800 } },
            { res_1920_1080,{ 1920, 1080 } },
            { res_576_576,{ 576, 576 } },
            { res_720_720,{ 720, 720 } },
            { res_1152_1152,{ 1152, 1152 } },
        };

        ds_rect_resolutions width_height_to_ds_rect_resolutions(uint32_t width, uint32_t height);

        // subpreset pattern used in firmware versions that do not support subpreset ID
        const std::vector<uint8_t> alternating_emitter_pattern_with_name{ 0x19, 0,
            0x41, 0x6c, 0x74, 0x65, 0x72, 0x6e, 0x61, 0x74, 0x69, 0x6e, 0x67, 0x5f, 0x45, 0x6d, 0x69, 0x74, 0x74, 0x65, 0x72, 0,
            0, 0x2, 0, 0x5, 0, 0x1, 0x1, 0, 0, 0, 0, 0, 0, 0, 0x5, 0, 0x1, 0x1, 0, 0, 0, 0x1, 0, 0, 0 };

        // subpreset ID for the alternating emitter subpreset as const
        // in order to permit the query of this option to check if the current subpreset ID
        // is the alternating emitter ID
        const uint8_t ALTERNATING_EMITTER_SUBPRESET_ID = 0x0f;

        const std::vector<uint8_t> alternating_emitter_pattern{ 0x5, ALTERNATING_EMITTER_SUBPRESET_ID, 0, 0, 0x2,
            0x4, 0x1, 0, 0x1, 0, 0, 0, 0, 0,
            0x4, 0x1, 0, 0x1, 0, 0x1, 0, 0, 0 };

        bool is_partial_device_allowed( const std::shared_ptr< context > & ctx );

    } // librealsense::ds
} // namespace librealsense
