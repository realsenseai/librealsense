// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2015 RealSense, Inc. All Rights Reserved.

#include "uvc-streamer.h"

const int UVC_PAYLOAD_MAX_HEADER_LENGTH         = 1024;
const int DEQUEUE_MILLISECONDS_TIMEOUT          = 50;
const int ENDPOINT_RESET_MILLISECONDS_TIMEOUT   = 100;

void cleanup_frame(backend_frame *ptr) {
    if (ptr) ptr->owner->deallocate(ptr);
}

namespace librealsense
{
    namespace platform
    {
        uvc_streamer::uvc_streamer(uvc_streamer_context context) :
            _context(context), _action_dispatcher(10, rsutils::concurrency::thread_category_usb_io, "uvc-strm-d")
        {
            auto inf = context.usb_device->get_interface(context.control->bInterfaceNumber);
            if (inf == nullptr)
                throw std::runtime_error("can't find UVC streaming interface of device: " + context.usb_device->get_info().id);
            _read_endpoint = inf->first_endpoint(platform::RS2_USB_ENDPOINT_DIRECTION_READ);

            _read_buff_length = UVC_PAYLOAD_MAX_HEADER_LENGTH + _context.control->dwMaxVideoFrameSize;
            LOG_INFO("endpoint " << (int)_read_endpoint->get_address() << " read buffer size: " << std::dec <<_read_buff_length);

            _action_dispatcher.start();

            _watchdog_timeout = static_cast<int64_t>(((1000.0 / _context.profile.fps) * 10));

            init();
        }

        uvc_streamer::~uvc_streamer()
        {
            flush();
        }

        void uvc_process_bulk_payload(backend_frame_ptr fp, size_t payload_len, backend_frames_queue& queue) {

            /* ignore empty payload transfers */
            if (!fp || payload_len < 2)
                return;

            uint8_t header_len = fp->pixels[0];
            uint8_t header_info = fp->pixels[1];

            size_t data_len = payload_len - header_len;

            if (header_info & 0x40)
            {
                LOG_ERROR("bad packet: error bit set");
                return;
            }
            if (header_len > payload_len)
            {
                LOG_ERROR("bogus packet: actual_len=" << payload_len << ", header_len=" << header_len);
                return;
            }


            LOG_DEBUG("Passing packet to user CB with size " << (data_len + header_len));
            librealsense::platform::frame_object fo{ data_len, header_len,
                                                     fp->pixels.data() + header_len , fp->pixels.data() };
            fp->fo = fo;

            queue.enqueue(std::move(fp));
        }

        void uvc_streamer::init()
        {
            _frames_archive = std::make_shared<backend_frames_archive>();
            // Get all pointers from archive and initialize their content
            std::vector<backend_frame *> frames;
            for (auto i = 0; i < _frames_archive->CAPACITY; i++) {
                auto ptr = _frames_archive->allocate();
                ptr->pixels.resize(_read_buff_length, 0);
                ptr->owner = _frames_archive.get();
                frames.push_back(ptr);
            }

            for (auto ptr : frames) {
                _frames_archive->deallocate(ptr);
            }

            _publish_frame_thread = std::make_shared<active_object<>>([this](dispatcher::cancellable_timer cancellable_timer)
            {
                backend_frame_ptr fp(nullptr, [](backend_frame *) {});
                if (_queue.dequeue(&fp, DEQUEUE_MILLISECONDS_TIMEOUT))
                {
                    if(_publish_frames && running())
                        _context.user_cb(_context.profile, fp->fo, []() mutable {});
                }
            }, rsutils::concurrency::thread_category_frame_processing, "uvc-pub-fr");

            _watchdog = std::make_shared<watchdog>([this]()
             {
                 _action_dispatcher.invoke([this](dispatcher::cancellable_timer c)
                   {
                       if(!_running || !_frame_arrived)
                           return;

                       LOG_ERROR("uvc streamer watchdog triggered on endpoint: " << (int)_read_endpoint->get_address());
                       _context.messenger->reset_endpoint(_read_endpoint, ENDPOINT_RESET_MILLISECONDS_TIMEOUT);
                       _frame_arrived = false;
                   });
             }, _watchdog_timeout, rsutils::concurrency::thread_category_usb_io, "uvc-wdog");

            _watchdog->start();

            _request_callback = std::make_shared<usb_request_callback>([this](platform::rs_usb_request r)
            {
                _action_dispatcher.invoke([this, r](dispatcher::cancellable_timer)
                {
                    if(!_running)
                      return;

                    auto al = r->get_actual_length();
                    // Relax the frame size constrain for compressed streams
                    bool is_compressed = val_in_range(_context.profile.format, { 0x4d4a5047U , 0x5a313648U}); // MJPEG, Z16H
                    if(al > 0L && ((al == r->get_buffer().data()[0] + _context.control->dwMaxVideoFrameSize) || is_compressed ))
                    {
                        auto f = backend_frame_ptr(_frames_archive->allocate(), &cleanup_frame);
                        if(f)
                        {
                            _frame_arrived = true;
                            _watchdog->kick();
                            memcpy(f->pixels.data(), r->get_buffer().data(), r->get_buffer().size());
                            uvc_process_bulk_payload(std::move(f), r->get_actual_length(), _queue);
                        }
                    }

                    auto sts = _context.messenger->submit_request(r);
                    if(sts != platform::RS2_USB_STATUS_SUCCESS)
                        LOG_ERROR("failed to submit UVC request, error: " << sts);
                });
            });

            _requests = std::vector<rs_usb_request>(_context.request_count);
            for(auto&& r : _requests)
            {
                r = _context.messenger->create_request(_read_endpoint);
                r->set_buffer(std::vector<uint8_t>(_read_buff_length));
                r->set_callback(_request_callback);
            }
        }

        void uvc_streamer::start()
        {
            _action_dispatcher.invoke_and_wait([this](dispatcher::cancellable_timer c)
            {
                if(_running)
                    return;

                _context.messenger->reset_endpoint(_read_endpoint, RS2_USB_ENDPOINT_DIRECTION_READ);

                {
                    std::lock_guard<std::mutex> lock(_running_mutex);
                    _running = true;
                }

                for(auto&& r : _requests)
                {
                    auto sts = _context.messenger->submit_request(r);
                    if(sts != platform::RS2_USB_STATUS_SUCCESS)
                        throw std::runtime_error("failed to submit UVC request while start streaming");
                }

                _publish_frame_thread->start();

            }, [this](){ return _running; });
        }

        void uvc_streamer::stop()
        {
            _action_dispatcher.invoke_and_wait([this](dispatcher::cancellable_timer c)
            {
                if(!_running)
                    return;

                _request_callback->cancel();

                _watchdog->stop();

                _frames_archive->stop_allocation();

                _queue.clear();

                for(auto&& r : _requests)
                  _context.messenger->cancel_request(r);

                _requests.clear();

                _frames_archive->wait_until_empty();

                _context.messenger->reset_endpoint(_read_endpoint, RS2_USB_ENDPOINT_DIRECTION_READ);

                _publish_frame_thread->stop();

                {
                    std::lock_guard<std::mutex> lock(_running_mutex);
                    _running = false;
                    _stopped_cv.notify_one();
                }

            }, [this](){ return !_running; });
        }

        void uvc_streamer::flush()
        {
            if(_running)
                stop();

            // synchronized so do not destroy shared pointers while it's still being running
            {
                std::unique_lock<std::mutex> lock(_running_mutex);
                _stopped_cv.wait_for(lock, std::chrono::seconds(1), [&]() { return !_running; });
            }

            _read_endpoint.reset();

            _watchdog.reset();
            _publish_frame_thread.reset();
            _request_callback.reset();

            _frames_archive.reset();

            _action_dispatcher.stop();
        }

        bool uvc_streamer::wait_for_first_frame(uint32_t timeout_ms)
        {
            auto start = std::chrono::system_clock::now();
            while(!_frame_arrived)
            {
                auto end = std::chrono::system_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
                if(duration > timeout_ms)
                    break;
            }
            return _frame_arrived;
        }
    }
}
