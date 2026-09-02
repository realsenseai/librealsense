// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

/*!
 * @file CompressedImage.h
 * This header file contains the declaration of the described types in the IDL file.
 *
 * Standalone CDR serialization type for sensor_msgs/msg/CompressedImage,
 * following the same generated-code conventions as Image.h.
 */

#ifndef _FAST_DDS_GENERATED_SENSOR_MSGS_MSG_COMPRESSEDIMAGE_H_
#define _FAST_DDS_GENERATED_SENSOR_MSGS_MSG_COMPRESSEDIMAGE_H_

#include <std_msgs/msg/Header.h>

#include <stdint.h>
#include <string>
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


namespace sensor_msgs {
    namespace msg {
        /*!
         * @brief This class represents the structure CompressedImage defined by the user in the IDL file.
         * @ingroup COMPRESSEDIMAGE
         */
        class CompressedImage
        {
        public:

            eProsima_user_DllExport CompressedImage();

            eProsima_user_DllExport ~CompressedImage();

            eProsima_user_DllExport CompressedImage(
                    const CompressedImage& x);

            eProsima_user_DllExport CompressedImage(
                    CompressedImage&& x) noexcept;

            eProsima_user_DllExport CompressedImage& operator =(
                    const CompressedImage& x);

            eProsima_user_DllExport CompressedImage& operator =(
                    CompressedImage&& x) noexcept;

            eProsima_user_DllExport bool operator ==(
                    const CompressedImage& x) const;

            eProsima_user_DllExport bool operator !=(
                    const CompressedImage& x) const;

            eProsima_user_DllExport void header(
                    const std_msgs::msg::Header& _header);

            eProsima_user_DllExport void header(
                    std_msgs::msg::Header&& _header);

            eProsima_user_DllExport const std_msgs::msg::Header& header() const;

            eProsima_user_DllExport std_msgs::msg::Header& header();

            eProsima_user_DllExport void format(
                    const std::string& _format);

            eProsima_user_DllExport void format(
                    std::string&& _format);

            eProsima_user_DllExport const std::string& format() const;

            eProsima_user_DllExport std::string& format();

            eProsima_user_DllExport void data(
                    const std::vector<uint8_t>& _data);

            eProsima_user_DllExport void data(
                    std::vector<uint8_t>&& _data);

            eProsima_user_DllExport const std::vector<uint8_t>& data() const;

            eProsima_user_DllExport std::vector<uint8_t>& data();

            eProsima_user_DllExport static size_t getMaxCdrSerializedSize(
                    size_t current_alignment = 0);

            eProsima_user_DllExport static size_t getCdrSerializedSize(
                    const sensor_msgs::msg::CompressedImage& data,
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
            std::string m_format;
            std::vector<uint8_t> m_data;
        };
    } // namespace msg
} // namespace sensor_msgs

#endif // _FAST_DDS_GENERATED_SENSOR_MSGS_MSG_COMPRESSEDIMAGE_H_
