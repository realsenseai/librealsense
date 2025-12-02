/* License: Apache 2.0. See LICENSE file in root directory.
Copyright(c) 2017 RealSense, Inc. All Rights Reserved. */

#include "pyrealsense2.h"
#include <librealsense2/rs.hpp>

#include <rsutils/string/from.h>
#include <src/image.cpp>  // bad idea? for get_image_bpp


namespace {


    std::ostream & operator<<( std::ostream & ss, const rs2::frame & self )
    {
        ss << rs2_format_to_string( self.get_profile().format() );
        ss << " #" << self.get_frame_number();
        ss << " @" << rsutils::string::from( self.get_timestamp() );
        return ss;
    }


}


void init_frame(py::module &m) {
    py::class_<BufData> BufData_py(m, "BufData", py::buffer_protocol());
    BufData_py.def_buffer([](BufData& self)
    { return py::buffer_info(
        self._ptr,
        self._itemsize,
        self._format,
        self._ndim,
        self._shape,
        self._strides); }
    );

    // Helper function for supporting python's buffer protocol
    auto get_frame_data = [](const rs2::frame& self) ->  BufData
    {
        if (auto vf = self.as<rs2::video_frame>()) {
            std::map<size_t, std::string> bytes_per_pixel_to_format = { { 1, std::string("@B") },{ 2, std::string("@H") },{ 3, std::string("@I") },{ 4, std::string("@I") } };
            switch (vf.get_profile().format()) {
            case RS2_FORMAT_RGB8: case RS2_FORMAT_BGR8:
                return BufData(const_cast<void*>(vf.get_data()), 1, bytes_per_pixel_to_format[1], 3,
                    { static_cast<size_t>(vf.get_height()), static_cast<size_t>(vf.get_width()), 3 },
                    { static_cast<size_t>(vf.get_stride_in_bytes()), static_cast<size_t>(vf.get_bytes_per_pixel()), 1 });
                break;
            case RS2_FORMAT_RGBA8: case RS2_FORMAT_BGRA8:
                return BufData(const_cast<void*>(vf.get_data()), 1, bytes_per_pixel_to_format[1], 3,
                    { static_cast<size_t>(vf.get_height()), static_cast<size_t>(vf.get_width()), 4 },
                    { static_cast<size_t>(vf.get_stride_in_bytes()), static_cast<size_t>(vf.get_bytes_per_pixel()), 1 });
                break;
            default:
                return BufData(const_cast<void*>(vf.get_data()), static_cast<size_t>(vf.get_bytes_per_pixel()), bytes_per_pixel_to_format[vf.get_bytes_per_pixel()], 2,
                    { static_cast<size_t>(vf.get_height()), static_cast<size_t>(vf.get_width()) },
                    { static_cast<size_t>(vf.get_stride_in_bytes()), static_cast<size_t>(vf.get_bytes_per_pixel()) });
            }
        }
        else
            return BufData(const_cast<void*>(self.get_data()), 1, std::string("@B"), self.get_data_size()); };
    
    /* rs_frame.hpp */
    py::class_<rs2::stream_profile> stream_profile(m, "stream_profile", "Stores details about the profile of a stream.");
    stream_profile.def(py::init<>())
        .def("stream_index", &rs2::stream_profile::stream_index, "The stream's index")
        .def("stream_type", &rs2::stream_profile::stream_type, "The stream's type")
        .def("format", &rs2::stream_profile::format, "The stream's format")
        .def("bytes_per_pixel", []( rs2::stream_profile const & self ) { return librealsense::get_image_bpp( self.format() ) / 8; } )
        .def("fps", &rs2::stream_profile::fps, "The streams framerate")
        .def("unique_id", &rs2::stream_profile::unique_id, "Unique index assigned whent the stream was created")
        .def("clone", &rs2::stream_profile::clone, "Clone the current profile and change the type, index and format to input parameters", "type"_a, "index"_a, "format"_a)
        .def(BIND_DOWNCAST(stream_profile, stream_profile))
        .def(BIND_DOWNCAST(stream_profile, video_stream_profile))
        .def(BIND_DOWNCAST(stream_profile, motion_stream_profile))
        .def(BIND_DOWNCAST(stream_profile, pose_stream_profile))
        .def("stream_name", &rs2::stream_profile::stream_name, "The stream's human-readable name.")
        .def("is_default", &rs2::stream_profile::is_default, "Checks if the stream profile is marked/assigned as default, "
             "meaning that the profile will be selected when the user requests stream configuration using wildcards.")
        .def("__nonzero__", &rs2::stream_profile::operator bool, "Checks if the profile is valid") // Called to implement truth value testing in Python 2
        .def("__bool__", &rs2::stream_profile::operator bool, "Checks if the profile is valid") // Called to implement truth value testing in Python 3

        .def("get_extrinsics_to", &rs2::stream_profile::get_extrinsics_to, "Get the extrinsic transformation between two profiles (representing physical sensors)", "to"_a)
        .def("register_extrinsics_to", &rs2::stream_profile::register_extrinsics_to, "Assign extrinsic transformation parameters "
             "to a specific profile (sensor). The extrinsic information is generally available as part of the camera calibration, "
             "and librealsense is responsible for retrieving and assigning these parameters where appropriate. This specific function "
             "is intended for synthetic/mock-up (software) devices for which the parameters are produced and injected by the user.", "to"_a, "extrinsics"_a)
        .def("__repr__", [](const rs2::stream_profile& self) {
            std::stringstream ss;
            if (auto vf = self.as<rs2::video_stream_profile>())
            {
                ss << "<" SNAME ".[video_]stream_profile: "
                    << vf.stream_type() << "(" << vf.stream_index() << ") " << vf.width()
                    << "x" << vf.height() << " @ " << vf.fps() << "fps "
                    << vf.format() << ">";
            }
            else
            {
                ss << "<" SNAME ".stream_profile: " << self.stream_type() << "(" << self.stream_index()
                    << ") @ " << self.fps() << "fps " << self.format() << ">";
            }
            return ss.str();
        });

    py::class_<rs2::video_stream_profile, rs2::stream_profile> video_stream_profile(m, "video_stream_profile", "Stream profile instance which contains additional video attributes.");
    video_stream_profile.def(py::init<const rs2::stream_profile&>(), "sp"_a)
        .def("width", &rs2::video_stream_profile::width) // No docstring in C++
        .def("height", &rs2::video_stream_profile::height) // No docstring in C++
        .def("get_intrinsics", &rs2::video_stream_profile::get_intrinsics, "Get stream profile instrinsics attributes.")
        .def_property_readonly("intrinsics", &rs2::video_stream_profile::get_intrinsics, "Stream profile instrinsics attributes. Identical to calling get_intrinsics.")
        .def("__repr__", [](const rs2::video_stream_profile& self) {
            std::stringstream ss;
            ss << "<" SNAME ".video_stream_profile: "
                << self.stream_type() << "(" << self.stream_index() << ") " << self.width()
                << "x" << self.height() << " @ " << self.fps() << "fps "
                << self.format() << ">";
            return ss.str();
        });

    py::class_<rs2::motion_stream_profile, rs2::stream_profile> motion_stream_profile(m, "motion_stream_profile", "Stream profile instance which contains IMU-specific intrinsics.");
    motion_stream_profile.def(py::init<const rs2::stream_profile&>(), "sp"_a)
        .def("get_motion_intrinsics", &rs2::motion_stream_profile::get_motion_intrinsics, "Returns scale and bias of a motion stream.");

    py::class_<rs2::pose_stream_profile, rs2::stream_profile> pose_stream_profile(m, "pose_stream_profile", "Stream profile instance with an explicit pose extension type.");
    pose_stream_profile.def(py::init<const rs2::stream_profile&>(), "sp"_a);

    py::class_<rs2::filter_interface> filter_interface(m, "filter_interface", "Interface for frame filtering functionality");
    filter_interface.def("process", &rs2::filter_interface::process, "frame"_a); // No docstring in C++

    py::class_<rs2::frame> frame(m, "frame", "Base class for multiple frame extensions");
    frame.def(py::init<>())
        // .def(py::self = py::self) // can't overload assignment in python
        .def(py::init<rs2::frame>())
        .def("swap", &rs2::frame::swap, "Swap the internal frame handle with the one in parameter", "other"_a)
        .def("__nonzero__", &rs2::frame::operator bool, "check if internal frame handle is valid") // Called to implement truth value testing in Python 2
        .def("__bool__", &rs2::frame::operator bool, "check if internal frame handle is valid") // Called to implement truth value testing in Python 3
        .def("get_timestamp", &rs2::frame::get_timestamp, "Retrieve the time at which the frame was captured")
        .def_property_readonly("timestamp", &rs2::frame::get_timestamp, "Time at which the frame was captured. Identical to calling get_timestamp.")
        .def("get_frame_timestamp_domain", &rs2::frame::get_frame_timestamp_domain, "Retrieve the timestamp domain.")
        .def_property_readonly("frame_timestamp_domain", &rs2::frame::get_frame_timestamp_domain, "The timestamp domain. Identical to calling get_frame_timestamp_domain.")
        .def("get_frame_metadata", &rs2::frame::get_frame_metadata, "Retrieve the current value of a single frame_metadata.", "frame_metadata"_a)
        .def("supports_frame_metadata", &rs2::frame::supports_frame_metadata, "Determine if the device allows a specific metadata to be queried.", "frame_metadata"_a)
        .def("get_frame_number", &rs2::frame::get_frame_number, "Retrieve the frame number.")
        .def_property_readonly("frame_number", &rs2::frame::get_frame_number, "The frame number. Identical to calling get_frame_number.")
        .def("get_data_size", &rs2::frame::get_data_size, "Retrieve data size from frame handle.")
        .def("get_data", get_frame_data, "Retrieve data from the frame handle.", py::keep_alive<0, 1>())
        .def_property_readonly("data", get_frame_data, "Data from the frame handle. Identical to calling get_data.", py::keep_alive<0, 1>())
        .def("get_profile", &rs2::frame::get_profile, "Retrieve stream profile from frame handle.")
        .def_property_readonly("profile", &rs2::frame::get_profile, "Stream profile from frame handle. Identical to calling get_profile.")
        .def("keep", &rs2::frame::keep, "Keep the frame, otherwise if no refernce to the frame, the frame will be released.")
        .def(BIND_DOWNCAST(frame, frame))
        .def(BIND_DOWNCAST(frame, points))
        .def(BIND_DOWNCAST(frame, labeled_points))
        .def(BIND_DOWNCAST(frame, frameset))
        .def(BIND_DOWNCAST(frame, video_frame))
        .def(BIND_DOWNCAST(frame, depth_frame))
        .def(BIND_DOWNCAST(frame, motion_frame))
        .def(BIND_DOWNCAST(frame, pose_frame))
        // No apply_filter?
        .def( "__repr__", []( const rs2::frame &self )
        {
            std::ostringstream ss;
            ss << "<" << SNAME << ".frame";
            if( ! self )
            {
                ss << " NULL";
            }
            else
            {
                if( auto fs = self.as< rs2::frameset >() )
                {
                    ss << "set";
                    for( auto sf : fs )
                        ss << "  " << sf;
                }
                else
                {
                    ss << " " << self;
                }
            }
            ss << ">";
            return ss.str();
        });

    py::class_<rs2::video_frame, rs2::frame> video_frame(m, "video_frame", "Extends the frame class with additional video related attributes and functions.");
    video_frame.def(py::init<rs2::frame>())
        .def("get_width", &rs2::video_frame::get_width, "Returns image width in pixels.")
        .def_property_readonly("width", &rs2::video_frame::get_width, "Image width in pixels. Identical to calling get_width.")
        .def("get_height", &rs2::video_frame::get_height, "Returns image height in pixels.")
        .def_property_readonly("height", &rs2::video_frame::get_height, "Image height in pixels. Identical to calling get_height.")
        .def("get_stride_in_bytes", &rs2::video_frame::get_stride_in_bytes, "Retrieve frame stride, meaning the actual line width in memory in bytes (not the logical image width).")
        .def_property_readonly("stride_in_bytes", &rs2::video_frame::get_stride_in_bytes, "Frame stride, meaning the actual line width in memory in bytes (not the logical image width). Identical to calling get_stride_in_bytes.")
        .def("get_bits_per_pixel", &rs2::video_frame::get_bits_per_pixel, "Retrieve bits per pixel.")
        .def_property_readonly("bits_per_pixel", &rs2::video_frame::get_bits_per_pixel, "Bits per pixel. Identical to calling get_bits_per_pixel.")
        .def("get_bytes_per_pixel", &rs2::video_frame::get_bytes_per_pixel, "Retrieve bytes per pixel.")
        .def_property_readonly("bytes_per_pixel", &rs2::video_frame::get_bytes_per_pixel, "Bytes per pixel. Identical to calling get_bytes_per_pixel.")
        .def("extract_target_dimensions", [](const rs2::video_frame& self, rs2_calib_target_type target_type)->std::vector<float>
        {
            std::vector<float> target_dims;
            if (target_type == RS2_CALIB_TARGET_RECT_GAUSSIAN_DOT_VERTICES)
            {
                target_dims.resize(4);
                self.extract_target_dimensions(RS2_CALIB_TARGET_RECT_GAUSSIAN_DOT_VERTICES, target_dims.data(), 4);
            }
            else if (target_type == RS2_CALIB_TARGET_ROI_RECT_GAUSSIAN_DOT_VERTICES)
            {
                target_dims.resize(4);
                self.extract_target_dimensions(RS2_CALIB_TARGET_ROI_RECT_GAUSSIAN_DOT_VERTICES, target_dims.data(), 4);
            }
            else if (target_type == RS2_CALIB_TARGET_POS_GAUSSIAN_DOT_VERTICES)
            {
                target_dims.resize(8);
                self.extract_target_dimensions(RS2_CALIB_TARGET_POS_GAUSSIAN_DOT_VERTICES, target_dims.data(), 8);
            }
            return target_dims;
        }, "This will calculate the four target dimenson size(s) in millimeter on the specific target.");

    py::class_<rs2::vertex> vertex(m, "vertex"); // No docstring in C++
    vertex.def_readwrite("x", &rs2::vertex::x)
        .def_readwrite("y", &rs2::vertex::y)
        .def_readwrite("z", &rs2::vertex::z)
        .def(py::init([]() { return rs2::vertex{}; }))
        .def(py::init([](float x, float y, float z) { return rs2::vertex{ x,y,z }; }))
        .def("__repr__", [](const rs2::vertex& v) {
            std::ostringstream oss;
            oss << v.x << ", " << v.y << ", " << v.z;
            return oss.str();
        });

    py::class_<rs2::texture_coordinate> texture_coordinate(m, "texture_coordinate"); // No docstring in C++
    texture_coordinate.def_readwrite("u", &rs2::texture_coordinate::u)
        .def_readwrite("v", &rs2::texture_coordinate::v)
        .def(py::init([]() { return rs2::texture_coordinate{}; }))
        .def(py::init([](float u, float v) { return rs2::texture_coordinate{ u, v }; }))
        .def("__repr__", [](const rs2::texture_coordinate& t) {
            std::ostringstream oss;
            oss << t.u << ", " << t.v;
            return oss.str();
        });

    py::class_<rs2::points, rs2::frame> points(m, "points", "Extends the frame class with additional point cloud related attributes and functions.");
    points.def(py::init<>())
        .def(py::init<rs2::frame>())
        .def("get_vertices", [](rs2::points& self, int dims) {
            auto verts = const_cast<rs2::vertex*>(self.get_vertices());
            auto profile = self.get_profile().as<rs2::video_stream_profile>();
            size_t h = profile.height(), w = profile.width();
            switch (dims) {
            case 1:
                return BufData(verts, sizeof(rs2::vertex), "@fff", self.size());
            case 2:
                return BufData(verts, sizeof(float), "@f", 3, self.size());
            case 3:
                return BufData(verts, sizeof(float), "@f", 3, { h, w, 3 }, { w*3*sizeof(float), 3*sizeof(float), sizeof(float) });
            default:
                throw std::domain_error("dims arg only supports values of 1, 2 or 3");
            }
        }, "Retrieve the vertices of the point cloud", py::keep_alive<0, 1>(), "dims"_a=1)
        .def("get_texture_coordinates", [](rs2::points& self, int dims) {
            auto tex = const_cast<rs2::texture_coordinate*>(self.get_texture_coordinates());
            auto profile = self.get_profile().as<rs2::video_stream_profile>();
            size_t h = profile.height(), w = profile.width();
            switch (dims) {
            case 1:
                return BufData(tex, sizeof(rs2::texture_coordinate), "@ff", self.size());
            case 2:
                return BufData(tex, sizeof(float), "@f", 2, self.size());
            case 3:
                return BufData(tex, sizeof(float), "@f", 2, { h, w, 2 }, { w*2*sizeof(float), 2*sizeof(float), sizeof(float) });
            default:
                throw std::domain_error("dims arg only supports values of 1, 2 or 3");
            }
        }, "Retrieve the texture coordinates (uv map) for the point cloud", py::keep_alive<0, 1>(), "dims"_a=1)
        .def("export_to_ply", &rs2::points::export_to_ply, "Export the point cloud to a PLY file")
        .def("size", &rs2::points::size); // No docstring in C++

    py::class_<rs2::depth_frame, rs2::video_frame> depth_frame(m, "depth_frame", "Extends the video_frame class with additional depth related attributes and functions.");
    depth_frame.def(py::init<rs2::frame>())
        .def("get_distance", &rs2::depth_frame::get_distance, "x"_a, "y"_a, "Provide the depth in meters at the given pixel")
        .def("get_units", &rs2::depth_frame::get_units, "Provide the scaling factor to use when converting from get_data() units to meters");
    
    // rs2::disparity_frame
    py::class_<rs2::disparity_frame, rs2::depth_frame> disparity_frame(m, "disparity_frame", "Extends the depth_frame class with additional disparity related attributes and functions.");
    disparity_frame.def(py::init<rs2::frame>())
        .def("get_baseline", &rs2::disparity_frame::get_baseline, "Retrieve the distance between the two IR sensors.");

    py::class_<rs2::motion_frame, rs2::frame> motion_frame(m, "motion_frame", "Extends the frame class with additional motion related attributes and functions");
    motion_frame.def(py::init<rs2::frame>())
        .def("get_motion_data", &rs2::motion_frame::get_motion_data, "Retrieve motion data from a GYRO/ACCEL sensor")
        .def("get_combined_motion_data", &rs2::motion_frame::get_combined_motion_data, "Retrieve motion data from a MOTION sensor")
        .def_property_readonly("motion_data", &rs2::motion_frame::get_motion_data, "Motion data from IMU sensor. Identical to calling get_motion_data.");

    py::class_<rs2::pose_frame, rs2::frame> pose_frame(m, "pose_frame", "Extends the frame class with additional pose related attributes and functions.");
    pose_frame.def(py::init<rs2::frame>())
        .def("get_pose_data", &rs2::pose_frame::get_pose_data, "Retrieve the pose data from T2xx position tracking sensor.")
        .def_property_readonly("pose_data", &rs2::pose_frame::get_pose_data, "Pose data from T2xx position tracking sensor. Identical to calling get_pose_data.");

    py::class_<rs2::labeled_points, rs2::frame> labeled_points(m, "labeled_points", "Extends the frame class with additional labeled points related attributes and functions.");
    labeled_points.def(py::init<rs2::frame>())
        .def("get_vertices", [](rs2::labeled_points& self) {
        auto verts = const_cast<rs2::vertex*>(self.get_vertices());
        return BufData(verts, sizeof(rs2::vertex), "@fff", self.size());
            }, "Retrieve the vertices of the labeled point cloud", py::keep_alive<0, 1>())
        .def("get_labels", [](rs2::labeled_points& self) {
        auto labels = const_cast<uint8_t*>(self.get_labels());
        return BufData(labels, sizeof(uint8_t), "@B", self.size());
            }, "Retrieve the labels of the labeled point cloud", py::keep_alive<0, 1>() )
                .def( "get_width", &rs2::labeled_points::get_width, "Returns labeled point cloud width in pixels." )
                .def_property_readonly( "width", &rs2::labeled_points::get_width, "labeled point cloud width in pixels. Identical to calling get_width." )
                .def( "get_height", &rs2::labeled_points::get_height, "Returns labeled point cloud height in pixels." )
                .def_property_readonly( "height", &rs2::labeled_points::get_height, "labeled point cloud height in pixels. Identical to calling get_height." )
                .def( "get_bpp", &rs2::labeled_points::get_bits_per_pixel, "Returns labeled point cloud bpp (bits per pixel)." );

    // TODO: Deprecate composite_frame, replace with frameset
    py::class_<rs2::frameset, rs2::frame> frameset(m, "composite_frame", "Extends the frame class with additional frameset related attributes and functions");
    frameset.def(py::init<rs2::frame>())
        .def("first_or_default", &rs2::frameset::first_or_default, "Retrieve the first frame of a specific stream and optionally with a specific format. "
             "If no frame is found, return an empty frame instance.", "s"_a, "f"_a = RS2_FORMAT_ANY)
        .def("first", &rs2::frameset::first, "Retrieve the first frame of a specific stream type and optionally with a specific format. "
             "If no frame is found, an error will be thrown.", "s"_a, "f"_a = RS2_FORMAT_ANY)
        .def("size", &rs2::frameset::size, "Return the size of the frameset")
        .def("__len__", &rs2::frameset::size, "Return the size of the frameset")
        .def("foreach", [](const rs2::frameset& self, std::function<void(rs2::frame)> callable) {
            self.foreach_rs(callable);
        }, "Extract internal frame handles from the frameset and invoke the action function", "callable"_a)
        .def("__getitem__", &rs2::frameset::operator[])
        .def("get_depth_frame", &rs2::frameset::get_depth_frame, "Retrieve the first depth frame, if no frame is found, return an empty frame instance.")
        .def("get_color_frame", &rs2::frameset::get_color_frame, "Retrieve the first color frame, if no frame is found, search for the color frame from IR stream. "
             "If one still can't be found, return an empty frame instance.", "index"_a = 0)
        .def("get_infrared_frame", &rs2::frameset::get_infrared_frame, "Retrieve the first infrared frame, if no frame is "
             "found, return an empty frame instance.", "index"_a = 0)
        .def("get_fisheye_frame", &rs2::frameset::get_fisheye_frame, "Retrieve the fisheye monochrome video frame", "index"_a = 0)
        .def("get_pose_frame", &rs2::frameset::get_pose_frame, "Retrieve the pose frame", "index"_a = 0)
        .def("get_labeled_point_cloud_frame", &rs2::frameset::get_labeled_point_cloud_frame, "Retrieve the labeled point cloud frame, if no frame is found, return an empty frame instance.")
        .def("__iter__", [](rs2::frameset& self) {
            return py::make_iterator(self.begin(), self.end());
        }, py::keep_alive<0, 1>())
        .def("__getitem__", [](const rs2::frameset& self, py::slice slice) {
            size_t start, stop, step, slicelength;
            if (!slice.compute(self.size(), &start, &stop, &step, &slicelength))
                throw py::error_already_set();
            auto *flist = new std::vector<rs2::frame>(slicelength);
            for (size_t i = 0; i < slicelength; ++i) {
                (*flist)[i] = self[start];
                start += step;
            }
            return flist;
        });
    /** end rs_frame.hpp **/
}
