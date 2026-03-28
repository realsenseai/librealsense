// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2018-25 RealSense, Inc. All Rights Reserved.

#include <iostream>

#include "librealsense2/rs.hpp"

#include <common/cli.h>

#include "converters/converter-csv.hpp"
#include "converters/converter-3d-csv.hpp"
#include "converters/converter-png.hpp"
#include "converters/converter-raw.hpp"
#include "converters/converter-ply.hpp"
#include "converters/converter-bin.hpp"
#include "converters/converter-text.hpp"

#include <mutex>

#define SECONDS_TO_NANOSECONDS 1000000000
 
using namespace std;


int main(int argc, char** argv) try
{
    // Parse command line arguments
    using cli = rs2::cli;

    cli::value<string> inputFilename('i', "input", "ros-bag-file", "", "ROS-bag filename", cli::required );
    cli::value<string> outputFilenamePng('p', "output-png", "png-path", "", "output PNG file(s) path");
    cli::value<string> outputFilenameCsv('v', "output-csv", "csv-path", "", "output CSV (depth matrix) file(s) path");
    cli::value<string> outputFilename3dCsv('V', "output-3d-csv", "3d-csv-path", "", "output 3d CSV (depth matrix) file(s) path");
    cli::value<string> outputFilenameRaw('r', "output-raw", "raw-path", "", "output RAW file(s) path");
    cli::value<string> outputFilenamePly('l', "output-ply", "ply-path", "", "output PLY file(s) path");
    cli::value<string> outputFilenameBin('b', "output-bin", "bin-path", "", "output BIN (depth matrix) file(s) path");
    cli::flag switchDepth( 'd', "depth", "convert depth frames (default - all supported)" );
    cli::flag switchColor( 'c', "color", "convert color frames (default - all supported)" );
    cli::flag switchTextOutput( 'T', "output-text", "output text to stdout" );
    cli::value <string> frameNumberStart('f', "first-framenumber", "frame", "", "ignore frames whose frame number is less than this value");
    cli::value <string> frameNumberEnd('t', "last-framenumber", "frame", "", "ignore frames whose frame number is greater than this value");
    cli::value <string> startTime('s', "start-time", "seconds", "", "ignore frames whose timestamp is less than this value (the first frame is at time 0)");
    cli::value <string> endTime('e', "end-time", "seconds", "", "ignore frames whose timestamp is greater than this value (the first frame is at time 0)" );

    auto settings = cli( "librealsense rs-convert tool" )
                        .default_log_level( RS2_LOG_SEVERITY_WARN )
                        .arg( inputFilename )
                        .arg( frameNumberEnd )
                        .arg( frameNumberStart )
                        .arg( endTime )
                        .arg( startTime )
                        .arg( outputFilenamePng )
                        .arg( outputFilenameCsv )
                        .arg( outputFilename3dCsv )
                        .arg( outputFilenameRaw )
                        .arg( outputFilenamePly )
                        .arg( outputFilenameBin )
                        .arg( switchDepth )
                        .arg( switchColor )
                        .arg( switchTextOutput )
                        .process( argc, argv );

    vector<shared_ptr<rs2::tools::converter::converter_base>> converters;
    shared_ptr<rs2::tools::converter::converter_ply> plyconverter;

    rs2_stream streamType = switchDepth.isSet() ? rs2_stream::RS2_STREAM_DEPTH
        : switchColor.isSet() ? rs2_stream::RS2_STREAM_COLOR
        : rs2_stream::RS2_STREAM_ANY;

    if (outputFilenameCsv.isSet())
    {
        converters.push_back(
            make_shared<rs2::tools::converter::converter_csv>(
                outputFilenameCsv.getValue()
                , streamType));
    }

    if (outputFilename3dCsv.isSet())
    {
        converters.push_back(
            make_shared<rs2::tools::converter::converter_3d_csv>(
                outputFilename3dCsv.getValue()
                , streamType));
    }

    if (outputFilenamePng.isSet())
    {
        converters.push_back(
            make_shared<rs2::tools::converter::converter_png>(
                outputFilenamePng.getValue()
                , streamType));
    }

    if (outputFilenameRaw.isSet())
    {
        converters.push_back(
            make_shared<rs2::tools::converter::converter_raw>(
                outputFilenameRaw.getValue()
                , streamType));
    }

    if (outputFilenameBin.isSet())
    {
        converters.push_back(
            make_shared<rs2::tools::converter::converter_bin>(
                outputFilenameBin.getValue()));
    }

    if( switchTextOutput.isSet() )
    {
        converters.push_back( make_shared< rs2::tools::converter::converter_text >() );
    }

    if (converters.empty() && !outputFilenamePly.isSet())
    {
        throw runtime_error("output not defined");
    }

    unsigned long long first_frame = 0;
    unsigned long long last_frame = 0;
    uint64_t start_time = 0;
    uint64_t end_time = 0;

    if (frameNumberStart.isSet())
    {
        first_frame = stoi(frameNumberStart.getValue());
    }
    if (frameNumberEnd.isSet())
    {
        last_frame = stoi(frameNumberEnd.getValue());
    }
    if (startTime.isSet())
    {
        start_time = (uint64_t) (SECONDS_TO_NANOSECONDS * (std::strtod( startTime.getValue().c_str(), nullptr )));
    }
    if (endTime.isSet())
    {
        end_time = (uint64_t) (SECONDS_TO_NANOSECONDS * (std::strtod( endTime.getValue().c_str(), nullptr )));
    }

    //in order to convert frames into ply we need synced depth and color frames, 
    //therefore we use pipeline
    if (outputFilenamePly.isSet()) {

        // Since we are running in blocking "non-real-time" mode,
        // we don't want to prevent process termination if some of the frames
        // did not find a match and hence were not serviced
        auto pipe = std::shared_ptr<rs2::pipeline>(
            new rs2::pipeline( settings.dump() ), [](rs2::pipeline*) {});

        plyconverter = make_shared<rs2::tools::converter::converter_ply>(
            outputFilenamePly.getValue());

        rs2::config cfg;
        cfg.enable_device_from_file(inputFilename.getValue());
        pipe->start(cfg);

        auto device = pipe->get_active_profile().get_device();
        rs2::playback playback = device.as<rs2::playback>();
        playback.set_real_time(false);

        auto duration = playback.get_duration();
        int progress = 0;
        auto frameNumber = 0ULL;

        rs2::frameset frameset;
        uint64_t posCurr = playback.get_position();

        // try_wait_for_frames will keep repeating the last frame at the end of the file,
        // so we need to exit the look in some other way!
        while (pipe->try_wait_for_frames(&frameset, 1000))
        {
            int posP = static_cast<int>(posCurr * 100. / duration.count());
            if (posP > progress)
            {
                progress = posP;
                cout << posP << "%" << "\r" << flush;
            }

            frameNumber = frameset[0].get_frame_number();

            bool process_frame = true;
            if (frameNumberStart.isSet() && frameNumber < first_frame)
                process_frame = false;
            else if (frameNumberEnd.isSet() && frameNumber > last_frame)
                process_frame = false;
            else if (startTime.isSet() && posCurr < start_time)
                process_frame = false;
            else if (endTime.isSet() && posCurr > end_time)
                process_frame = false;
         
            if( process_frame )
            {
                plyconverter->convert(frameset);
                plyconverter->wait();
            }

            auto posNext = playback.get_position();


            if (posNext < posCurr)
                break;

            posCurr = posNext;
        }
    }

    // for every converter other than ply,
    // we get the frames from playback sensors 
    // and convert them one by one
    if( ! converters.empty() )
    {
        rs2::context ctx;
        auto playback = ctx.load_device(inputFilename.getValue());
        playback.set_real_time(false);
        std::vector<rs2::sensor> sensors = playback.query_sensors();
        std::mutex mutex;

        auto duration = playback.get_duration();
        int progress = 0;
        uint64_t posCurr = playback.get_position();

        for (auto sensor : sensors)
        {
            if (!sensor.get_stream_profiles().size())
            {
                continue;
            }

            sensor.open(sensor.get_stream_profiles());
            sensor.start([&](rs2::frame frame)
            {
                std::lock_guard<std::mutex> lock(mutex);

                auto frameNumber = frame.get_frame_number();

                if (frameNumberStart.isSet() && frameNumber < first_frame)
                    return;
                if (frameNumberEnd.isSet() && frameNumber > last_frame)
                    return;
                if (startTime.isSet() && posCurr < start_time)
                    return;
                if (endTime.isSet() && posCurr > end_time)
                    return;

                for_each(converters.begin(), converters.end(),
                    [&frame](shared_ptr<rs2::tools::converter::converter_base>& converter) {
                    converter->convert(frame);
                });

                for_each(converters.begin(), converters.end(),
                    [](shared_ptr<rs2::tools::converter::converter_base>& converter) {
                    converter->wait();
                });
            });

        }

        //we need to clear the output of ply progress ("100%") before writing
        //the progress of the other converters in the same line
        cout << "\r    \r";

        while (true)
        {
            int posP = static_cast<int>(posCurr * 100. / duration.count());

            if (posP > progress)
            {
                progress = posP;
                if( ! switchTextOutput.isSet() )
                    cout << posP << "%" << "\r" << flush;
            }

            const uint64_t posNext = playback.get_position();
            if (posNext < posCurr)
                break;

            posCurr = posNext;
        }

        for (auto sensor : sensors)
        {
            if (!sensor.get_stream_profiles().size())
            {
                continue;
            }
            sensor.stop();
            sensor.close();
        }
    }


    for_each(converters.begin(), converters.end(),
        [](shared_ptr<rs2::tools::converter::converter_base>& converter) {
            converter->flush();
        });


    if( !switchTextOutput.isSet() )
        cout << endl;

    //print statistics for ply converter. 
    if (outputFilenamePly.isSet()) {
        cout << plyconverter->get_statistics() << endl;
    }

    if( ! switchTextOutput.isSet() )
        for_each( converters.begin(),
                  converters.end(),
                  []( shared_ptr< rs2::tools::converter::converter_base > & converter ) {
                      cout << converter->get_statistics() << endl;
                  } );


    return EXIT_SUCCESS;
}
catch (const rs2::error & e)
{
    cerr << "RealSense error calling " << e.get_failed_function()
        << "(" << e.get_failed_args() << "):\n    " << e.what() << endl;

    return EXIT_FAILURE;
}
catch (const exception & e)
{
    cerr << e.what() << endl;
    return EXIT_FAILURE;
}
catch (...)
{
    cerr << "some error" << endl;
    return EXIT_FAILURE;
}
