// License: Apache 2.0. Copyright (C) 2026 RealSense, Inc.
// Compile against the actual FW source with -Drsintent=fwintent.
#include "IntentProtocol.h"
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
