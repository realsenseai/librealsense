// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2015 RealSense, Inc. All Rights Reserved.

#include "profile.h"
#include "media/record/record_device.h"
#include "media/ros2/ros2_writer.h"

namespace librealsense
{
    namespace pipeline
    {
        profile::profile(std::shared_ptr<device_interface> dev,
            util::config config,
            const std::string& to_file) :
            _dev(dev), _to_file(to_file)
        {
            if (!to_file.empty())
            {
                if (!dev)
                    throw librealsense::invalid_value_exception("Failed to create a profile, device is null");

                _dev = std::make_shared<record_device>(dev, std::make_shared<ros2_writer>(to_file, dev->compress_while_record()));
            }
            _multistream = config.resolve(_dev.get());
        }

        std::shared_ptr<device_interface> profile::get_device()
        {
            //profile can be retrieved from a pipeline_config and pipeline::start()
            //either way, it is created by the pipeline

            //TODO: handle case where device has disconnected and reconnected
            //TODO: remember to recreate the device as record device in case of to_file.empty() == false
            if (!_dev)
            {
                throw std::runtime_error("Device is unavailable");
            }
            return _dev;
        }

        stream_profiles profile::get_active_streams() const
        {
            auto profiles_per_sensor = _multistream.get_profiles_per_sensor();
            stream_profiles profiles;
            for (auto&& kvp : profiles_per_sensor)
                for (auto&& p : kvp.second)
                    profiles.push_back(p);

            return profiles;
        }
    }
}
