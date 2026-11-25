// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2021 RealSense, Inc. All Rights Reserved.

#pragma once

#ifdef LIBCI_DEPENDENCY_realsense2
#include <librealsense2/rs.hpp>
#endif
#include "catch.h"
#include <sstream>


namespace test {


// This is the context in which the test is running -- the same --context passed to
// run-unit-tests.py
extern std::string context;


// True if --debug was passed to the test executable
extern bool debug;


// Serial number of the device the runner targeted for this test (--serial), empty if not passed
extern std::string serial;


class log_message
{
public:
    enum message_type : char
    {
        debug = 'D',
        info = 'I',
        warning = 'W',
        error = 'E',
        fatal = 'F'
    };

private:
    std::ostringstream _line;
    message_type _type;
    bool _empty = true;

public:
    log_message( message_type type ) : _type( type ) {}
    ~log_message();

    message_type type() const { return _type; }

    void print() {}

    // Usage equivalent to Python's log.d(...) -- including spaces in-between arguments:
    //     test::log.d( "This", "one" );
    template< typename T, typename... Types >
    void print( T const & arg, Types... args )
    {
        if( _line.tellp() )
            _line << ' ';
        _line << arg;
        print( args... );
    }

    // Allow this usage (without Python-like space separators):
    //     log_message( debug ) << "This " << std::setw(5) << "one";
    template< typename T >
    log_message & operator<<( T const & t )
    {
        _line << t;
        return *this;
    }

};
class debug_logger
{
public:
    template< typename... Types >
    void d( Types... args ) const
    {
        if( ! test::debug )
            return;
        log_message msg( log_message::debug );
        msg.print( args... );
    }
    template< typename... Types >
    void i( Types... args ) const
    {
        log_message msg( log_message::info );
        msg.print( args... );
    }
    template< typename... Types >
    void w( Types... args ) const
    {
        log_message msg( log_message::warning );
        msg.print( args... );
    }
    template< typename... Types >
    void e( Types... args ) const
    {
        log_message msg( log_message::error );
        msg.print( args... );
    }
    template< typename... Types >
    void f( Types... args ) const
    {
        log_message msg( log_message::fatal );
        msg.print( args... );
    }
};
extern debug_logger log;


}
