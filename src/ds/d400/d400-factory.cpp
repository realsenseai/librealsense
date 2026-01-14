// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2016-24 RealSense, Inc. All Rights Reserved.

#include <mutex>
#include <chrono>
#include <vector>
#include <iterator>
#include <cstddef>

#include "device.h"
#include "image.h"
#include "metadata-parser.h"
#include "context.h"

#include <src/core/matcher-factory.h>

#include "d400-info.h"
#include "d400-private.h"
#include "d400-options.h"
#include "ds/ds-timestamp.h"
#include "d400-nonmonochrome.h"
#include "d400-active.h"
#include "d400-color.h"
#include "d400-motion.h"
#include "d400-mipi-device.h"
#include <src/ds/ds-thermal-monitor.h>
#include "sync.h"

#include <src/platform/platform-utils.h>

#include "firmware_logger_device.h"
#include "device-calibration.h"

#include <rsutils/string/from.h>

#include <src/ds/features/auto-exposure-limit-feature.h>
#include <src/ds/features/gain-limit-feature.h>
#include <src/ds/features/gyro-sensitivity-feature.h>

namespace librealsense
{
    // PSR
    class rs400_device : public d400_nonmonochrome,
                         public ds_advanced_mode_base,
                         public firmware_logger_device
    {
    public:
        rs400_device( std::shared_ptr< const d400_info > const & dev_info, bool register_device_notifications )
            : device( dev_info, register_device_notifications )
            , backend_device( dev_info, register_device_notifications )
            , d400_device( dev_info )
            , d400_nonmonochrome( dev_info )
            , ds_advanced_mode_base()
            , firmware_logger_device( dev_info, d400_device::_hw_monitor, get_firmware_logs_command(), get_flash_logs_command() )
        {
            ds_advanced_mode_base::initialize_advanced_mode( this );
        }

        std::shared_ptr<matcher> create_matcher(const frame_holder& frame) const override;

        std::vector<tagged_profile> get_profiles_tags() const override
        {
            std::vector<tagged_profile> tags;
            auto usb_spec = get_usb_spec();
            if (usb_spec >= platform::usb3_type || usb_spec == platform::usb_undefined)
            {
                tags.push_back({ RS2_STREAM_DEPTH, -1, 1280, 720, RS2_FORMAT_Z16, 30, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
                tags.push_back({ RS2_STREAM_INFRARED, 1, 1280, 720, RS2_FORMAT_RGB8, 30, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
                tags.push_back({ RS2_STREAM_INFRARED, 2, 1280, 720, RS2_FORMAT_RGB8, 30, profile_tag::PROFILE_TAG_SUPERSET });
            }
            else
            {
                tags.push_back({ RS2_STREAM_DEPTH, -1, 640, 480, RS2_FORMAT_Z16, 15, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
                tags.push_back({ RS2_STREAM_INFRARED, 1, 640, 480, RS2_FORMAT_RGB8, 15, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
                tags.push_back({ RS2_STREAM_INFRARED, 2, 640, 480, RS2_FORMAT_RGB8, 15, profile_tag::PROFILE_TAG_SUPERSET });
            }
            return tags;
        };
    };

    class rs401_gmsl_device : public d400_color,
                              public d400_nonmonochrome,
                              public d400_mipi_device,
                              public firmware_logger_device
    {
    public:
        rs401_gmsl_device( std::shared_ptr< const d400_info > const & dev_info, bool register_device_notifications )
            : device( dev_info, register_device_notifications )
            , backend_device( dev_info, register_device_notifications )
            , d400_device( dev_info )
            , d400_color( dev_info )
            , d400_nonmonochrome( dev_info )
            , d400_mipi_device()
            , firmware_logger_device( dev_info, d400_device::_hw_monitor, get_firmware_logs_command(), get_flash_logs_command() )
        {
        }

        std::shared_ptr<matcher> create_matcher(const frame_holder& frame) const override;

        std::vector<tagged_profile> get_profiles_tags() const override
        {
            std::vector<tagged_profile> tags;

            int depth_width = 848;
            int depth_height = 480;
            int color_width = 848;
            int color_height = 480;
            int fps = 30;

            tags.push_back( { RS2_STREAM_COLOR,
                              -1,  // index
                              color_width,
                              color_height,
                              get_color_format(),
                              fps,
                              profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT } );
            tags.push_back( { RS2_STREAM_DEPTH,
                              -1,  // index
                              depth_width,
                              depth_height,
                              RS2_FORMAT_Z16,
                              fps,
                              profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT } );
            tags.push_back( { RS2_STREAM_INFRARED,
                              -1,  // index
                              depth_width,
                              depth_height,
                              get_ir_format(),
                              fps,
                              profile_tag::PROFILE_TAG_SUPERSET } );
            return tags;
        };
    };

    // Not used, should be removed with EOL devices clean up
    class rs405u_device : public ds5u_device,
                          public ds_advanced_mode_base,
                          public firmware_logger_device
    {
    public:
        rs405u_device( std::shared_ptr< const d400_info > const & dev_info, bool register_device_notifications )
            : device(dev_info, register_device_notifications)
            , backend_device( dev_info, register_device_notifications )
            , ds5u_device( dev_info )
            , ds_advanced_mode_base()
            , firmware_logger_device( dev_info, d400_device::_hw_monitor, get_firmware_logs_command(), get_flash_logs_command() )
        {
            ds_advanced_mode_base::initialize_advanced_mode( this );
        }

        std::shared_ptr<matcher> create_matcher(const frame_holder& frame) const override;

        std::vector<tagged_profile> get_profiles_tags() const override
        {
            std::vector<tagged_profile> tags;
            auto usb_spec = get_usb_spec();
            if (usb_spec >= platform::usb3_type || usb_spec == platform::usb_undefined)
            {
                tags.push_back({ RS2_STREAM_DEPTH, -1, 720, 720, RS2_FORMAT_Z16, 30, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
                tags.push_back({ RS2_STREAM_INFRARED, 1, 1152, 1152, RS2_FORMAT_Y8, 30, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
                tags.push_back({ RS2_STREAM_INFRARED, 2, 1152, 1152, RS2_FORMAT_Y16, 30, profile_tag::PROFILE_TAG_SUPERSET });
            }
            else
            {
                tags.push_back({ RS2_STREAM_DEPTH, -1, 720, 720, RS2_FORMAT_Z16, 15, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
                tags.push_back({ RS2_STREAM_INFRARED, 1, 1152, 1152, RS2_FORMAT_Y8, 15, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
                tags.push_back({ RS2_STREAM_INFRARED, 2, 1152, 1152, RS2_FORMAT_Y16, 15, profile_tag::PROFILE_TAG_SUPERSET });
            }
            return tags;
        };

        bool contradicts(const stream_profile_interface* a, const std::vector<stream_profile>& others) const override
        {
            if (auto vid_a = dynamic_cast<const video_stream_profile_interface*>(a))
            {
                for (auto request : others)
                {
                    if (a->get_framerate() != 0 && request.fps != 0 && (a->get_framerate() != request.fps))
                        return true;
                }
            }
            return false;
        }
    };

    // ASR (D460)
    class rs410_device : public d400_nonmonochrome,
                         public d400_active,
                         public ds_advanced_mode_base,
                         public firmware_logger_device
    {
    public:
        rs410_device( std::shared_ptr< const d400_info > const & dev_info, bool register_device_notifications )
            : device( dev_info, register_device_notifications )
            , backend_device( dev_info, register_device_notifications )
            , d400_device( dev_info )
            , d400_nonmonochrome( dev_info )
            , d400_active( dev_info )
            , ds_advanced_mode_base()
            , firmware_logger_device( dev_info, d400_device::_hw_monitor, get_firmware_logs_command(), get_flash_logs_command() )
        {
            ds_advanced_mode_base::initialize_advanced_mode( this );
        }

        std::shared_ptr<matcher> create_matcher(const frame_holder& frame) const override;

        std::vector<tagged_profile> get_profiles_tags() const override
        {
            std::vector<tagged_profile> tags;
            auto usb_spec = get_usb_spec();
            if (usb_spec >= platform::usb3_type || usb_spec == platform::usb_undefined)
            {
                tags.push_back({ RS2_STREAM_DEPTH, -1, 1280, 720, RS2_FORMAT_Z16, 30, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
                tags.push_back({ RS2_STREAM_INFRARED, 0, 1280, 720, RS2_FORMAT_RGB8, 30, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
            }
            else
            {
                //TODO: F416 currntlly detected as RS_USB2_PID when connected via USB2 port
            }
            return tags;
        };
    };

    // ASRC
    class rs415_device : public d400_nonmonochrome,
                         public d400_active,
                         public d400_color,
                         public ds_advanced_mode_base,
                         public firmware_logger_device
    {
    public:
        rs415_device( std::shared_ptr< const d400_info > const & dev_info, bool register_device_notifications )
            : device( dev_info, register_device_notifications )
            , backend_device( dev_info, register_device_notifications )
            , d400_device( dev_info )
            , d400_nonmonochrome( dev_info )
            , d400_active( dev_info )
            , d400_color( dev_info )
            , ds_advanced_mode_base()
            , firmware_logger_device( dev_info, d400_device::_hw_monitor, get_firmware_logs_command(), get_flash_logs_command() )
        {
            ds_advanced_mode_base::initialize_advanced_mode( this );
        }

        std::shared_ptr<matcher> create_matcher(const frame_holder& frame) const override;

        std::vector<tagged_profile> get_profiles_tags() const override
        {
            std::vector<tagged_profile> tags;
            auto usb_spec = get_usb_spec();
            if (usb_spec >= platform::usb3_type || usb_spec == platform::usb_undefined)
            {
                tags.push_back({ RS2_STREAM_COLOR, -1, 1280, 720, get_color_format(), 30, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
                tags.push_back({ RS2_STREAM_DEPTH, -1, 1280, 720, RS2_FORMAT_Z16, 30, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
                tags.push_back({ RS2_STREAM_INFRARED, -1, 1280, 720, RS2_FORMAT_Y8, 30, profile_tag::PROFILE_TAG_SUPERSET });
            }
            else
            {
                tags.push_back({ RS2_STREAM_COLOR, -1, 640, 480, get_color_format(), 15, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
                tags.push_back({ RS2_STREAM_DEPTH, -1, 640, 480, RS2_FORMAT_Z16, 15, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
                tags.push_back({ RS2_STREAM_INFRARED, -1, 640, 480, RS2_FORMAT_Y8, 15, profile_tag::PROFILE_TAG_SUPERSET });
            }
            return tags;
        };
    };

    // monochrome not implemented for this device
    class rs415_gmsl_device : public d400_active,
                              public d400_color,
                              public d400_mipi_device,
                              public firmware_logger_device
    {
    public:
        rs415_gmsl_device( std::shared_ptr< const d400_info > const & dev_info, bool register_device_notifications )
            : device( dev_info, register_device_notifications )
            , backend_device( dev_info, register_device_notifications )
            , d400_device( dev_info )
            , d400_active( dev_info )
            , d400_color( dev_info )
            , d400_mipi_device()
            , firmware_logger_device( dev_info, d400_device::_hw_monitor, get_firmware_logs_command(), get_flash_logs_command() )
        {
        }

        std::shared_ptr<matcher> create_matcher(const frame_holder& frame) const override;

        std::vector<tagged_profile> get_profiles_tags() const override
        {
            std::vector<tagged_profile> tags;
            auto usb_spec = get_usb_spec();
            if (usb_spec >= platform::usb3_type || usb_spec == platform::usb_undefined)
            {
                tags.push_back({ RS2_STREAM_COLOR, -1, 1280, 720, get_color_format(), 30, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
                tags.push_back({ RS2_STREAM_DEPTH, -1, 1280, 720, RS2_FORMAT_Z16, 30, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
                tags.push_back({ RS2_STREAM_INFRARED, -1, 1280, 720, RS2_FORMAT_Y8, 30, profile_tag::PROFILE_TAG_SUPERSET });
            }
            else
            {
                tags.push_back({ RS2_STREAM_COLOR, -1, 640, 480, get_color_format(), 15, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
                tags.push_back({ RS2_STREAM_DEPTH, -1, 640, 480, RS2_FORMAT_Z16, 15, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
                tags.push_back({ RS2_STREAM_INFRARED, -1, 640, 480, RS2_FORMAT_Y8, 15, profile_tag::PROFILE_TAG_SUPERSET });
            }
            return tags;
        };
    };


    class rs416_device : public d400_nonmonochrome,
                         public d400_active,
                         public ds_advanced_mode_base,
                         public firmware_logger_device
    {
    public:
        rs416_device( std::shared_ptr< const d400_info > const & dev_info, bool register_device_notifications )
            : device( dev_info, register_device_notifications )
            , backend_device( dev_info, register_device_notifications )
            , d400_device( dev_info )
            , d400_nonmonochrome( dev_info )
            , d400_active( dev_info )
            , ds_advanced_mode_base()
            , firmware_logger_device( dev_info, d400_device::_hw_monitor, get_firmware_logs_command(), get_flash_logs_command() )
        {
            ds_advanced_mode_base::initialize_advanced_mode( this );
        }

        std::shared_ptr<matcher> create_matcher(const frame_holder& frame) const override;

        std::vector<tagged_profile> get_profiles_tags() const override
        {
            std::vector<tagged_profile> tags;
            auto usb_spec = get_usb_spec();
            if (usb_spec >= platform::usb3_type || usb_spec == platform::usb_undefined)
            {
                tags.push_back({ RS2_STREAM_DEPTH, -1, 720, 720, RS2_FORMAT_Z16, 30, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
                tags.push_back({ RS2_STREAM_INFRARED, 0, 720, 720, RS2_FORMAT_RGB8, 30, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
            }
            else
            {
                tags.push_back({ RS2_STREAM_DEPTH, -1, 640, 480, RS2_FORMAT_Z16, 15, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
                tags.push_back({ RS2_STREAM_INFRARED, 1, 640, 480, RS2_FORMAT_Y8, 15, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
            }
            return tags;
        };

        bool contradicts(const stream_profile_interface* a, const std::vector<stream_profile>& others) const override
        {
            if (auto vid_a = dynamic_cast<const video_stream_profile_interface*>(a))
            {
                for (auto request : others)
                {
                    if (a->get_framerate() != 0 && request.fps != 0 && (a->get_framerate() != request.fps))
                        return true;
                }
            }
            return false;
        }
    };

    class rs416_rgb_device : public d400_nonmonochrome,
                             public d400_active,
                             public d400_color,
                             public ds_advanced_mode_base,
                             public firmware_logger_device

    {
    public:
        rs416_rgb_device( std::shared_ptr< const d400_info > const & dev_info, bool register_device_notifications )
            : device( dev_info, register_device_notifications )
            , backend_device( dev_info, register_device_notifications )
            , d400_device( dev_info )
            , d400_nonmonochrome( dev_info )
            , d400_active( dev_info )
            , d400_color( dev_info )
            , ds_advanced_mode_base()
            , firmware_logger_device( dev_info, d400_device::_hw_monitor, get_firmware_logs_command(), get_flash_logs_command() )
        {
            ds_advanced_mode_base::initialize_advanced_mode( this );
        }

        std::shared_ptr<matcher> create_matcher(const frame_holder& frame) const override;

        std::vector<tagged_profile> get_profiles_tags() const override
        {
            std::vector<tagged_profile> tags;
            auto usb_spec = get_usb_spec();
            if (usb_spec >= platform::usb3_type || usb_spec == platform::usb_undefined)
            {
                tags.push_back({ RS2_STREAM_DEPTH, -1, 720, 720, RS2_FORMAT_Z16, 30, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
                tags.push_back({ RS2_STREAM_COLOR, -1, 640, 480, get_color_format(), 30, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
                tags.push_back({ RS2_STREAM_INFRARED, 1, 720, 720, RS2_FORMAT_Y8, 30, profile_tag::PROFILE_TAG_SUPERSET });
            }
            else
            {
                tags.push_back({ RS2_STREAM_DEPTH, -1, 640, 480, RS2_FORMAT_Z16, 15, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
                tags.push_back({ RS2_STREAM_COLOR, -1, 640, 480, get_color_format(), 15, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
                tags.push_back({ RS2_STREAM_INFRARED, 1, 640, 480, RS2_FORMAT_Y8, 15, profile_tag::PROFILE_TAG_SUPERSET });
            }
            return tags;
        };

        bool contradicts(const stream_profile_interface* a, const std::vector<stream_profile>& others) const override
        {
            if (auto vid_a = dynamic_cast<const video_stream_profile_interface*>(a))
            {
                for (auto request : others)
                {
                    if (a->get_framerate() != 0 && request.fps != 0 && (a->get_framerate() != request.fps))
                        return true;
                }
            }
            return false;
        }
    };

    // PWGT
    class rs420_mm_device : public d400_motion,
                            public ds_advanced_mode_base,
                            public firmware_logger_device
    {
    public:
        rs420_mm_device( std::shared_ptr< const d400_info > const & dev_info, bool register_device_notifications )
            : device( dev_info, register_device_notifications )
            , backend_device( dev_info, register_device_notifications )
            , d400_device( dev_info )
            , d400_motion( dev_info )
            , ds_advanced_mode_base()
            , firmware_logger_device( dev_info, d400_device::_hw_monitor, get_firmware_logs_command(), get_flash_logs_command() )
        {
            ds_advanced_mode_base::initialize_advanced_mode( this );
        }

        std::shared_ptr<matcher> create_matcher(const frame_holder& frame) const override;

        std::vector<tagged_profile> get_profiles_tags() const override
        {
            std::vector<tagged_profile> tags;
            auto usb_spec = get_usb_spec();
            if (usb_spec >= platform::usb3_type || usb_spec == platform::usb_undefined)
            {
                tags.push_back({ RS2_STREAM_DEPTH, -1, 848, 480, RS2_FORMAT_Z16, 30, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
                tags.push_back({ RS2_STREAM_INFRARED, 1, 848, 480, RS2_FORMAT_Y8, 30, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
                tags.push_back({ RS2_STREAM_INFRARED, 2, 848, 480, RS2_FORMAT_Y8, 30, profile_tag::PROFILE_TAG_SUPERSET });
            }
            else
            {
                tags.push_back({ RS2_STREAM_DEPTH, -1, 640, 480, RS2_FORMAT_Z16, 15, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
                tags.push_back({ RS2_STREAM_INFRARED, 1, 640, 480, RS2_FORMAT_Y8, 15, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
                tags.push_back({ RS2_STREAM_INFRARED, 2, 640, 480, RS2_FORMAT_Y8, 15, profile_tag::PROFILE_TAG_SUPERSET });
            }

            if (_fw_version >= firmware_version("5.10.4.0"))
            {
                tags.push_back({ RS2_STREAM_FISHEYE, -1, 640, 480, RS2_FORMAT_RAW8, 30, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT});
                tags.push_back({RS2_STREAM_GYRO, -1, 0, 0, RS2_FORMAT_MOTION_XYZ32F, (int)odr::IMU_FPS_200, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
                tags.push_back({RS2_STREAM_ACCEL, -1, 0, 0, RS2_FORMAT_MOTION_XYZ32F, (int)odr::IMU_FPS_63, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
                tags.push_back({RS2_STREAM_ACCEL, -1, 0, 0, RS2_FORMAT_MOTION_XYZ32F, (int)odr::IMU_FPS_100, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
            }

            return tags;
        };
    };

    // PWG
    class rs420_device : public d400_device,
                         public ds_advanced_mode_base,
                         public firmware_logger_device
    {
    public:
        rs420_device( std::shared_ptr< const d400_info > const & dev_info, bool register_device_notifications )
            : device( dev_info, register_device_notifications )
            , backend_device( dev_info, register_device_notifications )
            , d400_device( dev_info )
            , ds_advanced_mode_base()
            , firmware_logger_device( dev_info, d400_device::_hw_monitor, get_firmware_logs_command(), get_flash_logs_command() )
        {
            ds_advanced_mode_base::initialize_advanced_mode( this );
        }

        std::shared_ptr<matcher> create_matcher(const frame_holder& frame) const override;

        std::vector<tagged_profile> get_profiles_tags() const override
        {
            std::vector<tagged_profile> tags;
            auto usb_spec = get_usb_spec();
            if (usb_spec >= platform::usb3_type || usb_spec == platform::usb_undefined)
            {
                tags.push_back({ RS2_STREAM_DEPTH, -1, 848, 480, RS2_FORMAT_Z16, 30, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
                tags.push_back({ RS2_STREAM_INFRARED, 1, 848, 480, RS2_FORMAT_Y8, 30, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
                tags.push_back({ RS2_STREAM_INFRARED, 2, 848, 480, RS2_FORMAT_Y8, 30, profile_tag::PROFILE_TAG_SUPERSET });
            }
            else
            {
                tags.push_back({ RS2_STREAM_DEPTH, -1, 640, 480, RS2_FORMAT_Z16, 15, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
                tags.push_back({ RS2_STREAM_INFRARED, 1, 640, 480, RS2_FORMAT_Y8, 15, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
                tags.push_back({ RS2_STREAM_INFRARED, 2, 640, 480, RS2_FORMAT_Y8, 15, profile_tag::PROFILE_TAG_SUPERSET });
            }
            return tags;
        };
    };

    class rs421_device : public d400_active,
                         public ds_advanced_mode_base,
                         public firmware_logger_device
    {
    public:
        rs421_device( std::shared_ptr< const d400_info > const & dev_info, bool register_device_notifications )
            : device( dev_info, register_device_notifications )
            , backend_device( dev_info, register_device_notifications )
            , d400_device( dev_info )
            , d400_active( dev_info )
            , ds_advanced_mode_base()
            , firmware_logger_device( dev_info, d400_device::_hw_monitor, get_firmware_logs_command(), get_flash_logs_command() )
        {
            ds_advanced_mode_base::initialize_advanced_mode( this );

            // Unregister unsupported options registered commonly by d400_device
            auto & depth_sensor = get_depth_sensor();
            depth_sensor.unregister_option( RS2_OPTION_OUTPUT_TRIGGER_ENABLED );
            depth_sensor.unregister_option( RS2_OPTION_INTER_CAM_SYNC_MODE );
        }

        std::shared_ptr<matcher> create_matcher(const frame_holder& frame) const override;

        std::vector<tagged_profile> get_profiles_tags() const override
        {
            std::vector<tagged_profile> tags;
            auto usb_spec = get_usb_spec();
            if (usb_spec >= platform::usb3_type || usb_spec == platform::usb_undefined)
            {
                tags.push_back({ RS2_STREAM_DEPTH, -1, 848, 480, RS2_FORMAT_Z16, 30, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
                tags.push_back({ RS2_STREAM_INFRARED, 1, 848, 480, RS2_FORMAT_Y8, 30, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
                tags.push_back({ RS2_STREAM_INFRARED, 2, 848, 480, RS2_FORMAT_Y8, 30, profile_tag::PROFILE_TAG_SUPERSET });
            }
            else
            {
                tags.push_back({ RS2_STREAM_DEPTH, -1, 640, 480, RS2_FORMAT_Z16, 15, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
                tags.push_back({ RS2_STREAM_INFRARED, 1, 640, 480, RS2_FORMAT_Y8, 15, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
                tags.push_back({ RS2_STREAM_INFRARED, 2, 640, 480, RS2_FORMAT_Y8, 15, profile_tag::PROFILE_TAG_SUPERSET });
            }
            return tags;
        };
    };

    // AWG
    class rs430_device : public d400_active,
                         public ds_advanced_mode_base,
                         public firmware_logger_device
    {
    public:
        rs430_device( std::shared_ptr< const d400_info > const & dev_info, bool register_device_notifications )
            : device( dev_info, register_device_notifications )
            , backend_device( dev_info, register_device_notifications )
            , d400_device( dev_info )
            , d400_active( dev_info )
            , ds_advanced_mode_base()
            , firmware_logger_device( dev_info, d400_device::_hw_monitor, get_firmware_logs_command(), get_flash_logs_command() )
        {
            ds_advanced_mode_base::initialize_advanced_mode( this );
        }

        std::shared_ptr<matcher> create_matcher(const frame_holder& frame) const override;

        std::vector<tagged_profile> get_profiles_tags() const override
        {
            std::vector<tagged_profile> tags;
            auto usb_spec = get_usb_spec();
            if (usb_spec >= platform::usb3_type || usb_spec == platform::usb_undefined)
            {
                tags.push_back({ RS2_STREAM_DEPTH, -1, 848, 480, RS2_FORMAT_Z16, 30, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
                tags.push_back({ RS2_STREAM_INFRARED, 1, 848, 480, RS2_FORMAT_Y8, 30, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
                tags.push_back({ RS2_STREAM_INFRARED, 2, 848, 480, RS2_FORMAT_Y8, 30, profile_tag::PROFILE_TAG_SUPERSET });
            }
            else
            {
                tags.push_back({ RS2_STREAM_DEPTH, -1, 640, 480, RS2_FORMAT_Z16, 15, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
                tags.push_back({ RS2_STREAM_INFRARED, 1, 640, 480, RS2_FORMAT_Y8, 15, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
                tags.push_back({ RS2_STREAM_INFRARED, 2, 640, 480, RS2_FORMAT_Y8, 15, profile_tag::PROFILE_TAG_SUPERSET });
            }
            return tags;
        };
    };

    class rs430i_device : public d400_active,
                          public ds_advanced_mode_base,
                          public d400_motion,
                          public firmware_logger_device
    {
    public:
        rs430i_device( std::shared_ptr< const d400_info > const & dev_info, bool register_device_notifications )
            : device( dev_info, register_device_notifications )
            , backend_device( dev_info, register_device_notifications )
            , d400_device( dev_info )
            , d400_active( dev_info )
            , ds_advanced_mode_base()
            , d400_motion( dev_info )
            , firmware_logger_device( dev_info, d400_device::_hw_monitor, get_firmware_logs_command(), get_flash_logs_command() )
        {
            ds_advanced_mode_base::initialize_advanced_mode( this );
        }

        std::vector<tagged_profile> get_profiles_tags() const override
        {
            std::vector<tagged_profile> tags;
            auto usb_spec = get_usb_spec();
            if (usb_spec >= platform::usb3_type || usb_spec == platform::usb_undefined)
            {
                tags.push_back({ RS2_STREAM_DEPTH, -1, 848, 480, RS2_FORMAT_Z16, 30, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
                tags.push_back({ RS2_STREAM_INFRARED, 1, 848, 480, RS2_FORMAT_Y8, 30, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
                tags.push_back({ RS2_STREAM_INFRARED, 2, 848, 480, RS2_FORMAT_Y8, 30, profile_tag::PROFILE_TAG_SUPERSET });
            }
            else
            {
                tags.push_back({ RS2_STREAM_DEPTH, -1, 640, 480, RS2_FORMAT_Z16, 15, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
                tags.push_back({ RS2_STREAM_INFRARED, 1, 640, 480, RS2_FORMAT_Y8, 15, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
                tags.push_back({ RS2_STREAM_INFRARED, 2, 640, 480, RS2_FORMAT_Y8, 15, profile_tag::PROFILE_TAG_SUPERSET });
            }
            tags.push_back({ RS2_STREAM_GYRO, -1, 0, 0, RS2_FORMAT_MOTION_XYZ32F, (int)odr::IMU_FPS_200, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
            tags.push_back({ RS2_STREAM_ACCEL, -1, 0, 0, RS2_FORMAT_MOTION_XYZ32F, (int)odr::IMU_FPS_63, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
            tags.push_back({ RS2_STREAM_ACCEL, -1, 0, 0, RS2_FORMAT_MOTION_XYZ32F, (int)odr::IMU_FPS_100, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });

            return tags;
        };
        std::shared_ptr<matcher> create_matcher(const frame_holder& frame) const override;
    };

    // AWGT
    class rs430_mm_device : public d400_active,
                            public d400_motion,
                            public ds_advanced_mode_base,
                            public firmware_logger_device
    {
    public:
        rs430_mm_device( std::shared_ptr< const d400_info > const & dev_info, bool register_device_notifications )
            : device( dev_info, register_device_notifications )
            , backend_device( dev_info, register_device_notifications )
            , d400_device( dev_info )
            , d400_active( dev_info )
            , d400_motion( dev_info )
            , ds_advanced_mode_base()
            , firmware_logger_device( dev_info, d400_device::_hw_monitor, get_firmware_logs_command(), get_flash_logs_command() )
        {
            ds_advanced_mode_base::initialize_advanced_mode( this );
        }

        std::shared_ptr<matcher> create_matcher(const frame_holder& frame) const override;

        std::vector<tagged_profile> get_profiles_tags() const override
        {
            std::vector<tagged_profile> tags;
            auto usb_spec = get_usb_spec();
            if (usb_spec >= platform::usb3_type || usb_spec == platform::usb_undefined)
            {
                tags.push_back({ RS2_STREAM_DEPTH, -1, 848, 480, RS2_FORMAT_Z16, 30, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
                tags.push_back({ RS2_STREAM_INFRARED, 1, 848, 480, RS2_FORMAT_Y8, 30, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
                tags.push_back({ RS2_STREAM_INFRARED, 2, 848, 480, RS2_FORMAT_Y8, 30, profile_tag::PROFILE_TAG_SUPERSET });

            }
            else
            {
                tags.push_back({ RS2_STREAM_DEPTH, -1, 640, 480, RS2_FORMAT_Z16, 15, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
                tags.push_back({ RS2_STREAM_INFRARED, 1, 640, 480, RS2_FORMAT_Y8, 15, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
                tags.push_back({ RS2_STREAM_INFRARED, 2, 640, 480, RS2_FORMAT_Y8, 15, profile_tag::PROFILE_TAG_SUPERSET });
            }

            tags.push_back({ RS2_STREAM_FISHEYE, -1, 640, 480, RS2_FORMAT_RAW8, 30, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT});
            if (_fw_version >= firmware_version("5.10.4.0")) // Back-compat with records is required
            {
                tags.push_back({RS2_STREAM_GYRO, -1, 0, 0, RS2_FORMAT_MOTION_XYZ32F, (int)odr::IMU_FPS_200, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
                tags.push_back({RS2_STREAM_ACCEL, -1, 0, 0, RS2_FORMAT_MOTION_XYZ32F, (int)odr::IMU_FPS_63, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
                tags.push_back({RS2_STREAM_ACCEL, -1, 0, 0, RS2_FORMAT_MOTION_XYZ32F, (int)odr::IMU_FPS_100, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
            }

            return tags;
        };
    };

    // AWGC
    class rs435_device : public d400_active,
                         public d400_color,
                         public ds_advanced_mode_base,
                         public firmware_logger_device
    {
    public:
        rs435_device( std::shared_ptr< const d400_info > const & dev_info, bool register_device_notifications )
            : device( dev_info, register_device_notifications )
            , backend_device( dev_info, register_device_notifications )
            , d400_device( dev_info )
            , d400_active( dev_info )
            , d400_color( dev_info )
            , ds_advanced_mode_base()
            , firmware_logger_device( dev_info, d400_device::_hw_monitor, get_firmware_logs_command(), get_flash_logs_command() )
        {
            ds_advanced_mode_base::initialize_advanced_mode( this );
        }

        std::shared_ptr<matcher> create_matcher(const frame_holder& frame) const override;

        std::vector<tagged_profile> get_profiles_tags() const override
        {
            std::vector<tagged_profile> tags;
            auto usb_spec = get_usb_spec();
            if (usb_spec >= platform::usb3_type || usb_spec == platform::usb_undefined)
            {
                tags.push_back({ RS2_STREAM_COLOR, -1, 640, 480, get_color_format(), 30, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
                tags.push_back({ RS2_STREAM_DEPTH, -1, 848, 480, RS2_FORMAT_Z16, 30, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
                tags.push_back({ RS2_STREAM_INFRARED, -1, 848, 480, RS2_FORMAT_Y8, 30, profile_tag::PROFILE_TAG_SUPERSET });
            }
            else
            {
                tags.push_back({ RS2_STREAM_COLOR, -1, 640, 480, get_color_format(), 15, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
                tags.push_back({ RS2_STREAM_DEPTH, -1, 640, 480, RS2_FORMAT_Z16, 15, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
                tags.push_back({ RS2_STREAM_INFRARED, -1, 640, 480, RS2_FORMAT_Y8, 15, profile_tag::PROFILE_TAG_SUPERSET });
            }
            return tags;
        };
    };

    // D457 Development
    class rs457_device : public d400_active,
                         public d400_color,
                         public d400_motion_uvc,
                         public d400_mipi_device,
                         public firmware_logger_device
    {
    public:
        rs457_device( std::shared_ptr< const d400_info > const & dev_info, bool register_device_notifications )
            : device( dev_info, register_device_notifications )
            , backend_device( dev_info, register_device_notifications )
            , d400_device( dev_info )
            , d400_active( dev_info )
            , d400_color( dev_info )
            , d400_motion_uvc( dev_info )
            , d400_mipi_device()
            , firmware_logger_device( dev_info, d400_device::_hw_monitor, get_firmware_logs_command(), get_flash_logs_command() )
        {
        }

        std::shared_ptr<matcher> create_matcher(const frame_holder& frame) const override;

        std::vector<tagged_profile> get_profiles_tags() const override
        {
            std::vector<tagged_profile> tags;

            tags.push_back({ RS2_STREAM_COLOR, -1, 640, 480, get_color_format(), 30, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
            tags.push_back({ RS2_STREAM_DEPTH, -1, 640, 480, RS2_FORMAT_Z16, 30, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
            tags.push_back({ RS2_STREAM_GYRO, -1, 0, 0, RS2_FORMAT_MOTION_XYZ32F, (int)odr::IMU_FPS_100, profile_tag::PROFILE_TAG_SUPERSET  | profile_tag::PROFILE_TAG_DEFAULT });
            tags.push_back({RS2_STREAM_ACCEL, -1, 0, 0, RS2_FORMAT_MOTION_XYZ32F, (int)odr::IMU_FPS_63, profile_tag::PROFILE_TAG_SUPERSET });
            tags.push_back({ RS2_STREAM_ACCEL, -1, 0, 0, RS2_FORMAT_MOTION_XYZ32F, (int)odr::IMU_FPS_100, profile_tag::PROFILE_TAG_SUPERSET  | profile_tag::PROFILE_TAG_DEFAULT });

            return tags;
        };
    };


    class rs430_gmsl_device : public d400_active,
                              public d400_motion_uvc,
                              public d400_mipi_device,
                              public firmware_logger_device
    {
    public:
        rs430_gmsl_device( std::shared_ptr< const d400_info > const & dev_info, bool register_device_notifications )
            : device( dev_info, register_device_notifications )
            , backend_device( dev_info, register_device_notifications )
            , d400_device( dev_info )
            , d400_active( dev_info )
            , d400_motion_uvc( dev_info )
            , d400_mipi_device()
            , firmware_logger_device( dev_info, d400_device::_hw_monitor, get_firmware_logs_command(), get_flash_logs_command() )
        {
        }

        std::shared_ptr<matcher> create_matcher(const frame_holder& frame) const override;

        std::vector<tagged_profile> get_profiles_tags() const override
        {
            std::vector<tagged_profile> tags;

            tags.push_back({ RS2_STREAM_DEPTH, -1, 640, 480, RS2_FORMAT_Z16, 30, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
            tags.push_back({ RS2_STREAM_GYRO, -1, 0, 0, RS2_FORMAT_MOTION_XYZ32F, (int)odr::IMU_FPS_100, profile_tag::PROFILE_TAG_SUPERSET });
            tags.push_back({ RS2_STREAM_ACCEL, -1, 0, 0, RS2_FORMAT_MOTION_XYZ32F, (int)odr::IMU_FPS_100, profile_tag::PROFILE_TAG_SUPERSET });

            return tags;
        };
    };

    // AWGCT
    class rs430_rgb_mm_device : public d400_active,
                                public d400_color,
                                public d400_motion,
                                public ds_advanced_mode_base,
                                public firmware_logger_device
    {
    public:
        rs430_rgb_mm_device( std::shared_ptr< const d400_info > const & dev_info, bool register_device_notifications )
            : device( dev_info, register_device_notifications )
            , backend_device( dev_info, register_device_notifications )
            , d400_device( dev_info )
            , d400_active( dev_info )
            , d400_color( dev_info )
            , d400_motion( dev_info )
            , ds_advanced_mode_base()
            , firmware_logger_device( dev_info, d400_device::_hw_monitor, get_firmware_logs_command(), get_flash_logs_command() )
        {
            ds_advanced_mode_base::initialize_advanced_mode( this );
        }

        std::shared_ptr<matcher> create_matcher(const frame_holder& frame) const override;

        std::vector<tagged_profile> get_profiles_tags() const override
        {
            std::vector<tagged_profile> tags;
            auto usb_spec = get_usb_spec();
            if (usb_spec >= platform::usb3_type || usb_spec == platform::usb_undefined)
            {
                tags.push_back({ RS2_STREAM_COLOR, -1, 640, 480, get_color_format(), 30, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
                tags.push_back({ RS2_STREAM_DEPTH, -1, 848, 480, RS2_FORMAT_Z16, 30, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
                tags.push_back({ RS2_STREAM_INFRARED, -1, 848, 480, RS2_FORMAT_Y8, 30, profile_tag::PROFILE_TAG_SUPERSET });
            }
            else
            {
                tags.push_back({ RS2_STREAM_COLOR, -1, 640, 480, get_color_format(), 15, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
                tags.push_back({ RS2_STREAM_DEPTH, -1, 640, 480, RS2_FORMAT_Z16, 15, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
                tags.push_back({ RS2_STREAM_INFRARED, -1, 640, 480, RS2_FORMAT_Y8, 15, profile_tag::PROFILE_TAG_SUPERSET });
            }
            return tags;
        };
    };


    class rs435i_device : public d400_active,
                          public d400_color,
                          public d400_motion,
                          public ds_advanced_mode_base,
                          public firmware_logger_device
    {
    public:
        rs435i_device( std::shared_ptr< const d400_info > const & dev_info, bool register_device_notifications )
            : device( dev_info, register_device_notifications )
            , backend_device( dev_info, register_device_notifications )
            , d400_device( dev_info )
            , d400_active( dev_info )
            , d400_color( dev_info )
            , d400_motion( dev_info )
            , ds_advanced_mode_base()
            , firmware_logger_device( dev_info, d400_device::_hw_monitor, get_firmware_logs_command(), get_flash_logs_command() )
        {
            ds_advanced_mode_base::initialize_advanced_mode( this );
#if !defined(__APPLE__) // Motion sensors not supported on macOS
            if( _fw_version >= firmware_version( 5, 16, 0, 0 ) )
                register_feature(
                    std::make_shared< gyro_sensitivity_feature >( get_raw_motion_sensor(), get_motion_sensor() ) );
#endif
        }


        std::shared_ptr<matcher> create_matcher(const frame_holder& frame) const override;

        std::vector<tagged_profile> get_profiles_tags() const override
        {
            std::vector<tagged_profile> tags;
            auto usb_spec = get_usb_spec();
            bool usb3mode = (usb_spec >= platform::usb3_type || usb_spec == platform::usb_undefined);

            int depth_width  = usb3mode ?      848 : 640;
            int depth_height = usb3mode ?      480 : 480;
            int color_width = usb3mode ?       1280 : 640;
            int color_height = usb3mode ?      720 : 480;
            int fps    = usb3mode ?            30 :  15;

            tags.push_back({ RS2_STREAM_COLOR, -1, color_width, color_height, get_color_format(), fps, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
            tags.push_back({ RS2_STREAM_DEPTH, -1, depth_width, depth_height, RS2_FORMAT_Z16, fps, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
            tags.push_back({ RS2_STREAM_INFRARED, -1, depth_width, depth_height, RS2_FORMAT_Y8, fps, profile_tag::PROFILE_TAG_SUPERSET });
            tags.push_back({RS2_STREAM_GYRO, -1, 0, 0, RS2_FORMAT_MOTION_XYZ32F, (int)odr::IMU_FPS_200, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
            tags.push_back({RS2_STREAM_ACCEL, -1, 0, 0, RS2_FORMAT_MOTION_XYZ32F, (int)odr::IMU_FPS_63, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
            tags.push_back({RS2_STREAM_ACCEL, -1, 0, 0, RS2_FORMAT_MOTION_XYZ32F, (int)odr::IMU_FPS_100, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
            return tags;
        };
        bool compress_while_record() const override { return false; }
    };

    class rs436_device : public d400_active,
                         public d400_color,
                         public d400_motion,
                         public ds_advanced_mode_base,
                         public firmware_logger_device
    {
    public:
        rs436_device( std::shared_ptr< const d400_info > const & dev_info, bool register_device_notifications )
            : device( dev_info, register_device_notifications )
            , backend_device( dev_info, register_device_notifications )
            , d400_device( dev_info )
            , d400_active( dev_info )
            , d400_color( dev_info )
            , d400_motion( dev_info )
            , ds_advanced_mode_base()
            , firmware_logger_device( dev_info, d400_device::_hw_monitor, get_firmware_logs_command(), get_flash_logs_command())
        {
            ds_advanced_mode_base::initialize_advanced_mode( this );
            register_feature(std::make_shared< gyro_sensitivity_feature >(get_raw_motion_sensor(), get_motion_sensor()));
        }


        std::shared_ptr<matcher> create_matcher(const frame_holder& frame) const override;

        std::vector<tagged_profile> get_profiles_tags() const override
        {
            std::vector<tagged_profile> tags;
            auto usb_spec = get_usb_spec();
            bool usb3mode = (usb_spec >= platform::usb3_type || usb_spec == platform::usb_undefined);

            int depth_width = usb3mode ? 848 : 640;
            int depth_height = usb3mode ? 480 : 480;
            int color_width = usb3mode ? 1280 : 640;
            int color_height = usb3mode ? 720 : 480;
            int fps = usb3mode ? 30 : 15;

            tags.push_back({ RS2_STREAM_COLOR, -1, color_width, color_height, get_color_format(), fps, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
            tags.push_back({ RS2_STREAM_DEPTH, -1, depth_width, depth_height, RS2_FORMAT_Z16, fps, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
            tags.push_back({ RS2_STREAM_INFRARED, -1, depth_width, depth_height, RS2_FORMAT_Y8, fps, profile_tag::PROFILE_TAG_SUPERSET });
            tags.push_back({ RS2_STREAM_GYRO, -1, 0, 0, RS2_FORMAT_MOTION_XYZ32F, (int)odr::IMU_FPS_200, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
            tags.push_back({ RS2_STREAM_ACCEL, -1, 0, 0, RS2_FORMAT_MOTION_XYZ32F, (int)odr::IMU_FPS_63, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
            tags.push_back({ RS2_STREAM_ACCEL, -1, 0, 0, RS2_FORMAT_MOTION_XYZ32F, (int)odr::IMU_FPS_100, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
            return tags;
        };
        bool compress_while_record() const override { return false; }
    };

    class rs400_imu_device : public d400_motion,
                             public ds_advanced_mode_base,
                             public firmware_logger_device
    {
    public:
        rs400_imu_device( std::shared_ptr< const d400_info > const & dev_info, bool register_device_notifications )
            : device( dev_info, register_device_notifications )
            , backend_device( dev_info, register_device_notifications )
            , d400_device( dev_info )
            , d400_motion( dev_info )
            , ds_advanced_mode_base()
            , firmware_logger_device( dev_info, d400_device::_hw_monitor, get_firmware_logs_command(), get_flash_logs_command() )
        {
            ds_advanced_mode_base::initialize_advanced_mode( this );
        }

        std::shared_ptr<matcher> create_matcher(const frame_holder& frame) const override;

        std::vector<tagged_profile> get_profiles_tags() const override
        {
            std::vector<tagged_profile> tags;
            tags.push_back({RS2_STREAM_GYRO, -1, 0, 0, RS2_FORMAT_MOTION_XYZ32F, (int)odr::IMU_FPS_200, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
            tags.push_back({RS2_STREAM_ACCEL, -1, 0, 0, RS2_FORMAT_MOTION_XYZ32F, (int)odr::IMU_FPS_63, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
            tags.push_back({RS2_STREAM_ACCEL, -1, 0, 0, RS2_FORMAT_MOTION_XYZ32F, (int)odr::IMU_FPS_100, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });

            return tags;
        };
    };

    class rs405_device : public d400_color,
                         public d400_nonmonochrome,
                         public ds_advanced_mode_base,
                         public firmware_logger_device
    {
    public:
        rs405_device( std::shared_ptr< const d400_info > const & dev_info, bool register_device_notifications )
            : device( dev_info, register_device_notifications )
            , backend_device( dev_info, register_device_notifications )
            , d400_device( dev_info )
            , d400_color( dev_info )
            , d400_nonmonochrome( dev_info )
            , ds_advanced_mode_base()
            , firmware_logger_device( dev_info, d400_device::_hw_monitor, get_firmware_logs_command(), get_flash_logs_command() )
        {
            ds_advanced_mode_base::initialize_advanced_mode( this );
        }

        std::shared_ptr<matcher> create_matcher(const frame_holder& frame) const override;

        std::vector<tagged_profile> get_profiles_tags() const override
        {
            std::vector<tagged_profile> tags;
            auto usb_spec = get_usb_spec();
            bool usb3mode = (usb_spec >= platform::usb3_type || usb_spec == platform::usb_undefined);

            int depth_width = 848;
            int depth_height = 480;
            int color_width = 848;
            int color_height = 480;
            int fps = usb3mode ?  30 : 10;

            tags.push_back( { RS2_STREAM_COLOR,
                              -1,  // index
                              color_width, color_height,
                              get_color_format(),
                              fps,
                              profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT } );
            tags.push_back( { RS2_STREAM_DEPTH,
                              -1,  // index
                              depth_width, depth_height,
                              RS2_FORMAT_Z16,
                              fps,
                              profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT } );
            tags.push_back( { RS2_STREAM_INFRARED,
                              -1,  // index
                              depth_width, depth_height,
                              get_ir_format(),
                              fps,
                              profile_tag::PROFILE_TAG_SUPERSET } );

            return tags;
        }

        bool compress_while_record() const override { return false; }

        bool contradicts(const stream_profile_interface* a, const std::vector<stream_profile>& others) const override
        {
            auto it = std::find_if(others.begin(), others.end(), [](const stream_profile& sp) {
                return sp.format == RS2_FORMAT_Y16;
                });
            bool unrectified_ir_requested = (it != others.end());

            if (auto vid_a = dynamic_cast<const video_stream_profile_interface*>(a))
            {
                for (auto request : others)
                {
                    if (a->get_framerate() != 0 && request.fps != 0 && (a->get_framerate() != request.fps))
                        return true;
                    if (!unrectified_ir_requested)
                    {
                        if (vid_a->get_width() != 0 && request.width != 0 && (vid_a->get_width() != request.width))
                            return true;
                        if (vid_a->get_height() != 0 && request.height != 0 && (vid_a->get_height() != request.height))
                            return true;
                    }
                }
            }
            return false;
        }
    };

    class rs455_device : public d400_nonmonochrome,
                         public d400_active,
                         public d400_color,
                         public d400_motion,
                         public ds_advanced_mode_base,
                         public firmware_logger_device,
                         public ds_thermal_tracking
    {
    public:
        rs455_device( std::shared_ptr< const d400_info > const & dev_info, bool register_device_notifications )
            : device( dev_info, register_device_notifications )
            , backend_device( dev_info, register_device_notifications )
            , d400_device( dev_info )
            , d400_nonmonochrome( dev_info )
            , d400_active( dev_info )
            , d400_color( dev_info )
            , d400_motion( dev_info )
            , ds_advanced_mode_base()
            , firmware_logger_device( dev_info, d400_device::_hw_monitor, get_firmware_logs_command(), get_flash_logs_command() )
            , ds_thermal_tracking( d400_device::_thermal_monitor )
        {
            ds_advanced_mode_base::initialize_advanced_mode( this );
#if !defined(__APPLE__) // Motion sensors not supported on macOS
            if( _fw_version >= firmware_version( 5, 16, 0, 0 ) )
                register_feature(
                    std::make_shared< gyro_sensitivity_feature >( get_raw_motion_sensor(), get_motion_sensor() ) );
#endif
        }

        std::shared_ptr<matcher> create_matcher(const frame_holder& frame) const override;

        std::vector<tagged_profile> get_profiles_tags() const override
        {
            std::vector<tagged_profile> tags;
            auto usb_spec = get_usb_spec();
            bool usb3mode = (usb_spec >= platform::usb3_type || usb_spec == platform::usb_undefined);

            int depth_width  = usb3mode ?    848 : 640;
            int depth_height = usb3mode ?    480 : 480;
            int color_width  = usb3mode ?   1280 : 640;
            int color_height = usb3mode ?    720 : 480;
            int fps          = usb3mode ?     30 :  15;

            tags.push_back({ RS2_STREAM_COLOR, -1, color_width, color_height, get_color_format(), fps, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
            tags.push_back({ RS2_STREAM_DEPTH, -1, depth_width, depth_height, RS2_FORMAT_Z16, fps, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
            tags.push_back({ RS2_STREAM_INFRARED, -1, depth_width, depth_height, get_ir_format(), fps, profile_tag::PROFILE_TAG_SUPERSET });
            tags.push_back({RS2_STREAM_GYRO, -1, 0, 0, RS2_FORMAT_MOTION_XYZ32F, (int)odr::IMU_FPS_200, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
            tags.push_back({RS2_STREAM_ACCEL, -1, 0, 0, RS2_FORMAT_MOTION_XYZ32F, (int)odr::IMU_FPS_63, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });
            tags.push_back({RS2_STREAM_ACCEL, -1, 0, 0, RS2_FORMAT_MOTION_XYZ32F, (int)odr::IMU_FPS_100, profile_tag::PROFILE_TAG_SUPERSET | profile_tag::PROFILE_TAG_DEFAULT });

            return tags;
        }

        bool compress_while_record() const override { return false; }

    };


    std::shared_ptr< device_interface > d400_info::create_device()
    {
        using namespace ds;

        if( _group.uvc_devices.empty() )
            throw std::runtime_error("Depth Camera not found!");

        auto const dev_info = std::dynamic_pointer_cast< const d400_info >( shared_from_this() );
        bool const register_device_notifications = true;

        auto pid = _group.uvc_devices.front().pid;

        try
        {
            switch( pid )
            {
            case RS400_PID:
                return std::make_shared< rs400_device >( dev_info, register_device_notifications );
            case RS405U_PID:
                return std::make_shared< rs405u_device >( dev_info, register_device_notifications );
            case RS410_PID:
            case RS460_PID:
                return std::make_shared< rs410_device >( dev_info, register_device_notifications );
            case RS415_PID:
                return std::make_shared< rs415_device >( dev_info, register_device_notifications );
            case RS416_PID:
                return std::make_shared< rs416_device >( dev_info, register_device_notifications );
            case RS416_RGB_PID:
                return std::make_shared< rs416_rgb_device >( dev_info, register_device_notifications );
            case RS420_PID:
                return std::make_shared< rs420_device >( dev_info, register_device_notifications );
            case RS421_PID:
                return std::make_shared< rs421_device >( dev_info, register_device_notifications );
            case RS420_MM_PID:
                return std::make_shared< rs420_mm_device >( dev_info, register_device_notifications );
            case RS430_PID:
                return std::make_shared< rs430_device >( dev_info, register_device_notifications );
            case RS430I_PID:
                return std::make_shared< rs430i_device >( dev_info, register_device_notifications );
            case RS430_MM_PID:
                return std::make_shared< rs430_mm_device >( dev_info, register_device_notifications );
            case RS430_MM_RGB_PID:
                return std::make_shared< rs430_rgb_mm_device >( dev_info, register_device_notifications );
            case RS435_RGB_PID:
                return std::make_shared< rs435_device >( dev_info, register_device_notifications );
            case RS435I_PID:
                return std::make_shared< rs435i_device >( dev_info, register_device_notifications );
            case RS436_PID:
                return std::make_shared< rs436_device >( dev_info, register_device_notifications );
            case RS_USB2_PID:
                return std::make_shared< rs410_device >( dev_info, register_device_notifications );
            case RS400_IMU_PID:
                return std::make_shared< rs400_imu_device >( dev_info, register_device_notifications );
            case RS405_PID:
                return std::make_shared< rs405_device >( dev_info, register_device_notifications );
            case RS455_PID:
                return std::make_shared< rs455_device >( dev_info, register_device_notifications );
            case RS457_PID:
                return std::make_shared< rs457_device >( dev_info, register_device_notifications );
            case RS430_GMSL_PID:
                return std::make_shared< rs430_gmsl_device >( dev_info, register_device_notifications );
            case RS415_GMSL_PID:
                return std::make_shared< rs415_gmsl_device >( dev_info, register_device_notifications );
            case RS401_GMSL_PID:
                return std::make_shared< rs401_gmsl_device >( dev_info, register_device_notifications );
            default:
                throw std::runtime_error( rsutils::string::from()
                                          << "Unsupported RS400 model! 0x" << std::hex << std::setw( 4 )
                                          << std::setfill( '0' ) << (int)pid );
            }
        }
        catch( const std::exception& e )
        {
            auto ctx = get_context();
            if( ctx && is_partial_device_allowed( ctx ) )
            {
                LOG_ERROR( rsutils::string::from() << "Failed to create device for PID 0x" << std::hex << std::setw( 4 )
                                                   << std::setfill( '0' ) << (int)pid << "! (" << e.what() << ")" );
                // Create a device with partial capabilities when allowed instead of failing
                return std::make_shared< rs400_device >( dev_info, register_device_notifications );
            }
            else
                throw; // rethrowing exception
        }
    }

    std::vector<std::shared_ptr<d400_info>> d400_info::pick_d400_devices(
        std::shared_ptr<context> ctx,
        platform::backend_device_group& group)
    {
        std::vector<platform::uvc_device_info> chosen;
        std::vector<std::shared_ptr<d400_info>> results;

        auto valid_pid = filter_by_product(group.uvc_devices, ds::rs400_sku_pid);
        auto group_devices = group_devices_and_hids_by_unique_id(group_devices_by_unique_id(valid_pid), group.hid_devices);

        for (auto& g : group_devices)
        {
            auto& devices = g.first;
            auto& hids = g.second;

            bool is_mi_0_present = mi_present(devices, 0);

            // Device with multi sensors can be enabled only if all sensors (RGB + Depth) are present
            auto is_pid_of_multisensor_device = [](int pid) { return std::find(std::begin(ds::d400_multi_sensors_pid), std::end(ds::d400_multi_sensors_pid), pid) != std::end(ds::d400_multi_sensors_pid); };
            auto is_pid_of_mipi_device = [](int pid) { return std::find(std::begin(ds::d400_mipi_device_pid), std::end(ds::d400_mipi_device_pid), pid) != std::end(ds::d400_mipi_device_pid); };

#if !defined(__APPLE__) // Not supported by macos
            auto is_pid_of_hid_sensor_device = [](int pid) { return std::find(std::begin(ds::d400_hid_sensors_pid),
                std::end(ds::d400_hid_sensors_pid), pid) != std::end(ds::d400_hid_sensors_pid); };
            bool is_device_hid_sensor = false;
            for (auto&& uvc : devices)
            {
                if (is_pid_of_hid_sensor_device(uvc.pid))
                    is_device_hid_sensor = true;
            }
#endif

            if (!devices.empty() && is_mi_0_present)
            {
                platform::usb_device_info hwm;

                std::vector<platform::usb_device_info> hwm_devices;
                if (ds::d400_try_fetch_usb_device(group.usb_devices, devices.front(), hwm))
                {
                    hwm_devices.push_back(hwm);
                }

                auto info = std::make_shared<d400_info>( ctx, std::move( devices ), std::move( hwm_devices ), std::move( hids ) );
                chosen.insert(chosen.end(), devices.begin(), devices.end());
                results.push_back(info);

            }
        }

        trim_device_list(group.uvc_devices, chosen);

        return results;
    }


    inline std::shared_ptr<matcher> create_composite_matcher(std::vector<std::shared_ptr<matcher>> matchers)
    {
        return std::make_shared<timestamp_composite_matcher>(matchers);
    }

    std::shared_ptr<matcher> rs400_device::create_matcher(const frame_holder& frame) const
    {
        std::vector< stream_interface * > streams;
        if( _depth_stream )
            streams.push_back( _depth_stream.get() );
        if( _left_ir_stream )
            streams.push_back( _left_ir_stream.get() );
        if( _right_ir_stream )
            streams.push_back( _right_ir_stream.get() );
        return matcher_factory::create(RS2_MATCHER_DEFAULT, streams);
    }

    std::shared_ptr<matcher> rs401_gmsl_device::create_matcher(const frame_holder& frame) const
    {
        std::vector<stream_interface*> streams = { _depth_stream.get() , _left_ir_stream.get() , _right_ir_stream.get(), _color_stream.get() };
        return matcher_factory::create(RS2_MATCHER_DEFAULT, streams);
    }

    std::shared_ptr<matcher> rs405u_device::create_matcher(const frame_holder& frame) const
    {
        std::vector<stream_interface*> streams = { _depth_stream.get() , _left_ir_stream.get() , _right_ir_stream.get()};
        return matcher_factory::create(RS2_MATCHER_DEFAULT, streams);
    }

    std::shared_ptr<matcher> rs410_device::create_matcher(const frame_holder& frame) const
    {
        std::vector<stream_interface*> streams = { _depth_stream.get() , _left_ir_stream.get() , _right_ir_stream.get()};
        return matcher_factory::create(RS2_MATCHER_DEFAULT, streams);
    }

    std::shared_ptr<matcher> rs415_device::create_matcher(const frame_holder& frame) const
    {
        std::vector<stream_interface*> streams = { _depth_stream.get() , _left_ir_stream.get() , _right_ir_stream.get(), _color_stream.get() };
        return matcher_factory::create(RS2_MATCHER_DEFAULT, streams);
    }

    std::shared_ptr<matcher> rs415_gmsl_device::create_matcher(const frame_holder& frame) const
    {
        std::vector<stream_interface*> streams = { _depth_stream.get() , _left_ir_stream.get() , _right_ir_stream.get(), _color_stream.get() };
        return matcher_factory::create(RS2_MATCHER_DEFAULT, streams);
    }

    std::shared_ptr<matcher> rs416_device::create_matcher(const frame_holder& frame) const
    {
        std::vector<stream_interface*> streams = { _depth_stream.get() , _left_ir_stream.get() , _right_ir_stream.get() };
        return matcher_factory::create(RS2_MATCHER_DEFAULT, streams);
    }

    std::shared_ptr<matcher> rs416_rgb_device::create_matcher(const frame_holder& frame) const
    {
        std::vector<stream_interface*> streams = { _depth_stream.get() , _left_ir_stream.get() , _right_ir_stream.get(), _color_stream.get() };
        return matcher_factory::create(RS2_MATCHER_DEFAULT, streams);
    }

    std::shared_ptr<matcher> rs420_mm_device::create_matcher(const frame_holder& frame) const
    {
        //TODO: add matcher to mm
        std::vector<stream_interface*> streams = { _depth_stream.get() , _left_ir_stream.get() , _right_ir_stream.get()};
        std::vector<stream_interface*> mm_streams = {
            _ds_motion_common->get_fisheye_stream().get(),
            _ds_motion_common->get_accel_stream().get(),
            _ds_motion_common->get_gyro_stream().get()
        };
        streams.insert(streams.end(), mm_streams.begin(), mm_streams.end());
        return matcher_factory::create(RS2_MATCHER_DEFAULT, streams);
    }

    std::shared_ptr<matcher> rs420_device::create_matcher(const frame_holder& frame) const
    {
        std::vector<stream_interface*> streams = { _depth_stream.get() , _left_ir_stream.get() , _right_ir_stream.get()};
        return matcher_factory::create(RS2_MATCHER_DEFAULT, streams);
    }

    std::shared_ptr< matcher > rs421_device::create_matcher( const frame_holder & frame ) const
    {
        std::vector< stream_interface * > streams = { _depth_stream.get(), _left_ir_stream.get(), _right_ir_stream.get() };
        return matcher_factory::create( RS2_MATCHER_DEFAULT, streams );
    }

    std::shared_ptr<matcher> rs430_device::create_matcher(const frame_holder& frame) const
    {
        std::vector<stream_interface*> streams = { _depth_stream.get() , _left_ir_stream.get() , _right_ir_stream.get() };
        return matcher_factory::create(RS2_MATCHER_DEFAULT, streams);
    }

    std::shared_ptr<matcher> rs430_mm_device::create_matcher(const frame_holder& frame) const
    {
        //TODO: add matcher to mm
        std::vector<stream_interface*> streams = { _depth_stream.get() , _left_ir_stream.get() , _right_ir_stream.get() };
        std::vector<stream_interface*> mm_streams = {
            _ds_motion_common->get_fisheye_stream().get(),
            _ds_motion_common->get_accel_stream().get(),
            _ds_motion_common->get_gyro_stream().get()
        };
        streams.insert(streams.end(), mm_streams.begin(), mm_streams.end());
        return matcher_factory::create(RS2_MATCHER_DEFAULT, streams);
    }

    std::shared_ptr<matcher> rs435_device::create_matcher(const frame_holder& frame) const
    {
        std::vector<stream_interface*> streams = { _depth_stream.get() , _left_ir_stream.get() , _right_ir_stream.get(), _color_stream.get() };
        return matcher_factory::create(RS2_MATCHER_DEFAULT, streams);
    }

    std::shared_ptr<matcher> rs430_rgb_mm_device::create_matcher(const frame_holder& frame) const
    {
        //TODO: add matcher to mm
        std::vector<stream_interface*> streams = { _depth_stream.get() , _left_ir_stream.get() , _right_ir_stream.get(), _color_stream.get() };
        std::vector<stream_interface*> mm_streams = {
            _ds_motion_common->get_fisheye_stream().get(),
            _ds_motion_common->get_accel_stream().get(),
            _ds_motion_common->get_gyro_stream().get()
        };
        streams.insert(streams.end(), mm_streams.begin(), mm_streams.end());
        return matcher_factory::create(RS2_MATCHER_DEFAULT, streams);
    }

    std::shared_ptr<matcher> rs430i_device::create_matcher(const frame_holder& frame) const
    {
        std::vector<stream_interface*> streams = { _depth_stream.get() , _left_ir_stream.get() , _right_ir_stream.get() };
        // TODO - A proper matcher for High-FPS sensor is required
        std::vector<stream_interface*> mm_streams = { _ds_motion_common->get_accel_stream().get(),
                                                      _ds_motion_common->get_gyro_stream().get() };
        streams.insert(streams.end(), mm_streams.begin(), mm_streams.end());
        return matcher_factory::create(RS2_MATCHER_DEFAULT, streams);
    }

    std::shared_ptr<matcher> rs435i_device::create_matcher(const frame_holder& frame) const
    {
        std::vector<stream_interface*> streams = { _depth_stream.get() , _left_ir_stream.get() , _right_ir_stream.get(), _color_stream.get() };
        // TODO - A proper matcher for High-FPS sensor is required
        std::vector<stream_interface*> mm_streams = { _ds_motion_common->get_accel_stream().get(), 
                                                      _ds_motion_common->get_gyro_stream().get()};
        streams.insert(streams.end(), mm_streams.begin(), mm_streams.end());
        return matcher_factory::create(RS2_MATCHER_DEFAULT, streams);
    }

    std::shared_ptr<matcher> rs436_device::create_matcher(const frame_holder& frame) const
    {
        std::vector<stream_interface*> streams = { _depth_stream.get() , _left_ir_stream.get() , _right_ir_stream.get(), _color_stream.get() };
        // TODO - A proper matcher for High-FPS sensor is required
        std::vector<stream_interface*> mm_streams = { _ds_motion_common->get_accel_stream().get(),
                                                      _ds_motion_common->get_gyro_stream().get() };
        streams.insert(streams.end(), mm_streams.begin(), mm_streams.end());
        return matcher_factory::create(RS2_MATCHER_DEFAULT, streams);
    }

    std::shared_ptr<matcher> rs457_device::create_matcher(const frame_holder& frame) const
    {
        std::vector<stream_interface*> streams = { _depth_stream.get() , _left_ir_stream.get() , _right_ir_stream.get(), _color_stream.get() };
        std::vector<stream_interface*> mm_streams = { _accel_stream.get(), _gyro_stream.get()};
        streams.insert(streams.end(), mm_streams.begin(), mm_streams.end());
        return matcher_factory::create(RS2_MATCHER_DEFAULT, streams);
    }


    std::shared_ptr<matcher> rs430_gmsl_device::create_matcher(const frame_holder& frame) const
    {
        std::vector<stream_interface*> streams = { _depth_stream.get() , _left_ir_stream.get() , _right_ir_stream.get() };
        std::vector<stream_interface*> mm_streams = { _accel_stream.get(), _gyro_stream.get()};
        streams.insert(streams.end(), mm_streams.begin(), mm_streams.end());
        return matcher_factory::create(RS2_MATCHER_DEFAULT, streams);
    }


    std::shared_ptr<matcher> rs400_imu_device::create_matcher(const frame_holder& frame) const
    {
        // TODO - A proper matcher for High-FPS sensor is required
        std::vector<stream_interface*> mm_streams = { _ds_motion_common->get_accel_stream().get(),
                                                      _ds_motion_common->get_gyro_stream().get()};
        return matcher_factory::create(RS2_MATCHER_DEFAULT, mm_streams);
    }

    std::shared_ptr<matcher> rs405_device::create_matcher(const frame_holder& frame) const
    {
        std::vector<stream_interface*> streams = { _depth_stream.get() , _left_ir_stream.get() , _right_ir_stream.get(), _color_stream.get() };
        return matcher_factory::create(RS2_MATCHER_DEFAULT, streams);
    }

    std::shared_ptr<matcher> rs455_device::create_matcher(const frame_holder& frame) const
    {
        std::vector<stream_interface*> streams = { _depth_stream.get() , _left_ir_stream.get() , _right_ir_stream.get(), _color_stream.get() };
        std::vector<stream_interface*> mm_streams = { _ds_motion_common->get_accel_stream().get(),
                                                      _ds_motion_common->get_gyro_stream().get()};
        streams.insert(streams.end(), mm_streams.begin(), mm_streams.end());
        return matcher_factory::create(RS2_MATCHER_DEFAULT, streams);
    }
}
