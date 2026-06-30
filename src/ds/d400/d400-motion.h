// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2015 RealSense, Inc. All Rights Reserved.

#pragma once

#include "d400-device.h"
#include "ds/ds-motion-common.h"
#include <rsutils/lazy.h>

namespace librealsense
{
    class d400_motion_base : public virtual d400_device
    {
    public:
        rs2_motion_device_intrinsic get_motion_intrinsics(rs2_stream) const;
        bool is_imu_high_accuracy() const override;
        double get_gyro_default_scale() const override;

        std::shared_ptr<auto_exposure_mechanism> register_auto_exposure_options(synthetic_sensor* ep,
            const platform::extension_unit* fisheye_xu);

    private:
        friend class ds_motion_sensor;
        friend class ds_fisheye_sensor;
        friend class ds_motion_common;

        std::shared_ptr< rsutils::lazy< ds::imu_intrinsic > > _accel_intrinsic;
        std::shared_ptr< rsutils::lazy< ds::imu_intrinsic > > _gyro_intrinsic;
        std::shared_ptr< rsutils::lazy< rs2_extrinsics > > _depth_to_imu;  // Mechanical installation pose

    protected:
        d400_motion_base( std::shared_ptr< const d400_info > const & dev_info );

        std::shared_ptr<ds_motion_common> _ds_motion_common;

        std::shared_ptr<stream_interface> _accel_stream;
        std::shared_ptr<stream_interface> _gyro_stream;

        uint16_t _pid;    // product PID
        optional_value<uint8_t> _motion_module_device_idx;
        bool _has_motion_module_failed = false;
    };

    class d400_motion : public d400_motion_base
    {
    public:
        d400_motion( std::shared_ptr< const d400_info > const & dev_info );

        std::shared_ptr<synthetic_sensor> create_hid_device( std::shared_ptr<context> ctx,
                                                             const std::vector<platform::hid_device_info>& all_hid_infos );
        ds_motion_sensor & get_motion_sensor();
        std::shared_ptr<hid_sensor > get_raw_motion_sensor();


    protected:
        friend class ds_motion_common;
        friend class ds_fisheye_sensor;
        friend class ds_motion_sensor;

        void register_gyro_sensitivity();

    private:
        void register_fisheye_options();
        void register_fisheye_metadata();

        void register_stream_to_extrinsic_group(const stream_interface& stream, uint32_t group_index);

        void initialize_fisheye_sensor(std::shared_ptr<context> ctx, const platform::backend_device_group& group);

        optional_value<uint8_t> _fisheye_device_idx;
        optional_value<uint8_t> _motion_module_device_idx;

    };

    class d400_motion_uvc : public d400_motion_base
    {
    public:
        d400_motion_uvc( std::shared_ptr< const d400_info > const & );

        std::shared_ptr<synthetic_sensor> create_uvc_device(std::shared_ptr<context> ctx,
            const std::vector<platform::uvc_device_info>& all_uvc_infos,
            const firmware_version& camera_fw_version);
    };
}
