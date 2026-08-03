/* License: Apache 2.0. See LICENSE file in root directory. */
/* Copyright(c) 2019 RealSense, Inc. All Rights Reserved. */
#pragma once

#include "archive.h"
#include <src/core/frame-interface.h>

#include <atomic>
#include <vector>

namespace librealsense
{
    constexpr static int RS2_USER_QUEUE_SIZE = 128;

    // Defines general frames storage model
    template<class T>
    class frame_archive : public std::enable_shared_from_this<frame_archive<T>>, public archive_interface
    {
        std::atomic<uint32_t>* max_frame_queue_size;
        std::atomic<uint32_t> published_frames_count;
        small_heap<T, RS2_USER_QUEUE_SIZE> published_frames;
        std::shared_ptr<metadata_parser_map> _metadata_parsers = nullptr;
        callbacks_heap callback_inflight;

        std::vector<T> freelist; // return frames here
        std::atomic<bool> recycle_frames;
        int pending_frames = 0;
        std::recursive_mutex mutex;

        std::weak_ptr<sensor_interface> _sensor;
        std::shared_ptr<sensor_interface> get_sensor() const override { return _sensor.lock(); }
        void set_sensor( const std::weak_ptr< sensor_interface > & s ) override { _sensor = s; }

        T alloc_frame(const size_t size, frame_additional_data && additional_data, bool requires_memory)
        {
            T backbuffer;
            //const size_t size = modes[stream].get_image_size(stream);
            {
                std::lock_guard<std::recursive_mutex> guard(mutex);

                if (requires_memory)
                {
                    // Attempt to obtain a buffer of the appropriate size from the freelist
                    for (auto it = begin(freelist); it != end(freelist); ++it)
                    {
                        if (it->data.size() == size)
                        {
                            backbuffer = std::move(*it);
                            freelist.erase(it);
                            break;
                        }
                    }
                }

                // Discard buffers that have been in the freelist for longer than 1s
                for (auto it = begin(freelist); it != end(freelist);)
                {
                    if (additional_data.timestamp > it->additional_data.timestamp + 1000) it = freelist.erase(it);
                    else ++it;
                }
            }

            if (requires_memory)
            {
                backbuffer.data.resize(size, 0); // TODO: Allow users to provide a custom allocator for frame buffers
            }
            // Record the logical size even when we don't allocate `data` (requires_memory==false,
            // e.g. zero-copy capture where the pixels live in an external continuation buffer), so
            // get_frame_data_size() stays correct. Harmless for the normal path (== data.size()).
            backbuffer.set_data_size(size);
            backbuffer.additional_data = std::move( additional_data );
            return backbuffer;
        }

        frame_interface* track_frame(T& f)
        {
            std::unique_lock<std::recursive_mutex> lock(mutex);

            auto published_frame = f.publish(this->shared_from_this());
            if (published_frame)
            {
                published_frame->acquire();
                return published_frame;
            }

            LOG_DEBUG("publish(...) failed");
            return nullptr;
        }

        void unpublish_frame(frame_interface * fi) override
        {
            if( fi )
            {
                auto f = (T *)fi;
                std::unique_lock<std::recursive_mutex> lock(mutex);

                fi->keep();

                if (recycle_frames)
                {
                    freelist.push_back(std::move(*f));
                }
                lock.unlock();

                if (f->is_fixed())
                    published_frames.deallocate(f);
                else
                    delete f;
            }
        }

        void keep_frame(frame_interface* frame) override
        {
            --published_frames_count;
        }

        frame_interface * publish_frame( frame_interface * fi ) override
        {
            auto f = (T *)fi;

            unsigned int max_frames = *max_frame_queue_size;

            if (published_frames_count >= max_frames
                && max_frames)
            {
                LOG_DEBUG("User didn't release frame resource.");
                return nullptr;
            }
            auto new_frame = (max_frames ? published_frames.allocate() : new T());

            if (new_frame)
            {
                if (max_frames)
                    new_frame->mark_fixed();
            }
            else
            {
                new_frame = new T();
            }

            ++published_frames_count;
            *new_frame = std::move(*f);

            return new_frame;
        }

        std::shared_ptr<metadata_parser_map> get_md_parsers() const override { return _metadata_parsers; };

        friend class frame;

    public:
        explicit frame_archive( std::atomic< uint32_t > * in_max_frame_queue_size,
                                std::shared_ptr< metadata_parser_map > const & parsers )
            : max_frame_queue_size( in_max_frame_queue_size )
            , recycle_frames( true )
            , _metadata_parsers( parsers )
        {
            published_frames_count = 0;
        }

        callback_invocation_holder begin_callback() override
        {
            return { callback_inflight.allocate(), &callback_inflight };
        }

        void release_frame_ref(frame_interface* ref)
        {
            ref->release();
        }

        frame_interface* alloc_and_track(const size_t size, frame_additional_data && additional_data, bool requires_memory) override
        {
            auto frame = alloc_frame( size, std::move( additional_data ), requires_memory );
            return track_frame(frame);
        }

        void flush() override
        {
            published_frames.stop_allocation();
            callback_inflight.stop_allocation();
            recycle_frames = false;

            auto callbacks_inflight = callback_inflight.get_size();
            if (callbacks_inflight > 0)
            {
                LOG_WARNING(callbacks_inflight << " callbacks are still running on some other threads. Waiting until all callbacks return...");
            }
            // wait until user is done with all the stuff he chose to borrow
            callback_inflight.wait_until_empty();

            {
                std::lock_guard<std::recursive_mutex> guard(mutex);
                freelist.clear();
            }

            pending_frames = published_frames.get_size();
            if (pending_frames > 0)
            {
                LOG_INFO("The user was holding on to "
                    << std::dec << pending_frames << " frames after stream 0x"
                    << std::hex << this << " stopped" << std::dec);
            }
            // frames and their frame refs are not flushed, by design
        }

        ~frame_archive()
        {
            if (pending_frames > 0)
            {
                LOG_DEBUG("All frames from stream 0x"
                    << std::hex << this << " are now released by the user" << std::dec);
            }
        }

    };

}
