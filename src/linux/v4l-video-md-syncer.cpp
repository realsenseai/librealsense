// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "v4l-video-md-syncer.h"

#include "types.h"

#include <cstring>

#include <errno.h>

namespace librealsense
{
    namespace platform
    {
        void v4l2_video_md_syncer::push_video(const sync_buffer& video_buffer)
        {
            std::lock_guard<std::mutex> lock(_syncer_mutex);
            if(!_is_ready)
            {
                LOG_DEBUG_V4L("video_md_syncer - push_video called but syncer not ready");
                return;
            }
            _video_queue.push(video_buffer);
            LOG_DEBUG_V4L("video_md_syncer - video pushed with sequence " << video_buffer._v4l2_buf->sequence << ", buf " << video_buffer._buffer_index);

            // remove old video_buffer
            if (_video_queue.size() > 2)
            {
                // Enqueue of video buffer before throwing its content away
                enqueue_front_buffer_before_throwing_it(_video_queue);
            }
        }

        void v4l2_video_md_syncer::push_metadata(const sync_buffer& md_buffer)
        {
            std::lock_guard<std::mutex> lock(_syncer_mutex);
            if(!_is_ready)
            {
                LOG_DEBUG_V4L("video_md_syncer - push_metadata called but syncer not ready");
                return;
            }
            // override front buffer if it has the same sequence that the new buffer - happens with metadata sequence 0
            if (_md_queue.size() > 0 && _md_queue.front()._v4l2_buf->sequence == md_buffer._v4l2_buf->sequence)
            {
                LOG_DEBUG_V4L("video_md_syncer - calling enqueue_front_buffer_before_throwing_it - md buf " << md_buffer._buffer_index << " and md buf " << _md_queue.front()._buffer_index << " have same sequence");
                enqueue_front_buffer_before_throwing_it(_md_queue);
            }
            _md_queue.push(md_buffer);
            LOG_DEBUG_V4L("video_md_syncer - md pushed with sequence " << md_buffer._v4l2_buf->sequence << ", buf " << md_buffer._buffer_index);
            LOG_DEBUG_V4L("video_md_syncer - md queue size = " << _md_queue.size());

            // remove old md_buffer
            if (_md_queue.size() > 2)
            {
                LOG_DEBUG_V4L("video_md_syncer - calling enqueue_front_buffer_before_throwing_it - md queue size is: " << _md_queue.size());
                // Enqueue of md buffer before throwing its content away
                enqueue_front_buffer_before_throwing_it(_md_queue);
            }
        }

        bool v4l2_video_md_syncer::pull_video_with_metadata(std::shared_ptr<v4l2_buffer>& video_buffer, std::shared_ptr<v4l2_buffer>& md_buffer,
                                                            int& video_fd, int& md_fd)
        {
            std::lock_guard<std::mutex> lock(_syncer_mutex);
            if(!_is_ready)
            {
                LOG_DEBUG_V4L("video_md_syncer - pull_video_with_metadata called but syncer not ready");
                return false;
            }
            if (_video_queue.empty())
            {
                LOG_DEBUG_V4L("video_md_syncer - video queue is empty");
                return false;
            }

            if (_md_queue.empty())
            {
                LOG_DEBUG_V4L("video_md_syncer - md queue is empty");
                return false;
            }

            sync_buffer video_candidate = _video_queue.front();
            sync_buffer md_candidate = _md_queue.front();

            // set video and md file descriptors
            video_fd = video_candidate._fd;
            md_fd = md_candidate._fd;

            // sync is ok if latest video and md have the same sequence
            if (video_candidate._v4l2_buf->sequence == md_candidate._v4l2_buf->sequence)
            {
                video_buffer = video_candidate._v4l2_buf;
                md_buffer = md_candidate._v4l2_buf;
                // removing from queues
                _video_queue.pop();
                _md_queue.pop();
                LOG_DEBUG_V4L("video_md_syncer - video and md pulled with sequence " << video_candidate._v4l2_buf->sequence);
                return true;
            }

            LOG_DEBUG_V4L("video_md_syncer - video_candidate seq " << video_candidate._v4l2_buf->sequence << ", md_candidate seq " << md_candidate._v4l2_buf->sequence);

            if (video_candidate._v4l2_buf->sequence > md_candidate._v4l2_buf->sequence && _md_queue.size() > 1)
            {
                // Enqueue of md buffer before throwing its content away
                enqueue_buffer_before_throwing_it(md_candidate);
                _md_queue.pop();

                // checking remaining metadata buffer in queue
                auto alternative_md_candidate = _md_queue.front();
                // sync is ok if latest video and md have the same sequence
                if (video_candidate._v4l2_buf->sequence == alternative_md_candidate._v4l2_buf->sequence)
                {
                    video_buffer = video_candidate._v4l2_buf;
                    md_buffer = alternative_md_candidate._v4l2_buf;
                    // removing from queues
                    _video_queue.pop();
                    _md_queue.pop();
                    LOG_DEBUG_V4L("video_md_syncer - video and md pulled with sequence " << video_candidate._v4l2_buf->sequence);
                    return true;
                }
            }
            if (video_candidate._v4l2_buf->sequence < md_candidate._v4l2_buf->sequence && _video_queue.size() > 1)
            {
                // Enqueue of md buffer before throwing its content away
                enqueue_buffer_before_throwing_it(video_candidate);
                _video_queue.pop();

                // checking remaining video buffer in queue
                auto alternative_video_candidate = _video_queue.front();
                // sync is ok if latest video and md have the same sequence
                if (alternative_video_candidate._v4l2_buf->sequence == md_candidate._v4l2_buf->sequence)
                {
                    video_buffer = alternative_video_candidate._v4l2_buf;
                    md_buffer = md_candidate._v4l2_buf;
                    // removing from queues
                    _video_queue.pop();
                    _md_queue.pop();
                    LOG_DEBUG_V4L("video_md_syncer - video and md pulled with sequence " << md_candidate._v4l2_buf->sequence);
                    return true;
                }
            }
            return false;
        }

        void v4l2_video_md_syncer::report_qbuf_failure(int fd)
        {
            if (errno == ENODEV)
            {
                _qbuf_device_disconnected = true;
                LOG_WARNING("Device disconnected: QBUF failed with ENODEV for fd " << fd);
                return;
            }
            LOG_ERROR("xioctl(VIDIOC_QBUF) failed when requesting new frame! fd: " << fd << " error: " << strerror(errno));
        }

        void v4l2_video_md_syncer::enqueue_buffer_before_throwing_it(const sync_buffer& sb)
        {
            // Enqueue of buffer before throwing its content away
            LOG_DEBUG_V4L("video_md_syncer - Enqueue buf " << std::dec << sb._buffer_index << " for fd " << sb._fd << " before dropping it");
            if (xioctl(sb._fd, VIDIOC_QBUF, sb._v4l2_buf.get()) < 0)
                report_qbuf_failure(sb._fd);
        }

        void v4l2_video_md_syncer::enqueue_front_buffer_before_throwing_it(std::queue<sync_buffer>& sync_queue)
        {
            // Enqueue of buffer before throwing its content away
            LOG_DEBUG_V4L("video_md_syncer - Enqueue buf " << std::dec << sync_queue.front()._buffer_index << " for fd " << sync_queue.front()._fd << " before dropping it");
            if (xioctl(sync_queue.front()._fd, VIDIOC_QBUF, sync_queue.front()._v4l2_buf.get()) < 0)
                report_qbuf_failure(sync_queue.front()._fd);
            sync_queue.pop();
        }


        void v4l2_video_md_syncer::stop()
        {
             _is_ready = false;
             flush_queues();
        }

        void v4l2_video_md_syncer::flush_queues()
        {
            // Empty queues
            LOG_DEBUG_V4L("video_md_syncer - flush video and md queues");
            std::lock_guard<std::mutex> lock(_syncer_mutex);
            while(!_video_queue.empty())
            {
                _video_queue.pop();
            }
            while(!_md_queue.empty())
            {
                _md_queue.pop();
            }
            LOG_DEBUG_V4L("video_md_syncer - flush video and md queues done - mq_q size = " << _md_queue.size() << ", video_q size = " << _video_queue.size());
        }
    }  // namespace platform
}  // namespace librealsense
