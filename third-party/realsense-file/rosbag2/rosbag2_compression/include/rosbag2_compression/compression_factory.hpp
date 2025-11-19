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

#ifndef ROSBAG2_COMPRESSION__COMPRESSION_FACTORY_HPP_
#define ROSBAG2_COMPRESSION__COMPRESSION_FACTORY_HPP_

#include <memory>
#include <string>

#include "base_compressor_interface.hpp"
#include "base_decompressor_interface.hpp"
#include "compression_options.hpp"
#include "visibility_control.hpp"

#ifdef _WIN32
# pragma warning(push)
# pragma warning(disable:4251)
#endif


namespace rosbag2_compression
{

/**
 * Implementation of CompressionFactory.
 * This class implements methods for creating instances of a BaseCompressionInterface and
 * BaseDecompressionInterface.
 * This class should only be used by CompressionFactory.
 */
class CompressionFactoryImpl;

class ROSBAG2_COMPRESSION_PUBLIC CompressionFactory
{
public:
  CompressionFactory();
  virtual ~CompressionFactory();

  /**
   * Create a compressor based on the specified compression format.
   *
   * \param compression_format The compression format as a string.
   * \return A unique pointer to the newly created compressor.
   * \throw invalid_argument If the compression format does not exist.
   */
  virtual std::unique_ptr<rosbag2_compression::BaseCompressorInterface>
  create_compressor(const std::string & compression_format);

  /**
   * Create a decompressor based on the specified compression format.
   *
   * \param compression_format The compression format as a string.
   * \return A unique pointer to the newly created decompressor.
   * \throw invalid_argument If the compression format does not exist.
   */
  virtual std::unique_ptr<rosbag2_compression::BaseDecompressorInterface>
  create_decompressor(const std::string & compression_format);

private:
  std::unique_ptr<CompressionFactoryImpl> impl_;
};

}  // namespace rosbag2_compression

#ifdef _WIN32
# pragma warning(pop)
#endif

#endif  // ROSBAG2_COMPRESSION__COMPRESSION_FACTORY_HPP_
