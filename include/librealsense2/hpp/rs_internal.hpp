// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 RealSense, Inc. All Rights Reserved.

#ifndef LIBREALSENSE_RS2_INTERNAL_HPP
#define LIBREALSENSE_RS2_INTERNAL_HPP

#include "rs_types.hpp"
#include "rs_device.hpp"
#include "rs_context.hpp"
#include "../h/rs_internal.h"

namespace rs2
{
    namespace internal
    {
        /**
        * \return            the time at specific time point, in live and redord contextes it will return the system time and in playback contextes it will return the recorded time
        */
        inline double get_time()
        {
            rs2_error* e = nullptr;
            auto time = rs2_get_time( &e);

            error::handle(e);

            return time;
        }
    }

    template<class T>
    class software_device_destruction_callback : public rs2_software_device_destruction_callback
    {
        T on_destruction_function;
    public:
        explicit software_device_destruction_callback(T on_destruction) : on_destruction_function(on_destruction) {}

        void on_destruction() override
        {
            on_destruction_function();
        }

        void release() override { delete this; }
    };

    class software_sensor : public sensor
    {
    public:
        /**
        * Add video stream to software sensor
        *
        * \param[in] video_stream   all the parameters that required to defind video stream
        */
        stream_profile add_video_stream(rs2_video_stream video_stream, bool is_default=false)
        {
            rs2_error* e = nullptr;

            auto profile = rs2_software_sensor_add_video_stream_ex(_sensor.get(), video_stream, is_default, &e);
            error::handle(e);

            stream_profile stream(profile);
            return stream;
        }

        /**
        * Add motion stream to software sensor
        *
        * \param[in] motion   all the parameters that required to defind motion stream
        */
        stream_profile add_motion_stream(rs2_motion_stream motion_stream, bool is_default=false)
        {
            rs2_error* e = nullptr;

            auto profile = rs2_software_sensor_add_motion_stream_ex(_sensor.get(), motion_stream, is_default, &e);
            error::handle(e);

            stream_profile stream(profile);
            return stream;
        }

        /**
        * Add pose stream to software sensor
        *
        * \param[in] pose   all the parameters that required to defind pose stream
        */
        stream_profile add_pose_stream(rs2_pose_stream pose_stream, bool is_default=false)
        {
            rs2_error* e = nullptr;

            auto profile = rs2_software_sensor_add_pose_stream_ex(_sensor.get(), pose_stream, is_default, &e);
            error::handle(e);

            stream_profile stream(profile);
            return stream;
        }

        /**
        * Add inference stream to software sensor
        *
        * \param[in] inference_stream   all the parameters that required to define an inference stream
        */
        stream_profile add_inference_stream(rs2_inference_stream inference_stream, bool is_default=false)
        {
            rs2_error* e = nullptr;

            auto profile = rs2_software_sensor_add_inference_stream_ex(_sensor.get(), inference_stream, is_default, &e);
            error::handle(e);

            stream_profile stream(profile);
            return stream;
        }

        /**
        * Inject video frame into the sensor
        *
        * \param[in] frame   all the parameters that required to define video frame
        */
        void on_video_frame(rs2_software_video_frame frame)
        {
            rs2_error* e = nullptr;
            rs2_software_sensor_on_video_frame(_sensor.get(), frame, &e);
            error::handle(e);
        }

        /**
        * Inject motion frame into the sensor
        *
        * \param[in] frame   all the parameters that required to define motion frame
        */
        void on_motion_frame(rs2_software_motion_frame frame)
        {
            rs2_error* e = nullptr;
            rs2_software_sensor_on_motion_frame(_sensor.get(), frame, &e);
            error::handle(e);
        }

        /**
        * Inject pose frame into the sensor
        *
        * \param[in] frame   all the parameters that required to define pose frame
        */
        void on_pose_frame(rs2_software_pose_frame frame)
        {
            rs2_error* e = nullptr;
            rs2_software_sensor_on_pose_frame(_sensor.get(), frame, &e);
            error::handle(e);
        }

        /**
        * Set frame metadata for the upcoming frames
        * \param[in] value metadata key to set
        * \param[in] type metadata value
        */
        void set_metadata(rs2_frame_metadata_value value, rs2_metadata_type type)
        {
            rs2_error* e = nullptr;
            rs2_software_sensor_set_metadata(_sensor.get(), value, type, &e);
            error::handle(e);
        }

        /**
        * Register read-only option that will be supported by the sensor
        *
        * \param[in] option  the option
        * \param[in] val  the initial value
        */
        void add_read_only_option(rs2_option option, float val)
        {
            rs2_error* e = nullptr;
            rs2_software_sensor_add_read_only_option(_sensor.get(), option, val, &e);
            error::handle(e);
        }

        /**
        * Update value of registered read-only option
        *
        * \param[in] option  the option
        * \param[in] val  the initial value
        */
        void set_read_only_option(rs2_option option, float val)
        {
            rs2_error* e = nullptr;
            rs2_software_sensor_update_read_only_option(_sensor.get(), option, val, &e);
            error::handle(e);
        }
        /**
        * Register option that will be supported by the sensor
        *
        * \param[in] option  the option
        * \param[in] range  range data for the option. range.def will be used as the initial value
        */
        void add_option(rs2_option option, const option_range& range, bool is_writable=true)
        {
            rs2_error* e = nullptr;
            rs2_software_sensor_add_option(_sensor.get(), option, range.min,
                range.max, range.step, range.def, is_writable, &e);
            error::handle(e);
        }

        void on_notification(rs2_software_notification notif)
        {
            rs2_error * e = nullptr;
            rs2_software_sensor_on_notification(_sensor.get(), notif, &e);
            error::handle(e);
        }
        /**
        * Sensors hold the parent device in scope via a shared_ptr. This function detaches that so that the
        * software sensor doesn't keep the software device alive. Note that this is dangerous as it opens the
        * door to accessing freed memory if care isn't taken.
        */
        void detach()
        {
            rs2_error * e = nullptr;
            rs2_software_sensor_detach(_sensor.get(), &e);
            error::handle(e);
        }

    private:
        friend class software_device;

        software_sensor(std::shared_ptr<rs2_sensor> s)
            : rs2::sensor(s)
        {
            rs2_error* e = nullptr;
            if (rs2_is_sensor_extendable_to(_sensor.get(), RS2_EXTENSION_SOFTWARE_SENSOR, &e) == 0 && !e)
            {
                _sensor = nullptr;
            }
            rs2::error::handle(e);
        }
    };


    class software_device : public device
    {
        std::shared_ptr<rs2_device> create_device_ptr(std::function<void(rs2_device*)> deleter)
        {
            rs2_error* e = nullptr;
            std::shared_ptr<rs2_device> dev(
                rs2_create_software_device(&e),
                deleter);
            error::handle(e);
            return dev;
        }

    public:
        software_device(std::function<void(rs2_device*)> deleter = &rs2_delete_device)
            : device(create_device_ptr(deleter))
        {
            this->set_destruction_callback([]{});
        }

        software_device(std::string name)
            : device(create_device_ptr(&rs2_delete_device))
        {
            update_info(RS2_CAMERA_INFO_NAME, name);
        }

        /**
        * Add software sensor with given name to the software device.
        *
        * \param[in] name   the name of the sensor
        */
        software_sensor add_sensor(std::string name)
        {
            rs2_error* e = nullptr;
            std::shared_ptr<rs2_sensor> sensor(
                rs2_software_device_add_sensor(_dev.get(), name.c_str(), &e),
                rs2_delete_sensor);
            error::handle(e);

            return software_sensor(sensor);
        }

        /**
        * Register destruction callback
        * \param[in] callback   destruction callback
        */
        template<class T>
        void set_destruction_callback(T callback) const
        {
            rs2_error* e = nullptr;
            rs2_software_device_set_destruction_callback_cpp(_dev.get(),
                new software_device_destruction_callback<T>(std::move(callback)), &e);
            error::handle(e);
        }

        /**
        * Add software device to existing context.
        * Any future queries on the context will return this device.
        * This operation cannot be undone (except for destroying the context)
        *
        * \param[in] ctx   context to add the device to
        */
        void add_to(context& ctx)
        {
            rs2_error* e = nullptr;
            rs2_context_add_software_device(ctx._context.get(), _dev.get(), &e);
            error::handle(e);
        }

        /**
        * Add a new camera info value, like serial number
        *
        * \param[in] info  Identifier of the camera info type
        * \param[in] val   string value to set to this camera info type
        */
        void register_info(rs2_camera_info info, const std::string& val)
        {
            rs2_error* e = nullptr;
            rs2_software_device_register_info(_dev.get(), info, val.c_str(), &e);
            error::handle(e);
        }

        /**
        * Update an existing camera info value, like serial number
        *
        * \param[in] info  Identifier of the camera info type
        * \param[in] val   string value to set to this camera info type
        */
        void update_info(rs2_camera_info info, const std::string& val)
        {
            rs2_error* e = nullptr;
            rs2_software_device_update_info(_dev.get(), info, val.c_str(), &e);
            error::handle(e);
        }

        /**
        * Set the wanted matcher type that will be used by the syncer
        * \param[in] matcher matcher type
        */
        void create_matcher(rs2_matchers matcher)
        {
            rs2_error* e = nullptr;
            rs2_software_device_create_matcher(_dev.get(), matcher, &e);
            error::handle(e);
        }
    };

    class firmware_log_message
    {
    public:
        explicit firmware_log_message(std::shared_ptr<rs2_firmware_log_message> msg) :
            _fw_log_message(msg) {}

        rs2_log_severity get_severity() const {
            rs2_error* e = nullptr;
            rs2_log_severity severity = rs2_fw_log_message_severity(_fw_log_message.get(), &e);
            error::handle(e);
            return severity;
        }
        std::string get_severity_str() const {
            return rs2_log_severity_to_string(get_severity());
        }

        uint32_t get_timestamp() const
        {
            rs2_error* e = nullptr;
            uint32_t timestamp = rs2_fw_log_message_timestamp(_fw_log_message.get(), &e);
            error::handle(e);
            return timestamp;
        }

        int size() const
        {
            rs2_error* e = nullptr;
            int size = rs2_fw_log_message_size(_fw_log_message.get(), &e);
            error::handle(e);
            return size;
        }

        std::vector<uint8_t> data() const
        {
            rs2_error* e = nullptr;
            auto size = rs2_fw_log_message_size(_fw_log_message.get(), &e);
            error::handle(e);
            std::vector<uint8_t> result;
            if (size > 0)
            {
                auto start = rs2_fw_log_message_data(_fw_log_message.get(), &e);
                error::handle(e);
                result.insert(result.begin(), start, start + size);
            }
            return result;
        }

        const std::shared_ptr<rs2_firmware_log_message> get_message() const { return _fw_log_message; }

    private:
        std::shared_ptr<rs2_firmware_log_message> _fw_log_message;
    };

    class firmware_log_parsed_message
    {
    public:
        explicit firmware_log_parsed_message(std::shared_ptr<rs2_firmware_log_parsed_message> msg) :
            _parsed_fw_log(msg) {}

        std::string message() const
        {
            rs2_error* e = nullptr;
            std::string msg(rs2_get_fw_log_parsed_message(_parsed_fw_log.get(), &e));
            error::handle(e);
            return msg;
        }
        std::string file_name() const
        {
            rs2_error* e = nullptr;
            std::string file_name(rs2_get_fw_log_parsed_file_name(_parsed_fw_log.get(), &e));
            error::handle(e);
            return file_name;
        }
        std::string thread_name() const
        {
            rs2_error* e = nullptr;
            std::string name(rs2_get_fw_log_parsed_thread_name(_parsed_fw_log.get(), &e));
            error::handle(e);
            return name;
        }
        std::string module_name() const
        {
            rs2_error * e = nullptr;
            std::string name( rs2_get_fw_log_parsed_module_name( _parsed_fw_log.get(), &e ) );
            error::handle( e );
            return name;
        }
        std::string severity() const
        {
            rs2_error* e = nullptr;
            rs2_log_severity sev = rs2_get_fw_log_parsed_severity(_parsed_fw_log.get(), &e);
            error::handle(e);
            return std::string(rs2_log_severity_to_string(sev));
        }
        uint32_t line() const
        {
            rs2_error* e = nullptr;
            uint32_t line(rs2_get_fw_log_parsed_line(_parsed_fw_log.get(), &e));
            error::handle(e);
            return line;
        }
        uint32_t timestamp() const
        {
            rs2_error* e = nullptr;
            uint32_t timestamp(rs2_get_fw_log_parsed_timestamp(_parsed_fw_log.get(), &e));
            error::handle(e);
            return timestamp;
        }

        uint32_t sequence_id() const
        {
            rs2_error* e = nullptr;
            uint32_t sequence(rs2_get_fw_log_parsed_sequence_id(_parsed_fw_log.get(), &e));
            error::handle(e);
            return sequence;
        }

        const std::shared_ptr<rs2_firmware_log_parsed_message> get_message() const { return _parsed_fw_log; }

    private:
        std::shared_ptr<rs2_firmware_log_parsed_message> _parsed_fw_log;
    };

    class firmware_logger : public device
    {
    public:
        firmware_logger(device d)
            : device(d.get())
        {
            rs2_error* e = nullptr;
            if (rs2_is_device_extendable_to(_dev.get(), RS2_EXTENSION_FW_LOGGER, &e) == 0 && !e)
            {
                _dev.reset();
            }
            error::handle(e);
        }

        void start_collecting()
        {
            rs2_error * e = nullptr;
            rs2_start_collecting_fw_logs( _dev.get(), &e );
            error::handle( e );
        }

        void stop_collecting()
        {
            rs2_error * e = nullptr;
            rs2_stop_collecting_fw_logs( _dev.get(), &e );
            error::handle( e );
        }

        rs2::firmware_log_message create_message()
        {
            rs2_error* e = nullptr;
            std::shared_ptr<rs2_firmware_log_message> msg(
                rs2_create_fw_log_message(_dev.get(), &e),
                rs2_delete_fw_log_message);
            error::handle(e);

            return firmware_log_message(msg);
        }

        rs2::firmware_log_parsed_message create_parsed_message()
        {
            rs2_error* e = nullptr;
            std::shared_ptr<rs2_firmware_log_parsed_message> msg(
                rs2_create_fw_log_parsed_message(_dev.get(), &e),
                rs2_delete_fw_log_parsed_message);
            error::handle(e);

            return firmware_log_parsed_message(msg);
        }

        bool get_firmware_log(rs2::firmware_log_message& msg) const
        {
            rs2_error* e = nullptr;
            rs2_firmware_log_message* m = msg.get_message().get();
            bool fw_log_pulling_status =
                !!rs2_get_fw_log(_dev.get(), m, &e);

            error::handle(e);

            return fw_log_pulling_status;
        }

        bool get_flash_log(rs2::firmware_log_message& msg) const
        {
            rs2_error* e = nullptr;
            rs2_firmware_log_message* m = msg.get_message().get();
            bool flash_log_pulling_status =
                !!rs2_get_flash_log(_dev.get(), m, &e);

            error::handle(e);

            return flash_log_pulling_status;
        }

        bool init_parser(const std::string& xml_content)
        {
            rs2_error* e = nullptr;

            bool parser_initialized = !!rs2_init_fw_log_parser(_dev.get(), xml_content.c_str(), &e);
            error::handle(e);

            return parser_initialized;
        }

        bool parse_log(const rs2::firmware_log_message& msg, const rs2::firmware_log_parsed_message& parsed_msg)
        {
            rs2_error* e = nullptr;

            bool parsingResult = !!rs2_parse_firmware_log(_dev.get(), msg.get_message().get(), parsed_msg.get_message().get(), &e);
            error::handle(e);

            return parsingResult;
        }

        unsigned int get_number_of_fw_logs() const
        {
            rs2_error* e = nullptr;
            unsigned int num_of_fw_logs = rs2_get_number_of_fw_logs(_dev.get(), &e);
            error::handle(e);

            return num_of_fw_logs;
        }
    };

    class terminal_parser
    {
    public:
        terminal_parser(const std::string& xml_content)
        {
            rs2_error* e = nullptr;

            _terminal_parser = std::shared_ptr<rs2_terminal_parser>(
                rs2_create_terminal_parser(xml_content.c_str(), &e),
                rs2_delete_terminal_parser);
            error::handle(e);
        }

        std::vector<uint8_t> parse_command(const std::string& command)
        {
            rs2_error* e = nullptr;

            std::shared_ptr<const rs2_raw_data_buffer> list(
                rs2_terminal_parse_command(_terminal_parser.get(), command.c_str(), (unsigned int)command.size(), &e),
                rs2_delete_raw_data);
            error::handle(e);

            auto size = rs2_get_raw_data_size(list.get(), &e);
            error::handle(e);

            auto start = rs2_get_raw_data(list.get(), &e);

            std::vector<uint8_t> results;
            results.insert(results.begin(), start, start + size);

            return results;
        }

        std::string parse_response(const std::string& command, const std::vector<uint8_t>& response)
        {
            rs2_error* e = nullptr;

            std::shared_ptr<const rs2_raw_data_buffer> list(
                rs2_terminal_parse_response(_terminal_parser.get(), command.c_str(), (unsigned int)command.size(),
                (void*)response.data(), (unsigned int)response.size(), &e),
                rs2_delete_raw_data);
            error::handle(e);

            auto size = rs2_get_raw_data_size(list.get(), &e);
            error::handle(e);

            auto start = rs2_get_raw_data(list.get(), &e);

            std::string results;
            results.insert(results.begin(), start, start + size);

            return results;
        }

    private:
        std::shared_ptr<rs2_terminal_parser> _terminal_parser;
    };

}
#endif // LIBREALSENSE_RS2_INTERNAL_HPP
