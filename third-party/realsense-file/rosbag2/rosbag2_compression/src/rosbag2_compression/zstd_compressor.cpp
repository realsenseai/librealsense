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

#include <chrono>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "rcpputils/filesystem_helper.hpp"

#include "rosbag2_compression/zstd_compressor.hpp"

#include "logging.hpp"

namespace
{
// Increasing the compression level will:
//   - Increase the time taken to compress
//   - Decrease the size of the compressed data
// Setting to zero uses Zstd's default value of 3.
constexpr const int kDefaultZstdCompressionLevel = 1;

// String constant used to identify ZstdCompressor.
constexpr const char kCompressionIdentifier[] = "zstd";
// Used as a parameter type in a function that accepts the output of ZSTD_compress.
using ZstdCompressReturnType = decltype(ZSTD_compress(nullptr, 0, nullptr, 0, 0));

/**
 * Open a file using the OS-specific C API.
 * \param uri is the path to the file.
 * \param read_mode is the read mode accepted by OS-specific fopen.
 * \return the FILE pointer or nullptr if the file was not opened.
 */
FILE * open_file(const std::string & uri, const std::string & read_mode)
{
  FILE * fp{nullptr};
#ifdef _WIN32
  fopen_s(&fp, uri.c_str(), read_mode.c_str());
#else
  fp = std::fopen(uri.c_str(), read_mode.c_str());
#endif
  return fp;
}

/**
 * Read a file from the supplied uri into a vector.
 * \param uri is the path to the file.
 * \return the contents of the buffer as a vector.
 */
std::vector<uint8_t> get_input_buffer(const std::string & uri)
{
  // Read in buffer, handling accordingly
  const auto file_pointer = open_file(uri.c_str(), "rb");
  if (file_pointer == nullptr) {
    std::stringstream errmsg;
    errmsg << "Error opening file: \"" << uri <<
      "\" for binary reading! errno(" << errno << ")";

    throw std::runtime_error{errmsg.str()};
  }

  const auto file_path = rcpputils::fs::path{uri};
  const auto decompressed_buffer_length = file_path.exists() ? file_path.file_size() : 0u;

  if (decompressed_buffer_length == 0) {
    fclose(file_pointer);

    std::stringstream errmsg;
    errmsg << "Unable to get size of file: \"" << uri << "\"";

    throw std::runtime_error{errmsg.str()};
  }

  // Allocate and read in
  std::vector<uint8_t> decompressed_buffer(decompressed_buffer_length);

  const auto read_count = fread(
    decompressed_buffer.data(), sizeof(uint8_t), decompressed_buffer.size(), file_pointer);

  if (read_count != decompressed_buffer_length) {
    ROSBAG2_COMPRESSION_LOG_ERROR_STREAM(
      "Bytes read (" << read_count <<
        ") != decompressed_buffer_length (" << decompressed_buffer.size() << ")!");
    // An error indicator is set by this, so the following check will throw
  }

  if (ferror(file_pointer)) {
    fclose(file_pointer);

    std::stringstream errmsg;
    errmsg << "Unable to read binary data from file: \"" << uri << "\"!";

    throw std::runtime_error{errmsg.str()};
  }
  fclose(file_pointer);
  return decompressed_buffer;
}

/**
 * Writes the output buffer to the specified file path.
 * \param output_buffer is the data to write.
 * \param uri is the relative file path to the output storage.
 */
void write_output_buffer(
  const std::vector<uint8_t> & output_buffer,
  const std::string & uri)
{
  if (output_buffer.empty()) {
    std::stringstream errmsg;
    errmsg << "Cannot write empty buffer to file: \"" << uri << "\"";

    throw std::runtime_error{errmsg.str()};
  }

  const auto file_pointer = open_file(uri.c_str(), "wb");
  const auto write_count = fwrite(
    output_buffer.data(),
    sizeof(uint8_t),
    output_buffer.size(),
    file_pointer);

  if (write_count != output_buffer.size()) {
    ROSBAG2_COMPRESSION_LOG_ERROR_STREAM(
      "Bytes written (" << write_count <<
        ") != output_buffer size (" << output_buffer.size() << ")!");
    // An error indicator is set by fwrite, so the following check will throw.
  }

  if (ferror(file_pointer)) {
    fclose(file_pointer);

    std::stringstream errmsg;
    errmsg << "Unable to write compressed data to file: \"" << uri << "\"!";

    throw std::runtime_error{errmsg.str()};
  }
  fclose(file_pointer);
}

/**
 * Checks compression_result and throws a runtime_error if there was a ZSTD error.
 * \param compression_result is the return value of ZSTD_compress.
 */
void throw_on_zstd_error(const ZstdCompressReturnType compression_result)
{
  if (ZSTD_isError(compression_result)) {
    std::stringstream error;
    error << "ZSTD compression error: " << ZSTD_getErrorName(compression_result);
    throw std::runtime_error{error.str()};
  }
}

/**
 * Prints compression statistics to the debug log stream.
 * The log statement is formatted in JSON.
 * Time is formatted as a decimal of seconds.
 *
 * Example:
 *   "Compression statistics: {"Time" : 1.2, "Compression Ratio" : 0.5}
 */
void print_compression_statistics(
  std::chrono::high_resolution_clock::time_point start,
  std::chrono::high_resolution_clock::time_point end,
  size_t decompressed_size, size_t compressed_size)
{
  const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
  const auto compression_ratio =
    static_cast<double>(decompressed_size) / static_cast<double>(compressed_size);
  ROSBAG2_COMPRESSION_LOG_DEBUG_STREAM(
    "\"Compression statistics\" : {" <<
      "\"Time\" : " << (duration.count() / 1000.0) <<
      ", \"Compression Ratio\" : " << compression_ratio <<
      "}"
  );
}
}  // namespace

namespace rosbag2_compression
{

std::string ZstdCompressor::compress_uri(const std::string & uri)
{
  const auto start = std::chrono::high_resolution_clock::now();
  const auto compressed_uri = uri + "." + get_compression_identifier();
  const auto decompressed_buffer = get_input_buffer(uri);

  // Allocate based on compression bound and compress
  const auto compressed_buffer_length = ZSTD_compressBound(decompressed_buffer.size());
  std::vector<uint8_t> compressed_buffer(compressed_buffer_length);

  // Perform compression and check.
  // compression_result is either the actual compressed size or an error code.
  const auto compression_result = ZSTD_compress(
    compressed_buffer.data(), compressed_buffer.size(),
    decompressed_buffer.data(), decompressed_buffer.size(), kDefaultZstdCompressionLevel);
  throw_on_zstd_error(compression_result);

  // Compression_buffer_length might be larger than the actual compression size
  // Resize compressed_buffer so its size is the actual compression size.
  compressed_buffer.resize(compression_result);
  write_output_buffer(compressed_buffer, compressed_uri);

  const auto end = std::chrono::high_resolution_clock::now();
  print_compression_statistics(start, end, decompressed_buffer.size(), compression_result);
  return compressed_uri;
}

void ZstdCompressor::compress_serialized_bag_message(
  rosbag2_storage::SerializedBagMessage *)
{
  throw std::logic_error{"Not implemented"};
}

std::string ZstdCompressor::get_compression_identifier() const
{
  return kCompressionIdentifier;
}

}  // namespace rosbag2_compression
