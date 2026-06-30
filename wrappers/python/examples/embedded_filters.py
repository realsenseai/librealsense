## License: Apache 2.0. See LICENSE file in root directory.
## Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#####################################################
##                   Embedded Filters              ##
#####################################################


import sys
import pyrealsense2 as rs


'''
Aim of this example is to show how the python API for embedded filters must be used.
Scenario is:
1. Get connected DDS device
2. Get its depth sensor
3. Get its embedded filters
4. For each embedded filter: show supported options and print current values
'''

def list_embedded_filter_options(embedded_filter):
    options = embedded_filter.get_supported_options()
    print("Supported options:")
    for opt in options:
        print(repr(opt) + ": " + str(embedded_filter.get_option_value(opt).value))
    print("\n")


def main(arguments=None):
    ctx = rs.context()
    try:
        device = ctx.query_devices()[0]
    except IndexError:
        print('Device is not connected')
        sys.exit(1)
    depth_sensor = device.first_depth_sensor()
    embedded_filters = depth_sensor.query_embedded_filters()
    if len(embedded_filters) == 0:
        print('No embedded filters found in this device')
        sys.exit(1)
    for filter in embedded_filters:
        if filter.get_type() == rs.embedded_filter_type.decimation:
            print("Decimation Embedded Filter found")
            list_embedded_filter_options(filter)
        elif filter.get_type() == rs.embedded_filter_type.temporal:
            print("Temporal Embedded Filter found")
            list_embedded_filter_options(filter)
        else:
            print("Embedded Filter found is of type: {}".format(filter.get_type()))


if __name__ == '__main__':
    main()
