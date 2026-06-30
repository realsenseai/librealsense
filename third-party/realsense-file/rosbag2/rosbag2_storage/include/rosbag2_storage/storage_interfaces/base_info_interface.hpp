// Copyright 2018 Open Source Robotics Foundation, Inc.
// Copyright 2018 Bosch Software Innovations, GmbH.
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

#ifndef ROSBAG2_STORAGE__STORAGE_INTERFACES__BASE_INFO_INTERFACE_HPP_
#define ROSBAG2_STORAGE__STORAGE_INTERFACES__BASE_INFO_INTERFACE_HPP_

#include <string>

#include "rosbag2_storage/bag_metadata.hpp"
#include "rosbag2_storage/visibility_control.hpp"

namespace rosbag2_storage
{
namespace storage_interfaces
{

class ROSBAG2_STORAGE_PUBLIC BaseInfoInterface
{
public:
  virtual ~BaseInfoInterface() = default;

  virtual BagMetadata get_metadata() = 0;

  /**
   * Retrieves the relative path to the backing of the storage plugin.
   *
   * \returns the relative path.
   */
  virtual std::string get_relative_file_path() const = 0;

  /**
   * Returns the size of the bagfile.
   * \returns the size of the bagfile in bytes.
   */
  virtual uint64_t get_bagfile_size() const = 0;

  /**
   * Returns the identifier for the storage plugin.
   * \returns the identifier.
   */
  virtual std::string get_storage_identifier() const = 0;
};

}  // namespace storage_interfaces
}  // namespace rosbag2_storage

#endif  // ROSBAG2_STORAGE__STORAGE_INTERFACES__BASE_INFO_INTERFACE_HPP_
