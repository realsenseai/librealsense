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

#ifndef ROSBAG2_CPP__SERIALIZATION_FORMAT_CONVERTER_FACTORY_HPP_
#define ROSBAG2_CPP__SERIALIZATION_FORMAT_CONVERTER_FACTORY_HPP_

#include "rosbag2_cpp/serialization_format_converter_factory_interface.hpp"

#include <memory>
#include <string>
#include <vector>

#include "rosbag2_cpp/visibility_control.hpp"

// This is necessary because of using stl types here. It is completely safe, because
// a) the member is not accessible from the outside
// b) there are no inline functions.
#ifdef _WIN32
# pragma warning(push)
# pragma warning(disable:4251)
#endif

namespace rosbag2_cpp
{
class SerializationFormatConverterFactoryImpl;

class ROSBAG2_CPP_PUBLIC SerializationFormatConverterFactory
  : public SerializationFormatConverterFactoryInterface
{
public:
  SerializationFormatConverterFactory();

  ~SerializationFormatConverterFactory() override;

  std::unique_ptr<converter_interfaces::SerializationFormatDeserializer>
  load_deserializer(const std::string & format) override;

  std::unique_ptr<converter_interfaces::SerializationFormatSerializer>
  load_serializer(const std::string & format) override;

private:
  std::unique_ptr<SerializationFormatConverterFactoryImpl> impl_;
};

}  // namespace rosbag2_cpp

#ifdef _WIN32
# pragma warning(pop)
#endif

#endif  // ROSBAG2_CPP__SERIALIZATION_FORMAT_CONVERTER_FACTORY_HPP_
