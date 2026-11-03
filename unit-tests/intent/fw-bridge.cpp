// License: Apache 2.0. Copyright (C) 2026 RealSense, Inc.
// Compile against the actual FW source with -Drsintent=fwintent.
#include "IntentProtocol.h"
extern "C" int fw_accept(const unsigned char *bytes, unsigned sku) {
    rsintent::Request request = {};
    auto error = rsintent::decode(bytes, 1024, request);
    if (error.code != rsintent::OK)
        return error.code;
    return rsintent::validate(request.model, static_cast<uint16_t>(sku)).code;
}
extern "C" void fw_template(unsigned sku, unsigned char *bytes) {
    rsintent::encode(rsintent::defaultModel(static_cast<uint16_t>(sku)), rsintent::SET, bytes,
                     1024);
}
