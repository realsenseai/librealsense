// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2015 RealSense, Inc. All Rights Reserved.

#include "config.h"
#include "pipeline.h"
#include "platform/platform-device-info.h"
#include "media/playback/playback-device-info.h"
#include "context.h"
#include <src/librealsense-exception.h>
#include <rsutils/string/from.h>


namespace librealsense
{
    namespace pipeline
    {
        config::config()
        {
            //empty
        }
        void config::enable_stream(rs2_stream stream, int index, uint32_t width, uint32_t height, rs2_format format, uint32_t fps)
        {
            std::lock_guard<std::mutex> lock(_mtx);
            _resolved_profile.reset();
            _stream_requests[{stream, index}] = { format, stream, index, width, height, fps };
            auto position = std::find(_streams_to_disable.begin(), _streams_to_disable.end(), std::pair<rs2_stream, int>{stream, index});
            if (position != _streams_to_disable.end())
                _streams_to_disable.erase(position); //means the element was found
        }

        void config::enable_all_stream()
        {
            std::lock_guard<std::mutex> lock(_mtx);
            _resolved_profile.reset();
            _stream_requests.clear();
            _enable_all_streams = true;
            _streams_to_disable.clear();
        }

        void config::enable_device(const std::string& serial)
        {
            std::lock_guard<std::mutex> lock(_mtx);
            _resolved_profile.reset();
            _device_request.serial = serial;
        }

        void config::enable_device_from_file(const std::string& file, bool repeat_playback = true)
        {
            std::lock_guard<std::mutex> lock(_mtx);
            if (!_device_request.record_output.empty())
            {
                throw std::runtime_error("Configuring both device from file, and record to file is unsupported");
            }
            _resolved_profile.reset();
            _device_request.filename = file;
            _playback_loop = repeat_playback;
        }

        void config::enable_record_to_file(const std::string& file)
        {
            std::lock_guard<std::mutex> lock(_mtx);
            if (!_device_request.filename.empty())
            {
                throw std::runtime_error("Configuring both device from file, and record to file is unsupported");
            }
            _resolved_profile.reset();
            _device_request.record_output = file;
        }

        std::shared_ptr<profile> config::get_cached_resolved_profile()
        {
            std::lock_guard<std::mutex> lock(_mtx);
            return _resolved_profile;
        }

        void config::disable_stream(rs2_stream stream, int index)
        {
            std::lock_guard<std::mutex> lock(_mtx);
            _streams_to_disable.push_back({ stream, std::max(index, 0) }); // for this DB default index should be 0 not -1 because it is compared to get_stream_index() API that returns 0 as default index

            auto itr = std::begin(_stream_requests);
            while (itr != std::end(_stream_requests))
            {
                //if this is the same stream type and also the user either requested all or it has the same index
                if (itr->first.first == stream && (index == -1 || itr->first.second == index))
                {
                    itr = _stream_requests.erase(itr);
                }
                else
                {
                    ++itr;
                }
            }
            _resolved_profile.reset();
        }

        void config::disable_all_streams()
        {
            std::lock_guard<std::mutex> lock(_mtx);
            _stream_requests.clear();
            _enable_all_streams = false;
            _resolved_profile.reset();
            _streams_to_disable.clear();
        }

        util::config config::filter_stream_requests(const stream_profiles& profiles) const
        {
            util::config config;
            if (!_streams_to_disable.empty())
            {
                for (auto prof : profiles)
                {
                    bool disable_stream = false;
                    auto p = prof.get();
                    auto vp = dynamic_cast<video_stream_profile*>(p);
                    for (auto& st : _streams_to_disable)
                    {
                        if (st.first == p->get_stream_type())
                        {
                            if (st.second != p->get_stream_index())
                                break; // don't disable stream if indexes don't match
                            disable_stream = true;
                            break;
                        }
                    }
                    if (disable_stream)
                        continue;
                    if (vp)
                        config.enable_stream(vp->get_stream_type(), vp->get_stream_index(), vp->get_width(), vp->get_height(), vp->get_format(), vp->get_framerate());
                    else
                        config.enable_stream(p->get_stream_type(), p->get_stream_index(), 0, 0, p->get_format(), p->get_framerate());
                }
            }
            else
            {
                config.enable_streams(profiles);
            }

            return config;
        }

        std::shared_ptr<profile> config::resolve(std::shared_ptr<device_interface> dev)
        {
            util::config config;
            util::config filtered_config;

            //if the user requested all streams
            if (_enable_all_streams)
            {
                stream_profiles profiles;
                for (size_t i = 0; i < dev->get_sensors_count(); ++i)
                {
                    auto&& sub = dev->get_sensor(i);
                    auto p = sub.get_stream_profiles(PROFILE_TAG_SUPERSET);
                    profiles.insert(profiles.end(), p.begin(), p.end());
                }
                filtered_config = filter_stream_requests(profiles);
                return std::make_shared<profile>(dev, filtered_config, _device_request.record_output);
            }

            //If the user did not request anything, give it the default, on playback all recorded streams are marked as default.
            if (_stream_requests.empty())
            {
                auto default_profiles = get_default_configuration(dev);
                filtered_config = filter_stream_requests(default_profiles);
                return std::make_shared<profile>(dev, filtered_config, _device_request.record_output);
            }

            //Enabled requested streams
            for (auto&& req : _stream_requests)
            {
                bool disable_stream = false;
                auto r = req.second;
                for (auto& st : _streams_to_disable)
                {
                    if (st.first == r.stream)
                    {
                        if (st.second > 0 && st.second != r.index) break; // don't disable stream if indexes don't match
                        disable_stream = true;
                        break;
                    }
                }
                if (disable_stream) continue;
                config.enable_stream(r.stream, r.index, r.width, r.height, r.format, r.fps);
            }
            return std::make_shared<profile>(dev, config, _device_request.record_output);
        }

        std::shared_ptr<profile> config::resolve(std::shared_ptr<pipeline> pipe, const std::chrono::milliseconds& timeout)
        {
            std::lock_guard<std::mutex> lock(_mtx);
            _resolved_profile.reset();

            // D5xx device point cloud, Phase 1: the pipeline opens Depth first and FW rejects PCL while Depth runs
            auto requested = [this]( rs2_stream s )
            {
                for( auto && r : _stream_requests )
                    if( r.first.first == s )
                        return true;
                return false;
            };
            if( requested( RS2_STREAM_POINT_CLOUD ) && requested( RS2_STREAM_DEPTH ) )
                throw invalid_value_exception( "Point Cloud and Depth cannot be streamed together; enable one of them" );

            std::shared_ptr< librealsense::device_interface > requested_device = pipe->get_device();
            if(! requested_device )
                requested_device = resolve_device_requests( pipe, timeout );

            //Resolve the the device that was specified by the user, this call will wait in case the device is not availabe.
            if (requested_device != nullptr)
            {
                _resolved_profile = resolve(requested_device);
                return _resolved_profile;
            }

            //Look for satisfy device in case the user did not specify one.
            auto devs = pipe->get_context()->query_devices(RS2_PRODUCT_LINE_ANY_INTEL);
            for (auto dev_info : devs)
            {
                try
                {
                    auto dev = dev_info->create_device();
                    _resolved_profile = resolve(dev);
                    return _resolved_profile;
                }
                catch (const std::exception& e)
                {
                    LOG_DEBUG("Iterate available devices - config can not be resolved. " << e.what());
                }
            }

            //If no device found wait for one
            auto dev = pipe->wait_for_device(timeout);
            if (dev != nullptr)
            {
                _resolved_profile = resolve(dev);
                return _resolved_profile;
            }

            throw std::runtime_error("Failed to resolve request. No device found that satisfies all requirements");

            assert(0); //Unreachable code
        }

        bool config::can_resolve(std::shared_ptr<pipeline> pipe)
        {
            try
            {    // Try to resolve from connected devices. Non-blocking call
                resolve(pipe);
                _resolved_profile.reset();
            }
            catch (const std::exception& e)
            {
                LOG_DEBUG("Config can not be resolved. " << e.what());
                return false;
            }
            catch (...)
            {
                return false;
            }
            return true;
        }

        std::shared_ptr<device_interface> config::get_or_add_playback_device(std::shared_ptr<context> ctx, const std::string& file)
        {
            //Check if the file is already loaded to context, and if so return that device
            for (auto&& d : ctx->query_devices(RS2_PRODUCT_LINE_ANY))
            {
                auto pdev = std::dynamic_pointer_cast< playback_device_info >( d );
                if( pdev && pdev->get_filename() == file )
                    return pdev->create_device();
            }

            auto dev_info = std::make_shared< playback_device_info >( ctx, file );
            ctx->add_device( dev_info );
            return dev_info->create_device();
        }

        std::shared_ptr<device_interface> config::resolve_device_requests(std::shared_ptr<pipeline> pipe, const std::chrono::milliseconds& timeout)
        {
            //Prefer filename over serial
            if (!_device_request.filename.empty())
            {
                std::shared_ptr<device_interface> dev;
                try
                {
                    dev = get_or_add_playback_device(pipe->get_context(), _device_request.filename);
                }
                catch (const std::exception& e)
                {
                    throw std::runtime_error( rsutils::string::from()
                                              << "Failed to resolve request. Request to enable_device_from_file(\""
                                              << _device_request.filename << "\") was invalid, Reason: " << e.what() );
                }
                //check if a serial number was also requested, and check again the device
                if (!_device_request.serial.empty())
                {
                    if (!dev->supports_info(RS2_CAMERA_INFO_SERIAL_NUMBER))
                    {
                        throw std::runtime_error( rsutils::string::from()
                                                  << "Failed to resolve request. "
                                                     "Conflic between enable_device_from_file(\""
                                                  << _device_request.filename << "\") and enable_device(\""
                                                  << _device_request.serial
                                                  << "\"), "
                                                     "File does not contain a device with such serial" );
                    }
                    else
                    {
                        std::string s = dev->get_info(RS2_CAMERA_INFO_SERIAL_NUMBER);
                        if (s != _device_request.serial)
                        {
                            throw std::runtime_error( rsutils::string::from()
                                                      << "Failed to resolve request. "
                                                         "Conflic between enable_device_from_file(\""
                                                      << _device_request.filename << "\") and enable_device(\""
                                                      << _device_request.serial
                                                      << "\"), "
                                                         "File contains device with different serial number ("
                                                      << s << "\")" );
                        }
                    }
                }
                return dev;
            }

            if (!_device_request.serial.empty())
            {
                return pipe->wait_for_device(timeout, _device_request.serial);
            }

            return nullptr;
        }

        stream_profiles config::get_default_configuration(std::shared_ptr<device_interface> dev)
        {
            stream_profiles default_profiles;

            for (unsigned int i = 0; i < dev->get_sensors_count(); i++)
            {
                auto&& sensor = dev->get_sensor(i);
                auto profiles = sensor.get_stream_profiles(profile_tag::PROFILE_TAG_DEFAULT);
                default_profiles.insert(std::end(default_profiles), std::begin(profiles), std::end(profiles));
            }

            return default_profiles;
        }

        bool config::get_repeat_playback() {
            return _playback_loop;
        }
    }
}
