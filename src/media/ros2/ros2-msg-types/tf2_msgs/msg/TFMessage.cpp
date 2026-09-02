// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

/*!
 * @file TFMessage.cpp
 * Standalone CDR serialization type for tf2_msgs/msg/TFMessage.
 */

#ifdef _WIN32
// Remove linker warning LNK4221 on Visual Studio
namespace {
char dummy;
}  // namespace
#endif  // _WIN32

#include "TFMessage.h"
#include <fastcdr/Cdr.h>

#include <fastcdr/exceptions/BadParamException.h>
using namespace eprosima::fastcdr::exception;

#include <utility>

tf2_msgs::msg::TFMessage::TFMessage()
{
}

tf2_msgs::msg::TFMessage::~TFMessage()
{
}

tf2_msgs::msg::TFMessage::TFMessage(
        const TFMessage& x)
{
    m_transforms = x.m_transforms;
}

tf2_msgs::msg::TFMessage::TFMessage(
        TFMessage&& x) noexcept
{
    m_transforms = std::move(x.m_transforms);
}

tf2_msgs::msg::TFMessage& tf2_msgs::msg::TFMessage::operator =(
        const TFMessage& x)
{
    m_transforms = x.m_transforms;

    return *this;
}

tf2_msgs::msg::TFMessage& tf2_msgs::msg::TFMessage::operator =(
        TFMessage&& x) noexcept
{
    m_transforms = std::move(x.m_transforms);

    return *this;
}

bool tf2_msgs::msg::TFMessage::operator ==(
        const TFMessage& x) const
{
    return (m_transforms == x.m_transforms);
}

bool tf2_msgs::msg::TFMessage::operator !=(
        const TFMessage& x) const
{
    return !(*this == x);
}

size_t tf2_msgs::msg::TFMessage::getMaxCdrSerializedSize(
        size_t current_alignment)
{
    size_t initial_alignment = current_alignment;

    current_alignment += 4 + eprosima::fastcdr::Cdr::alignment(current_alignment, 4);

    for (size_t a = 0; a < 100; ++a)
    {
        current_alignment += geometry_msgs::msg::TransformStamped::getMaxCdrSerializedSize(current_alignment);
    }

    return current_alignment - initial_alignment;
}

size_t tf2_msgs::msg::TFMessage::getCdrSerializedSize(
        const tf2_msgs::msg::TFMessage& data,
        size_t current_alignment)
{
    (void)data;
    size_t initial_alignment = current_alignment;

    current_alignment += 4 + eprosima::fastcdr::Cdr::alignment(current_alignment, 4);

    for (size_t a = 0; a < data.transforms().size(); ++a)
    {
        current_alignment += geometry_msgs::msg::TransformStamped::getCdrSerializedSize(data.transforms().at(a), current_alignment);
    }

    return current_alignment - initial_alignment;
}

void tf2_msgs::msg::TFMessage::serialize(
        eprosima::fastcdr::Cdr& scdr) const
{
    scdr << m_transforms;
}

void tf2_msgs::msg::TFMessage::deserialize(
        eprosima::fastcdr::Cdr& dcdr)
{
    dcdr >> m_transforms;
}

void tf2_msgs::msg::TFMessage::transforms(
        const std::vector<geometry_msgs::msg::TransformStamped>& _transforms)
{
    m_transforms = _transforms;
}

void tf2_msgs::msg::TFMessage::transforms(
        std::vector<geometry_msgs::msg::TransformStamped>&& _transforms)
{
    m_transforms = std::move(_transforms);
}

const std::vector<geometry_msgs::msg::TransformStamped>& tf2_msgs::msg::TFMessage::transforms() const
{
    return m_transforms;
}

std::vector<geometry_msgs::msg::TransformStamped>& tf2_msgs::msg::TFMessage::transforms()
{
    return m_transforms;
}

size_t tf2_msgs::msg::TFMessage::getKeyMaxCdrSerializedSize(
        size_t current_alignment)
{
    size_t current_align = current_alignment;

    return current_align;
}

bool tf2_msgs::msg::TFMessage::isKeyDefined()
{
    return false;
}

void tf2_msgs::msg::TFMessage::serializeKey(
        eprosima::fastcdr::Cdr& scdr) const
{
    (void) scdr;
}
