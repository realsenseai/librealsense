//// License: Apache 2.0. See LICENSE file in root directory.
//// Copyright(c) 2022 RealSense, Inc. All Rights Reserved.

#include "context.h"
#include "ds-private.h"

using namespace std;


namespace librealsense
{
    namespace ds
    {
        ds_rect_resolutions width_height_to_ds_rect_resolutions(uint32_t width, uint32_t height)
        {
            for (auto& elem : resolutions_list)
            {
                if (uint32_t(elem.second.x) == width && uint32_t(elem.second.y) == height)
                    return elem.first;
            }
            return max_ds_rect_resolutions;
        }

        rs2_intrinsics get_intrinsic_fisheye_table(const std::vector<uint8_t>& raw_data, uint32_t width, uint32_t height)
        {
            auto table = check_calib<ds::fisheye_calibration_table>(raw_data);

            rs2_intrinsics intrinsics;
            auto intrin = table->intrinsic;
            intrinsics.fx = intrin(0, 0);
            intrinsics.fy = intrin(1, 1);
            intrinsics.ppx = intrin(2, 0);
            intrinsics.ppy = intrin(2, 1);
            intrinsics.model = RS2_DISTORTION_FTHETA;

            intrinsics.height = height;
            intrinsics.width = width;

            std::memcpy( intrinsics.coeffs, table->distortion, sizeof( table->distortion ) );

            LOG_DEBUG(endl << array2str((float_4&)(intrinsics.fx, intrinsics.fy, intrinsics.ppx, intrinsics.ppy)) << endl);

            return intrinsics;
        }

        pose get_fisheye_extrinsics_data(const vector<uint8_t>& raw_data)
        {
            auto table = check_calib<fisheye_extrinsics_table>(raw_data);

            auto rot = table->rotation;
            auto trans = table->translation;

            pose ex = { {rot(0,0), rot(1,0),rot(2,0),rot(1,0), rot(1,1),rot(2,1),rot(0,2), rot(1,2),rot(2,2)},
                       {trans[0], trans[1], trans[2]} };
            return ex;
        }

        flash_structure get_rw_flash_structure(const uint32_t flash_version)
        {
            switch (flash_version)
            {
                // { number of payloads in section { ro table types }} see Flash.xml
            case 100: return { 2, { 17, 10, 40, 29, 30, 54} };
            case 101: return { 3, { 10, 16, 40, 29, 18, 19, 30, 20, 21, 54 } };
            case 102: return { 3, { 9, 10, 16, 40, 29, 18, 19, 30, 20, 21, 54 } };
            case 103: return { 4, { 9, 10, 16, 40, 29, 18, 19, 30, 20, 21, 54 } };
            case 104: return { 4, { 9, 10, 40, 29, 18, 19, 30, 20, 21, 54 } };
            case 105: return { 5, { 9, 10, 40, 29, 18, 19, 30, 20, 21, 54 } };
            case 106: return { 5, { 15, 9, 10, 16, 40, 29, 18, 19, 30, 20, 21, 54 } };
            case 107: return { 6, { 15, 9, 10, 16, 40, 29, 18, 19, 30, 20, 21, 54 } };
            default:
                throw std::runtime_error("Unsupported flash version: " + std::to_string(flash_version));
            }
        }

        flash_structure get_ro_flash_structure(const uint32_t flash_version)
        {
            switch (flash_version)
            {
                // { number of payloads in section { ro table types }} see Flash.xml
            case 100: return { 2, { 134, 25 } };
            default:
                throw std::runtime_error("Unsupported flash version: " + std::to_string(flash_version));
            }
        }

        flash_info get_flash_info(const std::vector<uint8_t>& flash_buffer)
        {
            flash_info rv = {};

            uint32_t header_offset = FLASH_INFO_HEADER_OFFSET;
            memcpy(&rv.header, flash_buffer.data() + header_offset, sizeof(rv.header));

            uint32_t ro_toc_offset = FLASH_RO_TABLE_OF_CONTENT_OFFSET;
            uint32_t rw_toc_offset = FLASH_RW_TABLE_OF_CONTENT_OFFSET;

            auto ro_toc = parse_table_of_contents(flash_buffer, ro_toc_offset);
            auto rw_toc = parse_table_of_contents(flash_buffer, rw_toc_offset);

            auto ro_structure = get_ro_flash_structure(ro_toc.header.version);
            auto rw_structure = get_rw_flash_structure(rw_toc.header.version);

            rv.read_only_section = parse_flash_section(flash_buffer, ro_toc, ro_structure);
            rv.read_only_section.offset = rv.header.read_only_start_address;
            rv.read_write_section = parse_flash_section(flash_buffer, rw_toc, rw_structure);
            rv.read_write_section.offset = rv.header.read_write_start_address;

            return rv;
        }

        std::string table_header::to_string() const
        {
            std::string res;
            res += "version:\t" + std::to_string(version) + "\n";
            res += "table_type:\t" + std::to_string(table_type) + "\n";
            res += "table_size:\t" + std::to_string(table_size) + "\n";
            res += "param:\t" + std::to_string(param) + "\n";
            res += "crc32:\t" + std::to_string(crc32) + "\n";
            return res;
        }

        bool is_partial_device_allowed( const std::shared_ptr< context > & ctx )
        {
            auto settings = ctx->get_settings();
            return settings.nested( "partial-device-allowed" ).default_value< bool >( false );
        }
    } // librealsense::ds
} // namespace librealsense