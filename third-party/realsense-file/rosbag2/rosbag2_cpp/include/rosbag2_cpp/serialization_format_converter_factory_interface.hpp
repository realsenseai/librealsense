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

#ifndef ROSBAG2_CPP__SERIALIZATION_FORMAT_CONVERTER_FACTORY_INTERFACE_HPP_
#define ROSBAG2_CPP__SERIALIZATION_FORMAT_CONVERTER_FACTORY_INTERFACE_HPP_

#include <memory>
#include <string>

#include "rosbag2_cpp/converter_interfaces/serialization_format_converter.hpp"
#include "rosbag2_cpp/visibility_control.hpp"

namespace rosbag2_cpp
{

class ROSBAG2_CPP_PUBLIC SerializationFormatConverterFactoryInterface
{
public:
  virtual ~SerializationFormatConverterFactoryInterface() = default;

  virtual std::unique_ptr<converter_interfaces::SerializationFormatDeserializer>
  load_deserializer(const std::string & format) = 0;

  virtual std::unique_ptr<converter_interfaces::SerializationFormatSerializer>
  load_serializer(const std::string & format) = 0;
};

}  // namespace rosbag2_cpp

#endif  // ROSBAG2_CPP__SERIALIZATION_FORMAT_CONVERTER_FACTORY_INTERFACE_HPP_
