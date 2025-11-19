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

#ifndef ROSBAG2_COMPRESSION__BASE_DECOMPRESSOR_INTERFACE_HPP_
#define ROSBAG2_COMPRESSION__BASE_DECOMPRESSOR_INTERFACE_HPP_

#include <memory>
#include <string>

#include "rosbag2_storage/serialized_bag_message.hpp"

#include "visibility_control.hpp"

namespace rosbag2_compression
{

/**
 * An interface for developers adding a new decompression algorithm to rosbag2.
 * These functions must be implemented so that a reader can properly decompress a file or bag
 * message.
 * A corresponding compressor with an identical compression format must also be implemented.
 *
 * Example file decompression usage:
 *
 * MyDecompressor my_decompressor();
 * std::string current_uri = get_current_file();
 * std::string compressed_path_uri = my_decompressor.decompress_uri(current_uri);
 * storage = storage_factory.open_read_only(compressed_path_uri, storage_options.storage_id);
 *
 * Example message decompression usage:
 *
 * MyDecompressor my_decompressor();
 * std::shared_ptr<SerializedBagMessage> bag_message = storage.read_next();
 * std::shared_ptr<SerializedBagMessage> decompressed_message =
 *   my_decompressor.decompress_serialized_bag_message(bag_message.get());
 */
class ROSBAG2_COMPRESSION_PUBLIC BaseDecompressorInterface
{
public:
  virtual ~BaseDecompressorInterface() = default;

  /**
   * Decompress a file on disk.
   *
   * \param uri Input file to decompress with file extension.
   * \return The relative path to the decompressed file without the compressed extension.
   */
  virtual std::string decompress_uri(const std::string & uri) = 0;

  /**
   * Decompress the serialized_data of a serialized bag message in place.
   *
   * \param[in,out] bag_message A serialized bag message.
   */
  virtual void decompress_serialized_bag_message(
    rosbag2_storage::SerializedBagMessage * bag_message) = 0;

  /**
   * Get the identifier of the compression algorithm. This is appended to the extension of the
   * compressed file.
   */
  virtual std::string get_decompression_identifier() const = 0;
};

}  // namespace rosbag2_compression

#endif  // ROSBAG2_COMPRESSION__BASE_DECOMPRESSOR_INTERFACE_HPP_
