// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017-24 RealSense, Inc. All Rights Reserved.

/* Include the librealsense C header files */
#include <librealsense2/rs.h>
#include <librealsense2/h/rs_pipeline.h>
#include <librealsense2/h/rs_option.h>
#include <librealsense2/h/rs_frame.h>
#include "example.h"

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                     These parameters are reconfigurable                                        //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define STREAM          RS2_STREAM_INFRARED  // rs2_stream is a types of data provided by RealSense device        //
#define FORMAT          RS2_FORMAT_Y8        // rs2_format identifies how binary data is encoded within a frame   //
#define WIDTH           640                  // Defines the number of columns for each frame or zero for auto resolve//
#define HEIGHT          0                    // Defines the number of lines for each frame or zero for auto resolve //
#define FPS             30                   // Defines the rate of frames per second                               //
#define STREAM_INDEX_1  1                    // Left IR camera index                                               //
#define STREAM_INDEX_2  2                    // Right IR camera index                                              //
#define HEIGHT_RATIO    20                   // Defines the height ratio between the original frame to the new frame//
#define WIDTH_RATIO     10                   // Defines the width ratio between the original frame to the new frame //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Global flag to control the main loop
static volatile int running = 1;

// Signal handler for Ctrl+C
void signal_handler(int sig)
{
    printf("\nReceived signal %d, stopping stream...\n", sig);
    running = 0;
}

int main(void)
{
    rs2_error* e = 0;

    // Register signal handler for Ctrl+C
    signal(SIGINT, signal_handler);

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
    rs2_pipeline* pipeline = rs2_create_pipeline(ctx, &e);
    check_error(e);

    // Create a config instance, used to specify hardware configuration
    // The returned object should be released with rs2_delete_config(...)
    rs2_config* config = rs2_create_config(&e);
    check_error(e);

    // Request IR streams - both left (index 1) and right (index 2)
    rs2_config_enable_stream(config, STREAM, STREAM_INDEX_1, WIDTH, HEIGHT, FORMAT, FPS, &e);
    check_error(e);
    rs2_config_enable_stream(config, STREAM, STREAM_INDEX_2, WIDTH, HEIGHT, FORMAT, FPS, &e);
    check_error(e);

    // Start the pipeline streaming
    // The returned object should be released with rs2_delete_pipeline_profile(...)
    rs2_pipeline_profile* pipeline_profile = rs2_pipeline_start_with_config(pipeline, config, &e);
    if (e)
    {
        printf("The connected device doesn't support infrared streaming!\n");
        exit(EXIT_FAILURE);
    }

    rs2_stream_profile_list* stream_profile_list = rs2_pipeline_profile_get_streams(pipeline_profile, &e);
    if (e)
    {
        printf("Failed to create stream profile list!\n");
        exit(EXIT_FAILURE);
    }

    // Get stream profiles for both IR cameras
    int num_profiles = rs2_get_stream_profiles_count(stream_profile_list, &e);
    check_error(e);
    
    printf("Found %d stream profiles\n", num_profiles);
    
    int width = 0, height = 0;
    int i;
    for (i = 0; i < num_profiles; i++)
    {
        rs2_stream_profile* stream_profile = (rs2_stream_profile*)rs2_get_stream_profile(stream_profile_list, i, &e);
        check_error(e);
        
        rs2_stream stream; rs2_format format; int index; int unique_id; int framerate;
        rs2_get_stream_profile_data(stream_profile, &stream, &format, &index, &unique_id, &framerate, &e);
        check_error(e);
        
        if (stream == RS2_STREAM_INFRARED)
        {
            rs2_get_video_stream_resolution(stream_profile, &width, &height, &e);
            check_error(e);
            printf("IR Stream %d: %dx%d @ %d fps\n", index, width, height, framerate);
        }
    }

    if (width == 0 || height == 0)
    {
        printf("Failed to get IR stream resolution!\n");
        exit(EXIT_FAILURE);
    }

    int rows = height / HEIGHT_RATIO;
    int row_length = width / WIDTH_RATIO;
    int display_size = (rows + 1) * (row_length + 1);
    
    char* buffer_left = calloc(display_size, sizeof(char));
    char* buffer_right = calloc(display_size, sizeof(char));

    printf("Starting IR streaming... Press Ctrl+C to stop\n\n");

    while (running)  // Until user presses Ctrl+C
    {
        // This call waits until a new composite_frame is available
        // composite_frame holds a set of frames. It is used to prevent frame drops
        // The returned object should be released with rs2_release_frame(...)
        rs2_frame* frames = rs2_pipeline_wait_for_frames(pipeline, RS2_DEFAULT_TIMEOUT, &e);
        check_error(e);

        // Returns the number of frames embedded within the composite frame
        int num_of_frames = rs2_embedded_frames_count(frames, &e);
        check_error(e);

        // Process each frame
        for (i = 0; i < num_of_frames; ++i)
        {
            // The returned object should be released with rs2_release_frame(...)
            rs2_frame* frame = rs2_extract_frame(frames, i, &e);
            check_error(e);

            // Get frame profile information
            rs2_stream_profile* profile = (rs2_stream_profile*)rs2_get_frame_stream_profile(frame, &e);
            check_error(e);
            
            rs2_stream stream; rs2_format format; int index; int unique_id; int framerate;
            rs2_get_stream_profile_data(profile, &stream, &format, &index, &unique_id, &framerate, &e);
            check_error(e);

            // Process only infrared frames
            if (stream == RS2_STREAM_INFRARED)
            {
                /* Retrieve IR data, configured as 8-bit grayscale values */
                const uint8_t* ir_frame_data = (const uint8_t*)(rs2_get_frame_data(frame, &e));
                check_error(e);

                char* buffer = (index == STREAM_INDEX_1) ? buffer_left : buffer_right;
                char* out = buffer;
                
                /* Print a simple text-based representation of the IR image */
                int x, y, j;
                int* intensity = calloc(row_length, sizeof(int));

                for (y = 0; y < height; ++y)
                {
                    for (x = 0; x < width; ++x)
                    {
                        // Create an intensity histogram for each row
                        int intensity_index = x / WIDTH_RATIO;
                        int ir_value = *ir_frame_data++;
                        intensity[intensity_index] += ir_value;
                    }

                    if ((y % HEIGHT_RATIO) == (HEIGHT_RATIO-1))
                    {
                        for (j = 0; j < row_length; ++j)
                        {
                            // Map intensity to ASCII characters (darker to brighter)
                            static const char pixels[] = " .:-=+*#%@";
                            int avg_intensity = intensity[j] / (HEIGHT_RATIO * WIDTH_RATIO);
                            int pixel_index = (avg_intensity * (sizeof(pixels) - 2)) / 255;
                            if (pixel_index >= (int)sizeof(pixels) - 1) pixel_index = (int)sizeof(pixels) - 2;
                            *out++ = pixels[pixel_index];
                            intensity[j] = 0;
                        }
                        *out++ = '\n';
                    }
                }
                *out++ = 0;

                // Display side by side when we have both frames
                if (index == STREAM_INDEX_2) // Right IR frame
                {
                    printf("\033[H\033[J"); // Clear screen
                    printf("Left IR (Index %d)%*sRight IR (Index %d)\n", 
                           STREAM_INDEX_1, row_length - 15, "", STREAM_INDEX_2);
                    printf("%s", "=");
                    for (j = 0; j < row_length; j++) printf("=");
                    printf("%*s", 5, "");
                    for (j = 0; j < row_length; j++) printf("=");
                    printf("\n");
                    
                    // Print both buffers side by side
                    char* left_line = buffer_left;
                    char* right_line = buffer_right;
                    char* left_next, * right_next;
                    
                    while (*left_line && *right_line)
                    {
                        left_next = strchr(left_line, '\n');
                        right_next = strchr(right_line, '\n');
                        
                        if (left_next) *left_next = 0;
                        if (right_next) *right_next = 0;
                        
                        printf("%-*s     %s\n", row_length, left_line, right_line);
                        
                        if (left_next) 
                        {
                            *left_next = '\n';
                            left_line = left_next + 1;
                        }
                        else break;
                        
                        if (right_next)
                        {
                            *right_next = '\n';
                            right_line = right_next + 1;
                        }
                        else break;
                    }
                }

                free(intensity);
            }

            rs2_release_frame(frame);
        }

        rs2_release_frame(frames);
    }

    printf("Stopping pipeline...\n");
    rs2_pipeline_stop(pipeline, &e);
    check_error(e);

    free(buffer_left);
    free(buffer_right);
    
    // Cleanup
    rs2_delete_config(config);
    rs2_delete_pipeline(pipeline);
    rs2_delete_pipeline_profile(pipeline_profile);
    rs2_delete_device(dev);
    rs2_delete_device_list(device_list);
    rs2_delete_context(ctx);
    
    return EXIT_SUCCESS;
}
