// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2024 RealSense, Inc. All Rights Reserved.
#pragma once

// Command-Line Interface for our tools/examples/etc.
// Requires that 'tclap' be added to project dependencies!

#include <tclap/CmdLine.h>
#include <tclap/ValueArg.h>

#include <librealsense2/rs.h>

#include <rsutils/os/ensure-console.h>
#include <rsutils/json.h>


namespace rs2 {


// Base version of CLI without any context functionality
class cli_no_context : public TCLAP::CmdLine
{
    using super = TCLAP::CmdLine;

    rs2_log_severity _default_log_level;

public:
    cli_no_context( std::string const & tool_name, std::string const & version_string )
        : super( tool_name,  // The message to be used in the usage output
                 ' ',        // Separates the argument flag/name from the value; leave default
                 version_string )
        , debug_arg( "debug", "Turn on librealsense debug logs" )
        , _default_log_level( RS2_LOG_SEVERITY_ERROR )  // By default, log ERROR messages only
    {
#ifdef BUILD_EASYLOGGINGPP
        add( debug_arg );
#endif
        // There isn't always a console... so if we need to show an error/usage, we need to enable it:
        setOutput( &_our_cmdline_output );
    }
    cli_no_context( std::string const & tool_name )
        : cli_no_context( tool_name, RS2_API_FULL_VERSION_STR ) {}

public:
    enum _required { required };

    class flag : public TCLAP::SwitchArg
    {
        using super = TCLAP::SwitchArg;

        // Override so we push to the back and reverse ordering isn't needed!
        void addToList( std::list< TCLAP::Arg * > & argList ) const override
            { argList.push_back( const_cast< flag * >( this ) ); }

    public:
        flag( std::string const & long_name, std::string const & description )
            : flag( long_name, false, description ) {}
        flag( char short_name, std::string const & long_name, std::string const & description )
            : flag( short_name, long_name, false, description ) {}
        // Avoid treating "string" as a boolean
        flag( std::string const &, char const *, std::string const & ) = delete;
        flag( std::string const & long_name, bool value, std::string const & description )
            : super( {}, long_name, description, value ) {}
        flag( char short_name, std::string const & long_name, bool value, std::string const & description )
            : super( std::string( 1, short_name ), long_name, description, value ) {}
    };

    template< class T >
    class value : public TCLAP::ValueArg< T >
    {
        using super = TCLAP::ValueArg< T >;

        // Override so we push to the back and reverse ordering isn't needed!
        void addToList( std::list< TCLAP::Arg * > & argList ) const override
            { argList.push_back( const_cast< value * >( this ) ); }

    public:
        value( std::string const & long_name, std::string const & type_description, T value, std::string const & description )
            : super( {}, long_name, description, false /* not required */, value, type_description ) {}
        value( std::string const & long_name, std::string const & type_description, T value, std::string const & description, cli_no_context::_required )
            : super( {}, long_name, description, true /* required */, value, type_description ) {}
        value( char short_name, std::string const & long_name, std::string const & type_description, T value, std::string const & description )
            : super( std::string( 1, short_name ), long_name, description, false /* not required */, value, type_description ) {}
        value( char short_name, std::string const & long_name, std::string const & type_description, T value, std::string const & description, cli_no_context::_required )
            : super( std::string( 1, short_name ), long_name, description, true /* required */, value, type_description ) {}
    };

    // One-line operation:
    //     auto settings = cli( "description" ).arg( arg1, arg2 ).process( argc, argv );
    // add() is still available:
    //     cli cmd( "description" );
    //     cmd.add( arg1 );
public:
    template< typename... Rest >
    cli_no_context & arg( TCLAP::Arg & a )
    {
        super::add( a );
        return *this;
    }
    template< typename... Rest >
    cli_no_context & arg( TCLAP::Arg & a, Rest... rest )
    {
        super::add( a );
        return arg( std::forward< Rest >( rest )... );
    }

    cli_no_context & default_log_level( rs2_log_severity severity )
    {
        _default_log_level = severity;
        return *this;
    }

    // Replace parse() with our own process()
protected:
    using super::parse;  // don't let user call parse() - enforce using process()

    virtual void apply()
    {
#ifdef BUILD_EASYLOGGINGPP
        if( debug_arg.getValue() )
            rsutils::os::ensure_console();
        if( rsutils::os::has_console() )
            rs2_log_to_console( debug_arg.getValue() ? RS2_LOG_SEVERITY_DEBUG : _default_log_level, nullptr );
#endif
    }

    virtual rsutils::json build_cli_settings()
    {
        return rsutils::json::object();
    }

public:
    rsutils::json process( int argc, const char * const * argv )
    {
        parse( argc, argv );
        apply();
        return build_cli_settings();
    }

protected:
    // Override basic output methods:
    // If help is needed, for example, usage() will get called but we have not even ensured we have a console yet!
    class cmdline_output : public TCLAP::StdOutput
    {
        using super = TCLAP::StdOutput;

    public:
        void usage( TCLAP::CmdLineInterface & c ) override
        {
            rsutils::os::ensure_console();
            super::usage( c );
        }

        void version( TCLAP::CmdLineInterface & c ) override
        {
            rsutils::os::ensure_console();
            super::version( c );
        }

        void failure( TCLAP::CmdLineInterface & c, TCLAP::ArgException & e ) override
        {
            rsutils::os::ensure_console();
            super::failure( c, e );
        }
    };

    cmdline_output _our_cmdline_output;

    // Public args that can be retrieved by the user
public:
    flag debug_arg;
};


// CLI with additional context-controlling behavior:
//      (none at this time)
//
class cli_no_dds : public cli_no_context
{
    using super = cli_no_context;

public:
    using super::super;  // inherit same ctors
};


// CLI with additional DDS-controlling behavior:
//      --eth
//      --eth-only
//      --no-eth
//      --domain-id <0-232>
//
class cli : public cli_no_dds
{
    using super = cli_no_dds;

public:
    cli( std::string const & tool_name, std::string const & version_string )
        : super( tool_name, version_string )
        , eth_arg( "eth", "Detect DDS devices, even if disabled in the configuration.\nNOTE - ignored when not built with DDS support" )
        , eth_only_arg( "eth-only", "Use ONLY DDS devices; do not look for USB/MIPI devices.\nNOTE - ignored when not built with DDS support" )
        , no_eth_arg( "no-eth", "Do not detect DDS devices, even if enabled in the configuration.\nNOTE - no-op when not built with DDS support" )
        , domain_id_arg( "domain-id", "0-232", 0, "Domain ID to use with DDS devices.\nNOTE - ignored when not built with DDS support" )
    {
        add( eth_arg );
        add( eth_only_arg );
        add( no_eth_arg );
        add( domain_id_arg );
    }
    cli( std::string const & tool_name )
        : cli( tool_name, RS2_API_FULL_VERSION_STR ) {}

public:
    rsutils::json build_cli_settings() override
    {
        auto settings = super::build_cli_settings();

#ifdef BUILD_WITH_DDS
        if( eth_arg.isSet() + eth_only_arg.isSet() + no_eth_arg.isSet() > 1 )
            throw TCLAP::CmdLineParseException( "--eth, --eth-only, and --no-eth are mutually exclusive", "FATAL" );
        if( domain_id_arg.isSet() + no_eth_arg.isSet() > 1 )
            throw TCLAP::CmdLineParseException( "--domain-id and --no-eth are mutually exclusive", "FATAL" );

        if( eth_arg.isSet() )
        {
            settings["dds"]["enabled"] = true;
        }
        else if( eth_only_arg.isSet() )
        {
            settings["device-mask"] = RS2_PRODUCT_LINE_SW_ONLY | RS2_PRODUCT_LINE_ANY;
            settings["dds"]["enabled"] = true;
        }
        else if( no_eth_arg.isSet() )
        {
            settings["dds"]["enabled"] = false;
        }

        if( domain_id_arg.isSet() )
        {
            if( domain_id_arg.getValue() < 0 || domain_id_arg.getValue() > 232 )
                throw TCLAP::ArgParseException( "expecting [0, 232]", domain_id_arg.toString() );
            settings["dds"]["domain"] = domain_id_arg.getValue();
        }
#endif // BUILD_WITH_DDS

        return settings;
    }

    // Public args that can be retrieved by the user
public:
    flag eth_arg;
    flag eth_only_arg;
    flag no_eth_arg;
    value< int > domain_id_arg;
};


}  // namespace rs2
