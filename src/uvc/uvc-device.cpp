// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2015 RealSense, Inc. All Rights Reserved.

#include "uvc-device.h"
#include "uvc-parser.h"
#include "uvc-streamer.h"

#include <rsutils/string/from.h>

#define UVC_AE_MODE_D0_MANUAL   ( 1 << 0 )
#define UVC_AE_MODE_D1_AUTO     ( 1 << 1 )
#define UVC_AE_MODE_D2_SP       ( 1 << 2 )
#define UVC_AE_MODE_D3_AP       ( 1 << 3 )

#define SWAP_UINT32(x) (((x) >> 24) | (((x) & 0x00FF0000) >> 8) | (((x) & 0x0000FF00) << 8) | ((x) << 24))

const int CONTROL_TRANSFER_TIMEOUT = 100;
const int INTERRUPT_BUFFER_SIZE = 1024;
const int FIRST_FRAME_MILLISECONDS_TIMEOUT = 2000;

class lock_singleton
{
public:
    static lock_singleton& instance()
    {
        static lock_singleton inst;
        return inst;
    }
    static void lock();
    static void unlock();

private:
    static std::recursive_mutex m;
};
std::recursive_mutex lock_singleton::m;
void lock_singleton::lock() { m.lock(); }
void lock_singleton::unlock() { m.unlock(); }


namespace librealsense
{
    namespace platform
    {
        std::vector<uvc_device_info> query_uvc_devices_info()
        {
            std::vector<platform::uvc_device_info> rv;
            auto usb_devices = platform::usb_enumerator::query_devices_info();
            for (auto&& info : usb_devices) 
            {
                if (info.cls != RS2_USB_CLASS_VIDEO)
                    continue;
                platform::uvc_device_info device_info;
                device_info.id = info.id;
                device_info.vid = info.vid;
                device_info.pid = info.pid;
                device_info.mi = info.mi;
                device_info.unique_id = info.unique_id;
                device_info.device_path = info.id;
                device_info.conn_spec = info.conn_spec;
                //LOG_INFO("Found UVC device: " << std::string(device_info).c_str());
                rv.push_back(device_info);
            }
            return rv;
        }

        std::shared_ptr<uvc_device> create_rsuvc_device(uvc_device_info info)
        {
            auto devices = usb_enumerator::query_devices_info();
            for (auto&& usb_info : devices)
            {
                if(usb_info.id != info.id)
                    continue;

                auto dev = usb_enumerator::create_usb_device(usb_info);
                if(dev)
                    return std::make_shared<rs_uvc_device>(dev, info);
            }

            return nullptr;
        }

        rs_uvc_device::rs_uvc_device( const rs_usb_device & usb_device, const uvc_device_info & info, uint8_t usb_request_count )
            : _info( info )
            , _power_counter( 0 )
            , _usb_device( usb_device )
            , _usb_request_count( usb_request_count )
            , _action_dispatcher( 10 )
        {
            _parser = std::make_shared<uvc_parser>(usb_device, info);
            _action_dispatcher.start();
        }

        rs_uvc_device::~rs_uvc_device()
        {
            try {
                set_power_state(D3);
            }
            catch (...) {
                LOG_WARNING("rs_uvc_device failed to switch power state on destructor");
            }
            _action_dispatcher.stop();
        }

        void rs_uvc_device::probe_and_commit(stream_profile profile, frame_callback callback, int buffers)
        {
            if (!_streamers.empty())
                throw std::runtime_error("Device is already streaming!");

            _profiles.push_back(profile);
            _frame_callbacks.push_back(callback);
        }

        void rs_uvc_device::stream_on(std::function<void(const notification& n)> error_handler)
        {
            if (_profiles.empty())
                throw std::runtime_error("Stream not configured");

            if (!_streamers.empty())
                throw std::runtime_error("Device is already streaming!");

            check_connection();

            try {
                for (uint32_t i = 0; i < _profiles.size(); ++i) {
                    play_profile(_profiles[i], _frame_callbacks[i]);
                }
            }
            catch (...) {
                for (auto &elem : _streams)
                    if (elem.callback)
                        close(elem.profile);

                _profiles.clear();
                _frame_callbacks.clear();

                throw;
            }
        }

        void rs_uvc_device::start_callbacks()
        {
            for(auto&& s : _streamers)
                s->enable_user_callbacks();
        }

        void rs_uvc_device::stop_callbacks()
        {
            for(auto&& s : _streamers)
                s->disable_user_callbacks();
        }

        void rs_uvc_device::close(stream_profile profile)
        {
            check_connection();

            for(auto&& s : _streamers)
                s->stop();

            auto elem = std::find_if(_streams.begin(), _streams.end(), [&](const profile_and_callback &pac) {
                return (pac.profile == profile && (pac.callback));
            });

            if (elem == _streams.end() && _frame_callbacks.empty())
                throw std::runtime_error("Camera is not streaming!");

            stop_stream_cleanup(profile, elem);

            if (!_profiles.empty())
                _streamers.clear();
        }

        void rs_uvc_device::set_power_state(power_state state)
        {
            _action_dispatcher.invoke_and_wait([&, this](dispatcher::cancellable_timer c)
            {
                if(state != _power_state)
                {
                    std::lock_guard< std::recursive_mutex > lock( _power_lock );
                    switch(state)
                    {
                        case D0:
                            if( _power_counter.fetch_add( 1 ) == 0 )
                            {
                                try
                                {
                                    _messenger = _usb_device->open(static_cast<uint8_t>(_info.mi));
                                    if (_messenger)
                                    {
                                        try
                                        {
                                            listen_to_interrupts();
                                        }
                                        catch(const std::exception& exception)
                                        {
                                            // this exception catching avoids crash when disconnecting 2 devices at once - bug seen in android os
                                            LOG_WARNING("rs_uvc_device exception in listen_to_interrupts method: " << exception.what());
                                        }
                                        _power_state = D0;
                                    }
                                }
                                //In case of failure need to decrease use counter
                                catch( std::exception const & e )
                                {
                                    _power_counter.fetch_add( -1 );
                                    throw e;
                                }
                                catch( ... )
                                {
                                    _power_counter.fetch_add( -1 );
                                    throw;
                                }
                            }
                            
                            break;
                        case D3:
                            if( _power_counter.fetch_add( -1 ) == 1 )
                            {
                                if( _messenger )
                                {
                                    close_uvc_device();
                                    _messenger.reset();
                                }
                                _power_state = D3;
                            }
                            break;
                    }
                }
            }, [this, state](){ return state == _power_state; });

            if(state != _power_state)
                throw std::runtime_error("failed to set power state");
        }

        power_state rs_uvc_device::get_power_state() const
        {
            return _power_state;
        }

        bool rs_uvc_device::set_xu(const extension_unit& xu, uint8_t ctrl, const uint8_t* data, int len)
        {
            return uvc_set_ctrl(xu.unit, ctrl, (void *) data, len);
        }

        bool rs_uvc_device::get_xu(const extension_unit& xu, uint8_t ctrl, uint8_t* data, int len) const
        {
            return uvc_get_ctrl(xu.unit, ctrl, (void *) data, len, UVC_GET_CUR);
        }

        control_range rs_uvc_device::get_xu_range(const extension_unit& xu, uint8_t control, int len) const
        {
            bool status;
            int max;
            int min;
            int step;
            int def;
            status = uvc_get_ctrl(xu.unit, control, &max, sizeof(int32_t),
                                  UVC_GET_MAX);
            if (!status)
                throw std::runtime_error("uvc_get_ctrl(UVC_GET_MAX) failed");

            status = uvc_get_ctrl(xu.unit, control, &min, sizeof(int32_t),
                                  UVC_GET_MIN);
            if (!status)
                throw std::runtime_error("uvc_get_ctrl(UVC_GET_MIN) failed");

            status = uvc_get_ctrl(xu.unit, control, &step, sizeof(int32_t),
                                  UVC_GET_RES);
            if (!status)
                throw std::runtime_error("uvc_get_ctrl(UVC_GET_RES) failed");

            status = uvc_get_ctrl(xu.unit, control, &def, sizeof(int32_t),
                                  UVC_GET_DEF);
            if (!status)
                throw std::runtime_error("uvc_get_ctrl(UVC_GET_DEF) failed");

            control_range result(min, max, step, def);
            return result;
        }

        bool rs_uvc_device::get_pu(rs2_option option, int32_t& value) const
        {
            int unit;
            int control = rs2_option_to_ctrl_selector(option, unit);
            unsigned int length = get_data_usb(UVC_GET_LEN, control, unit);

            value = get_data_usb(UVC_GET_CUR, control, unit, length);
            value = rs2_value_translate(UVC_GET_CUR, option, value);

            return true;
        }

        bool rs_uvc_device::set_pu(rs2_option option, int32_t value)
        {
            int unit;
            int control = rs2_option_to_ctrl_selector(option, unit);

            value = rs2_value_translate(UVC_SET_CUR, option, value);
            set_data_usb(UVC_SET_CUR, control, unit, value);

            return true;
        }

        control_range rs_uvc_device::get_pu_range(rs2_option option) const
        {
            int unit = 0;
            int min, max, step, def;
            int control = rs2_option_to_ctrl_selector(option, unit);

            min = get_data_usb(UVC_GET_MIN, control, unit);
            min = rs2_value_translate(UVC_GET_MIN, option, min);

            max = get_data_usb(UVC_GET_MAX, control, unit);
            max = rs2_value_translate(UVC_GET_MAX, option, max);

            step = get_data_usb(UVC_GET_RES, control, unit);
            step = rs2_value_translate(UVC_GET_RES, option, step);

            def = get_data_usb(UVC_GET_DEF, control, unit);
            def = rs2_value_translate(UVC_GET_DEF, option, def);

            control_range result(min, max, step, def);

            return result;
        }

        std::vector<stream_profile> rs_uvc_device::get_profiles() const
        {
            std::vector<stream_profile> results;
            int i = 0;

            check_connection();

            if (get_power_state() != D0) {
                throw std::runtime_error("Device must be powered to query supported profiles!");
            }

            auto formats = get_available_formats_all();

            for(auto&& f : formats)
            {
                stream_profile sp;
                sp.width = f.width;
                sp.height = f.height;
                sp.fps = f.fps;
                sp.format = f.fourcc;
                results.push_back(sp);
            }

            return results;
        }


        void rs_uvc_device::lock() const
        {
            lock_singleton::instance().lock();
        }

        void rs_uvc_device::unlock() const
        {
            lock_singleton::instance().unlock();
        }

        std::string rs_uvc_device::get_device_location() const
        {
            return _location;
        }

        usb_spec rs_uvc_device::get_usb_specification() const
        {
            // On Win7, USB type is determined only when the USB device is created, _info.conn_spec holds wrong information
            return _usb_device->get_info().conn_spec; 
        }

        // Translate between UVC 1.5 Spec and RS
        int32_t rs_uvc_device::rs2_value_translate(uvc_req_code action, rs2_option option,
                                                        int32_t value) const {
            // Value may be translated according to action/option value
            int32_t translated_value = value;

            switch (action) {
                case UVC_GET_CUR: // Translating from UVC 1.5 Spec up to RS
                    if (option == RS2_OPTION_ENABLE_AUTO_EXPOSURE) {
                        switch (value) {
                            case UVC_AE_MODE_D3_AP:
                                translated_value = 1;
                                break;
                            case UVC_AE_MODE_D0_MANUAL:
                                translated_value = 0;
                                break;
                            default:
                                throw std::runtime_error(
                                        "Unsupported GET value for RS2_OPTION_ENABLE_AUTO_EXPOSURE");
                        }
                    }
                    break;

                case UVC_SET_CUR: // Translating from RS down to UVC 1.5 Spec
                    if (option == RS2_OPTION_ENABLE_AUTO_EXPOSURE) {
                        switch (value) {
                            case 1:
                                // Enabling auto exposure
                                translated_value = UVC_AE_MODE_D3_AP;
                                break;
                            case 0:
                                // Disabling auto exposure
                                translated_value = UVC_AE_MODE_D0_MANUAL;
                                break;
                            default:
                                throw std::runtime_error(
                                        "Unsupported SET value for RS2_OPTION_ENABLE_AUTO_EXPOSURE");
                        }
                    }
                    break;

                case UVC_GET_MIN:
                    if (option == RS2_OPTION_ENABLE_AUTO_EXPOSURE) {
                        translated_value = 0; // Hardcoded MIN value
                    }
                    break;

                case UVC_GET_MAX:
                    if (option == RS2_OPTION_ENABLE_AUTO_EXPOSURE) {
                        translated_value = 1; // Hardcoded MAX value
                    }
                    break;

                case UVC_GET_RES:
                    if (option == RS2_OPTION_ENABLE_AUTO_EXPOSURE) {
                        translated_value = 1; // Hardcoded RES (step) value
                    }
                    break;

                case UVC_GET_DEF:
                    if (option == RS2_OPTION_ENABLE_AUTO_EXPOSURE) {
                        translated_value = 1; // Hardcoded DEF value
                    }
                    break;

                default:
                    throw std::runtime_error("Unsupported action translation");
            }
            return translated_value;
        }

        void rs_uvc_device::play_profile(stream_profile profile, frame_callback callback) {
            bool foundFormat = false;
            uvc_format_t selected_format{};
            uint8_t interface_number;
            // Return list of all available formats inside devices[0]
            auto formats = get_available_formats_all();

            for(auto&& f : formats)
            {
                if ((profile.format == f.fourcc) &&
                    (profile.fps == f.fps) &&
                    (profile.height == f.height) &&
                    (profile.width == f.width)) {
                        foundFormat = true;
                        selected_format = f;
                        interface_number = f.interfaceNumber;
                        break;
                }
            }

            if (foundFormat == false) {
                throw std::runtime_error("Failed to find supported format!");
            }

            auto inf = _usb_device->get_interface(interface_number);
            if (inf == nullptr)
                throw std::runtime_error("can't find UVC streaming interface of device: " + _usb_device->get_info().id);
            auto _read_endpoint = inf->first_endpoint(platform::RS2_USB_ENDPOINT_DIRECTION_READ);
            _messenger->reset_endpoint(_read_endpoint, 5000);

            auto ctrl = std::make_shared<uvc_stream_ctrl_t>();
            auto ret = get_stream_ctrl_format_size(selected_format, ctrl);
            if (ret != RS2_USB_STATUS_SUCCESS) {
                throw std::runtime_error("Failed to get control format size!");
            }

            auto sts = query_stream_ctrl(ctrl, 0, UVC_SET_CUR);
            if(sts != RS2_USB_STATUS_SUCCESS)
                throw std::runtime_error("Failed to start streaming!");

            uvc_streamer_context usc = { profile, callback, ctrl, _usb_device, _messenger, _usb_request_count };

            auto streamer = std::make_shared<uvc_streamer>(usc);
            _streamers.push_back(streamer);

           if(_streamers.size() == _profiles.size())
           {
               for(auto&& s : _streamers)
                    s->start();
           }
        }

        void rs_uvc_device::stop_stream_cleanup(const stream_profile &profile,
                                                     std::vector<profile_and_callback>::iterator &elem) {
            if (elem != _streams.end()) {
                elem->callback = nullptr;
                elem->profile.format = 0;
                elem->profile.fps = 0;
                elem->profile.width = 0;
                elem->profile.height = 0;
            }

            auto pos = std::find(_profiles.begin(), _profiles.end(), profile) - _profiles.begin();
            if (pos != _profiles.size()) {
                _profiles.erase(_profiles.begin() + pos);
                _frame_callbacks.erase(_frame_callbacks.begin() + pos);
            }
        }

        // Retreive USB data, sign bit is calculated according to length
        int32_t rs_uvc_device::get_data_usb(uvc_req_code action, int control, int unit,
                                                 unsigned int length) const {
            unsigned char buffer[4] = {0};
            int32_t ret = 0;
            
            usb_status sts = RS2_USB_STATUS_OTHER;
            uint32_t transferred = 0;
            _action_dispatcher.invoke_and_wait([&, this](dispatcher::cancellable_timer c)
            {
                if (_messenger)
                {
                    sts = _messenger->control_transfer(
                            UVC_REQ_TYPE_GET,
                            action,
                            control << 8,
                            unit << 8 | (_info.mi),
                            buffer,
                            sizeof(int32_t),
                            transferred,
                            0);
                }

            }, [this](){ return !_messenger; });

            if (sts != RS2_USB_STATUS_SUCCESS)
                throw std::runtime_error("get_data_usb failed, error: " + usb_status_to_string.at(sts));

            if (transferred != sizeof(int32_t)) 
                throw std::runtime_error("insufficient data read from USB");

            // Converting byte array buffer (with length 8/16/32) to int32
            switch (length) {
                case sizeof(uint8_t):
                    ret = B_TO_BYTE(buffer);
                    break;
                case sizeof(uint16_t) :
                    ret = SW_TO_SHORT(buffer);
                    break;
                case sizeof(uint32_t) :
                    ret = DW_TO_INT(buffer);
                    break;
                default:
                    throw std::runtime_error("unsupported length");
            }

            return ret;
        }

        int rs_uvc_device::rs2_option_to_ctrl_selector(rs2_option option, int &unit) const {
            // chances are that we will need to adjust some of these operation to the
            // processing unit and some to the camera terminal.
            unit = _parser->get_processing_unit().bUnitID;
            switch (option) {
                case RS2_OPTION_BACKLIGHT_COMPENSATION:
                    return UVC_PU_BACKLIGHT_COMPENSATION_CONTROL;
                case RS2_OPTION_BRIGHTNESS:
                    return UVC_PU_BRIGHTNESS_CONTROL;
                case RS2_OPTION_CONTRAST:
                    return UVC_PU_CONTRAST_CONTROL;
                case RS2_OPTION_EXPOSURE:
                    unit = _parser->get_input_terminal().bTerminalID;
                    return UVC_CT_EXPOSURE_TIME_ABSOLUTE_CONTROL;
                case RS2_OPTION_GAIN:
                    return UVC_PU_GAIN_CONTROL;
                case RS2_OPTION_GAMMA:
                    return UVC_PU_GAMMA_CONTROL;
                case RS2_OPTION_HUE:
                    return UVC_PU_HUE_CONTROL;
                case RS2_OPTION_SATURATION:
                    return UVC_PU_SATURATION_CONTROL;
                case RS2_OPTION_SHARPNESS:
                    return UVC_PU_SHARPNESS_CONTROL;
                case RS2_OPTION_WHITE_BALANCE:
                    return UVC_PU_WHITE_BALANCE_TEMPERATURE_CONTROL;
                case RS2_OPTION_ENABLE_AUTO_EXPOSURE:
                    unit = _parser->get_input_terminal().bTerminalID;
                    return UVC_CT_AE_MODE_CONTROL; // Automatic gain/exposure control
                case RS2_OPTION_ENABLE_AUTO_WHITE_BALANCE:
                    return UVC_PU_WHITE_BALANCE_TEMPERATURE_AUTO_CONTROL;
                case RS2_OPTION_POWER_LINE_FREQUENCY:
                    return UVC_PU_POWER_LINE_FREQUENCY_CONTROL;
                case RS2_OPTION_AUTO_EXPOSURE_PRIORITY:
                    unit = _parser->get_input_terminal().bTerminalID;
                    return UVC_CT_AE_PRIORITY_CONTROL;
                default:
                    throw linux_backend_exception(rsutils::string::from() << "invalid option : " << option);
            }
        }

        void rs_uvc_device::set_data_usb(uvc_req_code action, int control, int unit,
                                              int value) const {
            unsigned char buffer[4];
            INT_TO_DW(value, buffer);

            usb_status sts = RS2_USB_STATUS_OTHER;
            uint32_t transferred = 0;

            _action_dispatcher.invoke_and_wait([&, this](dispatcher::cancellable_timer c)
            {
                if (_messenger)
                {
                    sts = _messenger->control_transfer(
                          UVC_REQ_TYPE_SET,
                          action,
                          control << 8,
                          unit << 8 | (_info.mi),
                          buffer,
                          sizeof(int32_t),
                          transferred,
                          0);
                }

            }, [this](){ return !_messenger; });

            if (sts != RS2_USB_STATUS_SUCCESS)
                throw std::runtime_error("set_data_usb failed, error: " + usb_status_to_string.at(sts));

            if (transferred != sizeof(int32_t))
                throw std::runtime_error("insufficient data writen to USB");
        }

        bool rs_uvc_device::uvc_get_ctrl(uint8_t unit, uint8_t ctrl, void *data, int len, uvc_req_code req_code) const
        {
            usb_status sts;
            _action_dispatcher.invoke_and_wait([&, this](dispatcher::cancellable_timer c)
            {
                if (_messenger)
                {
                    uint32_t transferred;
                    sts = _messenger->control_transfer(
                            UVC_REQ_TYPE_GET, req_code,
                            ctrl << 8,
                            unit << 8 | _info.mi,
                            static_cast<unsigned char *>(data),
                            len, transferred, CONTROL_TRANSFER_TIMEOUT);
                }
            }, [this](){ return !_messenger; });

            if (sts == RS2_USB_STATUS_NO_DEVICE)
                throw std::runtime_error("usb device disconnected");

            return sts == RS2_USB_STATUS_SUCCESS;
        }

        bool rs_uvc_device::uvc_set_ctrl(uint8_t unit, uint8_t ctrl, void *data, int len)
        {
            usb_status sts;
            _action_dispatcher.invoke_and_wait([&, this](dispatcher::cancellable_timer c)
            {
                if (_messenger)
                {
                    uint32_t transferred;
                    sts = _messenger->control_transfer(
                            UVC_REQ_TYPE_SET, UVC_SET_CUR,
                            ctrl << 8,
                            unit << 8 | _info.mi,
                            static_cast<unsigned char *>(data),
                            len, transferred, CONTROL_TRANSFER_TIMEOUT);
                }
            }, [this](){ return !_messenger; });

            if (sts == RS2_USB_STATUS_NO_DEVICE)
                throw std::runtime_error("usb device disconnected");

            return sts == RS2_USB_STATUS_SUCCESS;
        }

        void rs_uvc_device::listen_to_interrupts()
        {
            auto ctrl_interface = _usb_device->get_interface(static_cast<uint8_t>(_info.mi));
            if (!ctrl_interface)
                return;
            auto iep = ctrl_interface->first_endpoint(RS2_USB_ENDPOINT_DIRECTION_READ, RS2_USB_ENDPOINT_INTERRUPT);
            if(!iep)
                return;

            _interrupt_callback = std::make_shared<usb_request_callback>
                    ([&](rs_usb_request response)
                     {
                         //TODO:MK Should call the sensor to handle via callback
                         if (response->get_actual_length() > 0)
                         {
                             std::string buff = "";
                             for (int i = 0; i < response->get_actual_length(); i++)
                                 buff += std::to_string(response->get_buffer()[i]) + ", ";
                             LOG_DEBUG("interrupt event received: " << buff.c_str());
                         }

                         _action_dispatcher.invoke([this](dispatcher::cancellable_timer c)
                         {
                             if (!_messenger)
                                 return;
                             _messenger->submit_request(_interrupt_request);
                         });
                     });

            _interrupt_request = _messenger->create_request(iep);
            _interrupt_request->set_buffer(std::vector<uint8_t>(INTERRUPT_BUFFER_SIZE));
            _interrupt_request->set_callback(_interrupt_callback);
            auto sts = _messenger->submit_request(_interrupt_request);
            if (sts != RS2_USB_STATUS_SUCCESS)
                throw std::runtime_error("failed to submit interrupt request, error: " + usb_status_to_string.at(sts));
        }

        void rs_uvc_device::close_uvc_device()
        {
            _streamers.clear();

            if(_interrupt_request)
            {
                _interrupt_callback->cancel();
                _messenger->cancel_request(_interrupt_request);
                _interrupt_request.reset();
            }
        }

        // Probe (Set and Get) streaming control block
        usb_status rs_uvc_device::probe_stream_ctrl(const std::shared_ptr<uvc_stream_ctrl_t>& control)
        {
            // Sending probe SET request - UVC_SET_CUR request in a probe/commit structure containing desired values for VS Format index, VS Frame index, and VS Frame Interval
            // UVC device will check the VS Format index, VS Frame index, and Frame interval properties to verify if possible and update the probe/commit structure if feasible
            auto sts = query_stream_ctrl(control, 1, UVC_SET_CUR);
            if(sts != RS2_USB_STATUS_SUCCESS)
                return sts;

            // Sending probe GET request - UVC_GET_CUR request to read the updated values from UVC device
            sts = query_stream_ctrl(control, 1, UVC_GET_CUR);
                if(sts != RS2_USB_STATUS_SUCCESS)
                    return sts;

            return RS2_USB_STATUS_SUCCESS;
        }

        std::vector<uvc_format_t> rs_uvc_device::get_available_formats_all() const
        {
            std::vector<uvc_format_t> rv;
            for(auto&& s : _parser->get_formats())
            {
                for(auto&& format : s.second)
                {
                    for(auto&& fd : format.frame_descs)
                    {
                        uvc_format_t cur_format{};
                        cur_format.height = fd.wHeight;
                        cur_format.width = fd.wWidth;
                        auto temp = SWAP_UINT32(*(const uint32_t *) format.guidFormat);
                        cur_format.fourcc = fourcc_map.count(temp) ? fourcc_map.at(temp) : temp;
                        cur_format.interfaceNumber = s.first;

                        for(auto&& i : fd.intervals)
                        {
                            if(i == 0)
                                continue;
                            cur_format.fps = 10000000 / i;
                            rv.push_back(cur_format);
                        }
                    }
                }
            }
            return rv;
        }

        usb_status rs_uvc_device::get_stream_ctrl_format_size(uvc_format_t format, const std::shared_ptr<uvc_stream_ctrl_t>& control)
        {
            for(auto&& s : _parser->get_formats())
            {
                for(auto&& curr_format : s.second)
                {
                    auto val = SWAP_UINT32(*(const uint32_t *) curr_format.guidFormat);
                    if(fourcc_map.count(val))
                        val = fourcc_map.at(val);

                    if (format.fourcc != val)
                        continue;

                    for(auto&& fd : curr_format.frame_descs)
                    {
                        if (fd.wWidth != format.width || fd.wHeight != format.height)
                            continue;

                        for(auto&& i : fd.intervals)
                        {
                            if (10000000 / i == (unsigned int) format.fps || format.fps == 0) {

                                /* get the max values -- we need the interface number to be able
                                to do this */
                                control->bInterfaceNumber = s.first;
                                auto sts = query_stream_ctrl(control, 1, UVC_GET_MAX);
                                if(sts != RS2_USB_STATUS_SUCCESS)
                                    return sts;

                                control->bmHint = (1 << 0); /* don't negotiate interval */
                                control->bFormatIndex = curr_format.bFormatIndex;
                                control->bFrameIndex = fd.bFrameIndex;
                                control->dwFrameInterval = i;
                                return probe_stream_ctrl(control);
                            }
                        }
                    }
                }
            }
            return RS2_USB_STATUS_INVALID_PARAM;
        }

        usb_status rs_uvc_device::query_stream_ctrl(const std::shared_ptr<uvc_stream_ctrl_t>& ctrl, uint8_t probe, int req)
        {
            uint8_t buf[48];
            size_t len;
            int err = 0;

            memset(buf, 0, sizeof(buf));
            switch (_parser->get_bcd_uvc()) {
                case 0x0110:
                    len = 34;
                    break;
                case 0x0150:
                    len = 48;
                    break;
                default:
                    len = 26;
                    break;
            }

            /* prepare for a SET transfer */
            if (req == UVC_SET_CUR) {
                SHORT_TO_SW(ctrl->bmHint, buf);
                buf[2] = ctrl->bFormatIndex;
                buf[3] = ctrl->bFrameIndex;
                INT_TO_DW(ctrl->dwFrameInterval, buf + 4);
                SHORT_TO_SW(ctrl->wKeyFrameRate, buf + 8);
                SHORT_TO_SW(ctrl->wPFrameRate, buf + 10);
                SHORT_TO_SW(ctrl->wCompQuality, buf + 12);
                SHORT_TO_SW(ctrl->wCompWindowSize, buf + 14);
                SHORT_TO_SW(ctrl->wDelay, buf + 16);
                INT_TO_DW(ctrl->dwMaxVideoFrameSize, buf + 18);
                INT_TO_DW(ctrl->dwMaxPayloadTransferSize, buf + 22);
                INT_TO_DW(0, buf + 18);
                INT_TO_DW(0, buf + 22);

                if (len >= 34) {
                    INT_TO_DW(ctrl->dwClockFrequency, buf + 26);
                    buf[30] = ctrl->bmFramingInfo;
                    buf[31] = ctrl->bPreferredVersion;
                    buf[32] = ctrl->bMinVersion;
                    buf[33] = ctrl->bMaxVersion;
                    /** @todo support UVC 1.1 */
                }

                if (len == 48) {
                    buf[34] = ctrl->bUsage;
                    buf[35] = ctrl->bBitDepthLuma;
                    buf[36] = ctrl->bmSettings;
                    buf[37] = ctrl->bMaxNumberOfRefFramesPlus1;
                    SHORT_TO_SW(ctrl->bmRateControlModes, buf + 38);
                    QUAD_TO_QW(ctrl->bmLayoutPerStream, buf + 40);
                }
            }

            usb_status sts;
            _action_dispatcher.invoke_and_wait([&, this](dispatcher::cancellable_timer c)
            {
                if (_messenger)
                {
                    int retries = 0;
                    do {
                        uint32_t transferred;
                        sts = _messenger->control_transfer(
                                req == UVC_SET_CUR ? UVC_REQ_TYPE_SET : UVC_REQ_TYPE_GET,
                                req,
                                probe ? (UVC_VS_PROBE_CONTROL << 8) : (UVC_VS_COMMIT_CONTROL << 8),
                                ctrl->bInterfaceNumber, // When requestType is directed to an interface, the driver automatically passes the interface number in the low byte of index
                                buf, static_cast<uint32_t>(len), transferred, 0);
                    } while (sts != RS2_USB_STATUS_SUCCESS && retries++ < 5);
                }
            }, [this](){ return !_messenger; });

            if (sts != RS2_USB_STATUS_SUCCESS)
            {
                LOG_ERROR("Probe-commit control transfer failed with error: " << usb_status_to_string.at(sts));
                return sts;
            }

            /* now decode following a GET transfer */
            if (req != UVC_SET_CUR) {
                ctrl->bmHint = SW_TO_SHORT(buf);
                ctrl->bFormatIndex = buf[2];
                ctrl->bFrameIndex = buf[3];
                ctrl->dwFrameInterval = DW_TO_INT(buf + 4);
                ctrl->wKeyFrameRate = SW_TO_SHORT(buf + 8);
                ctrl->wPFrameRate = SW_TO_SHORT(buf + 10);
                ctrl->wCompQuality = SW_TO_SHORT(buf + 12);
                ctrl->wCompWindowSize = SW_TO_SHORT(buf + 14);
                ctrl->wDelay = SW_TO_SHORT(buf + 16);
                ctrl->dwMaxVideoFrameSize = DW_TO_INT(buf + 18);
                ctrl->dwMaxPayloadTransferSize = DW_TO_INT(buf + 22);

                if (len >= 34) {
                    ctrl->dwClockFrequency = DW_TO_INT(buf + 26);
                    ctrl->bmFramingInfo = buf[30];
                    ctrl->bPreferredVersion = buf[31];
                    ctrl->bMinVersion = buf[32];
                    ctrl->bMaxVersion = buf[33];
                    /** @todo support UVC 1.1 */
                }

                if (len == 48) {
                    ctrl->bUsage = buf[34];
                    ctrl->bBitDepthLuma = buf[35];
                    ctrl->bmSettings = buf[36];
                    ctrl->bMaxNumberOfRefFramesPlus1 = buf[37];
                    ctrl->bmRateControlModes = DW_TO_INT(buf + 38);
                    ctrl->bmLayoutPerStream = QW_TO_QUAD(buf + 40);
                }
                else
                    ctrl->dwClockFrequency = _parser->get_clock_frequency();
            }
            return RS2_USB_STATUS_SUCCESS;
        }

        void rs_uvc_device::check_connection() const
        {
            auto devices = usb_enumerator::query_devices_info();
            for (auto&& usb_info : devices)
            {
                if(usb_info.unique_id == _info.unique_id)
                    return;
            }
            throw std::runtime_error("Camera is no longer connected!");
        }
    }
}
