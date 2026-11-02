// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "v4l-enumerator-usb.h"

#include "camera-identifier-v4l.h"  // camera_identifier_v4l_usb
#include "v4l-ioctl.h"              // get_dev_capabilities()
#include "v4l-usb-logic.h"
#include "types.h"
#include <src/librealsense-exception.h>
#include <rsutils/string/from.h>
#include <rsutils/accelerators/gpu.h>

#include <algorithm>
#include <map>
#include <sstream>

namespace librealsense
{
    namespace platform
    {
        namespace v4l_enum
        {
        uvc_device_info get_info_from_usb_device_path( const std::string & video_path,
                                                                       const std::string & dev_name,
                                                                       const std::string & name )
        {
            std::string busnum, devnum, devpath;

            if (!v4l_usb_logic::is_usb_path_valid(video_path, dev_name, busnum, devnum, devpath))
            {
#ifndef RS2_USE_CUDA
                if (rsutils::rs2_is_cuda_available())
                {
                    /* On the Jetson TX, the camera module is CSI & I2C and does not report as this code expects
                    Patch suggested by JetsonHacks: https://github.com/jetsonhacks/buildLibrealsense2TX */
                    LOG_INFO("Failed to read busnum/devnum. Device Path: " << ("/sys/class/video4linux/" + name));
                }
#endif
               throw linux_backend_exception("Failed to read busnum/devnum of usb device");
            }

            LOG_INFO("Enumerating UVC " << name << " v4l_path=" << video_path << " dev_name=" << dev_name);

            camera_identifier_v4l_usb usb_id;
            usb_id.resolve(name);

            uvc_device_info info{};
            info.pid = usb_id.get_pid();
            info.vid = usb_id.get_vid();
            info.mi = v4l_usb_logic::read_interface_number(name);
            info.id = dev_name;
            info.device_path = video_path;
            info.unique_id = busnum + "-" + devpath + "-" + devnum;
            // Find the USB specification (USB2/3) type from the underlying device, traversing from
            // /sys/devices/.../M-N/3-6:1.0/video4linux/video0 up to /sys/devices/.../M-N/version
            info.usb_conn_spec = v4l_usb_logic::get_usb_connection_type(video_path + "/../../../");
            info.uvc_capabilities = get_dev_capabilities(dev_name).device_caps;

            return info;
        }

        void sort_nodes_by_streaming_interface( std::vector<node_info>& nodes )
        {
            std::map<std::pair<std::string, uint16_t>, std::vector<size_t>> functions;
            for (size_t i = 0; i < nodes.size(); ++i)
                if (!nodes[i].first.is_mipi)  // a MIPI node has no USB descriptor to order by
                    functions[{ nodes[i].first.unique_id, nodes[i].first.mi }].push_back(i);

            for (auto&& function : functions)
            {
                auto& indices = function.second;
                if (indices.size() < 2)
                    continue;

                auto interfaces = v4l_usb_logic::read_streaming_interfaces_in_terminal_order(
                    nodes[indices.front()].first.device_path, function.first.second);
                if (interfaces.size() != indices.size())
                    continue;  // descriptor unreadable, or terminals with no node of their own - keep /dev/videoN order
                if (std::is_sorted(interfaces.begin(), interfaces.end()))
                    continue;  // terminals listed in interface order, as nearly every firmware does

                std::vector<std::pair<uint8_t, node_info>> group;
                for (size_t i = 0; i < indices.size(); ++i)
                    group.emplace_back(interfaces[i], nodes[indices[i]]);
                std::stable_sort(group.begin(), group.end(),
                                 [](const std::pair<uint8_t, node_info>& a, const std::pair<uint8_t, node_info>& b)
                                 { return a.first < b.first; });

                std::ostringstream reordered;
                for (size_t i = 0; i < indices.size(); ++i)
                {
                    nodes[indices[i]] = group[i].second;
                    reordered << " " << nodes[indices[i]].second;
                }
                LOG_DEBUG("Nodes of mi " << function.first.second << " reordered by streaming interface:" << reordered.str());
            }
        }
        }  // namespace v4l_enum
    }  // namespace platform
}  // namespace librealsense
