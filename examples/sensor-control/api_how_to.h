// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 RealSense, Inc. All Rights Reserved.

#pragma once

#include <iostream>
#include <iomanip>
#include <map>
#include <utility>
#include <vector>
#include <librealsense2/rs.hpp>
#include <librealsense2/h/rs_decimation_filter_dpp.h>
#include <librealsense2/h/rs_temporal_filter_dpp.h>
#include <librealsense2/h/rs_hdrd_control.h>
#include "helper.h"

using namespace helper;

/**
    The how_to class provides several functions for common usages of the sensor API
*/
class how_to
{
public:
    static rs2::device get_a_realsense_device()
    {
        // First, create a rs2::context.
        // The context represents the current platform with respect to connected devices
        rs2::context ctx;

        // Using the context we can get all connected devices in a device list
        rs2::device_list devices = ctx.query_devices();

        rs2::device selected_device;
        if (devices.size() == 0)
        {
            std::cerr << "No device connected, please connect a RealSense device" << std::endl;

            //To help with the boilerplate code of waiting for a device to connect
            //The SDK provides the rs2::device_hub class
            rs2::device_hub device_hub(ctx);

            //Using the device_hub we can block the program until a device connects
            selected_device = device_hub.wait_for_device();
        }
        else
        {
            std::cout << "Found the following devices:\n" << std::endl;

            // device_list is a "lazy" container of devices which allows
            //The device list provides 2 ways of iterating it
            //The first way is using an iterator (in this case hidden in the Range-based for loop)
            int index = 0;
            for (rs2::device device : devices)
            {
                std::cout << "  " << index++ << " : " << get_device_name(device) << std::endl;
            }

            uint32_t selected_device_index = get_user_selection("Select a device by index: ");

            // The second way is using the subscript ("[]") operator:
            if (selected_device_index >= devices.size())
            {
                throw std::out_of_range("Selected device index is out of range");
            }

            // Update the selected device
            selected_device = devices[selected_device_index];
        }

        return selected_device;
    }

    static void print_device_information(const rs2::device& dev)
    {
        // Each device provides some information on itself
        // The different types of available information are represented using the "RS2_CAMERA_INFO_*" enum

        std::cout << "Device information: " << std::endl;
        //The following code shows how to enumerate all of the RS2_CAMERA_INFO
        //Note that all enum types in the SDK start with the value of zero and end at the "*_COUNT" value
        for (int i = 0; i < static_cast<int>(RS2_CAMERA_INFO_COUNT); i++)
        {
            rs2_camera_info info_type = static_cast<rs2_camera_info>(i);
            //SDK enum types can be streamed to get a string that represents them
            std::cout << "  " << std::left << std::setw(20) << info_type << " : ";

            //A device might not support all types of RS2_CAMERA_INFO.
            //To prevent throwing exceptions from the "get_info" method we first check if the device supports this type of info
            if (dev.supports(info_type))
                std::cout << dev.get_info(info_type) << std::endl;
            else
                std::cout << "N/A" << std::endl;
        }
    }

    static std::string get_device_name(const rs2::device& dev)
    {
        // Each device provides some information on itself, such as name:
        std::string name = "Unknown Device";
        if (dev.supports(RS2_CAMERA_INFO_NAME))
            name = dev.get_info(RS2_CAMERA_INFO_NAME);

        // and the serial number of the device:
        std::string sn = "########";
        if (dev.supports(RS2_CAMERA_INFO_SERIAL_NUMBER))
            sn = std::string("#") + dev.get_info(RS2_CAMERA_INFO_SERIAL_NUMBER);

        return name + " " + sn;
    }

    static std::string get_sensor_name(const rs2::sensor& sensor)
    {
        // Sensors support additional information, such as a human readable name
        if (sensor.supports(RS2_CAMERA_INFO_NAME))
            return sensor.get_info(RS2_CAMERA_INFO_NAME);
        else
            return "Unknown Sensor";
    }

    static rs2::sensor get_a_sensor_from_a_device(const rs2::device& dev)
    {
        // A rs2::device is a container of rs2::sensors that have some correlation between them.
        // For example:
        //    * A device where all sensors are on a single board
        //    * A Robot with mounted sensors that share calibration information

        // Given a device, we can query its sensors using:
        std::vector<rs2::sensor> sensors = dev.query_sensors();

        std::cout << "Device consists of " << sensors.size() << " sensors:\n" << std::endl;
        int index = 0;
        // We can now iterate the sensors and print their names
        for (rs2::sensor sensor : sensors)
        {
            std::cout << "  " << index++ << " : " << get_sensor_name(sensor) << std::endl;
        }

        uint32_t selected_sensor_index = get_user_selection("Select a sensor by index: ");

        // The second way is using the subscript ("[]") operator:
        if (selected_sensor_index >= sensors.size())
        {
            throw std::out_of_range("Selected sensor index is out of range");
        }

        return  sensors[selected_sensor_index];
    }

    static rs2_option get_sensor_option(const rs2::sensor& sensor)
    {
        // Sensors usually have several options to control their properties
        //  such as Exposure, Brightness etc.

        std::cout << "Sensor supports the following options:\n" << std::endl;

        // The following loop shows how to iterate over all available options
        // Starting from 0 until RS2_OPTION_COUNT (exclusive)
        for (int i = 0; i < static_cast<int>(RS2_OPTION_COUNT); i++)
        {
            rs2_option option_type = static_cast<rs2_option>(i);
            //SDK enum types can be streamed to get a string that represents them
            std::cout << "  " << i << ": " << option_type;

            // To control an option, use the following api:

            // First, verify that the sensor actually supports this option
            if (sensor.supports(option_type))
            {
                std::cout << std::endl;

                // Get a human readable description of the option
                const char* description = sensor.get_option_description(option_type);
                std::cout << "       Description   : " << description << std::endl;

                // Get the current value of the option
                if (option_type != RS2_OPTION_REGION_OF_INTEREST)
                {
                    float current_value = sensor.get_option(option_type);
                    std::cout << "       Current Value : " << current_value << std::endl;
                }
                else
                {
                    rs2_option_rect current_value = sensor.get_option_value(option_type)->as_rect;
                    std::cout << "       Current Value : [(" << current_value.x1 << "," << current_value.y1 << "), (" << current_value.x2 << "," << current_value.y2 << ")]" << std::endl;
                }
                //To change the value of an option, please follow the change_sensor_option() function
            }
            else
            {
                std::cout << " is not supported" << std::endl;
            }
        }

        uint32_t selected_sensor_option = get_user_selection("Select an option by index: ");
        if (selected_sensor_option >= static_cast<int>(RS2_OPTION_COUNT))
        {
            throw std::out_of_range("Selected option is out of range");
        }
        return static_cast<rs2_option>(selected_sensor_option);
    }

    static float get_depth_units(const rs2::sensor& sensor)
    {
        //A Depth stream contains an image that is composed of pixels with depth information.
        //The value of each pixel is the distance from the camera, in some distance units.
        //To get the distance in units of meters, each pixel's value should be multiplied by the sensor's depth scale
        //Here is the way to grab this scale value for a "depth" sensor:
        if (rs2::depth_sensor dpt_sensor = sensor.as<rs2::depth_sensor>())
        {
            float scale = dpt_sensor.get_depth_scale();
            std::cout << "Scale factor for depth sensor is: " << scale << std::endl;
            return scale;
        }
        else
            throw std::runtime_error("Given sensor is not a depth sensor");
    }

    static void get_field_of_view(const rs2::stream_profile& stream)
    {
        // A sensor's stream (rs2::stream_profile) is in general a stream of data with no specific type.
        // For video streams (streams of images), the sensor that produces the data has a lens and thus has properties such
        //  as a focal point, distortion, and principal point.
        // To get these intrinsics parameters, we need to take a stream and first check if it is a video stream
        if (auto video_stream = stream.as<rs2::video_stream_profile>())
        {
            try
            {
                //If the stream is indeed a video stream, we can now simply call get_intrinsics()
                rs2_intrinsics intrinsics = video_stream.get_intrinsics();

                auto principal_point = std::make_pair(intrinsics.ppx, intrinsics.ppy);
                auto focal_length = std::make_pair(intrinsics.fx, intrinsics.fy);
                rs2_distortion model = intrinsics.model;

                std::cout << "Principal Point         : " << principal_point.first << ", " << principal_point.second << std::endl;
                std::cout << "Focal Length            : " << focal_length.first << ", " << focal_length.second << std::endl;
                std::cout << "Distortion Model        : " << model << std::endl;
                std::cout << "Distortion Coefficients : [" << intrinsics.coeffs[0] << "," << intrinsics.coeffs[1] << "," <<
                    intrinsics.coeffs[2] << "," << intrinsics.coeffs[3] << "," << intrinsics.coeffs[4] << "]" << std::endl;
            }
            catch (const std::exception& e)
            {
                std::cerr << "Failed to get intrinsics for the given stream. " << e.what() << std::endl;
            }
        }
        else if (auto motion_stream = stream.as<rs2::motion_stream_profile>())
        {
            try
            {
                //If the stream is indeed a motion stream, we can now simply call get_motion_intrinsics()
                rs2_motion_device_intrinsic intrinsics = motion_stream.get_motion_intrinsics();

                std::cout << " Scale X      cross axis      cross axis  Bias X \n";
                std::cout << " cross axis    Scale Y        cross axis  Bias Y  \n";
                std::cout << " cross axis    cross axis     Scale Z     Bias Z  \n";
                for (int i = 0; i < 3; i++)
                {
                    for (int j = 0; j < 4; j++)
                    {
                        std::cout << intrinsics.data[i][j] << "    ";
                    }
                    std::cout << "\n";
                }

                std::cout << "Variance of noise for X, Y, Z axis \n";
                for (int i = 0; i < 3; i++)
                    std::cout << intrinsics.noise_variances[i] << " ";
                std::cout << "\n";

                std::cout << "Variance of bias for X, Y, Z axis \n";
                for (int i = 0; i < 3; i++)
                    std::cout << intrinsics.bias_variances[i] << " ";
                std::cout << "\n";
            }
            catch (const std::exception& e)
            {
                std::cerr << "Failed to get intrinsics for the given stream. " << e.what() << std::endl;
            }
        }
        else
        {
            std::cerr << "Given stream profile has no intrinsics data" << std::endl;
        }
    }

    static void get_extrinsics(const rs2::stream_profile& from_stream, const rs2::stream_profile& to_stream)
    {
        // If the device/sensor that you are using contains more than a single stream, and it was calibrated
        // then the SDK provides a way of getting the transformation between any two streams (if such exists)
        try
        {
            // Given two streams, use the get_extrinsics_to() function to get the transformation from the stream to the other stream
            rs2_extrinsics extrinsics = from_stream.get_extrinsics_to(to_stream);
            std::cout << "Translation Vector : [" << extrinsics.translation[0] << "," << extrinsics.translation[1] << "," << extrinsics.translation[2] << "]\n";
            std::cout << "Rotation Matrix    : [" << extrinsics.rotation[0] << "," << extrinsics.rotation[3] << "," << extrinsics.rotation[6] << "]\n";
            std::cout << "                   : [" << extrinsics.rotation[1] << "," << extrinsics.rotation[4] << "," << extrinsics.rotation[7] << "]\n";
            std::cout << "                   : [" << extrinsics.rotation[2] << "," << extrinsics.rotation[5] << "," << extrinsics.rotation[8] << "]" << std::endl;
        }
        catch (const std::exception& e)
        {
            std::cerr << "Failed to get extrinsics for the given streams. " << e.what() << std::endl;
        }
    }

    static void change_sensor_option(const rs2::sensor& sensor, rs2_option option_type)
    {
        // Sensors usually have several options to control their properties
        //  such as Exposure, Brightness etc.

        // To control an option, use the following api:

        // First, verify that the sensor actually supports this option
        if (!sensor.supports(option_type))
        {
            std::cerr << "This option is not supported by this sensor" << std::endl;
            return;
        }

        if (option_type == RS2_OPTION_REGION_OF_INTEREST)
        {
            std::cerr << "Setting ROI is not demonstrated in this example. Check `set_region_of_interest` API" << std::endl;
            return;
        }

        // Each option provides its rs2::option_range to provide information on how it can be changed
        // To get the supported range of an option we do the following:

        std::cout << "Supported range for option " << option_type << ":" << std::endl;

        rs2::option_range range = sensor.get_option_range(option_type);
        float default_value = range.def;
        float maximum_supported_value = range.max;
        float minimum_supported_value = range.min;
        float difference_to_next_value = range.step;
        std::cout << "  Min Value     : " << minimum_supported_value << std::endl;
        std::cout << "  Max Value     : " << maximum_supported_value << std::endl;
        std::cout << "  Default Value : " << default_value << std::endl;
        std::cout << "  Step          : " << difference_to_next_value << std::endl;

        bool change_option = false;
        change_option = prompt_yes_no("Change option's value?");

        if (change_option)
        {
            std::cout << "Enter the new value for this option: ";
            float requested_value;
            std::cin >> requested_value;
            std::cout << std::endl;

            // To set an option to a different value, we can call set_option with a new value
            try
            {
                sensor.set_option(option_type, requested_value);
            }
            catch (const rs2::error& e)
            {
                // Some options can only be set while the camera is streaming,
                // and generally the hardware might fail so it is good practice to catch exceptions from set_option
                std::cerr << "Failed to set option " << option_type << ". (" << e.what() << ")" << std::endl;
            }
        }
    }

    static rs2::embedded_filter get_an_embedded_filter_from_a_sensor(const rs2::sensor& sensor)
    {
        // Composite options are not exposed directly on a sensor - each lives on one of the
        // sensor's EMBEDDED FILTERs, its own independent options registry. A sensor can have
        // several, and not every one necessarily exposes any composite option at all.
        std::vector<rs2::embedded_filter> filters = sensor.query_embedded_filters();
        if (filters.empty())
            throw std::runtime_error("This sensor has no embedded filters");

        std::cout << "Sensor has " << filters.size() << " embedded filter(s):\n" << std::endl;
        int index = 0;
        for (auto&& filter : filters)
            std::cout << "  " << index++ << " : " << rs2_embedded_filter_type_to_string(filter.get_type()) << std::endl;

        uint32_t selected_filter_index = get_user_selection("Select an embedded filter by index: ");
        if (selected_filter_index >= filters.size())
            throw std::out_of_range("Selected embedded filter index is out of range");

        return filters[selected_filter_index];
    }

    static rs2_composite_option_id get_a_composite_option(const rs2::embedded_filter& filter)
    {
        // Composite options are a completely separate identity space from the ordinary (scalar)
        // rs2_option controls seen in get_sensor_option() above: a single multi-field control
        // whose fields are all exchanged together, atomically, in ONE UVC transaction.
        std::vector<rs2_composite_option_id> ids = filter.get_supported_composite_options();
        if (ids.empty())
            throw std::runtime_error("This embedded filter has no composite options");

        std::cout << "Embedded filter supports the following composite option(s):\n" << std::endl;
        for (size_t i = 0; i < ids.size(); i++)
        {
            rs2_composite_option_id id = ids[i];
            std::cout << "  " << i << ": " << rs2_composite_option_id_to_string(id) << std::endl;
            std::cout << "       Read-only   : " << (filter.is_composite_option_read_only(id) ? "true" : "false") << std::endl;
            std::cout << "       Description : " << filter.get_composite_option_description(id) << std::endl;
        }

        uint32_t selected_index = get_user_selection("Select a composite option by index: ");
        if (selected_index >= ids.size())
            throw std::out_of_range("Selected composite option is out of range");

        return ids[selected_index];
    }

    static void print_composite_option_value(const rs2::embedded_filter& filter, rs2_composite_option_id id)
    {
        // Unlike get_sensor_option() above, there is no generic "any composite option" cast - the
        // caller is expected to know each id's documented wire struct and cast accordingly via
        // get_composite_option_as<T>(). A new composite option needs a case added here too.
        try
        {
            switch (id)
            {
            case RS2_COMPOSITE_OPTION_DECIMATION_FILTER_DPP:
            {
                rs2_decimation_filter_dpp_config v = filter.get_composite_option_as<rs2_decimation_filter_dpp_config>(id);
                std::cout << "  enabled   : " << v.enabled << std::endl;
                std::cout << "  magnitude : " << v.magnitude << std::endl;
                break;
            }
            case RS2_COMPOSITE_OPTION_TEMPORAL_FILTER_DPP:
            {
                rs2_temporal_filter_dpp_config v = filter.get_composite_option_as<rs2_temporal_filter_dpp_config>(id);
                std::cout << "  enabled           : " << v.enabled << std::endl;
                std::cout << "  smooth_alpha      : " << v.smooth_alpha << std::endl;
                std::cout << "  smooth_delta      : " << v.smooth_delta << std::endl;
                std::cout << "  persistency_index : " << v.persistency_index << std::endl;
                break;
            }
            case RS2_COMPOSITE_OPTION_HDRD_CONTROL:
            {
                rs2_hdrd_control v = filter.get_composite_option_as<rs2_hdrd_control>(id);
                std::cout << "  enable          : " << v.enable << std::endl;
                std::cout << "  filter_type     : " << v.filter_type << std::endl;
                std::cout << "  downscale_ratio : " << v.downscale_ratio << std::endl;
                std::cout << "  shift_mode      : " << v.shift_mode << std::endl;
                std::cout << "  shift_pixels    : " << v.shift_pixels << std::endl;
                std::cout << "  threshold_mode  : " << v.threshold_mode << std::endl;
                std::cout << "  threshold_mm    : " << v.threshold_mm << std::endl;
                break;
            }
            default:
                std::cout << "  (no typed printer registered for this composite option id)" << std::endl;
            }
        }
        catch (const rs2::error& e)
        {
            // A control that is registered but not actually functional on this device/FW is a
            // real, expected outcome - get_supported_composite_options() only reflects static
            // registration, never a live capability check.
            std::cerr << "Failed to read composite option " << rs2_composite_option_id_to_string(id) << ". (" << e.what() << ")" << std::endl;
        }
    }

    static void change_composite_option(const rs2::embedded_filter& filter, rs2_composite_option_id id)
    {
        // Each composite option is exchanged as ONE atomic struct, unlike change_sensor_option()
        // above - so changing even a single field means a read-modify-write of the WHOLE struct:
        // read current, change only the picked field, send the entire thing back.

        if (filter.is_composite_option_read_only(id))
        {
            std::cerr << "This composite option is read-only" << std::endl;
            return;
        }

        switch (id)
        {
        case RS2_COMPOSITE_OPTION_DECIMATION_FILTER_DPP:
        {
            rs2_decimation_filter_dpp_config cfg;
            try
            {
                cfg = filter.get_composite_option_as<rs2_decimation_filter_dpp_config>(id);
            }
            catch (const rs2::error& e)
            {
                std::cerr << "Failed to read current value: " << e.what() << std::endl;
                return;
            }

            rs2_decimation_filter_dpp_range range;
            try
            {
                range = filter.get_composite_option_range_as<rs2_decimation_filter_dpp_range>(id);
            }
            catch (const rs2::error& e)
            {
                std::cerr << "Failed to read supported range: " << e.what() << std::endl;
                return;
            }
            std::cout << "Supported range:" << std::endl;
            std::cout << "  enabled   : [" << range.min.enabled << ", " << range.max.enabled << "]" << std::endl;
            std::cout << "  magnitude : [" << range.min.magnitude << ", " << range.max.magnitude << "]" << std::endl;

            std::cout << "\nWhich field would you like to change?\n" << std::endl;
            std::cout << "  0 : enabled\n  1 : magnitude" << std::endl;
            uint32_t field_index = get_user_selection("Select a field by index: ");

            std::cout << "Enter the new value for this field: ";
            int32_t requested_value;
            std::cin >> requested_value;
            std::cout << std::endl;

            switch (field_index)
            {
            case 0: cfg.enabled = requested_value; break;
            case 1: cfg.magnitude = requested_value; break;
            default:
                std::cerr << "Selected field is out of range" << std::endl;
                return;
            }

            try
            {
                filter.set_composite_option_from(id, cfg);
            }
            catch (const rs2::error& e)
            {
                std::cerr << "Failed to set composite option " << rs2_composite_option_id_to_string(id) << ". (" << e.what() << ")" << std::endl;
            }
            break;
        }
        case RS2_COMPOSITE_OPTION_TEMPORAL_FILTER_DPP:
        {
            rs2_temporal_filter_dpp_config cfg;
            try
            {
                cfg = filter.get_composite_option_as<rs2_temporal_filter_dpp_config>(id);
            }
            catch (const rs2::error& e)
            {
                std::cerr << "Failed to read current value: " << e.what() << std::endl;
                return;
            }

            rs2_temporal_filter_dpp_range range;
            try
            {
                range = filter.get_composite_option_range_as<rs2_temporal_filter_dpp_range>(id);
            }
            catch (const rs2::error& e)
            {
                std::cerr << "Failed to read supported range: " << e.what() << std::endl;
                return;
            }
            std::cout << "Supported range:" << std::endl;
            std::cout << "  enabled           : [" << range.min.enabled << ", " << range.max.enabled << "]" << std::endl;
            std::cout << "  smooth_alpha      : [" << range.min.smooth_alpha << ", " << range.max.smooth_alpha << "]" << std::endl;
            std::cout << "  smooth_delta      : [" << range.min.smooth_delta << ", " << range.max.smooth_delta << "]" << std::endl;
            std::cout << "  persistency_index : [" << range.min.persistency_index << ", " << range.max.persistency_index << "]" << std::endl;

            std::cout << "\nWhich field would you like to change?\n" << std::endl;
            std::cout << "  0 : enabled\n  1 : smooth_alpha (normalized [0,1] scaled into [0,1000])\n"
                          "  2 : smooth_delta\n  3 : persistency_index" << std::endl;
            uint32_t field_index = get_user_selection("Select a field by index: ");

            std::cout << "Enter the new value for this field: ";
            int32_t requested_value;
            std::cin >> requested_value;
            std::cout << std::endl;

            switch (field_index)
            {
            case 0: cfg.enabled = requested_value; break;
            case 1: cfg.smooth_alpha = requested_value; break;
            case 2: cfg.smooth_delta = requested_value; break;
            case 3: cfg.persistency_index = requested_value; break;
            default:
                std::cerr << "Selected field is out of range" << std::endl;
                return;
            }

            try
            {
                filter.set_composite_option_from(id, cfg);
            }
            catch (const rs2::error& e)
            {
                std::cerr << "Failed to set composite option " << rs2_composite_option_id_to_string(id) << ". (" << e.what() << ")" << std::endl;
            }
            break;
        }
        case RS2_COMPOSITE_OPTION_HDRD_CONTROL:
        {
            rs2_hdrd_control cfg;
            try
            {
                cfg = filter.get_composite_option_as<rs2_hdrd_control>(id);
            }
            catch (const rs2::error& e)
            {
                std::cerr << "Failed to read current value: " << e.what() << std::endl;
                return;
            }

            rs2_hdrd_control_range range;
            try
            {
                range = filter.get_composite_option_range_as<rs2_hdrd_control_range>(id);
            }
            catch (const rs2::error& e)
            {
                std::cerr << "Failed to read supported range: " << e.what() << std::endl;
                return;
            }
            std::cout << "Supported range:" << std::endl;
            std::cout << "  enable          : [" << range.min.enable << ", " << range.max.enable << "]" << std::endl;
            std::cout << "  filter_type     : [" << range.min.filter_type << ", " << range.max.filter_type << "]" << std::endl;
            std::cout << "  downscale_ratio : [" << range.min.downscale_ratio << ", " << range.max.downscale_ratio << "]" << std::endl;
            std::cout << "  shift_mode      : [" << range.min.shift_mode << ", " << range.max.shift_mode << "]" << std::endl;
            std::cout << "  shift_pixels    : [" << range.min.shift_pixels << ", " << range.max.shift_pixels << "]" << std::endl;
            std::cout << "  threshold_mode  : [" << range.min.threshold_mode << ", " << range.max.threshold_mode << "]" << std::endl;
            std::cout << "  threshold_mm    : [" << range.min.threshold_mm << ", " << range.max.threshold_mm << "]" << std::endl;

            std::cout << "\nWhich field would you like to change?\n" << std::endl;
            std::cout << "  0 : enable\n  1 : filter_type\n  2 : downscale_ratio\n  3 : shift_mode\n"
                          "  4 : shift_pixels\n  5 : threshold_mode\n  6 : threshold_mm" << std::endl;
            uint32_t field_index = get_user_selection("Select a field by index: ");

            std::cout << "Enter the new value for this field: ";
            int requested_value;
            std::cin >> requested_value;
            std::cout << std::endl;

            switch (field_index)
            {
            case 0: cfg.enable = requested_value; break;
            case 1: cfg.filter_type = requested_value; break;
            case 2: cfg.downscale_ratio = requested_value; break;
            case 3: cfg.shift_mode = requested_value; break;
            case 4: cfg.shift_pixels = requested_value; break;
            case 5: cfg.threshold_mode = requested_value; break;
            case 6: cfg.threshold_mm = requested_value; break;
            default:
                std::cerr << "Selected field is out of range" << std::endl;
                return;
            }

            try
            {
                filter.set_composite_option_from(id, cfg);
            }
            catch (const rs2::error& e)
            {
                std::cerr << "Failed to set composite option " << rs2_composite_option_id_to_string(id) << ". (" << e.what() << ")" << std::endl;
            }
            break;
        }
        default:
            std::cerr << "No typed setter registered for this composite option id" << std::endl;
        }
    }

    static rs2::stream_profile choose_a_streaming_profile(const rs2::sensor& sensor)
    {
        // A Sensor is an object that is capable of streaming one or more types of data.
        // For example:
        //    * A stereo sensor with Left and Right Infrared streams that
        //        creates a stream of depth images
        //    * A motion sensor with an Accelerometer and Gyroscope that
        //        provides a stream of motion information

        // Using the sensor we can get all of its streaming profiles
        std::vector<rs2::stream_profile> stream_profiles = sensor.get_stream_profiles();

        // Usually a sensor provides one or more streams which are identifiable by their stream_type and stream_index
        // Each of these streams can have several profiles (e.g FHD/HHD/VGA/QVGA resolution, or 90/60/30 fps, etc..)
        //The following code shows how to go over a sensor's stream profiles, and group the profiles by streams.
        std::map<std::pair<rs2_stream, int>, int> unique_streams;
        for (auto&& sp : stream_profiles)
        {
            unique_streams[std::make_pair(sp.stream_type(), sp.stream_index())]++;
        }
        std::cout << "Sensor consists of " << unique_streams.size() << " streams: " << std::endl;
        for (size_t i = 0; i < unique_streams.size(); i++)
        {
            auto it = unique_streams.begin();
            std::advance(it, i);
            std::cout << "  - " << it->first.first << " #" << it->first.second << std::endl;
        }

        //Next, we go over all the stream profiles and print the details of each one
        std::cout << "Sensor provides the following stream profiles:" << std::endl;
        int profile_num = 0;
        for (rs2::stream_profile stream_profile : stream_profiles)
        {
            // A Stream is an abstraction for a sequence of data items of a
            //  single data type, which are ordered according to their time
            //  of creation or arrival.
            // The stream's data types are represented using the rs2_stream
            //  enumeration
            rs2_stream stream_data_type = stream_profile.stream_type();

            // The rs2_stream provides only types of data which are
            //  supported by the RealSense SDK
            // For example:
            //    * rs2_stream::RS2_STREAM_DEPTH describes a stream of depth images
            //    * rs2_stream::RS2_STREAM_COLOR describes a stream of color images
            //    * rs2_stream::RS2_STREAM_INFRARED describes a stream of infrared images

            // As mentioned, a sensor can have multiple streams.
            // In order to distinguish between streams with the same
            //  stream type we can use the following methods:

            // 1) Each stream type can have multiple occurances.
            //    All streams, of the same type, provided from a single
            //     device have distinct indices:
            int stream_index = stream_profile.stream_index();

            // 2) Each stream has a user-friendly name.
            //    The stream's name is not promised to be unique,
            //     rather a human readable description of the stream
            std::string stream_name = stream_profile.stream_name();

            // 3) Each stream in the system, which derives from the same
            //     rs2::context, has a unique identifier
            //    This identifier is unique across all streams, regardless of the stream type.
            int unique_stream_id = stream_profile.unique_id(); // The unique identifier can be used for comparing two streams
            std::cout << std::setw(3) << profile_num << ": " << stream_data_type << " #" << stream_index;

            // As noted, a stream is an abstraction.
            // In order to get additional data for the specific type of a
            //  stream, a mechanism of "Is" and "As" is provided:
            if (stream_profile.is<rs2::video_stream_profile>()) //"Is" will test if the type tested is of the type given
            {
                // "As" will try to convert the instance to the given type
                rs2::video_stream_profile video_stream_profile = stream_profile.as<rs2::video_stream_profile>();

                // After using the "as" method we can use the new data type
                //  for additinal operations:
                std::cout << " (Video Stream: " << video_stream_profile.format() << " " <<
                    video_stream_profile.width() << "x" << video_stream_profile.height() << "@ " << video_stream_profile.fps() << "Hz)";
            }
            std::cout << std::endl;
            profile_num++;
        }

        uint32_t selected_profile_index = get_user_selection("Please select the desired streaming profile: ");
        if (selected_profile_index >= stream_profiles.size())
        {
            throw std::out_of_range("Requested profile index is out of range");
        }

        return stream_profiles[selected_profile_index];
    }

    static void start_streaming_a_profile(const rs2::sensor& sensor, const rs2::stream_profile& stream_profile)
    {
        // The sensor controls turning the streaming on and off
        // To start streaming, two calls must be made with the following order:
        //  1) open(stream_profiles_to_open)
        //  2) start(function_to_handle_frames)

        // Open can be called with a single profile, or with a collection of profiles
        // Calling open() tries to get exclusive access to the sensor.
        // Opening a sensor may have side effects such as actually
        //  running, consume power, produce data, etc.
        sensor.open(stream_profile);

        std::ostringstream oss;
        oss << "Displaying profile " << stream_profile.stream_name();

        // In order to begin getting data from the sensor, we need to register a callback to handle frames (data)
        // To register a callback, the sensor's start() method should be invoked.
        // The start() method takes any type of callable object that takes a frame as its parameter
        // NOTE:
        //  * Since a sensor can stream multiple streams, and start()
        //     takes a single handler, multiple types of frames can
        //     arrive to the handler.
        //  * Different streams' frames arrive on different threads.
        //    This behavior requires the provided frame handler to the
        //     start method to be re-entrant

        // In this example we have created a class to handle the frames,
        // and we capture it by reference inside a C++11 lambda which is passed to the start() function
        helper::frame_viewer display(oss.str());
        sensor.start([&](rs2::frame f) { display(f); });

        // At this point, frames will asynchronously arrive to the callback handler
        // This thread will continue to run in parallel.
        // To prevent this thread from returning, we block it using the helper wait() function
        std::cout << "Streaming profile: " << stream_profile.stream_name() << ". Close display window to continue..." << std::endl;
        display.wait();

        // To stop streaming, we simply need to call the sensor's stop method
        // After returning from the call to stop(), no frames will arrive from this sensor
        sensor.stop();

        // To complete the stop operation, and release access of the device, we need to call close() per sensor
        sensor.close();
    }
};
