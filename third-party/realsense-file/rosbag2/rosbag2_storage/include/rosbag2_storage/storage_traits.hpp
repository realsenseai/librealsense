// Copyright 2018,  Open Source Robotics Foundation, Inc.
// Copyright 2018,  Bosch Software Innovations GmbH.
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

#ifndef ROSBAG2_STORAGE__STORAGE_TRAITS_HPP_
#define ROSBAG2_STORAGE__STORAGE_TRAITS_HPP_

#include "rosbag2_storage/storage_interfaces/base_io_interface.hpp"
#include "rosbag2_storage/storage_interfaces/read_only_interface.hpp"
#include "rosbag2_storage/storage_interfaces/read_write_interface.hpp"

namespace rosbag2_storage
{

template<typename T>
struct StorageTraits
{};

template<>
struct StorageTraits<storage_interfaces::ReadWriteInterface>
{
  static constexpr storage_interfaces::IOFlag io_flag = storage_interfaces::IOFlag::READ_WRITE;
  static constexpr const char * name = "rosbag2_storage::storage_interfaces::ReadWriteInterface";
};

template<>
struct StorageTraits<storage_interfaces::ReadOnlyInterface>
{
  static constexpr storage_interfaces::IOFlag io_flag = storage_interfaces::IOFlag::READ_ONLY;
  static constexpr const char * name = "rosbag2_storage::storage_interfaces::ReadOnlyInterface";
};

}  // namespace rosbag2_storage

#endif  // ROSBAG2_STORAGE__STORAGE_TRAITS_HPP_
