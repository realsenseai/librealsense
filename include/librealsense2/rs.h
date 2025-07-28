/* License: Apache 2.0. See LICENSE file in root directory.
   Copyright(c) 2017 Intel Corporation. All Rights Reserved. */

/** \file rs.h
* \brief
* Exposes librealsense functionality for C compilers
*/

#ifndef LIBREALSENSE_RS2_H
#define LIBREALSENSE_RS2_H

#ifdef __cplusplus
extern "C" {
#endif

#include "rsutil.h"
#include "h/rs_types.h"
#include "h/rs_context.h"
#include "h/rs_device.h"
#include "h/rs_frame.h"
#include "h/rs_option.h"
#include "h/rs_processing.h"
#include "h/rs_record_playback.h"
#include "h/rs_sensor.h"

#define RS2_API_MAJOR_VERSION    2
#define RS2_API_MINOR_VERSION    56
#define RS2_API_PATCH_VERSION    5
#define RS2_API_BUILD_VERSION    0

#ifndef STRINGIFY
#define STRINGIFY(arg) #arg
#endif
#ifndef VAR_ARG_STRING
#define VAR_ARG_STRING(arg) STRINGIFY(arg)
#endif

/* Versioning rules            : For each release at least one of [MJR/MNR/PTCH] triple is promoted                                             */
/*                             : Versions that differ by RS2_API_PATCH_VERSION only are interface-compatible, i.e. no user-code changes required */
/*                             : Versions that differ by MAJOR/MINOR VERSION component can introduce API changes                                */
/* Version in encoded integer format (1,9,x) -> 01090x. note that each component is limited into [0-99] range by design                         */
#define RS2_API_VERSION  (((RS2_API_MAJOR_VERSION) * 10000) + ((RS2_API_MINOR_VERSION) * 100) + (RS2_API_PATCH_VERSION))
/* Return version in "X.Y.Z" format */
#define RS2_API_VERSION_STR (VAR_ARG_STRING(RS2_API_MAJOR_VERSION.RS2_API_MINOR_VERSION.RS2_API_PATCH_VERSION))
#define RS2_API_FULL_VERSION_STR (VAR_ARG_STRING(RS2_API_MAJOR_VERSION.RS2_API_MINOR_VERSION.RS2_API_PATCH_VERSION.RS2_API_BUILD_VERSION))

/**
 * The library keeps persistent global settings in a per-user configuration file. These settings can relate to tool
 * behavior (e.g., realsense-viewer) or library functionality (context creation, etc.) where settings are accepted
 * (e.g., rs2_create_context_ex).
 * 
 * The filename is appended to the user's AppData folder on Windows (<AppData>/<filename>) and home directory as a
 * hidden file on Linux (~/.<filename>).
 * 
 * The content is a JSON file and by default contains an empty object so default values are used. JSON ordering may
 * shift when written to by the various tools but the contents should stay the same. Any values that do not match the
 * expected type may be ignored.
 */
#define RS2_CONFIG_FILENAME "realsense-config.json"

/**
* get the size of rs2_raw_data_buffer
* \param[in] buffer  pointer to rs2_raw_data_buffer returned by rs2_send_and_receive_raw_data
* \param[out] error  if non-null, receives any error that occurs during this call, otherwise, errors are ignored
* \return size of rs2_raw_data_buffer
*/
int rs2_get_raw_data_size(const rs2_raw_data_buffer* buffer, rs2_error** error);

/**
* Delete rs2_raw_data_buffer
* \param[in] buffer        rs2_raw_data_buffer returned by rs2_send_and_receive_raw_data
*/
void rs2_delete_raw_data(const rs2_raw_data_buffer* buffer);

/**
* Retrieve char array from rs2_raw_data_buffer
* \param[in] buffer   rs2_raw_data_buffer returned by rs2_send_and_receive_raw_data
* \param[out] error   if non-null, receives any error that occurs during this call, otherwise, errors are ignored
* \return raw data
*/
const unsigned char* rs2_get_raw_data(const rs2_raw_data_buffer* buffer, rs2_error** error);

/**
 * Retrieve the API version from the source code. Evaluate that the value is conformant to the established policies
 * \param[out] error  if non-null, receives any error that occurs during this call, otherwise, errors are ignored
 * \return            the version API encoded into integer value "1.9.3" -> 10903
 */
int rs2_get_api_version(rs2_error** error);

void rs2_log_to_console(rs2_log_severity min_severity, rs2_error ** error);

void rs2_log_to_file(rs2_log_severity min_severity, const char * file_path, rs2_error ** error);

void rs2_log_to_callback_cpp( rs2_log_severity min_severity, rs2_log_callback * callback, rs2_error ** error );

void rs2_log_to_callback( rs2_log_severity min_severity, rs2_log_callback_ptr callback, void * arg, rs2_error** error );

void rs2_reset_logger( rs2_error ** error);

/**
* Enable rolling log file when used with rs2_log_to_file:
* Upon reaching (max_size/2) bytes, the log will be renamed with an ".old" suffix and a new log created. Any
* previous .old file will be erased.
* Must have permissions to remove/rename files in log file directory.
* \param[in] max_size   max file size in megabytes
* \param[out] error     if non-null, receives any error that occurs during this call, otherwise, errors are ignored
*/
void rs2_enable_rolling_log_file( unsigned max_size, rs2_error ** error );


unsigned rs2_get_log_message_line_number( rs2_log_message const * msg, rs2_error** error );
const char * rs2_get_log_message_filename( rs2_log_message const * msg, rs2_error** error );
const char * rs2_get_raw_log_message( rs2_log_message const * msg, rs2_error** error );
const char * rs2_get_full_log_message( rs2_log_message const * msg, rs2_error** error );

/**
 * Add custom message into librealsense log
 * \param[in] severity  The log level for the message to be written under
 * \param[in] message   Message to be logged
 * \param[out] error  if non-null, receives any error that occurs during this call, otherwise, errors are ignored
 */
void rs2_log(rs2_log_severity severity, const char * message, rs2_error ** error);

/**
* Given the 2D depth coordinate (x,y) provide the corresponding depth in metric units
* \param[in] frame_ref  2D depth pixel coordinates (Left-Upper corner origin)
* \param[in] x,y  2D depth pixel coordinates (Left-Upper corner origin)
* \param[out] error  if non-null, receives any error that occurs during this call, otherwise, errors are ignored
*/
float rs2_depth_frame_get_distance(const rs2_frame* frame_ref, int x, int y, rs2_error** error);

/**
* return the time at specific time point
* \param[out] error  if non-null, receives any error that occurs during this call, otherwise, errors are ignored
* \return            the time at specific time point, in live and record mode it will return the system time and in playback mode it will return the recorded time
*/
rs2_time_t rs2_get_time( rs2_error** error);

void rs2_hw_monitor_get_opcode_string(int opcode, char* buffer, size_t buffer_size,rs2_device* device, rs2_error** error);

#ifdef __cplusplus
}
#endif
#endif
