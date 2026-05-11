// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2017-2024 RealSense, Inc. All Rights Reserved.

#ifndef LIBREALSENSE_RS2_DEVICE_HPP
#define LIBREALSENSE_RS2_DEVICE_HPP

#include "rs_types.hpp"
#include "rs_sensor.hpp"
#include <vector>
#include <string>

namespace rs2
{
    class context;
    class device_list;
    class pipeline_profile;
    class device_hub;

    class device
    {
    public:
        /**
        * returns the list of adjacent devices, sharing the same physical parent composite device
        * \return            the list of adjacent devices
        */
        std::vector<sensor> query_sensors() const
        {
            rs2_error* e = nullptr;
            std::shared_ptr<rs2_sensor_list> list(
                rs2_query_sensors(_dev.get(), &e),
                rs2_delete_sensor_list);
            error::handle(e);

            auto size = rs2_get_sensors_count(list.get(), &e);
            error::handle(e);

            std::vector<sensor> results;
            for (auto i = 0; i < size; i++)
            {
                std::shared_ptr<rs2_sensor> dev(
                    rs2_create_sensor(list.get(), i, &e),
                    rs2_delete_sensor);
                error::handle(e);

                sensor rs2_dev(dev);
                results.push_back(rs2_dev);
            }

            return results;
        }

        /**
         * \return   the type of device: USB/GMSL/DDS, etc.
         */
        std::string get_type() const
        {
            if( supports( RS2_CAMERA_INFO_CONNECTION_TYPE ) )
                return get_info(RS2_CAMERA_INFO_CONNECTION_TYPE);
            return {};
        }

        /**
         * \return   the one-line description: "[<type>] <name> s/n <#>"
         */
        std::string get_description() const
        {
            std::ostringstream os;
            auto type = get_type();
            if( supports( RS2_CAMERA_INFO_NAME ) )
            {
                if( ! type.empty() )
                    os << "[" << type << "] ";
                os << get_info( RS2_CAMERA_INFO_NAME );
            }
            else
            {
                if( ! type.empty() )
                    os << type << " device";
                else
                    os << "unknown device";
            }
            if( supports( RS2_CAMERA_INFO_SERIAL_NUMBER ) )
                os << " s/n " << get_info( RS2_CAMERA_INFO_SERIAL_NUMBER );
            return os.str();
        }

        template<class T>
        T first() const
        {
            for (auto&& s : query_sensors())
            {
                if (auto t = s.as<T>()) return t;
            }
            throw rs2::error("Could not find requested sensor type!");
        }

        /**
        * check if specific camera info is supported
        * \param[in] info    the parameter to check for support
        * \return                true if the parameter both exist and well-defined for the specific device
        */
        bool supports(rs2_camera_info info) const
        {
            rs2_error* e = nullptr;
            auto is_supported = rs2_supports_device_info(_dev.get(), info, &e);
            error::handle(e);
            return is_supported > 0;
        }

        /**
        * retrieve camera specific information, like versions of various internal components
        * \param[in] info     camera info type to retrieve
        * \return             the requested camera info string, in a format specific to the device model
        */
        const char* get_info(rs2_camera_info info) const
        {
            rs2_error* e = nullptr;
            auto result = rs2_get_device_info(_dev.get(), info, &e);
            error::handle(e);
            return result;
        }

        /**
        * send hardware reset request to the device
        */
        void hardware_reset()
        {
            rs2_error* e = nullptr;
            rs2_hardware_reset(_dev.get(), &e);
            error::handle(e);
        }

        /**
        * Find if a camera is in recovery mode
        */
        bool is_in_recovery_mode()
        {
            rs2_error* e = nullptr;
            auto result = rs2_is_in_recovery_mode(_dev.get(), &e);
            error::handle(e);
            return result;
        }

        device& operator=(const std::shared_ptr<rs2_device> dev)
        {
            _dev.reset();
            _dev = dev;
            return *this;
        }
        device& operator=(const device& dev)
        {
            *this = nullptr;
            _dev = dev._dev;
            return *this;
        }
        device() : _dev(nullptr) {}

        // Note: this checks the validity of rs2::device (i.e., if it's connected to a realsense device), and does
        // NOT reflect the current condition (connected/disconnected). Use is_connected() for that.
        operator bool() const
        {
            return _dev != nullptr;
        }
        const std::shared_ptr<rs2_device>& get() const
        {
            return _dev;
        }
        bool operator<( device const & other ) const
        {
            // All RealSense cameras have an update-ID but not always a serial number
            return std::strcmp( get_info( RS2_CAMERA_INFO_FIRMWARE_UPDATE_ID ),
                          other.get_info( RS2_CAMERA_INFO_FIRMWARE_UPDATE_ID ) )
                 < 0;
        }

        bool is_connected() const
        {
            rs2_error * e = nullptr;
            bool connected = rs2_device_is_connected( _dev.get(), &e );
            error::handle( e );
            return connected;
        }

        // Minimum firmware version supported by this device's SKU (e.g. "5.10.0.17").
        // Throws if the device does not implement the FW-update protocol or has no defined minimum.
        std::string get_firmware_min_version() const
        {
            rs2_error * e = nullptr;
            auto res = rs2_get_firmware_min_version( _dev.get(), &e );
            error::handle( e );
            return res ? std::string( res ) : std::string();
        }

        template<class T>
        bool is() const
        {
            T extension(*this);
            return extension;
        }

        template<class T>
        T as() const
        {
            T extension(*this);
            return extension;
        }
        virtual ~device()
        {
        }

        explicit operator std::shared_ptr<rs2_device>() { return _dev; };
        explicit device(std::shared_ptr<rs2_device> dev) : _dev(dev) {}
    protected:
        friend class rs2::context;
        friend class rs2::device_list;
        friend class rs2::pipeline_profile;
        friend class rs2::device_hub;

        std::shared_ptr<rs2_device> _dev;

    };

    template<class T>
    class update_progress_callback : public rs2_update_progress_callback
    {
        T _callback;

    public:
        explicit update_progress_callback(T callback) : _callback(callback) {}

        void on_update_progress(const float progress) override
        {
            _callback(progress);
        }

        void release() override { delete this; }
    };

    class updatable : public device
    {
    public:
        updatable() : device() {}
        updatable(device d)
            : device(d.get())
        {
            rs2_error* e = nullptr;
            if (rs2_is_device_extendable_to(_dev.get(), RS2_EXTENSION_UPDATABLE, &e) == 0 && !e)
            {
                _dev.reset();
            }
            error::handle(e);
        }

        // Move the device to update state, this will cause the updatable device to disconnect and reconnect as an update device.
        void enter_update_state() const
        {
            rs2_error* e = nullptr;
            rs2_enter_update_state(_dev.get(), &e);
            error::handle(e);
        }

        // Create backup of camera flash memory. Such backup does not constitute valid firmware image, and cannot be
        // loaded back to the device, but it does contain all calibration and device information."
        std::vector<uint8_t> create_flash_backup() const
        {
            std::vector<uint8_t> results;

            rs2_error* e = nullptr;
            std::shared_ptr<const rs2_raw_data_buffer> list(
                rs2_create_flash_backup_cpp(_dev.get(), nullptr, &e),
                rs2_delete_raw_data);
            error::handle(e);

            auto size = rs2_get_raw_data_size(list.get(), &e);
            error::handle(e);

            auto start = rs2_get_raw_data(list.get(), &e);

            results.insert(results.begin(), start, start + size);

            return results;
        }

        template<class T>
        std::vector<uint8_t> create_flash_backup(T callback) const
        {
            std::vector<uint8_t> results;

            rs2_error* e = nullptr;
            std::shared_ptr<const rs2_raw_data_buffer> list(
                rs2_create_flash_backup_cpp(_dev.get(), new update_progress_callback<T>(std::move(callback)), &e),
                rs2_delete_raw_data);
            error::handle(e);

            auto size = rs2_get_raw_data_size(list.get(), &e);
            error::handle(e);

            auto start = rs2_get_raw_data(list.get(), &e);

            results.insert(results.begin(), start, start + size);

            return results;
        }

        // check firmware compatibility with SKU
        bool check_firmware_compatibility(const std::vector<uint8_t>& image) const
        {
            rs2_error* e = nullptr;
            auto res = !!rs2_check_firmware_compatibility(_dev.get(), image.data(), (int)image.size(), &e);
            error::handle(e);
            return res;
        }

        // Update an updatable device to the provided unsigned firmware. This call is executed on the caller's thread.
        void update_unsigned(const std::vector<uint8_t>& image, int update_mode = RS2_UNSIGNED_UPDATE_MODE_UPDATE) const
        {
            rs2_error* e = nullptr;
            rs2_update_firmware_unsigned_cpp(_dev.get(), image.data(), (int)image.size(), nullptr, update_mode, &e);
            error::handle(e);
        }

        // Update an updatable device to the provided unsigned firmware. This call is executed on the caller's thread and it supports progress notifications via the callback.
        template<class T>
        void update_unsigned(const std::vector<uint8_t>& image, T callback, int update_mode = RS2_UNSIGNED_UPDATE_MODE_UPDATE) const
        {
            rs2_error* e = nullptr;
            rs2_update_firmware_unsigned_cpp(_dev.get(), image.data(), int(image.size()), new update_progress_callback<T>(std::move(callback)), update_mode, &e);
            error::handle(e);
        }
    };

    class update_device : public device
    {
    public:
        update_device() : device() {}
        update_device(device d)
            : device(d.get())
        {
            rs2_error* e = nullptr;
            if (rs2_is_device_extendable_to(_dev.get(), RS2_EXTENSION_UPDATE_DEVICE, &e) == 0 && !e)
            {
                _dev.reset();
            }
            error::handle(e);
        }

        // Update an updatable device to the provided firmware.
        // This call is executed on the caller's thread.
        void update(const std::vector<uint8_t>& fw_image) const
        {
            rs2_error* e = nullptr;
            rs2_update_firmware_cpp(_dev.get(), fw_image.data(), (int)fw_image.size(), NULL, &e);
            error::handle(e);
        }

        // Update an updatable device to the provided firmware.
        // This call is executed on the caller's thread and it supports progress notifications via the callback.
        template<class T>
        void update(const std::vector<uint8_t>& fw_image, T callback) const
        {
            rs2_error* e = nullptr;
            rs2_update_firmware_cpp(_dev.get(), fw_image.data(), int(fw_image.size()), new update_progress_callback<T>(std::move(callback)), &e);
            error::handle(e);
        }
    };

    typedef std::vector<uint8_t> calibration_table;

    class calibrated_device : public device
    {
    public:
        calibrated_device(device d)
            : device(d.get())
        {}

        /**
        * Write calibration that was set by set_calibration_table to device's EEPROM.
        */
        void write_calibration() const
        {
            rs2_error* e = nullptr;
            rs2_write_calibration(_dev.get(), &e);
            error::handle(e);
        }

        /**
        * Reset device to factory calibration
        */
        void reset_to_factory_calibration()
        {
            rs2_error* e = nullptr;
            rs2_reset_to_factory_calibration(_dev.get(), &e);
            error::handle(e);
        }
    };

    class auto_calibrated_device : public calibrated_device
    {
    public:
        auto_calibrated_device(device d)
            : calibrated_device(d)
        {
            rs2_error* e = nullptr;
            if (rs2_is_device_extendable_to(_dev.get(), RS2_EXTENSION_AUTO_CALIBRATED_DEVICE, &e) == 0 && !e)
            {
                _dev.reset();
            }
            error::handle(e);
        }

        /**
         * On-chip calibration intended to reduce the Depth noise. Applies to D400 Depth cameras
         * \param[in] json_content      Json string to configure regular speed on chip calibration parameters:
                                            {
                                              "calib type" : 0,
                                              "speed": 3,
                                              "scan parameter": 0,
                                              "adjust both sides": 0,
                                              "white wall mode": 0,
                                              "host assistance": 0
                                            }
                                            calib_type - calibraton type: 0 = regular, 1 = focal length, 2 = both regular and focal length in order
                                            speed - for regular calibration. value can be one of: Very fast = 0, Fast = 1, Medium = 2, Slow = 3, White wall = 4, default is Slow for type 0 and Fast for type 2
                                            scan_parameter - for regular calibration. value can be one of: Py scan (default) = 0, Rx scan = 1
                                            adjust_both_sides - for focal length calibration. value can be one of: 0 = adjust right only, 1 = adjust both sides
                                            white_wall_mode - white wall mode: 0 for normal mode and 1 for white wall mode
                                            host_assistance: 0 for no assistance, 1 for starting with assistance, 2 for first part feeding host data to firmware, 3 for second part of feeding host data to firmware (calib_type 2 only)
                                            if json is nullptr it will be ignored and calibration will use the default parameters
         * \param[out] health           The absolute value of regular calibration Health-Check captures how far camera calibration is from the optimal one
                                            [0, 0.25) - Good
                                            [0.25, 0.75) - Can be Improved
                                            [0.75, ) - Requires Calibration
                                        The absolute value of focal length calibration Health-Check captures how far camera calibration is from the optimal one
                                            [0, 0.15) - Good
                                            [0.15, 0.75) - Can be Improved
                                            [0.75, ) - Requires Calibration
                                        The two health numbers are encoded in one integer as follows for calib_type 2:
                                            Regular health number times 1000 are bits 0 to 11
                                            Regular health number is negative if bit 24 is 1
                                            Focal length health number times 1000 are bits 12 to 23
                                            Focal length health number is negative if bit 25 is 1
         * \param[in] callback          Optional callback to get progress notifications
         * \param[in] timeout_ms        Timeout in ms
         * \return                      New calibration table
        */
        template<class T>
        calibration_table run_on_chip_calibration(std::string json_content, float* health, T callback, int timeout_ms = 5000) const
        {
            std::vector<uint8_t> results;

            rs2_error* e = nullptr;
            auto buf = rs2_run_on_chip_calibration_cpp(_dev.get(), json_content.data(), int(json_content.size()), health, new update_progress_callback<T>(std::move(callback)), timeout_ms, &e);
            error::handle(e);

            std::shared_ptr<const rs2_raw_data_buffer> list(buf, rs2_delete_raw_data);

            auto size = rs2_get_raw_data_size(list.get(), &e);
            error::handle(e);

            auto start = rs2_get_raw_data(list.get(), &e);

            results.insert(results.begin(), start, start + size);

            return results;
        }

        /**
         * On-chip calibration intended to reduce the Depth noise. Applies to D400 Depth cameras
         * \param[in] json_content      Json string to configure regular speed on chip calibration parameters:
                                            {
                                              "focal length" : 0,
                                              "speed": 3,
                                              "scan parameter": 0,
                                              "adjust both sides": 0,
                                              "white wall mode": 0,
                                              "host assistance": 0
                                            }
                                            focal_length - calibraton type: 0 = regular, 1 = focal length, 2 = both regular and focal length in order
                                            speed - for regular calibration. value can be one of: Very fast = 0, Fast = 1, Medium = 2, Slow = 3, White wall = 4, default is Slow for type 0 and Fast for type 2
                                            scan_parameter - for regular calibration. value can be one of: Py scan (default) = 0, Rx scan = 1
                                            adjust_both_sides - for focal length calibration. value can be one of: 0 = adjust right only, 1 = adjust both sides
                                            white_wall_mode - white wall mode: 0 for normal mode and 1 for white wall mode
                                            host_assistance: 0 for no assistance, 1 for starting with assistance, 2 for first part feeding host data to firmware, 3 for second part of feeding host data to firmware (calib_type 2 only)
                                            if json is nullptr it will be ignored and calibration will use the default parameters
         * \param[out] health           The absolute value of regular calibration Health-Check captures how far camera calibration is from the optimal one
                                            [0, 0.25) - Good
                                            [0.25, 0.75) - Can be Improved
                                            [0.75, ) - Requires Calibration
                                        The absolute value of focal length calibration Health-Check captures how far camera calibration is from the optimal one
                                            [0, 0.15) - Good
                                            [0.15, 0.75) - Can be Improved
                                            [0.75, ) - Requires Calibration
                                        The two health numbers are encoded in one integer as follows for calib_type 2:
                                            Regular health number times 1000 are bits 0 to 11
                                            Regular health number is negative if bit 24 is 1
                                            Focal length health number times 1000 are bits 12 to 23
                                            Focal length health number is negative if bit 25 is 1
         * \param[in] timeout_ms        Timeout in ms
         * \return                      New calibration table
         */
        calibration_table run_on_chip_calibration(std::string json_content, float* health, int timeout_ms = 5000) const
        {
            std::vector<uint8_t> results;

            rs2_error* e = nullptr;
            const rs2_raw_data_buffer* buf = rs2_run_on_chip_calibration_cpp(_dev.get(), json_content.data(), static_cast< int >( json_content.size() ), health, nullptr, timeout_ms, &e);
            error::handle(e);
            std::shared_ptr<const rs2_raw_data_buffer> list(buf, rs2_delete_raw_data);

            auto size = rs2_get_raw_data_size(list.get(), &e);
            error::handle(e);

            auto start = rs2_get_raw_data(list.get(), &e);
            error::handle(e);

            results.insert(results.begin(), start, start + size);

            return results;
        }

        /**
         * Tare calibration adjusts the camera absolute distance to flat target. Applies to D400 Depth cameras
         * User needs to enter the known ground truth.
        * \param[in] ground_truth_mm     Ground truth in mm must be between 60 and 10000
        * \param[in] json_content        Json string to configure tare calibration parameters:
                                            {
                                              "average step count": 20,
                                              "step count": 20,
                                              "accuracy": 2,
                                              "scan parameter": 0,
                                              "data sampling": 0,
                                              "host assistance": 0,
                                              "depth" : 0
                                            }
                                            average step count - number of frames to average, must be between 1 - 30, default = 20
                                            step count - max iteration steps, must be between 5 - 30, default = 10
                                            accuracy - Subpixel accuracy level, value can be one of: Very high = 0 (0.025%), High = 1 (0.05%), Medium = 2 (0.1%), Low = 3 (0.2%), Default = Very high (0.025%), default is Medium
                                            scan_parameter - value can be one of: Py scan (default) = 0, Rx scan = 1
                                            data_sampling - value can be one of:polling data sampling = 0, interrupt data sampling = 1
                                            host_assistance: 0 for no assistance, 1 for starting with assistance, 2 for feeding host data to firmware
                                            depth: 0 for not relating to depth, > 0 for feeding depth from host to firmware, -1 for ending to feed depth from host to firmware
                                            if json is nullptr it will be ignored and calibration will use the default parameters
        * \param[in]  content_size       Json string size if its 0 the json will be ignored and calibration will use the default parameters
         * \param[out] health           The absolute value of regular calibration Health-Check captures how far camera calibration is from the optimal one
                                            [0, 0.25) - Good
                                            [0.25, 0.75) - Can be Improved
                                            [0.75, ) - Requires Calibration
        * \param[in]  callback           Optional callback to get progress notifications
        * \param[in] timeout_ms          Timeout in ms
        * \return                        New calibration table
        */
        template<class T>
        calibration_table run_tare_calibration(float ground_truth_mm, std::string json_content, float* health, T callback, int timeout_ms = 5000) const
        {
            std::vector<uint8_t> results;

            rs2_error* e = nullptr;
            const rs2_raw_data_buffer* buf = rs2_run_tare_calibration_cpp(_dev.get(), ground_truth_mm, json_content.data(), int(json_content.size()), health, new update_progress_callback<T>(std::move(callback)), timeout_ms, &e);
            error::handle(e);
            std::shared_ptr<const rs2_raw_data_buffer> list(buf, rs2_delete_raw_data);

            auto size = rs2_get_raw_data_size(list.get(), &e);
            error::handle(e);

            auto start = rs2_get_raw_data(list.get(), &e);

            results.insert(results.begin(), start, start + size);

            return results;
        }

        /**
         * Tare calibration adjusts the camera absolute distance to flat target. Applies to D400 Depth cameras
         * User needs to enter the known ground truth.
         * \param[in] ground_truth_mm     Ground truth in mm must be between 60 and 10000
         * \param[in] json_content        Json string to configure tare calibration parameters:
                                             {
                                               "average step count": 20,
                                               "step count": 20,
                                               "accuracy": 2,
                                               "scan parameter": 0,
                                               "data sampling": 0,
                                               "host assistance": 0,
                                               "depth" : 0
                                             }
                                             average step count - number of frames to average, must be between 1 - 30, default = 20
                                             step count - max iteration steps, must be between 5 - 30, default = 10
                                             accuracy - Subpixel accuracy level, value can be one of: Very high = 0 (0.025%), High = 1 (0.05%), Medium = 2 (0.1%), Low = 3 (0.2%), Default = Very high (0.025%), default is Medium
                                             scan_parameter - value can be one of: Py scan (default) = 0, Rx scan = 1
                                             data_sampling - value can be one of:polling data sampling = 0, interrupt data sampling = 1
                                             host_assistance: 0 for no assistance, 1 for starting with assistance, 2 for feeding host data to firmware
                                             depth: 0 for not relating to depth, > 0 for feeding depth from host to firmware, -1 for ending to feed depth from host to firmware
                                             if json is nullptr it will be ignored and calibration will use the default parameters
         * \param[in]  content_size       Json string size if its 0 the json will be ignored and calibration will use the default parameters
         * \param[out] health           The absolute value of regular calibration Health-Check captures how far camera calibration is from the optimal one
                                            [0, 0.25) - Good
                                            [0.25, 0.75) - Can be Improved
                                            [0.75, ) - Requires Calibration
         * \param[in] timeout_ms          Timeout in ms
         * \return                        New calibration table
         */
        calibration_table run_tare_calibration(float ground_truth_mm, std::string json_content, float * health, int timeout_ms = 5000) const
        {
            std::vector<uint8_t> results;

            rs2_error* e = nullptr;
            const rs2_raw_data_buffer* buf = rs2_run_tare_calibration_cpp(_dev.get(), ground_truth_mm, json_content.data(), static_cast< int >( json_content.size() ), health, nullptr, timeout_ms, &e);
            error::handle(e);
            std::shared_ptr<const rs2_raw_data_buffer> list(buf, rs2_delete_raw_data);

            auto size = rs2_get_raw_data_size(list.get(), &e);
            error::handle(e);

            auto start = rs2_get_raw_data(list.get(), &e);

            results.insert(results.begin(), start, start + size);

            return results;
        }

        /**
        * When doing a host-assited calibration (Tare or on-chip) add frame to the calibration process
         * \param[in] f     The next depth frame.
         * \param[in] callback            Optional callback to get progress notifications
         * \param[in] timeout_ms          Timeout in ms
         * \param[out] health             The health check numbers before and after calibration
        * \return a New calibration table when process is done. An empty table otherwise - need more frames.
        **/
        template<class T>
        calibration_table process_calibration_frame(rs2::frame f, float* const health, T callback, int timeout_ms = 5000) const
        {
            std::vector<uint8_t> results;

            rs2_error* e = nullptr;
            const rs2_raw_data_buffer* buf = rs2_process_calibration_frame(_dev.get(), f.get(), health, new update_progress_callback<T>(std::move(callback)), timeout_ms, &e);
            error::handle(e);
            std::shared_ptr<const rs2_raw_data_buffer> list(buf, rs2_delete_raw_data);

            auto size = rs2_get_raw_data_size(list.get(), &e);
            error::handle(e);

            auto start = rs2_get_raw_data(list.get(), &e);

            results.insert(results.begin(), start, start + size);

            return results;
        }

        /**
        * When doing a host-assited calibration (Tare or on-chip) add frame to the calibration process
         * \param[in] f     The next depth frame.
         * \param[in] timeout_ms          Timeout in ms
         * \param[out] health             The health check numbers before and after calibration
        * \return a New calibration table when process is done. An empty table otherwise - need more frames.
        **/
        calibration_table process_calibration_frame(rs2::frame f, float* const health, int timeout_ms = 5000) const
        {
            std::vector<uint8_t> results;

            rs2_error* e = nullptr;
            const rs2_raw_data_buffer* buf = rs2_process_calibration_frame(_dev.get(), f.get(), health, nullptr, timeout_ms, &e);
            error::handle(e);
            std::shared_ptr<const rs2_raw_data_buffer> list(buf, rs2_delete_raw_data);

            auto size = rs2_get_raw_data_size(list.get(), &e);
            error::handle(e);

            auto start = rs2_get_raw_data(list.get(), &e);

            results.insert(results.begin(), start, start + size);

            return results;
        }

        /**
        *  Read current calibration table from flash.
        * \return    Calibration table
        */
        calibration_table get_calibration_table()
        {
            std::vector<uint8_t> results;

            rs2_error* e = nullptr;
            std::shared_ptr<const rs2_raw_data_buffer> list(
                rs2_get_calibration_table(_dev.get(), &e),
                rs2_delete_raw_data);
            error::handle(e);

            auto size = rs2_get_raw_data_size(list.get(), &e);
            error::handle(e);

            auto start = rs2_get_raw_data(list.get(), &e);

            results.insert(results.begin(), start, start + size);

            return results;
        }

        /**
        *  Set current table to dynamic area.
        * \param[in]     Calibration table
        */
        void set_calibration_table(const calibration_table& calibration)
        {
            rs2_error* e = nullptr;
            rs2_set_calibration_table(_dev.get(), calibration.data(), static_cast< int >( calibration.size() ), &e);
            error::handle(e);
        }

        /**
        *  Run target-based focal length calibration for D400 Stereo Cameras
        * \param[in]    left_queue: container for left IR frames with resoluton of 1280x720 and the target in the center of 320x240 pixels ROI.
        * \param[in]    right_queue: container for right IR frames with resoluton of 1280x720 and the target in the center of 320x240 pixels ROI
        * \param[in]    target_w: the rectangle width in mm on the target
        * \param[in]    target_h: the rectangle height in mm on the target
        * \param[in]    adjust_both_sides: 1 for adjusting both left and right camera calibration tables and 0 for adjusting right camera calibraion table only
        * \param[out]   ratio: the corrected ratio from the calibration
        * \param[out]   angle: the target's tilt angle
        * \return       New calibration table
        */
        std::vector<uint8_t> run_focal_length_calibration(rs2::frame_queue left, rs2::frame_queue right, float target_w, float target_h, int adjust_both_sides,
            float* ratio, float* angle) const
        {
            std::vector<uint8_t> results;

            rs2_error* e = nullptr;
            std::shared_ptr<const rs2_raw_data_buffer> list(
                rs2_run_focal_length_calibration_cpp(_dev.get(), left.get().get(), right.get().get(), target_w, target_h, adjust_both_sides, ratio, angle, nullptr, &e),
                rs2_delete_raw_data);
            error::handle(e);

            auto size = rs2_get_raw_data_size(list.get(), &e);
            error::handle(e);

            auto start = rs2_get_raw_data(list.get(), &e);

            results.insert(results.begin(), start, start + size);

            return results;
        }

        /**
        *  Run target-based focal length calibration for D400 Stereo Cameras
        * \param[in]    left_queue: container for left IR frames with resoluton of 1280x720 and the target in the center of 320x240 pixels ROI.
        * \param[in]    right_queue: container for right IR frames with resoluton of 1280x720 and the target in the center of 320x240 pixels ROI
        * \param[in]    target_w: the rectangle width in mm on the target
        * \param[in]    target_h: the rectangle height in mm on the target
        * \param[in]    adjust_both_sides: 1 for adjusting both left and right camera calibration tables and 0 for adjusting right camera calibraion table only
        * \param[out]   ratio: the corrected ratio from the calibration
        * \param[out]   angle: the target's tilt angle
        * \return       New calibration table
        */
        template<class T>
        std::vector<uint8_t> run_focal_length_calibration(rs2::frame_queue left, rs2::frame_queue right, float target_w, float target_h, int adjust_both_sides,
            float* ratio, float* angle, T callback) const
        {
            std::vector<uint8_t> results;

            rs2_error* e = nullptr;
            std::shared_ptr<const rs2_raw_data_buffer> list(
                rs2_run_focal_length_calibration_cpp(_dev.get(), left.get().get(), right.get().get(), target_w, target_h, adjust_both_sides, ratio, angle,
                    new update_progress_callback<T>(std::move(callback)), &e),
                rs2_delete_raw_data);
            error::handle(e);

            auto size = rs2_get_raw_data_size(list.get(), &e);
            error::handle(e);

            auto start = rs2_get_raw_data(list.get(), &e);

            results.insert(results.begin(), start, start + size);

            return results;
        }

        /**
        *  Depth-RGB UV-Map calibration. Applicable for D400 cameras
        * \param[in]    left: container for left IR frames with resoluton of 1280x720 and the target in the center of 320x240 pixels ROI.
        * \param[in]    color: container for RGB frames with resoluton of 1280x720 and the target in the center of 320x240 pixels ROI
        * \param[in]    depth: container for Depth frames with resoluton of 1280x720 and the target in the center of 320x240 pixels ROI
        * \param[in]    py_px_only: 1 for calibrating color camera py and px only, 1 for calibrating color camera py, px, fy, and fx.
        * \param[out]   health: The four health check numbers int the oorder of px, py, fx, fy for the calibration
        * \param[in]    health_size: number of health check numbers, which is 4 by default
        * \param[in]    callback: Optional callback for update progress notifications, the progress value is normailzed to 1
        * \return       New calibration table
        */
        std::vector<uint8_t> run_uv_map_calibration(rs2::frame_queue left, rs2::frame_queue color, rs2::frame_queue depth, int py_px_only,
            float* health, int health_size) const
        {
            std::vector<uint8_t> results;

            rs2_error* e = nullptr;
            std::shared_ptr<const rs2_raw_data_buffer> list(
                rs2_run_uv_map_calibration_cpp(_dev.get(), left.get().get(), color.get().get(), depth.get().get(), py_px_only, health, health_size, nullptr, &e),
                rs2_delete_raw_data);
            error::handle(e);

            auto size = rs2_get_raw_data_size(list.get(), &e);
            error::handle(e);

            auto start = rs2_get_raw_data(list.get(), &e);

            results.insert(results.begin(), start, start + size);

            return results;
        }

        /**
        *  Depth-RGB UV-Map calibration. Applicable for D400 cameras
        * \param[in]    left: container for left IR frames with resoluton of 1280x720 and the target in the center of 320x240 pixels ROI.
        * \param[in]    color: container for RGB frames with resoluton of 1280x720 and the target in the center of 320x240 pixels ROI
        * \param[in]    depth: container for Depth frames with resoluton of 1280x720 and the target in the center of 320x240 pixels ROI
        * \param[in]    py_px_only: 1 for calibrating color camera py and px only, 1 for calibrating color camera py, px, fy, and fx.
        * \param[out]   health: The four health check numbers in order of px, py, fx, fy for the calibration
        * \param[in]    health_size: number of health check numbers, which is 4 by default
        * \param[in]    callback: Optional callback for update progress notifications, the progress value is normailzed to 1
        * \param[in]    client_data: Optional client data for the callback
        * \return       New calibration table
        */
        template<class T>
        std::vector<uint8_t> run_uv_map_calibration(rs2::frame_queue left, rs2::frame_queue color, rs2::frame_queue depth, int py_px_only,
            float* health, int health_size, T callback) const
        {
            std::vector<uint8_t> results;

            rs2_error* e = nullptr;
            std::shared_ptr<const rs2_raw_data_buffer> list(
                rs2_run_uv_map_calibration_cpp(_dev.get(), left.get().get(), color.get().get(), depth.get().get(), py_px_only, health, health_size,
                    new update_progress_callback<T>(std::move(callback)), &e),
                rs2_delete_raw_data);
            error::handle(e);

            auto size = rs2_get_raw_data_size(list.get(), &e);
            error::handle(e);

            auto start = rs2_get_raw_data(list.get(), &e);

            results.insert(results.begin(), start, start + size);

            return results;
        }

        /**
        *  Calculate Z for calibration target - distance to the target's plane
        * \param[in]    queue1-3: Frame queues of raw images used to calculate and extract the distance to a predefined target pattern.
        * For D400 the indexes 1-3 correspond to Left IR, Right IR and Depth with only the Left IR being used
        * \param[in]    target_width: expected target's horizontal dimension in mm
        * \param[in]    target_height: expected target's vertical dimension in mm
        * \return       Calculated distance (Z) to target in millimeter, return negative number on failure
        */
        float calculate_target_z(rs2::frame_queue queue1, rs2::frame_queue queue2, rs2::frame_queue queue3,
            float target_width, float target_height) const
        {
            rs2_error* e = nullptr;
            float result = rs2_calculate_target_z_cpp(_dev.get(), queue1.get().get(), queue2.get().get(), queue3.get().get(),
                target_width, target_height, nullptr, &e);
            error::handle(e);

            return result;
        }

        /**
        *  Calculate Z for calibration target - distance to the target's plane
        * \param[in]    queue1-3: Frame queues of raw images used to calculate and extract the distance to a predefined target pattern.
        * For D400 the indexes 1-3 correspond to Left IR, Right IR and Depth with only the Left IR being used
        * \param[in]    target_width: expected target's horizontal dimension in mm
        * \param[in]    target_height: expected target's vertical dimension in mm
        * \param[in]    callback: Optional callback for reporting progress status
        * \return       Calculated distance (Z) to target in millimeter, return negative number on failure
        */
        template<class T>
        float calculate_target_z(rs2::frame_queue queue1, rs2::frame_queue queue2, rs2::frame_queue queue3,
            float target_width, float target_height, T callback) const
        {
            rs2_error* e = nullptr;
            float result = rs2_calculate_target_z_cpp(_dev.get(), queue1.get().get(), queue2.get().get(), queue3.get().get(),
                target_width, target_height, new update_progress_callback<T>(std::move(callback)), &e);
            error::handle(e);

            return result;
        }

        std::string get_calibration_config() const
        {
            std::vector<uint8_t> result;

            rs2_error* e = nullptr;
            auto buffer = rs2_get_calibration_config(_dev.get(), &e);

            std::shared_ptr<const rs2_raw_data_buffer> list(buffer, rs2_delete_raw_data);
            error::handle(e);

            auto size = rs2_get_raw_data_size(list.get(), &e);
            error::handle(e);

            auto start = rs2_get_raw_data(list.get(), &e);
            error::handle(e);

            result.insert(result.begin(), start, start + size);

            return std::string(result.begin(), result.end());
        }

        void set_calibration_config(const std::string& calibration_config_json_str) const
        {
            rs2_error* e = nullptr;
            rs2_set_calibration_config(_dev.get(), calibration_config_json_str.c_str(), &e);
            error::handle(e);
        }
    };

    /*
        Wrapper around any callback function that is given to calibration_change_callback.
    */
    template< class callback >
    class calibration_change_callback : public rs2_calibration_change_callback
    {
        //using callback = std::function< void( rs2_calibration_status ) >;
        callback _callback;
    public:
        calibration_change_callback( callback cb ) : _callback( cb ) {}

        void on_calibration_change( rs2_calibration_status status ) noexcept override
        {
            try
            {
                _callback( status );
            }
            catch( ... ) { }
        }
        void release() override { delete this; }
    };

    class calibration_change_device : public device
    {
    public:
        calibration_change_device() = default;
        calibration_change_device(device d)
            : device(d.get())
        {
            rs2_error* e = nullptr;
            if( ! rs2_is_device_extendable_to( _dev.get(), RS2_EXTENSION_CALIBRATION_CHANGE_DEVICE, &e )  &&  ! e )
            {
                _dev.reset();
            }
            error::handle(e);
        }

        /*
        Your callback should look like this, for example:
            sensor.register_calibration_change_callback(
                []( rs2_calibration_status ) noexcept
                {
                    ...
                })
        */
        template< typename T >
        void register_calibration_change_callback(T callback)
        {
            // We wrap the callback with an interface and pass it to librealsense, who will
            // now manage its lifetime. Rather than deleting it, though, it will call its
            // release() function, where (back in our context) it can be safely deleted:
            rs2_error* e = nullptr;
            rs2_register_calibration_change_callback_cpp(
                _dev.get(),
                new calibration_change_callback< T >(std::move(callback)),
                &e);
            error::handle(e);
        }
    };

    class device_calibration : public calibration_change_device
    {
    public:
        device_calibration() = default;
        device_calibration( device d )
        {
            rs2_error* e = nullptr;
            if( rs2_is_device_extendable_to( d.get().get(), RS2_EXTENSION_DEVICE_CALIBRATION, &e ))
            {
                _dev = d.get();
            }
            error::handle( e );
        }

        /**
        * This will trigger the given calibration, if available
        */
        void trigger_device_calibration( rs2_calibration_type type )
        {
            rs2_error* e = nullptr;
            rs2_trigger_device_calibration( _dev.get(), type, &e );
            error::handle( e );
        }
    };

    class debug_protocol : public device
    {
    public:
        debug_protocol(device d)
                : device(d.get())
        {
            rs2_error* e = nullptr;
            if(rs2_is_device_extendable_to(_dev.get(), RS2_EXTENSION_DEBUG, &e) == 0 && !e)
            {
                _dev.reset();
            }
            error::handle(e);
        }

        std::vector<uint8_t> build_command(uint32_t opcode,
            uint32_t param1 = 0,
            uint32_t param2 = 0,
            uint32_t param3 = 0,
            uint32_t param4 = 0,
            std::vector<uint8_t> const & data = {}) const
        {
            std::vector<uint8_t> results;

            rs2_error* e = nullptr;
            auto buffer = rs2_build_debug_protocol_command(_dev.get(), opcode, param1, param2, param3, param4,
                (void*)data.data(), (uint32_t)data.size(), &e);
            error::handle(e);
            std::shared_ptr< const rs2_raw_data_buffer > list( buffer, rs2_delete_raw_data );

            auto size = rs2_get_raw_data_size(list.get(), &e);
            error::handle(e);

            auto start = rs2_get_raw_data(list.get(), &e);
            error::handle(e);

            results.insert(results.begin(), start, start + size);

            return results;
        }

        std::vector<uint8_t> send_and_receive_raw_data(const std::vector<uint8_t>& input) const
        {
            std::vector<uint8_t> results;

            rs2_error* e = nullptr;
            std::shared_ptr<const rs2_raw_data_buffer> list(
                    rs2_send_and_receive_raw_data(_dev.get(), (void*)input.data(), (uint32_t)input.size(), &e),
                    rs2_delete_raw_data);
            error::handle(e);

            auto size = rs2_get_raw_data_size(list.get(), &e);
            error::handle(e);

            auto start = rs2_get_raw_data(list.get(), &e);
            error::handle(e);

            results.insert(results.begin(), start, start + size);

            return results;
        }

        std::string get_opcode_string(int opcode)
        {
            rs2_error* e = nullptr;
            char buffer[1024];
            rs2_hw_monitor_get_opcode_string(opcode, buffer, sizeof(buffer), _dev.get(), &e);
            return std::string(buffer);
        }
    };

    class device_list
    {
    public:
        explicit device_list(std::shared_ptr<rs2_device_list> list)
            : _list(std::move(list)) {}

        device_list()
            : _list(nullptr) {}

        operator std::vector<device>() const
        {
            std::vector<device> res;
            for (auto&& dev : *this) res.push_back(dev);
            return res;
        }

        bool contains(const device& dev) const
        {
            rs2_error* e = nullptr;
            auto res = !!(rs2_device_list_contains(_list.get(), dev.get().get(), &e));
            error::handle(e);
            return res;
        }

        device_list& operator=(std::shared_ptr<rs2_device_list> list)
        {
            _list = std::move(list);
            return *this;
        }

        device operator[](uint32_t index) const
        {
            rs2_error* e = nullptr;
            std::shared_ptr<rs2_device> dev(
                rs2_create_device(_list.get(), index, &e),
                rs2_delete_device);
            error::handle(e);

            return device(dev);
        }

        uint32_t size() const
        {
            rs2_error* e = nullptr;
            auto size = rs2_get_device_count(_list.get(), &e);
            error::handle(e);
            return size;
        }

        device front() const { return std::move((*this)[0]); }
        device back() const
        {
            return std::move((*this)[size() - 1]);
        }

        class device_list_iterator
        {
            device_list_iterator(
                const device_list& device_list,
                uint32_t uint32_t)
                : _list(device_list),
                  _index(uint32_t)
            {
            }

        public:
            device operator*() const
            {
                return _list[_index];
            }
            bool operator!=(const device_list_iterator& other) const
            {
                return other._index != _index || &other._list != &_list;
            }
            bool operator==(const device_list_iterator& other) const
            {
                return !(*this != other);
            }
            device_list_iterator& operator++()
            {
                _index++;
                return *this;
            }
        private:
            friend device_list;
            const device_list& _list;
            uint32_t _index;
        };

        device_list_iterator begin() const
        {
            return device_list_iterator(*this, 0);
        }
        device_list_iterator end() const
        {
            return device_list_iterator(*this, size());
        }
        const rs2_device_list* get_list() const
        {
            return _list.get();
        }

        operator std::shared_ptr<rs2_device_list>() { return _list; };

    private:
        std::shared_ptr<rs2_device_list> _list;
    };
}
#endif // LIBREALSENSE_RS2_DEVICE_HPP
