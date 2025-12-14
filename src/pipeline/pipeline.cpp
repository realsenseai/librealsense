// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2015 RealSense, Inc. All Rights Reserved.

#include <algorithm>
#include "pipeline.h"
#include "stream.h"
#include "media/playback/playback_device.h"
#include "media/record/record_device.h"
//#include "media/ros/ros_writer.h"
#include <src/proc/syncer-processing-block.h>
#include <src/core/frame-callback.h>
#ifdef BUILD_WITH_DDS
#include <src/dds/rs-dds-device-proxy.h>
#endif

#include <rsutils/string/from.h>


namespace librealsense
{
    namespace pipeline
    {
        pipeline::pipeline(std::shared_ptr<librealsense::context> ctx) :
            _ctx(ctx),
            _dispatcher(10),
            _hub( device_hub::make( ctx, RS2_PRODUCT_LINE_ANY_INTEL )),
            _synced_streams({ RS2_STREAM_COLOR, RS2_STREAM_DEPTH, RS2_STREAM_INFRARED, RS2_STREAM_FISHEYE })
        {}

        pipeline::~pipeline()
        {
            if (_active_profile) {
                try {
                    unsafe_stop();
                }
                catch (...) {}
            }
        }

        std::shared_ptr<profile> pipeline::start(std::shared_ptr<config> conf, rs2_frame_callback_sptr callback)
        {
            std::lock_guard<std::mutex> lock(_mtx);
            if (_active_profile)
            {
                throw librealsense::wrong_api_call_sequence_exception("start() cannot be called before stop()");
            }
            _streams_callback = callback;
            unsafe_start(conf);
            return unsafe_get_active_profile();
        }

        void pipeline::set_device( std::shared_ptr< librealsense::device_interface >  dev ) 
        {
            _dev = dev;
        }

        std::shared_ptr< librealsense::device_interface > pipeline::get_device() 
        {
            return _dev;
        }

        std::shared_ptr<profile> pipeline::get_active_profile() const
        {
            std::lock_guard<std::mutex> lock(_mtx);
            return unsafe_get_active_profile();
        }

        std::shared_ptr<profile> pipeline::unsafe_get_active_profile() const
        {
            if (!_active_profile)
                throw librealsense::wrong_api_call_sequence_exception("get_active_profile() can only be called between a start() and a following stop()");

            return _active_profile;
        }

        void pipeline::unsafe_start(std::shared_ptr<config> conf)
        {
            std::shared_ptr<profile> profile = nullptr;
            //first try to get the previously resolved profile (if exists)
            auto cached_profile = conf->get_cached_resolved_profile();
            if (cached_profile)
            {
                profile = cached_profile;
            }
            else
            {
                const int NUM_TIMES_TO_RETRY = 3;
                for (int i = 1; i <= NUM_TIMES_TO_RETRY; i++)
                {
                    try
                    {
                        profile = conf->resolve(shared_from_this(), std::chrono::seconds(5));
                        break;
                    }
                    catch (...)
                    {
                        if (i == NUM_TIMES_TO_RETRY)
                            throw;
                    }
                }
            }

            assert(profile);
            if (!profile->_multistream.get_profiles().size())
                throw librealsense::wrong_api_call_sequence_exception("No streams are selected!");

            auto synced_streams_ids = on_start(profile);

            rs2_frame_callback_sptr callbacks = get_callback(synced_streams_ids);

            auto dev = profile->get_device();
            if (auto playback = As<librealsense::playback_device>(dev))
            {
                _playback_stopped_token = playback->playback_status_changed.subscribe( [this, callbacks](rs2_playback_status status)
                {
                    if (status == RS2_PLAYBACK_STATUS_STOPPED)
                    {
                        _dispatcher.invoke([this, callbacks](dispatcher::cancellable_timer t)
                        {
                            //If the pipeline holds a playback device, and it reached the end of file (stopped)
                            //Then we restart it
                            if (_active_profile && _prev_conf->get_repeat_playback())
                            {
                                _active_profile->_multistream.open();
                                _active_profile->_multistream.start(callbacks);
                            }
                        });
                    }
                } );
            }

            _dispatcher.start();
#ifdef BUILD_WITH_DDS
            if( Is< librealsense::dds_device_proxy >( dev ) )
            {
                // For DDS devices open() only sets requested profiles, start() actually starts streaming.
                // Calling open() then start() sends open-streams control for all sensors before starting to stream,
                // second open-streams control will resets the first (reverting first requested streams to defaults).
                // open_and_start() starts streaming from the first sensor before the second open so it does not reset.
                profile->_multistream.open_and_start( callbacks );
            }
            else
#endif
            {
                profile->_multistream.open();
                profile->_multistream.start( callbacks );
            }
            _active_profile = profile;
            _prev_conf = std::make_shared<config>(*conf);
        }

        void pipeline::stop()
        {
            std::lock_guard<std::mutex> lock(_mtx);
            if (!_active_profile)
            {
                throw librealsense::wrong_api_call_sequence_exception("stop() cannot be called before start()");
            }
            unsafe_stop();
        }

        void pipeline::unsafe_stop()
        {
            if (_active_profile)
            {
                try
                {
                    _syncer->stop();
                    _aggregator->stop();
                    auto dev = _active_profile->get_device();
                    if (auto playback = As<librealsense::playback_device>(dev))
                    {
                        _playback_stopped_token.cancel();
                    }
                    _active_profile->_multistream.stop();
                    _active_profile->_multistream.close();
                    _dispatcher.stop();
                }
                catch (...)
                {
                } // Stop will throw if device was disconnected. TODO - refactoring anticipated

                // shared pointers initialized when pipeline running with _active_profile
                // should be reset with _active_profile too
                _active_profile.reset();
                _prev_conf.reset();
                _streams_callback.reset();
            }
        }

        std::shared_ptr<device_interface> pipeline::wait_for_device(const std::chrono::milliseconds& timeout, const std::string& serial)
        {
            // pipeline's device selection shall be deterministic
            return _hub->wait_for_device(timeout, false, serial);
        }

        std::shared_ptr<librealsense::context> pipeline::get_context() const
        {
            return _ctx;
        }

        std::vector<int> pipeline::on_start(std::shared_ptr<profile> profile)
        {
            std::vector<int> _streams_to_aggregate_ids;
            std::vector<int> _streams_to_sync_ids;
            bool sync_any = false;
            if (std::find(_synced_streams.begin(), _synced_streams.end(), RS2_STREAM_ANY) != _synced_streams.end())
                sync_any = true;
            // check wich of the active profiles should be synced and update the sync list accordinglly
            for (auto&& s : profile->get_active_streams())
            {
                _streams_to_aggregate_ids.push_back(s->get_unique_id());
                bool sync_current = sync_any;
                if (!sync_any && std::find(_synced_streams.begin(), _synced_streams.end(), s->get_stream_type()) != _synced_streams.end())
                    sync_current = true;
                if(sync_current)
                    _streams_to_sync_ids.push_back(s->get_unique_id());
            }

            _syncer = std::unique_ptr<syncer_process_unit>(new syncer_process_unit());
            _aggregator = std::unique_ptr<aggregator>(new aggregator(_streams_to_aggregate_ids, _streams_to_sync_ids));

            if (_streams_callback)
                _aggregator->set_output_callback(_streams_callback);

            return _streams_to_sync_ids;
        }

        rs2_frame_callback_sptr pipeline::get_callback(std::vector<int> synced_streams_ids)
        {
            _syncer->set_output_callback(
                make_frame_callback( [&]( frame_holder fref ) { _aggregator->invoke( std::move( fref ) ); } ) );

            return make_frame_callback(
                [&, synced_streams_ids]( frame_holder fref )
                {
                    // if the user requested to sync the frame push it to the syncer, otherwise push it to the
                    // aggregator
                    if( std::find( synced_streams_ids.begin(),
                                   synced_streams_ids.end(),
                                   fref->get_stream()->get_unique_id() )
                        != synced_streams_ids.end() )
                        _syncer->invoke( std::move( fref ) );
                    else
                        _aggregator->invoke( std::move( fref ) );
                } );
        }

        frame_holder pipeline::wait_for_frames(unsigned int timeout_ms)
        {
            std::lock_guard<std::mutex> lock(_mtx);
            if (!_active_profile)
            {
                throw librealsense::wrong_api_call_sequence_exception("wait_for_frames cannot be called before start()");
            }
            if (_streams_callback)
            {
                throw librealsense::wrong_api_call_sequence_exception("wait_for_frames cannot be called if a callback was provided");
            }

            frame_holder f;
            if (_aggregator->dequeue(&f, timeout_ms))
            {
                return f;
            }

            //hub returns true even if device already reconnected
            if (!_hub->is_connected(*_active_profile->get_device()))
            {
                try
                {
                    auto prev_conf = _prev_conf;
                    unsafe_stop();
                    unsafe_start(prev_conf);

                    if (_aggregator->dequeue(&f, timeout_ms))
                    {
                        return f;
                    }

                }
                catch (const std::exception& e)
                {
                    throw std::runtime_error( rsutils::string::from() << "Device disconnected. Failed to reconnect: "
                                                                      << e.what() << timeout_ms );
                }
            }
            throw std::runtime_error( rsutils::string::from() << "Frame didn't arrive within " << timeout_ms );
        }

        bool pipeline::poll_for_frames(frame_holder* frame)
        {
            std::lock_guard<std::mutex> lock(_mtx);

            if (!_active_profile)
            {
                throw librealsense::wrong_api_call_sequence_exception("poll_for_frames cannot be called before start()");
            }
            if (_streams_callback)
            {
                throw librealsense::wrong_api_call_sequence_exception("poll_for_frames cannot be called if a callback was provided");
            }

            if (_aggregator->try_dequeue(frame))
            {
                return true;
            }
            return false;
        }

        bool pipeline::try_wait_for_frames(frame_holder* frame, unsigned int timeout_ms)
        {
            std::lock_guard<std::mutex> lock(_mtx);
            if (!_active_profile)
            {
                throw librealsense::wrong_api_call_sequence_exception("try_wait_for_frames cannot be called before start()");
            }
            if (_streams_callback)
            {
                throw librealsense::wrong_api_call_sequence_exception("try_wait_for_frames cannot be called if a callback was provided");
            }

            if (_aggregator->dequeue(frame, timeout_ms))
            {
                return true;
            }

            //hub returns true even if device already reconnected
            if (!_hub->is_connected(*_active_profile->get_device()))
            {
                try
                {
                    auto prev_conf = _prev_conf;
                    unsafe_stop();
                    unsafe_start(prev_conf);
                    return _aggregator->dequeue(frame, timeout_ms);
                }
                catch (const std::exception& e)
                {
                    LOG_INFO(e.what());
                    return false;
                }
            }
            return false;
        }
    }
}
