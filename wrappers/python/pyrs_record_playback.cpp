/* License: Apache 2.0. See LICENSE file in root directory.
Copyright(c) 2017 RealSense, Inc. All Rights Reserved. */

#include "pyrealsense2.h"
#include <librealsense2/hpp/rs_record_playback.hpp>

void init_record_playback(py::module &m) {
    /** rs_record_playback.hpp **/
// Not binding status_changed_callback, templated

    py::class_<rs2::playback, rs2::device, py_holder<rs2::playback>> playback(m, "playback"); // No docstring in C++
    playback.def(py::init<rs2::device>(), "device"_a)
        .def("pause", &rs2::playback::pause, "Pauses the playback. Calling pause() in \"Paused\" status does nothing. If "
             "pause() is called while playback status is \"Playing\" or \"Stopped\", the playback will not play until resume() is called.")
        .def("resume", &rs2::playback::resume, "Un-pauses the playback. Calling resume() while playback status is \"Playing\" or \"Stopped\" does nothing.")
        .def("file_name", &rs2::playback::file_name, "The name of the playback file.")
        .def("get_position", &rs2::playback::get_position, "Retrieves the current position of the playback in the file in terms of time. Units are expressed in nanoseconds.")
        .def("get_duration", &rs2::playback::get_duration, "Retrieves the total duration of the file.")
        .def("seek", &rs2::playback::seek, "Sets the playback to a specified time point of the played data.", "time"_a)
        .def("is_real_time", &rs2::playback::is_real_time, "Indicates if playback is in real time mode or non real time.")
        .def("set_real_time", &rs2::playback::set_real_time, "Set the playback to work in real time or non real time. In real time mode, playback will "
             "play the same way the file was recorded. If the application takes too long to handle the callback, frames may be dropped. In non real time "
             "mode, playback will wait for each callback to finish handling the data before reading the next frame. In this mode no frames will be dropped, "
             "and the application controls the framerate of playback via callback duration.", "real_time"_a)
        // set_playback_speed?
        .def("set_status_changed_callback", [](rs2::playback& self, std::function<void(rs2_playback_status)> callback) {
            self.set_status_changed_callback(std::move(callback));
        }, "Register to receive callback from playback device upon its status changes. Callbacks are invoked from the reading thread, "
           "and as such any heavy processing in the callback handler will affect the reading thread and may cause frame drops/high latency.", "callback"_a,
           py::call_guard<py::gil_scoped_release>())
        .def("current_status", &rs2::playback::current_status, "Returns the current state of the playback device");
    // Stop?

    py::class_<rs2::recorder, rs2::device, py_holder<rs2::recorder>> recorder(m, "recorder", "Records the given device and saves it to the given file as rosbag format.");
    recorder.def(py::init<const std::string&, rs2::device>())
        .def(py::init<const std::string&, rs2::device, bool>())
        .def("pause", &rs2::recorder::pause, "Pause the recording device without stopping the actual device from streaming.")
        .def("resume", &rs2::recorder::resume, "Unpauses the recording device, making it resume recording.");
    // filename?
    /** end rs_record_playback.hpp **/
}
