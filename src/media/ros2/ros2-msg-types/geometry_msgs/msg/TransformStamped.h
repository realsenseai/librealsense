// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

/*!
 * @file TransformStamped.h
 * Standalone CDR serialization type for geometry_msgs/msg/TransformStamped,
 * following the same generated-code conventions as Image.h.
 */

#ifndef _FAST_DDS_GENERATED_GEOMETRY_MSGS_MSG_TRANSFORMSTAMPED_H_
#define _FAST_DDS_GENERATED_GEOMETRY_MSGS_MSG_TRANSFORMSTAMPED_H_

#include <std_msgs/msg/Header.h>
#include <geometry_msgs/msg/Transform.h>

#include <string>

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
         * @brief This class represents the structure TransformStamped defined by the user in the IDL file.
         * @ingroup TRANSFORMSTAMPED
         */
        class TransformStamped
        {
        public:

            eProsima_user_DllExport TransformStamped();

            eProsima_user_DllExport ~TransformStamped();

            eProsima_user_DllExport TransformStamped(
                    const TransformStamped& x);

            eProsima_user_DllExport TransformStamped(
                    TransformStamped&& x) noexcept;

            eProsima_user_DllExport TransformStamped& operator =(
                    const TransformStamped& x);

            eProsima_user_DllExport TransformStamped& operator =(
                    TransformStamped&& x) noexcept;

            eProsima_user_DllExport bool operator ==(
                    const TransformStamped& x) const;

            eProsima_user_DllExport bool operator !=(
                    const TransformStamped& x) const;

            eProsima_user_DllExport void header(
                    const std_msgs::msg::Header& _header);

            eProsima_user_DllExport void header(
                    std_msgs::msg::Header&& _header);

            eProsima_user_DllExport const std_msgs::msg::Header& header() const;

            eProsima_user_DllExport std_msgs::msg::Header& header();

            eProsima_user_DllExport void child_frame_id(
                    const std::string& _child_frame_id);

            eProsima_user_DllExport void child_frame_id(
                    std::string&& _child_frame_id);

            eProsima_user_DllExport const std::string& child_frame_id() const;

            eProsima_user_DllExport std::string& child_frame_id();

            eProsima_user_DllExport void transform(
                    const geometry_msgs::msg::Transform& _transform);

            eProsima_user_DllExport void transform(
                    geometry_msgs::msg::Transform&& _transform);

            eProsima_user_DllExport const geometry_msgs::msg::Transform& transform() const;

            eProsima_user_DllExport geometry_msgs::msg::Transform& transform();

            eProsima_user_DllExport static size_t getMaxCdrSerializedSize(
                    size_t current_alignment = 0);

            eProsima_user_DllExport static size_t getCdrSerializedSize(
                    const geometry_msgs::msg::TransformStamped& data,
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

            std_msgs::msg::Header m_header;
            std::string m_child_frame_id;
            geometry_msgs::msg::Transform m_transform;
        };
    } // namespace msg
} // namespace geometry_msgs

#endif // _FAST_DDS_GENERATED_GEOMETRY_MSGS_MSG_TRANSFORMSTAMPED_H_
