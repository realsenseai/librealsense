// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2024 RealSense, Inc. All Rights Reserved.
#pragma once

#include <src/core/device-interface.h>
#include "fw-update-device-interface.h"
#include "usb/usb-device.h"
#include "platform/mipi-device.h"
#include <iosfwd>

namespace librealsense
{
    typedef enum rs2_dfu_status {
        RS2_DFU_STATUS_OK = 0x00,
        RS2_DFU_STATUS_TARGET = 0x01,       // File is not targeted for use by this device.
        RS2_DFU_STATUS_FILE = 0x02,         // File is for this device, but fails some vendor-specific verification test.
        RS2_DFU_STATUS_WRITE = 0x03,        // Device is unable to write memory.
        RS2_DFU_STATUS_ERASE = 0x04,        // Memory erase function failed.
        RS2_DFU_STATUS_CHECK_ERASED = 0x05, // Memory erase check failed.
        RS2_DFU_STATUS_PROG = 0x06,         // Program memory function failed.
        RS2_DFU_STATUS_VERIFY = 0x07,       // Programmed memory failed verification.
        RS2_DFU_STATUS_ADDRESS = 0x08,      // Cannot program memory due to received address that is out of range.
        RS2_DFU_STATUS_NOTDONE = 0x09,      // Received DFU_DNLOAD with wLength = 0, but device does    not think it has all of the data yet.
        RS2_DFU_STATUS_FIRMWARE = 0x0A,     // Device's firmware is corrupt.It cannot return to run - time    (non - DFU) operations.
        RS2_DFU_STATUS_VENDOR = 0x0B,       // iString indicates a vendor - specific RS2_DFU_STATUS_or.
        RS2_DFU_STATUS_USBR = 0x0C,         // Device detected unexpected USB reset signaling.
        RS2_DFU_STATUS_POR = 0x0D,          // Device detected unexpected power on reset.
        RS2_DFU_STATUS_UNKNOWN = 0x0E,      //  Something went wrong, but the device does not know what it    was.
        RS2_DFU_STATUS_STALLEDPKT = 0x0F    // Device stalled an unexpected request.
    } rs2_dfu_status;

    typedef enum rs2_dfu_state {
        RS2_DFU_STATE_APP_IDLE = 0,
        RS2_DFU_STATE_APP_DETACH = 1,
        RS2_DFU_STATE_DFU_IDLE = 2,
        RS2_DFU_STATE_DFU_DOWNLOAD_SYNC = 3,
        RS2_DFU_STATE_DFU_DOWNLOAD_BUSY = 4,
        RS2_DFU_STATE_DFU_DOWNLOAD_IDLE = 5,
        RS2_DFU_STATE_DFU_MANIFEST_SYNC = 6,
        RS2_DFU_STATE_DFU_MANIFEST = 7,
        RS2_DFU_STATE_DFU_MANIFEST_WAIT_RESET = 8,
        RS2_DFU_STATE_DFU_UPLOAD_IDLE = 9,
        RS2_DFU_STATE_DFU_ERROR = 10
    } rs2_dfu_state;

    std::ostream & operator<<( std::ostream &, rs2_dfu_state );

    typedef enum rs2_dfu_command {
        RS2_DFU_DETACH = 0,
        RS2_DFU_DOWNLOAD = 1,
        RS2_DFU_UPLOAD = 2,
        RS2_DFU_GET_STATUS = 3,
        RS2_DFU_CLEAR_STATUS = 4,
        RS2_DFU_GET_STATE = 5,
        RS2_DFU_ABORT = 6
    } rs2_dfu_command;

    struct dfu_status_payload {
        uint32_t    bStatus : 8;
        uint32_t    bwPollTimeout : 24;
        uint8_t    bState;
        uint8_t    iString;

        dfu_status_payload()
        {
            bStatus = RS2_DFU_STATUS_UNKNOWN;
            bwPollTimeout = 0;
            bState = RS2_DFU_STATE_DFU_ERROR;
            iString = 0;
        }

        bool is_in_state(const rs2_dfu_state state) const
        {
            return bStatus == RS2_DFU_STATUS_OK && bState == state;
        }

        bool is_error_state() const { return bState == RS2_DFU_STATE_DFU_ERROR; }
        bool is_ok() const { return bStatus == RS2_DFU_STATUS_OK; }
        rs2_dfu_state get_state() { return static_cast<rs2_dfu_state>(bState); }
        rs2_dfu_status get_status() { return static_cast<rs2_dfu_status>(bStatus); }
    };

    struct serial_number_data
    {
        uint8_t serial[6];
        uint8_t spare[2];
    };

    struct dfu_fw_status_payload
    {
        uint32_t spare1;
        uint32_t fw_last_version;
        uint32_t fw_highest_version;
        uint16_t fw_download_status;
        uint16_t dfu_is_locked;
        uint16_t dfu_version;
        serial_number_data serial_number;
        uint8_t spare2[42];
    };

    class update_device
        : public device_interface
        , public update_device_interface
    {
    public:
        update_device( std::shared_ptr< const device_info > const &,
                       std::shared_ptr< platform::usb_device > const & usb_device,
                       const std::string & _product_line );

        update_device( std::shared_ptr< const device_info > const &,
                       std::shared_ptr< platform::mipi_device > const & mipi_device,
                       const std::string & _product_line );

        virtual ~update_device();

        virtual void update(const void* fw_image, int fw_image_size, rs2_update_progress_callback_sptr = nullptr) const override;

        virtual sensor_interface& get_sensor(size_t i) override;

        virtual const sensor_interface& get_sensor(size_t i) const override;

        virtual size_t get_sensors_count() const override;

        virtual void hardware_reset() override;

        virtual std::shared_ptr<matcher> create_matcher(const frame_holder& frame) const override;

        virtual std::shared_ptr<context> get_context() const override;

        virtual std::shared_ptr< const device_info > get_device_info() const override;

        virtual std::pair<uint32_t, rs2_extrinsics> get_extrinsics(const stream_interface& stream) const override;

        virtual bool is_valid() const override;

        virtual bool is_in_recovery_mode() const override;

        virtual std::vector<tagged_profile> get_profiles_tags() const override;

        virtual void tag_profiles(stream_profiles profiles) const override;

        virtual bool compress_while_record() const override;

        virtual bool contradicts(const stream_profile_interface* a, const std::vector<stream_profile>& others) const override { return false; }

        //info_interface
        virtual const std::string& get_info(rs2_camera_info info) const override;

        virtual bool supports_info(rs2_camera_info info) const override;

        //recordable
        virtual void create_snapshot(std::shared_ptr<info_interface>& snapshot) const override;

        virtual void enable_recording(std::function<void(const info_interface&)> recording_function) override;

    protected:
        void update_usb(const void* fw_image, int fw_image_size, rs2_update_progress_callback_sptr = nullptr) const;
        void update_mipi(const void* fw_image, int fw_image_size, rs2_update_progress_callback_sptr = nullptr) const;
        rs2_dfu_state get_dfu_state(std::shared_ptr<platform::usb_messenger> messenger) const;
        void detach(std::shared_ptr<platform::usb_messenger> messenger) const;
        bool wait_for_state(std::shared_ptr<platform::usb_messenger> messenger, const rs2_dfu_state state, size_t timeout = 1000) const;
        virtual void dfu_manifest_phase(const platform::rs_usb_messenger& messenger, rs2_update_progress_callback_sptr update_progress_callback) const;
        void read_device_info(std::shared_ptr<platform::usb_messenger> messenger);
        std::string parse_serial_number(const std::vector<uint8_t>& buffer) const;

        const std::string & get_name() const { return _name; }
        const std::string & get_product_line() const { return _product_line; }
        const std::string & get_serial_number() const { return _serial_number; }
        std::string to_string(platform::usb_status state) const;
        virtual float compute_progress(float progress, float start, float end, float threshold) const;

        const int DEFAULT_TIMEOUT = 100;
        const int FW_UPDATE_INTERFACE_NUMBER = 0;
        const std::shared_ptr< const device_info > _dev_info;
        const platform::rs_usb_device _usb_device;
        const platform::rs_mipi_device _mipi_device;
        std::vector<uint8_t> _serial_number_buffer;
        std::string _highest_fw_version;
        std::string _last_fw_version;
        std::string _physical_port;
        std::string _pid;
        bool _is_dfu_locked = false;
        std::string _name;
        std::string _product_line;
        std::string _serial_number;
        std::string _connection_type;
    };
}
