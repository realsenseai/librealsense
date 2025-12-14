// Copyright 2020 Amazon.com, Inc. or its affiliates. All Rights Reserved.
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

#include "rosbag2_compression/sequential_compression_reader.hpp"

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "rcpputils/filesystem_helper.hpp"

#include "rosbag2_compression/compression_options.hpp"
#include "rosbag2_compression/zstd_decompressor.hpp"

#include "logging.hpp"

namespace rosbag2_compression
{

SequentialCompressionReader::SequentialCompressionReader(
  std::unique_ptr<rosbag2_compression::CompressionFactory> compression_factory,
  std::unique_ptr<rosbag2_storage::StorageFactoryInterface> storage_factory,
  std::shared_ptr<rosbag2_cpp::SerializationFormatConverterFactoryInterface> converter_factory,
  std::unique_ptr<rosbag2_storage::MetadataIo> metadata_io)
: SequentialReader(std::move(storage_factory), converter_factory, std::move(metadata_io)),
  compression_factory_{std::move(compression_factory)}
{}

SequentialCompressionReader::~SequentialCompressionReader()
{}

void SequentialCompressionReader::setup_decompression()
{
  compression_mode_ = rosbag2_compression::compression_mode_from_string(metadata_.compression_mode);
  if (compression_mode_ != rosbag2_compression::CompressionMode::NONE) {
    decompressor_ = compression_factory_->create_decompressor(metadata_.compression_format);
    // Decompress the first file so that it is readable.
    ROSBAG2_COMPRESSION_LOG_DEBUG_STREAM("Decompressing " << get_current_file().c_str());
    *current_file_iterator_ = decompressor_->decompress_uri(get_current_file());
  } else {
    throw std::invalid_argument{
            "SequentialCompressionReader requires a CompressionMode that is not NONE!"};
  }
}

void SequentialCompressionReader::open(
  const rosbag2_cpp::StorageOptions & storage_options,
  const rosbag2_cpp::ConverterOptions & converter_options)
{
  if (metadata_io_->metadata_file_exists(storage_options.uri)) {
    metadata_ = metadata_io_->read_metadata(storage_options.uri);
    if (metadata_.relative_file_paths.empty()) {
      ROSBAG2_COMPRESSION_LOG_WARN("No file paths were found in metadata.");
      return;
    }
    file_paths_ = metadata_.relative_file_paths;
    current_file_iterator_ = file_paths_.begin();
    setup_decompression();

    storage_ = storage_factory_->open_read_only(
      *current_file_iterator_, metadata_.storage_identifier);
    if (!storage_) {
      std::stringstream errmsg;
      errmsg << "No storage could be initialized for: \"" <<
        storage_options.uri << "\".";

      throw std::runtime_error{errmsg.str()};
    }
  } else {
    std::stringstream errmsg;
    errmsg << "Could not find metadata for bag: \"" << storage_options.uri <<
      "\". Legacy bag files are not supported if this is a ROS 1 bag file.";
    throw std::runtime_error{errmsg.str()};
  }
  const auto & topics = metadata_.topics_with_message_count;
  if (topics.empty()) {
    ROSBAG2_COMPRESSION_LOG_WARN("No topics were listed in metadata.");
    return;
  }
  fill_topics_metadata();

  // Currently a bag file can only be played if all topics have the same serialization format.
  check_topics_serialization_formats(topics);
  check_converter_serialization_format(
    converter_options.output_serialization_format,
    topics[0].topic_metadata.serialization_format);
}

std::shared_ptr<rosbag2_storage::SerializedBagMessage> SequentialCompressionReader::read_next()
{
  if (storage_ && decompressor_) {
    auto message = storage_->read_next();
    if (compression_mode_ == rosbag2_compression::CompressionMode::MESSAGE) {
      decompressor_->decompress_serialized_bag_message(message.get());
    }
    return converter_ ? converter_->convert(message) : message;
  }
  throw std::runtime_error{"Bag is not open. Call open() before reading."};
}


void SequentialCompressionReader::load_next_file()
{
  if (current_file_iterator_ == file_paths_.end()) {
    throw std::runtime_error{"Cannot load next file; already on last file!"};
  }

  if (compression_mode_ == rosbag2_compression::CompressionMode::NONE) {
    throw std::runtime_error{"Cannot use SequentialCompressionReader with NONE compression mode."};
  }

  ++current_file_iterator_;
  if (compression_mode_ == rosbag2_compression::CompressionMode::FILE) {
    if (decompressor_ == nullptr) {
      throw std::runtime_error{
              "The bag file was not properly opened. "
              "Somehow the compression mode was set without opening a decompressor."
      };
    }

    ROSBAG2_COMPRESSION_LOG_DEBUG_STREAM("Decompressing " << get_current_file().c_str());
    *current_file_iterator_ = decompressor_->decompress_uri(get_current_file());
  }
}
}  // namespace rosbag2_compression
