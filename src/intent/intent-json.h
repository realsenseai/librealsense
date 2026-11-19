// License: Apache 2.0. Copyright (C) 2026 RealSense, Inc.
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <rsutils/json-fwd.h>
namespace librealsense {
// STATUS and GET_CUR drive compatibility warnings, never trimming or version downgrade.
std::vector<uint8_t> encode_intent_json(const std::string &text,
                                       const std::vector<uint8_t> &status = {},
                                       const std::vector<uint8_t> &firmware = {},
                                       const std::vector<uint8_t> &discovery = {});
std::string decode_intent_capabilities(const std::vector<uint8_t> &discovery);
std::string decode_intent_json(const std::vector<uint8_t> &wire,
                               const std::vector<uint8_t> &status);
// Names last_result and every REJECT_REASON of a GLOBAL_INTENT_STATUS transfer (design 9.6).
}
namespace rsintent { struct Model; }
namespace librealsense {
std::string describe_intent_status(const std::vector<uint8_t> &status,
                                   const rsintent::Model *requested = nullptr);
std::string intent_format_name(uint32_t format);
std::string intent_output_mismatch(const rsutils::json &document, const std::string &name, int index,
                                   const char *format, int width, int height, int fps);
class d500_depth_sensor;
class device_interface;
void verify_intent_before_start(device_interface &device);
// Empty unless the device STATUS holds a stream-admission rejection.
std::string intent_rejection(device_interface &device) noexcept;
// Call only from a catch block: rethrows, appending intent_rejection() to backend errors.
[[noreturn]] void rethrow_with_intent_rejection(device_interface &device);
class intent_transport;
std::shared_ptr<intent_transport> probe_intent_transport(d500_depth_sensor &sensor);
std::string get_device_intent(device_interface &device);
void set_device_intent(device_interface &device, const std::string &document);
void clear_pipeline_intent(device_interface &device) noexcept;
class stream_profile_interface;
void verify_intent_outputs(device_interface &device,
    const std::vector<std::shared_ptr<stream_profile_interface>> &profiles);
void declare_pipeline_intent(device_interface &device,
    const std::vector<std::shared_ptr<stream_profile_interface>> &profiles);
} // namespace librealsense
