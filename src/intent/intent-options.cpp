// License: Apache 2.0. Copyright (C) 2026 RealSense, Inc.
#include "contract/IntentProtocol.h"
#include "intent-json.h"
#include <librealsense2/h/rs_composite_option.h>
#include <mutex>
#include <src/composite-option-interface.h>
#include <src/core/device-interface.h>
#include <src/ds/d500/d500-device.h>
#include <src/ds/ds-private.h>
#include <src/uvc-sensor.h>
namespace librealsense {
namespace {
class intent_option : public composite_option_interface {
    std::weak_ptr<uvc_sensor> _sensor;
    bool _status;
    uint16_t _sku;
    std::mutex _mutex;
    std::vector<uint8_t> _expected;

  public:
    intent_option(std::shared_ptr<uvc_sensor> sensor, bool status, uint16_t sku = 0)
        : _sensor(sensor), _status(status), _sku(sku) {}
    std::vector<uint8_t> get_raw() const override {
        auto sensor = _sensor.lock();
        if (!sensor)
            throw std::runtime_error("Intent: sensor disconnected");
        return sensor->invoke_powered([&](platform::uvc_device &dev) {
            std::vector<uint8_t> wire(_status ? 32U : rsintent::wire_size);
            if (!dev.get_xu(ds::depth_xu,
                            _status ? rsintent::GLOBAL_INTENT_STATUS : rsintent::GLOBAL_INTENT,
                            wire.data(), static_cast<int>(wire.size())))
                throw std::runtime_error("Intent GET failed");
            return wire;
        });
    }
    void set_raw(const void *data, size_t size) override {
        if (_status)
            throw std::invalid_argument("Intent status is read-only");
        if (!data || size != rsintent::wire_size)
            throw std::invalid_argument("Intent transfer must contain 1024 bytes");
        auto sensor = _sensor.lock();
        if (!sensor)
            throw std::runtime_error("Intent: sensor disconnected");
        rsintent::Request request = {};
        if (rsintent::decode(static_cast<const uint8_t *>(data), size, request).code !=
            rsintent::OK)
            throw std::invalid_argument("Intent: malformed payload");
        // Allocate and normalize before SET so a successful device mutation is
        // followed only by a nonthrowing local state update.
        std::vector<uint8_t> next;
        if (request.op == rsintent::SET) {
            auto model = rsintent::defaultModel(_sku);
            for (size_t i = 0; i < request.model.count; ++i)
                for (size_t j = 0; j < model.count; ++j)
                    if (request.model.streams[i].branch == model.streams[j].branch)
                        model.streams[j] = request.model.streams[i];
            next.resize(rsintent::wire_size);
            if (rsintent::encode(model, rsintent::SET, next.data(), next.size()).code !=
                rsintent::OK)
                throw std::invalid_argument("Intent: cannot normalize declaration");
        }
        std::lock_guard<std::mutex> lock(_mutex);
        sensor->invoke_powered([&](platform::uvc_device &dev) {
            if (!dev.set_xu(ds::depth_xu, rsintent::GLOBAL_INTENT,
                            static_cast<const uint8_t *>(data), static_cast<int>(size)))
                throw std::runtime_error("Intent SET rejected");
        });
        if (request.op != rsintent::VALIDATE_ONLY)
            _expected.swap(next);
    }

    void verify() {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_expected.empty())
            return;
        auto sensor = _sensor.lock();
        if (!sensor)
            throw std::runtime_error("Intent: sensor disconnected");
        uint8_t status[32] = {};
        sensor->invoke_powered([&](platform::uvc_device &dev) {
            if (!dev.get_xu(ds::depth_xu, rsintent::GLOBAL_INTENT_STATUS, status, 32))
                throw std::runtime_error("Intent: status read failed before start");
        });
        if (status[2] == 0U || get_raw() != _expected)
            throw std::runtime_error(
                "Intent cleared or replaced; declare it again before starting");
    }
    std::vector<uint8_t> get_raw_range() const override {
        throw std::invalid_argument("Intent is a document, not a numeric range");
    }
    bool is_read_only() const override { return _status; }
    bool is_enabled() const override { return true; }
    const char *get_description() const override {
        return _status ? "Global Intent status (read-only)" : "Global streams Intent (1.0)";
    }
};
} // namespace
void register_intent_options(d500_depth_sensor &sensor) {
    auto raw = std::dynamic_pointer_cast<uvc_sensor>(sensor.get_raw_sensor());
    if (!raw)
        return;
    auto status = std::make_shared<intent_option>(raw, true);
    uint16_t sku = 0;
    try {
        const auto wire = status->get_raw();
        if (wire.size() != 32U || wire[0] != rsintent::major)
            return;
        sku = rsintent::read16(wire.data() + 8);
        if (sku != 0x0c04U && sku != 0x0c05U)
            return;
    } catch (const std::exception &) {
        return;
    } // Unsupported firmware keeps legacy behavior.
    sensor.register_composite_option(RS2_COMPOSITE_OPTION_GLOBAL_INTENT,
                                     std::make_shared<intent_option>(raw, false, sku));
    sensor.register_composite_option(RS2_COMPOSITE_OPTION_GLOBAL_INTENT_STATUS, status);
}
} // namespace librealsense

namespace librealsense {
void verify_intent_before_start(device_interface &device) {
    for (size_t i = 0; i < device.get_sensors_count(); ++i) {
        auto &sensor = device.get_sensor(i);
        if (!sensor.supports_composite_option(RS2_COMPOSITE_OPTION_GLOBAL_INTENT))
            continue;
        auto *option = dynamic_cast<intent_option *>(
            &sensor.get_composite_option(RS2_COMPOSITE_OPTION_GLOBAL_INTENT));
        if (option)
            option->verify();
    }
}
} // namespace librealsense
