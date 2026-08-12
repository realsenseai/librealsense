// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 RealSense, Inc. All Rights Reserved.

#include <librealsense2/rs.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

/* Function calls to librealsense may raise errors of type rs_error*/
void check_error(rs2_error* e)
{
    if (e)
    {
        printf("rs_error was raised when calling %s(%s):\n", rs2_get_failed_function(e), rs2_get_failed_args(e));
        printf("    %s\n", rs2_get_error_message(e));
        exit(EXIT_FAILURE);
    }
}

void print_device_info(rs2_device* dev)
{
    rs2_error* e = 0;
    printf("\nUsing device 0, an %s\n", rs2_get_device_info(dev, RS2_CAMERA_INFO_NAME, &e));
    check_error(e);
    printf("    Serial number: %s\n", rs2_get_device_info(dev, RS2_CAMERA_INFO_SERIAL_NUMBER, &e));
    check_error(e);
    printf("    Firmware version: %s\n\n", rs2_get_device_info(dev, RS2_CAMERA_INFO_FIRMWARE_VERSION, &e));
    check_error(e);
}

/* Wait up to 30 seconds for USB or asynchronous Ethernet/DDS discovery. */
static rs2_device* wait_for_device(rs2_context* ctx, rs2_error** e)
{
    int waited_sec = 0;
    printf("Waiting for a RealSense device (USB or Ethernet/DDS, up to 30s)...\n");

    while (waited_sec < 30)
    {
        rs2_device_list* list = rs2_query_devices_ex(
            ctx, RS2_PRODUCT_LINE_ANY | RS2_PRODUCT_LINE_SW_ONLY, e);
        if ((e && *e) || !list)
            return NULL;

        int count = rs2_get_device_count(list, e);
        if (e && *e)
        {
            rs2_delete_device_list(list);
            return NULL;
        }

        if (count > 0)
        {
            rs2_device* dev = rs2_create_device(list, 0, e);
            rs2_delete_device_list(list);
            return dev;
        }

        rs2_delete_device_list(list);
#ifdef _WIN32
        Sleep(1000);
#else
        sleep(1);
#endif
        ++waited_sec;
    }
    return NULL;
}
