// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2015 RealSense, Inc. All Rights Reserved.

#pragma once

#include "d400-device.h"
#include "ds/ds-color-common.h"
#include <src/color-sensor.h>

#include "stream.h"
#include <src/metadata-parser.h>

#include <rsutils/lazy.h>
#include <map>

namespace librealsense
{
    class d400_color : public virtual d400_device
    {
    public:
        d400_color( std::shared_ptr< const d400_info > const & );

        synthetic_sensor& get_color_sensor()
        {
            return dynamic_cast<synthetic_sensor&>(get_sensor(_color_device_idx));
        }

        std::shared_ptr< uvc_sensor > get_raw_color_sensor()
        {
            synthetic_sensor & color_sensor = get_color_sensor();
            return std::dynamic_pointer_cast< uvc_sensor >( color_sensor.get_raw_sensor() );
        }

    protected:
        void register_color_features();

        rs2_format get_color_format() const;

        std::shared_ptr<stream_interface> _color_stream;
        std::shared_ptr<ds_color_common> _ds_color_common;
        uint8_t _color_device_idx = -1;

    private:
        void register_options();
        void register_processing_blocks();

        void register_metadata(const synthetic_sensor& color_ep) const;
        void register_metadata_mipi(const synthetic_sensor& color_ep) const;

        void register_stream_to_extrinsic_group(const stream_interface& stream, uint32_t group_index);

        void create_color_device(std::shared_ptr<context> ctx,
            const platform::backend_device_group& group);
        void init();

        template<class Attribute, typename Flag>
        std::shared_ptr<md_attribute_parser_base> create_color_md_mipi_parser(
            Attribute md_mipi_rgb_mode::* attribute,
            Flag flag,
            attrib_modifyer mod = nullptr) const
        {
            auto md_prop_offset = offsetof(metadata_mipi_rgb_raw, rgb_mode);
            return make_mipi_color_attribute_parser(attribute, flag, md_prop_offset, _fw_version, mod);
        }

        friend class d400_color_sensor;
        friend class rs435i_device;
        friend class ds_color_common;

        bool _separate_color;
        rsutils::lazy< std::vector< uint8_t > > _color_calib_table_raw;
        std::shared_ptr< rsutils::lazy< rs2_extrinsics > > _color_extrinsic;
    };

    class d400_color_sensor : public synthetic_sensor,
                             public video_sensor_interface,
                             public roi_sensor_base,
                             public color_sensor
    {
    public:
        explicit d400_color_sensor(d400_color* owner,
            std::shared_ptr<uvc_sensor> uvc_sensor,
            std::map<uint32_t, rs2_format> d400_color_fourcc_to_rs2_format,
            std::map<uint32_t, rs2_stream> d400_color_fourcc_to_rs2_stream)
            : synthetic_sensor("RGB Camera", uvc_sensor, owner, d400_color_fourcc_to_rs2_format, d400_color_fourcc_to_rs2_stream),
            _owner(owner)
        {}

        rs2_intrinsics get_intrinsics(const stream_profile& profile) const override;
        stream_profiles init_stream_profiles() override;
        processing_blocks get_recommended_processing_blocks() const override;

    protected:
        const d400_color* _owner;
    };

}
