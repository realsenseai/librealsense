// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

/*!
 * @file TransformStamped.cpp
 * Standalone CDR serialization type for geometry_msgs/msg/TransformStamped.
 */

#ifdef _WIN32
// Remove linker warning LNK4221 on Visual Studio
namespace {
char dummy;
}  // namespace
#endif  // _WIN32

#include "TransformStamped.h"
#include <fastcdr/Cdr.h>

#include <fastcdr/exceptions/BadParamException.h>
using namespace eprosima::fastcdr::exception;

#include <utility>

geometry_msgs::msg::TransformStamped::TransformStamped()
{
    m_child_frame_id = "";
}

geometry_msgs::msg::TransformStamped::~TransformStamped()
{
}

geometry_msgs::msg::TransformStamped::TransformStamped(
        const TransformStamped& x)
{
    m_header = x.m_header;
    m_child_frame_id = x.m_child_frame_id;
    m_transform = x.m_transform;
}

geometry_msgs::msg::TransformStamped::TransformStamped(
        TransformStamped&& x) noexcept
{
    m_header = std::move(x.m_header);
    m_child_frame_id = std::move(x.m_child_frame_id);
    m_transform = std::move(x.m_transform);
}

geometry_msgs::msg::TransformStamped& geometry_msgs::msg::TransformStamped::operator =(
        const TransformStamped& x)
{
    m_header = x.m_header;
    m_child_frame_id = x.m_child_frame_id;
    m_transform = x.m_transform;

    return *this;
}

geometry_msgs::msg::TransformStamped& geometry_msgs::msg::TransformStamped::operator =(
        TransformStamped&& x) noexcept
{
    m_header = std::move(x.m_header);
    m_child_frame_id = std::move(x.m_child_frame_id);
    m_transform = std::move(x.m_transform);

    return *this;
}

bool geometry_msgs::msg::TransformStamped::operator ==(
        const TransformStamped& x) const
{
    return (m_header == x.m_header && m_child_frame_id == x.m_child_frame_id && m_transform == x.m_transform);
}

bool geometry_msgs::msg::TransformStamped::operator !=(
        const TransformStamped& x) const
{
    return !(*this == x);
}

size_t geometry_msgs::msg::TransformStamped::getMaxCdrSerializedSize(
        size_t current_alignment)
{
    size_t initial_alignment = current_alignment;

    current_alignment += std_msgs::msg::Header::getMaxCdrSerializedSize(current_alignment);

    current_alignment += 4 + eprosima::fastcdr::Cdr::alignment(current_alignment, 4) + 255 + 1;

    current_alignment += geometry_msgs::msg::Transform::getMaxCdrSerializedSize(current_alignment);

    return current_alignment - initial_alignment;
}

size_t geometry_msgs::msg::TransformStamped::getCdrSerializedSize(
        const geometry_msgs::msg::TransformStamped& data,
        size_t current_alignment)
{
    (void)data;
    size_t initial_alignment = current_alignment;

    current_alignment += std_msgs::msg::Header::getCdrSerializedSize(data.header(), current_alignment);

    current_alignment += 4 + eprosima::fastcdr::Cdr::alignment(current_alignment, 4) + data.child_frame_id().size() + 1;

    current_alignment += geometry_msgs::msg::Transform::getCdrSerializedSize(data.transform(), current_alignment);

    return current_alignment - initial_alignment;
}

void geometry_msgs::msg::TransformStamped::serialize(
        eprosima::fastcdr::Cdr& scdr) const
{
    scdr << m_header;
    scdr << m_child_frame_id.c_str();
    scdr << m_transform;
}

void geometry_msgs::msg::TransformStamped::deserialize(
        eprosima::fastcdr::Cdr& dcdr)
{
    dcdr >> m_header;
    dcdr >> m_child_frame_id;
    dcdr >> m_transform;
}

void geometry_msgs::msg::TransformStamped::header(
        const std_msgs::msg::Header& _header)
{
    m_header = _header;
}

void geometry_msgs::msg::TransformStamped::header(
        std_msgs::msg::Header&& _header)
{
    m_header = std::move(_header);
}

const std_msgs::msg::Header& geometry_msgs::msg::TransformStamped::header() const
{
    return m_header;
}

std_msgs::msg::Header& geometry_msgs::msg::TransformStamped::header()
{
    return m_header;
}

void geometry_msgs::msg::TransformStamped::child_frame_id(
        const std::string& _child_frame_id)
{
    m_child_frame_id = _child_frame_id;
}

void geometry_msgs::msg::TransformStamped::child_frame_id(
        std::string&& _child_frame_id)
{
    m_child_frame_id = std::move(_child_frame_id);
}

const std::string& geometry_msgs::msg::TransformStamped::child_frame_id() const
{
    return m_child_frame_id;
}

std::string& geometry_msgs::msg::TransformStamped::child_frame_id()
{
    return m_child_frame_id;
}

void geometry_msgs::msg::TransformStamped::transform(
        const geometry_msgs::msg::Transform& _transform)
{
    m_transform = _transform;
}

void geometry_msgs::msg::TransformStamped::transform(
        geometry_msgs::msg::Transform&& _transform)
{
    m_transform = std::move(_transform);
}

const geometry_msgs::msg::Transform& geometry_msgs::msg::TransformStamped::transform() const
{
    return m_transform;
}

geometry_msgs::msg::Transform& geometry_msgs::msg::TransformStamped::transform()
{
    return m_transform;
}

size_t geometry_msgs::msg::TransformStamped::getKeyMaxCdrSerializedSize(
        size_t current_alignment)
{
    size_t current_align = current_alignment;

    return current_align;
}

bool geometry_msgs::msg::TransformStamped::isKeyDefined()
{
    return false;
}

void geometry_msgs::msg::TransformStamped::serializeKey(
        eprosima::fastcdr::Cdr& scdr) const
{
    (void) scdr;
}
