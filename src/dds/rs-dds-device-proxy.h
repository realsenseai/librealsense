// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2024 RealSense, Inc. All Rights Reserved.
#pragma once

#include <src/software-device.h>
#include <src/fw-update/fw-update-device-interface.h>
#include <src/core/debug.h>
#include "sid_index.h"
#include "rsdds-serializable.h"
#include <src/auto-calibrated-proxy.h>
#include <src/device-calibration.h>
#include <src/eth-config-device.h>
#include <src/core/advanced_mode.h>

#include <rsutils/json-fwd.h>
#include <memory>
#include <vector>
#include <set>


namespace realdds {
class dds_device;
class dds_stream;
class dds_video_stream;
class dds_motion_stream;
class dds_stream_profile;
}  // namespace realdds


namespace librealsense {


class stream;
class dds_sensor_proxy;
class stream_profile_interface;


// This is the rs2 device; it proxies to an actual DDS device that does all the actual
// work. For example:
//     auto dev_list = ctx.query_devices();
//     auto dev1 = dev_list[0];
//     auto dev2 = dev_list[0];
// dev1 and dev2 are two different rs2 devices, but they both go to the same DDS source!
//
class dds_device_proxy
    : public software_device
    , public debug_interface
    , public updatable                // unsigned, non-recovery-mode
    , public update_device_interface  // signed, recovery-mode
    , public auto_calibrated_proxy
    , public calibration_change_device
    , public eth_config_device
    , public ds_advanced_mode_base
{
    std::shared_ptr< realdds::dds_device > _dds_dev;
    std::map< std::string, std::vector< std::shared_ptr< stream_profile_interface > > > _stream_name_to_profiles;
    std::map< std::string, std::shared_ptr< librealsense::stream > > _stream_name_to_librs_stream;
    std::map< std::string, std::shared_ptr< dds_sensor_proxy > > _stream_name_to_owning_sensor;
    
    rsutils::subscription _calibration_changed_subscription;
    rsutils::subscription _metadata_subscription;

    int get_index_from_stream_name( const std::string & name ) const;
    int get_unique_index_in_sensor( const std::shared_ptr< dds_sensor_proxy > & sensor,
                                    const std::string & stream_name, rs2_stream stream_type ) const;
    void set_profile_intrinsics( std::shared_ptr< stream_profile_interface > const & profile,
                                 const std::shared_ptr< realdds::dds_stream > & stream ) const;
    void set_video_profile_intrinsics( std::shared_ptr< stream_profile_interface > const & profile,
                                       std::shared_ptr< const realdds::dds_video_stream > const & stream ) const;
    void set_motion_profile_intrinsics( std::shared_ptr< stream_profile_interface > profile,
                                       std::shared_ptr< realdds::dds_motion_stream > stream ) const;

public:
    dds_device_proxy( std::shared_ptr< const device_info > const &, std::shared_ptr< realdds::dds_device > const & dev );

    void tag_default_profile_of_stream( const std::shared_ptr< stream_profile_interface > & profile,
                                        const std::shared_ptr< const realdds::dds_stream > & stream ) const;

    std::shared_ptr< dds_sensor_proxy > create_sensor( std::string const & sensor_name, rs2_stream sensor_type );

    void tag_profiles( stream_profiles profiles ) const override;

    void hardware_reset() override;

    bool is_in_recovery_mode() const override;

    // open() only sets the requested profiles and start() streams; a second sensor's open-streams control would
    // revert the first sensor's streams to defaults unless the first is already streaming.
    bool start_sensors_when_opened() const override { return true; }

    // calibration_change_device
public:
    void register_calibration_change_callback( rs2_calibration_change_callback_sptr callback ) override
    {
        _calib_changed_callbacks.insert( callback );
    }

private:
    std::set< rs2_calibration_change_callback_sptr > _calib_changed_callbacks;

    // debug_interface
private:
    std::vector< uint8_t > send_receive_raw_data( const std::vector< uint8_t > & ) override;
    std::vector< uint8_t > build_command( uint32_t opcode,
                                          uint32_t param1 = 0,
                                          uint32_t param2 = 0,
                                          uint32_t param3 = 0,
                                          uint32_t param4 = 0,
                                          uint8_t const * data = nullptr,
                                          size_t dataLength = 0 ) const override;
    std::string get_opcode_string(int opcode) const override;

    // updatable: unsigned, non-recovery-mode
private:
    bool check_fw_compatibility( const std::vector< uint8_t > & image ) const override;
    void enter_update_state() const override {}
    std::vector< uint8_t > backup_flash( rs2_update_progress_callback_sptr ) override { return {}; }
    void update_flash( std::vector< uint8_t > const & image, rs2_update_progress_callback_sptr, int update_mode ) override;

    // update_device_interface: signed, recovery-mode
private:
    void update( const void * image, int image_size, rs2_update_progress_callback_sptr = nullptr ) const override;

    // eth_config_device
public:
    bool supports_ethernet_configuration() override;

    // ds_advanced_mode_base
private:
    void device_specific_initialization() override;
    void toggle_advanced_mode( bool enable ) override {}; // Cannot be toggled on DDS devices. Set in device info.
    std::vector<std::string> get_recommended_filters_names(const std::shared_ptr<realdds::dds_stream> stream) const;
};

}  // namespace librealsense