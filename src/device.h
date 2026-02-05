// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2015 RealSense, Inc. All Rights Reserved.

#pragma once

#include "basics.h"  // C4250
#include <src/core/device-interface.h>
#include <src/core/info.h>
#include <src/core/features-container.h>

#include "device-info.h"

#include <rsutils/lazy.h>
#include <rsutils/subscription.h>
#include <chrono>
#include <memory>
#include <vector>
#include <atomic>


namespace librealsense {


// Device profiles undergo format conversion before becoming available to the user. This is typically done within each
// sensor's init_stream_profiles(), but the behavior can be overriden by the user through the device JSON settings (see
// device ctor) to enable querying the device's raw or basic formats only.
// 
// Note that streaming may be available only with full (the default) profiles. The others are for inspection only, and
// can directly be queried in rs-enumerate-devices.
// 
// Note also that default profiles (those returned by get_profiles_tags()) must take the conversion method into account
// or profiles may not get tagged correctly in non-full modes.
//
enum class format_conversion
{
    // Report raw profiles as provided by the camera, without any manipulation whatsoever by librealsense.
    raw,

    // Take the raw profiles, perform the librealsense mappings, but then remove any "unwanted" mappings:
    //      - any conversion between formats is thrown away (e.g., YUYV->RGB8)
    //      - colored infrared is removed
    //      - interleaved (Y12I, Y8I) are kept (so become Y16 and Y8, respectively)
    // See formats_converter::drop_non_basic_formats()
    basic,

    // The default conversion mode: all the librealsense mappings are intact; the user will see profiles including
    // format conversions (e.g., YUYV->RGB8), etc.
    full
};


// Base implementation for most devices in librealsense. While it's not necessary to derive from this class, it greatly
// simplifies implementations.
//
class device
    : public virtual device_interface
    , public info_container
    , public features_container
{
public:
    virtual ~device();
    size_t get_sensors_count() const override;

    sensor_interface& get_sensor(size_t subdevice) override;
    const sensor_interface& get_sensor(size_t subdevice) const override;

    void hardware_reset() override;

    virtual std::shared_ptr<matcher> create_matcher(const frame_holder& frame) const override;

    size_t find_sensor_idx(const sensor_interface& s) const;

    std::shared_ptr< context > get_context() const override { return _dev_info->get_context(); }

    std::shared_ptr< const device_info > get_device_info() const override { return _dev_info; }

    std::pair<uint32_t, rs2_extrinsics> get_extrinsics(const stream_interface& stream) const override;

    bool is_valid() const override { return *_is_alive; }

    bool is_in_recovery_mode() const override { return false; }

    void tag_profiles(stream_profiles profiles) const override;

    virtual bool compress_while_record() const override { return true; }

    virtual bool contradicts(const stream_profile_interface* a, const std::vector<stream_profile>& others) const override;

    virtual void stop_activity() const;

    bool device_changed_notifications_on() const { return _device_change_subscription.is_active(); }

    format_conversion get_format_conversion() const;

protected:
    int add_sensor(const std::shared_ptr<sensor_interface>& sensor_base);
    int assign_sensor(const std::shared_ptr<sensor_interface>& sensor_base, uint8_t idx);
    void register_stream_to_extrinsic_group(const stream_interface& stream, uint32_t groupd_index);
    std::vector<rs2_format> map_supported_color_formats(rs2_format source_format, bool should_map_source_format = true
            , const std::vector<rs2_format>& undesired_formats = {});

    explicit device( std::shared_ptr< const device_info > const &, bool device_changed_notifications = true );

    std::map<int, std::pair<uint32_t, std::shared_ptr<const stream_interface>>> _extrinsics;

private:
    std::vector<std::shared_ptr<sensor_interface>> _sensors;
    std::shared_ptr< const device_info > _dev_info;
    std::shared_ptr< std::atomic< bool > > _is_alive;
    rsutils::subscription _device_change_subscription;
    rsutils::lazy< std::vector< tagged_profile > > _profiles_tags;
    rsutils::lazy< format_conversion > _format_conversion;
};


}
