/* License: Apache 2.0. See LICENSE file in root directory.
   Copyright(c) 2026 RealSense, Inc. All Rights Reserved. */

/** \file rs_nn.h
 * \brief
 * Exposes uploading neural-network models to the device for C compilers
 */

#ifndef LIBREALSENSE_RS2_NN_H
#define LIBREALSENSE_RS2_NN_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "rs_types.h"

/** \brief Where an uploaded NN model is stored */
typedef enum rs2_nn_target
{
    RS2_NN_TARGET_FLASH, /**< Stored in the model's flash slot, kept across reboots */
    RS2_NN_TARGET_DDR,   /**< Session only, lost on reboot. Not supported yet */
    RS2_NN_TARGET_COUNT  /**< Number of enumeration values. Not a valid input: intended to be used in for-loops. */
} rs2_nn_target;
const char * rs2_nn_target_to_string( rs2_nn_target target );

/**
 * Upload an NN model package from the Perception Service to the device.
 * The Perception sensor must not be streaming. When the install is done the SDK resets the camera: the device
 * disconnects and must be opened again, as after a firmware update. Firmware update on the device is refused while it
 * installs a model, uploaded by any process. If the upload does not complete, wait a minute before resetting or
 * reconnecting the camera: it may still be writing the model. Currently only RS2_NN_TARGET_FLASH, slot 0 and
 * activate != 0 are supported.
 * \param[in]  device       Device extendable to RS2_EXTENSION_NN_LOADER
 * \param[in]  file         Package content, sent as is
 * \param[in]  file_size    Package size in bytes
 * \param[in]  target       Where to store the model
 * \param[in]  slot         Model slot
 * \param[in]  activate     Non-zero to make the model the active one
 * \param[in]  callback     Optional progress callback, receives values in [0, 1]
 * \param[in]  client_data  Passed to the callback
 * \param[out] error        If non-null, receives any error that occurs during this call, otherwise, errors are ignored
 */
void rs2_nn_upload( const rs2_device * device, const void * file, int file_size, rs2_nn_target target, int slot,
                    int activate, rs2_update_progress_callback_ptr callback, void * client_data, rs2_error ** error );

/**
 * Same as rs2_nn_upload, with a callback object; takes ownership of the callback
 */
void rs2_nn_upload_cpp( const rs2_device * device, const void * file, int file_size, rs2_nn_target target, int slot,
                        int activate, rs2_update_progress_callback * callback, rs2_error ** error );

/**
 * Check whether the device supports NN model upload (product, firmware, not locked)
 * \param[in]  device       Device extendable to RS2_EXTENSION_NN_LOADER
 * \param[out] reason       If non-null, receives the reason when not supported, as a null-terminated string
 * \param[in]  reason_size  Size of the reason buffer
 * \param[out] error        If non-null, receives any error that occurs during this call, otherwise, errors are ignored
 * \return                  Non-zero if an upload is supported
 */
int rs2_nn_is_upload_supported( const rs2_device * device, char * reason, int reason_size, rs2_error ** error );

#ifdef __cplusplus
}
#endif
#endif
