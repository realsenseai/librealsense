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

#include "rosbag2_cpp/converter.hpp"

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "rosbag2_cpp/info.hpp"
#include "rosbag2_cpp/typesupport_helpers.hpp"
#include "rosbag2_cpp/storage_options.hpp"

#include "rosbag2_storage/metadata_io.hpp"
#include "rosbag2_storage/ros_helper.hpp"

namespace rosbag2_cpp
{

Converter::Converter(
  const std::string & input_format,
  const std::string & output_format,
  std::shared_ptr<SerializationFormatConverterFactoryInterface> converter_factory)
: Converter({input_format, output_format}, converter_factory)
{}

Converter::Converter(
  const ConverterOptions & converter_options,
  std::shared_ptr<SerializationFormatConverterFactoryInterface> converter_factory)
: converter_factory_(converter_factory),
  input_converter_(converter_factory_->load_deserializer(
      converter_options.input_serialization_format)),
  output_converter_(converter_factory_->load_serializer(
      converter_options.output_serialization_format))
{
  if (!input_converter_) {
    throw std::runtime_error(
            "Could not find converter for format " + converter_options.input_serialization_format);
  }
  if (!output_converter_) {
    throw std::runtime_error(
            "Could not find converter for format " + converter_options.output_serialization_format);
  }
}

Converter::~Converter()
{
  input_converter_.reset();
  output_converter_.reset();
  converter_factory_.reset();  // needs to be destroyed only after the converters
}

std::shared_ptr<rosbag2_storage::SerializedBagMessage> Converter::convert(
  std::shared_ptr<const rosbag2_storage::SerializedBagMessage> message)
{
  auto ts = topics_and_types_.at(message->topic_name).rmw_type_support;
  auto introspection_ts = topics_and_types_.at(message->topic_name).introspection_type_support;
  auto allocator = rcutils_get_default_allocator();
  std::shared_ptr<rosbag2_introspection_message_t> allocated_ros_message =
    allocate_introspection_message(introspection_ts, &allocator);

  input_converter_->deserialize(message, ts, allocated_ros_message);
  auto output_message = std::make_shared<rosbag2_storage::SerializedBagMessage>();
  output_message->serialized_data = rosbag2_storage::make_empty_serialized_message(0);
  output_converter_->serialize(allocated_ros_message, ts, output_message);
  return output_message;
}

void Converter::add_topic(const std::string & topic, const std::string & type)
{
  ConverterTypeSupport type_support;

  type_support.type_support_library = get_typesupport_library(
    type, "rosidl_typesupport_cpp");
  type_support.rmw_type_support = get_typesupport_handle(
    type, "rosidl_typesupport_cpp",
    type_support.type_support_library);

  type_support.introspection_type_support_library = get_typesupport_library(
    type, "rosidl_typesupport_introspection_cpp");
  type_support.introspection_type_support = get_typesupport_handle(
    type, "rosidl_typesupport_introspection_cpp",
    type_support.introspection_type_support_library);

  topics_and_types_.insert({topic, type_support});
}

}  // namespace rosbag2_cpp
