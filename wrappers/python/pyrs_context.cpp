/* License: Apache 2.0. See LICENSE file in root directory.
Copyright(c) 2017 RealSense, Inc. All Rights Reserved. */

#include "pyrealsense2.h"
#include <librealsense2/hpp/rs_context.hpp>

void init_context(py::module &m) {
        /* rs2_context.hpp */
    py::class_<rs2::event_information> event_information(m, "event_information"); // No docstring in C++
    event_information.def("was_removed", &rs2::event_information::was_removed, "Check if "
                          "a specific device was disconnected.", "dev"_a)
        .def("was_added", &rs2::event_information::was_added, "Check if "
             "a specific device was added.", "dev"_a)
        .def("get_new_devices", &rs2::event_information::get_new_devices, "Returns a "
             "list of all newly connected devices");

    // Not binding devices_changed_callback, templated

    py::class_< rs2::context >( m, "context", "Librealsense context class. Includes realsense API version." )
        .def( py::init< char const * >(), py::arg( "json-settings" ) = nullptr )
        .def( py::init<>( []( rsutils::json const & j ) { return rs2::context( j.dump() ); } ), py::arg( "json-settings" ) )
        .def("query_devices", (rs2::device_list(rs2::context::*)() const) &rs2::context::query_devices, "Create a static"
             " snapshot of all connected devices at the time of the call.")
        .def( "query_devices", ( rs2::device_list( rs2::context::* )(int) const ) & rs2::context::query_devices, "Create a static"
              " snapshot of all connected devices of specific product line at the time of the call." )
        .def_property_readonly("devices", (rs2::device_list(rs2::context::*)() const) &rs2::context::query_devices,
                               "A static snapshot of all connected devices at time of access. Identical to calling query_devices.")
        .def("query_all_sensors", &rs2::context::query_all_sensors, "Generate a flat list of "
             "all available sensors from all RealSense devices.")
        .def_property_readonly("sensors", &rs2::context::query_all_sensors, "A flat list of "
                               "all available sensors from all RealSense devices. Identical to calling query_all_sensors.")
        .def("get_sensor_parent", &rs2::context::get_sensor_parent, "s"_a) // no docstring in C++
        .def("set_devices_changed_callback", [](rs2::context& self, std::function<void(rs2::event_information)> &callback) {
            self.set_devices_changed_callback(callback);
        }, "Register devices changed callback.", "callback"_a)
        .def("load_device", &rs2::context::load_device, "Creates a devices from a RealSense file.\n"
             "On successful load, the device will be appended to the context and a devices_changed event triggered.",
             "filename"_a)
        .def("unload_device", &rs2::context::unload_device, "filename"_a) // No docstring in C++
        .def("unload_tracking_module", &rs2::context::unload_tracking_module) // No docstring in C++
        .def("convert_bag_to_db3",
             (void (rs2::context::*)(const std::string&, const std::string&)) &rs2::context::convert_bag_to_db3,
             "Convert a legacy ROS1 .bag recording to a ROS2 .db3 file.",
             "input"_a, "output"_a);

    // rs2::device_hub
    py::class_<rs2::device_hub>(m, "device_hub",
        "Encapsulates connect/disconnect handling and waiting for devices.")
        .def(py::init<rs2::context>(),
            py::arg("ctx"),
            py::keep_alive<1, 2>(),        // keep ctx alive as long as hub lives
            "Create a device_hub bound to the given context.")
        .def("wait_for_device",
            &rs2::device_hub::wait_for_device,
            "If a device is connected return it, otherwise wait until one connects.")
        .def("is_connected",
            &rs2::device_hub::is_connected,
            py::arg("device"),
            "Return True if the given device is still connected.");
    /** end rs_context.hpp **/
}
