/* License: Apache 2.0. See LICENSE file in root directory.
   Copyright(c) 2017 RealSense, Inc. All Rights Reserved. */

/** \file rs_internal.h
* \brief
* Exposes RealSense internal functionality for C compilers
*/


#ifndef LIBREALSENSE_RS2_INTERNAL_H
#define LIBREALSENSE_RS2_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif
#include "rs_types.h"
#include "rs_context.h"
#include "rs_sensor.h"
#include "rs_frame.h"
#include "rs_option.h"

/**
* Firmware size constants
*/
    const unsigned int signed_fw_size = 0x18031C;
    const unsigned int unsigned_fw_size = 0x200000;

/**
 * librealsense Recorder is intended for effective unit-testing
 * Currently supports three modes of operation:
 */
typedef enum rs2_recording_mode
{
    RS2_RECORDING_MODE_BLANK_FRAMES, /* frame metadata will be recorded, but pixel data will be replaced with zeros to save space */
    RS2_RECORDING_MODE_COMPRESSED,   /* frames will be encoded using a proprietary lossy encoding, aiming at x5 compression at some CPU expense */
    RS2_RECORDING_MODE_BEST_QUALITY, /* frames will not be compressed, but rather stored as-is. This gives best quality and low CPU overhead, but you might run out of memory */
    RS2_RECORDING_MODE_COUNT
} rs2_recording_mode;

/** \brief All the parameters required to define a video stream. */
typedef struct rs2_video_stream
{
    rs2_stream type;
    int index;
    int uid;
    int width;
    int height;
    int fps;
    int bpp;
    rs2_format fmt;
    rs2_intrinsics intrinsics;
} rs2_video_stream;

/** \brief All the parameters required to define a motion stream. */
typedef struct rs2_motion_stream
{
    rs2_stream type;
    int index;
    int uid;
    int fps;
    rs2_format fmt;
    rs2_motion_device_intrinsic intrinsics;
} rs2_motion_stream;

/** \brief All the parameters required to define a pose stream. */
typedef struct rs2_pose_stream
{
    rs2_stream type;
    int index;
    int uid;
    int fps;
    rs2_format fmt;
} rs2_pose_stream;

/** \brief All the parameters required to define an inference stream. */
typedef struct rs2_inference_stream
{
    rs2_stream type;
    int index;
    int uid;
    int fps;
    rs2_format fmt;
} rs2_inference_stream;

/** \brief All the parameters required to define a video frame. */
typedef struct rs2_software_video_frame
{
    void* pixels;
    void(*deleter)(void*);
    int stride;
    int bpp;
    rs2_time_t timestamp;
    rs2_timestamp_domain domain;
    int frame_number;
    const rs2_stream_profile* profile;
    float depth_units;
} rs2_software_video_frame;

/** \brief All the parameters required to define a motion frame. */
typedef struct rs2_software_motion_frame
{
    void* data;
    void(*deleter)(void*);
    rs2_time_t timestamp;
    rs2_timestamp_domain domain;
    int frame_number;
    const rs2_stream_profile* profile;
} rs2_software_motion_frame;

/** \brief All the parameters required to define a pose frame. */
typedef struct rs2_software_pose_frame
{
    struct pose_frame_info
    {
        float translation[3];
        float velocity[3];
        float acceleration[3];
        float rotation[4];
        float angular_velocity[3];
        float angular_acceleration[3];
        int tracker_confidence;
        int mapper_confidence;
    };
    void* data;
    void(*deleter)(void*);
    rs2_time_t timestamp;
    rs2_timestamp_domain domain;
    int frame_number;
    const rs2_stream_profile* profile;
} rs2_software_pose_frame;

/** \brief All the parameters required to define a sensor notification. */
typedef struct rs2_software_notification
{
    rs2_notification_category category;
    int type;
    rs2_log_severity severity;
    const char* description;
    const char* serialized_data;
} rs2_software_notification;

struct rs2_software_device_destruction_callback;

/**
 * Create librealsense context that will try to record all operations over librealsense into a file
 * \deprecated
 * \param[in] api_version realsense API version as provided by RS2_API_VERSION macro
 * \param[in] filename string representing the name of the file to record
 * \param[in] section  string representing the name of the section within existing recording
 * \param[out] error  if non-null, receives any error that occurs during this call, otherwise, errors are ignored
 * \return            context object, should be released by rs2_delete_context
 */
rs2_context* rs2_create_recording_context(int api_version, const char* filename, const char* section, rs2_recording_mode mode, rs2_error** error);

/**
 * Create librealsense context that given a file will respond to calls exactly as the recording did
 * if the user calls a method that was either not called during recording or violates causality of the recording error will be thrown
 * \deprecated
 * \param[in] api_version realsense API version as provided by RS2_API_VERSION macro
 * \param[in] filename string representing the name of the file to play back from
 * \param[in] section  string representing the name of the section within existing recording
 * \param[out] error  if non-null, receives any error that occurs during this call, otherwise, errors are ignored
 * \return            context object, should be released by rs2_delete_context
 */
rs2_context* rs2_create_mock_context(int api_version, const char* filename, const char* section, rs2_error** error);

/**
 * Create librealsense context that given a file will respond to calls exactly as the recording did
 * if the user calls a method that was either not called during recording or violates causality of the recording error will be thrown
 * \deprecated
 * \param[in] api_version realsense API version as provided by RS2_API_VERSION macro
 * \param[in] filename string representing the name of the file to play back from
 * \param[in] section  string representing the name of the section within existing recording
 * \param[in] min_api_version reject any file that was recorded before this version
 * \param[out] error  if non-null, receives any error that occurs during this call, otherwise, errors are ignored
 * \return            context object, should be released by rs2_delete_context
 */
rs2_context* rs2_create_mock_context_versioned(int api_version, const char* filename, const char* section, const char* min_api_version, rs2_error** error);

/**
 * Create software device to enable use librealsense logic without getting data from backend
 * but inject the data from outside
 * \param[out] error  if non-null, receives any error that occurs during this call, otherwise, errors are ignored
 * \return            software device object, should be released by rs2_delete_device
 */
rs2_device* rs2_create_software_device(rs2_error** error);

/**
 * Add sensor to the software device
 * \param[in] dev the software device
 * \param[in] sensor_name the name of the sensor
 * \param[out] error  if non-null, receives any error that occurs during this call, otherwise, errors are ignored
 * \return            software sensor object, should be released by rs2_delete_sensor
 */
rs2_sensor* rs2_software_device_add_sensor(rs2_device* dev, const char* sensor_name, rs2_error** error);

/**
 * Inject video frame to software sonsor
 * \param[in] sensor the software sensor
 * \param[in] frame all the frame components
 * \param[out] error  if non-null, receives any error that occurs during this call, otherwise, errors are ignored
 */
void rs2_software_sensor_on_video_frame(rs2_sensor* sensor, rs2_software_video_frame frame, rs2_error** error);

/**
* Inject motion frame to software sonsor
* \param[in] sensor the software sensor
* \param[in] frame all the frame components
* \param[out] error  if non-null, receives any error that occurs during this call, otherwise, errors are ignored
*/
void rs2_software_sensor_on_motion_frame(rs2_sensor* sensor, rs2_software_motion_frame frame, rs2_error** error);

/**
* Inject pose frame to software sonsor
* \param[in] sensor the software sensor
* \param[in] frame all the frame components
* \param[out] error  if non-null, receives any error that occurs during this call, otherwise, errors are ignored
*/
void rs2_software_sensor_on_pose_frame(rs2_sensor* sensor, rs2_software_pose_frame frame, rs2_error** error);


/**
* Inject notification to software sonsor
* \param[in] sensor the software sensor
* \param[in] notif all the notification components
* \param[out] error  if non-null, receives any error that occurs during this call, otherwise, errors are ignored
*/
void rs2_software_sensor_on_notification(rs2_sensor* sensor, rs2_software_notification notif, rs2_error** error);

/**
* Set frame metadata for the upcoming frames
* \param[in] sensor the software sensor
* \param[in] value metadata key to set
* \param[in] type metadata value
* \param[out] error  if non-null, receives any error that occurs during this call, otherwise, errors are ignored
*/
void rs2_software_sensor_set_metadata(rs2_sensor* sensor, rs2_frame_metadata_value value, rs2_metadata_type type, rs2_error** error);

/**
* set callback to be notified when a specific software device is destroyed
* \param[in] dev             software device
* \param[in] on_notification function pointer to register as callback
* \param[out] error          if non-null, receives any error that occurs during this call, otherwise, errors are ignored
*/
void rs2_software_device_set_destruction_callback(const rs2_device* dev, rs2_software_device_destruction_callback_ptr on_notification, void* user, rs2_error** error);

/**
* set callback to be notified when a specific software device is destroyed
* \param[in] dev      software device
* \param[in] callback callback object created from c++ application. ownership over the callback object is moved into the relevant device lock
* \param[out] error   if non-null, receives any error that occurs during this call, otherwise, errors are ignored
*/
void rs2_software_device_set_destruction_callback_cpp(const rs2_device* dev, rs2_software_device_destruction_callback* callback, rs2_error** error);

/**
 * Set the wanted matcher type that will be used by the syncer
 * \param[in] dev the software device
 * \param[in] matcher matcher type
 * \param[out] error  if non-null, receives any error that occurs during this call, otherwise, errors are ignored
 */
void rs2_software_device_create_matcher(rs2_device* dev, rs2_matchers matcher, rs2_error** error);

/**
 * Register a camera info value for the software device
 * \param[in] dev the software device
 * \param[in] info identifier for the camera info to add.
 * \param[in] val string value for this new camera info.
 * \param[out] error  if non-null, receives any error that occurs during this call, otherwise, errors are ignored
 */
void rs2_software_device_register_info(rs2_device* dev, rs2_camera_info info, const char *val, rs2_error** error);

/**
 * Update an existing camera info value for the software device
 * \param[in] dev the software device
 * \param[in] info identifier for the camera info to add.
 * \param[in] val string value for this new camera info.
 * \param[out] error  if non-null, receives any error that occurs during this call, otherwise, errors are ignored
 */
void rs2_software_device_update_info(rs2_device* dev, rs2_camera_info info, const char * val, rs2_error** error);

/**
 * Add video stream to sensor
 * \param[in] sensor the software sensor
 * \param[in] video_stream all the stream components
 * \param[out] error  if non-null, receives any error that occurs during this call, otherwise, errors are ignored
 */
rs2_stream_profile* rs2_software_sensor_add_video_stream(rs2_sensor* sensor, rs2_video_stream video_stream, rs2_error** error);

/**
 * Add video stream to sensor
 * \param[in] sensor the software sensor
 * \param[in] video_stream all the stream components
 * \param[in] is_default whether or not the stream should be a default stream for the device
 * \param[out] error  if non-null, receives any error that occurs during this call, otherwise, errors are ignored
 */
rs2_stream_profile* rs2_software_sensor_add_video_stream_ex(rs2_sensor* sensor, rs2_video_stream video_stream, int is_default, rs2_error** error);

/**
* Add motion stream to sensor
* \param[in] sensor the software sensor
* \param[in] motion_stream all the stream components
* \param[out] error  if non-null, receives any error that occurs during this call, otherwise, errors are ignored
*/
rs2_stream_profile* rs2_software_sensor_add_motion_stream(rs2_sensor* sensor, rs2_motion_stream motion_stream, rs2_error** error);

/**
* Add motion stream to sensor
* \param[in] sensor the software sensor
* \param[in] motion_stream all the stream components
* \param[in] is_default whether or not the stream should be a default stream for the device
* \param[out] error  if non-null, receives any error that occurs during this call, otherwise, errors are ignored
*/
rs2_stream_profile* rs2_software_sensor_add_motion_stream_ex(rs2_sensor* sensor, rs2_motion_stream motion_stream, int is_default, rs2_error** error);

/**
* Add pose stream to sensor
* \param[in] sensor the software sensor
* \param[in] pose_stream all the stream components
* \param[out] error  if non-null, receives any error that occurs during this call, otherwise, errors are ignored
*/
rs2_stream_profile* rs2_software_sensor_add_pose_stream(rs2_sensor* sensor, rs2_pose_stream pose_stream, rs2_error** error);

/**
* Add pose stream to sensor
* \param[in] sensor the software sensor
* \param[in] pose_stream all the stream components
* \param[in] is_default whether or not the stream should be a default stream for the device
* \param[out] error  if non-null, receives any error that occurs during this call, otherwise, errors are ignored
*/
rs2_stream_profile* rs2_software_sensor_add_pose_stream_ex(rs2_sensor* sensor, rs2_pose_stream pose_stream, int is_default, rs2_error** error);

/**
* Add inference stream to sensor
* \param[in] sensor the software sensor
* \param[in] inference_stream all the stream components
* \param[out] error  if non-null, receives any error that occurs during this call, otherwise, errors are ignored
*/
rs2_stream_profile* rs2_software_sensor_add_inference_stream(rs2_sensor* sensor, rs2_inference_stream inference_stream, rs2_error** error);

/**
* Add inference stream to sensor
* \param[in] sensor the software sensor
* \param[in] inference_stream all the stream components
* \param[in] is_default whether or not the stream should be a default stream for the device
* \param[out] error  if non-null, receives any error that occurs during this call, otherwise, errors are ignored
*/
rs2_stream_profile* rs2_software_sensor_add_inference_stream_ex(rs2_sensor* sensor, rs2_inference_stream inference_stream, int is_default, rs2_error** error);

/**
 * Add read only option to sensor
 * \param[in] sensor the software sensor
 * \param[in] option the wanted option
 * \param[in] val the initial value
 * \param[out] error  if non-null, receives any error that occurs during this call, otherwise, errors are ignored
 */
void rs2_software_sensor_add_read_only_option(rs2_sensor* sensor, rs2_option option, float val, rs2_error** error);

/**
 * Update the read only option added to sensor
 * \param[in] sensor the software sensor
 * \param[in] option the wanted option
 * \param[in] val the wanted value
 * \param[out] error  if non-null, receives any error that occurs during this call, otherwise, errors are ignored
 */
void rs2_software_sensor_update_read_only_option(rs2_sensor* sensor, rs2_option option, float val, rs2_error** error);

/**
 * Add an option to sensor
 * \param[in] sensor        the software sensor
 * \param[in] option        the wanted option
 * \param[in] min           the minimum value which will be accepted for this option
 * \param[in] max           the maximum value which will be accepted for this option
 * \param[in] step          the granularity of options which accept discrete values, or zero if the option accepts continuous values
 * \param[in] def           the initial value of the option
 * \param[in] is_writable   should the option be read-only or not
 * \param[out] error  if non-null, receives any error that occurs during this call, otherwise, errors are ignored
 */
void rs2_software_sensor_add_option(rs2_sensor* sensor, rs2_option option, float min, float max, float step, float def, int is_writable, rs2_error** error);

/**
* Sensors hold the parent device in scope via a shared_ptr. This function detaches that so that the software sensor doesn't keep the software device alive.
* Note that this is dangerous as it opens the door to accessing freed memory if care isn't taken.
* \param[in] sensor         the software sensor
* \param[out] error         if non-null, recieves any error that occurs during this call, otherwise, errors are ignored
*/
void rs2_software_sensor_detach(rs2_sensor* sensor, rs2_error** error);


/**
 * \brief Starts collecting FW log messages in the device.
 * \param[in] dev            Device that will start collecting log messages.
 * \param[out] error         If non-null, receives any error that occurs during this call, otherwise, errors are
 */
void rs2_start_collecting_fw_logs( rs2_device * dev, rs2_error ** error );

/**
 * \brief Stops collecting FW log messages in the device.
 * \param[in] dev            Device that will stop collecting log messages.
 * \param[out] error         If non-null, receives any error that occurs during this call, otherwise, errors are
 */
void rs2_stop_collecting_fw_logs( rs2_device * dev, rs2_error ** error );

/**
* \brief Creates RealSense firmware log message.
* \param[in] dev            Device from which the FW log will be taken using the created message
* \param[out] error         If non-null, receives any error that occurs during this call, otherwise, errors are ignored.
* \return                   pointer to created empty firmware log message
*/
rs2_firmware_log_message* rs2_create_fw_log_message(rs2_device* dev, rs2_error** error);

/**
* \brief Gets RealSense firmware log.
* \param[in] dev            Device from which the FW log should be taken
* \param[in] fw_log_msg     Firmware log message object to be filled
* \param[out] error         If non-null, receives any error that occurs during this call, otherwise, errors are ignored.
* \return                   true for success, false for failure - failure happens if no firmware log was sent by the hardware monitor
*/
int rs2_get_fw_log(rs2_device* dev, rs2_firmware_log_message* fw_log_msg, rs2_error** error);

/**
* \brief Gets RealSense flash log - this is a fw log that has been written in the device during the previous shutdown of the device
* \param[in] dev            Device from which the FW log should be taken
* \param[in] fw_log_msg     Firmware log message object to be filled
* \param[out] error         If non-null, receives any error that occurs during this call, otherwise, errors are ignored.
* \return                   true for success, false for failure - failure happens if no firmware log was sent by the hardware monitor
*/
int rs2_get_flash_log(rs2_device* dev, rs2_firmware_log_message* fw_log_msg, rs2_error** error);

/**
* Delete RealSense firmware log message
* \param[in]  device    Realsense firmware log message to delete
*/
void rs2_delete_fw_log_message(rs2_firmware_log_message* msg);

/**
* \brief Gets RealSense firmware log message data.
* \param[in] msg        firmware log message object
* \param[out] error     If non-null, receives any error that occurs during this call, otherwise, errors are ignored.
* \return               pointer to start of the firmware log message data
*/
const unsigned char* rs2_fw_log_message_data(rs2_firmware_log_message* msg, rs2_error** error);

/**
* \brief Gets RealSense firmware log message size.
* \param[in] msg        firmware log message object
* \param[out] error     If non-null, receives any error that occurs during this call, otherwise, errors are ignored.
* \return               size of the firmware log message data
*/
int rs2_fw_log_message_size(rs2_firmware_log_message* msg, rs2_error** error);


/**
* \brief Gets RealSense firmware log message timestamp.
* \param[in] msg        firmware log message object
* \param[out] error     If non-null, receives any error that occurs during this call, otherwise, errors are ignored.
* \return               timestamp of the firmware log message
*/
unsigned int rs2_fw_log_message_timestamp(rs2_firmware_log_message* msg, rs2_error** error);

/**
* \brief Gets RealSense firmware log message severity.
* \param[in] msg        firmware log message object
* \param[out] error     If non-null, receives any error that occurs during this call, otherwise, errors are ignored.
* \return               severity of the firmware log message data
*/
rs2_log_severity rs2_fw_log_message_severity(const rs2_firmware_log_message* msg, rs2_error** error);

/**
* \brief Initializes RealSense firmware logs parser in device.
* \param[in] dev            Device from which the FW log will be taken
* \param[in] xml_content    content of the xml file needed for parsing
* \param[out] error         If non-null, receives any error that occurs during this call, otherwise, errors are ignored.
* \return                   true for success, false for failure - failure happens if opening the xml from the xml_path input fails
*/
int rs2_init_fw_log_parser(rs2_device* dev, const char* xml_content, rs2_error** error);


/**
* \brief Creates RealSense firmware log parsed message.
* \param[in] dev            Device from which the FW log will be taken using the created message
* \param[out] error         If non-null, receives any error that occurs during this call, otherwise, errors are ignored.
* \return                   pointer to created empty firmware log message
*/
rs2_firmware_log_parsed_message* rs2_create_fw_log_parsed_message(rs2_device* dev, rs2_error** error);

/**
* \brief Deletes RealSense firmware log parsed message.
* \param[in] msg            message to be deleted
*/
void rs2_delete_fw_log_parsed_message(rs2_firmware_log_parsed_message* fw_log_parsed_msg);


/**
* \brief Gets RealSense firmware log parser
* \param[in] dev                Device from which the FW log will be taken
* \param[in] fw_log_msg         firmware log message to be parsed
* \param[in] parsed_msg         firmware log parsed message - place holder for the resulting parsed message
* \param[out] error             If non-null, receives any error that occurs during this call, otherwise, errors are ignored.
* \return                       true for success, false for failure - failure happens if message could not be parsed
*/
int rs2_parse_firmware_log(rs2_device* dev, rs2_firmware_log_message* fw_log_msg, rs2_firmware_log_parsed_message* parsed_msg, rs2_error** error);

/**
* \brief Returns number of fw logs already polled from device but not by user yet
* \param[in] dev                Device from which the FW log will be taken
* \param[out] error             If non-null, receives any error that occurs during this call, otherwise, errors are ignored.
* \return                       number of fw logs already polled from device but not by user yet
*/
unsigned int rs2_get_number_of_fw_logs(rs2_device* dev, rs2_error** error);
/**
* \brief Gets RealSense firmware log parsed message.
* \param[in] fw_log_parsed_msg      firmware log parsed message object
* \param[out] error     If non-null, receives any error that occurs during this call, otherwise, errors are ignored.
* \return               message of the firmware log parsed message
*/
const char* rs2_get_fw_log_parsed_message(rs2_firmware_log_parsed_message* fw_log_parsed_msg, rs2_error** error);

/**
* \brief Gets RealSense firmware log parsed message file name.
* \param[in] fw_log_parsed_msg      firmware log parsed message object
* \param[out] error     If non-null, receives any error that occurs during this call, otherwise, errors are ignored.
* \return               file name of the firmware log parsed message
*/
const char* rs2_get_fw_log_parsed_file_name(rs2_firmware_log_parsed_message* fw_log_parsed_msg, rs2_error** error);

/**
* \brief Gets RealSense firmware log parsed message source (SoC) or thread name.
* \param[in] fw_log_parsed_msg      firmware log parsed message object
* \param[out] error                 If non-null, receives any error that occurs during this call, otherwise, errors are ignored.
* \return                           source (SoC) or thread name of the firmware log parsed message
*/
const char* rs2_get_fw_log_parsed_thread_name(rs2_firmware_log_parsed_message* fw_log_parsed_msg, rs2_error** error);

/**
 * \brief Gets RealSense firmware log parsed message module name.
 * \param[in] fw_log_parsed_msg      firmware log parsed message object
 * \param[out] error                 If non-null, receives any error that occurs during this call, otherwise, errors are
 * ignored. \return                  module name of the firmware log parsed message
 */
const char * rs2_get_fw_log_parsed_module_name( rs2_firmware_log_parsed_message * fw_log_parsed_msg,
                                                rs2_error ** error );


/**
* \brief Gets RealSense firmware log parsed message severity.
* \param[in] fw_log_parsed_msg      firmware log parsed message object
* \param[out] error                 If non-null, receives any error that occurs during this call, otherwise, errors are ignored.
* \return                           severity of the firmware log parsed message
*/
rs2_log_severity rs2_get_fw_log_parsed_severity(rs2_firmware_log_parsed_message* fw_log_parsed_msg, rs2_error** error);

/**
* \brief Gets RealSense firmware log parsed message relevant line (in the file that is returned by rs2_get_fw_log_parsed_file_name).
* \param[in] fw_log_parsed_msg      firmware log parsed message object
* \param[out] error                 If non-null, receives any error that occurs during this call, otherwise, errors are ignored.
* \return                           line number of the firmware log parsed message
*/
unsigned int rs2_get_fw_log_parsed_line(rs2_firmware_log_parsed_message* fw_log_parsed_msg, rs2_error** error);

/**
* \brief Gets RealSense firmware log parsed message timestamp
* \param[in] fw_log_parsed_msg      firmware log parsed message object
* \param[out] error                 If non-null, receives any error that occurs during this call, otherwise, errors are ignored.
* \return                           timestamp of the firmware log parsed message
*/
unsigned int rs2_get_fw_log_parsed_timestamp(rs2_firmware_log_parsed_message* fw_log_parsed_msg, rs2_error** error);

/**
* \brief Gets RealSense firmware log parsed message sequence id - cyclic number of FW log with [0..15] range
* \param[in] fw_log_parsed_msg      firmware log parsed message object
* \param[out] error                 If non-null, receives any error that occurs during this call, otherwise, errors are ignored.
* \return                           sequence of the firmware log parsed message
*/
unsigned int rs2_get_fw_log_parsed_sequence_id(rs2_firmware_log_parsed_message* fw_log_parsed_msg, rs2_error** error);

/**
* \brief Creates RealSense terminal parser.
* \param[in] xml_content    content of the xml file needed for parsing
* \param[out] error         If non-null, receives any error that occurs during this call, otherwise, errors are ignored.
* \return                   pointer to created terminal parser object
*/
rs2_terminal_parser* rs2_create_terminal_parser(const char* xml_content, rs2_error** error);

/**
* \brief Deletes RealSense terminal parser.
* \param[in] terminal_parser            terminal parser to be deleted
*/
void rs2_delete_terminal_parser(rs2_terminal_parser* terminal_parser);

/**
* \brief Parses terminal command via RealSense terminal parser
* \param[in] terminal_parser        Terminal parser object
* \param[in] command                command to be sent to the hw monitor of the device
* \param[in] size_of_command        size of command to be sent to the hw monitor of the device
* \param[out] error                 If non-null, receives any error that occurs during this call, otherwise, errors are ignored.
* \return                           command to hw monitor, in hex
*/
rs2_raw_data_buffer* rs2_terminal_parse_command(rs2_terminal_parser* terminal_parser,
    const char* command, unsigned int size_of_command, rs2_error** error);

/**
* \brief Parses terminal response via RealSense terminal parser
* \param[in] terminal_parser        Terminal parser object
* \param[in] command                command sent to the hw monitor of the device
* \param[in] size_of_command        size of the command to sent to the hw monitor of the device
* \param[in] response               response received by the hw monitor of the device
* \param[in] size_of_response       size of the response received by the hw monitor of the device
* \param[out] error                 If non-null, receives any error that occurs during this call, otherwise, errors are ignored.
* \return                           answer parsed
*/
rs2_raw_data_buffer* rs2_terminal_parse_response(rs2_terminal_parser* terminal_parser,
    const char* command, unsigned int size_of_command,
    const void* response, unsigned int size_of_response, rs2_error** error);


#ifdef __cplusplus
}
#endif
#endif
