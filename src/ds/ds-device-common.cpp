// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2022 RealSense, Inc. All Rights Reserved.

#include "ds-device-common.h"

#include "fw-update/fw-update-device-interface.h"

#include "d400/d400-device.h"
#include "d500/d500-device.h"

#include "proc/hdr-merge.h"
#include "proc/sequence-id-filter.h"
#include "proc/decimation-filter.h"
#include "proc/threshold.h"
#include "proc/hole-filling-filter.h"
#include "proc/disparity-transform.h"
#include "proc/spatial-filter.h"
#include "proc/colorizer.h"
#include "proc/temporal-filter.h"

#include <src/backend.h>
#include <librealsense2/h/rs_internal.h>

namespace librealsense
{
    using namespace ds;

    ds_auto_exposure_roi_method::ds_auto_exposure_roi_method(
        const hw_monitor& hwm,
        ds::fw_cmd cmd)
        : _hw_monitor(hwm), _cmd(cmd) {}

    void ds_auto_exposure_roi_method::set(const region_of_interest& roi)
    {
        command cmd(_cmd);
        cmd.param1 = roi.min_y;
        cmd.param2 = roi.max_y;
        cmd.param3 = roi.min_x;
        cmd.param4 = roi.max_x;
        _hw_monitor.send(cmd);
    }

    region_of_interest ds_auto_exposure_roi_method::get() const
    {
        region_of_interest roi;
        command cmd(_cmd + 1);
        auto res = _hw_monitor.send(cmd);

        if (res.size() < 4 * sizeof(uint16_t))
        {
            throw std::runtime_error("Invalid result size!");
        }

        auto words = reinterpret_cast<uint16_t*>(res.data());

        roi.min_y = words[0];
        roi.max_y = words[1];
        roi.min_x = words[2];
        roi.max_x = words[3];

        return roi;
    }

    void ds_device_common::enter_update_state(const command& cmd) const
    {
        // Stop all data streaming/exchange pipes with HW
        _owner->stop_activity();

        using namespace std;
        using namespace std::chrono;

        try
        {
            LOG_INFO("entering to update state, device disconnect is expected");
            _hw_monitor->send(cmd);

            // We allow 6 seconds because on Linux the removal status is updated at a 5 seconds rate.
            const int MAX_ITERATIONS_FOR_DEVICE_DISCONNECTED_LOOP = DISCONNECT_PERIOD_MS / DELAY_FOR_RETRIES;
            for( auto i = 0; i < MAX_ITERATIONS_FOR_DEVICE_DISCONNECTED_LOOP; i++ )
            {
                // If the device was detected as removed we assume the device is entering update
                // mode Note: if no device status callback is registered we will wait the whole time
                // and it is OK
                if( ! _owner->is_valid() )
                {
                    this_thread::sleep_for( milliseconds( DELAY_FOR_CONNECTION ) );
                    return;
                }

                this_thread::sleep_for( milliseconds( DELAY_FOR_RETRIES ) );
            }

            if (_owner->device_changed_notifications_on())
                LOG_WARNING("Timeout waiting for device disconnect after DFU command!");
        }
        catch (std::exception& e)
        {
            LOG_WARNING(e.what());
        }
        catch (...)
        {
            LOG_ERROR("Unknown error during entering DFU state");
        }
    }

    std::shared_ptr< uvc_sensor > ds_device_common::get_raw_depth_sensor()
    {
        if( auto dev = dynamic_cast< d400_device * >( _owner ) )
            return dev->get_raw_depth_sensor();
        if( auto dev = dynamic_cast< d500_device * >( _owner ) )
            return dev->get_raw_depth_sensor();
        throw std::runtime_error( "device not referenced in the product line" );
    }

    bool ds_device_common::is_locked( const uint8_t * gvd_buff, uint32_t offset )
    {
        std::memcpy( &_is_locked, gvd_buff + offset, 1 );
        return _is_locked;
    }

    void ds_device_common::get_fw_details( const std::vector<uint8_t> &gvd_buff, std::string& optic_serial, std::string& asic_serial, std::string& fwv ) const
    {
        optic_serial = _hw_monitor->get_module_serial_string(gvd_buff, d400_gvd_offsets::module_serial_offset);
        asic_serial = _hw_monitor->get_module_serial_string(gvd_buff, d400_gvd_offsets::module_asic_serial_offset);
        fwv = _hw_monitor->get_firmware_version_string<uint8_t>(gvd_buff, d400_gvd_offsets::camera_fw_version_offset);
    }

    std::vector<uint8_t> ds_device_common::backup_flash( rs2_update_progress_callback_sptr callback )
    {
        int flash_size = 1024 * 2048;
        int max_bulk_size = 1016;
        int max_iterations = int(flash_size / max_bulk_size + 1);

        std::vector<uint8_t> flash;
        flash.reserve(flash_size);

        LOG_DEBUG("Flash backup started...");
        std::shared_ptr< uvc_sensor > raw_depth_sensor = get_raw_depth_sensor();
        raw_depth_sensor->invoke_powered([&](platform::uvc_device& dev)
            {
                for (int i = 0; i < max_iterations; i++)
                {
                    int offset = max_bulk_size * i;
                    int size = max_bulk_size;
                    if (i == max_iterations - 1)
                    {
                        size = flash_size - offset;
                    }

                    bool appended = false;

                    const int retries = 3;
                    for (int j = 0; j < retries && !appended; j++)
                    {
                        try
                        {
                            command cmd(ds::FRB);
                            cmd.param1 = offset;
                            cmd.param2 = size;
                            auto res = _hw_monitor->send(cmd);

                            flash.insert(flash.end(), res.begin(), res.end());
                            appended = true;
                            LOG_DEBUG("Flash backup - " << flash.size() << "/" << flash_size << " bytes downloaded");
                        }
                        catch (...)
                        {
                            if (j < retries - 1) std::this_thread::sleep_for(std::chrono::milliseconds(100));
                            else throw;
                        }
                    }

                    if (callback) callback->on_update_progress((float)i / max_iterations);
                }
                if (callback) callback->on_update_progress(1.0);
            });

        return flash;
    }

    void update_flash_section(std::shared_ptr<hw_monitor> hwm, const std::vector<uint8_t>& image, uint32_t offset, uint32_t size, rs2_update_progress_callback_sptr callback, float continue_from, float ratio, bool is_mipi)
    {
        size_t sector_count = size / ds::FLASH_SECTOR_SIZE;
        size_t first_sector = offset / ds::FLASH_SECTOR_SIZE;

        if (sector_count * ds::FLASH_SECTOR_SIZE != size)
            sector_count++;

        sector_count += first_sector;

        for (auto sector_index = first_sector; sector_index < sector_count; sector_index++)
        {
            command cmdFES(ds::FES);
            // On both FES & FWB commands We don't really need the response but we see that setting
            // false and sending many commands cause a failure. 
            // Looks like the FW is expecting it.
            cmdFES.require_response = true;
            cmdFES.param1 = (int)sector_index;
            cmdFES.param2 = 1;
            auto res = hwm->send(cmdFES);

            int max_transfer_size_per_packet = (int)HW_MONITOR_COMMAND_SIZE;

            // This is a WORKAROUND to support MIPI camera D457.
            // If the packet size is more than 128 bytes, FLASH_WRITE is not working as expected.
            // Remove this line after fixing the issue.
            if (is_mipi) max_transfer_size_per_packet = 128;

            for (int i = 0; i < ds::FLASH_SECTOR_SIZE; )
            {
                auto index = sector_index * ds::FLASH_SECTOR_SIZE + i;
                if (index >= offset + size)
                    break;
                int packet_size = std::min((int)(max_transfer_size_per_packet - (i % max_transfer_size_per_packet)), (int)(ds::FLASH_SECTOR_SIZE - i));

                command cmdFWB(ds::FWB);
                cmdFWB.require_response = true;
                cmdFWB.param1 = (int)index;
                cmdFWB.param2 = packet_size;
                cmdFWB.data.assign(image.data() + index, image.data() + index + packet_size);
                int retries = 0;
                bool fwb_succeeded = false;
                while (!fwb_succeeded && retries < 3)
                {
                    try
                    {
                        res = hwm->send(cmdFWB);
                        fwb_succeeded = true;
                        i += packet_size;
                    }
                    catch (const std::exception& e)
                    {
                        LOG_DEBUG("FWB command failed during section flash retry, retry number: " << retries << " ,error: " << e.what());
                        retries++;
                        if (retries == 3)
                        {
                            LOG_ERROR("FWB command failed 3 times during section flash!");
                            throw;
                        }
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    }
                }


            }

            if (callback)
                callback->on_update_progress(continue_from + (float)sector_index / (float)sector_count * ratio);
        }
    }

    void update_section(std::shared_ptr<hw_monitor> hwm, const std::vector<uint8_t>& merged_image, flash_section fs, uint32_t tables_size,
        rs2_update_progress_callback_sptr callback, float continue_from, float ratio, bool is_mipi)
    {
        auto first_table_offset = fs.tables.front().offset;
        float total_size = float(fs.app_size + tables_size);

        float app_ratio = fs.app_size / total_size * ratio;
        float tables_ratio = tables_size / total_size * ratio;

        update_flash_section(hwm, merged_image, fs.offset, fs.app_size, callback, continue_from, app_ratio, is_mipi);
        update_flash_section(hwm, merged_image, first_table_offset, tables_size, callback, app_ratio, tables_ratio, is_mipi);
    }

    void update_flash_internal(std::shared_ptr<hw_monitor> hwm, const std::vector<uint8_t>& image, std::vector<uint8_t>& flash_backup, rs2_update_progress_callback_sptr callback, int update_mode, bool is_mipi)
    {
        auto flash_image_info = ds::get_flash_info(image);
        auto flash_backup_info = ds::get_flash_info(flash_backup);
        auto merged_image = merge_images(flash_backup_info, flash_image_info, image);

        // update read-write section
        auto first_table_offset = flash_image_info.read_write_section.tables.front().offset;
        auto tables_size = flash_image_info.header.read_write_start_address + flash_image_info.header.read_write_size - first_table_offset;
        update_section(hwm, merged_image, flash_image_info.read_write_section, tables_size, callback, 0, update_mode == RS2_UNSIGNED_UPDATE_MODE_READ_ONLY ? 0.5f : 1.0f, is_mipi);

        if (update_mode == RS2_UNSIGNED_UPDATE_MODE_READ_ONLY)
        {
            // update read-only section
            auto first_table_offset = flash_image_info.read_only_section.tables.front().offset;
            auto tables_size = flash_image_info.header.read_only_start_address + flash_image_info.header.read_only_size - first_table_offset;
            update_section(hwm, merged_image, flash_image_info.read_only_section, tables_size, callback, 0.5, 0.5, is_mipi);
        }
    }

    void ds_device_common::update_flash(const std::vector<uint8_t>& image, rs2_update_progress_callback_sptr callback, int update_mode)
    {
        // check if the given FW size matches the expected FW size
        if( image.size() != unsigned_fw_size )
        {
            throw librealsense::invalid_value_exception( rsutils::string::from()
                << "Unsupported firmware binary image (unsigned) provided - "
                << image.size() << " bytes" );
        }


        if (_is_locked)
            throw std::runtime_error("this camera is locked and doesn't allow direct flash write, for firmware update use rs2_update_firmware method (DFU)");

        auto raw_depth_sensor = get_raw_depth_sensor();
        raw_depth_sensor->invoke_powered([&](platform::uvc_device& dev)
            {
                command cmdPFD(ds::PFD);
                cmdPFD.require_response = false;
                auto res = _hw_monitor->send(cmdPFD);

                switch (update_mode)
                {
                case RS2_UNSIGNED_UPDATE_MODE_FULL:
                    update_flash_section(_hw_monitor, image, 0, ds::FLASH_SIZE, callback, 0, 1.0, _is_mipi);
                    break;
                case RS2_UNSIGNED_UPDATE_MODE_UPDATE:
                case RS2_UNSIGNED_UPDATE_MODE_READ_ONLY:
                {
                    auto flash_backup = backup_flash(nullptr);
                    update_flash_internal(_hw_monitor, image, flash_backup, callback, update_mode, _is_mipi);
                    break;
                }
                default:
                    throw std::runtime_error("invalid update mode value");
                }

                if (callback) callback->on_update_progress(1.0);

                command cmdHWRST(ds::HWRST);
                cmdHWRST.require_response = false;
                res = _hw_monitor->send(cmdHWRST);
            });
    }

    bool ds_device_common::is_camera_in_advanced_mode() const
    {
        command cmd(ds::UAMG);
        assert(_hw_monitor);
        auto ret = _hw_monitor->send(cmd);
        if (ret.empty())
            throw invalid_value_exception("command result is empty!");

        return (0 != ret.front());
    }

    ds_notification_decoder::ds_notification_decoder( const std::map< int, std::string > & descriptions )
        : _descriptions( descriptions )
    {
    }
            
    notification ds_notification_decoder::decode( int value )
    {
        auto iter = _descriptions.find( static_cast< uint8_t >( value ) );
        if( iter != _descriptions.end() )
            return { RS2_NOTIFICATION_CATEGORY_HARDWARE_ERROR, value, RS2_LOG_SEVERITY_ERROR, iter->second };

        return { RS2_NOTIFICATION_CATEGORY_HARDWARE_ERROR, value, RS2_LOG_SEVERITY_WARN,
                 ( rsutils::string::from() << "HW report - unresolved type " << value ) };
    }

    processing_blocks get_ds_depth_recommended_proccesing_blocks()
    {
        auto res = get_depth_recommended_proccesing_blocks();
        res.push_back(std::make_shared<hdr_merge>()); // Requires HDR
        res.push_back(std::make_shared<sequence_id_filter>());
        res.push_back(std::make_shared<threshold>());
        res.push_back(std::make_shared<disparity_transform>(true));
        res.push_back(std::make_shared<spatial_filter>());
        res.push_back(std::make_shared<temporal_filter>());
        res.push_back(std::make_shared<hole_filling_filter>());
        res.push_back(std::make_shared<disparity_transform>(false));
        return res;
    }
} // namespace librealsense
