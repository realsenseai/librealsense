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

#ifndef ROSBAG2_CPP__WRITERS__SEQUENTIAL_WRITER_HPP_
#define ROSBAG2_CPP__WRITERS__SEQUENTIAL_WRITER_HPP_

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "rosbag2_cpp/converter.hpp"
#include "rosbag2_cpp/serialization_format_converter_factory.hpp"
#include "rosbag2_cpp/storage_options.hpp"
#include "rosbag2_cpp/writer_interfaces/base_writer_interface.hpp"
#include "rosbag2_cpp/visibility_control.hpp"

#include "rosbag2_storage/metadata_io.hpp"
#include "rosbag2_storage/storage_factory.hpp"
#include "rosbag2_storage/storage_factory_interface.hpp"
#include "rosbag2_storage/storage_interfaces/read_write_interface.hpp"

// This is necessary because of using stl types here. It is completely safe, because
// a) the member is not accessible from the outside
// b) there are no inline functions.
#ifdef _WIN32
# pragma warning(push)
# pragma warning(disable:4251)
#endif

namespace rosbag2_cpp
{
namespace writers
{

/**
 * The Writer allows writing messages to a new bag. For every topic, information about its type
 * needs to be added before writing the first message.
 */
class ROSBAG2_CPP_PUBLIC SequentialWriter
  : public rosbag2_cpp::writer_interfaces::BaseWriterInterface
{
public:
  explicit
  SequentialWriter(
    std::unique_ptr<rosbag2_storage::StorageFactoryInterface> storage_factory =
    std::make_unique<rosbag2_storage::StorageFactory>(),
    std::shared_ptr<SerializationFormatConverterFactoryInterface> converter_factory =
    std::make_shared<SerializationFormatConverterFactory>(),
    std::unique_ptr<rosbag2_storage::MetadataIo> metadata_io =
    std::make_unique<rosbag2_storage::MetadataIo>());

  ~SequentialWriter() override;

  /**
   * Opens a new bagfile and prepare it for writing messages. The bagfile must not exist.
   * This must be called before any other function is used.
   *
   * \param storage_options Options to configure the storage
   * \param converter_options options to define in which format incoming messages are stored
   **/
  void open(
    const StorageOptions & storage_options, const ConverterOptions & converter_options) override;

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

private:
  std::string base_folder_;
  std::unique_ptr<rosbag2_storage::StorageFactoryInterface> storage_factory_;
  std::shared_ptr<SerializationFormatConverterFactoryInterface> converter_factory_;
  std::shared_ptr<rosbag2_storage::storage_interfaces::ReadWriteInterface> storage_;
  std::unique_ptr<rosbag2_storage::MetadataIo> metadata_io_;
  std::unique_ptr<Converter> converter_;

  // Used in bagfile splitting; specifies the best-effort maximum sub-section of a bagfile in bytes.
  uint64_t max_bagfile_size_;

  // Intermediate cache to write multiple messages into the storage.
  // `max_cache_size` is the amount of messages to hold in storage before writing to disk.
  uint64_t max_cache_size_;
  std::vector<std::shared_ptr<const rosbag2_storage::SerializedBagMessage>> cache_;

  // Used to track topic -> message count
  std::unordered_map<std::string, rosbag2_storage::TopicInformation> topics_names_to_info_;

  rosbag2_storage::BagMetadata metadata_;

  // Closes the current backed storage and opens the next bagfile.
  void split_bagfile();

  // Checks if the current recording bagfile needs to be split and rolled over to a new file.
  bool should_split_bagfile() const;

  // Prepares the metadata by setting initial values.
  void init_metadata();

  // Record TopicInformation into metadata
  void finalize_metadata();
};

}  // namespace writers
}  // namespace rosbag2_cpp

#ifdef _WIN32
# pragma warning(pop)
#endif

#endif  // ROSBAG2_CPP__WRITERS__SEQUENTIAL_WRITER_HPP_
