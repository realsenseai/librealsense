// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

/*!
 * @file Transform.h
 * Standalone CDR serialization type for geometry_msgs/msg/Transform,
 * following the same generated-code conventions as Vector3.h.
 */

#ifndef _FAST_DDS_GENERATED_GEOMETRY_MSGS_MSG_TRANSFORM_H_
#define _FAST_DDS_GENERATED_GEOMETRY_MSGS_MSG_TRANSFORM_H_

#include <geometry_msgs/msg/Vector3.h>
#include <geometry_msgs/msg/Quaternion.h>

#if defined(_WIN32)
#if defined(EPROSIMA_USER_DLL_EXPORT)
#define eProsima_user_DllExport __declspec( dllexport )
#else
#define eProsima_user_DllExport
#endif  // EPROSIMA_USER_DLL_EXPORT
#else
#define eProsima_user_DllExport
#endif  // _WIN32

namespace eprosima {
namespace fastcdr {
class Cdr;
} // namespace fastcdr
} // namespace eprosima


namespace geometry_msgs {
    namespace msg {
        /*!
         * @brief This class represents the structure Transform defined by the user in the IDL file.
         * @ingroup TRANSFORM
         */
        class Transform
        {
        public:

            eProsima_user_DllExport Transform();

            eProsima_user_DllExport ~Transform();

            eProsima_user_DllExport Transform(
                    const Transform& x);

            eProsima_user_DllExport Transform(
                    Transform&& x) noexcept;

            eProsima_user_DllExport Transform& operator =(
                    const Transform& x);

            eProsima_user_DllExport Transform& operator =(
                    Transform&& x) noexcept;

            eProsima_user_DllExport bool operator ==(
                    const Transform& x) const;

            eProsima_user_DllExport bool operator !=(
                    const Transform& x) const;

            eProsima_user_DllExport void translation(
                    const geometry_msgs::msg::Vector3& _translation);

            eProsima_user_DllExport void translation(
                    geometry_msgs::msg::Vector3&& _translation);

            eProsima_user_DllExport const geometry_msgs::msg::Vector3& translation() const;

            eProsima_user_DllExport geometry_msgs::msg::Vector3& translation();

            eProsima_user_DllExport void rotation(
                    const geometry_msgs::msg::Quaternion& _rotation);

            eProsima_user_DllExport void rotation(
                    geometry_msgs::msg::Quaternion&& _rotation);

            eProsima_user_DllExport const geometry_msgs::msg::Quaternion& rotation() const;

            eProsima_user_DllExport geometry_msgs::msg::Quaternion& rotation();

            eProsima_user_DllExport static size_t getMaxCdrSerializedSize(
                    size_t current_alignment = 0);

            eProsima_user_DllExport static size_t getCdrSerializedSize(
                    const geometry_msgs::msg::Transform& data,
                    size_t current_alignment = 0);

            eProsima_user_DllExport void serialize(
                    eprosima::fastcdr::Cdr& cdr) const;

            eProsima_user_DllExport void deserialize(
                    eprosima::fastcdr::Cdr& cdr);

            eProsima_user_DllExport static size_t getKeyMaxCdrSerializedSize(
                    size_t current_alignment = 0);

            eProsima_user_DllExport static bool isKeyDefined();

            eProsima_user_DllExport void serializeKey(
                    eprosima::fastcdr::Cdr& cdr) const;

        private:

            geometry_msgs::msg::Vector3 m_translation;
            geometry_msgs::msg::Quaternion m_rotation;
        };
    } // namespace msg
} // namespace geometry_msgs

#endif // _FAST_DDS_GENERATED_GEOMETRY_MSGS_MSG_TRANSFORM_H_
