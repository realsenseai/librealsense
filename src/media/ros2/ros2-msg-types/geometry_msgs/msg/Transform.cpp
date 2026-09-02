// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

/*!
 * @file Transform.cpp
 * Standalone CDR serialization type for geometry_msgs/msg/Transform.
 */

#ifdef _WIN32
// Remove linker warning LNK4221 on Visual Studio
namespace {
char dummy;
}  // namespace
#endif  // _WIN32

#include "Transform.h"
#include <fastcdr/Cdr.h>

#include <fastcdr/exceptions/BadParamException.h>
using namespace eprosima::fastcdr::exception;

#include <utility>

geometry_msgs::msg::Transform::Transform()
{
}

geometry_msgs::msg::Transform::~Transform()
{
}

geometry_msgs::msg::Transform::Transform(
        const Transform& x)
{
    m_translation = x.m_translation;
    m_rotation = x.m_rotation;
}

geometry_msgs::msg::Transform::Transform(
        Transform&& x) noexcept
{
    m_translation = std::move(x.m_translation);
    m_rotation = std::move(x.m_rotation);
}

geometry_msgs::msg::Transform& geometry_msgs::msg::Transform::operator =(
        const Transform& x)
{
    m_translation = x.m_translation;
    m_rotation = x.m_rotation;

    return *this;
}

geometry_msgs::msg::Transform& geometry_msgs::msg::Transform::operator =(
        Transform&& x) noexcept
{
    m_translation = std::move(x.m_translation);
    m_rotation = std::move(x.m_rotation);

    return *this;
}

bool geometry_msgs::msg::Transform::operator ==(
        const Transform& x) const
{
    return (m_translation == x.m_translation && m_rotation == x.m_rotation);
}

bool geometry_msgs::msg::Transform::operator !=(
        const Transform& x) const
{
    return !(*this == x);
}

size_t geometry_msgs::msg::Transform::getMaxCdrSerializedSize(
        size_t current_alignment)
{
    size_t initial_alignment = current_alignment;

    current_alignment += geometry_msgs::msg::Vector3::getMaxCdrSerializedSize(current_alignment);
    current_alignment += geometry_msgs::msg::Quaternion::getMaxCdrSerializedSize(current_alignment);

    return current_alignment - initial_alignment;
}

size_t geometry_msgs::msg::Transform::getCdrSerializedSize(
        const geometry_msgs::msg::Transform& data,
        size_t current_alignment)
{
    (void)data;
    size_t initial_alignment = current_alignment;

    current_alignment += geometry_msgs::msg::Vector3::getCdrSerializedSize(data.translation(), current_alignment);
    current_alignment += geometry_msgs::msg::Quaternion::getCdrSerializedSize(data.rotation(), current_alignment);

    return current_alignment - initial_alignment;
}

void geometry_msgs::msg::Transform::serialize(
        eprosima::fastcdr::Cdr& scdr) const
{
    scdr << m_translation;
    scdr << m_rotation;
}

void geometry_msgs::msg::Transform::deserialize(
        eprosima::fastcdr::Cdr& dcdr)
{
    dcdr >> m_translation;
    dcdr >> m_rotation;
}

void geometry_msgs::msg::Transform::translation(
        const geometry_msgs::msg::Vector3& _translation)
{
    m_translation = _translation;
}

void geometry_msgs::msg::Transform::translation(
        geometry_msgs::msg::Vector3&& _translation)
{
    m_translation = std::move(_translation);
}

const geometry_msgs::msg::Vector3& geometry_msgs::msg::Transform::translation() const
{
    return m_translation;
}

geometry_msgs::msg::Vector3& geometry_msgs::msg::Transform::translation()
{
    return m_translation;
}

void geometry_msgs::msg::Transform::rotation(
        const geometry_msgs::msg::Quaternion& _rotation)
{
    m_rotation = _rotation;
}

void geometry_msgs::msg::Transform::rotation(
        geometry_msgs::msg::Quaternion&& _rotation)
{
    m_rotation = std::move(_rotation);
}

const geometry_msgs::msg::Quaternion& geometry_msgs::msg::Transform::rotation() const
{
    return m_rotation;
}

geometry_msgs::msg::Quaternion& geometry_msgs::msg::Transform::rotation()
{
    return m_rotation;
}

size_t geometry_msgs::msg::Transform::getKeyMaxCdrSerializedSize(
        size_t current_alignment)
{
    size_t current_align = current_alignment;

    return current_align;
}

bool geometry_msgs::msg::Transform::isKeyDefined()
{
    return false;
}

void geometry_msgs::msg::Transform::serializeKey(
        eprosima::fastcdr::Cdr& scdr) const
{
    (void) scdr;
}
