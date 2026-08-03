// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2022 RealSense, Inc. All Rights Reserved.

#pragma once

#include "hw-monitor.h"
#include "ds-private.h"
#include <src/core/notification.h>
#include <src/core/roi.h>

#include <chrono>


namespace librealsense
{
    class device_info;

    class ds_auto_exposure_roi_method : public region_of_interest_method
    {
    public:
        explicit ds_auto_exposure_roi_method(const hw_monitor& hwm,
            ds::fw_cmd cmd = ds::fw_cmd::SETAEROI);

        void set(const region_of_interest& roi) override;
        region_of_interest get() const override;
    private:
        const ds::fw_cmd _cmd;
        const hw_monitor& _hw_monitor;
    };

    class ds_device_common
    {
    public:
        ds_device_common(device* ds_device, std::shared_ptr<hw_monitor> hwm, bool is_mipi = false) :
            _owner(ds_device),
            _hw_monitor(hwm),
            _is_mipi(is_mipi),
            _is_locked(true)
        {}

        void enter_update_state(const command& cmd) const;
        std::vector<uint8_t> backup_flash( rs2_update_progress_callback_sptr callback);
        void update_flash(const std::vector<uint8_t>& image, rs2_update_progress_callback_sptr callback, int update_mode);

        bool is_camera_in_advanced_mode() const;
        bool is_locked( const uint8_t * gvd_buff, uint32_t offset );

        void hardware_reset( std::chrono::milliseconds reconnect_delay );

        void pause_options_watchers();
        void unpause_options_watchers();

    private:
        std::shared_ptr< uvc_sensor > get_raw_depth_sensor();

        // MIPI devices do not re-enumerate on reset; fake a disconnect-reconnect so the SDK rebuilds the device.
        static void simulate_device_reconnect( std::shared_ptr< const device_info > dev_info,
                                               std::chrono::milliseconds reconnect_delay );

        device* _owner;
        std::shared_ptr<hw_monitor> _hw_monitor;
        bool _is_locked;
        bool _is_mipi;
    };


    // RAII: pause the device's options watchers for the guard's lifetime.
    // Possible use - a MIPI device HW reset or FW update, whose watcher would otherwise keep polling a device that is mid-restart.
    class options_watcher_pause_guard
    {
    public:
        explicit options_watcher_pause_guard( ds_device_common & dev ) : _dev( dev )
        {
            _dev.pause_options_watchers();
        }
        ~options_watcher_pause_guard() noexcept { _dev.unpause_options_watchers(); }

        options_watcher_pause_guard( const options_watcher_pause_guard & ) = delete;
        options_watcher_pause_guard & operator=( const options_watcher_pause_guard & ) = delete;

    private:
        ds_device_common & _dev;
    };



    class ds_notification_decoder : public notification_decoder
    {
    public:
        ds_notification_decoder( const std::map< int, std::string > & descriptions );
        notification decode( int value ) override;

    private:
        const std::map< int, std::string > & _descriptions;
    };

    processing_blocks get_ds_depth_recommended_proccesing_blocks();
}
