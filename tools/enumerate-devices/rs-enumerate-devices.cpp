// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2015-24 RealSense, Inc. All Rights Reserved.

#include <librealsense2/rs.hpp>
#include <iostream>
#include <iomanip>
#include <map>
#include <set>
#include <cstring>
#include <cmath>
#include <limits>
#include <thread>

#include <rsutils/json.h>
using rsutils::json;

#include <common/cli.h>

using namespace std;
using namespace rs2;

std::vector<std::string> tokenize_floats(string input, char separator) {
    std::vector<std::string> tokens;
    stringstream ss(input);
    string token;

    while (std::getline(ss, token, separator)) {
        tokens.push_back(token);
    }

    return tokens;
}

void print(const rs2_extrinsics& extrinsics)
{
    stringstream ss;
    ss << " Rotation Matrix:\n";

    // Align displayed data along decimal point
    for (auto i = 0; i < 3; ++i)
    {
        for (auto j = 0; j < 3; ++j)
        {
            std::ostringstream oss;
            oss << extrinsics.rotation[j * 3 + i];
            auto tokens = tokenize_floats(oss.str().c_str(), '.');
            ss << right << setw(4) << tokens[0];
            if (tokens.size() > 1)
                ss << "." << left << setw(12) << tokens[1];
        }
        ss << endl;
    }

    ss << "\n Translation Vector: ";
    for (auto i = 0u; i < sizeof(extrinsics.translation) / sizeof(extrinsics.translation[0]); ++i)
        ss << setprecision(15) << extrinsics.translation[i] << "  ";

    cout << ss.str() << endl << endl;
}

void print(const rs2_motion_device_intrinsic& intrinsics)
{
    stringstream ss;
    ss << "Bias Variances: \t";

    for (auto i = 0u; i < sizeof(intrinsics.bias_variances) / sizeof(intrinsics.bias_variances[0]); ++i)
        ss << setprecision(15) << std::fixed << intrinsics.bias_variances[i] << "  ";

    ss << "\nNoise Variances: \t";
    for (auto i = 0u; i < sizeof(intrinsics.noise_variances) / sizeof(intrinsics.noise_variances[0]); ++i)
        ss << setprecision(15) << std::fixed << intrinsics.noise_variances[i] << "  ";

    ss << "\nSensitivity : " << std::endl;
    for (auto i = 0u; i < sizeof(intrinsics.data) / sizeof(intrinsics.data[0]); ++i)
    {
        for (auto j = 0u; j < sizeof(intrinsics.data[0]) / sizeof(intrinsics.data[0][0]); ++j)
            ss << std::right << std::setw(13) << setprecision(6) << intrinsics.data[i][j] << "  ";
        ss << "\n";
    }

    cout << ss.str() << endl << endl;
}

void print(const rs2_intrinsics& intrinsics)
{
    stringstream ss;
    ss << left << setw(14) << "  Width: " << "\t" << intrinsics.width << endl <<
        left << setw(14) << "  Height: " << "\t" << intrinsics.height << endl <<
        left << setw(14) << "  PPX: " << "\t" << setprecision(15) << intrinsics.ppx << endl <<
        left << setw(14) << "  PPY: " << "\t" << setprecision(15) << intrinsics.ppy << endl <<
        left << setw(14) << "  Fx: " << "\t" << setprecision(15) << intrinsics.fx << endl <<
        left << setw(14) << "  Fy: " << "\t" << setprecision(15) << intrinsics.fy << endl <<
        left << setw(14) << "  Distortion: " << "\t" << rs2_distortion_to_string(intrinsics.model) << endl <<
        left << setw(14) << "  Coeffs: ";

    for (auto i = 0u; i < sizeof(intrinsics.coeffs) / sizeof(intrinsics.coeffs[0]); ++i)
        ss << "\t" << setprecision(15) << intrinsics.coeffs[i] << "  ";

    float fov[2];
    rs2_fov(&intrinsics, fov);
    ss << endl << left << setw(14) << "  FOV (deg): " << "\t" << setprecision(4) << fov[0] << " x " << fov[1];

    cout << ss.str() << endl << endl;
}

bool safe_get_intrinsics(const video_stream_profile& profile, rs2_intrinsics& intrinsics)
{
    bool ret = false;
    try
    {
        intrinsics = profile.get_intrinsics();
        ret = true;
    }
    catch (...)
    {
    }

    return ret;
}

bool safe_get_motion_intrinsics(const motion_stream_profile& profile, rs2_motion_device_intrinsic& intrinsics)
{
    bool ret = false;
    try
    {
        intrinsics = profile.get_motion_intrinsics();
        ret = true;
    }
    catch (...)
    {
    }
    return ret;
}

struct stream_and_resolution {
    rs2_stream stream;
    int stream_index;
    int width;
    int height;
    string stream_name;

    bool operator <(const stream_and_resolution& obj) const
    {
        return (std::make_tuple(stream, stream_index, width, height) < std::make_tuple(obj.stream, obj.stream_index, obj.width, obj.height));
    }
};

struct stream_and_index {
    rs2_stream stream;
    int stream_index;

    bool operator <(const stream_and_index& obj) const
    {
        return (std::make_tuple(stream, stream_index) < std::make_tuple(obj.stream, obj.stream_index));
    }
};

bool operator ==(const rs2_intrinsics& lhs,
    const rs2_intrinsics& rhs)
{
    return  lhs.width == rhs.width &&
        lhs.height == rhs.height &&
        (fabs(lhs.ppx - rhs.ppx) < std::numeric_limits<float>::min()) &&
        (fabs(lhs.ppy - rhs.ppy) < std::numeric_limits<float>::min()) &&
        (fabs(lhs.fx - rhs.fx) < std::numeric_limits<float>::min()) &&
        (fabs(lhs.fy - rhs.fy) < std::numeric_limits<float>::min()) &&
        lhs.model == rhs.model &&
        !std::memcmp(lhs.coeffs, rhs.coeffs, sizeof(rhs.coeffs));
}

bool operator ==(const rs2_motion_device_intrinsic& lhs,
    const rs2_motion_device_intrinsic& rhs)
{
    return !std::memcmp(&lhs, &rhs, sizeof(rs2_motion_device_intrinsic));
}

string get_str_formats(const set<rs2_format>& formats)
{
    stringstream ss;
    for (auto format = formats.begin(); format != formats.end(); ++format)
    {
        ss << *format << ((format != formats.end()) && (next(format) == formats.end()) ? "" : "/");
    }
    return ss.str();
}

void output_modes( std::vector< stream_profile > const & profiles, bool verbose, bool show_defaults )
{
    size_t const w_res = 15;
    
    size_t w_format = 10;
    size_t w_stream = 10;
    bool video_stream = false;
    for( auto const & profile : profiles )
    {
        w_stream = std::max( profile.stream_name().length(), w_stream );
        w_format = std::max( strlen( rs2_format_to_string( profile.format() ) ), w_format );
        if( auto video = profile.as< video_stream_profile >() )
            video_stream = true;
    }
    w_stream += 2;
    w_format += 2;

    // Heading
    if( verbose )
        cout << "    (UID.IDX) ";
    else
        cout << "    ";
    cout << setw( w_stream ) << "STREAM";
    if( video_stream )
        cout << setw( w_res ) << "RESOLUTION";
    cout << setw( w_format ) << "FORMAT";
    cout << "FPS";
    cout << endl;
    // Show which streams are supported by this device
    if( verbose )
    {
        for( auto const & profile : profiles )
        {
            cout << ( show_defaults && profile.is_default() ? " +  " : "    " );
            cout << setw( 4 ) << right << ( " (" + std::to_string( profile.unique_id() ) );
            cout << '.';
            cout << setw( 5 ) << left << ( std::to_string( profile.stream_index() ) + ')' );
            cout << setw( w_stream ) << profile.stream_name();
            if( auto video = profile.as< video_stream_profile >() )
            {
                cout << setw( 4 ) << right << video.width();
                cout << 'x';
                cout << setw( w_res - 5 ) << left << video.height();
            }
            cout << setw( w_format ) << profile.format();
            cout << "@ " << profile.fps() << " Hz";
            cout << endl;
        }
    }
    else
    {
        std::ostringstream ss;
        int p_width = 0, p_height = 0, pp_w = 0, pp_h = 0;
        rs2_format p_format = RS2_FORMAT_ANY;
        std::string p_stream_name;
        bool p_default = false;
        auto print_last_stream = [&]()
        {
            cout << ( p_default ? " +  " : "    " );
            p_default = false;
            cout << setw( w_stream ) << p_stream_name;
            if( p_width || p_height )
            {
                cout << setw( w_res );
                if( p_width == pp_w && p_height == pp_h )
                    cout << "    |";
                else
                {
                    cout << setw( 4 ) << right << p_width;
                    cout << 'x';
                    cout << setw( w_res - 5 ) << left << p_height;
                    pp_w = p_width;
                    pp_h = p_height;
                }
            }
            cout << setw( w_format ) << p_format;
            cout << "@ " << ss.str() << " Hz";
            cout << endl;
        };
        for( auto && profile : profiles )
        {
            std::string stream_name = profile.stream_name();
            int w = 0, h = 0;
            if( auto video = profile.as< video_stream_profile >() )
            {
                w = video.width();
                h = video.height();
            }
            if( stream_name != p_stream_name || profile.format() != p_format || w != p_width || h != p_height )
            {
                if( ! ss.str().empty() )
                {
                    print_last_stream();
                    ss.str( std::string() );
                }
                p_stream_name = stream_name;
                p_format = profile.format();
                p_width = w;
                p_height = h;
            }
            else
            {
                ss << '/';
            }
            if( profile.is_default() && show_defaults )
                ss << "+", p_default = true;
            ss << profile.fps();
        }
        print_last_stream();
    }
}


int main(int argc, char** argv) try
{
    cli cmd( "librealsense rs-enumerate-devices tool" );

    cli::flag short_view_arg('s', "short", "Provide a one-line summary of the devices");
    cli::flag compact_view_arg('S', "compact", "Provide a short summary of the devices");
    cli::flag show_options_arg('o', "option", "Show all the supported options per subdevice");
    cli::flag show_calibration_data_arg( 'c', "calib_data", "Show extrinsic and intrinsic of all subdevices" );
    cli::flag show_defaults('d', "defaults", "Show the default streams configuration");
    cli::flag only_sw_arg( "sw-only", "Show only software devices (playback, DDS, etc. -- but not USB/HID/etc.)" );
    cli::value<string> format_arg( "format", "raw/basic/FULL", "full", "Choose which 'format-conversion' to use" );
    cli::flag verbose_arg( 'v', "verbose", "Show extra information" );
    cli::value<string> show_playback_device_arg('p', "playback_device", "path", "", "Inspect and enumerate playback device (from file)" );

    cmd.add(short_view_arg);
    cmd.add(compact_view_arg);
    cmd.add(show_options_arg);
    cmd.add(show_calibration_data_arg);
    cmd.add(only_sw_arg);
    cmd.add(format_arg);
    cmd.add(verbose_arg);
    cmd.add(show_defaults);
    cmd.add(show_playback_device_arg);

    auto settings = cmd.process( argc, argv );

    bool short_view = short_view_arg.getValue();
    bool compact_view = compact_view_arg.getValue();
    bool show_options = show_options_arg.getValue();
    bool show_calibration_data = show_calibration_data_arg.getValue();
    bool show_modes = !(compact_view || short_view);
    auto playback_dev_file = show_playback_device_arg.getValue();
    bool verbose = verbose_arg.getValue();
    bool show_dds = !cmd.no_eth_arg.getValue();

    if ((short_view || compact_view) && (show_options || show_calibration_data))
    {
        std::stringstream s;
        s << "rs-enumerate-devices ";
        for (int i = 1; i < argc; ++i)
            s << argv[i] << ' ';
        std::cout << "Warning: The command line \"" << s.str().c_str()
            <<"\" has conflicting requests.\n The flags \"-s\" / \"-S\" are compatible with \"-p\" only,"
            << " other options will be ignored.\nRefer to the tool's documentation for details\n" << std::endl;
    }

    // Obtain a list of devices currently present on the system
    if( format_arg.isSet() )
        settings["format-conversion"] = format_arg.getValue();

    settings["dds"]["enabled"] = show_dds;

    context ctx( settings.dump() );
    
    rs2::device d;
    if (!playback_dev_file.empty())
        d = ctx.load_device(playback_dev_file.data());

    int mask = RS2_PRODUCT_LINE_ANY_INTEL;
    if( only_sw_arg.getValue() )
        mask |= RS2_PRODUCT_LINE_SW_ONLY;
    auto devices_list = ctx.query_devices( mask );
    if( only_sw_arg.getValue() )
    {
        // For SW-only devices, allow some time for DDS devices to connect
        int tries = 3;
        cout << "No device detected. Waiting..." << flush;
        while( ! devices_list.size() && tries-- )
        {
            cout << "." << flush;
            std::this_thread::sleep_for( std::chrono::seconds( 1 ) );
            devices_list = ctx.query_devices( mask );
        }
        cout << endl;
    }

    // Retrieve the viable devices
    std::vector<rs2::device> devices;

    for ( auto i = 0u; i < devices_list.size(); i++)
    {
        try
        {
            auto dev = devices_list[i];
            devices.emplace_back(dev);
        }
        catch (const std::exception & e)
        {
            std::cout << "Could not create device - " << e.what() <<" . Check SDK logs for details" << std::endl;
        }
        catch(...)
        {
            std::cout << "Failed to created device. Check SDK logs for details" << std::endl;
        }
    }

    size_t device_count = devices.size();
    if( !device_count )
    {
        cout << "No device detected. Is it plugged in?\n";
        return EXIT_FAILURE;
    }

    if (short_view || compact_view)
    {
        auto dev_info = []( rs2::device dev, rs2_camera_info info )
        {
            if( dev.supports( info ) )
                return dev.get_info( info );
            return "N/A";
        };

        size_t w_name = 28;
        size_t w_sn = 18;
        for( auto i = 0u; i < devices.size(); ++i )
        {
            auto dev = devices[i];

            w_name = std::max( strlen( dev_info( dev, RS2_CAMERA_INFO_NAME ) ), w_name );
            w_sn = std::max( strlen( dev_info( dev, RS2_CAMERA_INFO_SERIAL_NUMBER ) ), w_sn );
        }
        w_name += 2;
        w_sn += 2;

        cout << left << setw(w_name) << "Device Name"
             << setw(w_sn) << "Serial Number"
             << "Firmware Version"
             << endl;

        for (auto i = 0u; i < devices.size(); ++i)
        {
            auto dev = devices[i];

            cout << left << setw(w_name) << dev_info(dev,RS2_CAMERA_INFO_NAME)
                << setw(w_sn) << dev_info(dev,RS2_CAMERA_INFO_SERIAL_NUMBER)
                << dev_info(dev,RS2_CAMERA_INFO_FIRMWARE_VERSION)
                << endl;
        }

        show_options = show_calibration_data = show_modes = false;
    }

    for (auto i = 0u; i < devices.size(); ++i)
    {

        auto dev = devices[i];

        if (!short_view)
        {
            // Show which options are supported by this device
            cout << "Device info: \n";
            if (auto pb_dev = dev.as<playback>())
                cout << "Playback Device (" << pb_dev.file_name() << ")" << std::endl;

            for (auto j = 0; j < RS2_CAMERA_INFO_COUNT; ++j)
            {
                auto param = static_cast<rs2_camera_info>(j);
                if (dev.supports(param))
                    cout << "    " << left << setw(30) << rs2_camera_info_to_string(rs2_camera_info(param))
                    << ": \t" << dev.get_info(param) << endl;
            }

            cout << endl;
        }

        if( show_defaults.getValue() && ! show_modes )
        {
            cout << "Default streams:" << endl;
            for( auto && sensor : dev.query_sensors() )
            {
                for( auto const & sp : sensor.get_stream_profiles() )
                {
                    if( sp.is_default() )
                    {
                        cout << "    " << sp.stream_name() << " as " << sp.format() << " at " << sp.fps() << " Hz";
                        if( auto vsp = sp.as<video_stream_profile>() )
                        {
                            cout << "; Resolution: " << vsp.width() << "x" << vsp.height();
                        }
                        cout << endl;
                    }
                }
            }
            cout << endl;
        }

        if (show_options)
        {
            for (auto&& sensor : dev.query_sensors())
            {
                cout << "Options for " << sensor.get_info(RS2_CAMERA_INFO_NAME) << endl;

                cout << setw(35) << " Supported options:" << setw(10) << "min" << setw(10)
                    << " max" << setw(6) << " step" << setw(10) << " default" << endl;
                for (auto j = 0; j < RS2_OPTION_COUNT; ++j)
                {
                    auto opt = static_cast<rs2_option>(j);
                    if (sensor.supports(opt))
                    {
                        try
                        {
                            auto range = sensor.get_option_range(opt);
                            cout << "    " << left << setw(30) << opt << " : "
                                << setw(5) << range.min << "... " << setw(12) << range.max
                                << setw(6) << range.step << setw(10) << range.def << "\n";
                        }
                        catch (const error & e)
                        {
                            cerr << "RealSense error calling " << e.get_failed_function() << "(" << e.get_failed_args() << "):\n    " << e.what() << endl;
                        }
                    }
                }
                cout << endl;
            }
        }

        if (show_modes)
        {
            for( auto&& sensor : dev.query_sensors() )
            {
                cout << "Stream Profiles supported by " << sensor.get_info( RS2_CAMERA_INFO_NAME ) << endl;
                
                cout << " Supported modes:\n";
                output_modes( sensor.get_stream_profiles(), verbose, show_defaults.getValue() );
                cout << endl;

                if( auto ds = debug_stream_sensor( sensor ) )
                {
                    auto debug_profiles = ds.get_debug_stream_profiles();
                    if( !debug_profiles.empty() )
                    {
                        cout << " Supported debug modes:\n";
                        output_modes( debug_profiles, verbose, show_defaults.getValue() );
                        cout << endl;
                    }
                }
            }
        }

        // Print Intrinsics
        if (show_calibration_data)
        {
            std::map<stream_and_index, stream_profile> streams;
            std::map<stream_and_resolution, std::vector<std::pair<std::set<rs2_format>, rs2_intrinsics>>> intrinsics_map;
            std::map<stream_and_resolution, std::vector<std::pair<std::set<rs2_format>, rs2_motion_device_intrinsic>>> motion_intrinsics_map;
            for (auto&& sensor : dev.query_sensors())
            {
                // Intrinsics
                for (auto&& profile : sensor.get_stream_profiles())
                {
                    if (auto video = profile.as<video_stream_profile>())
                    {
                        if (streams.find(stream_and_index{ profile.stream_type(), profile.stream_index() }) == streams.end())
                        {
                            streams[stream_and_index{ profile.stream_type(), profile.stream_index() }] = profile;
                        }

                        rs2_intrinsics intrinsics{};
                        stream_and_resolution stream_res{ profile.stream_type(), profile.stream_index(), video.width(), video.height(), profile.stream_name() };
                        if (safe_get_intrinsics(video, intrinsics))
                        {
                            auto it = std::find_if((intrinsics_map[stream_res]).begin(), (intrinsics_map[stream_res]).end(), [&](const std::pair<std::set<rs2_format>, rs2_intrinsics>& kvp) {
                                return intrinsics == kvp.second;
                                });
                            if (it == (intrinsics_map[stream_res]).end())
                            {
                                (intrinsics_map[stream_res]).push_back({ {profile.format()}, intrinsics });
                            }
                            else
                            {
                                it->first.insert(profile.format()); // If the intrinsics are equals, add the profile format to format set
                            }
                        }
                    }
                    else
                    {
                        if (motion_stream_profile motion = profile.as<motion_stream_profile>())
                        {
                            if (streams.find(stream_and_index{ profile.stream_type(), profile.stream_index() }) == streams.end())
                            {
                                streams[stream_and_index{ profile.stream_type(), profile.stream_index() }] = profile;
                            }

                            rs2_motion_device_intrinsic motion_intrinsics{};
                            stream_and_resolution stream_res{ profile.stream_type(), profile.stream_index(), motion.stream_type(), motion.stream_index(), profile.stream_name() };
                            if (safe_get_motion_intrinsics(motion, motion_intrinsics))
                            {
                                auto it = std::find_if((motion_intrinsics_map[stream_res]).begin(), (motion_intrinsics_map[stream_res]).end(),
                                    [&](const std::pair<std::set<rs2_format>, rs2_motion_device_intrinsic>& kvp)
                                    {
                                        return motion_intrinsics == kvp.second;
                                    });
                                if (it == (motion_intrinsics_map[stream_res]).end())
                                {
                                    (motion_intrinsics_map[stream_res]).push_back({ {profile.format()}, motion_intrinsics });
                                }
                                else
                                {
                                    it->first.insert(profile.format()); // If the intrinsics are equals, add the profile format to format set
                                }
                            }
                        }
                        else
                        {
                            if (pose_stream_profile pose = profile.as<pose_stream_profile>())
                            {
                                if (streams.find(stream_and_index{ profile.stream_type(), profile.stream_index() }) == streams.end())
                                {
                                    streams[stream_and_index{ profile.stream_type(), profile.stream_index() }] = profile;
                                }
                            }
                            else
                            {
                                cout << " Unhandled profile encountered: " << profile.stream_name() << "/" << profile.format() << std::endl;
                            }
                        }
                    }
                }
            }

            if (intrinsics_map.size())
            {
                cout << "Intrinsic Parameters:\n" << endl;
                for (auto& kvp : intrinsics_map)
                {
                    auto stream_res = kvp.first;
                    for (auto& intrinsics : kvp.second)
                    {
                        auto formats = "{" + get_str_formats(intrinsics.first) + "}";
                        cout << " Intrinsic of \"" << stream_res.stream_name << "\" / " << stream_res.width << "x"
                            << stream_res.height << " / " << formats << endl;
                        if (intrinsics.second == rs2_intrinsics{})
                        {
                            cout << "Intrinsic NOT available!\n\n";
                        }
                        else
                        {
                            print(intrinsics.second);
                        }
                    }
                }
            }

            if (motion_intrinsics_map.size())
            {
                cout << "Motion Intrinsic Parameters:\n" << endl;
                for (auto& kvp : motion_intrinsics_map)
                {
                    auto stream_res = kvp.first;
                    for (auto& intrinsics : kvp.second)
                    {
                        auto formats = get_str_formats(intrinsics.first);
                        cout << "Motion Intrinsic of \"" << stream_res.stream_name << "\"\t  " << formats << endl;
                        if (intrinsics.second == rs2_motion_device_intrinsic{})
                        {
                            cout << "Intrinsic NOT available!\n\n";
                        }
                        else
                        {
                            print(intrinsics.second);
                        }
                    }
                }
            }

            if (streams.size())
            {
                // Print Extrinsics
                cout << "\nExtrinsic Parameters:" << endl;
                rs2_extrinsics extrinsics{};
                for (auto kvp1 = streams.begin(); kvp1 != streams.end(); ++kvp1)
                {
                    for (auto kvp2 = streams.begin(); kvp2 != streams.end(); ++kvp2)
                    {
                        cout << "Extrinsic from \"" << kvp1->second.stream_name() << "\"\t  " <<
                            "To" << "\t  \"" << kvp2->second.stream_name() << "\" :\n";
                        try
                        {
                            extrinsics = kvp1->second.get_extrinsics_to(kvp2->second);
                            print(extrinsics);
                        }
                        catch (...)
                        {
                            cout << "N/A\n";
                        }
                    }
                }
            }
        }
    }

    return EXIT_SUCCESS;
}
catch (const error & e)
{
    cerr << "RealSense error calling " << e.get_failed_function() << "(" << e.get_failed_args() << "):\n    " << e.what() << endl;
    return EXIT_FAILURE;
}
catch( const exception & e )
{
    cerr << e.what() << endl;
    return EXIT_FAILURE;
}
catch( ... )
{
    cerr << "some error" << endl;
    return EXIT_FAILURE;
}
