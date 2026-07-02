// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

// Console tool that streams the Safety stream from a connected device and
// dumps every frame's metadata. It walks all rs2_frame_metadata_value entries
// and, for each one the frame supports, records the raw integer - mirroring
// the pattern used in examples/save-to-disk/rs-save-to-disk.cpp::metadata_to_csv
// and common/stream-model.cpp::show_stream_metadata.
//
// The tool has no hardcoded knowledge of individual safety fields, so new
// RS2_FRAME_METADATA_SAFETY_* entries show up in the output automatically as
// soon as librealsense is rebuilt - no code change here.

#include <librealsense2/rs.hpp>
#include <common/cli.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>


namespace {

std::atomic< bool > g_stop{ false };

void on_sigint( int )
{
    g_stop = true;
}

struct field
{
    rs2_frame_metadata_value md;
    std::string              name;
    long long                value;
};

std::vector< field > parse_frame( const rs2::frame & f )
{
    std::vector< field > out;
    out.reserve( RS2_FRAME_METADATA_COUNT );
    for( int i = 0; i < RS2_FRAME_METADATA_COUNT; ++i )
    {
        auto md = static_cast< rs2_frame_metadata_value >( i );
        if( ! f.supports_frame_metadata( md ) )
            continue;
        out.push_back( { md, rs2_frame_metadata_to_string( md ), f.get_frame_metadata( md ) } );
    }
    return out;
}


class writer
{
public:
    virtual ~writer() = default;
    virtual void write( const rs2::frame & f, const std::vector< field > & fields ) = 0;
};


class console_writer : public writer
{
public:
    explicit console_writer( std::ostream & os ) : _os( os ) {}

    void write( const rs2::frame & f, const std::vector< field > & fields ) override
    {
        std::size_t width = 0;
        for( const auto & fld : fields )
            width = std::max( width, fld.name.size() );

        _os << "\n=== Safety frame #" << f.get_frame_number()
            << "  hw_ts=" << std::fixed << std::setprecision( 3 ) << f.get_timestamp()
            << " ===\n";
        for( const auto & fld : fields )
        {
            _os << "  " << std::left << std::setw( static_cast< int >( width ) ) << fld.name
                << " : " << fld.value << '\n';
        }
        _os.flush();
    }

private:
    std::ostream & _os;
};


class csv_writer : public writer
{
public:
    explicit csv_writer( const std::string & path ) : _fh( path )
    {
        if( ! _fh.is_open() )
            throw std::runtime_error( "cannot open output file: " + path );
    }

    void write( const rs2::frame & f, const std::vector< field > & fields ) override
    {
        if( ! _header_written )
        {
            _fh << "frame_number,hw_timestamp";
            for( const auto & fld : fields )
                _fh << ',' << fld.name;
            _fh << '\n';
            _header_written = true;
            _header_fields.reserve( fields.size() );
            for( const auto & fld : fields )
                _header_fields.push_back( fld.name );
        }
        _fh << f.get_frame_number() << ',' << std::fixed << std::setprecision( 3 ) << f.get_timestamp();
        for( const auto & name : _header_fields )
        {
            _fh << ',';
            for( const auto & fld : fields )
            {
                if( fld.name == name )
                {
                    _fh << fld.value;
                    break;
                }
            }
        }
        _fh << '\n';
        _fh.flush();
    }

private:
    std::ofstream              _fh;
    bool                       _header_written = false;
    std::vector< std::string > _header_fields;
};

}  // namespace


int main( int argc, char * argv[] ) try
{
    using rs2::cli;
    cli cmd( "librealsense rs-safety-stream-collect tool" );
    cli::value< std::string > out_file(   'f', "FullFilePath",   "path",          "",  "output file (stdout if omitted, in console mode)" );
    cli::value< std::string > out_format( 'F', "Format",         "console|csv",   "console", "output format" );
    cli::value< std::string > serial(     's', "SerialNumber",   "serial_number", "",  "device serial number (defaults to first device)" );
    cli::value< double >      duration(   't', "Duration",       "seconds",       0.0, "stop after N seconds (0 = until Ctrl-C)" );
    cli::value< int >         max_frames( 'm', "MaxFramesNumber","frames",        0,   "stop after N frames (0 = unlimited)" );
    cli::value< int >         fps_arg(    'r', "Fps",            "fps",           30,  "safety stream FPS" );
    cli::value< int >         timeout_ms( 'w', "WaitTimeoutMs",  "ms",            5000,"wait_for_frames timeout" );

    cmd.add( out_file );
    cmd.add( out_format );
    cmd.add( serial );
    cmd.add( duration );
    cmd.add( max_frames );
    cmd.add( fps_arg );
    cmd.add( timeout_ms );
    auto settings = cmd.process( argc, argv );

    std::unique_ptr< writer > out;
    std::ofstream             fout;
    const std::string         fmt = out_format.getValue();
    if( fmt == "csv" )
    {
        if( out_file.getValue().empty() )
            throw std::runtime_error( "-f is required with -F csv" );
        out = std::unique_ptr< writer >( new csv_writer( out_file.getValue() ) );
    }
    else if( fmt == "console" )
    {
        if( ! out_file.getValue().empty() )
        {
            fout.open( out_file.getValue() );
            if( ! fout.is_open() )
                throw std::runtime_error( "cannot open output file: " + out_file.getValue() );
            out = std::unique_ptr< writer >( new console_writer( fout ) );
        }
        else
        {
            out = std::unique_ptr< writer >( new console_writer( std::cout ) );
        }
    }
    else
    {
        throw std::runtime_error( "unknown --Format: " + fmt + " (expected console|csv)" );
    }

    std::signal( SIGINT, on_sigint );

    rs2::context ctx( settings.dump() );
    rs2::config  cfg;
    if( ! serial.getValue().empty() )
        cfg.enable_device( serial.getValue() );
    cfg.enable_stream( RS2_STREAM_SAFETY, RS2_FORMAT_Y8, fps_arg.getValue() );

    rs2::pipeline pipe( ctx );
    auto          profile = pipe.start( cfg );
    auto          dev     = profile.get_device();
    std::string   name    = dev.supports( RS2_CAMERA_INFO_NAME )          ? dev.get_info( RS2_CAMERA_INFO_NAME )          : "?";
    std::string   sn      = dev.supports( RS2_CAMERA_INFO_SERIAL_NUMBER ) ? dev.get_info( RS2_CAMERA_INFO_SERIAL_NUMBER ) : "?";
    std::cerr << "Streaming safety from " << name << " (SN " << sn << ") at " << fps_arg.getValue()
              << " fps. Ctrl-C to stop." << std::endl;

    auto start = std::chrono::steady_clock::now();
    int  n     = 0;
    while( ! g_stop )
    {
        if( duration.getValue() > 0.0 )
        {
            auto elapsed = std::chrono::duration_cast< std::chrono::duration< double > >(
                               std::chrono::steady_clock::now() - start )
                               .count();
            if( elapsed >= duration.getValue() )
                break;
        }
        if( max_frames.getValue() > 0 && n >= max_frames.getValue() )
            break;

        rs2::frameset frames;
        try
        {
            frames = pipe.wait_for_frames( static_cast< unsigned int >( timeout_ms.getValue() ) );
        }
        catch( const rs2::error & e )
        {
            std::cerr << "wait_for_frames failed: " << e.what() << std::endl;
            continue;
        }

        rs2::frame safety = frames.first_or_default( RS2_STREAM_SAFETY );
        if( ! safety )
            continue;

        auto fields = parse_frame( safety );
        out->write( safety, fields );
        ++n;
    }

    pipe.stop();
    auto elapsed = std::chrono::duration_cast< std::chrono::duration< double > >(
                       std::chrono::steady_clock::now() - start )
                       .count();
    std::cerr << "\nCaptured " << n << " safety frames in " << std::fixed << std::setprecision( 2 )
              << elapsed << "s" << std::endl;

    return EXIT_SUCCESS;
}
catch( const rs2::error & e )
{
    std::cerr << "RealSense error calling " << e.get_failed_function() << "(" << e.get_failed_args()
              << "):\n    " << e.what() << std::endl;
    return EXIT_FAILURE;
}
catch( const std::exception & e )
{
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
}
