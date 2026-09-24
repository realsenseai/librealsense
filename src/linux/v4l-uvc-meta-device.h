// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#pragma once

#include "v4l-uvc-device.h"

namespace librealsense
{
    namespace platform
    {
        // Composition layer for uvc/metadata split nodes introduced with kernel 4.16
        class v4l_uvc_meta_device : public v4l_uvc_device
        {
        public:
            v4l_uvc_meta_device(const uvc_device_info& info, bool use_memory_map = false);

            virtual ~v4l_uvc_meta_device();

            bool is_platform_jetson() const override {return false;}

        protected:

            virtual void streamon() const override;
            virtual void streamoff() const override;
            virtual void negotiate_kernel_buffers(size_t num) const override;
            virtual void allocate_io_buffers(size_t num) override;
            virtual void map_device_descriptor() override;
            virtual void unmap_device_descriptor() override;
            virtual void set_format(stream_profile profile) override;
            virtual void prepare_capture_buffers() override;
            virtual void acquire_metadata(buffers_mgr & buf_mgr,fd_set &fds, bool compressed_format=false) override;
            void assign_md_device_capabilities();
            // checking if metadata is streamed
            virtual inline bool is_metadata_streamed() const override { return _md_fd > 0;}
            virtual inline std::shared_ptr<buffer> get_md_buffer(__u32 index) const override {return _md_buffers[index];}
            int _md_fd = -1;
            std::string _md_name = "";
            v4l2_buf_type _md_type = LOCAL_V4L2_BUF_TYPE_META_CAPTURE;

            std::vector<std::shared_ptr<buffer>> _md_buffers;

        private:
            bool _md_capabilities_assigned;
        };
    }  // namespace platform
}  // namespace librealsense
