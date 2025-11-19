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
#include <sstream>
#include <string>
#include <vector>

#include "rcpputils/filesystem_helper.hpp"

#include "rosbag2_compression/zstd_decompressor.hpp"

#include "logging.hpp"

namespace
{

// String constant used to identify ZstdDecompressor.
constexpr const char kDecompressionIdentifier[] = "zstd";
// Used as a parameter type in a function that accepts the output of ZSTD_decompress.
using ZstdDecompressReturnType = decltype(ZSTD_decompress(nullptr, 0, nullptr, 0));
// Used as a parameter type in a function that accepts the output of ZSTD_getFrameContentSize.
using ZstdGetFrameContentSizeReturnType = decltype(ZSTD_getFrameContentSize(nullptr, 0));

/**
 * Open a file using the C API.
 * This function calls OS-specific implementation of fopen.
 *
 * \param uri is the path to the file
 * \param read_mode is the read mode string accepted by fopen.
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
 *
 * \param uri is the path to the file.
 * \return the contents of the buffer as a vector.
 */
std::vector<uint8_t> get_input_buffer(const std::string & uri)
{
  // Read in buffer, handling accordingly
  const auto file_pointer = open_file(uri.c_str(), "rb");
  if (file_pointer == nullptr) {
    std::stringstream errmsg;
    errmsg << "Failed to open file: \"" << uri <<
      "\" for binary reading! errno(" << errno << ")";

    throw std::runtime_error{errmsg.str()};
  }

  const auto file_path = rcpputils::fs::path{uri};
  const auto compressed_buffer_length = file_path.exists() ? file_path.file_size() : 0u;
  if (compressed_buffer_length == 0) {
    fclose(file_pointer);

    std::stringstream errmsg;
    errmsg << "Unable to get size of file: \"" << uri << "\"";

    throw std::runtime_error{errmsg.str()};
  }

  // Initialize compress_buffer with size = compressed_buffer_length.
  // Uniform initialization cannot be used here since it will choose
  // the initializer list constructor instead.
  std::vector<uint8_t> compressed_buffer(compressed_buffer_length);

  const auto read_count = fread(
    compressed_buffer.data(), sizeof(decltype(compressed_buffer)::value_type),
    compressed_buffer_length, file_pointer);

  if (read_count != compressed_buffer_length) {
    ROSBAG2_COMPRESSION_LOG_ERROR_STREAM(
      "Bytes read !(" <<
        read_count << ") != compressed_buffer size (" << compressed_buffer.size() <<
        ")!");
    // An error indicator is set by fread, so the following check will throw.
  }

  if (ferror(file_pointer)) {
    fclose(file_pointer);

    std::stringstream errmsg;
    errmsg << "Unable to read binary data from file: \"" << uri << "\"!";

    throw std::runtime_error{errmsg.str()};
  }

  fclose(file_pointer);
  return compressed_buffer;
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
  if (file_pointer == nullptr) {
    std::stringstream errmsg;
    errmsg << "Failed to open file: \"" << uri <<
      "\" for binary writing! errno(" << errno << ")";

    throw std::runtime_error{errmsg.str()};
  }

  const auto write_count = fwrite(
    output_buffer.data(), sizeof(uint8_t),
    output_buffer.size(), file_pointer);

  if (write_count != output_buffer.size()) {
    ROSBAG2_COMPRESSION_LOG_ERROR_STREAM(
      "Bytes written (" <<
        write_count << " != output_buffer size (" << output_buffer.size() <<
        ")!");
    // An error indicator is set by fwrite, so the following check will throw.
  }

  if (ferror(file_pointer)) {
    fclose(file_pointer);

    std::stringstream errmsg;
    errmsg << "Unable to write decompressed data to file: \"" << uri << "\"!";

    throw std::runtime_error{errmsg.str()};
  }

  fclose(file_pointer);
}

/**
 * Checks compression_result and throws a runtime_error if there was a ZSTD error.
 * \param compression_result is the return value of ZSTD_decompress.
 */
void throw_on_zstd_error(const ZstdDecompressReturnType compression_result)
{
  if (ZSTD_isError(compression_result)) {
    std::stringstream error;
    error << "ZSTD decompression error: " << ZSTD_getErrorName(compression_result);

    throw std::runtime_error{error.str()};
  }
}

/**
 * Checks frame_content and throws a runtime_error if there was a ZSTD error
 * or frame_content is invalid.
 * \param frame_content is the return value of ZSTD_getFrameContentSize.
 */
void throw_on_invalid_frame_content(const ZstdGetFrameContentSizeReturnType frame_content)
{
  if (frame_content == ZSTD_CONTENTSIZE_ERROR) {
    throw std::runtime_error{"Unable to determine file size due to error."};
  } else if (frame_content == ZSTD_CONTENTSIZE_UNKNOWN) {
    throw std::runtime_error{"Unable to determine file size."};
  }
}

/**
 * Prints decompression statistics to the debug log stream.
 * The log statement is formatted as JSON.
 * Time is formatted as a decimal of seconds.
 *
 * Example:
 *  "Decompression statistics" : {"Time" : 1.2, "Compression Ratio" : 0.5}
 *
 * \param start is the time_point when compression started.
 * \param end is the time_point when compression ended.
 * \param decompressed_size is the file size after decompression
 * \param compressed_size is the compressed file size
 */
void print_decompression_statistics(
  const std::chrono::high_resolution_clock::time_point start,
  const std::chrono::high_resolution_clock::time_point end,
  const size_t decompressed_size,
  const size_t compressed_size)
{
  const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
  const auto decompression_ratio =
    static_cast<double>(decompressed_size) / static_cast<double>(compressed_size);

  ROSBAG2_COMPRESSION_LOG_DEBUG_STREAM(
    "\"Decompression statistics\" : {" <<
      "\"Time\" : " << (duration.count() / 1000.0) <<
      ", \"Decompression Ratio\" : " << decompression_ratio <<
      "}");
}
}  // namespace

namespace rosbag2_compression
{

std::string ZstdDecompressor::decompress_uri(const std::string & uri)
{
  const auto start = std::chrono::high_resolution_clock::now();
  const auto uri_path = rcpputils::fs::path{uri};
  const auto decompressed_uri = rcpputils::fs::remove_extension(uri_path).string();
  const auto compressed_buffer = get_input_buffer(uri);
  const auto compressed_buffer_length = compressed_buffer.size();

  const auto decompressed_buffer_length =
    ZSTD_getFrameContentSize(compressed_buffer.data(), compressed_buffer_length);

  throw_on_invalid_frame_content(decompressed_buffer_length);

  // Initializes decompressed_buffer with size = decompressed_buffer_length.
  // Uniform initialization cannot be used here since it will choose
  // the initializer list constructor instead.
  std::vector<uint8_t> decompressed_buffer(decompressed_buffer_length);

  const auto decompression_result = ZSTD_decompress(
    decompressed_buffer.data(), decompressed_buffer_length,
    compressed_buffer.data(), compressed_buffer_length);

  throw_on_zstd_error(decompression_result);
  decompressed_buffer.resize(decompression_result);

  write_output_buffer(decompressed_buffer, decompressed_uri);

  const auto end = std::chrono::high_resolution_clock::now();
  print_decompression_statistics(start, end, decompression_result, compressed_buffer_length);

  return decompressed_uri;
}

void ZstdDecompressor::decompress_serialized_bag_message(
  rosbag2_storage::SerializedBagMessage *)
{
  throw std::logic_error{"Not implemented"};
}

std::string ZstdDecompressor::get_decompression_identifier() const
{
  return kDecompressionIdentifier;
}
}  // namespace rosbag2_compression
