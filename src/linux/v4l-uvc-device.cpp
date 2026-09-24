// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "v4l-uvc-device.h"

#include "v4l-product-quirks.h"
#include <src/platform/hid-data.h>
#include <src/core/time-service.h>
#include <src/core/notification.h>
#include <src/librealsense-exception.h>
#include <rsutils/string/from.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <regex>
#include <set>
#include <sstream>

#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <linux/uvcvideo.h>
#include <linux/usb/video.h>

#pragma GCC diagnostic ignored "-Woverflow"

constexpr std::chrono::milliseconds DISCONNECT_RETRY_DELAY( 100 );

namespace librealsense
{
    namespace platform
    {
        v4l_uvc_device::v4l_uvc_device(const uvc_device_info& info, bool use_memory_map)
            : _name(info.id), 
              _device_path(info.device_path),
              _device_usb_spec(info.usb_conn_spec),
              _info(info),
              _is_capturing(false),
              _is_alive(true),
              _is_started(false),
              _thread(nullptr),
              _named_mtx(nullptr),
              _use_memory_map(use_memory_map),
              _fd(-1),
              _stop_pipe_fd{},
              _buf_dispatch(use_memory_map),
              _frame_drop_monitor(DEFAULT_KPI_FRAME_DROPS_PERCENTAGE),
              _are_device_capabilities_assigned(false)
        {
            _named_mtx = std::unique_ptr<named_mutex>(new named_mutex(_name, 5000));
        }

        v4l_uvc_device::~v4l_uvc_device()
        {
            _is_capturing = false;
            if (_thread && _thread->joinable()) _thread->join();
            for (auto&& fd : _fds)
            {
                try { if (fd) ::close(fd);} catch (...) {}
            }
        }

        void v4l_uvc_device::probe_and_commit(stream_profile profile, frame_callback callback, int buffers)
        {
            if(!_is_capturing && !_callback)
            {
                v4l2_fmtdesc pixel_format = {};
                pixel_format.type = _dev.buf_type;

                _variable_frame_size = false;
                while (ioctl(_fd, VIDIOC_ENUM_FMT, &pixel_format) == 0)
                {
                    v4l2_frmsizeenum frame_size = {};
                    frame_size.pixel_format = pixel_format.pixelformat;

                    uint32_t fourcc = (const big_endian<int> &)pixel_format.pixelformat;

                    // V4L2_FMT_FLAG_COMPRESSED means in v4l2 if the frame size isn't fixed - sizeimage is a maximum, not exact
                    if (fourcc == profile.format)
                        _variable_frame_size = (pixel_format.flags & V4L2_FMT_FLAG_COMPRESSED) != 0;

                    if (pixel_format.pixelformat == 0)
                    {
                        // Microsoft Depth GUIDs for R400 series are not yet recognized
                        // by the Linux kernel, but they do not require a patch, since there
                        // are "backup" Z16 and Y8 formats in place
                        static const std::set<std::string> pending_formats = {
                            "00000050-0000-0010-8000-00aa003",
                            "00000032-0000-0010-8000-00aa003",
                        };

                        if (std::find(pending_formats.begin(),
                                      pending_formats.end(),
                                      (const char*)pixel_format.description) ==
                            pending_formats.end())
                        {
                            const std::string s(rsutils::string::from() << "!" << pixel_format.description);
                            std::regex rgx("!([0-9a-f]+)-.*");
                            std::smatch match;

                            if (std::regex_search(s.begin(), s.end(), match, rgx))
                            {
                                std::stringstream ss;
                                ss <<  match[1];
                                int id;
                                ss >> std::hex >> id;
                                fourcc = (const big_endian<int> &)id;

                                if (fourcc == profile.format)
                                {
                                    throw linux_backend_exception(rsutils::string::from() << "The requested pixel format '"  << fourcc_to_string(id)
                                                                  << "' is not natively supported by the running Linux kernel and likely requires a patch");
                                }
                            }
                        }
                    }
                    ++pixel_format.index;
                }

                set_format(profile);

                v4l2_streamparm parm = {};
                parm.type = _dev.buf_type;
                if(xioctl(_fd, VIDIOC_G_PARM, &parm) < 0)
                    throw linux_backend_exception("xioctl(VIDIOC_G_PARM) failed");

                parm.parm.capture.timeperframe.numerator = 1;
                parm.parm.capture.timeperframe.denominator = profile.fps;
                if(xioctl(_fd, VIDIOC_S_PARM, &parm) < 0)
                    throw linux_backend_exception("xioctl(VIDIOC_S_PARM) failed");

                // Init memory mapped IO
                negotiate_kernel_buffers(static_cast<size_t>(buffers));
                allocate_io_buffers(static_cast<size_t>(buffers));

                _profile =  profile;
                _callback = callback;
            }
            else
            {
                throw wrong_api_call_sequence_exception("Device already streaming!");
            }
        }

        void v4l_uvc_device::stream_on(std::function<void(const notification& n)> error_handler)
        {
            if(!_is_capturing)
            {
                _error_handler = error_handler;

                // Start capturing
                prepare_capture_buffers();

                // Synchronise stream requests for meta and video data.
                streamon();

                _is_capturing = true;
                _thread = std::unique_ptr<std::thread>(new std::thread([this](){ capture_loop(); }));

                // Starting the video/metadata syncer
                _video_md_syncer.start();
            }
        }

        void v4l_uvc_device::prepare_capture_buffers()
        {
            for (auto&& buf : _buffers) buf->prepare_for_streaming(_fd);
        }

        void v4l_uvc_device::stop_data_capture()
        {
            _is_capturing = false;
            _is_started = false;

            // Stop nn-demand frames polling
            signal_stop();

            _thread->join();
            _thread.reset();

            // Notify kernel
            streamoff();
        }

        void v4l_uvc_device::start_callbacks()
        {
            _is_started = true;
        }

        void v4l_uvc_device::stop_callbacks()
        {
            _is_started = false;
        }

        void v4l_uvc_device::close(stream_profile)
        {
            if(_is_capturing)
            {
                stop_data_capture();
            }

            if (_callback)
            {
                // Release allocated buffers
                allocate_io_buffers(0);

                // Release IO
                negotiate_kernel_buffers(0);

                _callback = nullptr;
            }
        }

        void v4l_uvc_device::signal_stop()
        {
            _video_md_syncer.stop();;
            char buff[1]={};
            if (write(_stop_pipe_fd[1], buff, 1) < 0)
            {
                 throw linux_backend_exception("Could not signal video capture thread to stop. Error write to pipe.");
            }
        }

        std::string time_in_HH_MM_SS_MMM()
        {
            using namespace std::chrono;

            // get current time
            auto now = system_clock::now();

            // get number of milliseconds for the current second
            // (remainder after division into seconds)
            auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

            // convert to std::time_t in order to convert to std::tm (broken time)
            auto timer = system_clock::to_time_t(now);

            // convert to broken time
            std::tm bt = *std::localtime(&timer);

            std::ostringstream oss;

            oss << std::put_time(&bt, "%H:%M:%S"); // HH:MM:SS
            oss << '.' << std::setfill('0') << std::setw(3) << ms.count();
            return oss.str();
        }

        bool v4l_uvc_device::handle_enodev_on_dqbuf(const char* fd_label, int fd)
        {
            if (errno != ENODEV)
                return false;
            _device_disconnected = true;
            LOG_WARNING("Device disconnected: DQBUF failed with ENODEV for " << fd_label << " " << fd);
            return true;
        }

        void v4l_uvc_device::poll()
        {
            // A prior iteration observed ENODEV on a real DQBUF/QBUF call (set explicitly at the point of
            // failure, never inferred from ambient errno). Throttle retries until the device-removal
            // notification unwinds streaming, instead of spinning select() on an fd the kernel already dropped.
            // Both flags are read into locals before the check below, so neither is left unconsumed by the ||.
            bool device_disconnected = _device_disconnected.exchange(false);
            bool syncer_disconnected = _video_md_syncer.consume_device_disconnected();
            if (device_disconnected || syncer_disconnected)
            {
                std::this_thread::sleep_for(DISCONNECT_RETRY_DELAY);
                return;
            }

             fd_set fds{};
             FD_ZERO(&fds);
             for (auto fd : _fds)
             {
                 FD_SET(fd, &fds);
             }

            struct timespec mono_time;
            int ret = clock_gettime(CLOCK_MONOTONIC, &mono_time);
            if (ret) throw linux_backend_exception("could not query time!");

            struct timeval expiration_time = { mono_time.tv_sec + 5, mono_time.tv_nsec / 1000 };
            int val = 0;

            auto realtime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            auto time_since_epoch = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
            LOG_DEBUG_V4L("Select initiated at " << time_in_HH_MM_SS_MMM() << ", mono time " << time_since_epoch << ", host time " << realtime );
            do {
                struct timeval remaining;
                ret = clock_gettime(CLOCK_MONOTONIC, &mono_time);
                if (ret) throw linux_backend_exception("could not query time!");

                struct timeval current_time = { mono_time.tv_sec, mono_time.tv_nsec / 1000 };
                timersub(&expiration_time, &current_time, &remaining);
                // timercmp fails cpp check, reduce macro function from time.h
# define timercmp_lt(a, b) \
  (((a)->tv_sec == (b)->tv_sec) ? ((a)->tv_usec < (b)->tv_usec) : ((a)->tv_sec < (b)->tv_sec))
                if (timercmp_lt(&current_time, &expiration_time)) {
                    val = select(_max_fd + 1, &fds, nullptr, nullptr, &remaining);
                }
                else {
                    val = 0;
                    LOG_DEBUG_V4L("Select timeouted");
                }

                if (val< 0)
                    LOG_DEBUG_V4L("Select interrupted, val = " << val << ", error = " << errno);
            } while (val < 0 && errno == EINTR);

            LOG_DEBUG_V4L("Select done, val = " << val << " at " << time_in_HH_MM_SS_MMM());
            if(val < 0)
            {
                _is_capturing = false;
                _is_started = false;

                // Notify kernel
                streamoff();
            }
            else
            {
                if(val > 0)
                {
                    if(FD_ISSET(_stop_pipe_fd[0], &fds) || FD_ISSET(_stop_pipe_fd[1], &fds))
                    {
                        if(!_is_capturing)
                        {
                            LOG_INFO("V4L stream is closed");
                            return;
                        }
                        else
                        {
                            LOG_ERROR("Stop pipe was signalled during streaming");
                            return;
                        }
                    }
                    else // Check and acquire data buffers from kernel
                    {
                        bool md_extracted = false;
                        bool keep_md = false;
                        bool wa_applied = false;
                        buffers_mgr buf_mgr(_use_memory_map);
                        if (_buf_dispatch.metadata_size())
                        {
                            buf_mgr = _buf_dispatch;    // Handle over MD buffer from the previous cycle
                            md_extracted = true;
                            wa_applied = true;
                            _buf_dispatch.set_md_attributes(0,nullptr);
                        }

                        // Relax the required frame size for compressed formats, i.e. MJPG, Z16H
                        // The D5xx mapping streams need the same relaxation: their descriptor
                        // advertises the occupancy/point-cloud canvas, while the payload on the
                        // wire is a MAP1 frame whose length is the data, not width*height*bpp.
                        // Without this every frame is rejected as incomplete.
                        bool compressed_format = val_in_range(_profile.format, { 0x4d4a5047U , 0x5a313648U})
                                              || is_d5xx_mapping_interface( _info.pid, _info.mi );

                        // Compressed and kernel-reported variable-size formats deliver frames shorter than the buffer,
                        // so the size check doesn't apply - this covers the perception stream too.
                        bool skip_partial_frame_check = compressed_format || _variable_frame_size;

                        // METADATA STREAM
                        // Read metadata. Metadata node performs a blocking call to ensure video and metadata sync
                        acquire_metadata(buf_mgr,fds,compressed_format);
                        md_extracted = true;

                        if (wa_applied)
                        {
                            auto fn = *(uint32_t*)((char*)(buf_mgr.metadata_start())+28);
                            LOG_DEBUG_V4L("Extracting md buff, fn = " << fn);
                        }

                        // VIDEO STREAM
                        if(FD_ISSET(_fd, &fds))
                        {
                            FD_CLR(_fd,&fds);
                            v4l2_buffer buf = {};
                            struct v4l2_plane planes[VIDEO_MAX_PLANES] = {};
                            buf.type = _dev.buf_type;
                            buf.memory = _use_memory_map ? V4L2_MEMORY_MMAP : V4L2_MEMORY_USERPTR;
                            if (_dev.buf_type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
                                buf.m.planes = planes;
                                buf.length = VIDEO_MAX_PLANES;
                            }
                            if(xioctl(_fd, VIDIOC_DQBUF, &buf) < 0)
                            {
                                if (handle_enodev_on_dqbuf("fd", _fd))
                                    return;
                                LOG_DEBUG_V4L("Dequeued empty buf for fd " << std::dec << _fd);
                            }
                            LOG_DEBUG_V4L("Dequeued buf " << std::dec << buf.index << " for fd " << _fd << " seq " << buf.sequence);
                            buf.type = _dev.buf_type;
                            buf.memory = _use_memory_map ? V4L2_MEMORY_MMAP : V4L2_MEMORY_USERPTR;
                            if (_dev.buf_type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
                                buf.bytesused = buf.m.planes[0].bytesused;
                            }
                            auto buffer = _buffers[buf.index];
                            buf_mgr.handle_buffer(e_video_buf, _fd, buf, buffer);

                            if (_is_started)
                            {
                                if(buf.bytesused == 0)
                                {
                                    LOG_DEBUG_V4L("Empty video frame arrived, index " << buf.index);
                                    return;
                                }

                                // Drop partial and overflow frames (assumes D4XX metadata only)
                                bool partial_frame = (!skip_partial_frame_check && (buf.bytesused < buffer->get_full_length() - MAX_META_DATA_SIZE));
                                bool overflow_frame = (buf.bytesused ==  buffer->get_length_frame_only() + MAX_META_DATA_SIZE);
                                if (_dev.buf_type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
                                    /* metadata size is one line of profile, temporary disable validation */
                                    partial_frame = false;
                                    overflow_frame = false;
                                }
                                if (partial_frame || overflow_frame)
                                {
                                    auto percentage = (100 * buf.bytesused) / buffer->get_full_length();
                                    std::stringstream s;
                                    if (partial_frame)
                                    {
                                        s << "Incomplete video frame detected!\nSize " << buf.bytesused
                                            << " out of " << buffer->get_full_length() << " bytes (" << percentage << "%)";
                                        if (overflow_frame)
                                        {
                                            s << ". Overflow detected: payload size " << buffer->get_length_frame_only();
                                            LOG_ERROR("Corrupted UVC frame data, underflow and overflow reported:\n" << s.str().c_str());
                                        }
                                    }
                                    else
                                    {
                                        if (overflow_frame)
                                            s << "overflow video frame detected!\nSize " << buf.bytesused
                                                << ", payload size " << buffer->get_length_frame_only();
                                    }
                                    LOG_DEBUG("Incomplete frame received: " << s.str()); // Ev -try1
                                    bool kpi_violated = _frame_drop_monitor.update_and_check_kpi(_profile, buf.timestamp);
                                    if (kpi_violated)
                                    {
                                        librealsense::notification n = { RS2_NOTIFICATION_CATEGORY_FRAME_CORRUPTED, 0, RS2_LOG_SEVERITY_WARN, s.str() };
                                        _error_handler(n);
                                    }
                                    
                                    // Check if metadata was already allocated
                                    if (buf_mgr.metadata_size())
                                    {
                                        LOG_WARNING("Metadata was present when partial frame arrived, mark md as extracted");
                                        md_extracted = true;
                                        LOG_DEBUG_V4L("Discarding md due to invalid video payload");
                                        auto md_buf = buf_mgr.get_buffers().at(e_metadata_buf);
                                        md_buf._data_buf->request_next_frame(md_buf._file_desc,true);
                                    }
                                }
                                else
                                {
                                    if (!_info.has_metadata_node)
                                    {
                                        if(has_metadata())
                                        {
                                            auto timestamp = (double)buf.timestamp.tv_sec*1000.f + (double)buf.timestamp.tv_usec/1000.f;
                                            timestamp = monotonic_to_realtime(timestamp);

                                            // Read metadata. Metadata node performs a blocking call to ensure video and metadata sync
                                            acquire_metadata(buf_mgr,fds,compressed_format);
                                            md_extracted = true;

                                            if (wa_applied)
                                            {
                                                auto fn = *(uint32_t*)((char*)(buf_mgr.metadata_start())+28);
                                                LOG_DEBUG_V4L("Extracting md buff, fn = " << fn);
                                            }

                                            auto frame_sz = buf_mgr.md_node_present() ? buf.bytesused :
                                                                std::min(buf.bytesused - buf_mgr.metadata_size(), buffer->get_length_frame_only());
                                            frame_object fo{ frame_sz, buf_mgr.metadata_size(),
                                                             buffer->get_frame_start(), buf_mgr.metadata_start(), timestamp };

                                            buffer->attach_buffer(buf);
                                            buf_mgr.handle_buffer(e_video_buf,-1); // transfer new buffer request to the frame callback

                                            if (buf_mgr.verify_vd_md_sync())
                                            {
                                                //Invoke user callback and enqueue next frame
                                                _callback(_profile, fo, [buf_mgr]() mutable {
                                                    buf_mgr.request_next_frame();
                                                });
                                            }
                                            else
                                            {
                                                LOG_WARNING("Video frame dropped, video and metadata buffers inconsistency");
                                            }
                                        }
                                        else // when metadata is not enabled at all, streaming only video
                                        {
                                            auto timestamp = (double)buf.timestamp.tv_sec * 1000.f + (double)buf.timestamp.tv_usec / 1000.f;
                                            timestamp = monotonic_to_realtime(timestamp);

                                            LOG_DEBUG_V4L("no metadata streamed");
                                            if (buf_mgr.verify_vd_md_sync())
                                            {
                                                buffer->attach_buffer(buf);
                                                buf_mgr.handle_buffer(e_video_buf, -1); // transfer new buffer request to the frame callback


                                                auto frame_sz = buf_mgr.md_node_present() ? buf.bytesused :
                                                                    std::min(buf.bytesused - buf_mgr.metadata_size(),
                                                                             buffer->get_length_frame_only());

                                                uint8_t md_size = buf_mgr.metadata_size();
                                                void* md_start = buf_mgr.metadata_start();

                                                // IMU node (mi=4) delivers data with no metadata node, synthesize the metadata from the payload.
                                                // Frame size is 64 bytes on D400 but 256 on D500.
                                                metadata_hid_raw meta_data{};
                                                if (md_size == 0 && _info.mi == 4)
                                                {
                                                    populate_imu_data(meta_data, buffer->get_frame_start(), md_size, &md_start);
                                                }

                                                frame_object fo{ frame_sz, md_size,
                                                            buffer->get_frame_start(), md_start, timestamp };

                                                //Invoke user callback and enqueue next frame
                                                _callback(_profile, fo, [buf_mgr]() mutable {
                                                    buf_mgr.request_next_frame();
                                                });
                                            }
                                            else
                                            {
                                                LOG_WARNING("Video frame dropped, video and metadata buffers inconsistency");
                                            }
                                        }
                                    }
                                    else
                                    {
                                        // saving video buffer to syncer
                                        _video_md_syncer.push_video({std::make_shared<v4l2_buffer>(buf), _fd, buf.index});
                                        buf_mgr.handle_buffer(e_video_buf, -1);
                                    }
                                }
                            }
                            else
                            {
                                LOG_DEBUG_V4L("Video frame arrived in idle mode."); // TODO - verification
                            }
                        }
                        else
                        {
                            if (_is_started)
                                keep_md = true;
                            LOG_DEBUG("FD_ISSET: no data on video node sink");
                        }

                        // pulling synchronized video and metadata and uploading them to user's callback
                        upload_video_and_metadata_from_syncer(buf_mgr);
                    }
                }
                else // (val==0)
                {
                    LOG_WARNING("Frames didn't arrived within 5 seconds");
                    librealsense::notification n = {RS2_NOTIFICATION_CATEGORY_FRAMES_TIMEOUT, 0, RS2_LOG_SEVERITY_WARN,  "Frames didn't arrived within 5 seconds"};

                    _error_handler(n);
                }
            }
        }

        void v4l_uvc_device::populate_imu_data(metadata_hid_raw& meta_data, uint8_t* frame_start, uint8_t& md_size, void** md_start) const
        {
            meta_data.header.report_type = md_hid_report_type::hid_report_imu;
            meta_data.header.length = hid_header_size + metadata_imu_report_size;
            meta_data.header.timestamp = *(reinterpret_cast<uint64_t *>(frame_start + offsetof(hid_mipi_data, hwTs)));
            // Payload:
            meta_data.report_type.imu_report.header.md_type_id = md_type::META_DATA_HID_IMU_REPORT_ID;
            meta_data.report_type.imu_report.header.md_size = metadata_imu_report_size;

            md_size = sizeof(metadata_hid_raw);
            *md_start = &meta_data;
        }


        void v4l_uvc_device::upload_video_and_metadata_from_syncer(buffers_mgr& buf_mgr)
        {
            // uploading to user's callback
            std::shared_ptr<v4l2_buffer> video_v4l2_buffer;
            std::shared_ptr<v4l2_buffer> md_v4l2_buffer;

            if (_is_started && is_metadata_streamed())
            {
                int video_fd = -1, md_fd = -1;
                if (_video_md_syncer.pull_video_with_metadata(video_v4l2_buffer, md_v4l2_buffer, video_fd, md_fd))
                {
                    // Preparing video buffer
                    auto video_buffer = get_video_buffer(video_v4l2_buffer->index);
                    video_buffer->attach_buffer(*video_v4l2_buffer);

                    // happens when the video did not arrive on
                    // the current polling iteration (was taken from the syncer's video queue)
                    if (buf_mgr.get_buffers()[e_video_buf]._file_desc == -1)
                    {
                        buf_mgr.handle_buffer(e_video_buf, video_fd, *video_v4l2_buffer, video_buffer);
                    }
                    buf_mgr.handle_buffer(e_video_buf, -1); // transfer new buffer request to the frame callback

                    // Preparing metadata buffer
                    auto metadata_buffer = get_md_buffer(md_v4l2_buffer->index);
                    set_metadata_attributes(buf_mgr, md_v4l2_buffer->bytesused, metadata_buffer->get_frame_start());
                    metadata_buffer->attach_buffer(*md_v4l2_buffer);

                    if (buf_mgr.get_buffers()[e_metadata_buf]._file_desc == -1)
                    {
                        buf_mgr.handle_buffer(e_metadata_buf, md_fd, *md_v4l2_buffer, metadata_buffer);
                    }
                    buf_mgr.handle_buffer(e_metadata_buf, -1); // transfer new buffer request to the frame callback

                    auto frame_sz = buf_mgr.md_node_present() ? video_v4l2_buffer->bytesused :
                                        std::min(video_v4l2_buffer->bytesused - buf_mgr.metadata_size(),
                                                 video_buffer->get_length_frame_only());

                    auto timestamp = (double)video_v4l2_buffer->timestamp.tv_sec * 1000.f + (double)video_v4l2_buffer->timestamp.tv_usec / 1000.f;
                    timestamp = monotonic_to_realtime(timestamp);

                    // D457 work - to work with "normal camera", use frame_sz as the first input to the following frame_object:
                    //frame_object fo{ buf.bytesused - MAX_META_DATA_SIZE, buf_mgr.metadata_size(),
                    frame_object fo{ frame_sz, buf_mgr.metadata_size(),
                                     video_buffer->get_frame_start(), buf_mgr.metadata_start(), timestamp };

                    //Invoke user callback and enqueue next frame
                    _callback(_profile, fo, [buf_mgr]() mutable {
                        buf_mgr.request_next_frame();
                    });
                }
                else
                {
                    LOG_DEBUG("video_md_syncer - synchronized video and md could not be pulled");
                }
            }
        }

        void v4l_uvc_device::set_metadata_attributes(buffers_mgr& buf_mgr, __u32 bytesused, uint8_t* md_start)
        {
            size_t uvc_md_start_offset = sizeof(uvc_meta_buffer::ns) + sizeof(uvc_meta_buffer::sof);
            buf_mgr.set_md_attributes(bytesused - uvc_md_start_offset,
                                        md_start + uvc_md_start_offset);
        }
        void v4l_uvc_device::acquire_metadata(buffers_mgr& buf_mgr,fd_set &, bool compressed_format)
        {
            if (has_metadata())
                buf_mgr.set_md_from_video_node(compressed_format);
            else
                buf_mgr.set_md_attributes(0, nullptr);
        }

        void v4l_uvc_device::set_power_state(power_state state)
        {
            // calling fd.open leads to state D0 (active)
            // calling fd.close lead to state D3 (idle)
            if (state == D0 && _state == D3)
            {
                map_device_descriptor();
            }
            if (state == D3 && _state == D0)
            {
                close(_profile);
                unmap_device_descriptor();
            }
            _state = state;
        }

        bool v4l_uvc_device::set_xu(const extension_unit& xu, uint8_t control, const uint8_t* data, int size)
        {
            uvc_xu_control_query q = {static_cast<uint8_t>(xu.unit), control, UVC_SET_CUR,
                                      static_cast<uint16_t>(size), const_cast<uint8_t *>(data)};
            if(xioctl(_fd, UVCIOC_CTRL_QUERY, &q) < 0)
            {
                if (errno == EIO || errno == EAGAIN || errno == EBUSY)
                    return false;

                throw linux_backend_exception(rsutils::string::from() << "set_xu(...). xioctl(UVCIOC_CTRL_QUERY) failed on control "
                                              << static_cast< int >( control ));
            }

            return true;
        }
        bool v4l_uvc_device::get_xu(const extension_unit& xu, uint8_t control, uint8_t* data, int size) const
        {
            memset(data, 0, size);
            uvc_xu_control_query q = {static_cast<uint8_t>(xu.unit), control, UVC_GET_CUR,
                                      static_cast<uint16_t>(size), const_cast<uint8_t *>(data)};
            if(xioctl(_fd, UVCIOC_CTRL_QUERY, &q) < 0)
            {
                if (errno == EIO || errno == EAGAIN || errno == EBUSY)
                    return false;

                throw linux_backend_exception(rsutils::string::from() << "get_xu(...). xioctl(UVCIOC_CTRL_QUERY) failed on control "
                                              << static_cast< int >( control ));
            }

            return true;
        }
        control_range v4l_uvc_device::get_xu_range(const extension_unit& xu, uint8_t control, int len) const
        {
            control_range result{};
            __u16 size = 0;
            //__u32 value = 0;      // all of the real sense extended controls are up to 4 bytes
                                    // checking return value for UVC_GET_LEN and allocating
                                    // appropriately might be better
            //__u8 * data = (__u8 *)&value;
            // MS XU controls are partially supported only
            struct uvc_xu_control_query xquery = {};
            memset(&xquery, 0, sizeof(xquery));
            xquery.query = UVC_GET_LEN;
            xquery.size = 2; // size seems to always be 2 for the LEN query, but
                             //doesn't seem to be documented. Use result for size
                             //in all future queries of the same control number
            xquery.selector = control;
            xquery.unit = xu.unit;
            xquery.data = (__u8 *)&size;

            if(-1 == ioctl(_fd,UVCIOC_CTRL_QUERY,&xquery)){
                throw linux_backend_exception(rsutils::string::from() << "xioctl(UVCIOC_CTRL_QUERY) failed on control "
                                              << static_cast< int >( control ));
            }

            if( size > len )
                throw linux_backend_exception( rsutils::string::from()
                    << "get_xu_range: UVC_GET_LEN size " << size << " > requested " << len
                    << " on control " << static_cast< int >( control ) );

            std::vector<uint8_t> buf;
            auto buf_size = std::max((size_t)len,sizeof(__u32));
            buf.resize(buf_size);

            xquery.query = UVC_GET_MIN;
            xquery.size = size;
            xquery.selector = control;
            xquery.unit = xu.unit;
            xquery.data = buf.data();
            if(-1 == ioctl(_fd,UVCIOC_CTRL_QUERY,&xquery)){
                throw linux_backend_exception("xioctl(UVC_GET_MIN) failed");
            }
            result.min.resize(buf_size);
            std::copy(buf.begin(), buf.end(), result.min.begin());

            xquery.query = UVC_GET_MAX;
            xquery.size = size;
            xquery.selector = control;
            xquery.unit = xu.unit;
            xquery.data = buf.data();
            if(-1 == ioctl(_fd,UVCIOC_CTRL_QUERY,&xquery)){
                throw linux_backend_exception("xioctl(UVC_GET_MAX) failed");
            }
            result.max.resize(buf_size);
            std::copy(buf.begin(), buf.end(), result.max.begin());

            xquery.query = UVC_GET_DEF;
            xquery.size = size;
            xquery.selector = control;
            xquery.unit = xu.unit;
            xquery.data = buf.data();
            if(-1 == ioctl(_fd,UVCIOC_CTRL_QUERY,&xquery)){
                throw linux_backend_exception("xioctl(UVC_GET_DEF) failed");
            }
            result.def.resize(buf_size);
            std::copy(buf.begin(), buf.end(), result.def.begin());

            xquery.query = UVC_GET_RES;
            xquery.size = size;
            xquery.selector = control;
            xquery.unit = xu.unit;
            xquery.data = buf.data();
            if(-1 == ioctl(_fd,UVCIOC_CTRL_QUERY,&xquery)){
                throw linux_backend_exception("xioctl(UVC_GET_CUR) failed");
            }
            result.step.resize(buf_size);
            std::copy(buf.begin(), buf.end(), result.step.begin());

           return result;
        }

        bool v4l_uvc_device::get_pu(rs2_option opt, int32_t& value) const
        {
            struct v4l2_control control = {get_cid(opt), 0};
            if (xioctl(_fd, VIDIOC_G_CTRL, &control) < 0)
            {
                if (errno == EIO || errno == EAGAIN || errno == EBUSY)
                    return false;

                throw linux_backend_exception(rsutils::string::from()
                                              << "xioctl(VIDIOC_G_CTRL) failed on option " << rs2_option_to_string(opt)
                                              << ", errno=" << errno );
            }

            if (RS2_OPTION_ENABLE_AUTO_EXPOSURE==opt)  { control.value = (V4L2_EXPOSURE_MANUAL==control.value) ? 0 : 1; }
            value = control.value;

            return true;
        }

        bool v4l_uvc_device::set_pu(rs2_option opt, int32_t value)
        {
            struct v4l2_control control = {get_cid(opt), value};
            if (RS2_OPTION_ENABLE_AUTO_EXPOSURE==opt) { control.value = value ? V4L2_EXPOSURE_APERTURE_PRIORITY : V4L2_EXPOSURE_MANUAL; }
            
            // We chose not to protect the subscribe / unsubscribe with mutex due to performance reasons,
            // we prefer returning on timeout (and let the retry mechanism try again if exist) than blocking the main thread on every set command


            // RAII to handle unsubscribe in case of exceptions
            std::unique_ptr< uint32_t, std::function< void( uint32_t * ) > > unsubscriber(
                new uint32_t( control.id ),
                [this]( uint32_t * id ) {
                    if (id)
                    {
                        // `unsubscribe_from_ctrl_event()` may throw so we first release the memory allocated and than call it.
                        auto local_id = *id;
                        delete id;
                        unsubscribe_from_ctrl_event( local_id );
                    }
                } );

            subscribe_to_ctrl_event(control.id);

            // Set value
            if (xioctl(_fd, VIDIOC_S_CTRL, &control) < 0)
            {
                if (errno == EIO || errno == EAGAIN || errno == EBUSY)
                    return false;

                throw linux_backend_exception(rsutils::string::from()
                                              << "xioctl(VIDIOC_S_CTRL) failed on option " << rs2_option_to_string(opt)
                                              << ", value=" << value << ", errno=" << errno );
            }

            if (!pend_for_ctrl_status_event())
                return false;

            return true;
        }

        control_range v4l_uvc_device::get_pu_range(rs2_option option) const
        {
            // Auto controls range is trimed to {0,1} range
            if(option >= RS2_OPTION_ENABLE_AUTO_EXPOSURE && option <= RS2_OPTION_ENABLE_AUTO_WHITE_BALANCE)
            {
                static const int32_t min = 0, max = 1, step = 1, def = 1;
                control_range range(min, max, step, def);

                return range;
            }

            struct v4l2_query_ext_ctrl query = {};
            query.id = get_cid(option);
            if (xioctl(_fd, VIDIOC_QUERY_EXT_CTRL, &query) < 0)
            {
                // Some controls (exposure, auto exposure, auto hue) do not seem to work on V4L2
                // Instead of throwing an error, return an empty range. This will cause this control to be omitted on our UI sample.
                // TODO: Figure out what can be done about these options and make this work
                query.minimum = query.maximum = 0;
            }

            control_range range(query.minimum, query.maximum, query.step, query.default_value);

            return range;
        }

        std::vector<stream_profile> v4l_uvc_device::get_profiles() const
        {
            std::vector<stream_profile> results;

            // Retrieve the caps one by one, first get pixel format, then sizes, then
            // frame rates. See http://linuxtv.org/downloads/v4l-dvb-apis for reference.
            v4l2_fmtdesc pixel_format = {};
            pixel_format.type = _dev.buf_type;
            while (ioctl(_fd, VIDIOC_ENUM_FMT, &pixel_format) == 0)
            {
                v4l2_frmsizeenum frame_size = {};
                frame_size.pixel_format = pixel_format.pixelformat;

                uint32_t fourcc = (const big_endian<int> &)pixel_format.pixelformat;

                if (pixel_format.pixelformat == 0)
                {
                    // Microsoft Depth GUIDs for R400 series are not yet recognized
                    // by the Linux kernel, but they do not require a patch, since there
                    // are "backup" Z16 and Y8 formats in place
                    std::set<std::string> known_problematic_formats = {
                        "00000050-0000-0010-8000-00aa003",
                        "00000032-0000-0010-8000-00aa003",
                    };

                    if (std::find(known_problematic_formats.begin(),
                                  known_problematic_formats.end(),
                                  (const char*)pixel_format.description) ==
                        known_problematic_formats.end())
                    {
                        const std::string s(rsutils::string::from() << "!" << pixel_format.description);
                        std::regex rgx("!([0-9a-f]+)-.*");
                        std::smatch match;

                        if (std::regex_search(s.begin(), s.end(), match, rgx))
                        {
                            std::stringstream ss;
                            ss <<  match[1];
                            int id;
                            ss >> std::hex >> id;
                            fourcc = (const big_endian<int> &)id;

                            auto format_str = fourcc_to_string(id);
                            LOG_WARNING("Pixel format " << pixel_format.description << " likely requires patch for fourcc code " << format_str << "!");
                        }
                    }
                }
                else
                {
                    LOG_DEBUG("Recognized pixel-format " << pixel_format.description);
                }

                while (ioctl(_fd, VIDIOC_ENUM_FRAMESIZES, &frame_size) == 0)
                {
                    v4l2_frmivalenum frame_interval = {};
                    frame_interval.pixel_format = pixel_format.pixelformat;
                    frame_interval.width = frame_size.discrete.width;
                    frame_interval.height = frame_size.discrete.height;
                    while (ioctl(_fd, VIDIOC_ENUM_FRAMEINTERVALS, &frame_interval) == 0)
                    {
                        if (frame_interval.type == V4L2_FRMIVAL_TYPE_DISCRETE)
                        {
                            if (frame_interval.discrete.numerator != 0)
                            {
                                auto fps =
                                    static_cast<float>(frame_interval.discrete.denominator) /
                                    static_cast<float>(frame_interval.discrete.numerator);

                                // The device reports GREY for both mapping streams, so the
                                // labeled point cloud is re-tagged here to keep them apart.
                                // Two layouts: D585S / D585 legacy (0x0B6B / 0x0B6A) carry them
                                // on MI 13 at 2880-wide payloads; every other D5xx carries them
                                // on MI 11 with LPCL at 640x360. The MI test matters -- 640x360
                                // GREY also exists on the depth interface as infrared.
                                const bool d585s_layout
                                    = ( this->_info.pid == 0X0B6B || this->_info.pid == 0X0B6A )
                                   && frame_size.discrete.width == 2880
                                   && ( frame_size.discrete.height == 1040
                                     || frame_size.discrete.height == 260
                                     || frame_size.discrete.height == 32 );
                                const bool d5xx_mapping_layout
                                    = ( this->_info.pid != 0X0B6B && this->_info.pid != 0X0B6A )
                                   && is_d5xx_mapping_interface( this->_info.pid, this->_info.mi )
                                   && frame_size.discrete.width == 640
                                   && frame_size.discrete.height == 360;
                                // Per profile: `fourcc` describes the pixel format and is
                                // reused for every frame size, so re-tagging it here would
                                // leak PAL8 onto every later size of the same format.
                                uint32_t profile_fourcc = fourcc;
                                if (d585s_layout || d5xx_mapping_layout)
                                {
                                    profile_fourcc = 0x50414c38; // PAL8 used instead of GREY in order to distinguish between occupancy and point cloud streams
                                }

                                stream_profile p{};
                                p.format = profile_fourcc;
                                p.width = frame_size.discrete.width;
                                p.height = frame_size.discrete.height;
                                p.fps = fps;
                                if (profile_fourcc != 0) results.push_back(p);
                            }
                        }

                        ++frame_interval.index;
                    }

                     ++frame_size.index;
                }

                ++pixel_format.index;
            }
            return results;
        }

        void v4l_uvc_device::lock() const
        {
            _named_mtx->lock();
        }
        void v4l_uvc_device::unlock() const
        {
            _named_mtx->unlock();
        }

        uint32_t v4l_uvc_device::get_cid(rs2_option option) const
        {
            switch(option)
            {
            case RS2_OPTION_BACKLIGHT_COMPENSATION: return V4L2_CID_BACKLIGHT_COMPENSATION;
            case RS2_OPTION_BRIGHTNESS: return V4L2_CID_BRIGHTNESS;
            case RS2_OPTION_CONTRAST: return V4L2_CID_CONTRAST;
            case RS2_OPTION_EXPOSURE: return V4L2_CID_EXPOSURE_ABSOLUTE; // Is this actually valid? I'm getting a lot of VIDIOC error 22s...
            case RS2_OPTION_GAIN: return V4L2_CID_GAIN;
            case RS2_OPTION_GAMMA: return V4L2_CID_GAMMA;
            case RS2_OPTION_HUE: return V4L2_CID_HUE;
            case RS2_OPTION_SATURATION: return V4L2_CID_SATURATION;
            case RS2_OPTION_SHARPNESS: return V4L2_CID_SHARPNESS;
            case RS2_OPTION_WHITE_BALANCE: return V4L2_CID_WHITE_BALANCE_TEMPERATURE;
            case RS2_OPTION_ENABLE_AUTO_EXPOSURE: return V4L2_CID_EXPOSURE_AUTO; // Automatic gain/exposure control
            case RS2_OPTION_ENABLE_AUTO_WHITE_BALANCE: return V4L2_CID_AUTO_WHITE_BALANCE;
            case RS2_OPTION_POWER_LINE_FREQUENCY : return V4L2_CID_POWER_LINE_FREQUENCY;
            case RS2_OPTION_AUTO_EXPOSURE_PRIORITY: return V4L2_CID_EXPOSURE_AUTO_PRIORITY;
            default: throw linux_backend_exception(rsutils::string::from() << "no v4l2 cid for option " << option);
            }
        }

        void v4l_uvc_device::capture_loop()
        {
            try
            {
                while(_is_capturing)
                {
                    poll();
                }
            }
            catch (const std::exception& ex)
            {
                LOG_ERROR(ex.what());

                librealsense::notification n = {RS2_NOTIFICATION_CATEGORY_UNKNOWN_ERROR, 0, RS2_LOG_SEVERITY_ERROR, ex.what()};

                _error_handler(n);
            }
        }

        bool v4l_uvc_device::has_metadata() const
        {
            return !_use_memory_map;
        }

        void v4l_uvc_device::streamon() const
        {
            stream_ctl_on(_fd, _dev.buf_type);
        }

        void v4l_uvc_device::streamoff() const
        {
            stream_off(_fd, _dev.buf_type);
        }

        void v4l_uvc_device::negotiate_kernel_buffers(size_t num) const
        {
            req_io_buff(_fd, num, _name,
                        _use_memory_map ? V4L2_MEMORY_MMAP : V4L2_MEMORY_USERPTR,
                        _dev.buf_type);
        }

        void v4l_uvc_device::allocate_io_buffers(size_t buffers)
        {
            if (buffers)
            {
                for(size_t i = 0; i < buffers; ++i)
                {
                    _buffers.push_back(std::make_shared<buffer>(_fd, _dev.buf_type, _use_memory_map, i));
                }
            }
            else
            {
                for(size_t i = 0; i < _buffers.size(); i++)
                {
                    _buffers[i]->detach_buffer();
                }
                _buffers.resize(0);
            }
        }

        void v4l_uvc_device::map_device_descriptor()
        {
            _fd = open_v4l_node(_name);
            if(_fd < 0)
                throw linux_backend_exception(rsutils::string::from() <<__FUNCTION__ << " Cannot open '" << _name);

            if (pipe(_stop_pipe_fd) < 0)
                throw linux_backend_exception(rsutils::string::from() <<__FUNCTION__ << " Cannot create pipe!");

            if (_fds.size())
                throw linux_backend_exception(rsutils::string::from() <<__FUNCTION__ << " Device descriptor is already allocated");

            _fds.insert(_fds.end(),{_fd,_stop_pipe_fd[0],_stop_pipe_fd[1]});
            _max_fd = *std::max_element(_fds.begin(),_fds.end());

            if (!_are_device_capabilities_assigned)
            {
                assign_device_capabilities();
                _are_device_capabilities_assigned = true;
            }
        }

        void v4l_uvc_device::assign_device_capabilities()
        {
            v4l2_capability cap = {};
            if(xioctl(_fd, VIDIOC_QUERYCAP, &cap) < 0)
            {
                if(errno == EINVAL)
                    throw linux_backend_exception(_name + " is not V4L2 device");
                else
                    throw linux_backend_exception("xioctl(VIDIOC_QUERYCAP) failed");
            }
            if(!(cap.capabilities & (V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_VIDEO_CAPTURE)))
                throw linux_backend_exception(_name + " is no video capture device");

            if(!(cap.capabilities & V4L2_CAP_STREAMING))
                throw linux_backend_exception(_name + " does not support streaming I/O");
            _info.uvc_capabilities = cap.capabilities;
            _dev.cap = cap;
            /* supporting only one plane for IPU6 */
            _dev.num_planes = 1;
            if (cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) {
                _dev.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            } else if (cap.capabilities & V4L2_CAP_VIDEO_CAPTURE_MPLANE) {
                _dev.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
            } else {
                throw linux_backend_exception(_name + " Buffer type is unknown!");
            }
            // Select video input, video standard and tune here.
            v4l2_cropcap cropcap = {};
            cropcap.type = _dev.buf_type;
            if(xioctl(_fd, VIDIOC_CROPCAP, &cropcap) == 0)
            {
                v4l2_crop crop = {};
                crop.type = _dev.buf_type;
                crop.c = cropcap.defrect; // reset to default
                if(xioctl(_fd, VIDIOC_S_CROP, &crop) < 0)
                {
                    switch (errno)
                    {
                    case EINVAL: break; // Cropping not supported
                    default: break; // Errors ignored
                    }
                }
                _dev.cropcap = cropcap;
            } else {} // Errors ignored
        }

        void v4l_uvc_device::unmap_device_descriptor()
        {
            if(::close(_fd) < 0)
                throw linux_backend_exception("v4l_uvc_device: close(_fd) failed");

            if(::close(_stop_pipe_fd[0]) < 0)
               throw linux_backend_exception("v4l_uvc_device: close(_stop_pipe_fd[0]) failed");
            if(::close(_stop_pipe_fd[1]) < 0)
               throw linux_backend_exception("v4l_uvc_device: close(_stop_pipe_fd[1]) failed");

            _fd = 0;
            _stop_pipe_fd[0] = _stop_pipe_fd[1] = 0;
            _fds.clear();
        }

        void v4l_uvc_device::set_format(stream_profile profile)
        {
            v4l2_format fmt = {};
            fmt.type = _dev.buf_type;
            if (_dev.buf_type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
                fmt.fmt.pix_mp.width       = profile.width;
                fmt.fmt.pix_mp.height      = profile.height;
                fmt.fmt.pix_mp.pixelformat = (const big_endian<int> &)profile.format;
                fmt.fmt.pix_mp.field       = V4L2_FIELD_NONE;
                fmt.fmt.pix_mp.num_planes = _dev.num_planes;
                fmt.fmt.pix_mp.flags = 0;

                for (int i = 0; i < fmt.fmt.pix_mp.num_planes; i++) {
                    fmt.fmt.pix_mp.plane_fmt[i].bytesperline = 0;
                    fmt.fmt.pix_mp.plane_fmt[i].sizeimage = 0;
                }
            } else {
                fmt.fmt.pix.width       = profile.width;
                fmt.fmt.pix.height      = profile.height;
                fmt.fmt.pix.pixelformat = (const big_endian<int> &)profile.format;
                fmt.fmt.pix.field       = V4L2_FIELD_NONE;
            }
            if(xioctl(_fd, VIDIOC_S_FMT, &fmt) < 0)
            {
                throw linux_backend_exception(rsutils::string::from() << "xioctl(VIDIOC_S_FMT) failed, errno=" << errno);
            }
            else
                LOG_INFO("Video node was successfully configured to " << fourcc_to_string(fmt.fmt.pix.pixelformat) << " format" <<", fd " << std::dec << _fd);

            LOG_INFO("Trying to configure fourcc " << fourcc_to_string(fmt.fmt.pix.pixelformat));
        }

        void v4l_uvc_device::subscribe_to_ctrl_event( uint32_t control_id )
        {
            struct v4l2_event_subscription event_subscription ;
            event_subscription.flags = V4L2_EVENT_SUB_FL_ALLOW_FEEDBACK;
            event_subscription.type =  V4L2_EVENT_CTRL;
            event_subscription.id = control_id;
            memset(event_subscription.reserved,0, sizeof(event_subscription.reserved));
            if  (xioctl(_fd, VIDIOC_SUBSCRIBE_EVENT, &event_subscription) < 0)
            {
                throw linux_backend_exception(rsutils::string::from() << "xioctl(VIDIOC_SUBSCRIBE_EVENT) with control_id = " << control_id << " failed");
            }
        }

        void v4l_uvc_device::unsubscribe_from_ctrl_event( uint32_t control_id )
        {
            struct v4l2_event_subscription event_subscription ;
            event_subscription.flags = V4L2_EVENT_SUB_FL_ALLOW_FEEDBACK;
            event_subscription.type =  V4L2_EVENT_CTRL;
            event_subscription.id = control_id;
            memset(event_subscription.reserved,0, sizeof(event_subscription.reserved));
            if  (xioctl(_fd, VIDIOC_UNSUBSCRIBE_EVENT, &event_subscription) < 0)
            {
                throw linux_backend_exception(rsutils::string::from() << "xioctl(VIDIOC_UNSUBSCRIBE_EVENT) with control_id = " << control_id << " failed");
            }
        }


        bool v4l_uvc_device::pend_for_ctrl_status_event()
        {
            struct v4l2_event event;
            memset(&event, 0 , sizeof(event));

            // Poll registered events and verify that set control event raised (wait max of 10 * 2 = 20 [ms])
            static int MAX_POLL_RETRIES = 10;
            for ( int i = 0 ; i < MAX_POLL_RETRIES && event.type != V4L2_EVENT_CTRL ; i++)
            {
                if(xioctl(_fd, VIDIOC_DQEVENT, &event) < 0)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(2));
                }
            }

            return event.type == V4L2_EVENT_CTRL;
        }
    }  // namespace platform
}  // namespace librealsense
