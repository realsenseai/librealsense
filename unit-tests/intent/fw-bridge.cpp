// License: Apache 2.0. Copyright (C) 2026 RealSense, Inc.
// Compile against the actual FW source with -Drsintent=fwintent.
#include "IntentProtocol.h"
extern "C" int fw_accept_on(const unsigned char *bytes, unsigned sku, unsigned transport) {
    if (transport > rsintent::GMSL) return rsintent::CONSTRAINT;
    rsintent::Request request = {};
    auto error = rsintent::decode(bytes, rsintent::wire_size, request);
    if (error.code != rsintent::OK)
        return error.code;
    if (request.op == rsintent::CLEAR) return 0;
#ifdef INTENT_FW_RULES
    error = rsintent::completeness(request.model, static_cast<uint16_t>(sku), request.minorVersion);
    if (error.code != rsintent::OK) return error.code;
#endif
    return rsintent::validate(request.model, static_cast<uint16_t>(sku),
                              static_cast<rsintent::Transport>(transport)).code;
}
extern "C" int fw_accept(const unsigned char *bytes, unsigned sku) {
    return fw_accept_on(bytes, sku, rsintent::USB3);
}
extern "C" int fw_decode(const unsigned char *bytes, unsigned size, unsigned *detail,
                         unsigned *offset) {
    rsintent::Request request = {};
    auto error = rsintent::decode(bytes, size, request);
    *detail = error.detail;
    *offset = error.offset;
    return error.code;
}
extern "C" void fw_template(unsigned sku, unsigned char *bytes) {
    rsintent::encode(rsintent::defaultModel(static_cast<uint16_t>(sku)), rsintent::SET, bytes,
                     rsintent::wire_size);
}
#ifdef INTENT_FW_RULES
#include "IntentController.h"
namespace {
struct IdleDevice : rsintent::Backend {
    bool idle() const override { return true; }
    rsintent::Error check(const rsintent::Model &, uint16_t) override { return rsintent::Error(); }
    rsintent::Error prepare(const rsintent::Model &) override { return rsintent::Error(); }
    rsintent::Error gate(uint8_t, bool, bool) override { return rsintent::Error(); }
    rsintent::Error release() override { return rsintent::Error(); }
};
} // namespace
#endif
// FW controller: SET `intent`, then COMMIT one stream; the FW STATUS transfer goes to `status`.
extern "C" int fw_admission(const unsigned char *intent, unsigned sku, const unsigned *stream,
                            unsigned char *status) {
#ifdef INTENT_FW_RULES
    IdleDevice device;
    rsintent::Controller controller(static_cast<uint16_t>(sku), device);
    rsintent::Error error = controller.set(intent, rsintent::wire_size);
    if (error.code == rsintent::OK) {
        const rsintent::Stream s = {static_cast<uint8_t>(stream[0]), static_cast<uint8_t>(stream[1]), true,
                                    stream[2], static_cast<uint16_t>(stream[3]),
                                    static_cast<uint16_t>(stream[4]), static_cast<uint16_t>(stream[5])};
        uint32_t generation = 0;
        error = controller.commit(s, generation);
    }
    if (controller.status(status, rsintent::status_size).code != rsintent::OK) return -1;
    return error.code;
#else
    (void)intent, (void)sku, (void)stream, (void)status;
    return -1;
#endif
}
extern "C" int fw_has_rules() {
#ifdef INTENT_FW_RULES
    return 1;
#else
    return 0;
#endif
}
