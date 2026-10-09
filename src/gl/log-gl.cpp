// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "log.h"

#ifdef BUILD_EASYLOGGINGPP
// The storage must be defined before the logger below is constructed (same translation unit)
#ifdef SHARED_LIBS
INITIALIZE_EASYLOGGINGPP
#endif
char log_gl_name[] = LIBREALSENSE_ELPP_ID;
static librealsense::logger_type<log_gl_name> logger_gl;
#endif
