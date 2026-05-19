/* License: Apache 2.0. See LICENSE file in root directory.
Copyright(c) 2017 RealSense, Inc. All Rights Reserved. */

#include "pyrealsense2.h"
#include <librealsense2/hpp/rs_internal.hpp>
#include <cstring>

void init_internal(py::module &m) {
    /** rs_internal.h **/
    py::class_<rs2_video_stream> video_stream(m, "video_stream", "All the parameters"
                                              " required to define a video stream.");
    video_stream.def(py::init<>())
        .def_readwrite("type", &rs2_video_stream::type)
        .def_readwrite("index", &rs2_video_stream::index)
        .def_readwrite("uid", &rs2_video_stream::uid)
        .def_readwrite("width", &rs2_video_stream::width)
        .def_readwrite("height", &rs2_video_stream::height)
        .def_readwrite("fps", &rs2_video_stream::fps)
        .def_readwrite("bpp", &rs2_video_stream::bpp)
        .def_readwrite("fmt", &rs2_video_stream::fmt)
        .def_readwrite("intrinsics", &rs2_video_stream::intrinsics);

    py::class_<rs2_motion_stream> motion_stream(m, "motion_stream", "All the parameters"
        " required to define a motion stream.");
    motion_stream.def(py::init<>())
        .def_readwrite("type", &rs2_motion_stream::type)
        .def_readwrite("index", &rs2_motion_stream::index)
        .def_readwrite("uid", &rs2_motion_stream::uid)
        .def_readwrite("fps", &rs2_motion_stream::fps)
        .def_readwrite("fmt", &rs2_motion_stream::fmt)
        .def_readwrite("intrinsics", &rs2_motion_stream::intrinsics);

    py::class_<rs2_pose_stream> pose_stream(m, "pose_stream", "All the parameters"
        " required to define a pose stream.");
    pose_stream.def(py::init<>())
        .def_readwrite("type", &rs2_pose_stream::type)
        .def_readwrite("index", &rs2_pose_stream::index)
        .def_readwrite("uid", &rs2_pose_stream::uid)
        .def_readwrite("fps", &rs2_pose_stream::fps)
        .def_readwrite("fmt", &rs2_pose_stream::fmt);

    py::class_<rs2_inference_stream> inference_stream(m, "inference_stream", "All the parameters"
        " required to define an inference stream.");
    inference_stream.def(py::init<>())
        .def_readwrite("type",  &rs2_inference_stream::type)
        .def_readwrite("index", &rs2_inference_stream::index)
        .def_readwrite("uid",   &rs2_inference_stream::uid)
        .def_readwrite("fps",   &rs2_inference_stream::fps)
        .def_readwrite("fmt",   &rs2_inference_stream::fmt);

    py::class_< rs2_software_video_frame >( m,
                                            "software_video_frame",
                                            "All the parameters required to define a video frame" )
        .def( py::init( []() { return rs2_software_video_frame{ 0 }; } ) )
        .def_property(
            "pixels",
            []( const rs2_software_video_frame & self ) {
                if( ! self.profile || ! self.pixels )
                    return BufData( nullptr, 1, "b", 0 );
                // TODO: Not all formats (e.g. RAW10) are properly handled (see struct FmtToType in
                // python.hpp)
                auto vp = rs2::stream_profile( self.profile ).as< rs2::video_stream_profile >();
                size_t size = fmt_to_value< itemsize >( vp.format() );
                size_t upp = self.bpp / size;
                if( upp == 1 )
                    return BufData( self.pixels,
                                    size,
                                    fmt_to_value< fmtstring >( vp.format() ),
                                    2,
                                    { (size_t)vp.height(), (size_t)vp.width() },
                                    { vp.width() * size, size } );
                else
                    return BufData( self.pixels,
                                    size,
                                    fmt_to_value< fmtstring >( vp.format() ),
                                    3,
                                    { (size_t)vp.height(), (size_t)vp.width(), upp },
                                    { vp.width() * upp * size, upp * size, size } );
            },
            []( rs2_software_video_frame & self, py::buffer buf ) {
                if( self.deleter )
                    self.deleter( self.pixels );
                auto data = buf.request();
                if( ! data.size || ! data.itemsize )
                {
                    self.pixels = nullptr;
                    self.deleter = nullptr;
                }
                else
                {
                    self.pixels
                        = new uint8_t[data.size
                                      * data.itemsize];  // fwiw, might be possible to convert
                                                         // data.format -> underlying data type
                    std::memcpy( self.pixels, data.ptr, data.size * data.itemsize );
                    self.deleter = []( void * ptr ) {
                        delete[]( uint8_t * ) ptr;
                    };
                }
            } )
        .def_readwrite("stride", &rs2_software_video_frame::stride)
        .def_readwrite("bpp", &rs2_software_video_frame::bpp)
        .def_readwrite("timestamp", &rs2_software_video_frame::timestamp)
        .def_readwrite("domain", &rs2_software_video_frame::domain)
        .def_readwrite("frame_number", &rs2_software_video_frame::frame_number)
        .def_readwrite("depth_units", &rs2_software_video_frame::depth_units)
        .def_property(
            "profile",
            []( const rs2_software_video_frame & self ) {
                if( ! self.profile )
                    return rs2::video_stream_profile();
                else
                    return rs2::stream_profile( self.profile ).as< rs2::video_stream_profile >();
            },
            []( rs2_software_video_frame & self, rs2::video_stream_profile const & profile ) {
                self.profile = profile.get();
            } )
        .def( "__repr__", []( const rs2_software_video_frame& self ) {
            std::ostringstream ss;
            ss << "<" << SNAME << ".software_video_frame";
            if( self.profile )
            {
                rs2::stream_profile profile( self.profile );
                ss << " " << rs2_format_to_string( profile.format() );
            }
            ss << " #" << self.frame_number;
            ss << " @" << self.timestamp;
            ss << ">";
            return ss.str();
        } );

    py::class_<rs2_software_motion_frame> software_motion_frame(m, "software_motion_frame", "All the parameters "
                                                                "required to define a motion frame.");
    software_motion_frame  //
        .def( py::init( []() { return rs2_software_motion_frame{ 0 }; } ) )
        .def_property("data", [](const rs2_software_motion_frame& self) -> rs2_vector {
            auto data = reinterpret_cast<const float*>(self.data);
            return rs2_vector{ data[0], data[1], data[2] };
        }, [](rs2_software_motion_frame& self, rs2_vector data) {
            if (self.deleter) self.deleter(self.data);
            self.data = new float[3];
            float *dptr = reinterpret_cast<float*>(self.data);
            dptr[0] = data.x;
            dptr[1] = data.y;
            dptr[2] = data.z;
            self.deleter = [](void* ptr){ delete[]( float * ) ptr; };
        })
        .def_readwrite("timestamp", &rs2_software_motion_frame::timestamp)
        .def_readwrite("domain", &rs2_software_motion_frame::domain)
        .def_readwrite("frame_number", &rs2_software_motion_frame::frame_number)
        .def_property("profile", [](const rs2_software_motion_frame& self) { return rs2::stream_profile(self.profile).as<rs2::motion_stream_profile>(); },
                      [](rs2_software_motion_frame& self, rs2::motion_stream_profile profile) { self.profile = profile.get(); });

    py::class_<rs2_software_pose_frame> software_pose_frame(m, "software_pose_frame", "All the parameters "
                                                            "required to define a pose frame.");
    software_pose_frame  //
        .def( py::init( []() { return rs2_software_pose_frame{ 0 }; } ) )
        .def_property(
            "data",
            []( const rs2_software_pose_frame & self ) -> rs2_pose {
                return *reinterpret_cast< const rs2_pose * >( self.data );
            },
            []( rs2_software_pose_frame & self, rs2_pose data ) {
                if( self.deleter )
                    self.deleter( self.data );
                self.data = new rs2_pose( data );
                self.deleter = []( void * ptr ) {
                    delete(rs2_pose *)ptr;
                };
            } )
        .def_readwrite("timestamp", &rs2_software_pose_frame::timestamp)
        .def_readwrite("domain", &rs2_software_pose_frame::domain)
        .def_readwrite("frame_number", &rs2_software_pose_frame::frame_number)
        .def_property("profile", [](const rs2_software_pose_frame& self) { return rs2::stream_profile(self.profile).as<rs2::pose_stream_profile>(); },
                      [](rs2_software_pose_frame& self, rs2::pose_stream_profile profile) { self.profile = profile.get(); });

    py::class_<rs2_software_notification> software_notification(m, "software_notification", "All the parameters "
                                                                "required to define a sensor notification.");
    software_notification.def(py::init<>())
        .def_readwrite("category", &rs2_software_notification::category)
        .def_readwrite("type", &rs2_software_notification::type)
        .def_readwrite("severity", &rs2_software_notification::severity)
        .def_readwrite("description", &rs2_software_notification::description)
        .def_readwrite("serialized_data", &rs2_software_notification::serialized_data);
    /** end rs_internal.h **/
    
    /** rs_internal.hpp **/
    // rs2::software_sensor
    py::class_<rs2::software_sensor, rs2::sensor, py_holder<rs2::software_sensor>> software_sensor(m, "software_sensor");
    software_sensor
        .def( "add_video_stream",
              &rs2::software_sensor::add_video_stream,
              "Add video stream to software sensor",
              "video_stream"_a,
              "is_default"_a = false )
        .def("add_motion_stream", &rs2::software_sensor::add_motion_stream, "Add motion stream to software sensor",
            "motion_stream"_a, "is_default"_a = false)
        .def("add_pose_stream", &rs2::software_sensor::add_pose_stream, "Add pose stream to software sensor",
            "pose_stream"_a, "is_default"_a = false)
        .def("add_inference_stream", &rs2::software_sensor::add_inference_stream, "Add inference stream to software sensor",
            "inference_stream"_a, "is_default"_a = false)
        .def("on_video_frame", &rs2::software_sensor::on_video_frame, "Inject video frame into the sensor", "frame"_a)
        .def("on_motion_frame", &rs2::software_sensor::on_motion_frame, "Inject motion frame into the sensor", "frame"_a)
        .def("on_pose_frame", &rs2::software_sensor::on_pose_frame, "Inject pose frame into the sensor", "frame"_a)
        .def("set_metadata", &rs2::software_sensor::set_metadata, "Set frame metadata for the upcoming frames", "value"_a, "type"_a)
        .def("add_read_only_option", &rs2::software_sensor::add_read_only_option, "Register read-only option that "
             "will be supported by the sensor", "option"_a, "val"_a)
        .def("set_read_only_option", &rs2::software_sensor::set_read_only_option, "Update value of registered "
             "read-only option", "option"_a, "val"_a)
        .def("add_option", &rs2::software_sensor::add_option, "Register option that will be supported by the sensor",
             "option"_a, "range"_a, "is_writable"_a=true)
        .def("on_notification", &rs2::software_sensor::on_notification, "notif"_a);

    // rs2::software_device
    py::class_<rs2::software_device, rs2::device, py_holder<rs2::software_device>> software_device(m, "software_device");
    software_device.def(py::init<>())
        .def("add_sensor", &rs2::software_device::add_sensor, "Add software sensor with given name "
            "to the software device.", "name"_a)
        .def("set_destruction_callback", &rs2::software_device::set_destruction_callback<std::function<void()>>,
             "Register destruction callback", "callback"_a)
        .def("add_to", &rs2::software_device::add_to, "Add software device to existing context.\n"
             "Any future queries on the context will return this device.\n"
             "This operation cannot be undone (except for destroying the context)", "ctx"_a)
        .def("register_info", &rs2::software_device::register_info, "Add a new camera info value, like serial number",
             "info"_a, "val"_a)
        .def("update_info", &rs2::software_device::update_info, "Update an existing camera info value, like serial number",
             "info"_a, "val"_a)
        .def("create_matcher", &rs2::software_device::create_matcher, "Set the wanted matcher type that will "
            "be used by the syncer", "matcher"_a);

    // rs2::firmware_log_message
    py::class_<rs2::firmware_log_message> firmware_log_message(m, "firmware_log_message");
    firmware_log_message.def("get_severity", &rs2::firmware_log_message::get_severity, "Get severity ")
        .def("get_severity_str", &rs2::firmware_log_message::get_severity_str, "Get severity string ")
        .def("get_timestamp", &rs2::firmware_log_message::get_timestamp, "Get timestamp ")
        .def("get_data", &rs2::firmware_log_message::data, "Get data ")
        .def("get_size", &rs2::firmware_log_message::size, "Get size ");

    // rs2::firmware_log_parsed_message
    py::class_<rs2::firmware_log_parsed_message> firmware_log_parsed_message(m, "firmware_log_parsed_message");
    firmware_log_parsed_message.def("get_message", &rs2::firmware_log_parsed_message::message, "Get message ")
        .def("get_file_name", &rs2::firmware_log_parsed_message::file_name, "Get file name ")
        .def("get_thread_name", &rs2::firmware_log_parsed_message::thread_name, "Get thread name ")
        .def("get_module_name", &rs2::firmware_log_parsed_message::module_name, "Get module name ")
        .def("get_severity", &rs2::firmware_log_parsed_message::severity, "Get severity ")
        .def("get_line", &rs2::firmware_log_parsed_message::line, "Get line ")
        .def("get_timestamp", &rs2::firmware_log_parsed_message::timestamp, "Get timestamp ")
        .def("get_sequence_id", &rs2::firmware_log_parsed_message::sequence_id, "Get sequence id");

    // rs2::firmware_logger
    py::class_<rs2::firmware_logger, rs2::device, py_holder<rs2::firmware_logger>> firmware_logger(m, "firmware_logger");
    firmware_logger.def(py::init<rs2::device>(), "device"_a)
        .def("create_message", &rs2::firmware_logger::create_message, "Create FW Log")
        .def("create_parsed_message", &rs2::firmware_logger::create_parsed_message, "Create FW Parsed Log")
        .def("start_collecting", &rs2::firmware_logger::start_collecting, "Start collecting FW logs")
        .def("stop_collecting", &rs2::firmware_logger::stop_collecting, "Stop collecting FW logs")
        .def("get_number_of_fw_logs", &rs2::firmware_logger::get_number_of_fw_logs, "Get Number of Fw Logs Polled From Device")
        .def("get_firmware_log", &rs2::firmware_logger::get_firmware_log, "Get FW Log", "msg"_a)
        .def("get_flash_log", &rs2::firmware_logger::get_flash_log, "Get Flash Log", "msg"_a)
        .def("init_parser", &rs2::firmware_logger::init_parser, "Initialize Parser with content of xml file", "xml_content"_a)
        .def("parse_log", &rs2::firmware_logger::parse_log, "Parse Fw Log ", "msg"_a, "parsed_msg"_a);

    // rs2::terminal_parser
    py::class_<rs2::terminal_parser> terminal_parser(m, "terminal_parser");
    terminal_parser.def(py::init<const std::string&>(), "xml_content"_a) 
        .def("parse_command", &rs2::terminal_parser::parse_command, "Parse Command ", "cmd"_a)
        .def("parse_response", &rs2::terminal_parser::parse_response, "Parse Response ", "cmd"_a, "response"_a);

    /** end rs_internal.hpp **/
}
