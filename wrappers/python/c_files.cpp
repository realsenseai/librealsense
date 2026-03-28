/* License: Apache 2.0. See LICENSE file in root directory.
Copyright(c) 2017 RealSense, Inc. All Rights Reserved. */

#include "pyrealsense2.h"
#include <librealsense2/rs.h>
#include <iomanip>

std::string make_pythonic_str(std::string str)
{
    std::transform(begin(str), end(str), begin(str), ::tolower); //Make lower case
    std::replace(begin(str), end(str), ' ', '_'); //replace spaces with underscore
    if (str == "6dof") //Currently this is the only enum that starts with a digit
    {
        return "six_dof";
    }
    return str;
}


void init_c_files(py::module &m) {
       /** Binding of rs2_* enums */
    BIND_ENUM(m, rs2_notification_category, RS2_NOTIFICATION_CATEGORY_COUNT, "Category of the librealsense notification.")
    // rs2_exception_type
    BIND_ENUM(m, rs2_distortion, RS2_DISTORTION_COUNT, "Distortion model: defines how pixel coordinates should be mapped to sensor coordinates.")
    BIND_ENUM(m, rs2_log_severity, RS2_LOG_SEVERITY_COUNT, "Severity of the librealsense logger.")
    BIND_ENUM(m, rs2_thread_category, RS2_THREAD_CATEGORY_COUNT, "Category of a library-created thread, for use with the thread-start callback.")
    BIND_ENUM(m, rs2_extension, RS2_EXTENSION_COUNT, "Specifies advanced interfaces (capabilities) objects may implement.")
    BIND_ENUM(m, rs2_matchers, RS2_MATCHER_COUNT, "Specifies types of different matchers.")
    BIND_ENUM(m, rs2_camera_info, RS2_CAMERA_INFO_COUNT, "This information is mainly available for camera debug and troubleshooting and should not be used in applications.")
    BIND_ENUM(m, rs2_stream, RS2_STREAM_COUNT, "Streams are different types of data provided by RealSense devices.")
    BIND_ENUM(m, rs2_format, RS2_FORMAT_COUNT, "A stream's format identifies how binary data is encoded within a frame.")
    BIND_ENUM(m, rs2_timestamp_domain, RS2_TIMESTAMP_DOMAIN_COUNT, "Specifies the clock in relation to which the frame timestamp was measured.")
    BIND_ENUM(m, rs2_frame_metadata_value, RS2_FRAME_METADATA_COUNT, "Per-Frame-Metadata is the set of read-only properties that might be exposed for each individual frame.")
    BIND_ENUM(m, rs2_calib_target_type, RS2_CALIB_TARGET_COUNT, "Calibration target type.")

    BIND_ENUM(m, rs2_option, RS2_OPTION_COUNT+1, "Defines general configuration controls. These can generally be mapped to camera UVC controls, and can be set / queried at any time unless stated otherwise.")
    // Without __repr__ and __str__, we get the default 'enum_base' showing '???'
    py_rs2_option.attr( "__repr__" ) = py::cpp_function(
        []( const py::object & arg ) -> py::str
        {
            auto type = py::type::handle_of( arg );
            py::object type_name = type.attr( "__name__" );
            py::int_ arg_int( arg );
            py::str enum_name( rs2_option_to_string( rs2_option( (int) arg_int ) ) );
            return py::str( "<{} {} '{}'>" ).format( std::move( type_name ), arg_int, enum_name );
        },
        py::name( "__repr__" ),
        py::is_method( py_rs2_option ) );
    py_rs2_option.attr( "__str__" ) = py::cpp_function(
        []( const py::object & arg ) -> py::str {
            py::int_ arg_int( arg );
            return rs2_option_to_string( rs2_option( (int) arg_int ) );
        },
        py::name( "name" ),
        py::is_method( py_rs2_option ) );
    // Force binding of deprecated (renamed) options that we still want to expose for backwards compatibility
    py_rs2_option.value("ambient_light", RS2_OPTION_AMBIENT_LIGHT);
    py_rs2_option.value( "lld_temperature", RS2_OPTION_LLD_TEMPERATURE );

    m.def( "option_from_string", &rs2_option_from_string );

    BIND_ENUM(m, rs2_option_type, RS2_OPTION_TYPE_COUNT, "The different types option values can take on")
    BIND_ENUM(m, rs2_l500_visual_preset, RS2_L500_VISUAL_PRESET_COUNT, "For L500 devices: provides optimized settings (presets) for specific types of usage.")
    BIND_ENUM(m, rs2_rs400_visual_preset, RS2_RS400_VISUAL_PRESET_COUNT, "For D400 devices: provides optimized settings (presets) for specific types of usage.")
    BIND_ENUM(m, rs2_playback_status, RS2_PLAYBACK_STATUS_COUNT, "") // No docsDtring in C++
    BIND_ENUM(m, rs2_calibration_type, RS2_CALIBRATION_TYPE_COUNT, "Calibration type for use in device_calibration")
    BIND_ENUM_CUSTOM(m, rs2_calibration_status, RS2_CALIBRATION_STATUS_FIRST, RS2_CALIBRATION_STATUS_LAST, "Calibration callback status for use in device_calibration.trigger_device_calibration")
    BIND_ENUM(m, rs2_d500_intercam_sync_mode, RS2_D500_INTERCAM_SYNC_COUNT, "For D500: intercamera synchronization mode")

    /** rs_types.h **/
    py::class_<rs2_intrinsics> intrinsics(m, "intrinsics", "Video stream intrinsics.");
    intrinsics.def(py::init<>())
        .def_readwrite("width", &rs2_intrinsics::width, "Width of the image in pixels")
        .def_readwrite("height", &rs2_intrinsics::height, "Height of the image in pixels")
        .def_readwrite("ppx", &rs2_intrinsics::ppx, "Horizontal coordinate of the principal point of the image, as a pixel offset from the left edge")
        .def_readwrite("ppy", &rs2_intrinsics::ppy, "Vertical coordinate of the principal point of the image, as a pixel offset from the top edge")
        .def_readwrite("fx", &rs2_intrinsics::fx, "Focal length of the image plane, as a multiple of pixel width")
        .def_readwrite("fy", &rs2_intrinsics::fy, "Focal length of the image plane, as a multiple of pixel height")
        .def_readwrite("model", &rs2_intrinsics::model, "Distortion model of the image")
        .def_property(BIND_RAW_ARRAY_PROPERTY(rs2_intrinsics, coeffs, float, 5), "Distortion coefficients")
        .def( "__repr__",
              []( const rs2_intrinsics & i )
              {
                  std::ostringstream ss;
                  ss << "[ " << i.width << "x" << i.height
                     << "  p[" << i.ppx << " " << i.ppy << "]"
                     << "  f[" << i.fx << " " << i.fy << "]"
                     << "  " << rs2_distortion_to_string( i.model ) << " [" << i.coeffs[0] << " " << i.coeffs[1] << " "
                     << i.coeffs[2] << " " << i.coeffs[3] << " " << i.coeffs[4] << "] ]";
                  return ss.str();
              } );

    py::class_<rs2_motion_device_intrinsic> motion_device_intrinsic(m, "motion_device_intrinsic", "Motion device intrinsics: scale, bias, and variances.");
    motion_device_intrinsic.def(py::init<>())
        .def_property(BIND_RAW_2D_ARRAY_PROPERTY(rs2_motion_device_intrinsic, data, float, 3, 4), "3x4 matrix with 3x3 scale and cross axis and 3x1 biases")
        .def_property(BIND_RAW_ARRAY_PROPERTY(rs2_motion_device_intrinsic, noise_variances, float, 3), "Variance of noise for X, Y, and Z axis")
        .def_property(BIND_RAW_ARRAY_PROPERTY(rs2_motion_device_intrinsic, bias_variances, float, 3), "Variance of bias for X, Y, and Z axis")
        .def("__repr__", [](const rs2_motion_device_intrinsic& self) {
            std::stringstream ss;
            ss << "data: " << matrix_to_string(self.data) << ", ";
            ss << "noise_variances: " << array_to_string(self.noise_variances) << ", ";
            ss << "bias_variances: " << array_to_string(self.bias_variances);
            return ss.str();
        });

    // rs2_vertex
    // rs2_pixel
    
    py::class_<rs2_vector> vector(m, "vector", "3D vector in Euclidean coordinate space.");
    vector.def(py::init<>())
        .def_readwrite("x", &rs2_vector::x)
        .def_readwrite("y", &rs2_vector::y)
        .def_readwrite("z", &rs2_vector::z)
        .def("__repr__", [](const rs2_vector& self) {
            std::stringstream ss;
            ss << "x: " << self.x << ", ";
            ss << "y: " << self.y << ", ";
            ss << "z: " << self.z;
            return ss.str();
        });

    py::class_<rs2_quaternion> quaternion(m, "quaternion", "Quaternion used to represent rotation.");
    quaternion.def(py::init<>())
        .def_readwrite("x", &rs2_quaternion::x)
        .def_readwrite("y", &rs2_quaternion::y)
        .def_readwrite("z", &rs2_quaternion::z)
        .def_readwrite("w", &rs2_quaternion::w)
        .def("__repr__", [](const rs2_quaternion& self) {
            std::stringstream ss;
            ss << "x: " << self.x << ", ";
            ss << "y: " << self.y << ", ";
            ss << "z: " << self.z << ", ";
            ss << "w: " << self.w;
            return ss.str();
        });

    py::class_< rs2_combined_motion > combined_motion( m, "combined_motion", "IMU combined GYRO & ACCEL data" );
    combined_motion.def( py::init<>() )
        .def_property(
            "angular_velocity",
            []( rs2_combined_motion const & self )
            {
                return rs2_vector{ (float)self.angular_velocity.x,
                                   (float)self.angular_velocity.y,
                                   (float)self.angular_velocity.z };
            },
            []( rs2_combined_motion & self, rs2_vector const & v ) {
                self.angular_velocity = { v.x, v.y, v.z };
            } )
        .def_property(
            "linear_acceleration",
            []( rs2_combined_motion const & self )
            {
                return rs2_vector{ (float)self.linear_acceleration.x,
                                   (float)self.linear_acceleration.y,
                                   (float)self.linear_acceleration.z };
            },
            []( rs2_combined_motion & self, rs2_vector const & v ) {
                self.linear_acceleration = { v.x, v.y, v.z };
            } )
        .def( "__repr__",
              []( const rs2_combined_motion & self )
              {
                  std::ostringstream ss;
                  ss << "gyro[" << self.angular_velocity.x << "," << self.angular_velocity.y << ","
                      << self.angular_velocity.z << "] accel[" << self.linear_acceleration.x << ","
                      << self.linear_acceleration.y << "," << self.linear_acceleration.z << "]";
                  return ss.str();
              } );

    py::class_<rs2_pose> pose(m, "pose"); // No docstring in C++
    pose.def(py::init<>())
        .def_readwrite("translation", &rs2_pose::translation, "X, Y, Z values of translation, in meters (relative to initial position)")
        .def_readwrite("velocity", &rs2_pose::velocity, "X, Y, Z values of velocity, in meters/sec")
        .def_readwrite("acceleration", &rs2_pose::acceleration, "X, Y, Z values of acceleration, in meters/sec^2")
        .def_readwrite("rotation", &rs2_pose::rotation, "Qi, Qj, Qk, Qr components of rotation as represented in quaternion rotation (relative to initial position)")
        .def_readwrite("angular_velocity", &rs2_pose::angular_velocity, "X, Y, Z values of angular velocity, in radians/sec")
        .def_readwrite("angular_acceleration", &rs2_pose::angular_acceleration, "X, Y, Z values of angular acceleration, in radians/sec^2")
        .def_readwrite("tracker_confidence", &rs2_pose::tracker_confidence, "Pose confidence 0x0 - Failed, 0x1 - Low, 0x2 - Medium, 0x3 - High")
        .def_readwrite("mapper_confidence", &rs2_pose::mapper_confidence, "Pose map confidence 0x0 - Failed, 0x1 - Low, 0x2 - Medium, 0x3 - High");
    /** end rs_types.h **/

    /** rs_sensor.h **/
    py::class_<rs2_extrinsics> extrinsics(m, "extrinsics", "Cross-stream extrinsics: encodes the topology describing how the different devices are oriented.");
    extrinsics.def(py::init<>())
        .def_property(BIND_RAW_ARRAY_PROPERTY(rs2_extrinsics, rotation, float, 9), "Column - major 3x3 rotation matrix")
        .def_property(BIND_RAW_ARRAY_PROPERTY(rs2_extrinsics, translation, float, 3), "Three-element translation vector, in meters")
        .def("__repr__", [](const rs2_extrinsics &e) {
            std::stringstream ss;
            ss << "rotation: " << array_to_string(e.rotation);
            ss << "\ntranslation: " << array_to_string(e.translation);
            return ss.str();
        });
    /** end rs_sensor.h **/
}
