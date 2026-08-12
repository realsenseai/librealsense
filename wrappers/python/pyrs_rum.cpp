/* License: Apache 2.0. See LICENSE file in root directory.
Copyright(c) 2026 RealSense, Inc. All Rights Reserved. */

#include "pyrealsense2.h"
#include <librealsense2/rs.hpp>

void init_rum(py::module &m)
{
    auto rum = m.def_submodule( "rum", "Real User Monitoring (RUM) usage statistics" );
    rum.def( "get_report_path", &rs2::rum::get_report_path,
             "Path of the on-disk RUM report file (read it to inspect or upload the report)." );
    rum.def( "set_cloud_enabled", &rs2::rum::set_cloud_enabled, "enabled"_a,
             "Set the cloud-upload consent flag (persists to the per-user config file)." );
    rum.def( "is_cloud_enabled", &rs2::rum::is_cloud_enabled,
             "Resolved cloud-upload consent (RS2_RUM_CLOUD_ENABLED env var overrides the config file)." );
}
