// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.
#pragma once

#include <rsutils/json-fwd.h>

#include <string>
#include <mutex>


namespace librealsense {
namespace rum {


// RUM cloud-upload consent, persisted in the shared realsense-config.json (the same file the
// viewer's config_file reads/writes). Stored string-encoded so SDK writes and viewer writes
// interoperate. (The anonymous source_id lives with the report store, not here.)
class rum_config
{
public:
    // Process-wide instance backed by realsense-config.json under the app-data folder.
    static rum_config & instance();

    // Explicit file path — used by tests to avoid touching the real user config.
    explicit rum_config( std::string filename );

    // Resolved consent: RS2_RUM_CLOUD_ENABLED env var wins, then the config key,
    // otherwise false (missing decision means no upload).
    bool is_cloud_enabled() const;

    void set_cloud_enabled( bool enabled );

private:
    // Load/save the backing config file (owns _filename). Callers hold _mutex.
    rsutils::json load_config() const;
    bool save_config( rsutils::json const & j );

    std::string _filename;
    mutable std::mutex _mutex;
};


}  // namespace rum
}  // namespace librealsense
