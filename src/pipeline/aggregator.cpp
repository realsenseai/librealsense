// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2015 RealSense, Inc. All Rights Reserved.

#include <algorithm>
#include "stream.h"
#include "aggregator.h"
#include <src/composite-frame.h>
#include <src/core/frame-processor-callback.h>

namespace librealsense
{
    namespace pipeline
    {
        aggregator::aggregator(const std::vector<int>& streams_to_aggregate, const std::vector<int>& streams_to_sync) :
            processing_block("aggregator"),
            _queue(new single_consumer_frame_queue<frame_holder>(1)),
            _streams_to_aggregate_ids(streams_to_aggregate),
            _streams_to_sync_ids(streams_to_sync),
            _accepting(true)
        {
            set_processing_callback(
                make_frame_processor_callback( [&]( frame_holder && frame, synthetic_source_interface * source )
                                               { handle_frame( std::move( frame ), source ); } ) );
        }

        void aggregator::handle_frame(frame_holder frame, synthetic_source_interface* source)
        {
            if (!_accepting) {
                // If this causes stopping a pipeline with realtime=false playback device to
                // generate high CPU utilization for a significant length of time, adding a
                // short sleep here should mitigate it.
//                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                return;
            }
            std::lock_guard<std::mutex> lock(_mutex);
            auto comp = dynamic_cast<composite_frame*>(frame.frame);
            LOG_DEBUG( "aggregator::handle_frame: received " << (comp ? "composite" : "single")
                       << " frame, stream_type=" << frame->get_stream()->get_stream_type()
                       << " unique_id=" << frame->get_stream()->get_unique_id() );
            if (comp)
            {
                for (auto i = 0; i < comp->get_embedded_frames_count(); i++)
                {
                    auto f = comp->get_frame(i);
                    f->acquire();
                    _last_set[f->get_stream()->get_unique_id()] = f;
                    LOG_DEBUG( "aggregator::handle_frame: composite embeds stream_type="
                               << f->get_stream()->get_stream_type()
                               << " unique_id=" << f->get_stream()->get_unique_id() );
                }

                // in case not all required streams were aggregated don't publish the frame set
                for (int s : _streams_to_aggregate_ids)
                {
                    if (!_last_set[s])
                    {
                        LOG_DEBUG( "aggregator::handle_frame: composite path incomplete, missing unique_id=" << s );
                        return;
                    }
                }
                LOG_DEBUG( "aggregator::handle_frame: composite path complete, publishing frameset" );

                // prepare the output frame set for wait_for_frames/poll_frames calls
                std::vector<frame_holder> sync_set;
                // prepare the output frame set for the callbacks
                std::vector<frame_holder> async_set;
                for (auto&& s : _last_set)
                {
                    sync_set.push_back(s.second.clone());
                    // send only the synchronized frames to the user callback
                    if (std::find(_streams_to_sync_ids.begin(), _streams_to_sync_ids.end(),
                        s.second->get_stream()->get_unique_id()) != _streams_to_sync_ids.end())
                        async_set.push_back(s.second.clone());
                }

                frame_holder sync_fref = source->allocate_composite_frame(std::move(sync_set));
                frame_holder async_fref = source->allocate_composite_frame(std::move(async_set));

                if (!sync_fref || !async_fref)
                {
                    LOG_ERROR("Failed to allocate composite frame");
                    return;
                }
                // for async pipeline usage - provide only the synchronized frames to the user via callback
                source->frame_ready(async_fref.clone());

                // for sync pipeline usage - push the aggregated to the output queue
                _queue->enqueue(sync_fref.clone());
            }
            else
            {
                source->frame_ready(frame.clone());
                _last_set[frame->get_stream()->get_unique_id()] = frame.clone();
                LOG_DEBUG( "aggregator::handle_frame: single-frame path, _last_set now has "
                           << _last_set.size() << "/" << _streams_to_aggregate_ids.size()
                           << " streams, _streams_to_sync_ids.empty()="
                           << _streams_to_sync_ids.empty() );
                if (_streams_to_sync_ids.empty() && _last_set.size() == _streams_to_aggregate_ids.size())
                {
                    // prepare the output frame set for wait_for_frames/poll_frames calls
                    std::vector<frame_holder> sync_set;
                    for (auto&& s : _last_set)
                        sync_set.push_back(s.second.clone());

                    frame_holder sync_fref = source->allocate_composite_frame(std::move(sync_set));
                    if (!sync_fref)
                    {
                        LOG_ERROR("Failed to allocate composite frame");
                        return;
                    }
                    // for sync pipeline usage - push the aggregated to the output queue
                    _queue->enqueue(sync_fref.clone());
                }
            }
        }

        bool aggregator::dequeue(frame_holder* item, unsigned int timeout_ms)
        {
            return _queue->dequeue(item, timeout_ms);
        }

        bool aggregator::try_dequeue(frame_holder* item)
        {
            return _queue->try_dequeue(item);
        }

        void aggregator::start()
        {
            _accepting = true;
        }

        void aggregator::stop()
        {
            _accepting = false;
            _queue->stop();
        }
    }
}
