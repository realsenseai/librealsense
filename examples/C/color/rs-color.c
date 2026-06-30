// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017-24 RealSense, Inc. All Rights Reserved.

/* Include the librealsense C header files */
#include <librealsense2/rs.h>
#include <librealsense2/h/rs_pipeline.h>
#include <librealsense2/h/rs_frame.h>

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include "example.h"


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                     These parameters are reconfigurable                                        //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define STREAM          RS2_STREAM_COLOR  // Type of data provided by RealSense device                            //
#define FORMAT          RS2_FORMAT_ANY    // How binary data is encoded within a frame (ANY)                      //
#define WIDTH           0                 // Number of columns for each frame (ANY)                               //
#define HEIGHT          0                 // Number of lines for each frame (ANY)                                 //
#define FPS             0                 // Rate of frames per second (ANY)                                      //
#define STREAM_INDEX    -1                // Used for multiple streams of the same type (ANY)                     //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


int main()
{
    rs2_error* e = 0;

    // Create a context object. This object owns the handles to all connected realsense devices.
    // The returned object should be released with rs2_delete_context(...)
    rs2_context* ctx = rs2_create_context(RS2_API_VERSION, &e);
    check_error(e);

    /* Get a list of all the connected devices. */
    // The returned object should be released with rs2_delete_device_list(...)
    rs2_device_list* device_list = rs2_query_devices(ctx, &e);
    check_error(e);

    int dev_count = rs2_get_device_count(device_list, &e);
    check_error(e);
    printf("There are %d connected RealSense devices.\n", dev_count);
    if (0 == dev_count)
        return EXIT_FAILURE;

    // Get the first connected device
    // The returned object should be released with rs2_delete_device(...)
    rs2_device* dev = rs2_create_device(device_list, 0, &e);
    check_error(e);

    print_device_info(dev);

    // Create a pipeline to configure, start and stop camera streaming
    // The returned object should be released with rs2_delete_pipeline(...)
    rs2_pipeline* pipeline =  rs2_create_pipeline(ctx, &e);
    check_error(e);

    // Create a config instance, used to specify hardware configuration
    // The retunred object should be released with rs2_delete_config(...)
    rs2_config* config = rs2_create_config(&e);
    check_error(e);

    // Request a specific configuration
    rs2_config_enable_stream(config, STREAM, STREAM_INDEX, WIDTH, HEIGHT, FORMAT, FPS, &e);
    check_error(e);

    // Start the pipeline streaming
    // The retunred object should be released with rs2_delete_pipeline_profile(...)
    rs2_pipeline_profile* pipeline_profile = rs2_pipeline_start_with_config(pipeline, config, &e);
    if (e)
    {
        printf("The connected device doesn't support color streaming!\n");
        exit(EXIT_FAILURE);
    }

    while (1)  // Until user presses Ctrl+C
    {
        // This call waits until a new composite_frame is available
        // composite_frame holds a set of frames. It is used to prevent frame drops
        // The returned object should be released with rs2_release_frame(...)
        rs2_frame* frames = rs2_pipeline_wait_for_frames(pipeline, RS2_DEFAULT_TIMEOUT, &e);
        check_error(e);

        // Returns the number of frames embedded within the composite frame
        int num_of_frames = rs2_embedded_frames_count(frames, &e);
        check_error(e);

        int i;
        for (i = 0; i < num_of_frames; ++i)
        {
            // The retunred object should be released with rs2_release_frame(...)
            rs2_frame* frame = rs2_extract_frame(frames, i, &e);
            check_error(e);

            const uint8_t* rgb_frame_data = (const uint8_t*)(rs2_get_frame_data(frame, &e));
            check_error(e);

            unsigned long long frame_number = rs2_get_frame_number(frame, &e);
            check_error(e);

            rs2_time_t frame_timestamp = rs2_get_frame_timestamp(frame, &e);
            check_error(e);

            // Specifies the clock in relation to which the frame timestamp was measured
            rs2_timestamp_domain frame_timestamp_domain = rs2_get_frame_timestamp_domain(frame, &e);
            check_error(e);
            const char* frame_timestamp_domain_str = rs2_timestamp_domain_to_string(frame_timestamp_domain);

            rs2_metadata_type frame_metadata_time_of_arrival = rs2_get_frame_metadata(frame, RS2_FRAME_METADATA_TIME_OF_ARRIVAL, &e);
            check_error(e);

            printf("RGB frame arrived.\n");
            printf("First 10 bytes: ");
            int i;
            for(i=0; i < 10; ++i)
                printf("%02x ", rgb_frame_data[i]);

            printf("\nFrame No: %llu\n", frame_number);
            printf("Timestamp: %f\n", frame_timestamp);
            printf("Timestamp domain: %s\n", frame_timestamp_domain_str);
            printf("Time of arrival: %lld\n\n", frame_metadata_time_of_arrival);
            rs2_release_frame(frame);
        }

        rs2_release_frame(frames);
    }
}
