// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2018 RealSense, Inc. All Rights Reserved.

#include "software-device.h"
#include "software-sensor.h"
#include "environment.h"
#include "core/matcher-factory.h"
#include "core/notification.h"
#include "stream.h"


namespace librealsense
{
    software_device::software_device( std::shared_ptr< const device_info > const & dev_info, bool device_changed_notifications )
        : device( dev_info, device_changed_notifications )
    {
    }

    librealsense::software_device::~software_device()
    {
        if (_user_destruction_callback)
            _user_destruction_callback->on_destruction();
    }

    software_sensor& software_device::add_software_sensor(const std::string& name)
    {
        auto sensor = std::make_shared<software_sensor>(name, this);
        add_sensor(sensor);
        _software_sensors.push_back(sensor);

        return *sensor;
    }

    void software_device::register_extrinsic(const stream_interface& stream)
    {
        // Called per frame; a stream's group is decided once and never changes
        if (_extrinsics.count(stream.get_unique_id()))
            return;

        uint32_t max_idx = 0;
        for (auto & pair : _extrinsics) {
            if (pair.second.first > max_idx) max_idx = pair.second.first;
            rs2_extrinsics ext;
            if (environment::get_instance().get_extrinsics_graph().try_fetch_extrinsics(stream, *pair.second.second, &ext)) {
                register_stream_to_extrinsic_group(stream, pair.second.first);
                return;
            }
        }
        register_stream_to_extrinsic_group(stream, max_idx+1);
    }

    void software_device::register_destruction_callback( destruction_callback_ptr callback )
    {
        _user_destruction_callback = std::move(callback);
    }

    software_sensor& software_device::get_software_sensor( size_t index)
    {
        if (index >= _software_sensors.size())
        {
            throw rs2::error("Requested index is out of range!");
        }
        return *_software_sensors[index];
    }
    
    void software_device::set_matcher_type(rs2_matchers matcher)
    {
        _matcher = matcher;
    }

    std::shared_ptr<matcher> software_device::create_matcher(const frame_holder& frame) const
    {
        using stream_uid = std::pair< rs2_stream, int >;
        std::set< stream_uid > streams;
        std::vector< stream_interface * > stream_interfaces;

        for( auto const & s : _software_sensors )
            for( auto const & p : s->_sw_profiles )
                if( streams.insert( { p->get_stream_type(), p->get_unique_id() } ).second )
                    stream_interfaces.push_back( p.get() );

        return matcher_factory::create( _matcher, stream_interfaces );
    }


} // namespace librealsense

