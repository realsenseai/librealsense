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

#include "rosbag2_cpp/writers/sequential_writer.hpp"

#include <algorithm>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include "rcpputils/filesystem_helper.hpp"

#include "rosbag2_cpp/info.hpp"
#include "rosbag2_cpp/storage_options.hpp"

namespace rosbag2_cpp
{
namespace writers
{

namespace
{
std::string format_storage_uri(const std::string & base_folder, uint64_t storage_count)
{
  // Right now `base_folder_` is always just the folder name for where to install the bagfile.
  // The name of the folder needs to be queried in case
  // SequentialWriter is opened with a relative path.
  std::stringstream storage_file_name;
  storage_file_name << rcpputils::fs::path(base_folder).filename().string() << "_" << storage_count;

  return (rcpputils::fs::path(base_folder) / storage_file_name.str()).string();
}

std::string strip_parent_path(const std::string & relative_path)
{
  return rcpputils::fs::path(relative_path).filename().string();
}

}  // namespace

SequentialWriter::SequentialWriter(
  std::unique_ptr<rosbag2_storage::StorageFactoryInterface> storage_factory,
  std::shared_ptr<SerializationFormatConverterFactoryInterface> converter_factory,
  std::unique_ptr<rosbag2_storage::MetadataIo> metadata_io)
: storage_factory_(std::move(storage_factory)),
  converter_factory_(std::move(converter_factory)),
  storage_(nullptr),
  metadata_io_(std::move(metadata_io)),
  converter_(nullptr),
  max_bagfile_size_(rosbag2_storage::storage_interfaces::MAX_BAGFILE_SIZE_NO_SPLIT),
  topics_names_to_info_(),
  metadata_()
{}

SequentialWriter::~SequentialWriter()
{
  reset();
}

void SequentialWriter::init_metadata()
{
  metadata_ = rosbag2_storage::BagMetadata{};
  metadata_.storage_identifier = storage_->get_storage_identifier();
  metadata_.starting_time = std::chrono::time_point<std::chrono::high_resolution_clock>(
    std::chrono::nanoseconds::max());
  metadata_.relative_file_paths = {strip_parent_path(storage_->get_relative_file_path())};
}

void SequentialWriter::open(
  const StorageOptions & storage_options,
  const ConverterOptions & converter_options)
{
  base_folder_ = storage_options.uri;
  max_bagfile_size_ = storage_options.max_bagfile_size;
  max_cache_size_ = storage_options.max_cache_size;

  cache_.reserve(max_cache_size_);

  if (converter_options.output_serialization_format !=
    converter_options.input_serialization_format)
  {
    converter_ = std::make_unique<Converter>(converter_options, converter_factory_);
  }

  rcpputils::fs::path db_path(base_folder_);
  if (db_path.is_directory()) {
    std::stringstream error;
    error << "Database directory already exists (" << db_path.string() <<
      "), can't overwrite existing database";
    throw std::runtime_error{error.str()};
  }

  bool dir_created = rcpputils::fs::create_directories(db_path);
  if (!dir_created) {
    std::stringstream error;
    error << "Failed to create database directory (" << db_path.string() << ").";
    throw std::runtime_error{error.str()};
  }

  const auto storage_uri = format_storage_uri(base_folder_, 0);

  storage_ = storage_factory_->open_read_write(storage_uri, storage_options.storage_id);
  if (!storage_) {
    throw std::runtime_error("No storage could be initialized. Abort");
  }

  if (max_bagfile_size_ != 0 &&
    max_bagfile_size_ < storage_->get_minimum_split_file_size())
  {
    std::stringstream error;
    error << "Invalid bag splitting size given. Please provide a value greater than " <<
      storage_->get_minimum_split_file_size() << ". Specified value of " <<
      storage_options.max_bagfile_size;
    throw std::runtime_error{error.str()};
  }

  init_metadata();
}

void SequentialWriter::reset()
{
  if (!base_folder_.empty()) {
    finalize_metadata();
    metadata_io_->write_metadata(base_folder_, metadata_);
  }

  storage_.reset();  // Necessary to ensure that the storage is destroyed before the factory
  storage_factory_.reset();
}

void SequentialWriter::create_topic(const rosbag2_storage::TopicMetadata & topic_with_type)
{
  if (!storage_) {
    throw std::runtime_error("Bag is not open. Call open() before writing.");
  }

  if (converter_) {
    converter_->add_topic(topic_with_type.name, topic_with_type.type);
  }

  if (topics_names_to_info_.find(topic_with_type.name) ==
    topics_names_to_info_.end())
  {
    rosbag2_storage::TopicInformation info{};
    info.topic_metadata = topic_with_type;

    const auto insert_res = topics_names_to_info_.insert(
      std::make_pair(topic_with_type.name, info));

    if (!insert_res.second) {
      std::stringstream errmsg;
      errmsg << "Failed to insert topic \"" << topic_with_type.name << "\"!";

      throw std::runtime_error(errmsg.str());
    }

    storage_->create_topic(topic_with_type);
  }
}

void SequentialWriter::remove_topic(const rosbag2_storage::TopicMetadata & topic_with_type)
{
  if (!storage_) {
    throw std::runtime_error("Bag is not open. Call open() before removing.");
  }

  if (topics_names_to_info_.erase(topic_with_type.name) > 0) {
    storage_->remove_topic(topic_with_type);
  } else {
    std::stringstream errmsg;
    errmsg << "Failed to remove the non-existing topic \"" <<
      topic_with_type.name << "\"!";

    throw std::runtime_error(errmsg.str());
  }
}

void SequentialWriter::split_bagfile()
{
  const auto storage_uri = format_storage_uri(
    base_folder_,
    metadata_.relative_file_paths.size());
  storage_ = storage_factory_->open_read_write(storage_uri, metadata_.storage_identifier);

  if (!storage_) {
    std::stringstream errmsg;
    errmsg << "Failed to rollover bagfile to new file: \"" << storage_uri << "\"!";

    throw std::runtime_error(errmsg.str());
  }

  metadata_.relative_file_paths.push_back(strip_parent_path(storage_->get_relative_file_path()));

  // Re-register all topics since we rolled-over to a new bagfile.
  for (const auto & topic : topics_names_to_info_) {
    storage_->create_topic(topic.second.topic_metadata);
  }
}

void SequentialWriter::write(std::shared_ptr<rosbag2_storage::SerializedBagMessage> message)
{
  if (!storage_) {
    throw std::runtime_error("Bag is not open. Call open() before writing.");
  }

  // Update the message count for the Topic.
  ++topics_names_to_info_.at(message->topic_name).message_count;

  if (should_split_bagfile()) {
    split_bagfile();
  }

  const auto message_timestamp = std::chrono::time_point<std::chrono::high_resolution_clock>(
    std::chrono::nanoseconds(message->time_stamp));
  metadata_.starting_time = std::min(metadata_.starting_time, message_timestamp);

  const auto duration = message_timestamp - metadata_.starting_time;
  metadata_.duration = std::max(metadata_.duration, duration);

  // if cache size is set to zero, we directly call write
  if (max_cache_size_ == 0u) {
    storage_->write(converter_ ? converter_->convert(message) : message);
  } else {
    cache_.push_back(converter_ ? converter_->convert(message) : message);
    if (cache_.size() >= max_cache_size_) {
      storage_->write(cache_);
      // reset cache
      cache_.clear();
      cache_.reserve(max_cache_size_);
    }
  }
}

bool SequentialWriter::should_split_bagfile() const
{
  if (max_bagfile_size_ == rosbag2_storage::storage_interfaces::MAX_BAGFILE_SIZE_NO_SPLIT) {
    return false;
  } else {
    return storage_->get_bagfile_size() > max_bagfile_size_;
  }
}

void SequentialWriter::finalize_metadata()
{
  metadata_.bag_size = 0;

  for (const auto & path : metadata_.relative_file_paths) {
    const auto bag_path = rcpputils::fs::path{path};

    if (bag_path.exists()) {
      metadata_.bag_size += bag_path.file_size();
    }
  }

  metadata_.topics_with_message_count.clear();
  metadata_.topics_with_message_count.reserve(topics_names_to_info_.size());
  metadata_.message_count = 0;

  for (const auto & topic : topics_names_to_info_) {
    metadata_.topics_with_message_count.push_back(topic.second);
    metadata_.message_count += topic.second.message_count;
  }
}

}  // namespace writers
}  // namespace rosbag2_cpp
