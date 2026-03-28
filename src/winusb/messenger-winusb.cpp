// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2015 RealSense, Inc. All Rights Reserved.

#if (_MSC_FULL_VER < 180031101)
#error At least Visual Studio 2013 Update 4 is required to compile this backend
#endif

#include "messenger-winusb.h"
#include "device-winusb.h"
#include "usb/usb-enumerator.h"
#include "types.h"

#include <Windows.h>
#include <Sddl.h>
#include <string>
#include <regex>
#include <sstream>
#include <mutex>

#pragma comment(lib, "winusb.lib")

namespace librealsense
{
    namespace platform
    {
        usb_messenger_winusb::usb_messenger_winusb(const std::shared_ptr<usb_device_winusb> device,
            std::shared_ptr<handle_winusb> handle)
            : _device(device), _handle(handle)
        {

        }

        usb_messenger_winusb::~usb_messenger_winusb()
        {
            for (auto&& d : _dispatchers)
            {
                d.second->stop();
                d.second.reset();
            }
        }

        usb_status usb_messenger_winusb::control_transfer(int request_type, int request, int value, int index, uint8_t* buffer, uint32_t length, uint32_t& transferred, uint32_t timeout_ms)
        {
            WINUSB_SETUP_PACKET setupPacket;
            ULONG length_transferred;

            setupPacket.RequestType = request_type;
            setupPacket.Request = request;
            setupPacket.Value = value;
            setupPacket.Index = index;
            setupPacket.Length = length;

            auto interface_number = 0xFF & index;
            auto h = _handle->get_interface_handle(interface_number);

            auto sts = set_timeout_policy(h, 0, timeout_ms);
            if (sts != RS2_USB_STATUS_SUCCESS)
                return sts;

            if (!WinUsb_ControlTransfer(h, setupPacket, buffer, length, &length_transferred, NULL))
            {
                auto lastResult = GetLastError();
                LOG_WARNING("control_transfer failed, error: " << lastResult);
                return winusb_status_to_rs(lastResult);
            }
            transferred = length_transferred;
            return RS2_USB_STATUS_SUCCESS;
        }

        usb_status usb_messenger_winusb::reset_endpoint(const rs_usb_endpoint& endpoint, uint32_t timeout_ms)
        {
            auto h = _handle->get_interface_handle(endpoint->get_interface_number());

            if (!WinUsb_ResetPipe(h, endpoint->get_address()))
            {
                auto lastResult = GetLastError();
                LOG_ERROR("control_transfer failed, error: " << lastResult);
                return winusb_status_to_rs(lastResult);
            }
            return RS2_USB_STATUS_SUCCESS;
        }

        usb_status usb_messenger_winusb::bulk_transfer(const rs_usb_endpoint& endpoint, uint8_t* buffer, uint32_t length, uint32_t& transferred, uint32_t timeout_ms)
        {
            ULONG length_transferred;

            auto h = _handle->get_interface_handle(endpoint->get_interface_number());

            auto sts = set_timeout_policy(h, endpoint->get_address(), timeout_ms);
            if (sts != RS2_USB_STATUS_SUCCESS)
                return sts;

            bool res;
            if (USB_ENDPOINT_DIRECTION_IN(endpoint->get_address()))
                res = WinUsb_ReadPipe(h, endpoint->get_address(), const_cast<unsigned char*>(buffer), length, &length_transferred, NULL) != 0;
            else
                res = WinUsb_WritePipe(h, endpoint->get_address(), const_cast<unsigned char*>(buffer), length, &length_transferred, NULL) != 0;
            if (!res)
            {
                auto lastResult = GetLastError();
                LOG_ERROR("bulk_transfer failed, error: " << lastResult);
                return winusb_status_to_rs(lastResult);
            }
            transferred = length_transferred;
            return RS2_USB_STATUS_SUCCESS;
        }

        usb_status usb_messenger_winusb::set_timeout_policy(WINUSB_INTERFACE_HANDLE handle, uint8_t endpoint, uint32_t timeout_ms)
        {
            // WinUsb_SetPipePolicy function sets the policy for a specific pipe associated with an endpoint on the device
            // PIPE_TRANSFER_TIMEOUT: Waits for a time-out interval before canceling the request
            auto sts = WinUsb_SetPipePolicy(handle, endpoint, PIPE_TRANSFER_TIMEOUT, sizeof(timeout_ms), &timeout_ms);
            if (!sts)
            {
                auto lastResult = GetLastError();
                LOG_DEBUG( "failed to set timeout policy, error: " << lastResult );
                return winusb_status_to_rs(lastResult);
            }
            return RS2_USB_STATUS_SUCCESS;
        }

        rs_usb_request usb_messenger_winusb::create_request(rs_usb_endpoint endpoint)
        {
            auto rv = std::make_shared<usb_request_winusb>(_device, endpoint);
            return rv;
        }

        usb_status usb_messenger_winusb::submit_request(const rs_usb_request& request)
        {
            ULONG read_pipe_transfer_size = 0;
            auto ep = request->get_endpoint();
            auto in = ep->get_interface_number();
            auto epa = request->get_endpoint()->get_address();
            auto ovl = reinterpret_cast<OVERLAPPED*>(request->get_native_request());
            auto h = _handle->get_interface_handle(in);
            auto buffer_size = static_cast<ULONG>(request->get_buffer().size());

            auto buffer = const_cast<uint8_t*>(request->get_buffer().data());
            int res = WinUsb_ReadPipe(h, epa, buffer, buffer_size, &read_pipe_transfer_size, ovl);
            if (0 != res)
                return winusb_status_to_rs(res);

            auto lastError = GetLastError();
            if (lastError != ERROR_IO_PENDING)
                return winusb_status_to_rs(lastError);

            get_dispatcher(epa)->invoke([&, request, h, ovl](dispatcher::cancellable_timer c)
            {
                ULONG overlapped_transfer_size = 0;

                auto sts = WinUsb_GetOverlappedResult(h, ovl, &overlapped_transfer_size, TRUE);
                if (sts)
                {
                    auto cb = request->get_callback();
                    cb->callback(request);
                }
            });

            return RS2_USB_STATUS_SUCCESS;
        }

        usb_status usb_messenger_winusb::cancel_request(const rs_usb_request& request)
        {
            auto ovl = reinterpret_cast<OVERLAPPED*>(request->get_native_request());
            auto h = _handle->get_device_handle();

            if(CancelIoEx(h, ovl))
                return RS2_USB_STATUS_SUCCESS;

            auto sts = GetLastError();
            return winusb_status_to_rs(sts);
        }

        std::shared_ptr<dispatcher> usb_messenger_winusb::get_dispatcher(uint8_t endpoint)
        {
            std::lock_guard<std::mutex> lk(_mutex);
            if (_dispatchers.find(endpoint) == _dispatchers.end())
            {
                std::string name = std::string("winusb-ep") + std::to_string(endpoint);
                _dispatchers[endpoint] = std::make_shared<dispatcher>(10, rsutils::concurrency::thread_category_usb_io, name);
                _dispatchers[endpoint]->start();
            }
            return _dispatchers.at(endpoint);
        }
    }
}
