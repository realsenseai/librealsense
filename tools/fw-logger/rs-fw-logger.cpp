// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2015 RealSense, Inc. All Rights Reserved.

#include <librealsense2/rs.hpp>
#include <librealsense2/hpp/rs_internal.hpp>

#include <common/cli.h>

#include <rsutils/string/string-utilities.h>
#include <fstream>
#include <thread>


using namespace std;


string datetime_string()
{
    auto t = time(nullptr);
    char buffer[20] = {};
    const tm* time = localtime(&t);
    if (nullptr != time)
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", time);

    return string(buffer);
}

int main(int argc, char* argv[]) try
{
    int default_polling_interval_ms = 100;
    using cli = rs2::cli_no_dds;
    cli cmd( "librealsense rs-fw-logger example tool" );
    cli::value<string> sn_arg('s', "sn", "serial-number", "", "Camera serial number");
    cli::value<string> xml_arg('l', "load", "path", "", "Full HW Logger Events XML file path");
    cli::value<string> out_arg('o', "out", "path", "", "Full output file path");
    cli::value<int> polling_interval_arg('p', "polling_interval", "milliseconds", default_polling_interval_ms, "Time interval between each log messages polling (in milliseconds)");
    cli::flag flash_logs_arg( 'f', "flash", "Flash Logs Request" );
    cmd.add(sn_arg);
    cmd.add(xml_arg);
    cmd.add(out_arg);
    cmd.add(polling_interval_arg);
    cmd.add(flash_logs_arg);
    auto settings = cmd.process(argc, argv);

    rs2::log_to_file(RS2_LOG_SEVERITY_WARN, "librealsense.log");

    auto use_xml_file = false;
    auto output_file_path = out_arg.getValue();
    std::ofstream output_file(output_file_path);
    // write to file if it is open, else write to console
    std::ostream& out = (!output_file.is_open() ? std::cout : output_file);

    auto sn = sn_arg.getValue();
    auto xml_full_file_path = xml_arg.getValue();
    auto polling_interval_ms = polling_interval_arg.getValue();
    if (polling_interval_ms < 25 || polling_interval_ms > 300)
    {
        out << "Polling interval time provided: " << polling_interval_ms << "ms, is not in the valid range [25,300]. Default value " << default_polling_interval_ms << "ms is used." << std::endl;
        polling_interval_ms = default_polling_interval_ms;
    }

    bool are_flash_logs_requested = flash_logs_arg.isSet();

    rs2::context ctx( settings.dump() );
    rs2::device_hub hub(ctx);

    bool should_loop_end = false;

    while (!should_loop_end)
    {
        try
        {
            rs2::device dev;
            bool found = false;

            while(!found)
            {
                auto devs = ctx.query_devices();
                out << "\n" << devs.size() << " realSense devices detected.";

                for (rs2::device d : devs)
                {
                    string serial = d.get_info(RS2_CAMERA_INFO_SERIAL_NUMBER);

                    if (sn.empty() || (!sn.empty() && serial.compare(sn) == 0))
                    {
                        dev = d;
                        found = true;
                        break;
                    }
                }

                out << "\nWaiting for RealSense device " << sn << (!sn.empty() ? " " : "") << "to connect ...";
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            }

            string serial = dev.get_info(RS2_CAMERA_INFO_SERIAL_NUMBER);
            out << "\nRealSense device " << serial << " was connected for logging.\n";

            setvbuf(stdout, NULL, _IONBF, 0); // unbuffering stdout

            // Not every device exposes firmware logging (e.g. DDS/Ethernet
            // devices such as the D555). Fail with a clear message instead of
            // dereferencing a null firmware_logger handle downstream.
            if (!dev.is<rs2::firmware_logger>())
            {
                cerr << "\nDevice " << serial
                     << " does not support firmware logging (RS2_EXTENSION_FW_LOGGER)." << endl;
                return EXIT_FAILURE;
            }

            auto fw_log_device = dev.as<rs2::firmware_logger>();

            bool using_parser = false;
            if (!xml_full_file_path.empty())
            {
                ifstream f(xml_full_file_path);
                if (f.good())
                {
                    std::string xml_content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
                    bool parser_initialized = fw_log_device.init_parser(xml_content);
                    if (parser_initialized)
                        using_parser = true;
                }
            }

            fw_log_device.start_collecting();

            bool are_there_remaining_flash_logs_to_pull = true;
            auto time_of_previous_polling_ms = std::chrono::high_resolution_clock::now();

            while (hub.is_connected(dev))
            {
                if (are_flash_logs_requested && !are_there_remaining_flash_logs_to_pull)
                {
                    should_loop_end = true;
                    break;
                }
                auto log_message = fw_log_device.create_message();
                bool result = false;
                if (are_flash_logs_requested)
                {
                    result = fw_log_device.get_flash_log(log_message);
                }
                else
                {
                    result = fw_log_device.get_firmware_log(log_message);
                }
                if (result)
                {
                    std::vector<string> fw_log_lines;
                    if (using_parser)
                    {
                        auto parsed_log = fw_log_device.create_parsed_message();
                        bool parsing_result = fw_log_device.parse_log(log_message, parsed_log);
                        
                        std::string module_print = parsed_log.module_name() + " ";
                        if( module_print == "Unknown " )
                            module_print.clear();  // Some devices don't support FW log modules

                        stringstream sstr;
                        sstr << datetime_string() << " " << parsed_log.timestamp() << " " << parsed_log.sequence_id()
                             << " " << parsed_log.severity() << " " << parsed_log.thread_name() << " " << module_print
                             << parsed_log.file_name() << " " << parsed_log.line() << " " << parsed_log.message();
                        
                        fw_log_lines.push_back(sstr.str());
                    }
                    else
                    {
                        stringstream sstr;
                        sstr << datetime_string() << "  FW_Log_Data:";
                        std::vector<uint8_t> msg_data = log_message.data();
                        for (int i = 0; i < msg_data.size(); ++i)
                        {
                        sstr << rsutils::string::hexify(msg_data[i]) << " ";
                        }
                        fw_log_lines.push_back(sstr.str());
                    }
                    for (auto& line : fw_log_lines)
                        out << line << endl;
                }
                else
                {
                    if (are_flash_logs_requested)
                    {
                        are_there_remaining_flash_logs_to_pull = false;
                    }
                }
                auto num_of_messages = fw_log_device.get_number_of_fw_logs();
                if (num_of_messages == 0)
                {
                    auto current_time = std::chrono::high_resolution_clock::now();
                    auto time_since_previous_polling_ms = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - time_of_previous_polling_ms).count();
                    if (time_since_previous_polling_ms < polling_interval_ms)
                    {
                        std::this_thread::sleep_for(chrono::milliseconds(polling_interval_ms - time_since_previous_polling_ms));
                    }
                    time_of_previous_polling_ms = std::chrono::high_resolution_clock::now();
                }
            }

            fw_log_device.stop_collecting();
        }
        catch (const rs2::error & e)
        {
            cerr << "RealSense error calling " << e.get_failed_function() << "(" << e.get_failed_args() << "):\n    " << e.what() << endl;
        }
    }

    return EXIT_SUCCESS;
}
catch( const rs2::error & e )
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
