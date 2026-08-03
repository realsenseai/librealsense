/* License: Apache 2.0. See LICENSE file in root directory.
   Copyright(c) 2026 RealSense, Inc. All Rights Reserved. */

/** \file rs_d500_config_tables.h
* \brief
* Exposes the D500 flash configuration tables at device level for C compilers.
* All D500 cameras share the same flash layout, so these are available on any D500 device,
* unlike the safety sensor which only the D585S exposes.
*/

#ifndef LIBREALSENSE_RS2_D500_CONFIG_TABLES_H
#define LIBREALSENSE_RS2_D500_CONFIG_TABLES_H

#ifdef __cplusplus
extern "C" {
#endif

#include "rs_types.h"

/**
* rs2_d500_get_safety_preset
* \param[in]   device        D500 device
* \param[in]   index         Index to read from
* \param[out]  error         If non-null, receives any error that occurs during this call, otherwise, errors are ignored
* \return                    JSON string representing the safety preset at the given index as rs2_raw_data_buffer
*/
const rs2_raw_data_buffer* rs2_d500_get_safety_preset(rs2_device const* device, int index, rs2_error** error);

/**
* rs2_d500_set_safety_preset
* \param[in]  device        D500 device
* \param[in]  index         Index to write to
* \param[in]  sp_json_str   Safety preset as JSON string
* \param[out] error         If non-null, receives any error that occurs during this call, otherwise, errors are ignored
*/
void rs2_d500_set_safety_preset(rs2_device const* device, int index, const char* sp_json_str, rs2_error** error);

/**
* rs2_d500_get_safety_interface_config
* \param[in]   device        D500 device
* \param[in]   loc           Calibration location that needs to be read from
* \param[out]  error         If non-null, receives any error that occurs during this call, otherwise, errors are ignored
* \return                    JSON string representing the safety interface config as rs2_raw_data_buffer
*/
const rs2_raw_data_buffer* rs2_d500_get_safety_interface_config(rs2_device const* device, rs2_calib_location loc, rs2_error** error);

/**
* rs2_d500_set_safety_interface_config
* \param[in]  device          D500 device
* \param[in]  sic_json_str    Safety interface config as JSON string
* \param[out] error           If non-null, receives any error that occurs during this call, otherwise, errors are ignored
*/
void rs2_d500_set_safety_interface_config(rs2_device const* device, const char* sic_json_str, rs2_error** error);

/**
* rs2_d500_get_application_config
* \param[in]  device        D500 device
* \param[out] error         If non-null, receives any error that occurs during this call, otherwise, errors are ignored
* \return                   JSON string representing the application config as rs2_raw_data_buffer
*/
const rs2_raw_data_buffer* rs2_d500_get_application_config(rs2_device const* device, rs2_error** error);

/**
* rs2_d500_set_application_config
* \param[in]  device                           D500 device
* \param[in]  application_config_json_str      Application config as JSON string
* \param[out] error                            If non-null, receives any error that occurs during this call, otherwise, errors are ignored
*/
void rs2_d500_set_application_config(rs2_device const* device, const char* application_config_json_str, rs2_error** error);

/**
* rs2_d500_set_parameters
* Set individual exposed parameters without handling whole tables. Each parameter is routed to its
* table(s) and field(s) and each affected table is read-modified-written once.
* \param[in]  device            D500 device
* \param[in]  params_json_str   Flat JSON object: { "<param name>": <value>, ... }
* \param[out] error             If non-null, receives any error that occurs during this call, otherwise, errors are ignored
*/
void rs2_d500_set_parameters(rs2_device const* device, const char* params_json_str, rs2_error** error);

/**
* rs2_d500_get_parameters
* \param[in]  device        D500 device
* \param[out] error         If non-null, receives any error that occurs during this call, otherwise, errors are ignored
* \return                   Flat JSON object with the current value of every exposed parameter, as rs2_raw_data_buffer
*/
const rs2_raw_data_buffer* rs2_d500_get_parameters(rs2_device const* device, rs2_error** error);

#ifdef __cplusplus
}
#endif
#endif  // LIBREALSENSE_RS2_D500_CONFIG_TABLES_H
