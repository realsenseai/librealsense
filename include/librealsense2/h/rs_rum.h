/* License: Apache 2.0. See LICENSE file in root directory.
   Copyright(c) 2026 RealSense, Inc. All Rights Reserved. */

/** \file rs_rum.h
* \brief
* Exposes RUM (Real User Monitoring) functionality for C compilers.
*
* RUM collects anonymous, aggregated SDK usage statistics locally. Data only leaves
* the machine when the user explicitly opts in to cloud upload. These entry points are
* always compiled and functional: rs2_rum_get_report_path returns the path of the on-disk
* report file (SDK/system metadata plus whatever has been collected) and the consent get/set
* always read and write the per-user config file. The ENABLE_STATS build option (default OFF)
* gates only the instrumentation hooks that feed the collector; when it is off the report's
* collected lists (devices, streams, filters, options, notifications) stay empty, but
* the API itself behaves identically.
*/


#ifndef LIBREALSENSE_RS2_RUM_H
#define LIBREALSENSE_RS2_RUM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "rs_types.h"

/**
* Retrieve the path of the on-disk RUM report file (in the app-data folder). The SDK writes the
* accumulated report there when a context is destroyed; read that file to inspect or upload it. No
* upload is performed.
* \param[out] error  If non-null, receives any error that occurs during this call, otherwise, errors are ignored
* \return  A raw-data buffer holding the UTF-8 file path; release with rs2_delete_raw_data
*/
const rs2_raw_data_buffer* rs2_rum_get_report_path(rs2_error** error);

/**
* Set the cloud-upload consent flag. Persists to the per-user configuration file.
* \param[in]  enabled  Non-zero to opt in to cloud upload, zero to opt out
* \param[out] error    If non-null, receives any error that occurs during this call, otherwise, errors are ignored
*/
void rs2_rum_set_cloud_enabled(int enabled, rs2_error** error);

/**
* Query the resolved cloud-upload consent (RS2_RUM_CLOUD_ENABLED env var overrides the config file).
* \param[out] error  If non-null, receives any error that occurs during this call, otherwise, errors are ignored
* \return  Non-zero if cloud upload is enabled, zero otherwise
*/
int rs2_rum_is_cloud_enabled(rs2_error** error);

#ifdef __cplusplus
}
#endif
#endif
