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

#ifndef ROSBAG2_STORAGE__IMPL__STORAGE_FACTORY_IMPL_HPP_
#define ROSBAG2_STORAGE__IMPL__STORAGE_FACTORY_IMPL_HPP_

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "pluginlib/class_loader.hpp"

#include "rosbag2_storage/storage_interfaces/read_only_interface.hpp"
#include "rosbag2_storage/storage_interfaces/read_write_interface.hpp"

#include "rosbag2_storage/storage_factory.hpp"
#include "rosbag2_storage/storage_traits.hpp"
#include "rosbag2_storage/logging.hpp"

namespace rosbag2_storage
{

using storage_interfaces::ReadOnlyInterface;
using storage_interfaces::ReadWriteInterface;

template<typename InterfaceT>
std::shared_ptr<pluginlib::ClassLoader<InterfaceT>>
get_class_loader()
{
  const char * lookup_name = StorageTraits<InterfaceT>::name;
  return std::make_shared<pluginlib::ClassLoader<InterfaceT>>("rosbag2_storage", lookup_name);
}

template<
  typename InterfaceT,
  storage_interfaces::IOFlag flag = StorageTraits<InterfaceT>::io_flag
>
std::shared_ptr<InterfaceT>
get_interface_instance(
  std::shared_ptr<pluginlib::ClassLoader<InterfaceT>> class_loader,
  const std::string & storage_id,
  const std::string & uri)
{
  const auto & registered_classes = class_loader->getDeclaredClasses();
  auto class_exists = std::find(registered_classes.begin(), registered_classes.end(), storage_id);
  if (class_exists == registered_classes.end()) {
    ROSBAG2_STORAGE_LOG_DEBUG_STREAM("Requested storage id '" << storage_id << "' does not exist");
    return nullptr;
  }

  std::shared_ptr<InterfaceT> instance = nullptr;
  try {
    auto unmanaged_instance = class_loader->createUnmanagedInstance(storage_id);
    instance = std::shared_ptr<InterfaceT>(unmanaged_instance);
  } catch (const std::runtime_error & ex) {
    ROSBAG2_STORAGE_LOG_ERROR_STREAM(
      "Unable to load instance of read write interface: " << ex.what());
    return nullptr;
  }

  try {
    instance->open(uri, flag);
    return instance;
  } catch (const std::runtime_error & ex) {
    ROSBAG2_STORAGE_LOG_ERROR_STREAM(
      "Could not open '" << uri << "' with '" << storage_id << "'. Error: " << ex.what());
    return nullptr;
  }
}

class StorageFactoryImpl
{
public:
  StorageFactoryImpl()
  {
    try {
      read_write_class_loader_ = get_class_loader<ReadWriteInterface>();
    } catch (const std::exception & e) {
      ROSBAG2_STORAGE_LOG_ERROR_STREAM("Unable to create class load instance: " << e.what());
      throw e;
    }

    try {
      read_only_class_loader_ = get_class_loader<ReadOnlyInterface>();
    } catch (const std::exception & e) {
      ROSBAG2_STORAGE_LOG_ERROR_STREAM("Unable to create class load instance: " << e.what());
      throw e;
    }
  }

  virtual ~StorageFactoryImpl() = default;

  std::shared_ptr<ReadWriteInterface> open_read_write(
    const std::string & uri, const std::string & storage_id)
  {
    auto instance = get_interface_instance(read_write_class_loader_, storage_id, uri);

    if (instance == nullptr) {
      ROSBAG2_STORAGE_LOG_ERROR_STREAM(
        "Could not load/open plugin with storage id '" << storage_id << "'.");
    }

    return instance;
  }

  std::shared_ptr<ReadOnlyInterface> open_read_only(
    const std::string & uri, const std::string & storage_id)
  {
    // try to load the instance as read_only interface
    auto instance = get_interface_instance(read_only_class_loader_, storage_id, uri);
    // try to load as read_write if not successful
    if (instance == nullptr) {
      instance = get_interface_instance<ReadWriteInterface, storage_interfaces::IOFlag::READ_ONLY>(
        read_write_class_loader_, storage_id, uri);
    }

    if (instance == nullptr) {
      ROSBAG2_STORAGE_LOG_ERROR_STREAM(
        "Could not load/open plugin with storage id '" << storage_id << "'.");
    }

    return instance;
  }

private:
  std::shared_ptr<pluginlib::ClassLoader<ReadWriteInterface>> read_write_class_loader_;
  std::shared_ptr<pluginlib::ClassLoader<ReadOnlyInterface>> read_only_class_loader_;
};

}  // namespace rosbag2_storage

#endif  // ROSBAG2_STORAGE__IMPL__STORAGE_FACTORY_IMPL_HPP_
