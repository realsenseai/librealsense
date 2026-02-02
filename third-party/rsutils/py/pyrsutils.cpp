// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2022 RealSense, Inc. All Rights Reserved.

#include <rsutils/py/pybind11.h>
#include <rsutils/easylogging/easyloggingpp.h>
#include <rsutils/string/split.h>
#include <rsutils/string/from.h>
#include <rsutils/string/shorten-json-string.h>
#include <rsutils/version.h>
#include <rsutils/number/running-average.h>
#include <rsutils/number/stabilized-value.h>
#include <rsutils/os/executable-name.h>
#include <rsutils/os/special-folder.h>
#include <rsutils/type/eth-config.h>
#include <rsutils/type/ip-address.h>


#define NAME pyrsutils
#define SNAME "pyrsutils"


PYBIND11_MODULE(NAME, m) {
    m.doc() = R"pbdoc(
        RealSense Utilities Python Bindings
    )pbdoc";
    m.attr( "__version__" ) = "0.1";  // RS2_API_VERSION_STR;

    rsutils::configure_elpp_logger();

    m.def( "debug",
           &rsutils::configure_elpp_logger,
           py::arg( "enable" ),
           py::arg( "nested-string" ) = "",
           py::arg( "logger" ) = LIBREALSENSE_ELPP_ID );

    m.def( "split", &rsutils::string::split );
    m.def(
        "shorten_json_string",
        []( std::string const & str, size_t max_length )
        { return rsutils::string::shorten_json_string( str, max_length ).to_string(); },
        py::arg( "string" ),
        py::arg( "max-length" ) = 96 );
    m.def(
        "shorten_json_string",
        []( rsutils::json const & j, size_t max_length )
        { return rsutils::string::shorten_json_string( j.dump(), max_length ).to_string(); },
        py::arg( "json" ),
        py::arg( "max-length" ) = 96 );
    m.def(
        "json_dump",  // pretty print, using our own output
        []( rsutils::json const & j, size_t indent )
        {
            std::ostringstream os;
            if( indent )
                os << std::setw( indent );
            os << j;
            return os.str();
        },
        py::arg( "json" ),
        py::arg( "indent" ) = 4 );
    m.def( "executable_path", &rsutils::os::executable_path );
    m.def( "executable_name", &rsutils::os::executable_name, py::arg( "with_extension" ) = false );

    m.def( "string_from_double", []( double d ) { return rsutils::string::from( d ).str(); } );

    using rsutils::version;
    py::class_< version >( m, "version" )
        .def( py::init<>() )
        .def( py::init< std::string const & >() )
        .def( py::init< version::sub_type, version::sub_type, version::sub_type, version::sub_type >(),
              py::arg( "major" ),
              py::arg( "minor" ),
              py::arg( "patch" ),
              py::arg( "build" ) = 0 )
        .def_static( "from_number", []( version::number_type n ) { return version( n ); } )
        .def( "is_valid", &version::is_valid )
        .def( "__bool__", &version::is_valid )
        .def( "major", &version::get_major )
        .def( "minor", &version::get_minor )
        .def( "patch", &version::get_patch )
        .def( "build", &version::get_build )
        .def( "without_build", &version::without_build )
        .def( "to_string", &version::to_string )
        .def( "__str__", &version::to_string )
        .def( "__repr__",
              []( version const & self ) {
                  std::ostringstream os;
                  os << "<" SNAME ".version";
                  if( self.is_valid() )
                      os << " " << self.to_string();
                  os << ">";
                  return os.str();
              } )
        .def_readwrite( "number", &version::number )
        .def( py::self < py::self )
        .def( py::self <= py::self )
        .def( py::self == py::self )
        .def( py::self != py::self )
        .def( py::self >= py::self )
        .def( py::self > py::self )
        .def( "is_between", &version::is_between );

    using int_avg = rsutils::number::running_average< int64_t >;
    py::class_< int_avg >( m, "running_average_i" )
        .def( py::init<>() )
        .def( "__nonzero__", &int_avg::size )  // Called to implement truth value testing in Python 2
        .def( "__bool__", &int_avg::size )     // Called to implement truth value testing in Python 3
        .def( "size", &int_avg::size )
        .def( "get", &int_avg::get )
        .def( "leftover", &int_avg::leftover )
        .def( "fraction", &int_avg::fraction )
        .def( "get_double", &int_avg::get_double )
        .def( "__int__", &int_avg::get )
        .def( "__float__", &int_avg::get_double )
        .def( "__str__", []( int_avg const & self ) -> std::string { return rsutils::string::from( self.get_double() ); } )
        .def( "__repr__",
              []( int_avg const & self ) -> std::string {
                  return rsutils::string::from() << "<" SNAME ".running_average<int64_t>"
                                                 << " " << self.get() << " "
                                                 << ( self.leftover() < 0 ? "" : "+" ) << self.leftover()
                                                 << "/" << self.size() << ">";
              } )
        .def( "add", &int_avg::add );

    using double_avg = rsutils::number::running_average< double >;
    py::class_< double_avg >( m, "running_average" )
        .def( py::init<>() )
        .def( "__nonzero__", &double_avg::size )  // Called to implement truth value testing in Python 2
        .def( "__bool__", &double_avg::size )     // Called to implement truth value testing in Python 3
        .def( "size", &double_avg::size )
        .def( "get", &double_avg::get )
        .def( "__float__", &double_avg::get )
        .def( "__str__", []( double_avg const & self ) -> std::string { return rsutils::string::from( self.get() ); } )
        .def( "__repr__",
              []( double_avg const & self ) -> std::string {
                  return rsutils::string::from() << "<" SNAME ".running_average<double>"
                                                 << " " << self.get() << " /" << self.size() << ">";
              } )
        .def( "add", &double_avg::add );

    using stabilized_value = rsutils::number::stabilized_value< double >;
    auto not_empty = []( stabilized_value const & self ) {
        return ! self.empty();
    };
    auto to_string = []( stabilized_value const & self ) -> std::string {
        if( self.empty() )
            return "EMPTY";
        return rsutils::string::from( self.get() );
    };
    py::class_< stabilized_value >( m, "stabilized_value" )
        .def( py::init< size_t >() )
        .def( "empty", &stabilized_value::empty )
        .def( "__bool__", not_empty )
        .def( "add", &stabilized_value::add )
        .def( "get", &stabilized_value::get, py::arg( "stabilization-percent" ) = 0.75 )
        .def( "clear", &stabilized_value::clear )
        .def( "to_string", to_string )
        .def( "__str__", to_string );

    py::enum_< rsutils::os::special_folder >( m, "special_folder" )
        .value( "app_data", rsutils::os::special_folder::app_data )
        .value( "temp_folder", rsutils::os::special_folder::temp_folder )
        .value( "user_desktop", rsutils::os::special_folder::user_desktop )
        .value( "user_documents", rsutils::os::special_folder::user_documents )
        .value( "user_pictures", rsutils::os::special_folder::user_pictures )
        .value( "user_videos", rsutils::os::special_folder::user_videos );
    m.def( "get_special_folder", rsutils::os::get_special_folder );

    using rsutils::type::ip_address;
    py::class_< ip_address >( m, "ip_address" )
        .def( py::init<>() )
        .def( py::init< uint8_t, uint8_t, uint8_t, uint8_t >(), "b1", "b2", "b3", "b4" )
        .def( py::init< const uint8_t* >(), "b" )
        .def( "is_valid", &ip_address::is_valid )
        .def( "empty", &ip_address::empty )
        .def( "clear", &ip_address::clear )
        .def( "__str__", &ip_address::to_string )
        .def( "__eq__", &ip_address::operator== )
        .def( "__ne__", &ip_address::operator!= )
        .def( "get_components", []( const ip_address & self, uint8_t & b0, uint8_t & b1, uint8_t & b2, uint8_t & b3 ) { self.get_components( b0, b1, b2, b3 ); }, "Get IP address components" )
        .def( "get_components", []( const ip_address & self, uint8_t b[4] ) { self.get_components( b ); }, "Get IP address components" );

    using rsutils::type::ip_3;
    py::class_< ip_3 >( m, "ip_3" )
        .def( py::init<>() )
        .def_readwrite( "ip", &ip_3::ip )
        .def_readwrite( "netmask", &ip_3::netmask )
        .def_readwrite( "gateway", &ip_3::gateway );

    using rsutils::type::eth_config_header;
    py::class_< eth_config_header >( m, "eth_config_header" )
        .def( py::init<>() )
        .def_readwrite( "version", &eth_config_header::version )
        .def_readwrite( "size", &eth_config_header::size )
        .def_readwrite( "crc", &eth_config_header::crc );

    using rsutils::type::link_priority;
    py::enum_< link_priority >( m, "link_priority" )
        .value( "usb_only", link_priority::usb_only )
        .value( "eth_only", link_priority::eth_only )
        .value( "eth_first", link_priority::eth_first )
        .value( "usb_first", link_priority::usb_first )
        .value( "dynamic_eth_first", link_priority::dynamic_eth_first )
        .value( "dynamic_usb_first", link_priority::dynamic_usb_first );

    using rsutils::type::eth_config;
    py::class_< eth_config::dds_t >( m, "eth_config::dds_t" )
        .def( py::init<>() )
        .def_readwrite( "domain_id", &eth_config::dds_t::domain_id );

    py::class_< eth_config::link_t >( m, "eth_config::link_t" )
        .def( py::init<>() )
        .def_readwrite( "mtu", &eth_config::link_t::mtu )
        .def_readwrite( "speed", &eth_config::link_t::speed )
        .def_readwrite( "timeout", &eth_config::link_t::timeout )
        .def_readwrite( "priority", &eth_config::link_t::priority );

    py::class_< eth_config::dhcp_t >( m, "eth_config::dhcp_t" )
        .def( py::init<>() )
        .def_readwrite( "on", &eth_config::dhcp_t::on )
        .def_readwrite( "timeout", &eth_config::dhcp_t::timeout );

    py::class_< eth_config >( m, "eth_config" )
        .def( py::init<>() )
        .def( py::init< std::vector< uint8_t > const & >() )
        .def_readwrite( "header", &eth_config::header )
        .def_readwrite( "mac_address", &eth_config::mac_address )
        .def_readwrite( "configured", &eth_config::configured )
        .def_readwrite( "actual", &eth_config::actual )
        .def_readwrite( "dds", &eth_config::dds )
        .def_readwrite( "link", &eth_config::link )
        .def_readwrite( "dhcp", &eth_config::dhcp )
        .def_readwrite( "transmission_delay", &eth_config::transmission_delay )
        .def_readwrite( "udp_ttl", &eth_config::udp_ttl )
        .def( "build_command", &eth_config::build_command )
        .def( "validate", &eth_config::validate );
}
