// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2023 RealSense, Inc. All Rights Reserved.

#pragma once

#include <src/usb/usb-types.h>

#include <sstream>


namespace librealsense {
namespace platform {


struct uvc_device_info
{
    std::string id;  // to distinguish between different pins of the same device
    uint16_t vid = 0;
    uint16_t pid = 0;
    uint16_t mi = 0;
    std::string unique_id;
    std::string device_path;
    std::string subdev_name;
    std::string subdevice_path;
    std::string dfu_device_path; // for mipi multiple cameras
    std::string serial;
    usb_spec conn_spec = usb_undefined;
    uint32_t uvc_capabilities = 0;
    bool has_metadata_node = false;
    std::string metadata_node_id;

    operator std::string() const
    {
        std::ostringstream s;
        s << "id- " << id << "\nvid- " << std::hex << vid << "\npid- " << std::hex << pid << "\nmi- " << std::dec << mi
          << "\nunique_id- " << unique_id << "\npath- " << device_path << "\nUVC capabilities- " << std::hex
          << uvc_capabilities << "\nUVC specification- " << std::hex << (uint16_t)conn_spec << std::dec
          << ( has_metadata_node ? ( "\nmetadata node-" + metadata_node_id ) : "" ) << std::endl;

        return s.str();
    }

    bool operator<( const uvc_device_info & obj ) const
    {
        return ( std::make_tuple( id, vid, pid, mi, unique_id, device_path )
                 < std::make_tuple( obj.id, obj.vid, obj.pid, obj.mi, obj.unique_id, obj.device_path ) );
    }
};

inline bool operator==( const uvc_device_info & a, const uvc_device_info & b )
{
    return ( a.vid == b.vid ) && ( a.pid == b.pid ) && ( a.mi == b.mi ) && ( a.unique_id == b.unique_id )
        && ( a.id == b.id ) && ( a.device_path == b.device_path ) && ( a.conn_spec == b.conn_spec );
}


}  // namespace platform
}  // namespace librealsense
