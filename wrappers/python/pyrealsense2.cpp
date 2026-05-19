/* License: Apache 2.0. See LICENSE file in root directory.
Copyright(c) 2017 RealSense, Inc. All Rights Reserved. */

#include "pyrealsense2.h"
#include <librealsense2/rs.hpp>
#include <librealsense2/hpp/rs_export.hpp>
#include <src/types.h>

PYBIND11_MODULE(NAME, m) {
    m.doc() = R"pbdoc(
        Librealsense Python Bindings
        ==============================
        Library for accessing RealSense cameras
    )pbdoc";
    m.attr("__version__") = RS2_API_VERSION_STR;
    m.attr("__full_version__") = RS2_API_FULL_VERSION_STR;

    init_c_files(m);
    init_types(m);
    init_frame(m);
    init_options(m);
    init_processing(m);
    init_sensor(m);
    init_device(m);
    init_record_playback(m);
    init_context(m);
    init_pipeline(m);
    init_internal(m); // must be run after init_frame()
    init_export(m);
    init_advanced_mode(m);
    init_serializable_device(m);
    init_util(m);
    init_eth_config(m);
    
    /** rs_export.hpp **/
    py::class_<rs2::save_to_ply, rs2::filter>(m, "save_to_ply")
        .def(py::init<std::string, rs2::pointcloud>(), "filename"_a = "RealSense Pointcloud ", "pc"_a = rs2::pointcloud())
        .def_property_readonly_static("option_ignore_color", [](py::object) { return rs2::save_to_ply::OPTION_IGNORE_COLOR; })
        .def_property_readonly_static("option_ply_mesh", [](py::object) { return rs2::save_to_ply::OPTION_PLY_MESH; })
        .def_property_readonly_static("option_ply_binary", [](py::object) { return rs2::save_to_ply::OPTION_PLY_BINARY; })
        .def_property_readonly_static("option_ply_normals", [](py::object) { return rs2::save_to_ply::OPTION_PLY_NORMALS; })
        .def_property_readonly_static("option_ply_threshold", [](py::object) { return rs2::save_to_ply::OPTION_PLY_THRESHOLD; });

    m.def("log_to_console", &rs2::log_to_console, "min_severity"_a);
    m.def("log_to_file", &rs2::log_to_file, "min_severity"_a, "file_path"_a);
    m.def("reset_logger", &rs2::reset_logger);
    m.def("enable_rolling_log_file", &rs2::enable_rolling_log_file, "max_size"_a);

    // Access to log_message is only from a callback (see log_to_callback below) and so already
    // should have the GIL acquired
    py::class_<rs2::log_message> log_message(m, "log_message");
    log_message.def("line_number", &rs2::log_message::line_number)
        .def("filename", &rs2::log_message::filename)
        .def("raw", &rs2::log_message::raw)
        .def("full", &rs2::log_message::full)
        .def("__str__", &rs2::log_message::raw)
        .def("__repr__", &rs2::log_message::full);

    // We want to enable Python callbacks for logging, but need to be careful:
    // The machanism used by librealsense keeps a pointer to an object that is then released
    // on destruction/exit. Usually this works fine, except that here, with Python and its GIL,
    // Pybind tries to acquire the GIL when the thread state is no longer valid and we get
    // into an infinite wait.
    // This is how the code would look like if we didn't had this issue
#if 0
    m.def( "log_to_callback",
           []( rs2_log_severity min_severity, std::function< void( rs2_log_severity, rs2::log_message ) > callback )
           {
               py::gil_scoped_release gil;
               rs2::log_to_callback( min_severity,
                                     [callback]( rs2_log_severity severity, rs2::log_message const & msg ) noexcept
                                     {
                                         py::gil_scoped_acquire gil;
                                         callback( severity, msg );
                                     } );
           } );
#else
    // Instead, as a workaround, we override the usual mechanism to intentionally not free up
    // if we see the Python thread state isn't valid (see release() below):
    class py_log_callback : public rs2::log_callback
    {
        typedef rs2::log_callback super;

    public:
        py_log_callback( log_fn && on_log )
            : super( std::move( on_log ) )
        {
        }

        void on_log( rs2_log_severity severity, rs2_log_message const & msg ) noexcept override
        {
            try
            {
                // We're not being called from Python but instead are calling it,
                // we need to acquire it to not have issues with other threads...
                py::gil_scoped_acquire gil;
                super::on_log( severity, msg );
            }
            catch( std::exception const & e )
            {
                std::cerr << "EXCEPTION in " SNAME ".log_to_callback: " << e.what() << std::endl;
            }
            catch( ... )
            {
                std::cerr << "UNKNOWN EXCEPTION in " SNAME ".log_to_callback" << std::endl;
            }
        }

        void release() override
        {
            // When we exit() python, we get here with an invalid thread-state and the delete
            // locks the thread indefinitely!
            if( PyGILState_GetThisThreadState() )
                super::release();
        }
    };
    m.def( "log_to_callback",
           []( rs2_log_severity min_severity, py_log_callback::log_fn callback )
           {
               rs2_error * e = nullptr;
               py::gil_scoped_release gil;
               rs2_log_to_callback_cpp( min_severity, new py_log_callback( std::move( callback ) ), &e );
               rs2::error::handle( e );
           } );
#endif

    // A call to rs.log() will cause a callback to get called! We should already own the GIL, but
    // release it just in case to let others do their thing...
    m.def("log", &rs2::log, "severity"_a, "message"_a, py::call_guard<py::gil_scoped_release>());
}
