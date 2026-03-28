// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2015 RealSense, Inc. All Rights Reserved.

#include "rs-fw-logger.h"

#include <android/log.h>
#include <sstream>
#include <thread>
#include <chrono>
#include <fstream>

#include <rsutils/concurrency/thread-utils.h>

#include "../../../tools/fw-logger/fw-logs-parser.h"
#include "../../../include/librealsense2/hpp/rs_context.hpp"

#define TAG "rs_fw_log"

void log(std::string message)
{
    __android_log_print(ANDROID_LOG_INFO, TAG, "%s", message.c_str());
}

std::string hexify(unsigned char n)
{
    std::string res;

    do
    {
        res += "0123456789ABCDEF"[n % 16];
        n >>= 4;
    } while (n);

    std::reverse(res.begin(), res.end());

    if (res.size() == 1)
    {
        res.insert(0, "0");
    }

    return res;
}

void android_fw_logger::read_log_loop()
{
    _active = true;

    std::unique_ptr<fw_logger::fw_logs_parser> fw_log_parser;
    auto use_xml_file = false;
    if (!_xml_path.empty())
    {
        std::ifstream f(_xml_path);
        if (f.good())
        {
            fw_log_parser = std::unique_ptr<fw_logger::fw_logs_parser>(new fw_logger::fw_logs_parser(_xml_path));
            use_xml_file = true;
        }
    }

    while(_active)
    {    try
        {
            rs2::context ctx;
            auto devs = ctx.query_devices();
            if(devs.size() == 0){
                _active = false;
                break;
            }
            auto dev = ctx.query_devices()[0];

            std::vector<uint8_t> input;
            auto str_op_code = dev.get_info(RS2_CAMERA_INFO_DEBUG_OP_CODE);
            auto op_code = static_cast<uint8_t>(std::stoi(str_op_code));
            input = {0x14, 0x00, 0xab, 0xcd, op_code, 0x00, 0x00, 0x00,
                     0xf4, 0x01, 0x00, 0x00, 0x00,    0x00, 0x00, 0x00,
                     0x00, 0x00, 0x00, 0x00, 0x00,    0x00, 0x00, 0x00};

            std::stringstream dev_info;
            dev_info << "Device Name: " << dev.get_info(RS2_CAMERA_INFO_NAME) << "\nDevice Location: " << dev.get_info(RS2_CAMERA_INFO_PHYSICAL_PORT) << "\n\n";
            log(dev_info.str());

            while (_active)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(_sample_rate));

                auto raw_data = dev.as<rs2::debug_protocol>().send_and_receive_raw_data(input);
                std::vector<std::string> fw_log_lines = {""};
                if (raw_data.size() <= 4)
                    continue;

                if (use_xml_file)
                {
                    fw_logger::fw_logs_binary_data fw_logs_binary_data = {raw_data};
                    fw_logs_binary_data.logs_buffer.erase(fw_logs_binary_data.logs_buffer.begin(),fw_logs_binary_data.logs_buffer.begin()+4);
                    fw_log_lines = fw_log_parser->get_fw_log_lines(fw_logs_binary_data);
                }
                else
                {
                    std::stringstream sstr;
                    sstr << "FW_Log_Data:";
                    for (size_t i = 0; i < raw_data.size(); ++i)
                        sstr << hexify(raw_data[i]) << " ";

                    fw_log_lines.push_back(sstr.str());
                }

                for (auto& line : fw_log_lines)
                    log(line);

            }
        }
        catch (const rs2::error & e)
        {
            std::stringstream cerr;
            cerr << "RealSense error calling " << e.get_failed_function() << "(" << e.get_failed_args() << "):\n    " << e.what();

            log(cerr.str());
        }
    }
}

android_fw_logger::android_fw_logger(std::string xml_path, int sample_rate) : _xml_path(xml_path), _sample_rate(sample_rate)
{
    log("StartReadingFwLogs");
    _thread = rsutils::concurrency::create_thread(
        rsutils::concurrency::thread_category_utility, "fw-logger",
        [this]() { read_log_loop(); });
}

android_fw_logger::~android_fw_logger()
{
    log("StopReadingFwLogs");
    _active = false;
    _thread.join();
}
