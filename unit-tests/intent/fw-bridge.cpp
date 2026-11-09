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
extern "C" int fw_has_rules() {
#ifdef INTENT_FW_RULES
    return 1;
#else
    return 0;
#endif
}
