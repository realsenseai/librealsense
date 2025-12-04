// Copyright 2018 Open Source Robotics Foundation, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef ROSBAG2_STORAGE__STORAGE_INTERFACES__READ_WRITE_INTERFACE_HPP_
#define ROSBAG2_STORAGE__STORAGE_INTERFACES__READ_WRITE_INTERFACE_HPP_

#include <string>

#include "rosbag2_storage/storage_filter.hpp"
#include "rosbag2_storage/storage_interfaces/read_only_interface.hpp"
#include "rosbag2_storage/storage_interfaces/base_write_interface.hpp"
#include "rosbag2_storage/visibility_control.hpp"

namespace rosbag2_storage
{
namespace storage_interfaces
{

class ROSBAG2_STORAGE_PUBLIC ReadWriteInterface
  : public ReadOnlyInterface, public BaseWriteInterface
{
public:
  ~ReadWriteInterface() override = default;

  void open(const std::string & uri, IOFlag io_flag = IOFlag::READ_WRITE) override = 0;

  uint64_t get_bagfile_size() const override = 0;

  std::string get_storage_identifier() const override = 0;

  virtual uint64_t get_minimum_split_file_size() const = 0;

  void set_filter(const StorageFilter & storage_filter) override = 0;

  void reset_filter() override = 0;
};

}  // namespace storage_interfaces
}  // namespace rosbag2_storage

#endif  // ROSBAG2_STORAGE__STORAGE_INTERFACES__READ_WRITE_INTERFACE_HPP_
