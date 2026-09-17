// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

#include "v4l-mipi-device.h"

#include "v4l-mipi-logic.h"
#include "v4l-product-quirks.h"
#include <src/librealsense-exception.h>
#include <rsutils/string/from.h>

#include <cstring>

#include <errno.h>
#include <sys/ioctl.h>

namespace librealsense
{
    namespace platform
    {
        v4l_mipi_device::v4l_mipi_device(const uvc_device_info& info, bool use_memory_map):
            v4l_uvc_meta_device(info,use_memory_map)
        {}

        v4l_mipi_device::~v4l_mipi_device()
        {}

        bool v4l_mipi_device::get_pu(rs2_option opt, int32_t& value) const
        {
            v4l2_ext_control control{v4l_mipi_logic::option_to_cid(opt), 0, 0, 0};
            // Extract the control group from the underlying control query
            v4l2_ext_controls ctrls_block { control.id&0xffff0000, 1, 0, 0, 0, &control};

            if (xioctl(_fd, VIDIOC_G_EXT_CTRLS, &ctrls_block) < 0)
            {
                if (errno == EIO || errno == EAGAIN) // TODO: Log?
                    return false;

                throw linux_backend_exception(rsutils::string::from()
                                              << "xioctl(VIDIOC_G_EXT_CTRLS) failed on option " << rs2_option_to_string(opt)
                                              << ", errno=" << errno );
            }

            if (opt == RS2_OPTION_ENABLE_AUTO_EXPOSURE)
                control.value = (V4L2_EXPOSURE_MANUAL==control.value) ? 0 : 1;
            value = control.value;

            return true;
        }

        bool v4l_mipi_device::set_pu(rs2_option opt, int32_t value)
        {
            v4l2_ext_control control{v4l_mipi_logic::option_to_cid(opt), 0, 0, value};
            if (opt == RS2_OPTION_ENABLE_AUTO_EXPOSURE)
                control.value = value ? V4L2_EXPOSURE_APERTURE_PRIORITY : V4L2_EXPOSURE_MANUAL;

            // Extract the control group from the underlying control query
            v4l2_ext_controls ctrls_block{ control.id & 0xffff0000, 1, 0, 0, 0, &control };
            if (xioctl(_fd, VIDIOC_S_EXT_CTRLS, &ctrls_block) < 0)
            {
                if (errno == EIO || errno == EAGAIN) // TODO: Log?
                    return false;

                throw linux_backend_exception(rsutils::string::from()
                                              << "xioctl(VIDIOC_S_EXT_CTRLS) failed on option " << rs2_option_to_string(opt)
                                              << ", value=" << value << ", errno=" << errno );
            }

            return true;
        }

        bool v4l_mipi_device::set_xu(const extension_unit& xu, uint8_t control, const uint8_t* data, int size)
        {
            v4l2_ext_control xctrl{v4l_mipi_logic::xu_to_cid(xu,control,is_d5xx_product_line(_info.pid)), uint32_t(size), 0, 0};
            switch (size)
            {
                case 1: xctrl.value   = *(reinterpret_cast<const uint8_t*>(data)); break;
                case 2: xctrl.value   = *reinterpret_cast<const uint16_t*>(data); break; // TODO check signed/unsigned
                case 4: xctrl.value   = *reinterpret_cast<const int32_t*>(data); break;
                case 8: xctrl.value64 = *reinterpret_cast<const int64_t*>(data); break;
                default:
                    xctrl.p_u8 = const_cast<uint8_t*>(data); // TODO aggregate initialization with union
            }

            if (v4l_mipi_logic::is_auto_exposure_control(control))
                xctrl.value = xctrl.value ? V4L2_EXPOSURE_APERTURE_PRIORITY : V4L2_EXPOSURE_MANUAL;

            // Extract the control group from the underlying control query
            v4l2_ext_controls ctrls_block { xctrl.id&0xffff0000, 1, 0, 0, 0, &xctrl };

            int retVal = xioctl(_fd, VIDIOC_S_EXT_CTRLS, &ctrls_block);
            if (retVal < 0)
            {
                if (errno == EIO || errno == EAGAIN) // TODO: Log?
                    return false;

                throw linux_backend_exception(rsutils::string::from()
                                              << "xioctl(VIDIOC_S_EXT_CTRLS) failed on control "
                                              << static_cast< int >( control ) << ", errno=" << errno );
            }
            return true;
        }

        bool v4l_mipi_device::get_xu(const extension_unit& xu, uint8_t control, uint8_t* data, int size) const
        {
            v4l2_ext_control xctrl{v4l_mipi_logic::xu_to_cid(xu,control,is_d5xx_product_line(_info.pid)), uint32_t(size), 0, 0};
            xctrl.p_u8 = data;

            v4l2_ext_controls ext {xctrl.id & 0xffff0000, 1, 0, 0, 0, &xctrl};

            // the ioctl fails once when performing send and receive right after it
            // it succeeds on the second time
            int tries = 2;
            while(tries--)
            {
                int ret = xioctl(_fd, VIDIOC_G_EXT_CTRLS, &ext);
                if (ret < 0)
                {
                    // exception is thrown if the ioctl fails twice
                    continue;
                }

                if (v4l_mipi_logic::is_auto_exposure_control(control))
                  xctrl.value = (V4L2_EXPOSURE_MANUAL == xctrl.value) ? 0 : 1;

                // used to parse the data when only a value is returned (e.g. laser power),
                // and not a pointer to a buffer of data (e.g. gvd)
                if (size < sizeof(__s64))
                    memcpy(data,(void*)(&xctrl.value), size);

                return true;
            }

            // sending error on ioctl failure
            if (errno == EIO || errno == EAGAIN) // TODO: Log?
                return false;
            throw linux_backend_exception(rsutils::string::from() << "xioctl(VIDIOC_G_EXT_CTRLS) failed on control " << static_cast<int>(control) << ", errno=" << errno);
        }

        control_range v4l_mipi_device::get_xu_range(const extension_unit& xu, uint8_t control, int len) const
        {
            v4l2_query_ext_ctrl xctrl_query{};
            xctrl_query.id = v4l_mipi_logic::xu_to_cid(xu,control,is_d5xx_product_line(_info.pid));

            if(0 > ioctl(_fd,VIDIOC_QUERY_EXT_CTRL,&xctrl_query)){
                throw linux_backend_exception(rsutils::string::from() << "xioctl(VIDIOC_QUERY_EXT_CTRL) failed, errno=" << errno);
            }

            if ((xctrl_query.elems !=1 ) ||
                (xctrl_query.minimum < std::numeric_limits<int32_t>::min()) ||
                (xctrl_query.maximum > std::numeric_limits<int32_t>::max()))
                throw linux_backend_exception(rsutils::string::from() << "Mipi Control range for " << xctrl_query.name
                    << " is not compliant with backend interface: [min,max,default,step]:\n"
                    << xctrl_query.minimum << ", " << xctrl_query.maximum << ", "
                    << xctrl_query.default_value << ", " << xctrl_query.step
                    << "\n Elements = " << xctrl_query.elems);

            if (v4l_mipi_logic::is_auto_exposure_control(control))
                return {0, 1, 1, 1};
            return { static_cast<int32_t>(xctrl_query.minimum), static_cast<int32_t>(xctrl_query.maximum),
                     static_cast<int32_t>(xctrl_query.step), static_cast<int32_t>(xctrl_query.default_value)};
        }

        control_range v4l_mipi_device::get_pu_range(rs2_option option) const
        {
            // Auto controls range is trimed to {0,1} range
            if(option >= RS2_OPTION_ENABLE_AUTO_EXPOSURE && option <= RS2_OPTION_ENABLE_AUTO_WHITE_BALANCE)
            {
                static const int32_t min = 0, max = 1, step = 1, def = 1;
                control_range range(min, max, step, def);

                return range;
            }

            struct v4l2_query_ext_ctrl query = {};
            query.id = v4l_mipi_logic::option_to_cid(option);
            if (xioctl(_fd, VIDIOC_QUERY_EXT_CTRL, &query) < 0)
            {
                // Some controls (exposure, auto exposure, auto hue) do not seem to work on V4L2
                // Instead of throwing an error, return an empty range. This will cause this control to be omitted on our UI sample.
                // TODO: Figure out what can be done about these options and make this work
                query.minimum = query.maximum = 0;
            }

            control_range range(query.minimum, query.maximum, query.step, query.default_value);

            return range;
        }

        void v4l_mipi_device::set_metadata_attributes(buffers_mgr& buf_mgr, __u32 bytesused, uint8_t* md_start)
        {
            buf_mgr.set_md_attributes(bytesused, md_start);
        }

        bool v4l_mipi_device::is_platform_jetson() const
        {
            v4l2_capability cap = get_dev_capabilities(_name);

            std::string driver_str = reinterpret_cast<char*>(cap.driver);
            // checking if "tegra" is part of the driver string
            size_t pos = driver_str.find("tegra");
            return pos != std::string::npos;
        }
    }  // namespace platform
}  // namespace librealsense
