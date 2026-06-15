/* License: Apache 2.0. See LICENSE file in root directory.
   Copyright(c) 2026 RealSense, Inc. All Rights Reserved. */

/** \file rs_rum.h
* \brief
* Exposes RUM (Real User Monitoring) functionality for C compilers.
*
* RUM collects anonymous, aggregated SDK usage statistics locally. Data only leaves
* the machine when the user explicitly opts in to cloud upload. These entry points are
* always present; when the SDK is built with ENABLED_STATS=OFF (default is ON) they
* become inert (report/queries return empty, mutating calls are no-ops).
*/


#ifndef LIBREALSENSE_RS2_RUM_H
#define LIBREALSENSE_RS2_RUM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "rs_types.h"

/**
* Retrieve the live RUM report for the current session as a JSON buffer, reflecting everything
* collected so far in this process. The SDK also persists the report to the app-data folder when a
* context is destroyed (for later upload), but this call reads the in-memory aggregate, not the file.
* No upload is performed.
* \param[out] error  If non-null, receives any error that occurs during this call, otherwise, errors are ignored
* \return  A raw-data buffer holding the UTF-8 JSON report; release with rs2_delete_raw_data
*/
const rs2_raw_data_buffer* rs2_rum_get_report(rs2_error** error);

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
