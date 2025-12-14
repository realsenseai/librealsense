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

#ifndef ROSBAG2_STORAGE__STORAGE_FACTORY_HPP_
#define ROSBAG2_STORAGE__STORAGE_FACTORY_HPP_

#include <memory>
#include <string>

#include "rosbag2_storage/storage_factory_interface.hpp"
#include "rosbag2_storage/visibility_control.hpp"

// This is necessary because of using stl types here. It is completely safe, because
// a) the member is not accessible from the outside
// b) there are no inline functions.
#ifdef _WIN32
# pragma warning(push)
# pragma warning(disable:4251)
#endif

namespace rosbag2_storage
{

namespace storage_interfaces
{
class ReadOnlyInterface;
class ReadWriteInterface;
}  // namespace storage_interfaces

class StorageFactoryImpl;

/// Factory to create instances of various storage interfaces
class ROSBAG2_STORAGE_PUBLIC StorageFactory : public StorageFactoryInterface
{
public:
  StorageFactory();
  ~StorageFactory() override;

  std::shared_ptr<storage_interfaces::ReadOnlyInterface>
  open_read_only(const std::string & uri, const std::string & storage_id) override;

  std::shared_ptr<storage_interfaces::ReadWriteInterface>
  open_read_write(const std::string & uri, const std::string & storage_id) override;

private:
  std::unique_ptr<StorageFactoryImpl> impl_;
};

}  // namespace rosbag2_storage

#ifdef _WIN32
# pragma warning(pop)
#endif

#endif  // ROSBAG2_STORAGE__STORAGE_FACTORY_HPP_
