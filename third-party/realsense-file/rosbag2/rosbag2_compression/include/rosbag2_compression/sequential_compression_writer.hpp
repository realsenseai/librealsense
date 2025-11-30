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

#ifndef ROSBAG2_COMPRESSION__SEQUENTIAL_COMPRESSION_WRITER_HPP_
#define ROSBAG2_COMPRESSION__SEQUENTIAL_COMPRESSION_WRITER_HPP_

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "rosbag2_cpp/converter.hpp"
#include "rosbag2_cpp/converter_options.hpp"
#include "rosbag2_cpp/serialization_format_converter_factory.hpp"
#include "rosbag2_cpp/serialization_format_converter_factory_interface.hpp"
#include "rosbag2_cpp/storage_options.hpp"
#include "rosbag2_cpp/writer_interfaces/base_writer_interface.hpp"

#include "rosbag2_storage/metadata_io.hpp"
#include "rosbag2_storage/storage_factory.hpp"
#include "rosbag2_storage/storage_factory_interface.hpp"
#include "rosbag2_storage/storage_interfaces/base_io_interface.hpp"

#include "rosbag2_compression/compression_options.hpp"

#include "base_compressor_interface.hpp"
#include "compression_factory.hpp"
#include "compression_options.hpp"
#include "visibility_control.hpp"

#ifdef _WIN32
# pragma warning(push)
# pragma warning(disable:4251)
#endif

namespace rosbag2_compression
{

class ROSBAG2_COMPRESSION_PUBLIC SequentialCompressionWriter
  : public rosbag2_cpp::writer_interfaces::BaseWriterInterface
{
public:
  explicit SequentialCompressionWriter(
    const rosbag2_compression::CompressionOptions & compression_options =
    rosbag2_compression::CompressionOptions());

  SequentialCompressionWriter(
    const rosbag2_compression::CompressionOptions & compression_options,
    std::unique_ptr<rosbag2_compression::CompressionFactory> compression_factory,
    std::unique_ptr<rosbag2_storage::StorageFactoryInterface> storage_factory,
    std::shared_ptr<rosbag2_cpp::SerializationFormatConverterFactoryInterface> converter_factory,
    std::unique_ptr<rosbag2_storage::MetadataIo> metadata_io);

  ~SequentialCompressionWriter() override;

  /**
   * Opens a new bagfile and prepare it for writing messages. The bagfile must not exist.
   * This must be called before any other function is used.
   *
   * \param storage_options Options to configure the storage
   * \param converter_options options to define in which format incoming messages are stored
   **/
  void open(
    const rosbag2_cpp::StorageOptions & storage_options,
    const rosbag2_cpp::ConverterOptions & converter_options) override;

  /**
   * Attempt to compress the last open file and reset the storage and storage factory.
   * This method must be exception safe because it is called by the destructor.
   */
  void reset() override;

  /**
   * Create a new topic in the underlying storage. Needs to be called for every topic used within
   * a message which is passed to write(...).
   *
   * \param topic_with_type name and type identifier of topic to be created
   * \throws runtime_error if the Writer is not open.
   */
  void create_topic(const rosbag2_storage::TopicMetadata & topic_with_type) override;

  /**
   * Remove a new topic in the underlying storage.
   * If creation of subscription fails remove the topic
   * from the db (more of cleanup)
   *
   * \param topic_with_type name and type identifier of topic to be created
   * \throws runtime_error if the Writer is not open.
   */
  void remove_topic(const rosbag2_storage::TopicMetadata & topic_with_type) override;

  /**
   * Write a message to a bagfile. The topic needs to have been created before writing is possible.
   *
   * \param message to be written to the bagfile
   * \throws runtime_error if the Writer is not open.
   */
  void write(std::shared_ptr<rosbag2_storage::SerializedBagMessage> message) override;

protected:
  /**
   * Compress the most recent file and update the metadata file path.
   */
  virtual void compress_last_file();

  /**
   * Checks if the compression by message option is specified and a compressor exists.
   *
   * If the above conditions are satisfied, compresses the serialized bag message.
   *
   * \param message The message to compress.
   * \return True if compression occurred, false otherwise.
   */
  virtual void compress_message(std::shared_ptr<rosbag2_storage::SerializedBagMessage> message);

  /**
   * Initializes the compressor if a compression mode is specified.
   *
   * \throws std::invalid_argument if compression_options isn't supported.
   */
  virtual void setup_compression();

private:
  std::string base_folder_;
  std::unique_ptr<rosbag2_storage::StorageFactoryInterface> storage_factory_{};
  std::shared_ptr<rosbag2_cpp::SerializationFormatConverterFactoryInterface> converter_factory_{};
  std::shared_ptr<rosbag2_storage::storage_interfaces::ReadWriteInterface> storage_{};
  std::unique_ptr<rosbag2_storage::MetadataIo> metadata_io_{};
  std::unique_ptr<rosbag2_cpp::Converter> converter_{};
  std::unique_ptr<rosbag2_compression::BaseCompressorInterface> compressor_{};
  std::unique_ptr<rosbag2_compression::CompressionFactory> compression_factory_{};

  // Used in bagfile splitting; specifies the best-effort maximum sub-section of a bagfile in bytes.
  uint64_t max_bagfile_size_{rosbag2_storage::storage_interfaces::MAX_BAGFILE_SIZE_NO_SPLIT};

  // Used to track topic -> message count
  std::unordered_map<std::string, rosbag2_storage::TopicInformation> topics_names_to_info_{};

  rosbag2_storage::BagMetadata metadata_{};

  rosbag2_compression::CompressionOptions compression_options_{};

  bool should_compress_last_file_{true};

  // Closes the current backed storage and opens the next bagfile.
  void split_bagfile();

  // Checks if the current recording bagfile needs to be split and rolled over to a new file.
  bool should_split_bagfile() const;

  // Prepares the metadata by setting initial values.
  void init_metadata();

  // Record TopicInformation into metadata
  void finalize_metadata();
};
}  // namespace rosbag2_compression
#endif  // ROSBAG2_COMPRESSION__SEQUENTIAL_COMPRESSION_WRITER_HPP_
