/* License: Apache 2.0. See LICENSE file in root directory.
Copyright(c) 2017 RealSense, Inc. All Rights Reserved. */

#include "pyrealsense2.h"
#include <librealsense2/hpp/rs_pipeline.hpp>

void init_pipeline(py::module &m) {
        /** rs_pipeline.hpp **/
    py::class_<rs2::pipeline_profile> pipeline_profile(m, "pipeline_profile", "The pipeline profile includes a device and a selection of active streams, with specific profiles.\n"
                                                       "The profile is a selection of the above under filters and conditions defined by the pipeline.\n"
                                                       "Streams may belong to more than one sensor of the device.");
    pipeline_profile.def(py::init<>())
        .def("get_streams", &rs2::pipeline_profile::get_streams, "Return the selected streams profiles, which are enabled in this profile.")
        .def("get_stream", &rs2::pipeline_profile::get_stream, "Return the stream profile that is enabled for the specified stream in this profile.", "stream_type"_a, "stream_index"_a = -1)
        .def("get_device", &rs2::pipeline_profile::get_device, "Retrieve the device used by the pipeline.\n"
             "The device class provides the application access to control camera additional settings - get device "
             "information, sensor options information, options value query and set, sensor specific extensions.\n"
             "Since the pipeline controls the device streams configuration, activation state and frames reading, "
             "calling the device API functions, which execute those operations, results in unexpected behavior.\n"
             "The pipeline streaming device is selected during pipeline start(). Devices of profiles, which are not returned "
             "by pipeline start() or get_active_profile(), are not guaranteed to be used by the pipeline.");

    // Workaround to allow python implicit conversion of pipeline to std::shared_ptr<rs2_pipeline>
    struct pipeline_wrapper // TODO: not sure this is necessary
    {
        std::shared_ptr<rs2_pipeline> _ptr;
    };
    py::class_<pipeline_wrapper>(m, "pipeline_wrapper")
        .def(py::init([](rs2::pipeline p) { return pipeline_wrapper{ p }; }));

    py::implicitly_convertible<rs2::pipeline, pipeline_wrapper>();
    
    py::class_<rs2::config> config(m, "config", "The config allows pipeline users to request filters for the pipeline streams and device selection and configuration.\n"
                                   "This is an optional step in pipeline creation, as the pipeline resolves its streaming device internally.\n"
                                   "Config provides its users a way to set the filters and test if there is no conflict with the pipeline requirements from the device.\n"
                                   "It also allows the user to find a matching device for the config filters and the pipeline, in order to select a device explicitly, "
                                   "and modify its controls before streaming starts.");
    config.def(py::init<>())
        // NOTE: do not overload functions with default arg, python cannot distinguish between enums
        // We specifically only allow "enable_stream" functions with unique arguments count (LibRS SDK allow overload as it's C/C++ types)
        .def("enable_stream", (void (rs2::config::*)(rs2_stream, int, int, int, rs2_format, int)) &rs2::config::enable_stream, "Enable a device stream explicitly, with selected stream parameters.\n"
             "The method allows the application to request a stream with specific configuration.\n"
             "If no stream is explicitly enabled, the pipeline configures the device and its streams according to the attached computer vision modules and processing blocks "
             "requirements, or default configuration for the first available device.\n"
             "The application can configure any of the input stream parameters according to its requirement, or set to 0 for don't care value.\n"
             "The config accumulates the application calls for enable configuration methods, until the configuration is applied.\n"
             "Multiple enable stream calls for the same stream override each other, and the last call is maintained.\n"
             "Upon calling resolve(), the config checks for conflicts between the application configuration requests and the attached computer vision "
             "modules and processing blocks requirements, and fails if conflicts are found.\n"
             "Before resolve() is called, no conflict check is done.", "stream_type"_a, "stream_index"_a, "width"_a, "height"_a, "format"_a, "framerate"_a)
        
        
        .def("enable_stream", []( rs2::config* c, rs2_stream s ) -> void { return c->enable_stream( s ); }, "Stream type only. Other parameters are resolved internally.", "stream_type"_a )
        .def("enable_stream", (void (rs2::config::*)(rs2_stream, int)) &rs2::config::enable_stream, "Stream type and possibly also stream index. Other parameters are resolved internally.", "stream_type"_a, "stream_index"_a)
        .def("enable_stream", (void (rs2::config::*)(rs2_stream, rs2_format, int))&rs2::config::enable_stream, "Stream type and format, and possibly frame rate. Other parameters are resolved internally.", "stream_type"_a, "format"_a, "framerate"_a)
        .def("enable_stream", (void (rs2::config::*)(rs2_stream, int, int, rs2_format, int)) &rs2::config::enable_stream, "Stream type and resolution, and possibly format and frame rate. Other parameters are resolved internally.", "stream_type"_a, "width"_a, "height"_a, "format"_a, "framerate"_a)
        .def("enable_stream", (void (rs2::config::*)(rs2_stream, int, rs2_format, int)) &rs2::config::enable_stream, "Stream type, index, and format, and possibly framerate. Other parameters are resolved internally.", "stream_type"_a, "stream_index"_a, "format"_a, "framerate"_a)
        .def("enable_all_streams", &rs2::config::enable_all_streams, "Enable all device streams explicitly.\n"
             "The conditions and behavior of this method are similar to those of enable_stream().\n"
             "This filter enables all raw streams of the selected device. The device is either selected explicitly by the application, "
             "or by the pipeline requirements or default. The list of streams is device dependent.")
        .def("enable_device", &rs2::config::enable_device, "Select a specific device explicitly by its serial number, to be used by the pipeline.\n"
             "The conditions and behavior of this method are similar to those of enable_stream().\n"
             "This method is required if the application needs to set device or sensor settings prior to pipeline streaming, "
             "to enforce the pipeline to use the configured device.", "serial"_a)
        .def("enable_device_from_file", &rs2::config::enable_device_from_file, "Select a recorded device from a file, to be used by the pipeline through playback.\n"
             "The device available streams are as recorded to the file, and resolve() considers only this device and configuration as available.\n"
             "This request cannot be used if enable_record_to_file() is called for the current config, and vice versa.", "file_name"_a, "repeat_playback"_a = true)
        .def("enable_record_to_file", &rs2::config::enable_record_to_file, "Requires that the resolved device would be recorded to file.\n"
             "This request cannot be used if enable_device_from_file() is called for the current config, and vice versa as available.", "file_name"_a)
        .def("disable_stream", &rs2::config::disable_stream, "Disable a device stream explicitly, to remove any requests on this stream profile.\n"
             "The stream can still be enabled due to pipeline computer vision module request. This call removes any filter on the stream configuration.", "stream"_a, "index"_a = -1)
        .def("disable_all_streams", &rs2::config::disable_all_streams, "Disable all device stream explicitly, to remove any requests on the streams profiles.\n"
             "The streams can still be enabled due to pipeline computer vision module request. This call removes any filter on the streams configuration.")
        .def("resolve", [](rs2::config* c, pipeline_wrapper pw) -> rs2::pipeline_profile { return c->resolve(pw._ptr); }, "Resolve the configuration filters, "
             "to find a matching device and streams profiles.\n"
             "The method resolves the user configuration filters for the device and streams, and combines them with the requirements of the computer vision modules "
             "and processing blocks attached to the pipeline. If there are no conflicts of requests, it looks for an available device, which can satisfy all requests, "
             "and selects the first matching streams configuration.\n"
             "In the absence of any request, the config object selects the first available device and the first color and depth streams configuration."
             "The pipeline profile selection during start() follows the same method. Thus, the selected profile is the same, if no change occurs to the available devices."
             "Resolving the pipeline configuration provides the application access to the pipeline selected device for advanced control."
             "The returned configuration is not applied to the device, so the application doesn't own the device sensors. However, the application can call enable_device(), "
             "to enforce the device returned by this method is selected by pipeline start(), and configure the device and sensors options or extensions before streaming starts.", "p"_a)
        .def("can_resolve", [](rs2::config* c, pipeline_wrapper pw) -> bool { return c->can_resolve(pw._ptr); }, "Check if the config can resolve the configuration filters, "
             "to find a matching device and streams profiles. The resolution conditions are as described in resolve().", "p"_a);
    
    py::class_<rs2::pipeline, py_holder<rs2::pipeline>> pipeline(m, "pipeline", "The pipeline simplifies the user interaction with the device and computer vision processing modules.\n"
                                       "The class abstracts the camera configuration and streaming, and the vision modules triggering and threading.\n"
                                       "It lets the application focus on the computer vision output of the modules, or the device output data.\n"
                                       "The pipeline can manage computer vision modules, which are implemented as a processing blocks.\n"
                                       "The pipeline is the consumer of the processing block interface, while the application consumes the computer vision interface.");
    pipeline  //
        .def( py::init<>(), "With a default context" )
        .def( py::init< rs2::context >(),
              "The caller can provide a context created by the application, usually for playback or testing purposes",
              "ctx"_a )
        // TODO: Streamline this wall of text
        .def("start", (rs2::pipeline_profile(rs2::pipeline::*)()) &rs2::pipeline::start, "Start the pipeline streaming with its default configuration.\n"
             "The pipeline streaming loop captures samples from the device, and delivers them to the attached computer vision modules and processing "
             "blocks, according to each module requirements and threading model.\n"
             "During the loop execution, the application can access the camera streams by calling wait_for_frames() or poll_for_frames().\n"
             "The streaming loop runs until the pipeline is stopped.\n"
             "Starting the pipeline is possible only when it is not started. If the pipeline was started, an exception is raised.\n")
        .def("start", (rs2::pipeline_profile(rs2::pipeline::*)(const rs2::config&)) &rs2::pipeline::start, "Start the pipeline streaming according to the configuraion.\n"
             "The pipeline streaming loop captures samples from the device, and delivers them to the attached computer vision modules and processing blocks, according to "
             "each module requirements and threading model.\n"
             "During the loop execution, the application can access the camera streams by calling wait_for_frames() or poll_for_frames().\n"
             "The streaming loop runs until the pipeline is stopped.\n"
             "Starting the pipeline is possible only when it is not started. If the pipeline was started, an exception is raised.\n"
             "The pipeline selects and activates the device upon start, according to configuration or a default configuration.\n"
             "When the rs2::config is provided to the method, the pipeline tries to activate the config resolve() result.\n"
             "If the application requests are conflicting with pipeline computer vision modules or no matching device is available on the platform, the method fails.\n"
             "Available configurations and devices may change between config resolve() call and pipeline start, in case devices are connected or disconnected, or another "
             "application acquires ownership of a device.", "config"_a)
        .def("start", [](rs2::pipeline& self, std::function<void(rs2::frame)> f) { return self.start(f); }, "Start the pipeline streaming with its default configuration.\n"
             "The pipeline captures samples from the device, and delivers them to the provided frame callback.\n"
             "Starting the pipeline is possible only when it is not started. If the pipeline was started, an exception is raised.\n"
             "When starting the pipeline with a callback both wait_for_frames() and poll_for_frames() will throw exception.", "callback"_a)
        .def("start", [](rs2::pipeline& self, const rs2::config& config, std::function<void(rs2::frame)> f) { return self.start(config, f); }, "Start the pipeline streaming according to the configuraion.\n"
             "The pipeline captures samples from the device, and delivers them to the provided frame callback.\n"
             "Starting the pipeline is possible only when it is not started. If the pipeline was started, an exception is raised.\n"
             "When starting the pipeline with a callback both wait_for_frames() and poll_for_frames() will throw exception.\n"
             "The pipeline selects and activates the device upon start, according to configuration or a default configuration.\n"
             "When the rs2::config is provided to the method, the pipeline tries to activate the config resolve() result.\n"
             "If the application requests are conflicting with pipeline computer vision modules or no matching device is available on the platform, the method fails.\n"
             "Available configurations and devices may change between config resolve() call and pipeline start, in case devices are connected or disconnected, "
             "or another application acquires ownership of a device.", "config"_a, "callback"_a)
        .def("start", [](rs2::pipeline& self, rs2::frame_queue& queue) { return self.start(queue); },"Start the pipeline streaming with its default configuration.\n"
             "The pipeline captures samples from the device, and delivers them to the provided frame queue.\n"
             "Starting the pipeline is possible only when it is not started. If the pipeline was started, an exception is raised.\n"
             "When starting the pipeline with a callback both wait_for_frames() and poll_for_frames() will throw exception.", "queue"_a)
        .def("start", [](rs2::pipeline& self, const rs2::config& config, rs2::frame_queue queue) { return self.start(config, queue); }, "Start the pipeline streaming according to the configuraion.\n"
            "The pipeline captures samples from the device, and delivers them to the provided frame queue.\n"
            "Starting the pipeline is possible only when it is not started. If the pipeline was started, an exception is raised.\n"
            "When starting the pipeline with a callback both wait_for_frames() and poll_for_frames() will throw exception.\n"
            "The pipeline selects and activates the device upon start, according to configuration or a default configuration.\n"
            "When the rs2::config is provided to the method, the pipeline tries to activate the config resolve() result.\n"
            "If the application requests are conflicting with pipeline computer vision modules or no matching device is available on the platform, the method fails.\n"
            "Available configurations and devices may change between config resolve() call and pipeline start, in case devices are connected or disconnected, "
            "or another application acquires ownership of a device.", "config"_a, "queue"_a)
        .def("stop", &rs2::pipeline::stop, "Stop the pipeline streaming.\n"
             "The pipeline stops delivering samples to the attached computer vision modules and processing blocks, stops the device streaming and releases "
             "the device resources used by the pipeline. It is the application's responsibility to release any frame reference it owns.\n"
             "The method takes effect only after start() was called, otherwise an exception is raised.", py::call_guard<py::gil_scoped_release>())
        .def("wait_for_frames", &rs2::pipeline::wait_for_frames, "Wait until a new set of frames becomes available.\n"
             "The frames set includes time-synchronized frames of each enabled stream in the pipeline.\n"
             "In case of different frame rates of the streams, the frames set include a matching frame of the slow stream, which may have been included in previous frames set.\n"
             "The method blocks the calling thread, and fetches the latest unread frames set.\n"
             "Device frames, which were produced while the function wasn't called, are dropped. To avoid frame drops, this method should be called as fast as the device frame rate.\n"
             "The application can maintain the frames handles to defer processing. However, if the application maintains too long history, "
             "the device may lack memory resources to produce new frames, and the following call to this method shall fail to retrieve new "
             "frames, until resources become available.", "timeout_ms"_a = 5000, py::call_guard<py::gil_scoped_release>())
        .def("poll_for_frames", [](const rs2::pipeline &self) {
                rs2::frameset frames;
                self.poll_for_frames(&frames);
                return frames;
            }, "Check if a new set of frames is available and retrieve the latest undelivered set.\n"
             "The frames set includes time-synchronized frames of each enabled stream in the pipeline.\n"
             "The method returns without blocking the calling thread, with status of new frames available or not.\n"
             "If available, it fetches the latest frames set.\n"
             "Device frames, which were produced while the function wasn't called, are dropped.\n"
             "To avoid frame drops, this method should be called as fast as the device frame rate.\n"
             "The application can maintain the frames handles to defer processing. However, if the application maintains too long "
             "history, the device may lack memory resources to produce new frames, and the following calls to this method shall "
             "return no new frames, until resources become available.")
        .def("try_wait_for_frames", [](const rs2::pipeline &self, unsigned int timeout_ms) {
            rs2::frameset fs;
            auto success = self.try_wait_for_frames(&fs, timeout_ms);
            return std::make_tuple(success, fs);
        }, "timeout_ms"_a = 5000, py::call_guard<py::gil_scoped_release>())
        .def("get_active_profile", &rs2::pipeline::get_active_profile) // No docstring in C++
        .def( "set_device", &rs2::pipeline::set_device,
              "The function is used to assign the device, useful when the user wish to set controls that cannot be set while streaming. ",
              "device"_a );
    /** end rs_pipeline.hpp **/
}
