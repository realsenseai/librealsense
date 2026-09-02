// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

/*!
 * @file TFMessage.h
 * Standalone CDR serialization type for tf2_msgs/msg/TFMessage,
 * following the same generated-code conventions as Image.h.
 * Written by hand per the fastddsgen output structure (see
 * third-party/realdds/include/realdds/topics/readme.md for the generation procedure);
 * wire format verified against ROS2 Humble tf2 tooling.
 */

#ifndef _FAST_DDS_GENERATED_TF2_MSGS_MSG_TFMESSAGE_H_
#define _FAST_DDS_GENERATED_TF2_MSGS_MSG_TFMESSAGE_H_

#include <geometry_msgs/msg/TransformStamped.h>

#include <vector>

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


namespace tf2_msgs {
    namespace msg {
        /*!
         * @brief This class represents the structure TFMessage defined by the user in the IDL file.
         * @ingroup TFMESSAGE
         */
        class TFMessage
        {
        public:

            eProsima_user_DllExport TFMessage();

            eProsima_user_DllExport ~TFMessage();

            eProsima_user_DllExport TFMessage(
                    const TFMessage& x);

            eProsima_user_DllExport TFMessage(
                    TFMessage&& x) noexcept;

            eProsima_user_DllExport TFMessage& operator =(
                    const TFMessage& x);

            eProsima_user_DllExport TFMessage& operator =(
                    TFMessage&& x) noexcept;

            eProsima_user_DllExport bool operator ==(
                    const TFMessage& x) const;

            eProsima_user_DllExport bool operator !=(
                    const TFMessage& x) const;

            eProsima_user_DllExport void transforms(
                    const std::vector<geometry_msgs::msg::TransformStamped>& _transforms);

            eProsima_user_DllExport void transforms(
                    std::vector<geometry_msgs::msg::TransformStamped>&& _transforms);

            eProsima_user_DllExport const std::vector<geometry_msgs::msg::TransformStamped>& transforms() const;

            eProsima_user_DllExport std::vector<geometry_msgs::msg::TransformStamped>& transforms();

            eProsima_user_DllExport static size_t getMaxCdrSerializedSize(
                    size_t current_alignment = 0);

            eProsima_user_DllExport static size_t getCdrSerializedSize(
                    const tf2_msgs::msg::TFMessage& data,
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

            std::vector<geometry_msgs::msg::TransformStamped> m_transforms;
        };
    } // namespace msg
} // namespace tf2_msgs

#endif // _FAST_DDS_GENERATED_TF2_MSGS_MSG_TFMESSAGE_H_
