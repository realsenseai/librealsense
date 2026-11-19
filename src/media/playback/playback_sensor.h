// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 RealSense, Inc. All Rights Reserved.

#pragma once
#include "../../core/roi.h"
#include "../../core/extension.h"
#include "../../core/serialization.h"
#include "../../archive.h"
#include "../../sensor.h"
#include "../../types.h"

#include <rsutils/signal.h>


namespace librealsense
{
    class playback_sensor : public sensor_interface,
        public extendable_interface,
        public info_container,
        public options_container,
        public std::enable_shared_from_this<playback_sensor>
    {
        std::function< void( uint32_t, rs2_frame_callback_sptr ) > _on_started;
        std::function< void( uint32_t, bool ) > _on_stopped;
        std::function< void( const std::vector< device_serializer::stream_identifier > & ) > _on_opened;
        std::function< void( const std::vector< device_serializer::stream_identifier > & ) > _on_closed;

    public:
        using frame_interface_callback_t = std::function<void(frame_holder)>;

        playback_sensor(device_interface& parent_device, const device_serializer::sensor_snapshot& sensor_description);
        virtual ~playback_sensor();

        void on_started( std::function< void( uint32_t id, rs2_frame_callback_sptr user_callback ) > && callback )
            { _on_started = std::move( callback ); }
        void on_stopped( std::function< void( uint32_t id, bool invoke_required ) > && callback )
            { _on_stopped = std::move( callback ); }
        void on_opened( std::function< void( const std::vector< device_serializer::stream_identifier > & ) > && callback )
            { _on_opened = std::move( callback ); }
        void on_closed( std::function< void( const std::vector< device_serializer::stream_identifier > & ) > && callback )
            { _on_closed = std::move( callback ); }

        stream_profiles get_stream_profiles(int tag = profile_tag::PROFILE_TAG_ANY) const override;
        void open(const stream_profiles& requests) override;
        void close() override;
        void register_notifications_callback( rs2_notifications_callback_sptr callback ) override;
        rs2_notifications_callback_sptr get_notifications_callback() const override;
        void start( rs2_frame_callback_sptr callback ) override;
        void stop() override;
        bool is_streaming() const override;
        bool extend_to(rs2_extension extension_type, void** ext) override;
        device_interface& get_device() override;
        void update_option(rs2_option id, std::shared_ptr<option> option);
        void stop(bool invoke_required);
        void flush_pending_frames();
        void update(const device_serializer::sensor_snapshot& sensor_snapshot);
        rs2_frame_callback_sptr get_frames_callback() const override;
        void set_frames_callback( rs2_frame_callback_sptr callback ) override;
        stream_profiles get_active_streams() const override;
        stream_profiles const & get_raw_stream_profiles() const override { return m_available_profiles; }
        int register_before_streaming_changes_callback(std::function<void(bool)> callback) override;
        void unregister_before_start_callback(int token) override;
        void raise_notification(const notification& n);
        bool streams_contains_one_frame_or_more();
        virtual processing_blocks get_recommended_processing_blocks() const override
        {
            auto processing_blocks_snapshot = m_sensor_description.get_sensor_extensions_snapshots().find(RS2_EXTENSION_RECOMMENDED_FILTERS);
            if (processing_blocks_snapshot == nullptr)
            {
                throw invalid_value_exception("Recorded file does not contain sensor processing blocks");
            }
            auto processing_blocks_api = As<recommended_proccesing_blocks_interface>(processing_blocks_snapshot);
            if (processing_blocks_api == nullptr)
            {
                throw invalid_value_exception("Failed to get options interface from sensor snapshots");
            }
            return processing_blocks_api->get_recommended_processing_blocks();
        }
        embedded_filters get_supported_embedded_filters() const override { return embedded_filters(); };

        rsutils::subscription register_options_changed_callback( options_watcher::callback && cb ) override
        {
            throw not_implemented_exception( "Registering options value changed callback is not implemented for playback sensor" );
        }

    protected:
        void set_active_streams(const stream_profiles& requests);

    private:
        void register_sensor_streams(const stream_profiles& vector);
        void register_sensor_infos(const device_serializer::sensor_snapshot& sensor_snapshot);
        void register_sensor_options(const device_serializer::sensor_snapshot& sensor_snapshot);
        


        rs2_frame_callback_sptr m_user_callback;
        notifications_processor _notifications_processor;
        using stream_unique_id = int;
        std::map<stream_unique_id, std::shared_ptr<dispatcher>> m_dispatchers;
        std::atomic<bool> m_is_started;
        device_serializer::sensor_snapshot m_sensor_description;
        uint32_t m_sensor_id;
        std::mutex m_mutex;
        std::map<std::pair<rs2_stream, uint32_t>, std::shared_ptr<stream_profile_interface>> m_streams;
        device_interface& m_parent_device;
        stream_profiles m_available_profiles;
        stream_profiles m_active_streams;
        mutable std::mutex m_active_profile_mutex;
        const unsigned int _default_queue_size;

    public:
        //handle frame use 3 lambda functions that determines if and when a frame should be published.
        //calc_sleep - calculates the duration that the sensor should wait before publishing the frame,
        // the start point for this calculation is the last playback resume.
        //is_paused - check if the playback was paused while waiting for the frame publish time.
        //update_last_pushed_frame - lets the playback device know that a specific frame was published,
        // the playback device will use this info to determine which frames should be played next in a pause/resume scenario.
        template <class T, class K, class P>
        void handle_frame(frame_holder frame, bool is_real_time, T calc_sleep, K is_paused, P update_last_pushed_frame)
        {
            if (frame == nullptr)
            {
                throw invalid_value_exception("null frame passed to handle_frame");
            }
            if (m_is_started)
            {
                frame->get_owner()->set_sensor(shared_from_this());
                auto type = frame->get_stream()->get_stream_type();
                auto index = static_cast<uint32_t>(frame->get_stream()->get_stream_index());
                frame->set_stream(m_streams[std::make_pair(type, index)]);
                frame->set_sensor(shared_from_this());
                auto stream_id = frame.frame->get_stream()->get_unique_id();
                //TODO: Ziv, remove usage of shared_ptr when frame_holder is cpoyable
                auto pf = std::make_shared<frame_holder>(std::move(frame));

                auto callback = [this, is_real_time, stream_id, pf, calc_sleep, is_paused, update_last_pushed_frame](dispatcher::cancellable_timer t)
                {                  
                    device_serializer::nanoseconds sleep_for = calc_sleep();
                    if (sleep_for.count() > 0)
                        t.try_sleep( sleep_for );

                    LOG_DEBUG("callback--> "<< *pf);

                    frame_interface* pframe = nullptr;

                    if (!is_streaming())
                        return;

                    if (is_paused())
                        return;

                    std::swap((*pf).frame, pframe);
                    
                    // We want to make sure that we have a valid reference to this shared_ptr so if
                    // "reset()" it will not destroy the object
                    auto user_callback = m_user_callback;
                    if (user_callback)
                        user_callback->on_frame((rs2_frame*)pframe);
                    
                    update_last_pushed_frame();
                    
                };
                m_dispatchers.at(stream_id)->invoke(callback, !is_real_time);

                // On non-real-time, we want the playback to run in synchronous mode:
                // The playback will dispatch each frame and wait for it callback to finish before
                // moving on to the next one.
                if( ! is_real_time )
                    m_dispatchers.at( stream_id )->flush();
            }
        }
    };
}
