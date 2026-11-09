// License: Apache 2.0. Copyright (C) 2026 RealSense, Inc.
#include "intent-json.h"
#include "intent-tlv.h"
#include "test-logging.h"
#include <cassert>
#include <iostream>
using librealsense::encode_intent_json;
namespace tlv = librealsense::intent_tlv;
int main() {
    assert(rsintent::minor == 1);
    WarningCapture warnings;
    std::vector<uint8_t> status(rsintent::status_size);
    status[0] = rsintent::major;
    const std::string twoC = R"({"version":"1.1.0","sensors":[
        {"sensor":"STEREO","streams":[{"branch":"DEPTH","state":"DISABLED"},
        {"branch":"IR_LR","state":"DISABLED"},{"branch":"RGB_L","state":"DISABLED"},
        {"branch":"RGB_R","state":"DISABLED"}]}]})";
    auto old = encode_intent_json(twoC, status);
    rsintent::Request request = {};
    assert(rsintent::decode(old.data(), old.size(), request).code == rsintent::OK);
    assert(request.model.count == 3 && !rsintent::find(request.model, 6));
    const auto stereo = tlv::parse(tlv::body(old)[0].value);
    for (const auto &r : stereo) if (r.type == rsintent::STREAM) assert(tlv::id(r) != 6);
    warnings.contains("branch RGB_R");
    status[1] = 1;
    auto current = encode_intent_json(twoC, status);
    assert(rsintent::decode(current.data(), current.size(), request).code == rsintent::OK);
    assert(request.model.count == 4 && rsintent::find(request.model, 6));
    const std::string threeC = R"({"version":"1.1.0","sensors":[
        {"sensor":"STEREO","streams":[{"branch":"DEPTH","state":"DISABLED"},
        {"branch":"IR_LR","state":"DISABLED"}]},
        {"sensor":"COLOR","streams":[{"branch":"RGB","state":"DISABLED"}]}]})";
    status[1] = 0;
    old = encode_intent_json(threeC, status);
    auto records = tlv::body(old);
    assert(records.size() == 1 && tlv::id(records[0]) == 1);
    assert(rsintent::decode(old.data(), old.size(), request).code == rsintent::OK);
    assert(!rsintent::find(request.model, 7));
    warnings.contains("branch RGB");
    warnings.contains("sensor COLOR");
    status[0] = 2;
    bool caught = false;
    try { (void)encode_intent_json(twoC, status); }
    catch (const std::invalid_argument &) { caught = true; }
    assert(caught);
    std::cout << "PASS: SDK N/FW N-1 omissions, named warnings, local major rejection\n";
}
