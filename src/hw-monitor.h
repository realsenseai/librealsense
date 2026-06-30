// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2015 RealSense, Inc. All Rights Reserved.

#pragma once

#include "uvc-sensor.h"
#include <mutex>
#include "platform/command-transfer.h"
#include <string>
#include <algorithm>
#include <vector>


namespace librealsense
{
    const uint8_t   IV_COMMAND_FIRMWARE_UPDATE_MODE = 0x01;
    const uint8_t   IV_COMMAND_GET_CALIBRATION_DATA = 0x02;
    const uint8_t   IV_COMMAND_LASER_POWER          = 0x03;
    const uint8_t   IV_COMMAND_DEPTH_ACCURACY       = 0x04;
    const uint8_t   IV_COMMAND_ZUNIT                = 0x05;
    const uint8_t   IV_COMMAND_LOW_CONFIDENCE_LEVEL = 0x06;
    const uint8_t   IV_COMMAND_INTENSITY_IMAGE_TYPE = 0x07;
    const uint8_t   IV_COMMAND_MOTION_VS_RANGE_TRADE= 0x08;
    const uint8_t   IV_COMMAND_POWER_GEAR           = 0x09;
    const uint8_t   IV_COMMAND_FILTER_OPTION        = 0x0A;
    const uint8_t   IV_COMMAND_VERSION              = 0x0B;
    const uint8_t   IV_COMMAND_CONFIDENCE_THRESHHOLD= 0x0C;

    const uint8_t   IVCAM_MONITOR_INTERFACE         = 0x4;
    const uint8_t   IVCAM_MONITOR_ENDPOINT_OUT      = 0x1;
    const uint8_t   IVCAM_MONITOR_ENDPOINT_IN       = 0x81;
    const uint8_t   IVCAM_MIN_SUPPORTED_VERSION     = 13;
    const uint8_t   IVCAM_MONITOR_HEADER_SIZE       = (sizeof(uint32_t) * 6);
    const uint8_t   NUM_OF_CALIBRATION_PARAMS       = 100;
    const uint8_t   PARAMETERS2_BUFFER_SIZE          = 50;
    const uint8_t   SIZE_OF_CALIB_HEADER_BYTES      = 4;
    const uint8_t   NUM_OF_CALIBRATION_COEFFS       = 64;

    const uint16_t  MAX_SIZE_OF_CALIB_PARAM_BYTES   = 800;
    const uint16_t  SIZE_OF_CALIB_PARAM_BYTES       = 512;
    const uint16_t  IVCAM_MONITOR_MAGIC_NUMBER      = 0xcdab;
    const uint16_t  IVCAM_MONITOR_MAX_BUFFER_SIZE   = 1024;
    const uint16_t  IVCAM_MONITOR_MUTEX_TIMEOUT     = 3000;
    const uint16_t  HW_MONITOR_COMMAND_SIZE         = 1000;
    const uint16_t  HW_MONITOR_BUFFER_SIZE          = 1024;
    const uint16_t  HW_MONITOR_DATA_SIZE_OFFSET     = 1020;
    const uint16_t  SIZE_OF_HW_MONITOR_HEADER       = 4;

    class uvc_sensor;

    class locked_transfer
    {
    public:
        locked_transfer(std::shared_ptr<platform::command_transfer> command_transfer, const std::shared_ptr< uvc_sensor > & uvc_ep)
            :_command_transfer(command_transfer),
            _uvc_sensor_base(uvc_ep)
        {}

        std::vector<uint8_t> send_receive(
            uint8_t const * pb, size_t cb,
            int timeout_ms = 5000,
            bool require_response = true)
        {
            std::shared_ptr<int> token(_heap.allocate(), [&](int* ptr)
            {
                if (ptr) _heap.deallocate(ptr);
            });
            if( !token.get() ) throw io_exception( "heap allocation failed" );

            std::lock_guard<std::recursive_mutex> lock(_local_mtx);
            auto strong_uvc = _uvc_sensor_base.lock();
            if( ! strong_uvc )
                return std::vector< uint8_t >();

            return strong_uvc->invoke_powered([&] ( platform::uvc_device & dev )
                {
                    std::lock_guard<platform::uvc_device> lock(dev);
                    if (_command_transfer)
                        return _command_transfer->send_receive(pb, cb, timeout_ms, require_response);
                    else
                        throw std::runtime_error("command_transfer data member NULL - objet must have been destroyed");
                });
        }

        ~locked_transfer()
        {
            try
            {
                _heap.wait_until_empty();
            }
            catch(...)
            {
                LOG_DEBUG( "Error while waiting for an empty heap" );
            }
        }
    private:
        std::shared_ptr<platform::command_transfer> _command_transfer;
        std::weak_ptr< uvc_sensor> _uvc_sensor_base;
        std::recursive_mutex _local_mtx;
        small_heap<int, 256> _heap;
    };

    struct command
    {
        uint8_t  cmd;
        uint32_t param1;
        uint32_t param2;
        uint32_t param3;
        uint32_t param4;
        std::vector<uint8_t> data;
        int     timeout_ms = 5000;
        bool    require_response = true;

        explicit command(uint8_t cmd, uint32_t param1 = 0, uint32_t param2 = 0,
                uint32_t param3 = 0, uint32_t param4 = 0, int timeout_ms = 5000,
                bool require_response = true)
            : cmd(cmd), param1(param1),
              param2(param2),
              param3(param3), param4(param4),
              timeout_ms(timeout_ms), require_response(require_response)
        {
        }
    };

    typedef int32_t hwmon_response_type;
    class hwmon_response_interface { // base class for different product lines to implement responses
    public:
        virtual std::string hwmon_error2str(int e) const = 0;
        virtual hwmon_response_type success_value() const = 0;
    };

    class hw_monitor
    {
    protected:
        struct hwmon_cmd_details
        {
            bool                                         require_response;
            std::array<uint8_t, HW_MONITOR_BUFFER_SIZE>  sendCommandData;
            int                                          sizeOfSendCommandData;
            long                                         timeOut;
            std::array<uint8_t, 4>                       receivedOpcode;
            std::array<uint8_t, HW_MONITOR_BUFFER_SIZE>  receivedCommandData;
            size_t                                       receivedCommandDataLength;
        };

        void execute_usb_command(uint8_t const *out, size_t outSize, uint32_t& op, uint8_t* in, 
            size_t& inSize, bool require_response) const;
        static void update_cmd_details(hwmon_cmd_details& details, size_t receivedCmdLen, unsigned char* outputBuffer);
        void send_hw_monitor_command(hwmon_cmd_details& details) const;

        std::shared_ptr<locked_transfer> _locked_transfer;

        static const size_t size_of_command_without_data = 24U;

    public:
        explicit hw_monitor(std::shared_ptr<locked_transfer> locked_transfer, std::shared_ptr<hwmon_response_interface> hwmon_response)
            : _locked_transfer(std::move(locked_transfer)), _hwmon_response(hwmon_response)
        {}

        static void fill_usb_buffer( int opCodeNumber,
                                      int p1,
                                      int p2,
                                      int p3,
                                      int p4,
                                      uint8_t const * data,
                                      int dataLength,
                                      uint8_t * bufferToSend,
                                      int & length );

        static command build_command_from_data(const std::vector<uint8_t> data);

        virtual std::vector<uint8_t> send( std::vector<uint8_t> const & data ) const;
        virtual std::vector<uint8_t> send( command const & cmd, hwmon_response_type * = nullptr, bool locked_transfer = false ) const;
        static std::vector<uint8_t> build_command(uint32_t opcode,
            uint32_t param1 = 0,
            uint32_t param2 = 0,
            uint32_t param3 = 0,
            uint32_t param4 = 0,
            uint8_t const * data = nullptr,
            size_t dataLength = 0);

        void get_gvd( size_t sz,
                      unsigned char * gvd,
                      uint8_t gvd_cmd,
                      const std::set< int32_t > * retry_error_codes = nullptr ) const;

        template<typename T>
        std::string get_firmware_version_string( const std::vector< uint8_t > & buff,
                                                 size_t index,
                                                 size_t length = 4,
                                                 bool reversed = true )
        {
            std::stringstream formattedBuffer;
            auto component_bytes_size = sizeof( T );
            std::string s = "";
            if( buff.size() < index + ( length * component_bytes_size ))
            {
                // Don't throw as we want to be back compatible even w/o a working version
                LOG_ERROR( "GVD FW version cannot be read!" );
                return formattedBuffer.str();
            }

            // We iterate through the version components (major.minor.patch.build) and append each
            // string value to the result string
            std::vector<int> components_value;
            for( auto i = 0; i < length; i++ )
            {
                size_t component_index = index + ( i * component_bytes_size );

                // We use int on purpose as types like uint8_t doesn't work as expected with << operator
                int component_value =  *reinterpret_cast< const T * >( buff.data() + component_index );
                components_value.push_back(component_value);
            }

            if( reversed )
                std::reverse( components_value.begin(), components_value.end() );
            
            for( auto & element : components_value )
            {
                formattedBuffer << s << element;
                s = ".";
            }

            return formattedBuffer.str();
        }

        static std::string get_module_serial_string(const std::vector<uint8_t>& buff, size_t index, size_t length = 6);

        template <typename T>
        T get_gvd_field(const std::vector<uint8_t>& data, size_t index)
        {
            T rv = 0;
            if (index + sizeof(T) >= data.size())
                throw std::runtime_error("get_gvd_field - index out of bounds, buffer size: " +
                    std::to_string(data.size()) + ", index: " + std::to_string(index));
            for (int i = 0; i < sizeof(T); i++)
                rv += data[index + i] << (i * 8);
            return rv;
        }

        std::shared_ptr<hwmon_response_interface> _hwmon_response;

        std::string hwmon_error_string(command const&, hwmon_response_type e) const;
    };
}
