// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2015 RealSense, Inc. All Rights Reserved.

#include "context-libusb.h"
#include "../types.h"
#include <chrono>
#include <thread>

namespace librealsense
{
    namespace platform
    {       
        usb_context::usb_context() : _ctx(NULL), _list(NULL), _count(0)
        {
            const int max_retries = 10;
            const int retry_delay_ms = 100;
            
            for(int attempt = 0; attempt < max_retries; attempt++) {
                LOG_DEBUG("Attempting libusb_init (attempt " << (attempt + 1) << "/" << max_retries << ")...");
                try {
                    auto sts = libusb_init(&_ctx);
                    if(sts == LIBUSB_SUCCESS)
                    {
                        _count = libusb_get_device_list(_ctx, &_list);
                        LOG_INFO("Found " << _count << " USB devices");
                        return; // Success, exit constructor
                    }
                    else
                    {
                        LOG_ERROR("libusb_init failed with status: " << sts << " (attempt " << (attempt + 1) << ")");
                        if(_ctx) {
                            libusb_exit(_ctx);
                            _ctx = nullptr;
                        }
                    }
                }
                catch(const std::exception& e) {
                    LOG_ERROR("Exception during libusb_init (attempt " << (attempt + 1) << "): " << e.what());
                    if(_ctx) {
                        libusb_exit(_ctx);
                        _ctx = nullptr;
                    }
                }
                catch(...) {
                    LOG_ERROR("Unknown exception during libusb_init (attempt " << (attempt + 1) << ")");
                    if(_ctx) {
                        libusb_exit(_ctx);
                        _ctx = nullptr;
                    }
                }
                
                // Wait before retry (except on last attempt)
                if(attempt < max_retries - 1) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(retry_delay_ms));
                }
            }
            
            // All attempts failed
            LOG_ERROR("libusb_init failed after " << max_retries << " attempts");
            _ctx = nullptr;
            _list = nullptr;
            _count = 0;
        }
        
        usb_context::~usb_context()
        {
            if (_list)
                libusb_free_device_list(_list, true);
            assert(_handler_requests == 0); // we need the last libusb_close to trigger an event to stop the event thread
            if (_event_handler.joinable())
                _event_handler.join();
            if (_ctx)
                libusb_exit(_ctx);
        }
        
        libusb_context* usb_context::get()
        {
            return _ctx;
        } 
    
        void usb_context::start_event_handler()
        {
            if (!_ctx) return; // Skip if libusb initialization failed
            
            std::lock_guard<std::mutex> lk(_mutex);
            if (!_handler_requests) {
                // see "Applications which do not use hotplug support" in libusb's io.c
                if (_event_handler.joinable()) {
                    _event_handler.join();
                    _kill_handler_thread = 0;
                }
                _event_handler = std::thread([this]() {
                    while (!_kill_handler_thread)
                        libusb_handle_events_completed(_ctx, &_kill_handler_thread);
                });
            }
            _handler_requests++;
        }

        void usb_context::stop_event_handler()
        {
            std::lock_guard<std::mutex> lk(_mutex);
            _handler_requests--;
            if (!_handler_requests)
                // the last libusb_close will trigger and event and the handler thread will notice this is set
                _kill_handler_thread = 1;
        }

        libusb_device* usb_context::get_device(uint8_t index)
        {
            return index < _count ? _list[index] : NULL;
        }
        
        size_t usb_context::device_count()
        {
            return _count;
        }
    }
}
