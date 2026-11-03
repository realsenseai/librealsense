/* License: Apache 2.0. Copyright (C) 2026 RealSense, Inc. */
#ifndef LIBREALSENSE_RS_INTENT_H
#define LIBREALSENSE_RS_INTENT_H
#include "rs_types.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Device-wide streams Intent as UTF-8 JSON (no terminating NUL in the buffer).
 * Release GET's result with rs2_delete_raw_data. Unsupported firmware reports an
 * error. SET never silently falls back to legacy. Clear with {"op":"CLEAR"}.
 * Intent is volatile and remains configured after streams stop. */
const rs2_raw_data_buffer *rs2_get_intent(const rs2_device *device, rs2_error **error);
void rs2_set_intent(const rs2_device *device, const char *json, rs2_error **error);
#ifdef __cplusplus
}
#endif
#endif
