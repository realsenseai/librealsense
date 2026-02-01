// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2024 RealSense, Inc. All Rights Reserved.

#include <librealsense2/rs.hpp>

#include <rsutils/json.h>
#include <vector>
#include <map>
#include <string>
#include <cstring>
#include <iostream>
#include <fstream>
#include <thread>
#include <mutex>
#include <chrono>
#include <condition_variable>

#include <common/cli.h>

#define WAIT_FOR_DEVICE_TIMEOUT 15

#if _WIN32
#include <io.h>
#define ISATTY _isatty
#define FILENO _fileno
#else
#include <unistd.h>
#define ISATTY isatty
#define FILENO fileno
#endif

using rsutils::json;

std::condition_variable cv;
std::mutex mutex;
std::string selected_serial_number;

rs2::device new_device;
rs2::update_device new_fw_update_device;

bool done = false;

std::vector<uint8_t> read_fw_file(std::string file_path)
{
    std::vector<uint8_t> rv;

    std::ifstream file(file_path, std::ios::in | std::ios::binary | std::ios::ate);
    if (file.is_open())
    {
        rv.resize(file.tellg());

        try
        {
            file.seekg(0, std::ios::beg);
            file.read((char*)rv.data(), rv.size());
        }
        catch (...)
        {
            // Nothing to do, file goodbit is false
        }
        if (!file.good())
        {
            std::cout << std::endl << "Error reading firmware file";
            rv.resize(0); // Signal error, don't use partial read data
        }

    }

    return rv;
}

void print_device_info(rs2::device d)
{
    std::map<rs2_camera_info, std::string> camera_info;

    for (int i = 0; i < RS2_CAMERA_INFO_COUNT; i++)
    {
        auto info = (rs2_camera_info)i;
        camera_info[info] = d.supports(info) ? d.get_info(info) : "unknown";
    }

    std::cout << d.get_description() <<
        ", update serial number: " << camera_info[RS2_CAMERA_INFO_FIRMWARE_UPDATE_ID] <<
        ", firmware version: " << camera_info[RS2_CAMERA_INFO_FIRMWARE_VERSION];

    if (d.supports(RS2_CAMERA_INFO_SMCU_FW_VERSION))
        std::cout << ", SMCU firmware version: " << camera_info[RS2_CAMERA_INFO_SMCU_FW_VERSION];

    std::cout << std::endl;
}

std::vector<uint8_t> read_firmware_data(bool is_set, const std::string& file_path)
{
    if (!is_set)
    {
        throw rs2::error("Firmware file must be selected");
    }

    std::vector<uint8_t> fw_image = read_fw_file(file_path);

    if (fw_image.size() == 0)
    {
        throw rs2::error("Failed to read firmware file");
    }

    return fw_image;
}


void update(rs2::update_device fwu_dev, std::vector< uint8_t > const& fw_image)
{
    std::cout << std::endl << "Firmware update started. Please don't disconnect device!" << std::endl << std::endl;

    if (ISATTY(FILENO(stdout)))
    {
        fwu_dev.update(fw_image, [&](const float progress)
            {
                printf("\rFirmware update progress: %d[%%]", (int)(progress * 100));
                std::cout.flush();
            });
    }
    else
    {
        fwu_dev.update(fw_image, [&](const float progress) {});
    }
    std::cout << std::endl << std::endl << "Firmware update done" << std::endl;
}

void list_devices(rs2::context ctx)
{
    auto devs = ctx.query_devices();
    if (devs.size() == 0)
    {
        std::cout << std::endl << "There are no connected devices" << std::endl;
        return;
    }

    std::cout << std::endl << "Connected devices:" << std::endl;


    int counter = 0;
    for (auto&& d : devs)
    {
        std::cout << ++counter << ") ";
        print_device_info(d);
    }
}


void waiting_for_device_to_reconnect(rs2::context& ctx, rs2::cli::value<std::string>& serial_number_arg)
{
    std::cout << std::endl << "Waiting for device to reconnect..." << std::endl;
    std::unique_lock<std::mutex> lk(mutex);
    cv.wait_for(lk, std::chrono::seconds(WAIT_FOR_DEVICE_TIMEOUT), [&] { return !done || new_device; });

    if (done)
    {
        auto devs = ctx.query_devices();
        for (auto&& d : devs)
        {
            auto sn = d.supports(RS2_CAMERA_INFO_SERIAL_NUMBER) ? d.get_info(RS2_CAMERA_INFO_SERIAL_NUMBER) : "unknown";
            if (serial_number_arg.isSet() && sn != selected_serial_number)
                continue;

            auto fw = d.supports(RS2_CAMERA_INFO_FIRMWARE_VERSION) ? d.get_info(RS2_CAMERA_INFO_FIRMWARE_VERSION) : "unknown";
            std::cout << std::endl << "Device " << sn << " successfully updated to FW: " << fw << std::endl;
        }
    }

}

bool is_fw_compatible(const rs2::device& dev, const std::vector< uint8_t >& fw_image)
{
    auto upd = dev.as<rs2::updatable>();
    if ( !upd )
    {
        throw std::runtime_error("Device could not be used as updatable device");
    }
    // checking compatibility bewtween firmware and device
    if ( !upd.check_firmware_compatibility( fw_image ) )
    {
        std::stringstream ss;
        ss << "This firmware version is not compatible with ";
        ss << dev.get_info( RS2_CAMERA_INFO_NAME ) << std::endl;
        std::cout << std::endl << ss.str() << std::endl;
        return false;
    }
    return true;
}

bool is_mipi_device(const rs2::device& dev)
{
    bool is_mipi_device = false;
    if (dev.supports(RS2_CAMERA_INFO_CONNECTION_TYPE))
    {
        std::string connection_type = dev.get_info(RS2_CAMERA_INFO_CONNECTION_TYPE);
        if (connection_type == "GMSL")
            is_mipi_device = true;
    }
    return is_mipi_device;
}

int update_recovery_device(rs2::context& ctx, rs2::cli::value<std::string>& file_arg)
{
    std::vector<uint8_t> fw_image = read_firmware_data(file_arg.isSet(), file_arg.getValue());

    std::cout << std::endl << "Update to FW: " << file_arg.getValue() << std::endl;
    auto devs = ctx.query_devices();
    rs2::device recovery_device;

    for (auto&& d : devs)
    {
        if (!d.is_in_recovery_mode())
            continue;
        auto sn = d.get_info(RS2_CAMERA_INFO_FIRMWARE_UPDATE_ID);
        if (!selected_serial_number.empty() && sn != selected_serial_number)
            continue;
        if (recovery_device)
        {
            std::cout << std::endl << "More than one recovery device is connected; serial number must be specified" << std::endl << std::endl;
            return EXIT_FAILURE;
        }
        recovery_device = d;
    }
    if (!recovery_device)
    {
        std::cout << std::endl << "No recovery devices were found!" << std::endl << std::endl;
        return EXIT_FAILURE;
    }
    try
    {
        auto update_serial_number = recovery_device.get_info(RS2_CAMERA_INFO_FIRMWARE_UPDATE_ID);
        bool d457_recovery_device = strcmp(recovery_device.get_info(RS2_CAMERA_INFO_PRODUCT_ID), "BBCD") == 0;
        volatile bool recovery_device_found = false;
        ctx.set_devices_changed_callback([&](rs2::event_information& info) {
            for (auto&& d : info.get_new_devices())
            {
                if (d.is_in_recovery_mode())
                    continue;
                auto recovery_sn = d.get_info(RS2_CAMERA_INFO_FIRMWARE_UPDATE_ID);
                if (recovery_sn == update_serial_number)
                {
                    {
                        std::lock_guard< std::mutex > lk(mutex);
                        recovery_device_found = true;
                    }
                    cv.notify_one();
                    break;
                }
            }
            });
        std::cout << std::endl << "Recovering device: " << std::endl;
        print_device_info(recovery_device);

        std::string camera_name = recovery_device.get_info(RS2_CAMERA_INFO_NAME);
        // on D555 check FW compatibility also sends FW to the device
        if (camera_name.find("D555") != std::string::npos &&
            (!is_fw_compatible(recovery_device, fw_image)))
            return EXIT_FAILURE;

        update(recovery_device, fw_image);
        std::cout << "Waiting for new device..." << std::endl;
        if (!d457_recovery_device)
        {
            std::unique_lock< std::mutex > lk(mutex);
            if (!recovery_device_found
                && !cv.wait_for(lk, std::chrono::seconds(WAIT_FOR_DEVICE_TIMEOUT), [&]() {
                    return recovery_device_found;
                    }))
            {
                std::cout << "... timed out!" << std::endl;
                return EXIT_FAILURE;
            }
        }
        std::cout << std::endl << "Recovery done" << std::endl;
        if (d457_recovery_device)
        {
            std::cout << std::endl << "For GMSL device please reload d4xx driver:" << std::endl;
            std::cout << "sudo rmmod d4xx && sudo modprobe d4xx" << std::endl;
            std::cout << "or reboot the system" << std::endl;
        }
        return EXIT_SUCCESS;
    }
    catch (...)
    {
        std::cout << std::endl << "Failed to recover device" << std::endl;
        return EXIT_FAILURE;
    }
}

void backup_flash(rs2::device& d, rs2::cli::value<std::string>& backup_arg)
{
    std::cout << std::endl << "Trying to back-up device flash" << std::endl;

    std::vector< uint8_t > flash;
    if (ISATTY(FILENO(stdout)))
    {
        flash = d.as< rs2::updatable >().create_flash_backup([&](const float progress) {
            printf("\rFlash backup progress: %d[%%]", (int)(progress * 100));
            std::cout.flush();
            });
    }
    else
        flash = d.as<rs2::updatable>().create_flash_backup([&](const float progress) {});

    if (flash.empty())
    {
        std::cout << std::endl << "Backup flash is not supported";
    }
    else
    {
        auto temp = backup_arg.getValue();
        std::ofstream file(temp.c_str(), std::ios::binary);
        try
        {
            file.write((const char*)flash.data(), flash.size());
        }
        catch (...)
        {
            // Nothing to do, file goodbit is false
        }
        if (!file.good())
        {
            std::cout << std::endl << "Creating backup file failed";
        }
    }
}

void update_unsigned_fw(rs2::device& d, const std::vector<uint8_t>& fw_image)
{
    std::cout << std::endl << "Unsigned Firmware update started. Please don't disconnect device!" << std::endl << std::endl;

    if (ISATTY(FILENO(stdout)))
    {
        d.as<rs2::updatable>().update_unsigned(fw_image, [&](const float progress)
            {
                printf("\rUnsigned Firmware update progress: %d[%%]", (int)(progress * 100));
                std::cout.flush();
            });
    }
    else
        d.as<rs2::updatable>().update_unsigned(fw_image, [&](const float progress) {});

    std::cout << std::endl << std::endl << "Unsigned Firmware update done" << std::endl;
    done = true;
}

int update_signed_fw(rs2::device& d, const std::vector<uint8_t>& fw_image)
{
    if (!is_fw_compatible(d, fw_image))
        return EXIT_FAILURE;

    if (!is_mipi_device(d))
    {
        auto upd = d.as<rs2::updatable>();
        upd.enter_update_state();
    }

    // Some devices may immediately get in an update state
    // this if statement is also relevant for mipi devices
    if (d.is< rs2::update_device >())
    {
        new_fw_update_device = d;
    }
    else
    {
        std::unique_lock<std::mutex> lk(mutex);
        if (!cv.wait_for(lk, std::chrono::seconds(WAIT_FOR_DEVICE_TIMEOUT), [&] { return new_fw_update_device; }))
        {
            std::cout << std::endl << "Failed to locate a device in FW update mode" << std::endl;
            return EXIT_FAILURE;
        }
    }

    new_device = rs2::device();  // otherwise the wait will exit right away
    update(new_fw_update_device, fw_image);

    done = true;
    return EXIT_SUCCESS;
}

int main(int argc, char** argv)
try
{
    using rs2::cli;
    cli cmd("librealsense rs-fw-update tool");

    cli::flag list_devices_arg('l', "list_devices", "List all available devices");
    cli::flag recover_arg('r', "recover", "Recover all connected devices which are in recovery mode");
    cli::flag unsigned_arg('u', "unsigned", "Update unsigned firmware, available only for unlocked cameras");
    cli::value<std::string> backup_arg('b', "backup", "path", "", "Create a backup to the camera flash and saved it to the given path");
    cli::value<std::string> file_arg('f', "file", "path", "", "Path of the firmware image file");
    cli::value<std::string> serial_number_arg('s', "serial_number", "string", "", "The serial number of the device to be update, this is mandatory if more than one device is connected");

    cmd.default_log_level(RS2_LOG_SEVERITY_WARN);
    cmd.add(list_devices_arg);
    cmd.add(recover_arg);
    cmd.add(unsigned_arg);
    cmd.add(file_arg);
    cmd.add(serial_number_arg);
    cmd.add(backup_arg);

    auto settings = cmd.process(argc, argv);
    rs2::context ctx(settings.dump());

    if (!list_devices_arg.isSet() && !recover_arg.isSet() && !unsigned_arg.isSet() &&
        !backup_arg.isSet() && !file_arg.isSet() && !serial_number_arg.isSet())
    {
        std::cout << std::endl << "Nothing to do, run again with -h for help" << std::endl;
        list_devices(ctx);
        return EXIT_SUCCESS;
    }

    if (list_devices_arg.isSet())
    {
        list_devices(ctx);
        return EXIT_SUCCESS;
    }

    if (!file_arg.isSet() && !backup_arg.isSet())
    {
        std::cout << std::endl << "Nothing to do, run again with -h for help" << std::endl;
        return EXIT_FAILURE;
    }

    if (serial_number_arg.isSet())
    {
        selected_serial_number = serial_number_arg.getValue();
        std::cout << std::endl << "Search for device with serial number: " << selected_serial_number << std::endl;
    }


    std::string update_serial_number;

    // Recovery
    if (recover_arg.isSet())
    {
        update_recovery_device(ctx, file_arg);
    }

    // Update device
    ctx.set_devices_changed_callback([&](rs2::event_information& info)
        {
            if (info.get_new_devices().size() == 0)
            {
                return;
            }

            for (auto&& d : info.get_new_devices())
            {
                std::lock_guard<std::mutex> lk(mutex);
                if (d.is_in_recovery_mode() && (d.get_info(RS2_CAMERA_INFO_FIRMWARE_UPDATE_ID) == update_serial_number))
                    new_fw_update_device = d;
                else
                    new_device = d;
            }
            if (new_fw_update_device || new_device)
                cv.notify_one();
        });

    auto devs = ctx.query_devices();

    if (!serial_number_arg.isSet() && devs.size() > 1)
    {
        std::cout << std::endl << "Several devices are connected, serial number must be selected using -s <serial_number>" << std::endl;
        return EXIT_FAILURE;
    }

    if (devs.size() == 1)
    {
        auto dev = devs[0];
        if (dev.is_in_recovery_mode() && !dev.is< rs2::updatable >())
        {
            std::cout << std::endl << "Device is in recovery mode, use -r to recover" << std::endl << std::endl;
            return EXIT_FAILURE;
        }
    }

    if (devs.size() == 0)
    {
        std::cout << std::endl << "No devices were found" << std::endl << std::endl;
        return EXIT_FAILURE;
    }

    bool device_found = false;

    for (auto&& d : devs)
    {
        if (!d.is< rs2::updatable >() || !d.supports(RS2_CAMERA_INFO_FIRMWARE_UPDATE_ID))
            continue;

        if (devs.size() > 1 && selected_serial_number.empty())
        {
            std::cout << "Please use the serial number argument to specify which device needs firmware update. " << std::endl;
            return EXIT_FAILURE;
        }

        if (devs.size() != 1)
        {
            auto sn = d.get_info(d.supports(RS2_CAMERA_INFO_SERIAL_NUMBER) ? RS2_CAMERA_INFO_SERIAL_NUMBER
                : RS2_CAMERA_INFO_FIRMWARE_UPDATE_ID);
            if (sn != selected_serial_number)
                continue;
        }

        if (d.supports(RS2_CAMERA_INFO_USB_TYPE_DESCRIPTOR))
        {
            std::string usb_type = d.get_info(RS2_CAMERA_INFO_USB_TYPE_DESCRIPTOR);
            if (usb_type.find("2.") != std::string::npos) {
                std::cout << std::endl << "Warning! the camera is connected via USB 2 port, in case the process fails, connect the camera to a USB 3 port and try again" << std::endl;
            }
        }

        device_found = true;
        update_serial_number = d.get_info(RS2_CAMERA_INFO_FIRMWARE_UPDATE_ID);

        if (backup_arg.isSet())
        {
            backup_flash(d, backup_arg);

        }

        // FW DFU
        if (file_arg.isSet())
        {
            std::vector<uint8_t> fw_image = read_firmware_data(file_arg.isSet(), file_arg.getValue());

            std::cout << std::endl << "Updating device FW: " << std::endl;
            print_device_info(d);

            if (unsigned_arg.isSet())
            {
                update_unsigned_fw(d, fw_image);
            }
            else
            {
                return update_signed_fw(d, fw_image);
            }
        }
    }

    if (!device_found)
    {
        if (serial_number_arg.isSet())
            std::cout << std::endl << "Couldn't find the requested serial number" << std::endl;
        else if (devs.size() == 1)
        {
            std::cout << std::endl << "Nothing to do, run again with -h for help" << std::endl;
        }
        return EXIT_FAILURE;
    }

    waiting_for_device_to_reconnect(ctx, serial_number_arg);


    return EXIT_SUCCESS;
}
catch (const rs2::error& e)
{
    std::cerr << "RealSense error calling " << e.get_failed_function() << "(" << e.get_failed_args() << "):\n    " << e.what() << std::endl;
    return EXIT_FAILURE;
}
catch (const std::exception& e)
{
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
}
catch (...)
{
    std::cerr << "some error" << std::endl;
    return EXIT_FAILURE;
}
