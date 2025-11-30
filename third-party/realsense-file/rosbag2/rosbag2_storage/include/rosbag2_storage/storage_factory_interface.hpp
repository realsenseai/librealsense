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

#ifndef ROSBAG2_STORAGE__STORAGE_FACTORY_INTERFACE_HPP_
#define ROSBAG2_STORAGE__STORAGE_FACTORY_INTERFACE_HPP_

#include <memory>
#include <string>

#include "rosbag2_storage/storage_interfaces/read_only_interface.hpp"
#include "rosbag2_storage/storage_interfaces/read_write_interface.hpp"
#include "rosbag2_storage/visibility_control.hpp"

namespace rosbag2_storage
{

class ROSBAG2_STORAGE_PUBLIC StorageFactoryInterface
{
public:
  virtual ~StorageFactoryInterface() = default;

  virtual std::shared_ptr<storage_interfaces::ReadOnlyInterface>
  open_read_only(const std::string & uri, const std::string & storage_id) = 0;

  virtual std::shared_ptr<storage_interfaces::ReadWriteInterface>
  open_read_write(const std::string & uri, const std::string & storage_id) = 0;
};

}  // namespace rosbag2_storage


#endif  // ROSBAG2_STORAGE__STORAGE_FACTORY_INTERFACE_HPP_
