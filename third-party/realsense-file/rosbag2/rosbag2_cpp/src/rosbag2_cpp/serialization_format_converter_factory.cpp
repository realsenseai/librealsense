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

#include "rosbag2_cpp/serialization_format_converter_factory.hpp"

#include <memory>
#include <string>
#include <vector>

#include "./serialization_format_converter_factory_impl.hpp"

namespace rosbag2_cpp
{

SerializationFormatConverterFactory::SerializationFormatConverterFactory()
: impl_(std::make_unique<SerializationFormatConverterFactoryImpl>())
{}

SerializationFormatConverterFactory::~SerializationFormatConverterFactory() = default;

std::unique_ptr<converter_interfaces::SerializationFormatDeserializer>
SerializationFormatConverterFactory::load_deserializer(const std::string & format)
{
  return impl_->load_deserializer(format);
}

std::unique_ptr<converter_interfaces::SerializationFormatSerializer>
SerializationFormatConverterFactory::load_serializer(const std::string & format)
{
  return impl_->load_serializer(format);
}

}  // namespace rosbag2_cpp
