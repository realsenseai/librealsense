// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 RealSense, Inc. All Rights Reserved.

#include <iostream>
#include <iomanip>
#include <map>
#include <utility>
#include <vector>
#include <librealsense2/rs.hpp>
#include <algorithm>
#include "../example.hpp"
#include "api_how_to.h"
#include "helper.h"

// Forward declaration of the create_sensor_actions which returns a collection of functions
//  and their description that operate on a device or sensor
using sensor_action = std::pair<std::function<void(rs2::device, rs2::sensor)>, std::string>;
std::vector<sensor_action> create_sensor_actions();

int main(int argc, char * argv[]) try
{
    rs2::rum::set_app_name( "rs-sensor-control" );
    //In this example we will go through a flow of selecting a device, then selecting one of its sensors,
    // and finally run some operations on that sensor

    //We will use the "how_to" class to make the code clear, expressive and encapsulate common actions inside a function

    std::vector<sensor_action> sensor_actions = create_sensor_actions();

    bool choose_a_device = true;
    while (choose_a_device)
    {
        print_separator();
        //First thing, let's choose a device:
        rs2::device dev = how_to::get_a_realsense_device();

        //Print the device's information
        how_to::print_device_information(dev);

        bool choose_a_sensor = true;
        while (choose_a_sensor)
        {
            print_separator();
            //Next, choose one of the device's sensors
            rs2::sensor sensor = how_to::get_a_sensor_from_a_device(dev);

            bool control_sensor = true;
            while (control_sensor)
            {
                print_separator();
                std::cout << "What would you like to do with the sensor?\n" << std::endl;
                int i = 0;
                for (auto&& action : sensor_actions)
                {
                    std::cout << i++ << " : " << action.second << std::endl;
                }
                uint32_t selected_action_index = helper::get_user_selection("Select an action: ");
                if (selected_action_index >= sensor_actions.size())
                {
                    throw std::out_of_range("Selected action index is out of range");
                }

                auto selected_action = sensor_actions[selected_action_index].first;

                bool repeat = true;
                while (repeat)
                {
                    print_separator();
                    selected_action(dev, sensor);
                    repeat = prompt_yes_no(sensor_actions[selected_action_index].second + " again?");
                }
                control_sensor = prompt_yes_no("Choose another action for this sensor?");
            }

            choose_a_sensor = prompt_yes_no("Choose another sensor?");
        }

        choose_a_device = prompt_yes_no("Choose another device?");
    }

    return EXIT_SUCCESS;
}
catch (const rs2::error & e)
{
    std::cerr << "RealSense error calling " << e.get_failed_function() << "(" << e.get_failed_args() << "):\n    " << e.what() << std::endl;
    return EXIT_FAILURE;
}
catch (const std::exception & e)
{
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
}

void control_sensor_options(rs2::device device, rs2::sensor sensor)
{
    // The rs2::sensor allows you to control its properties such as Exposure, Brightness etc.
    rs2_option sensor_option = how_to::get_sensor_option(sensor);
    how_to::change_sensor_option(sensor, sensor_option);
}
void control_composite_options(rs2::device device, rs2::sensor sensor)
{
    // Composite options are a completely separate identity space from the ordinary (scalar)
    // options above, and live on one of the sensor's embedded filters rather than the sensor
    // itself - see rs2::embedded_filter / rs_composite_option.h.
    rs2::embedded_filter filter = how_to::get_an_embedded_filter_from_a_sensor(sensor);
    rs2_composite_option_id id = how_to::get_a_composite_option(filter);

    std::cout << "\nCurrent value:" << std::endl;
    how_to::print_composite_option_value(filter, id);

    if (prompt_yes_no("Change this composite option's value?"))
    {
        how_to::change_composite_option(filter, id);

        std::cout << "\nValue after change:" << std::endl;
        how_to::print_composite_option_value(filter, id);
    }
}
void display_live_stream(rs2::device device, rs2::sensor sensor)
{
    // The rs2::sensor allows you to control its streams
    // We will first choose a single stream profile from the available profiles of the sensor
    rs2::stream_profile selected_profile = how_to::choose_a_streaming_profile(sensor);

    // Next, we will display the stream in a window
    how_to::start_streaming_a_profile(sensor, selected_profile);
}
void show_stream_intrinsics(rs2::device device, rs2::sensor sensor)
{
    // Each stream has its own intrinsic information, first choose a stream, then display its intrinsics
    rs2::stream_profile selected_profile = how_to::choose_a_streaming_profile(sensor);
    how_to::get_field_of_view(selected_profile);
}
void show_extrinsics_between_streams(rs2::device device, rs2::sensor sensor)
{
    // A rs2::device can have its sensors and streams calibrated and provide the extrinsics between 2 streams.
    std::cout << "Please choose a sensor and then a stream that will be used as the origin of extrinsic transformation:\n" << std::endl;
    rs2::sensor from_sensor = how_to::get_a_sensor_from_a_device(device);
    rs2::stream_profile from = how_to::choose_a_streaming_profile(from_sensor);

    std::cout << "Please choose a sensor and then a stream that will be used as the target of extrinsic transformation::\n" << std::endl;
    rs2::sensor to_sensor = how_to::get_a_sensor_from_a_device(device);
    rs2::stream_profile to = how_to::choose_a_streaming_profile(to_sensor);

    how_to::get_extrinsics(from, to);
}
std::vector<sensor_action> create_sensor_actions()
{
    //This function creates several functions ("sensor_action") that takes a device and a sensor,
    // and perform some specific action
    return std::vector<sensor_action> {
            std::make_pair(&control_sensor_options, "Control sensor's options"),
            std::make_pair(&control_composite_options, "Control sensor's composite options"),
            std::make_pair(&display_live_stream, "Control sensor's streams"),
            std::make_pair(&show_stream_intrinsics, "Show stream intrinsics"),
            std::make_pair(&show_extrinsics_between_streams, "Display extrinsics")
    };
}
