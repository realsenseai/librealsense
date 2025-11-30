// Copyright 2018, Bosch Software Innovations GmbH.
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

#ifndef ROSBAG2_CPP__CONVERTER_INTERFACES__SERIALIZATION_FORMAT_CONVERTER_HPP_
#define ROSBAG2_CPP__CONVERTER_INTERFACES__SERIALIZATION_FORMAT_CONVERTER_HPP_

#include <memory>

#include "rosbag2_cpp/converter_interfaces/serialization_format_serializer.hpp"
#include "rosbag2_cpp/converter_interfaces/serialization_format_deserializer.hpp"

/**
 * This is a convenience class for plugin developers.
 * When developing a plugin to both write and
 * read a specified serialization format, inherit from this class
 */
namespace rosbag2_cpp
{
namespace converter_interfaces
{

class SerializationFormatConverter
  : public SerializationFormatSerializer, public SerializationFormatDeserializer
{};

}  // namespace converter_interfaces
}  // namespace rosbag2_cpp

#endif  // ROSBAG2_CPP__CONVERTER_INTERFACES__SERIALIZATION_FORMAT_CONVERTER_HPP_
