/* License: Apache 2.0. See LICENSE file in root directory.
Copyright(c) 2017 RealSense, Inc. All Rights Reserved. */

#include "pyrealsense2.h"
#include <librealsense2/hpp/rs_internal.hpp>
#include <librealsense2/hpp/rs_device.hpp>
#include <librealsense2/hpp/rs_safety_sensor.hpp>
#include <librealsense2/hpp/rs_record_playback.hpp> // for downcasts
#include <common/metadata-helper.h>

#include <iostream>

void init_device(py::module &m) {
    /** rs_device.hpp **/
    // rs2::device destructor calls functions that might wait for Python callbacks to finish (e.g. dispatcher::stop)
    // Those callbacks need to hold the GIL so the py_holder releases our hold of the GIL to let them run.
    // Not releasing the GIL might lead to a deadlock (in certain scenario/timing).
    // Note - device inheritors need to use py_holder as well.
    py::class_<rs2::device, py_holder<rs2::device>> device(m, "device"); // No docstring in C++
    device.def("query_sensors", &rs2::device::query_sensors, "Returns the list of adjacent devices, "
               "sharing the same physical parent composite device.")
        .def_property_readonly("sensors", &rs2::device::query_sensors, "List of adjacent devices, "
                               "sharing the same physical parent composite device. Identical to calling query_sensors.")
        .def("first_depth_sensor", [](rs2::device& self) { return self.first<rs2::depth_sensor>(); }) // No docstring in C++
        .def("first_roi_sensor", [](rs2::device& self) { return self.first<rs2::roi_sensor>(); }) // No docstring in C++
        .def("first_pose_sensor", [](rs2::device& self) { return self.first<rs2::pose_sensor>(); }) // No docstring in C++
        .def("first_color_sensor", [](rs2::device& self) { return self.first<rs2::color_sensor>(); }) // No docstring in C++
        .def("first_motion_sensor", [](rs2::device& self) { return self.first<rs2::motion_sensor>(); }) // No docstring in C++
        .def("first_fisheye_sensor", [](rs2::device& self) { return self.first<rs2::fisheye_sensor>(); }) // No docstring in C++
        .def("first_safety_sensor", [](rs2::device& self) { return self.first<rs2::safety_sensor>(); }) // No docstring in C++
        .def("supports", &rs2::device::supports, "Check if specific camera info is supported.", "info"_a)
        .def("get_info", &rs2::device::get_info, "Retrieve camera specific information, "
             "like versions of various internal components", "info"_a)
        .def("hardware_reset", &rs2::device::hardware_reset, "Send hardware reset request to the device",
             py::call_guard<py::gil_scoped_release>())
        .def(py::init<>())
        .def("__nonzero__", &rs2::device::operator bool) // Called to implement truth value testing in Python 2
        .def("__bool__", &rs2::device::operator bool) // Called to implement truth value testing in Python 3
        .def( "is_connected", &rs2::device::is_connected )
        .def("is_in_recovery_mode", &rs2::device::is_in_recovery_mode)
        .def(BIND_DOWNCAST(device, debug_protocol))
        .def(BIND_DOWNCAST(device, playback))
        .def(BIND_DOWNCAST(device, recorder))
        .def(BIND_DOWNCAST(device, updatable))
        .def(BIND_DOWNCAST(device, update_device))
        .def(BIND_DOWNCAST(device, auto_calibrated_device))
        .def(BIND_DOWNCAST(device, device_calibration))
        .def(BIND_DOWNCAST(device, calibration_change_device))
        .def(BIND_DOWNCAST(device, firmware_logger))
        .def("__repr__", [](const rs2::device &self) {
            std::ostringstream ss;
            auto name = self.get_info( RS2_CAMERA_INFO_NAME );
            if( 0 == strncmp( name, "Intel RealSense ", 16 ) )
                name += 16;
            ss << "<" SNAME ".device: " << name;
            if (self.supports(RS2_CAMERA_INFO_SERIAL_NUMBER))
                ss << " (S/N: " << self.get_info(RS2_CAMERA_INFO_SERIAL_NUMBER);
            else
                ss << " (FW update id: " << self.get_info(RS2_CAMERA_INFO_FIRMWARE_UPDATE_ID);
            if (self.supports(RS2_CAMERA_INFO_FIRMWARE_VERSION))
                ss << " FW: " << self.get_info(RS2_CAMERA_INFO_FIRMWARE_VERSION);
            if (self.supports(RS2_CAMERA_INFO_SMCU_FW_VERSION))
                ss << " SMCU: " << self.get_info(RS2_CAMERA_INFO_SMCU_FW_VERSION);
            if (self.supports(RS2_CAMERA_INFO_MIPI_DRIVER_VERSION))
                ss << " MIPI DRIVER: " << self.get_info(RS2_CAMERA_INFO_MIPI_DRIVER_VERSION);
            if( self.supports( RS2_CAMERA_INFO_CAMERA_LOCKED )
                && strcmp( "YES", self.get_info( RS2_CAMERA_INFO_CAMERA_LOCKED ) ) )
                ss << " UNLOCKED";
            if (self.supports(RS2_CAMERA_INFO_CONNECTION_TYPE))
            {
                auto connection_type = self.get_info(RS2_CAMERA_INFO_CONNECTION_TYPE);
                ss << " on ";
                ss << connection_type;
                if (strcmp(connection_type, "USB") == 0)
                    if (self.supports(RS2_CAMERA_INFO_USB_TYPE_DESCRIPTOR))
                        ss << self.get_info(RS2_CAMERA_INFO_USB_TYPE_DESCRIPTOR);
            }
            
            else if( self.supports( RS2_CAMERA_INFO_PHYSICAL_PORT ) )
                ss << "  @ " << self.get_info( RS2_CAMERA_INFO_PHYSICAL_PORT );
            ss << ")>";
            return ss.str();
        })
        .def("is_metadata_enabled", [](const rs2::device& self) -> bool{
            auto depth_sens = self.first< rs2::depth_sensor >();

            if (!depth_sens.supports(RS2_CAMERA_INFO_PHYSICAL_PORT))
            {
                throw std::runtime_error("Device does not support checking metadata with this API");
            }
            std::string id = depth_sens.get_info(RS2_CAMERA_INFO_PHYSICAL_PORT);

            return rs2::metadata_helper::instance().is_enabled(id);
        });

    // not binding update_progress_callback, templated

    py::class_<rs2::updatable, rs2::device, py_holder<rs2::updatable>> updatable(m, "updatable"); // No docstring in C++
    updatable.def(py::init<rs2::device>())
        .def("enter_update_state", &rs2::updatable::enter_update_state, "Move the device to update state, this will cause the updatable device to disconnect and reconnect as an update device.", py::call_guard<py::gil_scoped_release>())
        .def("create_flash_backup", (std::vector<uint8_t>(rs2::updatable::*)() const) &rs2::updatable::create_flash_backup,
             "Create backup of camera flash memory. Such backup does not constitute valid firmware image, and cannot be "
             "loaded back to the device, but it does contain all calibration and device information.", py::call_guard<py::gil_scoped_release>())
        .def("create_flash_backup", [](rs2::updatable& self, std::function<void(float)> f) { return self.create_flash_backup(std::move(f)); },
             "Create backup of camera flash memory. Such backup does not constitute valid firmware image, and cannot be "
             "loaded back to the device, but it does contain all calibration and device information.",
             "callback"_a, py::call_guard<py::gil_scoped_release>())
        .def("update_unsigned", (void(rs2::updatable::*)(const std::vector<uint8_t>&, int) const) &rs2::updatable::update_unsigned,
             "Update an updatable device to the provided unsigned firmware. This call is executed on the caller's thread.", "fw_image"_a,
             "update_mode"_a = RS2_UNSIGNED_UPDATE_MODE_UPDATE, py::call_guard<py::gil_scoped_release>())
        .def("update_unsigned", [](rs2::updatable& self, const std::vector<uint8_t>& fw_image, std::function<void(float)> f, int update_mode) { return self.update_unsigned(fw_image, std::move(f), update_mode); },
             "Update an updatable device to the provided unsigned firmware. This call is executed on the caller's thread and provides progress notifications via the callback.",
             "fw_image"_a, "callback"_a, "update_mode"_a = RS2_UNSIGNED_UPDATE_MODE_UPDATE, py::call_guard<py::gil_scoped_release>())
        .def("check_firmware_compatibility", &rs2::updatable::check_firmware_compatibility, "Check firmware compatibility with device. "
            "This method should be called before burning a signed firmware.", "image"_a);

    py::class_<rs2::update_device, rs2::device, py_holder<rs2::update_device>> update_device(m, "update_device");
    update_device.def(py::init<rs2::device>())
        .def("update", [](rs2::update_device& self, const std::vector<uint8_t>& fw_image) { return self.update(fw_image); },
             "Update an updatable device to the provided firmware. This call is executed on the caller's thread.", "fw_image"_a, py::call_guard<py::gil_scoped_release>())
        .def("update", [](rs2::update_device& self, const std::vector<uint8_t>& fw_image, std::function<void(float)> f) { return self.update(fw_image, std::move(f)); },
             "Update an updatable device to the provided firmware. This call is executed on the caller's thread and provides progress notifications via the callback.",
             "fw_image"_a, "callback"_a, py::call_guard<py::gil_scoped_release>());

    py::class_<rs2::auto_calibrated_device, rs2::device, py_holder<rs2::auto_calibrated_device>> auto_calibrated_device(m, "auto_calibrated_device");
    auto_calibrated_device.def(py::init<rs2::device>(), "device"_a)
        .def("write_calibration", &rs2::auto_calibrated_device::write_calibration, "Write calibration that was set by set_calibration_table to device's EEPROM.", py::call_guard<py::gil_scoped_release>())
        .def("run_on_chip_calibration", [](rs2::auto_calibrated_device& self, std::string json_content, int timeout_ms)
        { 
            float health;
            rs2::calibration_table table = self.run_on_chip_calibration(json_content, &health, timeout_ms);
            return std::make_tuple(table, std::make_tuple(health, 0.0));
        },"This will improve the depth noise (plane fit RMS). This call is executed on the caller's thread.","json_content"_a, "timeout_ms"_a, py::call_guard<py::gil_scoped_release>())
        .def("run_on_chip_calibration", [](rs2::auto_calibrated_device& self, std::string json_content, std::function<void(float)> f, int timeout_ms)
        {
            float health;
            rs2::calibration_table table = self.run_on_chip_calibration(json_content, &health, std::move(f), timeout_ms);
            return std::make_tuple(table, std::make_tuple(health, 0.0));
        },"This will improve the depth noise (plane fit RMS). This call is executed on the caller's thread and provides progress notifications via the callback.", "json_content"_a, "callback"_a, "timeout_ms"_a, py::call_guard<py::gil_scoped_release>())
        .def("run_tare_calibration", [](const rs2::auto_calibrated_device& self, float ground_truth_mm, std::string json_content, int timeout_ms)
        {
            float health[] = { 0,0 };
            rs2::calibration_table table = self.run_tare_calibration(ground_truth_mm, json_content, health, timeout_ms);
            return std::make_tuple(table, std::make_tuple(health[0], health[1]));
        }, "This will adjust camera absolute distance to flat target. This call is executed on the caller's thread.", "ground_truth_mm"_a, "json_content"_a, "timeout_ms"_a, py::call_guard<py::gil_scoped_release>())
        .def("run_tare_calibration", [](const rs2::auto_calibrated_device& self, float ground_truth_mm, std::string json_content, std::function<void(float)> callback, int timeout_ms)
        {
            float health[] = { 0,0 };
            rs2::calibration_table table = self.run_tare_calibration(ground_truth_mm, json_content, health, std::move(callback), timeout_ms);
            return std::make_tuple(table, std::make_tuple(health[0], health[1]));
        }, "This will adjust camera absolute distance to flat target. This call is executed on the caller's thread and it supports progress notifications via the callback.", "ground_truth_mm"_a, "json_content"_a, "callback"_a, "timeout_ms"_a, py::call_guard<py::gil_scoped_release>())
        .def("process_calibration_frame", [](const rs2::auto_calibrated_device& self, rs2::frame frame, int timeout_ms)
            {
            float health[] = { 0,0 };
            rs2::calibration_table table = self.process_calibration_frame(frame, health, timeout_ms);
            return std::make_tuple(table, std::make_tuple(health[0], health[1]));
            }, "This will add a frame to the calibration process initiated by run_tare_calibration or run_on_chip_calibration as host assistant process. This call is executed on the caller's thread  and it supports progress notifications via the callback.", "frame"_a, "timeout_ms"_a, py::call_guard<py::gil_scoped_release>())
        .def("process_calibration_frame", [](const rs2::auto_calibrated_device& self, rs2::frame frame, std::function<void(float)> callback, int timeout_ms)
            {
            float health[] = { 0,0 };
            rs2::calibration_table table = self.process_calibration_frame(frame, health, std::move(callback), timeout_ms);
            return std::make_tuple(table, std::make_tuple(health[0], health[1]));
            }, "This will add a frame to the calibration process initiated by run_tare_calibration or run_on_chip_calibration as host assistant process. This call is executed on the caller's thread and it supports progress notifications via the callback.", "frame"_a, "callback"_a, "timeout_ms"_a, py::call_guard<py::gil_scoped_release>())
        .def("run_focal_length_calibration", [](const rs2::auto_calibrated_device& self, rs2::frame_queue left_queue, rs2::frame_queue right_queue,
                float target_width_mm, float target_heigth_mm, int adjust_both_sides)
        {
                float ratio{};
                float angle{};
            return std::make_tuple(self.run_focal_length_calibration(left_queue, right_queue, target_width_mm, target_heigth_mm, adjust_both_sides,
                &ratio, &angle), ratio, angle);
        }, "Run target-based focal length calibration. This call is executed on the caller's thread.",
            "left_queue"_a, "right_queue"_a, "target_width_mm"_a, "target_heigth_mm"_a, "adjust_both_sides"_a,py::call_guard<py::gil_scoped_release>())

        .def("run_focal_length_calibration", [](const rs2::auto_calibrated_device& self, rs2::frame_queue left_queue, rs2::frame_queue right_queue,
                float target_width_mm, float target_heigth_mm, int adjust_both_sides, std::function<void(float)> callback)
        {
                float ratio = 0.f;
                float angle = 0.f;
            return std::make_tuple(self.run_focal_length_calibration(left_queue, right_queue, target_width_mm, target_heigth_mm, adjust_both_sides,
                &ratio, &angle, std::move(callback)), ratio, angle);
        }, "Run target-based focal length calibration. This call is executed on the caller's thread and provides progress notifications via the callback.",
            "left_queue"_a, "right_queue"_a, "target_width_mm"_a, "target_heigth_mm"_a, "adjust_both_sides"_a, "callback"_a, py::call_guard<py::gil_scoped_release>())
        .def("run_uv_map_calibration", [](const rs2::auto_calibrated_device& self, rs2::frame_queue left, rs2::frame_queue color, rs2::frame_queue depth,
            int py_px_only)
        {
            constexpr int health_check_params = 4; // px, py, fx, fy for the calibration
            float health[health_check_params];
            return std::make_tuple(self.run_uv_map_calibration(left, color, depth, py_px_only, health, health_check_params), health);
        }, "Run target-based Depth-RGB UV-map calibraion. This call is executed on the caller's thread.",
            "left"_a, "color"_a, "depth"_a, "py_px_only"_a)
        .def("run_uv_map_calibration", [](const rs2::auto_calibrated_device& self, rs2::frame_queue left, rs2::frame_queue color, rs2::frame_queue depth,
                int py_px_only, std::function<void(float)> callback)
            {
                constexpr int health_check_params = 4; // px, py, fx, fy for the calibration
                float health[health_check_params];
                return std::make_tuple(self.run_uv_map_calibration(left, color, depth, py_px_only, health, health_check_params, std::move(callback)), health);
            }, "Run target-based Depth-RGB UV-map calibraion. This call is executed on the caller's thread and provides progress notifications via the callback.",
            "left"_a, "color"_a, "depth"_a, "py_px_only"_a, "callback"_a, py::call_guard<py::gil_scoped_release>())
        .def("calculate_target_z", [](const rs2::auto_calibrated_device& self, rs2::frame_queue queue1, rs2::frame_queue queue2, rs2::frame_queue queue3,
            float target_width_mm, float target_height_mm)
            {
                return self.calculate_target_z(queue1, queue2, queue3, target_width_mm, target_height_mm);
            }, "Calculate Z for calibration target - distance to the target's plane.",
            "queue1"_a, "queue2"_a, "queue3"_a, "target_width_mm"_a, "target_height_mm"_a, py::call_guard<py::gil_scoped_release>())
        .def("calculate_target_z", [](const rs2::auto_calibrated_device& self, rs2::frame_queue queue1, rs2::frame_queue queue2, rs2::frame_queue queue3,
            float target_width_mm, float target_height_mm, std::function<void(float)> callback)
            {
                return self.calculate_target_z(queue1, queue2, queue3, target_width_mm, target_height_mm, std::move(callback));
            }, "Calculate Z for calibration target - distance to the target's plane. This call is executed on the caller's thread and provides progress notifications via the callback.",
            "queue1"_a, "queue2"_a, "queue3"_a, "target_width_mm"_a, "target_height_mm"_a, "callback"_a, py::call_guard<py::gil_scoped_release>())
        .def("get_calibration_table", &rs2::auto_calibrated_device::get_calibration_table, "Read current calibration table from flash.")
        .def("set_calibration_table", &rs2::auto_calibrated_device::set_calibration_table, "Set current table to dynamic area.")
        .def("reset_to_factory_calibration", &rs2::auto_calibrated_device::reset_to_factory_calibration, "Reset device to factory calibration.")
        .def("get_calibration_config", &rs2::auto_calibrated_device::get_calibration_config, "Get Calibration Config Table", py::call_guard<py::gil_scoped_release>())
        .def("set_calibration_config", &rs2::auto_calibrated_device::set_calibration_config, "Set Calibration Config Table", "calibration_config_json_str"_a, py::call_guard<py::gil_scoped_release>());

    py::class_<rs2::device_calibration, rs2::device, py_holder<rs2::device_calibration>> device_calibration( m, "device_calibration" );
    device_calibration.def( py::init<rs2::device>(), "device"_a )
        .def( "trigger_device_calibration",
            []( rs2::device_calibration & self, rs2_calibration_type type )
            {
                py::gil_scoped_release gil;
                self.trigger_device_calibration( type );
            },
            "Trigger the given calibration, if available", "calibration_type"_a )
        .def( "register_calibration_change_callback",
            []( rs2::device_calibration& self, std::function<void( rs2_calibration_status )> callback )
            {
                self.register_calibration_change_callback(
                    [callback = std::move(callback)]( rs2_calibration_status status )
                    {
                        try
                        {
                            // "When calling a C++ function from Python, the GIL is always held"
                            // -- since we're not being called from Python but instead are calling it,
                            // we need to acquire it to not have issues with other threads...
                            py::gil_scoped_acquire gil;
                            callback( status );
                        }
                        catch( ... )
                        {
                            std::cerr << "?!?!?!!? exception in python register_calibration_change_callback ?!?!?!?!?" << std::endl;
                        }
                    } );
            },
            "Register (only once!) a callback that gets called for each change in calibration", "callback"_a );


    py::class_<rs2::calibration_change_device, rs2::device, py_holder<rs2::calibration_change_device>> calibration_change_device(m, "calibration_change_device");
    calibration_change_device.def(py::init<rs2::device>(), "device"_a)
        .def("register_calibration_change_callback",
            [](rs2::calibration_change_device& self, std::function<void(rs2_calibration_status)> callback)
            {
                self.register_calibration_change_callback(
                    [callback = std::move(callback)](rs2_calibration_status status)
                    {
                        try
                        {
                            // "When calling a C++ function from Python, the GIL is always held"
                            // -- since we're not being called from Python but instead are calling it,
                            // we need to acquire it to not have issues with other threads...
                            py::gil_scoped_acquire gil;
                            callback(status);
                        }
                        catch (...)
                        {
                            std::cerr << "?!?!?!!? exception in python register_calibration_change_callback ?!?!?!?!?" << std::endl;
                        }
                    });
            },
            "Register (only once!) a callback that gets called for each change in calibration", "callback"_a);

    py::class_<rs2::debug_protocol, py_holder<rs2::debug_protocol>> debug_protocol(m, "debug_protocol"); // No docstring in C++
    debug_protocol.def(py::init<rs2::device>())
        .def("build_command", &rs2::debug_protocol::build_command, "opcode"_a, "param1"_a = 0, 
            "param2"_a = 0, "param3"_a = 0, "param4"_a = 0, "data"_a = std::vector<uint8_t>()) 
        .def("send_and_receive_raw_data", &rs2::debug_protocol::send_and_receive_raw_data,
            "input"_a);  // No docstring in C++

    py::class_<rs2::device_list> device_list(m, "device_list"); // No docstring in C++
    device_list.def(py::init<>())
        .def("contains", &rs2::device_list::contains) // No docstring in C++
        .def("__getitem__", [](const rs2::device_list& self, size_t i) {
            if (i >= self.size())
                throw py::index_error();
            return self[uint32_t(i)];
        })
        .def("__len__", &rs2::device_list::size)
        .def("size", &rs2::device_list::size) // No docstring in C++
        .def("__iter__", [](const rs2::device_list& self) {
            return py::make_iterator(self.begin(), self.end());
        }, py::keep_alive<0, 1>())
        .def("__getitem__", [](const rs2::device_list& self, py::slice slice) {
            size_t start, stop, step, slicelength;
            if (!slice.compute(self.size(), &start, &stop, &step, &slicelength))
                throw py::error_already_set();
            auto *dlist = new std::vector<rs2::device>(slicelength);
            for (size_t i = 0; i < slicelength; ++i) {
                (*dlist)[i] = self[uint32_t(start)];
                start += step;
            }
            return dlist;
        })
        .def("front", &rs2::device_list::front) // No docstring in C++
        .def("back", &rs2::device_list::back); // No docstring in C++

    /** end rs_device.hpp **/
}
