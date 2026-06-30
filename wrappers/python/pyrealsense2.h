/* License: Apache 2.0. See LICENSE file in root directory.
Copyright(c) 2017 RealSense, Inc. All Rights Reserved. */

#pragma once

#include <rsutils/py/pybind11.h>

#define NAME pyrealsense2
#define SNAME "pyrealsense2"

// GIL-releasing custom deleter for pybind11 holder types.
//
// When a Python reference to a librealsense wrapper (rs.device, rs.sensor, rs.debug_protocol, ...) is dropped,
// CPython calls tp_dealloc on the wrapper while the calling thread still holds the GIL.
// The default holder (std::unique_ptr<T>) runs the C++ destructor, which might wait for a Python callback on another thread
// (e.g. notifications_processor::~notifications_processor() ) that might need to re-acquire the GIL, causing a deadlock if
// we don't release the GIL before the destructor runs.
//
// This deleter releases the GIL before delete and re-acquires it after, so the destructor runs without the GIL.
// Any nested gil_scoped_acquire (e.g. pybind11's func_handle destructor freeing a stored Python callback) just
// re-acquires it briefly -- safe and standard.
template < typename T >
struct gil_releasing_deleter
{
    void operator()( T * p ) const noexcept
    {
        py::gil_scoped_release release;
        delete p;
    }
};

template < typename T >
using py_holder = std::unique_ptr< T, gil_releasing_deleter< T > >;

PYBIND11_DECLARE_HOLDER_TYPE( T, py_holder< T > )

// For rs2_format
#include "../include/librealsense2/h/rs_sensor.h"

// Hacky little bit of half-functions to make .def(BIND_DOWNCAST) look nice for binding as/is functions
#define BIND_DOWNCAST(class, downcast) "is_"#downcast, &rs2::class::is<rs2::downcast>).def("as_"#downcast, &rs2::class::as<rs2::downcast>

// Binding enums
const std::string rs2_prefix{ "rs2_" };
std::string make_pythonic_str(std::string str);
#define BIND_ENUM(module, rs2_enum_type, RS2_ENUM_COUNT, docstring) BIND_ENUM_CUSTOM( module, rs2_enum_type, 0, RS2_ENUM_COUNT-1, docstring)
#define BIND_ENUM_CUSTOM(module, rs2_enum_type, FIRST, LAST, docstring)                                                     \
    static std::string rs2_enum_type##pyclass_name = std::string(#rs2_enum_type).substr(rs2_prefix.length());               \
    /* Above 'static' is required in order to keep the string alive since py::class_ does not copy it */                    \
    py::enum_<rs2_enum_type> py_##rs2_enum_type(module, rs2_enum_type##pyclass_name.c_str(), docstring);                    \
    /* std::cout << std::endl << "## " << rs2_enum_type##pyclass_name  << ":" << std::endl; */                              \
    for (int i = FIRST; i <= LAST; i++)                                                                                     \
    {                                                                                                                       \
        rs2_enum_type v = static_cast<rs2_enum_type>(i);                                                                    \
        const char* enum_name = rs2_enum_type##_to_string(v);                                                               \
        auto python_name = make_pythonic_str(enum_name);                                                                    \
        py_##rs2_enum_type.value(python_name.c_str(), v);                                                                   \
        /* std::cout << " - " << python_name << std::endl;    */                                                            \
    }

// Binding arrays and matrices
template<typename T, size_t SIZE>
void copy_raw_array(T(&dst)[SIZE], const std::array<T, SIZE>& src)
{
    for (size_t i = 0; i < SIZE; i++)
    {
        dst[i] = src[i];
    }
}

template<typename T, size_t NROWS, size_t NCOLS>
void copy_raw_2d_array(T(&dst)[NROWS][NCOLS], const std::array<std::array<T, NCOLS>, NROWS>& src)
{
    for (size_t i = 0; i < NROWS; i++)
    {
        for (size_t j = 0; j < NCOLS; j++)
        {
            dst[i][j] = src[i][j];
        }
    }
}
template <typename T, size_t N>
std::string array_to_string(const T(&arr)[N])
{
    std::ostringstream oss;
    oss << "[";
    for (int i = 0; i < N; i++)
    {
        if (i != 0)
            oss << ", ";
        oss << arr[i];
    }
    oss << "]";
    return oss.str();
}

template <typename T, size_t N, size_t M>
std::string matrix_to_string(const T(&arr)[N][M])
{
    std::ostringstream oss;
    oss << "[";
    for (int i = 0; i < N; i++)
    {
        if (i != 0)
            oss << ", ";
        oss << "[";
        for (int j = 0; j < M; j++)
        {
            if (j != 0)
                oss << ", ";
            oss << arr[i][j];
        }
        oss << "]";
    }
    oss << "]";
    return oss.str();
}

#define BIND_RAW_ARRAY_GETTER(T, member, valueT, SIZE) [](const T& self) -> const std::array<valueT, SIZE>& { return reinterpret_cast<const std::array<valueT, SIZE>&>(self.member); }
#define BIND_RAW_ARRAY_SETTER(T, member, valueT, SIZE) [](T& self, const std::array<valueT, SIZE>& src) { copy_raw_array(self.member, src); }

#define BIND_RAW_2D_ARRAY_GETTER(T, member, valueT, NROWS, NCOLS) [](const T& self) -> const std::array<std::array<valueT, NCOLS>, NROWS>& { return reinterpret_cast<const std::array<std::array<valueT, NCOLS>, NROWS>&>(self.member); }
#define BIND_RAW_2D_ARRAY_SETTER(T, member, valueT, NROWS, NCOLS) [](T& self, const std::array<std::array<valueT, NCOLS>, NROWS>& src) { copy_raw_2d_array(self.member, src); }

#define BIND_RAW_ARRAY_PROPERTY(T, member, valueT, SIZE) #member, BIND_RAW_ARRAY_GETTER(T, member, valueT, SIZE), BIND_RAW_ARRAY_SETTER(T, member, valueT, SIZE)
#define BIND_RAW_2D_ARRAY_PROPERTY(T, member, valueT, NROWS, NCOLS) #member, BIND_RAW_2D_ARRAY_GETTER(T, member, valueT, NROWS, NCOLS), BIND_RAW_2D_ARRAY_SETTER(T, member, valueT, NROWS, NCOLS)

// TODO: Fill in missing formats
// Map format->data type for various things
template <rs2_format> struct FmtToType { using type = uint8_t; }; // Default to uint8_t
#define MAP_FMT_TO_TYPE(F, T) template <> struct FmtToType<F> { using type = T; }
MAP_FMT_TO_TYPE(RS2_FORMAT_Z16, uint16_t);
//MAP_FMT_TO_TYPE(RS2_FORMAT_DISPARITY16, );
MAP_FMT_TO_TYPE(RS2_FORMAT_XYZ32F, float);
MAP_FMT_TO_TYPE(RS2_FORMAT_YUYV, uint8_t);
MAP_FMT_TO_TYPE(RS2_FORMAT_RGB8, uint8_t);
MAP_FMT_TO_TYPE(RS2_FORMAT_BGR8, uint8_t);
MAP_FMT_TO_TYPE(RS2_FORMAT_RGBA8, uint8_t);
MAP_FMT_TO_TYPE(RS2_FORMAT_BGRA8, uint8_t);
MAP_FMT_TO_TYPE(RS2_FORMAT_Y8, uint8_t);
MAP_FMT_TO_TYPE(RS2_FORMAT_Y16, uint16_t);
//MAP_FMT_TO_TYPE(RS2_FORMAT_RAW10, );
MAP_FMT_TO_TYPE(RS2_FORMAT_RAW16, uint16_t);
MAP_FMT_TO_TYPE(RS2_FORMAT_RAW8, uint8_t);
MAP_FMT_TO_TYPE(RS2_FORMAT_UYVY, uint8_t);
//MAP_FMT_TO_TYPE(RS2_FORMAT_MOTION_RAW, );
MAP_FMT_TO_TYPE(RS2_FORMAT_MOTION_XYZ32F, float);
//MAP_FMT_TO_TYPE(RS2_FORMAT_GPIO_RAW, );
//MAP_FMT_TO_TYPE(RS2_FORMAT_6DOF, );
MAP_FMT_TO_TYPE(RS2_FORMAT_DISPARITY32, float);
MAP_FMT_TO_TYPE(RS2_FORMAT_Y10BPACK, uint8_t);
MAP_FMT_TO_TYPE(RS2_FORMAT_DISTANCE, float);
//MAP_FMT_TO_TYPE(RS2_FORMAT_MJPEG, );
MAP_FMT_TO_TYPE(RS2_FORMAT_Y8I, uint8_t);
//MAP_FMT_TO_TYPE(RS2_FORMAT_Y12I, );
//MAP_FMT_TO_TYPE(RS2_FORMAT_INZI, );
MAP_FMT_TO_TYPE(RS2_FORMAT_INVI, uint8_t);
//MAP_FMT_TO_TYPE(RS2_FORMAT_W10, );
MAP_FMT_TO_TYPE(RS2_FORMAT_FG, uint16_t);
//MAP_FMT_TO_TYPE(RS2_FORMAT_Y411, ); // RS2_FORMAT_Y411 is 12 bit per pixel and don't fit to any type
template <rs2_format FMT> struct itemsize {
    static constexpr size_t func() { return sizeof(typename FmtToType<FMT>::type); }
};
template <rs2_format FMT> struct fmtstring {
    static const std::string func() { return py::format_descriptor<typename FmtToType<FMT>::type>::format(); }
};

template<template<rs2_format> class F>
/*constexpr*/ auto fmt_to_value(rs2_format fmt) -> typename std::result_of<decltype(&F<RS2_FORMAT_ANY>::func)()>::type {
    switch (fmt) {
    case RS2_FORMAT_Z16: return F<RS2_FORMAT_Z16>::func();
    case RS2_FORMAT_DISPARITY16: return F<RS2_FORMAT_DISPARITY16>::func();
    case RS2_FORMAT_XYZ32F: return F<RS2_FORMAT_XYZ32F>::func();
    case RS2_FORMAT_YUYV: return F<RS2_FORMAT_YUYV>::func();
    case RS2_FORMAT_RGB8: return F<RS2_FORMAT_RGB8>::func();
    case RS2_FORMAT_BGR8: return F<RS2_FORMAT_BGR8>::func();
    case RS2_FORMAT_RGBA8: return F<RS2_FORMAT_RGBA8>::func();
    case RS2_FORMAT_BGRA8: return F<RS2_FORMAT_BGRA8>::func();
    case RS2_FORMAT_Y8: return F<RS2_FORMAT_Y8>::func();
    case RS2_FORMAT_Y16: return F<RS2_FORMAT_Y16>::func();
    case RS2_FORMAT_RAW10: return F<RS2_FORMAT_RAW10>::func();
    case RS2_FORMAT_RAW16: return F<RS2_FORMAT_RAW16>::func();
    case RS2_FORMAT_RAW8: return F<RS2_FORMAT_RAW8>::func();
    case RS2_FORMAT_UYVY: return F<RS2_FORMAT_UYVY>::func();
    case RS2_FORMAT_MOTION_RAW: return F<RS2_FORMAT_MOTION_RAW>::func();
    case RS2_FORMAT_MOTION_XYZ32F: return F<RS2_FORMAT_MOTION_XYZ32F>::func();
    case RS2_FORMAT_GPIO_RAW: return F<RS2_FORMAT_GPIO_RAW>::func();
    case RS2_FORMAT_6DOF: return F<RS2_FORMAT_6DOF>::func();
    case RS2_FORMAT_DISPARITY32: return F<RS2_FORMAT_DISPARITY32>::func();
    case RS2_FORMAT_Y10BPACK: return F<RS2_FORMAT_Y10BPACK>::func();
    case RS2_FORMAT_DISTANCE: return F<RS2_FORMAT_DISTANCE>::func();
    case RS2_FORMAT_MJPEG: return F<RS2_FORMAT_MJPEG>::func();
    case RS2_FORMAT_Y8I: return F<RS2_FORMAT_Y8I>::func();
    case RS2_FORMAT_Y12I: return F<RS2_FORMAT_Y12I>::func();
    case RS2_FORMAT_INZI: return F<RS2_FORMAT_INZI>::func();
    case RS2_FORMAT_INVI: return F<RS2_FORMAT_INVI>::func();
    case RS2_FORMAT_W10: return F<RS2_FORMAT_W10>::func();
    case RS2_FORMAT_FG: return F<RS2_FORMAT_FG>::func();
    case RS2_FORMAT_Y411: return F<RS2_FORMAT_Y411>::func();
    // c++11 standard doesn't allow throw in constexpr function switch case
    case RS2_FORMAT_COUNT: throw std::runtime_error("format.count is not a valid value for arguments of type format!");
    default: return F<RS2_FORMAT_ANY>::func();
    }
}

// Support Python's buffer protocol
class BufData {
public:
    void *_ptr = nullptr;         // Pointer to the underlying storage
    size_t _itemsize = 0;         // Size of individual items in bytes
    std::string _format;          // For homogeneous buffers, this should be set to format_descriptor<T>::format()
    size_t _ndim = 0;             // Number of dimensions
    std::vector<size_t> _shape;   // Shape of the tensor (1 entry per dimension)
    std::vector<size_t> _strides; // Number of entries between adjacent entries (for each per dimension)
public:
    BufData(void *ptr, size_t itemsize, const std::string& format, size_t ndim, const std::vector<size_t> &shape, const std::vector<size_t> &strides)
        : _ptr(ptr), _itemsize(itemsize), _format(format), _ndim(ndim), _shape(shape), _strides(strides) {}
    BufData(void *ptr, size_t itemsize, const std::string& format, size_t size)
        : BufData(ptr, itemsize, format, 1, std::vector<size_t> { size }, std::vector<size_t> { itemsize }) { }
    BufData(void *ptr, // Raw data pointer
        size_t itemsize, // Size of the type in bytes
        const std::string& format, // Data type's format descriptor (e.g. "@f" for float xyz)
        size_t dim, // number of data elements per group (e.g. 3 for float xyz)
        size_t count) // Number of groups
        : BufData(ptr, itemsize, format, 2, std::vector<size_t> { count, dim }, std::vector<size_t> { itemsize*dim, itemsize }) { }
};

/*PYBIND11_MAKE_OPAQUE(std::vector<rs2::stream_profile>)*/

// Partial module definition functions
void init_c_files(py::module& m);
void init_types(py::module &m);
void init_frame(py::module &m);
void init_options(py::module &m);
void init_processing(py::module &m);
void init_sensor(py::module &m);
void init_device(py::module &m);
void init_record_playback(py::module &m);
void init_context(py::module &m);
void init_pipeline(py::module &m);
void init_internal(py::module &m);
void init_export(py::module &m);
void init_advanced_mode(py::module &m);
void init_serializable_device(py::module& m);
void init_util(py::module &m);
void init_eth_config(py::module &m);
