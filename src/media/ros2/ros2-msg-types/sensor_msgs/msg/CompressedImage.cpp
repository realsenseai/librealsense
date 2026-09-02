// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

/*!
 * @file CompressedImage.cpp
 * This source file contains the definition of the described types in the IDL file.
 *
 * Standalone CDR serialization type for sensor_msgs/msg/CompressedImage,
 * following the same generated-code conventions as Image.cpp.
 */

#ifdef _WIN32
// Remove linker warning LNK4221 on Visual Studio
namespace {
char dummy;
}  // namespace
#endif  // _WIN32

#include "CompressedImage.h"
#include <fastcdr/Cdr.h>

#include <fastcdr/exceptions/BadParamException.h>
using namespace eprosima::fastcdr::exception;

#include <utility>

sensor_msgs::msg::CompressedImage::CompressedImage()
{
    m_format = "";
}

sensor_msgs::msg::CompressedImage::~CompressedImage()
{
}

sensor_msgs::msg::CompressedImage::CompressedImage(
        const CompressedImage& x)
{
    m_header = x.m_header;
    m_format = x.m_format;
    m_data = x.m_data;
}

sensor_msgs::msg::CompressedImage::CompressedImage(
        CompressedImage&& x) noexcept
{
    m_header = std::move(x.m_header);
    m_format = std::move(x.m_format);
    m_data = std::move(x.m_data);
}

sensor_msgs::msg::CompressedImage& sensor_msgs::msg::CompressedImage::operator =(
        const CompressedImage& x)
{
    m_header = x.m_header;
    m_format = x.m_format;
    m_data = x.m_data;

    return *this;
}

sensor_msgs::msg::CompressedImage& sensor_msgs::msg::CompressedImage::operator =(
        CompressedImage&& x) noexcept
{
    m_header = std::move(x.m_header);
    m_format = std::move(x.m_format);
    m_data = std::move(x.m_data);

    return *this;
}

bool sensor_msgs::msg::CompressedImage::operator ==(
        const CompressedImage& x) const
{
    return (m_header == x.m_header && m_format == x.m_format && m_data == x.m_data);
}

bool sensor_msgs::msg::CompressedImage::operator !=(
        const CompressedImage& x) const
{
    return !(*this == x);
}

size_t sensor_msgs::msg::CompressedImage::getMaxCdrSerializedSize(
        size_t current_alignment)
{
    size_t initial_alignment = current_alignment;

    current_alignment += std_msgs::msg::Header::getMaxCdrSerializedSize(current_alignment);

    current_alignment += 4 + eprosima::fastcdr::Cdr::alignment(current_alignment, 4) + 255 + 1;

    current_alignment += 4 + eprosima::fastcdr::Cdr::alignment(current_alignment, 4);
    current_alignment += (100 * 1) + eprosima::fastcdr::Cdr::alignment(current_alignment, 1);

    return current_alignment - initial_alignment;
}

size_t sensor_msgs::msg::CompressedImage::getCdrSerializedSize(
        const sensor_msgs::msg::CompressedImage& data,
        size_t current_alignment)
{
    (void)data;
    size_t initial_alignment = current_alignment;

    current_alignment += std_msgs::msg::Header::getCdrSerializedSize(data.header(), current_alignment);

    current_alignment += 4 + eprosima::fastcdr::Cdr::alignment(current_alignment, 4) + data.format().size() + 1;

    current_alignment += 4 + eprosima::fastcdr::Cdr::alignment(current_alignment, 4);

    if (data.data().size() > 0)
    {
        current_alignment += (data.data().size() * 1) + eprosima::fastcdr::Cdr::alignment(current_alignment, 1);
    }

    return current_alignment - initial_alignment;
}

void sensor_msgs::msg::CompressedImage::serialize(
        eprosima::fastcdr::Cdr& scdr) const
{
    scdr << m_header;
    scdr << m_format.c_str();
    scdr << m_data;
}

void sensor_msgs::msg::CompressedImage::deserialize(
        eprosima::fastcdr::Cdr& dcdr)
{
    dcdr >> m_header;
    dcdr >> m_format;
    dcdr >> m_data;
}

void sensor_msgs::msg::CompressedImage::header(
        const std_msgs::msg::Header& _header)
{
    m_header = _header;
}

void sensor_msgs::msg::CompressedImage::header(
        std_msgs::msg::Header&& _header)
{
    m_header = std::move(_header);
}

const std_msgs::msg::Header& sensor_msgs::msg::CompressedImage::header() const
{
    return m_header;
}

std_msgs::msg::Header& sensor_msgs::msg::CompressedImage::header()
{
    return m_header;
}

void sensor_msgs::msg::CompressedImage::format(
        const std::string& _format)
{
    m_format = _format;
}

void sensor_msgs::msg::CompressedImage::format(
        std::string&& _format)
{
    m_format = std::move(_format);
}

const std::string& sensor_msgs::msg::CompressedImage::format() const
{
    return m_format;
}

std::string& sensor_msgs::msg::CompressedImage::format()
{
    return m_format;
}

void sensor_msgs::msg::CompressedImage::data(
        const std::vector<uint8_t>& _data)
{
    m_data = _data;
}

void sensor_msgs::msg::CompressedImage::data(
        std::vector<uint8_t>&& _data)
{
    m_data = std::move(_data);
}

const std::vector<uint8_t>& sensor_msgs::msg::CompressedImage::data() const
{
    return m_data;
}

std::vector<uint8_t>& sensor_msgs::msg::CompressedImage::data()
{
    return m_data;
}

size_t sensor_msgs::msg::CompressedImage::getKeyMaxCdrSerializedSize(
        size_t current_alignment)
{
    size_t current_align = current_alignment;

    return current_align;
}

bool sensor_msgs::msg::CompressedImage::isKeyDefined()
{
    return false;
}

void sensor_msgs::msg::CompressedImage::serializeKey(
        eprosima::fastcdr::Cdr& scdr) const
{
    (void) scdr;
}
