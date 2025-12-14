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

#include <memory>
#include <string>

#include "rosbag2_compression/compression_factory.hpp"

#include "compression_factory_impl.hpp"

namespace rosbag2_compression
{

CompressionFactory::CompressionFactory()
: impl_(new CompressionFactoryImpl()) {}

CompressionFactory::~CompressionFactory() = default;

std::unique_ptr<rosbag2_compression::BaseCompressorInterface>
CompressionFactory::create_compressor(const std::string & compression_format)
{
  return impl_->create_compressor(compression_format);
}

std::unique_ptr<rosbag2_compression::BaseDecompressorInterface>
CompressionFactory::create_decompressor(const std::string & compression_format)
{
  return impl_->create_decompressor(compression_format);
}

}  // namespace rosbag2_compression
