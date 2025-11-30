// Copyright 2018, Bosch Software Innovations GmbH.
// Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
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

#include "rosbag2_cpp/reader.hpp"

#include <memory>
#include <utility>
#include <vector>

#include "rosbag2_cpp/info.hpp"
#include "rosbag2_cpp/reader_interfaces/base_reader_interface.hpp"

namespace rosbag2_cpp
{

Reader::Reader(std::unique_ptr<reader_interfaces::BaseReaderInterface> reader_impl)
: reader_impl_(std::move(reader_impl))
{}

Reader::~Reader()
{
  reader_impl_->reset();
}

void Reader::open(
  const StorageOptions & storage_options,
  const ConverterOptions & converter_options)
{
  reader_impl_->open(storage_options, converter_options);
}

bool Reader::has_next()
{
  return reader_impl_->has_next();
}

std::shared_ptr<rosbag2_storage::SerializedBagMessage> Reader::read_next()
{
  return reader_impl_->read_next();
}

const rosbag2_storage::BagMetadata & Reader::get_metadata() const
{
  return reader_impl_->get_metadata();
}

std::vector<rosbag2_storage::TopicMetadata> Reader::get_all_topics_and_types() const
{
  return reader_impl_->get_all_topics_and_types();
}

void Reader::set_filter(const rosbag2_storage::StorageFilter & storage_filter)
{
  reader_impl_->set_filter(storage_filter);
}

void Reader::reset_filter()
{
  reader_impl_->reset_filter();
}

}  // namespace rosbag2_cpp
