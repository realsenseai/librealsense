// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017 RealSense, Inc. All Rights Reserved.

#include <librealsense2/rs.h>
#include <librealsense2/h/rs_context.h>
#include <stdio.h>
#include <stdlib.h>

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

/* Wait until a device is present (USB is immediate; Ethernet/DDS needs discovery).
 * Returns an owned device; caller must rs2_delete_device(). */
static rs2_device* wait_for_device(rs2_context* ctx, rs2_error** e)
{
    rs2_device_hub* hub = rs2_create_device_hub(ctx, e);
    if (e && *e)
        return NULL;
    printf("Waiting for a RealSense device (USB or Ethernet/DDS)...\n");
    rs2_device* dev = rs2_device_hub_wait_for_device(hub, e);
    rs2_delete_device_hub(hub);
    return dev;
}
