// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 RealSense, Inc. All Rights Reserved.

#include <cmath>
#include "playback_device.h"
#include "playback-device-info.h"
#include "core/motion.h"
#include "stream.h"
#include "media/ros2/ros2_reader.h"
#include "environment.h"
#include "sync.h"
#include <src/depth-sensor.h>
#include <src/color-sensor.h>
#include <src/pose.h>
#include <src/safety-sensor.h>
#include <src/depth-mapping-sensor.h>

#include <rsutils/string/from.h>


using namespace librealsense;

static bool is_video_stream( rs2_stream stream )
{
    return stream != RS2_STREAM_GYRO && stream != RS2_STREAM_ACCEL && stream != RS2_STREAM_POSE;
}

std::shared_ptr< device_interface > playback_device_info::create_device()
{
    auto playback_dev
        = std::make_shared< playback_device >( shared_from_this(),
                                               std::make_shared< ros2_reader >( _filename, get_context() ) );
    return playback_dev;
}

playback_device::playback_device( std::shared_ptr< const device_info > const & dev_info,
                                  std::shared_ptr< device_serializer::reader > const & serializer )
    : m_read_thread( []() { return std::make_shared< dispatcher >( std::numeric_limits< unsigned int >::max() ); } )
    , m_device_info( dev_info )
    , m_is_started( false )
    , m_is_paused( false )
    , m_sample_rate( 1 )
    , m_real_time( true )
    , m_prev_timestamp( 0 )
    , m_last_published_timestamp( 0 )
{
    if (serializer == nullptr)
    {
        throw invalid_value_exception("null serializer");
    }

    m_reader = serializer;
    (*m_read_thread)->start();

    //Read header and build device from recorded device snapshot
    m_device_description = m_reader->query_device_description(nanoseconds(0));

    register_device_info(m_device_description);
    //Create playback sensor that simulate the recorded sensors
    m_sensors = create_playback_sensors(m_device_description);

    //all of the recorded streams are marked as default
    for (auto sensor_pair : m_sensors)
    {
        auto profiles = sensor_pair.second->get_stream_profiles();
        tag_profiles(profiles);
    }
    register_extrinsics(m_device_description);
}

std::map<uint32_t, std::shared_ptr<playback_sensor>> playback_device::create_playback_sensors(const device_snapshot& device_description)
{
    std::map<uint32_t, std::shared_ptr<playback_sensor>> sensors;
    for (auto sensor_snapshot : device_description.get_sensors_snapshots())
    {
        //Each sensor will know its capabilities from the sensor_snapshot
        auto sensor = std::make_shared<playback_sensor>(*this, sensor_snapshot);

        sensor->on_started( [this](uint32_t id, rs2_frame_callback_sptr user_callback) -> void
        {
            (*m_read_thread)->invoke([this, id, user_callback](dispatcher::cancellable_timer c)
            {
                std::lock_guard<std::mutex> locker(_active_sensors_mutex);
                auto it = m_active_sensors.find(id);
                if (it == m_active_sensors.end())
                {
                    m_active_sensors[id] = m_sensors[id];

                    if (m_active_sensors.size() == 1) //On the first sensor that starts, start the reading thread
                    {
                        start();
                    }
                }
            });
        } );

        sensor->on_stopped( [this](uint32_t id, bool invoke_required) -> void
        {
            //stopped could be called by the user (when calling sensor.stop(), main thread==invoke required) or from the reader_thread when
            // reaching eof, or some read error (which means invoke is not required)

            auto action = [this, id]()
            {
                bool need_to_stop_device = false;
                {
                    std::lock_guard<std::mutex> locker(_active_sensors_mutex);
                    auto it = m_active_sensors.find(id);
                    if (it != m_active_sensors.end())
                    {
                        m_active_sensors.erase(it);
                        if (m_active_sensors.size() == 0)
                        {
                            need_to_stop_device = true;
                        }
                    }
                }

                // If all sensors were stopped, we want to stop the device from dispatching frames.
                // We invoke "stop_internal()" for concurrency reasons
                if (need_to_stop_device)
                {
                    ( *m_read_thread )->invoke( [this]( dispatcher::cancellable_timer c ) {
                        stop_internal();
                    } );
                }
            };
            if (invoke_required)
            {
                (*m_read_thread)->invoke([action](dispatcher::cancellable_timer c) { action(); });
            }
            else
            {
                action();
            }
        } );

        sensor->on_opened( [this](const std::vector<device_serializer::stream_identifier>& filters) -> void
        {
            (*m_read_thread)->invoke([this, filters](dispatcher::cancellable_timer c)
            {
                m_reader->enable_stream(filters);
            });
        } );

        sensor->on_closed( [this](const std::vector<device_serializer::stream_identifier>& filters) -> void
        {
            (*m_read_thread)->invoke([this, filters](dispatcher::cancellable_timer c)
            {
                m_reader->disable_stream(filters);
            });
        } );

        sensors[sensor_snapshot.get_sensor_index()] = sensor;
    }
    return sensors;
}

std::shared_ptr<stream_profile_interface> playback_device::get_stream(const std::map<unsigned, std::shared_ptr<playback_sensor>>& sensors_map, device_serializer::stream_identifier stream_id)
{
    for (auto sensor_pair : sensors_map)
    {
        if(sensor_pair.first == stream_id.sensor_index)
        {
            for (auto stream_profile : sensor_pair.second->get_stream_profiles())
            {
                if(stream_profile->get_stream_type() == stream_id.stream_type && stream_profile->get_stream_index() == stream_id.stream_index)
                {
                    return stream_profile;
                }
            }
        }
    }
    throw invalid_value_exception("File contains extrinsics that do not map to an existing stream");
}

rs2_extrinsics playback_device::calc_extrinsic(const rs2_extrinsics& from, const rs2_extrinsics& to)
{
    //NOTE: Assuming here that recording is writing extrinsics **from** the stream at hand **to** some reference point
    return from_pose(to_pose(to) * inverse(to_pose(from)));
}

playback_device::~playback_device()
{
    std::vector< std::shared_ptr< playback_sensor > > playback_sensors_copy;
    {
        std::lock_guard< std::mutex > locker(_active_sensors_mutex);
        for (auto s : m_active_sensors)
            playback_sensors_copy.push_back(s.second);
    }


    for (auto&& sensor : playback_sensors_copy)
    {
        if (sensor)
        {
            sensor->stop();
        }
    }

    (*m_read_thread)->stop();
}

std::shared_ptr<context> playback_device::get_context() const
{
    return m_device_info->get_context();
}

sensor_interface& playback_device::get_sensor(size_t i)
{
    return *m_sensors.at(static_cast<uint32_t>(i));
}

size_t playback_device::get_sensors_count() const
{
    return m_sensors.size();
}

const sensor_interface& playback_device::get_sensor(size_t i) const
{
    auto sensor = m_sensors.at(static_cast<uint32_t>(i));
    return *std::dynamic_pointer_cast<sensor_interface>(sensor);
}

void playback_device::hardware_reset()
{
    //Nothing to see here folks
}

bool playback_device::extend_to(rs2_extension extension_type, void** ext)
{
    std::shared_ptr<extension_snapshot> e = m_device_description.get_device_extensions_snapshots().find(extension_type);
    return try_extend_snapshot(e, extension_type, ext);
}

std::shared_ptr<matcher> playback_device::create_matcher(const frame_holder& frame) const
{
    std::vector<std::shared_ptr<matcher>> sync_matchers;
    std::vector<std::shared_ptr<matcher>> non_sync_matchers;

    for (auto const& sensor : m_sensors)
    {
        auto stream_profiles = sensor.second->get_stream_profiles(); 

        for( auto const & stream_profile : stream_profiles )
        {
            if(is_video_stream(stream_profile->get_stream_type()))
                sync_matchers.push_back(std::make_shared<identity_matcher>(stream_profile->get_unique_id(), stream_profile->get_stream_type()));
            else
                non_sync_matchers.push_back(std::make_shared<identity_matcher>(stream_profile->get_unique_id(), stream_profile->get_stream_type()));
        }
    }
    
    std::vector<std::shared_ptr<matcher>> all_matchers;

    if (!sync_matchers.empty())
       all_matchers.push_back(std::make_shared<timestamp_composite_matcher>(sync_matchers));

    if (!non_sync_matchers.empty())
        all_matchers.insert(all_matchers.end(), non_sync_matchers.begin(), non_sync_matchers.end());

    return std::make_shared<composite_identity_matcher>(all_matchers);
}

void playback_device::set_frame_rate(double rate)
{
    LOG_INFO("Request to change playback frame rate to: " << rate);
    if(rate < 0)
    {
        throw invalid_value_exception( rsutils::string::from() << "Failed to set frame rate to "
                                                               << std::to_string( rate ) << ", value is less than 0" );
    }
    (*m_read_thread)->invoke([this, rate](dispatcher::cancellable_timer t)
    {
        LOG_INFO("Changing playback frame rate to: " << rate);
        m_sample_rate = rate;
        update_time_base(m_prev_timestamp);
    });
}

void playback_device::seek_to_time(std::chrono::nanoseconds time)
{
    LOG_INFO("Request to seek to: " << time.count());
    (*m_read_thread)->invoke([this, time](dispatcher::cancellable_timer t)
    {
        LOG_INFO("Seek to time: " << time.count());
        m_reader->seek_to_time(time);
        m_device_description = m_reader->query_device_description(time);
        update_extensions(m_device_description);
        m_prev_timestamp = time; //Updating prev timestamp to make get_position return true indication even when playbakc is paused
        catch_up();
        if (m_is_paused)
        {
            //raise_last_frames(time);
            auto current_frames = m_reader->fetch_last_frames(time);
            for (auto&& f : current_frames)
            {
                if (auto frame = f->as<serialized_frame>())
                {
                    if (frame->stream_id.device_index != get_device_index() || frame->stream_id.sensor_index >= m_sensors.size())
                    {
                        std::string error_msg = rsutils::string::from()
                                             << "Unexpected sensor index while playing file (Read index = "
                                             << frame->stream_id.sensor_index << ")";
                        LOG_ERROR(error_msg);
                    }
                    //push frame to the sensor (see handle_frame definition for more details)
                    m_sensors.at(frame->stream_id.sensor_index)->handle_frame(std::move(frame->frame), m_real_time,
                        []() { return device_serializer::nanoseconds(0); },
                        []() { return false; },
                        [this, time]()
                        {
                            std::lock_guard<std::mutex> locker(m_last_published_timestamp_mutex);
                            m_last_published_timestamp = time;
                        });
                }
            }
        }
    });
    if ((*m_read_thread)->flush() == false)
    {
        LOG_ERROR("Error - timeout waiting for seek_to_time, possible deadlock detected");
        assert(0); //Detect this immediately in debug
    }
}

rs2_playback_status playback_device::get_current_status() const
{
    return m_is_started ?
        m_is_paused ? RS2_PLAYBACK_STATUS_PAUSED : RS2_PLAYBACK_STATUS_PLAYING
        : RS2_PLAYBACK_STATUS_STOPPED;
}

uint64_t playback_device::get_duration() const
{
    return m_reader->query_duration().count();
}

void playback_device::pause()
{
    LOG_DEBUG("Playback Pause called");

    /*
        Playing ---->  pause()   set m_is_paused  to True  ----> Paused
        Paused  ---->  pause()   set m_is_paused  to True  ----> Do nothing
        Stopped ---->  pause()   set m_is_paused  to True  ----> Do nothing
    */
    (*m_read_thread)->invoke([this](dispatcher::cancellable_timer t)
    {
        LOG_DEBUG("Playback pause invoked");

        if (m_is_paused)
            return;

        m_is_paused = true;

        if(m_is_started)
        {
            //Wait for any remaining sensor callbacks to return
            for (auto sensor : m_sensors)
            {
                sensor.second->flush_pending_frames();
            }
        }
        LOG_DEBUG("Notifying RS2_PLAYBACK_STATUS_PAUSED");
        playback_status_changed.raise( RS2_PLAYBACK_STATUS_PAUSED );
    });
    if ((*m_read_thread)->flush() == false)
    {
        LOG_ERROR("Error - timeout waiting for pause, possible deadlock detected");
        assert(0); //Detect this immediately in debug
    }
    LOG_INFO("Playback Paused");
}

void playback_device::resume()
{
    LOG_DEBUG("Playback resume called");
    (*m_read_thread)->invoke([this](dispatcher::cancellable_timer t)
    {
        LOG_DEBUG("Playback resume invoked");
        if (m_is_paused == false)
            return;

        auto total_duration = m_reader->query_duration();
        {
            std::lock_guard< std::mutex > locker( m_last_published_timestamp_mutex );
            if( m_last_published_timestamp >= total_duration )
                m_last_published_timestamp = device_serializer::nanoseconds(0);
            m_reader->reset();
            m_reader->seek_to_time(m_last_published_timestamp);
            while (m_last_published_timestamp != device_serializer::nanoseconds(0) && !m_reader->read_next_data()->is<serialized_frame>());
        }
        m_is_paused = false;
        catch_up();

        try_looping();
    });
    if ((*m_read_thread)->flush() == false)
    {
        LOG_ERROR("Error - timeout waiting for resume, possible deadlock detected");
        assert(0); //Detect this immediately in debug
    }
    LOG_INFO("Playback Resumed");
}

void playback_device::set_real_time(bool real_time)
{
    LOG_INFO("Set real time to " << ((real_time) ? "True" : "False"));
    m_real_time = real_time;
}

bool playback_device::is_real_time() const
{
    return m_real_time;
}

std::shared_ptr< const device_info > playback_device::get_device_info() const
{
    return m_device_info;
}

std::pair<uint32_t, rs2_extrinsics> playback_device::get_extrinsics(const stream_interface& stream) const
{
    return m_extrinsics_map.at(stream.get_unique_id());
}

bool playback_device::is_valid() const
{
    return true;
}

bool playback_device::is_in_recovery_mode() const
{
    return false;
}

void playback_device::update_time_base(device_serializer::nanoseconds base_timestamp)
{
    m_base_sys_time = std::chrono::high_resolution_clock::now();
    m_base_timestamp = base_timestamp;
    LOG_DEBUG("Updating Time Base... m_base_sys_time " << m_base_sys_time.time_since_epoch().count() << " m_base_timestamp " << m_base_timestamp.count());
}

device_serializer::nanoseconds playback_device::calc_sleep_time(device_serializer::nanoseconds timestamp)
{
    if (!m_real_time)
        return device_serializer::nanoseconds(0);
    //The time to sleep returned here equals to the difference between the file recording time
    // and the playback time.
    auto now = std::chrono::high_resolution_clock::now();
    auto play_time = now - m_base_sys_time;

    //Sometimes the first stream skip the first frame on the ros reader
    //and the second stream go back to the first frame so its timestamp is smaller then the base timestamp
    //in this case we need to restart the m_base_timestamp again
    if(timestamp < m_base_timestamp)
    {
        update_time_base(timestamp);
    }
    auto time_diff = timestamp - m_base_timestamp;
    auto recorded_time = std::chrono::duration_cast<device_serializer::nanoseconds>(time_diff / m_sample_rate.load());

    LOG_DEBUG("Time Now  : " << now.time_since_epoch().count() << " ,    Time When Started: " << m_base_sys_time.time_since_epoch().count() << " , Diff: " << play_time.count() << " == " << (play_time.count() * 1e-6) << "ms");
    LOG_DEBUG("Original Recording Delta: " << time_diff.count() << " == " << (time_diff.count() * 1e-6) << "ms");
    LOG_DEBUG("Frame Time: " << timestamp.count() << "  , First Frame: " << m_base_timestamp.count() << " ,  Diff: " << recorded_time.count() << " == " << (recorded_time.count() * 1e-6) << "ms");

    if(recorded_time < play_time)
    {
        LOG_DEBUG("Recorded Time < Playing Time  (not sleeping)");
        return device_serializer::nanoseconds(0);
    }
    auto sleep_time = (recorded_time - play_time);
    LOG_DEBUG("Sleep Time: " << sleep_time.count() << " == " << (sleep_time.count() * 1e-6) << " ms");
    return sleep_time;
}

void playback_device::start()
{
    //Start reading from the file
    //Start is called only from Stopped state
    /*
    Playing ---->  start()   set m_is_started to True  ----> Do nothing
    Paused  ---->  start()   set m_is_started to True  ----> Do nothing
    Stopped ---->  start()   set m_is_started to True  ----> Paused/Playing (depends on m_is_paused)
    */
    LOG_DEBUG("playback start called");

    if (m_is_started)
        return; //nothing to do

    m_is_started = true;
    catch_up();
    try_looping();
    LOG_INFO("Playback started");
}

void playback_device::stop()
{
    LOG_DEBUG("playback stop called");
    (*m_read_thread)->invoke([this](dispatcher::cancellable_timer t)
    {
        LOG_DEBUG("playback stop invoked");
        stop_internal();
    });
    if ((*m_read_thread)->flush() == false)
    {
        LOG_ERROR("Error - timeout waiting for flush, possible deadlock detected");
        assert(0); //Detect this immediately in debug
    }

    LOG_INFO("Playback stopped");
}

void playback_device::stop_internal()
{
    //stop_internal() is called from within the reading thread
    LOG_DEBUG("stop_internal() called");
    if (m_is_started == false)
        return; //nothing to do


    m_is_started = false;
    m_is_paused = false;

    m_reader->reset();
    m_prev_timestamp = std::chrono::nanoseconds(0);
    catch_up();
    playback_status_changed.raise( RS2_PLAYBACK_STATUS_STOPPED );
    LOG_DEBUG("stop_internal() end");
}

template <typename T>
void playback_device::do_loop(T action)
{
    (*m_read_thread)->invoke([this, action](dispatcher::cancellable_timer c)
    {
        bool action_succeeded = false;
        if (m_is_started)
        {
            try
            {
                action_succeeded = action();
            }
            catch (const std::exception& e)
            {
                LOG_ERROR("Failed to read next frame from file: " << e.what());
                //TODO: notify user that playback unexpectedly ended
                action_succeeded = false; //will make the scope_guard stop the sensors, must return.
            }
        }

        //On failure, exit thread
        if(action_succeeded == false && m_is_started)
        {
            // Stopping the sensor will call another function which will remove the sensor from the
            // list of active sensors, which will cause issues -- so we copy it first
            std::vector< std::shared_ptr< playback_sensor > > playback_sensors_copy;
            {
                std::lock_guard< std::mutex > locker( _active_sensors_mutex );
                for (auto s : m_active_sensors)
                    playback_sensors_copy.push_back( s.second );
            }
            for( auto & psc : playback_sensors_copy )
            {
                if( psc )
                {
                    psc->flush_pending_frames();
                    psc->stop( false );
                }
            }

            std::lock_guard<std::mutex> locker(m_last_published_timestamp_mutex);
            m_last_published_timestamp = device_serializer::nanoseconds(0);
        }

        //Continue looping?
        if (m_is_started == true && m_is_paused == false)
        {
            do_loop(action);
        }
    });
}

// Called in real-time only
// Return should indicate whether any frames are available: if there are, we need to sleep before proceeding
bool playback_device::prefetch_done()
{
    std::lock_guard<std::mutex> locker(_active_sensors_mutex);
    for (auto s : m_active_sensors)
    {
        if (s.second->streams_contains_one_frame_or_more())
            return true;
    }
    return false;
}

void playback_device::try_looping()
{
    //try_looping is called from start() or resume()
    if (m_is_started && m_is_paused == false)
    {
        //Notify subscribers that playback status changed
        if (m_is_paused)
        {
            playback_status_changed.raise( RS2_PLAYBACK_STATUS_PAUSED );
        }
        else
        {
            playback_status_changed.raise( RS2_PLAYBACK_STATUS_PLAYING );
        }
    }

    auto read_action = [this]() -> bool
    {
        LOG_DEBUG("Read action invoked");

        //Read next data from the serializer, on success: 'obj' will be a valid object that came from
        // sensor number 'sensor_index' with a timestamp equal to 'timestamp'
        std::shared_ptr<serialized_data> data = m_reader->read_next_data();
        if (data->as<serialized_end_of_file>())
        {
            LOG_INFO("End of file reached");
            return false;
        }

        auto timestamp = data->get_timestamp();
        m_prev_timestamp = timestamp;
        //Objects with timestamp of 0 are non streams.
        if (m_base_timestamp.count() == 0)
        {
            //As long as m_base_timestamp is 0, update it to object's timestamp.
            //Once a streaming object arrive, the base will change from 0
            update_time_base(timestamp);
        }

        //Calculate the duration for the reader to sleep (i.e wait for next frame)
        if (m_real_time && prefetch_done())
        {
            auto sleep_time = calc_sleep_time(timestamp);
            if (sleep_time.count() > 0)
            {
                if (m_sample_rate > 0)
                {
                    LOG_DEBUG("Sleeping for: " << (sleep_time.count() * 1e-6));
                    std::this_thread::sleep_for(sleep_time);
                }
            }
        }

        if (auto frame = data->as<serialized_frame>())
        {
            frame->frame.frame->set_blocking(!m_real_time);
            if (frame->stream_id.device_index != get_device_index() || frame->stream_id.sensor_index >= m_sensors.size())
            {
                std::string error_msg = rsutils::string::from()
                                     << "Unexpected sensor index while playing file (Read index = "
                                     << frame->stream_id.sensor_index << ")";
                LOG_ERROR(error_msg);
                throw invalid_value_exception(error_msg);
            }
            LOG_DEBUG("Dispatching frame " << frame->frame);

            if (data->is<serialized_invalid_frame>())
            {
                LOG_WARNING("Bad frame from reader, ignoring");
                return true;
            }
            {
                std::lock_guard< std::mutex > locker( _active_sensors_mutex );
                auto it = m_active_sensors.find( frame->stream_id.sensor_index );
                if( it == m_active_sensors.end() )
                {
                    LOG_DEBUG( "stream " << frame->stream_id.sensor_index
                                         << " is not longer active, frame dropped!" );
                    return true;
                }


                // Dispatch frame to the relevant sensor (see handle_frame definition for more
                // details)
                it->second->handle_frame(
                    std::move( frame->frame ),
                    m_real_time,
                    [this, timestamp]() { return calc_sleep_time( timestamp ); },
                    [this]() { return m_is_paused == true; },
                    [this, timestamp]() {
                        std::lock_guard< std::mutex > locker( m_last_published_timestamp_mutex );
                        m_last_published_timestamp = timestamp;
                    } );
            }
            return true;
        }

        if (auto option_data = data->as<serialized_option>())
        {
            m_sensors.at(option_data->sensor_id.sensor_index)->update_option(option_data->option_id, option_data->option);
            return true;
        }

        if (auto notification_data = data->as<serialized_notification>())
        {
            m_sensors.at(notification_data->sensor_id.sensor_index)->raise_notification(notification_data->notif);
            return true;
        }
        return false;
    };
    do_loop(read_action);
}

const std::string& playback_device::get_file_name() const
{
    return m_reader->get_file_name();
}

uint64_t playback_device::get_position() const
{
    return m_prev_timestamp.count();
}
void playback_device::catch_up()
{
    m_base_timestamp = std::chrono::microseconds(0);
    LOG_DEBUG("Catching up");
}

void playback_device::register_device_info(const device_serializer::device_snapshot& device_description)
{
    auto info_snapshot = device_description.get_device_extensions_snapshots().find(RS2_EXTENSION_INFO);
    if (info_snapshot == nullptr)
    {
        LOG_WARNING("Recorded file does not contain device informatiom");
        return;
    }

    auto info_api = As<info_interface>(info_snapshot);
    if (info_api == nullptr)
    {
        throw invalid_value_exception("Failed to get info interface from device snapshots");
    }
    for (int i = 0; i < RS2_CAMERA_INFO_COUNT; ++i)
    {
        rs2_camera_info info = static_cast<rs2_camera_info>(i);
        if (info_api->supports_info(info))
        {
            register_info(info, info_api->get_info(info));
        }
    }
}

void playback_device::register_extrinsics(const device_serializer::device_snapshot& device_description)
{
    //Register extrinsics
    for (auto e1 : device_description.get_extrinsics_map())
    {
        for (auto e2 : device_description.get_extrinsics_map())
        {
            if (e1.second.first != e2.second.first)
            {
                //Not under the same extrinsics group
                continue;
            }

            auto p1 = get_stream(m_sensors, e1.first);
            auto p2 = get_stream(m_sensors, e2.first);
            rs2_extrinsics x = calc_extrinsic(e1.second.second, e2.second.second);
            auto extrinsic_fetcher = std::make_shared< rsutils::lazy< rs2_extrinsics > >( [x]()
            {
                return x;
            });
            m_extrinsics_map[p1->get_unique_id()] = e1.second;
            m_extrinsics_map[p2->get_unique_id()] = e2.second;
            environment::get_instance().get_extrinsics_graph().register_extrinsics(*p1, *p2, extrinsic_fetcher);
            m_extrinsics_fetchers.push_back(extrinsic_fetcher);  //Caching the lazy<rs2_extrinsics> since context holds weak_ptr
        }
    }
}

bool playback_device::try_extend_snapshot(std::shared_ptr<extension_snapshot>& e, rs2_extension extension_type, void** ext)
{
    if (e == nullptr)
    {
        return false;
    }

    // NOTE: the extensions listed here may not all have a base of recordable<>, but a snapshot of which is nonetheless
    // created by ros_reader. I.e., any snapshot created by ros_reader needs to be listed here, even if the extension is
    // not recordable!
    switch (extension_type)
    {
    case RS2_EXTENSION_INFO:    return try_extend<info_interface>(e, ext);
    case RS2_EXTENSION_OPTIONS: return try_extend<options_interface>(e, ext);
    case RS2_EXTENSION_VIDEO:   return try_extend<video_sensor_interface>(e, ext);
    case RS2_EXTENSION_ROI:     return try_extend<roi_sensor_interface>(e, ext);
    case RS2_EXTENSION_DEPTH_SENSOR: return try_extend<depth_sensor>(e, ext);
    case RS2_EXTENSION_DEPTH_STEREO_SENSOR: return try_extend<depth_stereo_sensor>(e, ext);
    case RS2_EXTENSION_COLOR_SENSOR: return try_extend<color_sensor>(e, ext);
    case RS2_EXTENSION_MOTION_SENSOR: return try_extend<motion_sensor>(e, ext);
    case RS2_EXTENSION_FISHEYE_SENSOR: return try_extend<fisheye_sensor>(e, ext);
    case RS2_EXTENSION_SAFETY_SENSOR: return try_extend<safety_sensor>(e, ext);
    case RS2_EXTENSION_DEPTH_MAPPING_SENSOR: return try_extend<depth_mapping_sensor>(e, ext);
    case RS2_EXTENSION_UNKNOWN: //[[fallthrough]]
    case RS2_EXTENSION_COUNT:   //[[fallthrough]]
    default:
        LOG_WARNING("Unsupported extension type: " << extension_type);
    }
    return false;
}

void playback_device::update_extensions(const device_serializer::device_snapshot& device_description)
{
    //TODO: Need to update all extensions not just options
    for (auto sensor_snapshot : device_description.get_sensors_snapshots())
    {
        auto sensor_index = sensor_snapshot.get_sensor_index();
        m_sensors.at(sensor_index)->update(sensor_snapshot);
    }
}
