// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

#include "camera-identifier-v4l.h"
#include "v4l-node-info.h"

#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace librealsense
{
    namespace platform
    {
        // Transport-agnostic V4L2 enumeration: scan /sys and /dev, pair video with metadata nodes, and
        // hand each node to the USB or MIPI resolver. The USB and MIPI halves live in
        // v4l-enumerator-usb.h and v4l-enumerator-mipi.h; only get_info_from_v4l_video_path knows both.
        namespace v4l_enum
        {
            // Walk every enumerable UVC node - USB and MIPI alike - and hand each resolved device to 'action'.
            void foreach_uvc_device( std::function<void(const uvc_device_info&, const std::string&)> action );

            std::vector<path_and_identifier> collect_v4l_video_path_and_identifier();
            std::vector<path_and_identifier> collect_dev_video_path_and_identifier();
            bool get_identifier_from_v4l_video_path(const std::string& v4l_video_path, identifier& key);
            bool get_devname_from_v4l_video_path(const std::string& v4l_video_path, std::string& devname,
                                                    const std::vector<std::pair <std::string, std::string>>& v4l_to_dev_video_paths);

            std::vector<std::pair <std::string, std::string>> generate_v4l_to_dev_video_paths(const std::vector<path_and_identifier>& v4l_videos,
                                                                                                     const std::vector<path_and_identifier>& dev_videos);
            std::vector<node_info> collect_uvc_nodes(const std::vector<path_and_identifier>& v4l_videos,
                                                            const std::vector<std::pair <std::string, std::string>>& v4l_to_dev_video_paths);
            std::vector<node_info> match_video_with_metadata_nodes(const std::vector<node_info>& uvc_nodes);

            // The one USB/MIPI fork in enumeration: dispatches on the shape of the sysfs path.
            bool get_info_from_v4l_video_path(const std::string& v4l_video_path, const std::string& dev_name, uvc_device_info& info, bool is_mipi_rs_enum_nodes_empty,
                                                 camera_identifier_v4l_mipi& mipi_id);
        }  // namespace v4l_enum
    }  // namespace platform
}  // namespace librealsense
