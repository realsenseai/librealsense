// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2024 RealSense, Inc. All Rights Reserved.

#include "pyrealsense2.h"
#include <librealsense2/hpp/rs_options.hpp>
#include <librealsense2/h/rs_decimation_filter_dpp.h>
#include <librealsense2/h/rs_temporal_filter_dpp.h>
#include <librealsense2/h/rs_hdrd_control.h>

using rsutils::json;


void init_options(py::module &m) {
    /** rs_options.hpp **/

    // Typed struct bindings for the two known composite-option wire layouts, bound directly
    // against the real C structs via pointer-to-member (same mechanism as rs2_intrinsics above)
    // so a shape change fails to compile instead of silently misinterpreting bytes.
    py::class_< dpp_header >(
        m, "dpp_header", "Wire header shared by the HKR DPP composite-option control family - see rs_dpp_header.h." )
        .def( py::init<>() )
        .def_readwrite( "version", &dpp_header::version )
        .def_readwrite( "flags", &dpp_header::flags )
        .def_readwrite( "ctl_id", &dpp_header::ctl_id )
        .def_readwrite( "param_count", &dpp_header::param_count )
        .def_readwrite( "param_type", &dpp_header::param_type );

    py::class_< rs2_decimation_filter_dpp_config >(
        m, "decimation_filter_dpp_config", "Decimation Filter DPP composite option payload - see rs_decimation_filter_dpp.h." )
        .def( py::init<>() )
        .def_readwrite( "header", &rs2_decimation_filter_dpp_config::header )
        .def_readwrite( "enabled", &rs2_decimation_filter_dpp_config::enabled )
        .def_readwrite( "magnitude", &rs2_decimation_filter_dpp_config::magnitude )
        .def( "__repr__",
              []( rs2_decimation_filter_dpp_config const & v )
              {
                  std::ostringstream ss;
                  ss << "<decimation_filter_dpp_config enabled=" << v.enabled << " magnitude=" << v.magnitude << ">";
                  return ss.str();
              } );

    py::class_< rs2_decimation_filter_dpp_range >(
        m, "decimation_filter_dpp_range", "Supported {min,max,step,default} bounds for decimation_filter_dpp_config - see rs_decimation_filter_dpp.h." )
        .def( py::init<>() )
        .def_readwrite( "min", &rs2_decimation_filter_dpp_range::min )
        .def_readwrite( "max", &rs2_decimation_filter_dpp_range::max )
        .def_readwrite( "step", &rs2_decimation_filter_dpp_range::step )
        .def_readwrite( "default", &rs2_decimation_filter_dpp_range::def );

    py::class_< rs2_temporal_filter_dpp_config >(
        m, "temporal_filter_dpp_config", "Temporal Filter DPP composite option payload - see rs_temporal_filter_dpp.h." )
        .def( py::init<>() )
        .def_readwrite( "header", &rs2_temporal_filter_dpp_config::header )
        .def_readwrite( "enabled", &rs2_temporal_filter_dpp_config::enabled )
        .def_readwrite( "smooth_alpha", &rs2_temporal_filter_dpp_config::smooth_alpha )
        .def_readwrite( "smooth_delta", &rs2_temporal_filter_dpp_config::smooth_delta )
        .def_readwrite( "persistency_index", &rs2_temporal_filter_dpp_config::persistency_index )
        .def( "__repr__",
              []( rs2_temporal_filter_dpp_config const & v )
              {
                  std::ostringstream ss;
                  ss << "<temporal_filter_dpp_config enabled=" << v.enabled << " smooth_alpha=" << v.smooth_alpha
                     << " smooth_delta=" << v.smooth_delta << " persistency_index=" << v.persistency_index << ">";
                  return ss.str();
              } );

    py::class_< rs2_temporal_filter_dpp_range >(
        m, "temporal_filter_dpp_range", "Supported {min,max,step,default} bounds for temporal_filter_dpp_config - see rs_temporal_filter_dpp.h." )
        .def( py::init<>() )
        .def_readwrite( "min", &rs2_temporal_filter_dpp_range::min )
        .def_readwrite( "max", &rs2_temporal_filter_dpp_range::max )
        .def_readwrite( "step", &rs2_temporal_filter_dpp_range::step )
        .def_readwrite( "default", &rs2_temporal_filter_dpp_range::def );

    py::class_< rs2_hdrd_control >(
        m, "hdrd_control", "Improved Close Range Control composite option payload - see rs_hdrd_control.h." )
        .def( py::init<>() )
        .def_readwrite( "header", &rs2_hdrd_control::header )
        .def_readwrite( "enable", &rs2_hdrd_control::enable )
        .def_readwrite( "filter_type", &rs2_hdrd_control::filter_type )
        .def_readwrite( "downscale_ratio", &rs2_hdrd_control::downscale_ratio )
        .def_readwrite( "shift_mode", &rs2_hdrd_control::shift_mode )
        .def_readwrite( "shift_pixels", &rs2_hdrd_control::shift_pixels )
        .def_readwrite( "threshold_mode", &rs2_hdrd_control::threshold_mode )
        .def_readwrite( "threshold_mm", &rs2_hdrd_control::threshold_mm )
        .def( "__repr__",
              []( rs2_hdrd_control const & v )
              {
                  std::ostringstream ss;
                  ss << "<hdrd_control enable=" << v.enable << " filter_type=" << v.filter_type
                     << " downscale_ratio=" << v.downscale_ratio << " shift_mode=" << v.shift_mode
                     << " shift_pixels=" << v.shift_pixels << " threshold_mode=" << v.threshold_mode
                     << " threshold_mm=" << v.threshold_mm << ">";
                  return ss.str();
              } );

    py::class_< rs2_hdrd_control_range >(
        m, "hdrd_control_range", "Supported {min,max,step,default} bounds for hdrd_control - see rs_hdrd_control.h." )
        .def( py::init<>() )
        .def_readwrite( "min", &rs2_hdrd_control_range::min )
        .def_readwrite( "max", &rs2_hdrd_control_range::max )
        .def_readwrite( "step", &rs2_hdrd_control_range::step )
        // 'def' is a Python keyword and cannot be used as an attribute name (obj.def is a syntax
        // error) - exposed as "default" instead, the one place this binding's naming has to
        // diverge from the C++ field it mirrors.
        .def_readwrite( "default", &rs2_hdrd_control_range::def );

    // Expose option values as a custom struct rather than 'rs2_option_value*'
    struct option_value
    {
        rs2_option id;
        rs2_option_type type;
        py::object value;
        py::object range;  // None if the option has no range

        option_value( rs2::option_value const & value_ )
            : id( value_->id )
            , type( value_->type )
            , range( value_.has_range() ? py::cast( value_.range() ) : py::cast< py::none >( Py_None ) )
        {
            if( ! value_->is_valid )
                value = py::cast< py::none >( Py_None );
            else if( RS2_OPTION_TYPE_FLOAT == value_->type )
                value = py::float_( value_->as_float );
            else if( RS2_OPTION_TYPE_STRING == value_->type )
                value = py::str( value_->as_string );
            else if( RS2_OPTION_TYPE_INTEGER == value_->type )
                value = py::int_( value_->as_integer );
            else if( RS2_OPTION_TYPE_BOOLEAN == value_->type )
                value = py::bool_( value_->as_integer );
            else
                value = py::cast< py::none >( Py_None );
        }
    };
    py::class_< option_value >( m, "option_value" )
        .def_readwrite( "id", &option_value::id )
        .def_readwrite( "value", &option_value::value )  // None if no value available
        .def_readwrite( "type", &option_value::type )
        .def_readwrite( "range", &option_value::range )  // None if no range available
        .def( "__repr__",
              []( option_value const & self )
              {
                  std::ostringstream os;
                  os << '<' << rs2_option_to_string( self.id );
                  os << ' ' << py::str( self.value );
                  os << '>';
                  return os.str();
              } );
    // given an iterator, return an option_value
    struct option_value_from_iterator
    {
        option_value operator()( rs2::options_list::iterator const & it ) { return option_value( *it ); }
    };
    py::class_< rs2::options_list >( m, "options_list" )
        .def( "__getitem__",
              []( const rs2::options_list & self, size_t i )
              {
                  if( i >= self.size() )
                      throw py::index_error();
                  return option_value( self[i] );
              } )
        .def( "__len__", &rs2::options_list::size )
        .def( "__iter__",
              []( const rs2::options_list & self )
              {
                  return py::detail::make_iterator_impl< option_value_from_iterator,   // how to access value from iterator
                                                         py::return_value_policy::reference_internal,  // pybind default
                                                         rs2::options_list::iterator,  // iterator type
                                                         rs2::options_list::iterator,  // sentinel (end) type
                                                         option_value                  // value type
                                                         >( self.begin(), self.end() );
              },
              py::keep_alive< 0, 1 >() );

    py::class_<rs2::options> options(m, "options", "Base class for options interface. Should be used via sensor or processing_block."); // No docstring in C++
    options.def("is_option_read_only", &rs2::options::is_option_read_only, "Check if particular option "
                "is read only.", "option"_a)
        .def("get_option", &rs2::options::get_option, "Read option value from the device.", "option"_a, py::call_guard<py::gil_scoped_release>())
        .def( "get_option_value",
              []( rs2::options const & self, rs2_option option_id ) -> option_value
                  { return self.get_option_value( option_id ); } )
        .def( "set_option_value",
              []( rs2::options const & self, rs2_option option_id, json value )
              {
                  rs2::option_value rs2_value;
                  switch( value.type() )
                  {
                  case json::value_t::null:
                      rs2_value = rs2::option_value( option_id, rs2::option_value::invalid );
                      break;

                  case json::value_t::string:
                      rs2_value = rs2::option_value( option_id, value.string_ref().c_str() );
                      break;

                  case json::value_t::number_float:
                      rs2_value = rs2::option_value( option_id, value.get< float >() );
                      break;

                  case json::value_t::number_unsigned:
                  case json::value_t::number_integer:
                      rs2_value = rs2::option_value( option_id, value.get< int64_t >() );
                      break;

                  case json::value_t::boolean:
                      rs2_value = rs2::option_value( option_id, value.get< bool >() );
                      break;

                  default:
                      throw std::runtime_error( "invalid value type: " + value.dump() );
                  }
                  self.set_option_value( rs2_value );
              } )
        .def("get_option_range", &rs2::options::get_option_range, "Retrieve the available range of values "
             "of a supported option", "option"_a, py::call_guard<py::gil_scoped_release>())
        .def("set_option", &rs2::options::set_option, "Write new value to device option", "option"_a, "value"_a, py::call_guard<py::gil_scoped_release>())
        .def("supports", (bool (rs2::options::*)(rs2_option option) const) &rs2::options::supports, "Check if particular "
             "option is supported by a subdevice", "option"_a)
        .def("get_option_description", &rs2::options::get_option_description, "Get option description.", "option"_a)
        .def("get_option_value_description", &rs2::options::get_option_value_description, "Get option value description "
             "(In case a specific option value holds special meaning)", "option"_a, "value"_a)
        .def("get_supported_options", &rs2::options::get_supported_options, "Retrieve list of supported options") // No docstring in C++
        .def( "get_supported_option_values", &rs2::options::get_supported_option_values,
              "Retrieve the supported options, each with its value and range", py::call_guard< py::gil_scoped_release >() )
        .def( "on_options_changed", &rs2::options::on_options_changed,
              "Sets a callback to notify in case options in this container change value", "callback"_a )
        // Composite options are a completely separate identity space from ordinary rs2_option
        // scalar options above. Mirrors the C++ wrapper's "raw bytes only, no is<T>()/as<T>()"
        // surface - Python has no templates either, so get/set hand back/take raw bytes.
        .def(
            "get_composite_option",
            []( rs2::options const & self, rs2_composite_option_id id ) -> py::bytes
            {
                // NOT py::call_guard<gil_scoped_release>() on the whole function - constructing
                // py::bytes below is a real Python C-API call and needs the GIL held. Release it
                // only around the blocking device round-trip itself.
                std::vector< uint8_t > raw;
                {
                    py::gil_scoped_release release;
                    raw = self.get_composite_option( id );
                }
                return py::bytes( reinterpret_cast< const char * >( raw.data() ), raw.size() );
            },
            "Read a composite option's current raw payload, atomically, in ONE UVC transaction - "
            "cast the returned bytes against the option's documented wire layout.",
            "option"_a )
        .def(
            "set_composite_option",
            []( rs2::options const & self, rs2_composite_option_id id, py::buffer data )
            {
                // data.request() touches the Python buffer protocol - must run under the GIL, so
                // it happens before the scoped release below, not inside a call_guard.
                auto info = data.request();
                py::gil_scoped_release release;
                self.set_composite_option( id, info.ptr, (size_t)( info.size * info.itemsize ) );
            },
            "Write new value to a composite option, atomically, in ONE UVC transaction - `data` "
            "must match the option's documented wire layout byte-for-byte (e.g. bytes(struct.pack(...))).",
            "option"_a,
            "data"_a )
        .def(
            "get_composite_option_range",
            []( rs2::options const & self, rs2_composite_option_id id ) -> py::bytes
            {
                std::vector< uint8_t > raw;
                {
                    py::gil_scoped_release release;
                    raw = self.get_composite_option_range( id );
                }
                return py::bytes( reinterpret_cast< const char * >( raw.data() ), raw.size() );
            },
            "Read a composite option's supported {min,max,step,def} bounds, packed together - "
            "cast the returned bytes against the option's documented range struct.",
            "option"_a )
        .def( "supports_composite_option", &rs2::options::supports_composite_option,
              "Check if a particular composite option is supported by this options container.", "option"_a )
        .def( "is_composite_option_read_only", &rs2::options::is_composite_option_read_only,
              "Check if a particular composite option is read only.", "option"_a )
        .def( "get_composite_option_description", &rs2::options::get_composite_option_description,
              "Get a composite option's human-readable description.", "option"_a )
        .def( "get_supported_composite_options", &rs2::options::get_supported_composite_options,
              "Retrieve the list of composite option ids this options container supports." )
        // Typed counterparts to get/set_composite_option() above - the Python equivalent of the
        // C++ wrapper's own get_composite_option_as<T>() templates. Python has no templates, so
        // each known struct gets its own named method rather than one generic one.
        .def(
            "get_decimation_filter_dpp_config",
            []( rs2::options const & self, rs2_composite_option_id id ) -> rs2_decimation_filter_dpp_config
            {
                py::gil_scoped_release release;
                return self.get_composite_option_as< rs2_decimation_filter_dpp_config >( id );
            },
            "Typed counterpart to get_composite_option() for Decimation Filter DPP.",
            "option"_a )
        .def(
            "set_decimation_filter_dpp_config",
            []( rs2::options const & self, rs2_composite_option_id id, rs2_decimation_filter_dpp_config const & value )
            {
                py::gil_scoped_release release;
                self.set_composite_option_from( id, value );
            },
            "Typed counterpart to set_composite_option() for Decimation Filter DPP.",
            "option"_a,
            "value"_a )
        .def(
            "get_decimation_filter_dpp_range",
            []( rs2::options const & self, rs2_composite_option_id id ) -> rs2_decimation_filter_dpp_range
            {
                py::gil_scoped_release release;
                return self.get_composite_option_range_as< rs2_decimation_filter_dpp_range >( id );
            },
            "Typed counterpart to get_composite_option_range() for Decimation Filter DPP.",
            "option"_a )
        .def(
            "get_temporal_filter_dpp_config",
            []( rs2::options const & self, rs2_composite_option_id id ) -> rs2_temporal_filter_dpp_config
            {
                py::gil_scoped_release release;
                return self.get_composite_option_as< rs2_temporal_filter_dpp_config >( id );
            },
            "Typed counterpart to get_composite_option() for Temporal Filter DPP.",
            "option"_a )
        .def(
            "set_temporal_filter_dpp_config",
            []( rs2::options const & self, rs2_composite_option_id id, rs2_temporal_filter_dpp_config const & value )
            {
                py::gil_scoped_release release;
                self.set_composite_option_from( id, value );
            },
            "Typed counterpart to set_composite_option() for Temporal Filter DPP.",
            "option"_a,
            "value"_a )
        .def(
            "get_temporal_filter_dpp_range",
            []( rs2::options const & self, rs2_composite_option_id id ) -> rs2_temporal_filter_dpp_range
            {
                py::gil_scoped_release release;
                return self.get_composite_option_range_as< rs2_temporal_filter_dpp_range >( id );
            },
            "Typed counterpart to get_composite_option_range() for Temporal Filter DPP.",
            "option"_a )
        .def(
            "get_hdrd_control",
            []( rs2::options const & self, rs2_composite_option_id id ) -> rs2_hdrd_control
            {
                py::gil_scoped_release release;
                return self.get_composite_option_as< rs2_hdrd_control >( id );
            },
            "Typed counterpart to get_composite_option() for Improved Close Range Control - returns a "
            "hdrd_control object (bound directly against the real C struct) instead of raw bytes.",
            "option"_a )
        .def(
            "set_hdrd_control",
            []( rs2::options const & self, rs2_composite_option_id id, rs2_hdrd_control const & value )
            {
                py::gil_scoped_release release;
                self.set_composite_option_from( id, value );
            },
            "Typed counterpart to set_composite_option() for Improved Close Range Control.",
            "option"_a,
            "value"_a )
        .def(
            "get_hdrd_control_range",
            []( rs2::options const & self, rs2_composite_option_id id ) -> rs2_hdrd_control_range
            {
                py::gil_scoped_release release;
                return self.get_composite_option_range_as< rs2_hdrd_control_range >( id );
            },
            "Typed counterpart to get_composite_option_range() for Improved Close Range Control.",
            "option"_a );

    /** end rs_options.hpp **/
}
