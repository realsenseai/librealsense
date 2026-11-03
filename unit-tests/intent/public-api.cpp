// License: Apache 2.0. Copyright (C) 2026 RealSense, Inc.
#include <cassert>
#include <librealsense2/hpp/rs_intent.hpp>
#include <librealsense2/rs.hpp>
// Compile the opt-in wrapper without requiring attached hardware.
void public_example(const rs2::device &device) {
    rs2::intent_device intent(device);
    const std::string saved = intent.get_intent();
    intent.set_intent(saved);
    intent.set_intent(R"({"op":"CLEAR"})");
}
int main() {
    rs2_error *error = nullptr;
    assert(rs2_get_intent(nullptr, &error) == nullptr && error);
    rs2_free_error(error);
    error = nullptr;
    rs2_set_intent(nullptr, "{}", &error);
    assert(error);
    rs2_free_error(error);
}
