// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#ifndef LIBREALSENSE_RS2_RUM_HPP
#define LIBREALSENSE_RS2_RUM_HPP

#include "rs_types.hpp"
#include "../h/rs_rum.h"

#include <string>
#include <memory>

namespace rs2
{
    namespace rum
    {
        // The live RUM report for the current session as a JSON string (the in-memory aggregate,
        // not the on-disk store).
        inline std::string get_report()
        {
            rs2_error* e = nullptr;
            std::shared_ptr<const rs2_raw_data_buffer> buffer(
                rs2_rum_get_report(&e), rs2_delete_raw_data);
            error::handle(e);
            if (!buffer)
                return std::string();
            auto size = rs2_get_raw_data_size(buffer.get(), &e);
            error::handle(e);
            auto data = rs2_get_raw_data(buffer.get(), &e);
            error::handle(e);
            if (!data)
                return std::string();
            return std::string(reinterpret_cast<const char*>(data), size);
        }

        // Set the cloud-upload consent flag; persists to the per-user config file.
        inline void set_cloud_enabled(bool enabled)
        {
            rs2_error* e = nullptr;
            rs2_rum_set_cloud_enabled(enabled ? 1 : 0, &e);
            error::handle(e);
        }

        // Resolved cloud-upload consent (env var overrides the config file).
        inline bool is_cloud_enabled()
        {
            rs2_error* e = nullptr;
            auto enabled = rs2_rum_is_cloud_enabled(&e);
            error::handle(e);
            return enabled != 0;
        }
    }
}

#endif // LIBREALSENSE_RS2_RUM_HPP
