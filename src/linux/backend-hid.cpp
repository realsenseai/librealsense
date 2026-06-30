// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2015 RealSense, Inc. All Rights Reserved.

#include "metadata.h"
#include "backend-hid.h"
#include "backend.h"
#include "types.h"

#include <rsutils/string/from.h>
#include "rsutils/accelerators/gpu.h"

#include <thread>
#include <chrono>
#include <ctime>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>

#pragma GCC diagnostic ignored "-Woverflow"

const size_t MAX_DEV_PARENT_DIR = 10;

const uint8_t HID_METADATA_SIZE = 8;     // bytes
const size_t HID_DATA_ACTUAL_SIZE = 6;  // bytes

const std::string IIO_DEVICE_PREFIX("iio:device");
const std::string IIO_ROOT_PATH("/sys/bus/iio/devices");
const std::string HID_CUSTOM_PATH("/sys/bus/platform/drivers/hid_sensor_custom");

//#define DEBUG_HID
#ifdef DEBUG_HID
#define LOG_DEBUG_HID(...)   do { CLOG(DEBUG   ,LIBREALSENSE_ELPP_ID) << __VA_ARGS__; } while(false)
#else
#define LOG_DEBUG_HID(...)
#endif //DEBUG_HID

namespace librealsense
{
    namespace platform
    {
        hid_input::hid_input(const std::string& iio_device_path, const std::string& input_name)
        {
            info.device_path = iio_device_path;
            static const std::string input_prefix = "in_";
            // validate if input includes th "in_" prefix. if it is . remove it.
            if (input_name.substr(0,input_prefix.size()) == input_prefix)
            {
                info.input = input_name.substr(input_prefix.size(), input_name.size());
            }
            else
            {
                info.input = input_name;
            }

            init();
        }

        hid_input::~hid_input()
        {
            try
            {
                enable( false );
            }
            catch(...)
            {
                LOG_DEBUG( "Error while disabling a hid device" );
            }
        }

        // enable scan input. doing so cause the input to be part of the data provided in the polling.
        void hid_input::enable(bool is_enable)
        {
            auto input_data = is_enable ? 1 : 0;
            // open the element requested and enable and disable.
            auto element_path = info.device_path + "/scan_elements/" + "in_" + info.input + "_en";
            std::ofstream iio_device_file(element_path);

            if (!iio_device_file.is_open())
            {
                throw linux_backend_exception(rsutils::string::from() << "Failed to open scan_element " << element_path);
            }
            iio_device_file << input_data;
            iio_device_file.close();

            info.enabled = is_enable;
        }

        // initialize the input by reading the input parameters.
        void hid_input::init()
        {
            char buffer[1024] = {};

            static const std::string input_suffix = "_en";
            // check if input contains the "en" suffix, if it is . remove it.
            if (info.input.substr(info.input.size()-input_suffix.size(), input_suffix.size()) == input_suffix) {
                info.input = info.input.substr(0, info.input.size()-input_suffix.size());
            }

            // read scan type.
            auto read_scan_type_path = std::string(info.device_path + "/scan_elements/in_" + info.input + "_type");
            std::ifstream device_type_file(read_scan_type_path);
            if (!device_type_file)
            {
                throw linux_backend_exception(rsutils::string::from() << "Failed to open read_scan_type " << read_scan_type_path);
            }

            device_type_file.getline(buffer, sizeof(buffer));
            uint32_t pad_int;
            char sign_char, endia_nchar;
            // TODO: parse with regex
            auto ret = std::sscanf(buffer,
                                   "%ce:%c%u/%u>>%u",
                                   &endia_nchar,
                                   &sign_char,
                                   &info.bits_used,
                                   &pad_int,
                                   &info.shift);

            if (ret < 0)
            {
                throw linux_backend_exception(rsutils::string::from() << "Failed to parse device_type " << read_scan_type_path);
            }

            device_type_file.close();

            info.big_endian = (endia_nchar == 'b');
            info.bytes = pad_int / 8;
            info.is_signed = (sign_char == 's');

            if (info.bits_used == 64)
                info.mask = ~0;
            else
                info.mask = (1ULL << info.bits_used) - 1;


            // read scan index.
            auto read_scan_index_path = info.device_path + "/scan_elements/in_" + info.input + "_index";
            std::ifstream device_index_file(read_scan_index_path);
            if (!device_index_file)
            {
                throw linux_backend_exception(rsutils::string::from() << "Failed to open scan_index " << read_scan_index_path);
            }

            device_index_file.getline(buffer, sizeof(buffer));
            info.index = std::stoi(buffer);

            device_index_file.close();

            // read enable state.
            auto read_enable_state_path = info.device_path + "/scan_elements/in_" + info.input + "_en";
            std::ifstream device_enabled_file(read_enable_state_path);
            if (!device_enabled_file)
            {
                throw linux_backend_exception(rsutils::string::from() << "Failed to open scan_index " << read_enable_state_path);
            }

            device_enabled_file.getline(buffer, sizeof(buffer));
            info.enabled = (std::stoi(buffer) == 0) ? false : true;

            device_enabled_file.close();
        }

        hid_custom_sensor::hid_custom_sensor(const std::string& device_path, const std::string& sensor_name)
            : _fd(0),
              _stop_pipe_fd{},
              _custom_device_path(device_path),
              _custom_sensor_name(sensor_name),
              _custom_device_name(""),
              _callback(nullptr),
              _is_capturing(false),
              _hid_thread(nullptr)
        {
            init();
        }

        hid_custom_sensor::~hid_custom_sensor()
        {
            try
            {
                if (_is_capturing)
                    stop_capture();
            }
            catch(...)
            {
                LOG_ERROR("An error has occurred while hid_custom_sensor dtor()!");
            }
        }

        std::vector<uint8_t> hid_custom_sensor::get_report_data(const std::string& report_name, custom_sensor_report_field report_field)
        {
            static const std::map<custom_sensor_report_field, std::string> report_fields = {{minimum,   "-minimum"},
                                                                                            {maximum,   "-maximum"},
                                                                                            {name,      "-name"},
                                                                                            {size,      "-size"},
                                                                                            {unit_expo, "-unit-expo"},
                                                                                            {units,     "-units"},
                                                                                            {value,     "-value"}};
            try{
                auto& report_folder = _reports.at(report_name);
                auto report_path = _custom_device_path + "/" + report_folder + "/" + report_folder + report_fields.at(report_field);
                return read_report(report_path);
            }
            catch(std::out_of_range)
            {
                throw invalid_value_exception(rsutils::string::from() << "report directory name " << report_name << " not found!");
            }
        }


        // start capturing and polling.
        void hid_custom_sensor::start_capture(hid_callback sensor_callback)
        {
            if (_is_capturing)
                return;

            std::ostringstream device_path;
            device_path << "/dev/" << _custom_device_name;

            auto read_device_path_str = device_path.str();
            std::ifstream device_file(read_device_path_str);

            // find device in file system.
            if (!device_file.good())
            {
                throw linux_backend_exception("custom hid device is busy or not found!");
            }

            device_file.close();

            enable(true);

            const auto max_retries = 10;
            auto retries = 0;
            while(++retries < max_retries)
            {
                if ((_fd = open(read_device_path_str.c_str(), O_RDONLY | O_NONBLOCK)) > 0)
                    break;

                LOG_WARNING("open() failed!");
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }

            if ((retries == max_retries) && (_fd <= 0))
            {
                enable(false);
                throw linux_backend_exception("open() failed with all retries!");
            }

            if (pipe(_stop_pipe_fd) < 0)
            {
                close(_fd);
                enable(false);
                throw linux_backend_exception("hid_custom_sensor: Cannot create pipe!");
            }

            _callback = sensor_callback;
            _is_capturing = true;
            _hid_thread = std::unique_ptr<std::thread>(new std::thread([this, read_device_path_str](){
                const uint32_t channel_size = 24; // TODO: why 24?
                std::vector<uint8_t> raw_data(channel_size * hid_buf_len);

                do {
                    fd_set fds;
                    FD_ZERO(&fds);
                    FD_SET(_fd, &fds);
                    FD_SET(_stop_pipe_fd[0], &fds);

                    int max_fd = std::max(_stop_pipe_fd[0], _fd);
                    size_t read_size = 0;

                    struct timeval tv = {5,0};
                    LOG_DEBUG_HID("HID Select initiated");
                    auto val = select(max_fd + 1, &fds, nullptr, nullptr, &tv);
                    LOG_DEBUG_HID("HID Select done, val = " << val);
                    if (val < 0)
                    {
                        // TODO: write to log?
                        continue;
                    }
                    else if (val > 0)
                    {
                        if(FD_ISSET(_stop_pipe_fd[0], &fds))
                        {
                            if(!_is_capturing)
                            {
                                LOG_INFO("hid_custom_sensor: Stream finished");
                                return;
                            }
                        }
                        else if (FD_ISSET(_fd, &fds))
                        {
                            read_size = read(_fd, raw_data.data(), raw_data.size());
                            if (read_size <= 0 )
                                continue;
                        }
                        else
                        {
                            // TODO: write to log?
                            LOG_WARNING("HID unresolved event : after select->FD_ISSET");
                            continue;
                        }

                        auto sz= read_size / channel_size;
                        if (sz > 2)
                        {
                            LOG_DEBUG("HID: Going to handle " <<  sz << " packets");
                        }
                        for (auto i = 0; i < read_size / channel_size; ++i)
                        {
                            auto p_raw_data = raw_data.data() + channel_size * i;

                            // TODO: code refactoring to reduce latency
                            sensor_data sens_data{};
                            sens_data.sensor = hid_sensor{get_sensor_name()};

                            sens_data.fo = {channel_size, channel_size, p_raw_data, p_raw_data};
                            this->_callback(sens_data);
                        }
                        if (sz > 2)
                        {
                            LOG_DEBUG("HID: Finished to handle " <<  sz << " packets");
                        }
                    }
                    else
                    {
                        LOG_WARNING("hid_custom_sensor: Frames didn't arrived within 5 seconds");
                    }
                } while(this->_is_capturing);
            }));
        }

        void hid_custom_sensor::stop_capture()
        {
            if (!_is_capturing)
            {
                enable(false);
                return;
            }

            _is_capturing = false;
            signal_stop();
            _hid_thread->join();
            enable(false);
            _callback = nullptr;

            if(::close(_fd) < 0)
                throw linux_backend_exception("hid_custom_sensor: close(_fd) failed");

            if(::close(_stop_pipe_fd[0]) < 0)
               throw linux_backend_exception("hid_custom_sensor: close(_stop_pipe_fd[0]) failed");
            if(::close(_stop_pipe_fd[1]) < 0)
               throw linux_backend_exception("hid_custom_sensor: close(_stop_pipe_fd[1]) failed");

            _fd = 0;
            _stop_pipe_fd[0] = _stop_pipe_fd[1] = 0;
        }

        std::vector<uint8_t> hid_custom_sensor::read_report(const std::string& name_report_path)
        {
            auto fd = open(name_report_path.c_str(), O_RDONLY | O_NONBLOCK);
            if (fd < 0)
                throw linux_backend_exception("Failed to open report!");

            std::vector<uint8_t> buffer;
            buffer.resize(MAX_INPUT);
            auto read_size = read(fd, buffer.data(), buffer.size());
            close(fd);

            if (read_size <= 0)
                throw linux_backend_exception("Failed to read custom report!");

            buffer.resize(read_size);
            buffer[buffer.size() - 1] = '\0'; // Replace '\n' with '\0'
            return buffer;
        }

        void hid_custom_sensor::init()
        {
            static const char* prefix_feature_name = "feature";
            static const char* prefix_input_name = "input";
            static const char* suffix_name_field = "name";
            DIR* dir = nullptr;
            struct dirent* ent = nullptr;
            if ((dir = opendir(_custom_device_path.c_str())) != nullptr)
            {
              while ((ent = readdir(dir)) != nullptr)
              {
                  auto str = std::string(ent->d_name);
                  if (str.find(prefix_feature_name) != std::string::npos ||
                      str.find(prefix_input_name) != std::string::npos)
                  {
                      DIR* report_dir = nullptr;
                      struct dirent* report_ent = nullptr;
                      auto report_path = _custom_device_path + "/" + ent->d_name;
                      if ((report_dir = opendir(report_path.c_str())) != nullptr)
                      {
                          while ((report_ent = readdir(report_dir)) != nullptr)
                          {
                              auto report_str = std::string(report_ent->d_name);
                              if (report_str.find(suffix_name_field) != std::string::npos)
                              {
                                  auto name_report_path = report_path + "/" + report_ent->d_name;
                                  auto buffer = read_report(name_report_path);

                                  std::string name_report(reinterpret_cast<char const*>(buffer.data()));
                                  _reports.insert(std::make_pair(name_report, ent->d_name));
                              }
                          }
                          closedir(report_dir);
                      }
                  }
              }
              closedir(dir);
            }

            // get device name
            auto pos = _custom_device_path.find_last_of("/");
            if (pos < _custom_device_path.size())
                _custom_device_name = _custom_device_path.substr(pos + 1);
        }

        void hid_custom_sensor::enable(bool state)
        {
            auto input_data = state ? 1 : 0;
            auto element_path = _custom_device_path + "/enable_sensor";
            std::ofstream custom_device_file(element_path);

            if (!custom_device_file.is_open())
            {
                throw linux_backend_exception(rsutils::string::from() << "Failed to enable_sensor " << element_path);
            }
            custom_device_file << input_data;
            custom_device_file.close();

            // Work-around for the HID custom driver failing to release resources,
            // preventing the device to enter idle U3 mode
            // reading out the value will refresh the device tree in kernel
//            if (!state)
//            {
//                std::string feat_val("default");
//                if(!(std::ifstream(_custom_device_path + "/feature-0-200309/feature-0-200309-value") >> feat_val))
//                    throw linux_backend_exception("Failed to read feat_val");
//                std::cout << "Feat value is " << feat_val << std::endl;
//            }
        }

        void hid_custom_sensor::signal_stop()
        {
            char buff[1];
            buff[0] = 0;
            if (write(_stop_pipe_fd[1], buff, 1) < 0)
            {
                 throw linux_backend_exception("hid_custom_sensor: Could not signal video capture thread to stop. Error write to pipe.");
            }
        }

        iio_hid_sensor::iio_hid_sensor(const std::string& device_path, uint32_t frequency, float sensitivity)
            : _stop_pipe_fd{},
              _fd(0),
              _iio_device_number(0),
              _iio_device_path(device_path),
              _sensor_name(""),
              _sampling_frequency_name(""),
              _callback(nullptr),
              _is_capturing(false),
              _pm_dispatcher(16)    // queue for async power management commands
        {
            init(frequency, sensitivity);
        }

        iio_hid_sensor::~iio_hid_sensor()
        {
            try
            {
                try {
                    // Ensure PM sync
                    _pm_dispatcher.flush();
                    stop_capture();
                } catch(...){}

                clear_buffer();
            }
            catch(...){}

            // clear inputs.
            _inputs.clear();
        }

        // start capturing and polling.
        void iio_hid_sensor::start_capture(hid_callback sensor_callback)
        {
            if (_is_capturing)
                return;

            set_power(true);
            std::ostringstream iio_read_device_path;
            iio_read_device_path << "/dev/" << IIO_DEVICE_PREFIX << _iio_device_number;

            auto iio_read_device_path_str = iio_read_device_path.str();
            std::ifstream iio_device_file(iio_read_device_path_str);

            // find iio_device in file system.
            if (!iio_device_file.good())
            {
                throw linux_backend_exception("iio hid device is busy or not found!");
            }

            iio_device_file.close();

            // count number of enabled count elements and sort by their index.
            create_channel_array();

            const auto max_retries = 10;
            auto retries = 0;
            while(++retries < max_retries)
            {
                if ((_fd = open(iio_read_device_path_str.c_str(), O_RDONLY | O_NONBLOCK)) > 0)
                    break;

                LOG_WARNING("open() failed!");
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }

            if ((retries == max_retries) && (_fd <= 0))
            {
                _channels.clear();
                throw linux_backend_exception("open() failed with all retries!");
            }

            if (pipe(_stop_pipe_fd) < 0)
            {
                close(_fd);
                _channels.clear();
                throw linux_backend_exception("iio_hid_sensor: Cannot create pipe!");
            }

            _callback = sensor_callback;
            _is_capturing = true;
            _hid_thread = std::unique_ptr<std::thread>(new std::thread([this](){
                const uint32_t channel_size = get_channel_size();
                size_t raw_data_size = channel_size*hid_buf_len;

                std::vector<uint8_t> raw_data(raw_data_size);
                auto metadata = has_metadata();

                do {
                    fd_set fds;
                    FD_ZERO(&fds);
                    FD_SET(_fd, &fds);
                    FD_SET(_stop_pipe_fd[0], &fds);

                    int max_fd = std::max(_stop_pipe_fd[0], _fd);

                    ssize_t read_size = 0;
                    struct timeval tv = {5, 0};
                    LOG_DEBUG_HID("HID IIO Select initiated");
                    auto val = select(max_fd + 1, &fds, nullptr, nullptr, &tv);
                    LOG_DEBUG_HID("HID IIO Select done, val = " << val);

                    if (val < 0)
                    {
                        // TODO: write to log?
                        LOG_WARNING("iio_hid_sensor: select failed, return val = " << val);
                        continue;
                    }
                    else if (val > 0)
                    {
                        if(FD_ISSET(_stop_pipe_fd[0], &fds))
                        {
                            if(!_is_capturing)
                            {
                                LOG_INFO("iio_hid_sensor: Stream finished");
                                return;
                            }
                        }
                        else if (FD_ISSET(_fd, &fds))
                        {
                            read_size = read(_fd, raw_data.data(), raw_data_size);
                            if (read_size < 0 )
                                continue;
                        }
                        else
                        {
                            // TODO: write to log?
                            LOG_WARNING("HID IIO unresolved event : after select->FD_ISSET");
                            continue;
                        }

                        auto sz= read_size / channel_size;
                        if (sz > 2)
                        {
                            LOG_DEBUG("HID: Going to handle " <<  sz << " packets");
                        }
                        // TODO: code refactoring to reduce latency
                        for (auto i = 0; i < sz; ++i)
                        {
                            auto now_ts = std::chrono::duration<double, std::milli>(std::chrono::system_clock::now().time_since_epoch()).count();
                            auto p_raw_data = raw_data.data() + channel_size * i;
                            sensor_data sens_data{};
                            sens_data.sensor = hid_sensor{get_sensor_name()};

                            auto hid_data_size = channel_size - (metadata ? HID_METADATA_SIZE : 0);
                            // Populate HID IMU data - Header
                            metadata_hid_raw meta_data{};
                            meta_data.header.report_type = md_hid_report_type::hid_report_imu;
                            meta_data.header.length = hid_header_size + metadata_imu_report_size;
                            meta_data.header.timestamp = *(reinterpret_cast<uint64_t *>(&p_raw_data[16]));
                            // Payload:
                            meta_data.report_type.imu_report.header.md_type_id = md_type::META_DATA_HID_IMU_REPORT_ID;
                            meta_data.report_type.imu_report.header.md_size = metadata_imu_report_size;
//                            meta_data.report_type.imu_report.flags = static_cast<uint8_t>( md_hid_imu_attributes::custom_timestamp_attirbute |
//                                                                                            md_hid_imu_attributes::imu_counter_attribute |
//                                                                                            md_hid_imu_attributes::usb_counter_attribute);
//                            meta_data.report_type.imu_report.custom_timestamp = meta_data.header.timestamp;
//                            meta_data.report_type.imu_report.imu_counter = p_raw_data[30];
//                            meta_data.report_type.imu_report.usb_counter = p_raw_data[31];

                            sens_data.fo = {hid_data_size, metadata? meta_data.header.length: uint8_t(0),
                                            p_raw_data,  metadata? &meta_data : nullptr, now_ts};
                            //Linux HID provides timestamps in nanosec. Convert to usec (FW default)
                            if (metadata)
                            {
                                //auto* ts_nsec = reinterpret_cast<uint64_t*>(const_cast<void*>(sens_data.fo.metadata));
                                //*ts_nsec /=1000;
                                meta_data.header.timestamp /=1000;
                            }

//                            for (auto i=0ul; i<channel_size; i++)
//                                std::cout << std::hex << int(p_raw_data[i]) << " ";
//                            std::cout << std::dec << std::endl;

                            this->_callback(sens_data);
                        }
                        if (sz > 2)
                        {
                            LOG_DEBUG("HID: Finished to handle " <<  sz << " packets");
                        }
                    }
                    else
                    {
                        LOG_WARNING("iio_hid_sensor: Frames didn't arrived within the predefined interval");
                        std::this_thread::sleep_for(std::chrono::milliseconds(2));
                    }
                } while(this->_is_capturing);
            }));
        }

        void iio_hid_sensor::stop_capture()
        {
            if (!_is_capturing)
                return;

            _is_capturing = false;
            set_power(false);
            signal_stop();
            _hid_thread->join();
            _callback = nullptr;
            _channels.clear();

            if(::close(_fd) < 0)
                throw linux_backend_exception("iio_hid_sensor: close(_fd) failed");

            if(::close(_stop_pipe_fd[0]) < 0)
               throw linux_backend_exception("iio_hid_sensor: close(_stop_pipe_fd[0]) failed");
            if(::close(_stop_pipe_fd[1]) < 0)
               throw linux_backend_exception("iio_hid_sensor: close(_stop_pipe_fd[1]) failed");

            _fd = 0;
            _stop_pipe_fd[0] = _stop_pipe_fd[1] = 0;
        }

        void iio_hid_sensor::clear_buffer()
        {
            std::ostringstream iio_read_device_path;
            iio_read_device_path << "/dev/" << IIO_DEVICE_PREFIX << _iio_device_number;

            std::unique_ptr<int, std::function<void(int*)> > fd(
                        new int (_fd = open(iio_read_device_path.str().c_str(), O_RDONLY | O_NONBLOCK)),
                        [&](int* d){ if (d && (*d)) { _fd = ::close(*d); } delete d; });

            if (!(*fd > 0))
                throw linux_backend_exception("open() failed with all retries!");

            // count enabled elements and sort by their index.
            create_channel_array();

            const uint32_t channel_size = get_channel_size();
            auto raw_data_size = channel_size*hid_buf_len;

            std::vector<uint8_t> raw_data(raw_data_size);

            auto read_size = read(_fd, raw_data.data(), raw_data_size);
            while(read_size > 0)
                read_size = read(_fd, raw_data.data(), raw_data_size);

            _channels.clear();
        }

        void iio_hid_sensor::set_frequency(uint32_t frequency)
        {
            auto sampling_frequency_path = _iio_device_path + "/" + _sampling_frequency_name;
            std::ofstream iio_device_file(sampling_frequency_path);

            if (!iio_device_file.is_open())
            {
                 throw linux_backend_exception(rsutils::string::from() << "Failed to set frequency " << frequency <<
                                               ". device path: " << sampling_frequency_path);
            }
            iio_device_file << frequency;
            iio_device_file.close();
        }

        void iio_hid_sensor::set_sensitivity( float sensitivity ) 
        {
            auto sensitivity_path = _iio_device_path + "/" + _sensitivity_name;
            std::ofstream iio_device_file( sensitivity_path );

            if( ! iio_device_file.is_open() )
            {
                throw linux_backend_exception( rsutils::string::from()
                                               << "Failed to set sensitivity " << sensitivity
                                                                       << ". device path: " << sensitivity_path );
            }
            iio_device_file << sensitivity;
            iio_device_file.close();
        }

        // Asynchronous power management
        void iio_hid_sensor::set_power(bool on)
        {
            auto path = _iio_device_path + "/buffer/enable";

            // Enqueue power management change
            _pm_dispatcher.invoke([path,on](dispatcher::cancellable_timer /*t*/)
            {
                //auto st = std::chrono::high_resolution_clock::now();

                if (!write_fs_attribute(path, on))
                {
                    LOG_WARNING("HID set_power " << int(on) << " failed for " << path);
                }
            },true);
        }

        void iio_hid_sensor::signal_stop()
        {
            char buff[1];
            buff[0] = 0;
            if (write(_stop_pipe_fd[1], buff, 1) < 0)
            {
                 throw linux_backend_exception("iio_hid_sensor: Could not signal video capture thread to stop. Error write to pipe.");
            }
        }

        bool iio_hid_sensor::has_metadata()
        {
            //for FW>=5.16 HID_METADATA_SIZE is 12 
            if(get_output_size() >= HID_DATA_ACTUAL_SIZE + HID_METADATA_SIZE)
                return true;
            return false;
        }

        bool iio_hid_sensor::sort_hids(std::shared_ptr<hid_input> first, std::shared_ptr<hid_input> second)
        {
            return (second->get_hid_input_info().index >= first->get_hid_input_info().index);
        }

        void iio_hid_sensor::create_channel_array()
        {
            // build enabled channels.
            for(auto& input : _inputs)
            {
                if (input->get_hid_input_info().enabled)
                {
                    _channels.push_back(input);
                }
            }

            _channels.sort(sort_hids);
        }

        // initialize the device sensor. reading its name and all of its inputs.
        void iio_hid_sensor::init(uint32_t frequency, float sensitivity)
        {
            std::ifstream iio_device_file(_iio_device_path + "/name");

            // find iio_device in file system.
            if (!iio_device_file.good())
            {
                throw linux_backend_exception(rsutils::string::from() << "Failed to open device sensor. " << _iio_device_path);
            }

            char name_buffer[256] = {};
            iio_device_file.getline(name_buffer,sizeof(name_buffer));
            _sensor_name = std::string(name_buffer);

            iio_device_file.close();

            // get IIO device number
            static const std::string suffix_iio_device_path("/" + IIO_DEVICE_PREFIX);
            auto pos = _iio_device_path.find_last_of(suffix_iio_device_path);
            if (pos == std::string::npos)
                throw linux_backend_exception(rsutils::string::from() << "Wrong iio device path " << _iio_device_path);

            auto substr = _iio_device_path.substr(pos + 1);
            if (std::all_of(substr.begin(), substr.end(), ::isdigit))
            {
                _iio_device_number = atoi(substr.c_str());
            }
            else
            {
                throw linux_backend_exception(rsutils::string::from() << "IIO device number is incorrect! Failed to open device sensor. " << _iio_device_path);
            }

            _pm_dispatcher.start();

            // HID iio kernel driver async initialization may fail to map the kernel objects hierarchy (iio triggers) properly
            // The patch will rectify this behaviour
            std::string current_trigger = _sensor_name + "-dev" + _iio_device_path.back();
            std::string path = _iio_device_path + "/trigger/current_trigger";
            _pm_thread = std::unique_ptr<std::thread>(new std::thread([path,current_trigger](){
                bool retry =true;
                while (retry) {
                    try {
                        if (write_fs_attribute(path, current_trigger))
                            break;
                        else
                            std::this_thread::sleep_for(std::chrono::milliseconds(50));
                    }
                    catch(...){} // Device disconnect
                    retry = false;
                }
            }));
            _pm_thread->detach();

            // read all available input of the iio_device
            read_device_inputs();

            // get the specific name of sampling_frequency
            _sampling_frequency_name = get_sampling_frequency_name();

            // get the specific name of sensitivity
            _sensitivity_name = get_sensitivity_name();
            
            for (auto& input : _inputs)
                input->enable(true);

            set_frequency(frequency);
            set_sensitivity( sensitivity );
            write_fs_attribute(_iio_device_path + "/buffer/length", hid_buf_len);
        }

        // calculate the storage size of a scan
        uint32_t iio_hid_sensor::get_channel_size() const
        {
            assert(!_channels.empty());
            auto bytes = 0;

            for (auto& elem : _channels)
            {
                auto input_info = elem->get_hid_input_info();
                if (bytes % input_info.bytes == 0)
                {
                    input_info.location = bytes;
                }
                else
                {
                    input_info.location = bytes - bytes % input_info.bytes
                                          + input_info.bytes;
                }

                bytes = input_info.location + input_info.bytes;
            }

            return bytes;
        }

        // calculate the actual size of data
        uint32_t iio_hid_sensor::get_output_size() const
        {
            assert(!_channels.empty());
            auto bits_used = 0.;

            for (auto& elem : _channels)
            {
                auto input_info = elem->get_hid_input_info();
                bits_used += input_info.bits_used;
            }

            return std::ceil(bits_used / CHAR_BIT);
        }

        std::string iio_hid_sensor::get_sampling_frequency_name() const
        {
            std::string sampling_frequency_name = "";
            DIR *dir = nullptr;
            struct dirent *dir_ent = nullptr;

            // start enumerate the scan elemnts dir.
            dir = opendir(_iio_device_path.c_str());
            if (dir == nullptr)
            {
                 throw linux_backend_exception(rsutils::string::from() << "Failed to open scan_element " << _iio_device_path);
            }

            // verify file format. should include in_ (input) and _en (enable).
            while ((dir_ent = readdir(dir)) != nullptr)
            {
                if (dir_ent->d_type != DT_DIR)
                {
                    std::string file(dir_ent->d_name);
                    if (file.find("sampling_frequency") != std::string::npos)
                    {
                        sampling_frequency_name = file;
                    }
                }
            }
            closedir(dir);
            return sampling_frequency_name;
        }

        /*The sensitivity value we need to transfer to FW is stored in a file named with "hysteresis" in its title.
        Therefore, we are searching for a file that includes "hysteresis" in its name.*/
        std::string iio_hid_sensor::get_sensitivity_name() const
        {
            std::string sensitivity_name = "";
            DIR * dir = nullptr;
            struct dirent * dir_ent = nullptr;

            // start enumerate the scan elemnts dir.
            dir = opendir( _iio_device_path.c_str() );
            if( dir == nullptr )
            {
                throw linux_backend_exception( rsutils::string::from()
                                               << "Failed to open scan_element " << _iio_device_path );
            }

            // verify file format. should include in_ (input) and _en (enable).
            while( ( dir_ent = readdir( dir ) ) != nullptr )
            {
                if( dir_ent->d_type != DT_DIR )
                {
                    std::string file( dir_ent->d_name );
                    if( file.find( "hysteresis" ) != std::string::npos )
                    {
                        sensitivity_name = file;
                    }
                }
            }
            closedir( dir );
            return sensitivity_name;
        }


        // read the IIO device inputs.
        void iio_hid_sensor::read_device_inputs()
        {
            DIR *dir = nullptr;
            struct dirent *dir_ent = nullptr;

            auto scan_elements_path = _iio_device_path + "/scan_elements";
            // start enumerate the scan elemnts dir.
            dir = opendir(scan_elements_path.c_str());
            if (dir == nullptr)
            {
                throw linux_backend_exception(rsutils::string::from() << "Failed to open scan_element " << _iio_device_path);
            }

            // verify file format. should include in_ (input) and _en (enable).
            while ((dir_ent = readdir(dir)) != nullptr)
            {
                if (dir_ent->d_type != DT_DIR)
                {
                    std::string file(dir_ent->d_name);
                    std::string prefix = "in_";
                    std::string suffix = "_en";
                    if (file.substr(0,prefix.size()) == prefix &&
                        file.substr(file.size()-suffix.size(),suffix.size()) == suffix) {
                        // initialize input.

                        try
                        {
                            auto new_input = std::make_shared<hid_input>(_iio_device_path, file);
                            // push to input list.
                            
                            _inputs.push_front(new_input);
                        }
                        catch(...)
                        {
                            // fail to initialize this input. continue to the next one.
                            continue;
                        }
                    }
                }
            }
            closedir(dir);
        }

        v4l_hid_device::v4l_hid_device(const hid_device_info& info)
        {
            bool found = false;
            v4l_hid_device::foreach_hid_device([&](const hid_device_info& hid_dev_info){
                if (hid_dev_info.unique_id == info.unique_id)
                {
                    _hid_device_infos.push_back(hid_dev_info);
                    found = true;
                }
            });

            if (!found)
                throw linux_backend_exception("hid device is no longer connected!");
        }

        v4l_hid_device::~v4l_hid_device()
        {
            for (auto& elem : _streaming_iio_sensors)
            {
                try
                {
                    elem->stop_capture();
                }
                catch(...)
                {
                    LOG_DEBUG( "Error while stopping capture sensor" );
                }
            }

            for (auto& elem : _streaming_custom_sensors)
            {
                try
                {
                    elem->stop_capture();
                }
                catch(...)
                {
                    LOG_DEBUG( "Error while stopping capture sensor" );
                }
            }
        }

        void v4l_hid_device::open(const std::vector<hid_profile>& hid_profiles)
        {
            _hid_profiles = hid_profiles;
             for (auto& device_info : _hid_device_infos)
             {
                try
                {
                    if (device_info.id == custom_id)
                    {
                        auto device = std::unique_ptr<hid_custom_sensor>(new hid_custom_sensor(device_info.device_path,
                                                                                               device_info.id));
                        _hid_custom_sensors.push_back(std::move(device));
                    }
                    else
                    {
                        uint32_t frequency = 0;
                        float sensitivity = 0;
                        for (auto& profile : hid_profiles)
                        {
                            if (profile.sensor_name == device_info.id)
                            {
                                frequency = profile.frequency;
                                sensitivity = profile.sensitivity;
                                break;
                            }
                        }

                        if (frequency == 0)
                            continue;

                        auto device = std::unique_ptr<iio_hid_sensor>(new iio_hid_sensor(device_info.device_path, frequency, sensitivity));
                        _iio_hid_sensors.push_back(std::move(device));
                    }
                }
                catch(...)
                {
                    for (auto& hid_sensor : _iio_hid_sensors)
                    {
                        hid_sensor.reset();
                    }
                    _iio_hid_sensors.clear();
                    LOG_ERROR("Hid device is busy!");
                    throw;
                }
            }
        }

        void v4l_hid_device::close()
        {
            for (auto& hid_iio_sensor : _iio_hid_sensors)
            {
                hid_iio_sensor.reset();
            }
            _iio_hid_sensors.clear();

            for (auto& hid_custom_sensor : _hid_custom_sensors)
            {
                hid_custom_sensor.reset();
            }
            _hid_custom_sensors.clear();
        }

        std::vector<hid_sensor> v4l_hid_device::get_sensors()
        {
            std::vector<hid_sensor> iio_sensors;

            for (auto& sensor : _hid_profiles)
                iio_sensors.push_back({ sensor.sensor_name });


            for (auto& elem : _hid_custom_sensors)
            {
                iio_sensors.push_back(hid_sensor{elem->get_sensor_name()});
            }
            return iio_sensors;
        }

        void v4l_hid_device::start_capture(hid_callback callback)
        {
            for (auto& profile : _hid_profiles)
            {
                for (auto& sensor : _iio_hid_sensors)
                {
                    if (sensor->get_sensor_name() == profile.sensor_name)
                    {
                        _streaming_iio_sensors.push_back(sensor.get());
                    }
                }

                for (auto& sensor : _hid_custom_sensors)
                {
                    if (sensor->get_sensor_name() == profile.sensor_name)
                    {
                        _streaming_custom_sensors.push_back(sensor.get());
                    }
                }

                if (_streaming_iio_sensors.empty() && _streaming_custom_sensors.empty())
                    LOG_ERROR("sensor " + profile.sensor_name + " not found!");
            }

            if (!_streaming_iio_sensors.empty())
            {
                std::vector<iio_hid_sensor*> captured_sensors;
                try{
                for (auto& elem : _streaming_iio_sensors)
                {
                    elem->start_capture(callback);
                    captured_sensors.push_back(elem);
                }
                }
                catch(...)
                {
                    for (auto& elem : captured_sensors)
                        elem->stop_capture();

                    _streaming_iio_sensors.clear();
                    throw;
                }
            }

            if (!_streaming_custom_sensors.empty())
            {
                std::vector<hid_custom_sensor*> captured_sensors;
                try{
                for (auto& elem : _streaming_custom_sensors)
                {
                    elem->start_capture(callback);
                    captured_sensors.push_back(elem);
                }
                }
                catch(...)
                {
                    for (auto& elem : captured_sensors)
                        elem->stop_capture();

                    _streaming_custom_sensors.clear();
                    throw;
                }
            }

        }

        void v4l_hid_device::stop_capture()
        {
            for (auto& sensor : _iio_hid_sensors)
            {
                    sensor->stop_capture();
            }

            _streaming_iio_sensors.clear();

            for (auto& sensor : _hid_custom_sensors)
            {
                sensor->stop_capture();
            }

            _streaming_custom_sensors.clear();
        }

        std::vector<uint8_t> v4l_hid_device::get_custom_report_data(const std::string& custom_sensor_name,
                                                    const std::string& report_name,
                                                    custom_sensor_report_field report_field)
        {
            auto it = std::find_if(begin(_hid_custom_sensors), end(_hid_custom_sensors),
                [&](const std::unique_ptr<hid_custom_sensor>& hcs)
            {
                return hcs->get_sensor_name() == custom_sensor_name;
            });
            if (it != end(_hid_custom_sensors))
            {
                return (*it)->get_report_data(report_name, report_field);
            }
            throw linux_backend_exception(rsutils::string::from() << " custom sensor " << custom_sensor_name << " not found!");
        }

        void v4l_hid_device::foreach_hid_device(std::function<void(const hid_device_info&)> action)
        {
            // Common HID Sensors
            DIR* dir = nullptr;
            struct dirent* ent = nullptr;
            std::vector<std::string> common_sensors;
            if ((dir = opendir(IIO_ROOT_PATH.c_str())) != nullptr)
            {
              while ((ent = readdir(dir)) != nullptr)
              {
                  auto str = std::string(ent->d_name);
                  if (str.find(IIO_DEVICE_PREFIX) != std::string::npos)
                      common_sensors.push_back(IIO_ROOT_PATH + "/" + str);
              }
              closedir(dir);
            }

            for (auto& elem : common_sensors)
            {
                hid_device_info hid_dev_info{};
                if (!get_hid_device_info(elem.c_str(), hid_dev_info))
                {
#ifdef RS2_USE_CUDA
                    if (rsutils::rs2_is_cuda_available())
                    {
                        /* On the Jetson TX, ina3221x is the power monitor (I2C bus)
                        This code is checking the IIA device directory, but tries to compare as USB HID device
                        The ina3221x is not a HID device. Check here to avoid spamming the console.
                        Patch suggested by JetsonHacks: https://github.com/jetsonhacks/buildLibrealsense2TX */
                        std::string device_path_str(elem.c_str());
                        device_path_str += "/";
                        std::string dev_name;
                        std::ifstream(device_path_str + "name") >> dev_name;
                        if (dev_name != std::string("ina3221x")) {
                            LOG_WARNING("Failed to read busnum/devnum. Device Path: " << elem);
                        }
                        continue;
                    }
#endif
                    LOG_INFO("Failed to read busnum/devnum. Device Path: " << elem);
                    continue;
                }
                action(hid_dev_info);
            }


            // Custom HID Sensors
            static const char* prefix_custom_sensor_name = "HID-SENSOR-2000e1";
            std::vector<std::string> custom_sensors;
            dir = nullptr;
            ent = nullptr;
            if ((dir = opendir(HID_CUSTOM_PATH.c_str())) != nullptr)
            {
              while ((ent = readdir(dir)) != nullptr)
              {
                  auto str = std::string(ent->d_name);
                  if (str.find(prefix_custom_sensor_name) != std::string::npos)
                      custom_sensors.push_back(HID_CUSTOM_PATH + "/" + str);
              }
              closedir(dir);
            }


            for (auto& elem : custom_sensors)
            {
                hid_device_info hid_dev_info{};
                if(!get_hid_device_info(elem.c_str(), hid_dev_info))
                {
                    LOG_WARNING("Failed to read busnum/devnum. Custom HID Device Path: " << elem);
                    continue;
                }

                hid_dev_info.id = custom_id;
                action(hid_dev_info);
            }
        }

        bool v4l_hid_device::get_hid_device_info(const char* dev_path, hid_device_info& device_info)
        {
            char device_path[PATH_MAX] = {};
            if (nullptr == realpath(dev_path, device_path))
            {
                LOG_WARNING("Could not resolve HID path: " << dev_path);
                return false;
            }

            std::string device_path_str(device_path);
            device_path_str+="/";
            std::string busnum, devnum, devpath, vid, pid, dev_id, dev_name;
            std::ifstream(device_path_str + "name") >> dev_name;
            auto valid = false;
            for(auto i=0UL; i < MAX_DEV_PARENT_DIR; ++i)
            {
                if(std::ifstream(device_path_str + "busnum") >> busnum)
                {
                    if(std::ifstream(device_path_str + "devnum") >> devnum)
                    {
                        if(std::ifstream(device_path_str + "devpath") >> devpath)
                        {
                            if(std::ifstream(device_path_str + "idVendor") >> vid)
                            {
                                if(std::ifstream(device_path_str + "idProduct") >> pid)
                                {
                                    if(std::ifstream(device_path_str + "dev") >> dev_id)
                                    {
                                        valid = true;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
                device_path_str += "../";
            }

            if (valid)
            {
                device_info.vid = vid;
                device_info.pid = pid;
                device_info.unique_id = busnum + "-" + devpath + "-" + devnum;
                device_info.id = dev_name;
                device_info.device_path = device_path;
            }

            return valid;
        }
    }
}
