// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

#include "v4l-uvc-meta-device.h"

namespace librealsense
{
    namespace platform
    {
        // D457 Development. To be merged into underlying class
        class v4l_mipi_device : public v4l_uvc_meta_device
        {
        public:
            v4l_mipi_device(const uvc_device_info& info, bool use_memory_map = true);

            virtual ~v4l_mipi_device();

            bool get_pu(rs2_option opt, int32_t& value) const override;
            bool set_pu(rs2_option opt, int32_t value) override;
            bool set_xu(const extension_unit& xu, uint8_t control, const uint8_t* data, int size) override;
            bool get_xu(const extension_unit& xu, uint8_t control, uint8_t* data, int size) const override;
            control_range get_xu_range(const extension_unit& xu, uint8_t control, int len) const override;
            control_range get_pu_range(rs2_option option) const override;
            void set_metadata_attributes(buffers_mgr& buf_mgr, __u32 bytesused, uint8_t* md_start) override;
            bool is_platform_jetson() const override;
        };
    }  // namespace platform
}  // namespace librealsense
