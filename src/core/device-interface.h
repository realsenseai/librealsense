// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2023 RealSense, Inc. All Rights Reserved.

#pragma once

#include "info-interface.h"
#include "tagged-profile.h"
#include "stream-profile.h"

#include <src/core/has-features-interface.h>

#include <librealsense2/h/rs_sensor.h>
#include <functional>
#include <vector>
#include <memory>
#include <utility>  // std::pair


namespace librealsense {


class sensor_interface;
class matcher;
class frame_holder;
class context;
class device_info;
class stream_interface;
class stream_profile_interface;


// Base class for all devices in librealsense
//
class device_interface
    : public virtual info_interface
    , public virtual has_features_interface
    , public std::enable_shared_from_this< device_interface >
{
public:
    virtual ~device_interface() = default;

    // Return the context to which this device belongs
    // Typically, same as get_device_info()->context()
    //
    virtual std::shared_ptr< context > get_context() const = 0;

    // Returns the device_info object from which this device was created
    //
    virtual std::shared_ptr< const device_info > get_device_info() const = 0;

    virtual sensor_interface & get_sensor( size_t i ) = 0;
    virtual const sensor_interface & get_sensor( size_t i ) const = 0;
    virtual size_t get_sensors_count() const = 0;

    virtual void hardware_reset() = 0;

    virtual std::shared_ptr< matcher > create_matcher( const frame_holder & frame ) const = 0;

    virtual std::pair< uint32_t, rs2_extrinsics > get_extrinsics( const stream_interface & ) const = 0;

    virtual bool is_valid() const = 0;

    virtual bool is_in_recovery_mode() const = 0;

    virtual std::vector< tagged_profile > get_profiles_tags() const = 0;

    using stream_profiles = std::vector< std::shared_ptr< stream_profile_interface > >;
    virtual void tag_profiles( stream_profiles profiles ) const = 0;

    // Return true if this device should use compression when recording to a bag file
    //
    virtual bool compress_while_record() const = 0;

    virtual bool contradicts( const stream_profile_interface * a,
                              const std::vector< stream_profile > & others ) const = 0;

    // True when the pipeline must start each sensor right after opening it, rather than opening all sensors first
    virtual bool start_sensors_when_opened() const { return false; }
};


}  // namespace librealsense
