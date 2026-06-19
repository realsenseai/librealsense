// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2015 RealSense, Inc. All Rights Reserved.

#pragma once

#include "types.h"
#include "context-libusb.h"

#include <chrono>
#include <thread>

#include "libusb.h"

#include <rsutils/string/from.h>

namespace librealsense
{
    namespace platform
    {
        static usb_status libusb_status_to_rs(int sts)
        {
            switch (sts)
            {
                case LIBUSB_SUCCESS: return RS2_USB_STATUS_SUCCESS;
                case LIBUSB_ERROR_IO: return RS2_USB_STATUS_IO;
                case LIBUSB_ERROR_INVALID_PARAM: return RS2_USB_STATUS_INVALID_PARAM;
                case LIBUSB_ERROR_ACCESS: return RS2_USB_STATUS_ACCESS;
                case LIBUSB_ERROR_NO_DEVICE: return RS2_USB_STATUS_NO_DEVICE;
                case LIBUSB_ERROR_NOT_FOUND: return RS2_USB_STATUS_NOT_FOUND;
                case LIBUSB_ERROR_BUSY: return RS2_USB_STATUS_BUSY;
                case LIBUSB_ERROR_TIMEOUT: return RS2_USB_STATUS_TIMEOUT;
                case LIBUSB_ERROR_OVERFLOW: return RS2_USB_STATUS_OVERFLOW;
                case LIBUSB_ERROR_PIPE: return RS2_USB_STATUS_PIPE;
                case LIBUSB_ERROR_INTERRUPTED: return RS2_USB_STATUS_INTERRUPTED;
                case LIBUSB_ERROR_NO_MEM: return RS2_USB_STATUS_NO_MEM;
                case LIBUSB_ERROR_NOT_SUPPORTED: return RS2_USB_STATUS_NOT_SUPPORTED;
                case LIBUSB_ERROR_OTHER: return RS2_USB_STATUS_OTHER;
                default: return RS2_USB_STATUS_OTHER;
            }
        }

        class handle_libusb
        {
        public:
            handle_libusb(std::shared_ptr<usb_context> context, libusb_device* device, std::shared_ptr<usb_interface_libusb> interface) :
                    _first_interface(interface), _context(context), _handle(nullptr)
            {
                auto sts = libusb_open(device, &_handle);
                if (sts == LIBUSB_ERROR_BUSY || sts == LIBUSB_ERROR_ACCESS)
                {
                    auto retry_counter = 20;
                    do
                    {
                        LOG_WARNING("failed to open usb device, error: " << sts << " - retrying...");
                        std::this_thread::sleep_for(std::chrono::milliseconds(50));
                        sts = libusb_open(device, &_handle);
                    }
                    while ((sts == LIBUSB_ERROR_BUSY || sts == LIBUSB_ERROR_ACCESS) && --retry_counter > 0);
                }

                if(sts != LIBUSB_SUCCESS)
                {
                    auto rs_sts =  libusb_status_to_rs(sts);
                    std::stringstream msg;
                    msg << "failed to open usb interface: " << (int)interface->get_number() << ", error: " << usb_status_to_string.at(rs_sts);
                    LOG_ERROR(msg.str());
                    throw std::runtime_error(msg.str());
                }

                sts = libusb_set_auto_detach_kernel_driver(_handle, true); // detach from kernel driver when claimed and re-attach when released.
                if(sts != LIBUSB_SUCCESS)
                {
                    auto rs_sts =  libusb_status_to_rs(sts);
                    std::stringstream msg;
                    msg << "failed to set kernel driver auto detach: " << (int)interface->get_number() << ", error: " << usb_status_to_string.at(rs_sts);
                    LOG_ERROR(msg.str());
                    libusb_close(_handle);
                    _handle = nullptr;
                    throw std::runtime_error(msg.str());
                }

#ifdef __APPLE__
                // WHY this exists: the camera exposes standard UVC interfaces (VideoControl +
                // VideoStreaming for depth, IR and color). On macOS those are auto-claimed by
                // UVCAssistant - the CoreMediaIO USB-video system extension
                // (com.apple.cmio.uvcassistantextension) that publishes every UVC camera to
                // AVFoundation/Photo Booth/FaceTime/etc. With no app open it STILL holds the
                // streaming interfaces exclusively; you can see it in IORegistry as
                // 'UsbExclusiveOwner = pid <n>, UVCAssistant' on interface 1 (depth stream) and
                // interface 2. That exclusive owner is why libusb_claim_interface returns
                // ACCESS, and which interface fails alternates run-to-run purely on who-claimed-
                // -first timing.
                //
                // WHY relying on libusb's auto-detach (set above) is not enough: that path only
                // acts when a driver is detected bound at claim time, and it can only evict a
                // *kernel* driver. UVCAssistant is a *userspace* process, so the auto-detach
                // check races the enumeration window and frequently no-ops.
                //
                // WHY we force a detach here: calling libusb_detach_kernel_driver explicitly
                // (rather than depending on the lazy auto-detach) forces libusb's capture-mode
                // re-enumeration (USBDeviceReEnumerate + kUSBReEnumerateCaptureDeviceMask) up
                // front. That captures the WHOLE device and is the strongest lever libusb gives
                // us to take the interfaces back before claiming. Needs root or the
                // com.apple.vm.device-access entitlement; NOT_SUPPORTED / NOT_FOUND just mean
                // "nothing to capture", so those are not failures.
                //
                // CAVEAT: because UVCAssistant lives in userspace and re-grabs UVC devices as
                // they appear, even capture-mode re-enumeration does not always evict it - a
                // physical replug or a fresh boot can come up with UVCAssistant already owning
                // the interfaces, in which case capture loses the race and the claim below still
                // fails with ACCESS. The fully reliable fixes are out-of-process: knock
                // UVCAssistant off the device right before launch ('sudo killall UVCAssistant'
                // then start streaming in the respawn window so we claim first), sign the binary
                // with the com.apple.vm.device-access capture entitlement, or disable the camera
                // assistant entirely. This detach maximizes our chances; it is not a guarantee.
                {
                    auto detach_sts = libusb_detach_kernel_driver(_handle, interface->get_number());
                    if (detach_sts != LIBUSB_SUCCESS &&
                        detach_sts != LIBUSB_ERROR_NOT_SUPPORTED &&
                        detach_sts != LIBUSB_ERROR_NOT_FOUND)
                    {
                        LOG_WARNING("failed to capture usb device for interface "
                                    << (int)interface->get_number() << ", error: " << detach_sts
                                    << " - continuing, claim may still succeed");
                    }
                }
#endif

                try
                {
                    claim_interface_or_throw(interface->get_number());
                    for(auto&& i : interface->get_associated_interfaces())
                        claim_interface_or_throw(i->get_number());
                }
                catch(...)
                {
                    // The constructor threw after libusb_open succeeded, so the destructor
                    // will NOT run. Release whatever we managed to claim and close the device
                    // here; otherwise the leaked open handle keeps the device busy and defeats
                    // the higher-level open retry in usb_device_libusb::get_handle.
                    for(auto&& i : interface->get_associated_interfaces())
                        libusb_release_interface(_handle, i->get_number());
                    libusb_release_interface(_handle, interface->get_number());
                    libusb_close(_handle);
                    _handle = nullptr;
                    throw;
                }

                _context->start_event_handler();
            }

            ~handle_libusb()
            {
                _context->stop_event_handler();
                for(auto&& i : _first_interface->get_associated_interfaces())
                    libusb_release_interface(_handle, i->get_number());
                libusb_release_interface(_handle, _first_interface->get_number());
                libusb_close(_handle);
            }

            libusb_device_handle* get()
            {
                return _handle;
            }

        private:
            void claim_interface_or_throw(uint8_t interface)
            {
                auto rs_sts = claim_interface(interface);
                if(rs_sts != RS2_USB_STATUS_SUCCESS)
                    throw std::runtime_error(rsutils::string::from() << "Unable to claim interface " << (int)interface << ", error: " << usb_status_to_string.at(rs_sts));
            }

            usb_status claim_interface(uint8_t interface)
            {
               
                auto sts = libusb_claim_interface(_handle, interface);

                if (sts != LIBUSB_SUCCESS)
                {
                    // Retry only on transient BUSY here. We deliberately do NOT tight-loop on
                    // ACCESS: on macOS every claim attempt forces libusb to reset and
                    // re-enumerate the device to detach the system UVC driver (UVCAssistant),
                    // so hammering claim keeps the device in perpetual reset and never lets it
                    // settle. ACCESS is instead recovered one level up (usb_device_libusb::
                    // get_handle), which rebuilds the handle from scratch after a backoff so
                    // the bus can settle between attempts.
                    if( sts == LIBUSB_ERROR_BUSY )
                    {
                        auto retry_counter = 5;
                        do
                        {
                            LOG_WARNING( "failed to claim usb interface, interface "
                                          << (int)interface << ", is busy - retrying..." );
                            std::this_thread::sleep_for( std::chrono::milliseconds( 50 ) );

                            sts = libusb_claim_interface( _handle, interface );
                            if( sts == LIBUSB_SUCCESS )
                            {
                                LOG_DEBUG( "retrying success, interface = " << (int)interface );
                                return RS2_USB_STATUS_SUCCESS;
                            }
                        }
                        while( sts == LIBUSB_ERROR_BUSY && --retry_counter > 0 );
                    }

                    auto rs_sts = libusb_status_to_rs(sts);
                    LOG_ERROR( "failed to claim usb interface: "
                               << (int)interface << ", error: "
                               << usb_status_to_string.at( rs_sts ) );
                    return rs_sts;
                }

                return RS2_USB_STATUS_SUCCESS;
            }

            std::shared_ptr<usb_context> _context;
            std::shared_ptr<usb_interface_libusb> _first_interface;
            libusb_device_handle* _handle;
        };
    }
}
