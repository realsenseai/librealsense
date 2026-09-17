// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "v4l-enumerator.h"

#include "v4l-enumerator-mipi.h"
#include "v4l-enumerator-usb.h"

#include "v4l-ioctl.h"
#include "v4l-mipi-logic.h"
#include "v4l-usb-logic.h"
#include "types.h"
#include <src/librealsense-exception.h>
#include <src/platform/mipi-device-info.h>
#include <rsutils/string/from.h>
#include <rsutils/accelerators/gpu.h>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <map>
#include <regex>
#include <set>
#include <sstream>

#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>

#include <linux/videodev2.h>

namespace librealsense
{
    namespace platform
    {
        namespace v4l_enum
        {
        std::vector<path_and_identifier> collect_dev_video_path_and_identifier()
        {
            // building vector of /dev/videoX files with path, major, minor
            std::vector<path_and_identifier> dev_videos;
            DIR * dev_dir = opendir("/dev");
            if (!dev_dir)
            {
                LOG_ERROR("Cannot access /dev");
                throw linux_backend_exception(rsutils::string::from() << "Cannot access /dev");
            }

            // searching for match in /dev, in means of major, minor
            while (dirent * entry = readdir(dev_dir))
            {
                std::string name = entry->d_name;
                std::string dev_path = "/dev/" + name;

                struct stat st = {};
                if (stat(dev_path.c_str(), &st) < 0)
                {
                    continue;
                }
                dev_videos.push_back({dev_path, {major(st.st_rdev), minor(st.st_rdev)}});
            }
            closedir(dev_dir);

            return dev_videos;
        }

        bool get_identifier_from_v4l_video_path(const std::string& v4l_video_path, identifier& key)
        {
            // dev file is in paths:
            // - /sys/class/video4linux/videoX/dev for video files
            // - /sys/class/d4xx-class/d4xx-dfu-30-XXXa for mipi dfu files

            // dev file contains major_number:minor_number
            // while major_number is typically 81 for video4linux devices and 506 for d4xx-class devices

            std::ifstream dev_file(v4l_video_path + "/dev");
            if (!dev_file)
            {
                LOG_ERROR("Cannot access " + v4l_video_path + "/dev");
                return false;
            }

            std::string dev_line;
            std::getline(dev_file, dev_line);
            char sep = '\0';
            std::istringstream iss(dev_line);
            if (!(iss >> key.major >> sep >> key.minor) || sep != ':') {
                LOG_ERROR("Could not read major and minor from " + v4l_video_path + "/dev");
                return false;
            }
            return true;
        }

        std::vector<std::pair <std::string, std::string>> generate_v4l_to_dev_video_paths(const std::vector<path_and_identifier>& v4l_videos,
                                                                                                          const std::vector<path_and_identifier>& dev_videos)
        {
            std::vector<std::pair<std::string, std::string>> v4l_to_dev_video_paths;

            // going over video paths like /sys/class/video4linux/videoX
            // find their mapping in /dev/videoY
            // above videoX and videoY are often the same, but not always
            // for example when working with unprivileged containers, the name in /dev may not contain the string "video"
            // it depends on how the devices have been mapped (bind-mounted) into the container
            for(auto&& v4l_video : v4l_videos)
            {
                // searching for match in /dev, in means of major, minor
                for (auto&& dev_video : dev_videos)
                {
                    if (v4l_video.key== dev_video.key)
                    {
                        v4l_to_dev_video_paths.push_back(std::make_pair(v4l_video.path, dev_video.path));
                        break;
                    }
                }
            }
            return v4l_to_dev_video_paths;
        }

        bool get_devname_from_v4l_video_path(const std::string& v4l_video_path, std::string& dev_name,
                                                         const std::vector<std::pair <std::string, std::string>>& v4l_to_dev_video_paths)
        {
            for (auto&& v4l_to_dev_pair : v4l_to_dev_video_paths)
            {
                if (v4l_to_dev_pair.first == v4l_video_path)
                {
                    dev_name = v4l_to_dev_pair.second;
                    return true;
                }
            }
            return false;
        }

        std::vector<path_and_identifier> collect_v4l_video_path_and_identifier()
        {
            std::vector<path_and_identifier> v4l_videos;
            // Enumerate all subdevices present on the system
            DIR * dir = opendir("/sys/class/video4linux");
            if(!dir)
            {
                LOG_INFO("Cannot access /sys/class/video4linux");
                return v4l_videos;
            }
            while (dirent * entry = readdir(dir))
            {
                std::string name = entry->d_name;
                if(name == "." || name == "..") continue;

                // Resolve a pathname to ignore virtual video devices and  sub-devices
                static const std::regex video_dev_pattern("(\\/video\\d+)$");

                std::string path = "/sys/class/video4linux/" + name;
                std::string v4l_path{};
                char buff[PATH_MAX] = {0};
                if (realpath(path.c_str(), buff) != nullptr)
                {
                    v4l_path = std::string(buff);
                    if (v4l_path.find("virtual") != std::string::npos)
                        continue;
                    if (!std::regex_search(v4l_path, video_dev_pattern))
                    {
                        //LOG_INFO("Skipping Video4Linux entry " << v4l_path << " - not a device");
                        continue;
                    }
                    identifier key{0, 0};
                    if (!get_identifier_from_v4l_video_path(v4l_path, key))
                        continue;

                    v4l_videos.push_back({v4l_path, key});
                }
            }
            closedir(dir);

            // UVC nodes shall be traversed in ascending order for metadata nodes assignment ("dev/video1, Video2..
            // Replace lexicographic with numeric sort to ensure "video2" is listed before "video11"
            std::sort(v4l_videos.begin(), v4l_videos.end(),
                      [](const path_and_identifier& first, const path_and_identifier& second)
            {
                // getting videoXX
                std::string first_video = first.path.substr(first.path.find_last_of('/') + 1);
                std::string second_video = second.path.substr(second.path.find_last_of('/') + 1);

                // getting the index XX from videoXX
                std::stringstream first_index(first_video.substr(first_video.find_first_of("0123456789")));
                std::stringstream second_index(second_video.substr(second_video.find_first_of("0123456789")));
                int left_id = 0, right_id = 0;
                first_index >> left_id;
                second_index >> right_id;
                return left_id < right_id;
            });
            return v4l_videos;
        }

        std::vector<node_info> match_video_with_metadata_nodes(const std::vector<node_info>& uvc_nodes)
        {
            // Assume uvc_nodes is already sorted according to videoXX (video0, then video1...)
            // Assume for each metadata node with index N there is a origin streaming node with index (N-1)
            std::vector<node_info> uvc_devices;
            for (auto&& cur_node : uvc_nodes)
            {
                try
                {
                    if (!(cur_node.first.uvc_capabilities & V4L2_CAP_META_CAPTURE))
                        uvc_devices.emplace_back(cur_node);
                    else
                    {
                        if (uvc_devices.empty())
                        {
                            LOG_ERROR("UVC meta-node with no video streaming node encountered: " << std::string(cur_node.first));
                            continue;
                        }

                        // Update the preceding uvc item with metadata node info
                        auto uvc_node = uvc_devices.back();

                        if (uvc_node.first.uvc_capabilities & V4L2_CAP_META_CAPTURE)
                        {
                            LOG_ERROR("Consequtive UVC meta-nodes encountered: " << std::string(uvc_node.first) << " and " << std::string(cur_node.first) );
                            continue;
                        }

                        if (uvc_node.first.has_metadata_node)
                        {
                            LOG_ERROR( "Metadata node for uvc device: " << std::string(uvc_node.first) << " was previously assigned ");
                            continue;
                        }

                        // modify the last element with metadata node info
                        uvc_node.first.has_metadata_node = true;
                        uvc_node.first.metadata_node_id = cur_node.first.id;
                        uvc_devices.back() = uvc_node;
                    }
                }
                catch(const std::exception & e)
                {
                    LOG_ERROR("Failed to map meta-node "  << std::string(cur_node.first) <<", error encountered: " << e.what());
                }
            }
            return uvc_devices;
        }

        bool get_info_from_v4l_video_path( const std::string & v4l_video_path, const std::string & dev_name,
                                                           uvc_device_info & info, bool is_mipi_rs_enum_nodes_empty, camera_identifier_v4l_mipi& mipi_id)
        {
            bool res = false;

            // following line grabs video0 from "/sys/devices/.../video0" paths
            auto name = v4l_video_path.substr(v4l_video_path.find_last_of('/') + 1);

            if (v4l_usb_logic::is_usb_device_path(v4l_video_path))
            {
                info = get_info_from_usb_device_path(v4l_video_path, dev_name, name);
                res = true;
            }
            else if(is_mipi_rs_enum_nodes_empty) //video4linux devices that are not USB devices and not previously enumerated by rs links
            {
                // filter out all possible codecs, work only with compatible driver
                static const std::regex rs_mipi_compatible(".vi:|ipu6");
                info = get_info_from_mipi_device_path(v4l_video_path, name, mipi_id);
                if (regex_search(info.unique_id, rs_mipi_compatible)) {
                    res = true;
                }
            }
            else // continue as we already have mipi nodes enumerated by rs links in uvc_nodes
            {
                // empty
            }
            return res;
        }

        std::vector<node_info> collect_uvc_nodes(const std::vector<path_and_identifier>& v4l_videos,
                                                                 const std::vector<std::pair <std::string, std::string>>& v4l_to_dev_video_paths)
        {
            std::vector<node_info> uvc_nodes = get_mipi_rs_enum_nodes();
            bool is_mipi_rs_enum_nodes_empty = uvc_nodes.empty();

            // Carries a fallback MIPI camera's PID/VID from its depth node to its sibling nodes across the loop
            camera_identifier_v4l_mipi mipi_id;
            for(auto&& v4l_video : v4l_videos)
            {
                try
                {
                    std::string dev_name;
                    if (!get_devname_from_v4l_video_path(v4l_video.path, dev_name, v4l_to_dev_video_paths))
                    {
                        continue;
                    }

                    uvc_device_info info;
                    if (get_info_from_v4l_video_path(v4l_video.path, dev_name, info, is_mipi_rs_enum_nodes_empty, mipi_id))
                    {
                        uvc_nodes.emplace_back(info, dev_name);
                    }
                }
                catch(const std::exception & e)
                {
                    LOG_INFO("Not a USB video device: " << e.what());
                }
            }

            return uvc_nodes;
        }

        // uvcvideo creates one /dev/videoN per UVC output terminal, numbered in VideoControl descriptor order, so
        // /dev/videoN order can disagree with VideoStreaming interface order (D585 2C reverses its two color
        // terminals). Sort by interface - the order Windows enumerates pins in - so a pin index means one endpoint.
        void foreach_uvc_device( std::function<void(const uvc_device_info&, const std::string&)> action )
        {
            // building vector of /sys/class/video4linux/.../videoX files with path, major, minor
            std::vector<path_and_identifier> v4l_videos = collect_v4l_video_path_and_identifier();
            // building vector of /dev/videoX files with path, major, minor
            std::vector<path_and_identifier> dev_videos = collect_dev_video_path_and_identifier();
            // generate map of "/sys/class/video4linux/.../videoX" to "/dev/videoY"
            auto v4l_to_dev_video_paths = generate_v4l_to_dev_video_paths(v4l_videos, dev_videos);

            // Collect UVC nodes info to bundle metadata and video
            std::vector<node_info> uvc_nodes = collect_uvc_nodes(v4l_videos, v4l_to_dev_video_paths);

            // Matching video and metadata nodes
            std::vector<node_info> uvc_devices = match_video_with_metadata_nodes(uvc_nodes);

            sort_nodes_by_streaming_interface(uvc_devices);

            try
            {
                // Dispatch registration for enumerated uvc devices
                for (auto&& dev : uvc_devices)
                    action(dev.first, dev.second);
            }
            catch(const std::exception & e)
            {
                LOG_ERROR("Registration of UVC device failed: " << e.what());
            }
        }
        }  // namespace v4l_enum
    }  // namespace platform
}  // namespace librealsense
