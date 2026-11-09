// License: Apache 2.0. Copyright (C) 2026 RealSense, Inc.
#pragma once
#include "../h/rs_intent.h"
#include "rs_device.hpp"
namespace rs2 {
// Explicit opt-in wrapper; construction does not change device configuration.
class intent_device : public device {
  public:
    explicit intent_device(const device &dev) : device(dev.get()) {}
    std::string get_intent() const {
        rs2_error *e = nullptr;
        const rs2_raw_data_buffer *p = rs2_get_intent(get().get(), &e);
        error::handle(e);
        std::unique_ptr<const rs2_raw_data_buffer, decltype(&rs2_delete_raw_data)> owned(
            p, rs2_delete_raw_data);
        const int size = rs2_get_raw_data_size(p, &e);
        error::handle(e);
        const auto bytes = rs2_get_raw_data(p, &e);
        error::handle(e);
        return std::string(reinterpret_cast<const char *>(bytes), static_cast<size_t>(size));
    }
    void set_intent(const std::string &json) const {
        rs2_error *e = nullptr;
        rs2_set_intent(get().get(), json.c_str(), &e);
        error::handle(e);
    }
};
} // namespace rs2
