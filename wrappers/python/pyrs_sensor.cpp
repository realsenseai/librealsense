/* License: Apache 2.0. See LICENSE file in root directory.
Copyright(c) 2017 RealSense, Inc. All Rights Reserved. */

#include "pyrealsense2.h"
#include <librealsense2/hpp/rs_sensor.hpp>
#include <librealsense2/hpp/rs_safety_sensor.hpp>
#include "max-usable-range-sensor.h"

void init_sensor(py::module &m) {
    /** rs_sensor.hpp **/
    py::class_<rs2::notification> notification(m, "notification"); // No docstring in C++
    notification.def(py::init<>())
        .def("get_category", &rs2::notification::get_category,
             "Retrieve the notification's category.")
        .def_property_readonly("category", &rs2::notification::get_category,
             "The notification's category. Identical to calling get_category.")
        .def("get_description", &rs2::notification::get_description,
             "Retrieve the notification's description.")
        .def_property_readonly("description", &rs2::notification::get_description,
             "The notification's description. Identical to calling get_description.")
        .def("get_timestamp", &rs2::notification::get_timestamp,
             "Retrieve the notification's arrival timestamp.")
        .def_property_readonly("timestamp", &rs2::notification::get_timestamp,
             "The notification's arrival timestamp. Identical to calling get_timestamp.")
        .def("get_severity", &rs2::notification::get_severity,
             "Retrieve the notification's severity.")
        .def_property_readonly("severity", &rs2::notification::get_severity,
             "The notification's severity. Identical to calling get_severity.")
        .def("get_serialized_data", &rs2::notification::get_severity,
             "Retrieve the notification's serialized data.")
        .def_property_readonly("serialized_data", &rs2::notification::get_serialized_data,
             "The notification's serialized data. Identical to calling get_serialized_data.")
        .def("__repr__", [](const rs2::notification &n) {
            return n.get_description();
        });

    // not binding notifications_callback, templated

    py::class_<rs2::sensor, rs2::options> sensor(m, "sensor"); // No docstring in C++
    sensor.def("open", (void (rs2::sensor::*)(const rs2::stream_profile&) const) &rs2::sensor::open,
               "Open sensor for exclusive access, by commiting to a configuration", "profile"_a, py::call_guard<py::gil_scoped_release>())
        .def("supports", (bool (rs2::sensor::*)(rs2_camera_info) const) &rs2::sensor::supports,
             "Check if specific camera info is supported.", "info")
        .def("supports", (bool (rs2::sensor::*)(rs2_option) const) &rs2::options::supports,
             "Check if specific camera info is supported.", "info")
        .def("get_info", &rs2::sensor::get_info, "Retrieve camera specific information, "
             "like versions of various internal components.", "info"_a)
        .def("set_notifications_callback", [](const rs2::sensor& self, std::function<void(rs2::notification)> callback) {
            self.set_notifications_callback(callback);
        }, "Register Notifications callback", "callback"_a)
        .def("open", (void (rs2::sensor::*)(const std::vector<rs2::stream_profile>&) const) &rs2::sensor::open,
             "Open sensor for exclusive access, by committing to a composite configuration, specifying one or "
             "more stream profiles.", "profiles"_a, py::call_guard<py::gil_scoped_release>())
        .def("close", &rs2::sensor::close, "Close sensor for exclusive access.", py::call_guard<py::gil_scoped_release>())
        .def("start", [](const rs2::sensor& self, std::function<void(rs2::frame)> callback) {
            self.start(callback);
        }, "Start passing frames into user provided callback.", "callback"_a,py::call_guard< py::gil_scoped_release >())
        .def("start", [](const rs2::sensor& self, rs2::syncer& syncer) {
            self.start(syncer);
        }, "Start passing frames into user provided syncer.", "syncer"_a, py::call_guard< py::gil_scoped_release >())
        .def("start", [](const rs2::sensor& self, rs2::frame_queue& queue) {
            self.start(queue);
        }, "start passing frames into specified frame_queue", "queue"_a, py::call_guard< py::gil_scoped_release >())
        .def("stop", &rs2::sensor::stop, "Stop streaming.", py::call_guard<py::gil_scoped_release>())
        .def("get_stream_profiles", &rs2::sensor::get_stream_profiles, "Retrieves the list of stream profiles supported by the sensor.")
        .def("get_active_streams", &rs2::sensor::get_active_streams, "Retrieves the list of stream profiles currently streaming on the sensor.")
        .def_property_readonly("profiles", &rs2::sensor::get_stream_profiles, "The list of stream profiles supported by the sensor. Identical to calling get_stream_profiles")
        .def("get_recommended_filters", &rs2::sensor::get_recommended_filters, "Return the recommended list of filters by the sensor.")
		.def("query_embedded_filters", &rs2::sensor::query_embedded_filters, "Return the list of embedded filters in the sensor.")
        .def("get_embedded_filter", [](const rs2::sensor& self, rs2_embedded_filter_type filter_type) {
            switch (filter_type) {
                case RS2_EMBEDDED_FILTER_TYPE_DECIMATION:
                    return rs2::embedded_filter(self.get_embedded_filter<rs2::embedded_decimation_filter>());
                case RS2_EMBEDDED_FILTER_TYPE_TEMPORAL:
                    return rs2::embedded_filter(self.get_embedded_filter<rs2::embedded_temporal_filter>());
                default:
                    throw std::runtime_error("Unsupported embedded filter type");
            }
        }, "Return the embedded filter in the sensor by filter type.", "filter_type"_a)
        .def("get_embedded_decimation_filter", &rs2::sensor::get_embedded_filter<rs2::embedded_decimation_filter>, 
             "Return the embedded decimation filter in the sensor.")
        .def("get_embedded_temporal_filter", &rs2::sensor::get_embedded_filter<rs2::embedded_temporal_filter>, 
             "Return the embedded temporal filter in the sensor.")
        .def(py::init<>())
        .def("__nonzero__", &rs2::sensor::operator bool) // Called to implement truth value testing in Python 2
        .def("__bool__", &rs2::sensor::operator bool)    // Called to implement truth value testing in Python 3
        .def(BIND_DOWNCAST(sensor, roi_sensor))
        .def(BIND_DOWNCAST(sensor, depth_sensor))
        .def(BIND_DOWNCAST(sensor, color_sensor))
        .def(BIND_DOWNCAST(sensor, motion_sensor))
        .def(BIND_DOWNCAST(sensor, fisheye_sensor))
        .def(BIND_DOWNCAST(sensor, safety_sensor))
        .def(BIND_DOWNCAST(sensor, pose_sensor))
        .def(BIND_DOWNCAST(sensor, wheel_odometer))
        .def(BIND_DOWNCAST(sensor, max_usable_range_sensor))
        .def(BIND_DOWNCAST(sensor, debug_stream_sensor))
        .def(BIND_DOWNCAST(sensor, inference_sensor))
        .def(BIND_DOWNCAST(sensor, object_detection_sensor))
        .def_property_readonly( "name",
                                []( const rs2::sensor & self ) {
                                    std::string name;
                                    if( self.supports( RS2_CAMERA_INFO_NAME ) )
                                        name = self.get_info( RS2_CAMERA_INFO_NAME );
                                    return name;
                                } )
        .def( "__repr__", []( const rs2::sensor & self ) {
            std::ostringstream ss;
            ss << "<" SNAME ".sensor";
            if( self.supports( RS2_CAMERA_INFO_NAME ) )
                ss << ": \"" << self.get_info( RS2_CAMERA_INFO_NAME ) << "\"";
            ss << ">";
            return ss.str();
        } )
        .def_static("from_frame", [](rs2::frame frame) {
            auto sptr = rs2::sensor_from_frame(frame);
            return *sptr;
            }, "frame"_a);

    py::class_<rs2::roi_sensor, rs2::sensor> roi_sensor(m, "roi_sensor"); // No docstring in C++
    roi_sensor.def(py::init<rs2::sensor>(), "sensor"_a)
        .def("set_region_of_interest", &rs2::roi_sensor::set_region_of_interest, "roi"_a) // No docstring in C++
        .def("get_region_of_interest", &rs2::roi_sensor::get_region_of_interest); // No docstring in C++

    py::class_<rs2::depth_sensor, rs2::sensor> depth_sensor(m, "depth_sensor"); // No docstring in C++
    depth_sensor.def(py::init<rs2::sensor>(), "sensor"_a)
        .def("get_depth_scale", &rs2::depth_sensor::get_depth_scale,
            "Retrieves mapping between the units of the depth image and meters.");

    py::class_<rs2::color_sensor, rs2::sensor> color_sensor(m, "color_sensor"); // No docstring in C++
    color_sensor.def(py::init<rs2::sensor>(), "sensor"_a);

    py::class_<rs2::motion_sensor, rs2::sensor> motion_sensor(m, "motion_sensor"); // No docstring in C++
    motion_sensor.def(py::init<rs2::sensor>(), "sensor"_a);

    py::class_<rs2::fisheye_sensor, rs2::sensor> fisheye_sensor(m, "fisheye_sensor"); // No docstring in C++
    fisheye_sensor.def(py::init<rs2::sensor>(), "sensor"_a);

    py::class_<rs2::inference_sensor, rs2::sensor> inference_sensor(m, "inference_sensor"); // No docstring in C++
    inference_sensor.def(py::init<rs2::sensor>(), "sensor"_a);

    py::class_<rs2::object_detection_sensor, rs2::inference_sensor> object_detection_sensor(m, "object_detection_sensor"); // No docstring in C++
    object_detection_sensor.def(py::init<rs2::sensor>(), "sensor"_a);

    py::class_<rs2::safety_sensor, rs2::sensor> safety_sensor(m, "safety_sensor"); // No docstring in C++
    safety_sensor.def(py::init<rs2::sensor>(), "sensor"_a)
        .def("get_safety_preset", &rs2::safety_sensor::get_safety_preset, "get safety preset at index", "index"_a, py::call_guard<py::gil_scoped_release>())
        .def("set_safety_preset", &rs2::safety_sensor::set_safety_preset, "set safety preset at index", "index"_a, "safety_preset"_a, py::call_guard<py::gil_scoped_release>())
        .def("get_safety_interface_config", 
            &rs2::safety_sensor::get_safety_interface_config,
            "get safety interface config", "calib_location"_a = RS2_CALIB_LOCATION_RAM, py::call_guard<py::gil_scoped_release>())
        .def("set_safety_interface_config", &rs2::safety_sensor::set_safety_interface_config,
            "set safety interface config", "safety_interface_config"_a, py::call_guard<py::gil_scoped_release>())
        .def("get_application_config", &rs2::safety_sensor::get_application_config, "get application config", py::call_guard<py::gil_scoped_release>())
        .def("set_application_config", &rs2::safety_sensor::set_application_config, "set application config", "application_config_json_str"_a, py::call_guard<py::gil_scoped_release>());

    py::class_<rs2::max_usable_range_sensor, rs2::sensor> mur_sensor(m, "max_usable_range_sensor");
    mur_sensor.def(py::init<rs2::sensor>(), "sensor"_a)
        .def("get_max_usable_depth_range",
            &rs2::max_usable_range_sensor::get_max_usable_depth_range,
            py::call_guard< py::gil_scoped_release >());
    
    py::class_< rs2::debug_stream_sensor, rs2::sensor > ds_sensor( m, "debug_stream_sensor" );
    ds_sensor.def( py::init< rs2::sensor >(), "sensor"_a )
        .def( "get_debug_stream_profiles",
               &rs2::debug_stream_sensor::get_debug_stream_profiles,
              py::call_guard< py::gil_scoped_release >() );

    // rs2::depth_stereo_sensor
    py::class_<rs2::depth_stereo_sensor, rs2::depth_sensor> depth_stereo_sensor(m, "depth_stereo_sensor"); // No docstring in C++
    depth_stereo_sensor.def(py::init<rs2::sensor>())
        .def("get_stereo_baseline", &rs2::depth_stereo_sensor::get_stereo_baseline, "Retrieve the stereoscopic baseline value from the sensor.");

    py::class_<rs2::pose_sensor, rs2::sensor> pose_sensor(m, "pose_sensor"); // No docstring in C++
    pose_sensor.def(py::init<rs2::sensor>(), "sensor"_a)
        .def("import_localization_map", &rs2::pose_sensor::import_localization_map,
             "Load relocalization map onto device. Only one relocalization map can be imported at a time; "
             "any previously existing map will be overwritten.\n"
             "The imported map exists simultaneously with the map created during the most recent tracking "
             "session after start(),"
             "and they are merged after the imported map is relocalized.\n"
             "This operation must be done before start().", "lmap_buf"_a)
        .def("export_localization_map", &rs2::pose_sensor::export_localization_map,
             "Get relocalization map that is currently on device, created and updated during most "
             "recent tracking session.\n"
             "Can be called before or after stop().")
        .def("set_static_node", &rs2::pose_sensor::set_static_node,
             "Creates a named virtual landmark in the current map, known as static node.\n"
             "The static node's pose is provided relative to the origin of current coordinate system of device poses.\n"
             "This function fails if the current tracker confidence is below 3 (high confidence).",
             "guid"_a, "pos"_a, "orient"_a)
        .def("get_static_node", [](const rs2::pose_sensor& self, const std::string& guid) {
            rs2_vector pos;
            rs2_quaternion orient;
            bool res = self.get_static_node(guid, pos, orient);
            return std::make_tuple(res, pos, orient);
            }, "Gets the current pose of a static node that was created in the current map or in an imported map.\n"
            "Static nodes of imported maps are available after relocalizing the imported map.\n"
                "The static node's pose is returned relative to the current origin of coordinates of device poses.\n"
                "Thus, poses of static nodes of an imported map are consistent with current device poses after relocalization.\n"
                "This function fails if the current tracker confidence is below 3 (high confidence).",
                "guid"_a)
        .def("remove_static_node", &rs2::pose_sensor::remove_static_node,
            "Removes a named virtual landmark in the current map, known as static node.\n"
            "guid"_a)
                .def("__nonzero__", &rs2::pose_sensor::operator bool) // Called to implement truth value testing in Python 2
                .def("__bool__", &rs2::pose_sensor::operator bool); // Called to implement truth value testing in Python 3

    py::class_<rs2::wheel_odometer, rs2::sensor> wheel_odometer(m, "wheel_odometer"); // No docstring in C++
    wheel_odometer.def(py::init<rs2::sensor>(), "sensor"_a)
        .def("load_wheel_odometery_config", &rs2::wheel_odometer::load_wheel_odometery_config,
            "Load Wheel odometer settings from host to device.", "odometry_config_buf"_a)
        .def("send_wheel_odometry", &rs2::wheel_odometer::send_wheel_odometry,
            "Send wheel odometry data for each individual sensor (wheel)",
            "wo_sensor_id"_a, "frame_num"_a, "translational_velocity"_a);
    /** end rs_sensor.hpp **/
}
