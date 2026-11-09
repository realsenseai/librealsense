// License: Apache 2.0. Copyright (C) 2026 RealSense, Inc.
#include "src/intent/intent-json.h"
#include "src/intent/intent-tlv.h"
#include "test-logging.h"
#include <cassert>
using rsutils::json;
using librealsense::decode_intent_json;
using librealsense::encode_intent_json;
namespace tlv = librealsense::intent_tlv;
static void reject(const json &j) {
    try { (void)encode_intent_json(j.dump()); }
    catch (const std::exception &) { return; }
    assert(!"invalid layout accepted");
}
void compatibility() {
    std::vector<uint8_t> status(rsintent::status_size);
    status[0] = rsintent::major;
    status[1] = 2;
    status[18] = 9;
    auto base = encode_intent_json(R"({"version":"1.2.9","sensors":[
        {"sensor":"STEREO","streams":[{"branch":"DEPTH","format":"Z16","width":1280,"height":720,"fps":30},
        {"branch":"IR_LR","state":"DISABLED"}]},
        {"sensor":"COLOR","streams":[{"branch":"RGB","state":"DISABLED"}]}]})");
    WarningCapture warnings;
    for (uint8_t flags = 0; flags < 4; ++flags) {
        auto records = tlv::body(base);
        auto children = tlv::parse(records[0].value);
        auto attributes = tlv::parse(children[1].value);
        attributes.insert(attributes.begin() + 1, {0xe3, flags, {0x12, 0x34, 0x56}});
        std::reverse(attributes.begin(), attributes.end());
        children[1].value = tlv::pack(attributes);
        children.insert(children.begin() + 1, {0xe2, flags, {0x78}});
        tlv::Record branchId = {rsintent::BRANCH_ID, 0, {99}};
        children.push_back({rsintent::STREAM, 3, tlv::pack({branchId, {0xe4, flags, {0xfe, 0xdc}}})});
        records[0].value = tlv::pack(children);
        // Noncanonical known flags and record order must survive too.
        records[0].flags = 3;
        records.insert(records.begin(), {0xe1, flags, {0xab, 0xcd}});
        tlv::Record sensorId = {rsintent::SENSOR_ID, 0, {220, 0, 0, 0}};
        records.push_back({rsintent::SENSOR, 2, tlv::pack({sensorId, {0xe5, flags, {0xaa}}})});
        auto future = tlv::transfer(base, records);
        const auto text = decode_intent_json(future, status);
        assert(json::parse(text).at("version") == "1.2.9");
        assert(encode_intent_json(text) == future);
        assert(encode_intent_json(text, status) == future);
        auto document = json::parse(text);
        assert(document["layout"].back()["id"] == 220);
        assert(document["sensors"][0]["layout"].back()["id"] == 99);
        // A known-field edit must not change any opaque value, flag, order or parent.
        document["sensors"][0]["streams"][0]["width"] = 640;
        auto edited = encode_intent_json(document.dump(), status);
        auto editedRecords = tlv::body(edited);
        auto editedChildren = tlv::parse(editedRecords[1].value);
        auto editedAttributes = tlv::parse(editedChildren[2].value);
        for (auto &a : attributes) if (a.type == rsintent::WIDTH) a.value = {0x80, 2};
        children[2].value = tlv::pack(attributes);
        records[1].value = tlv::pack(children);
        assert(edited == tlv::transfer(base, records));
        assert(editedAttributes.size() == attributes.size());
        auto bad = json::parse(text);
        bad["layout"][0]["flags"] = 4; reject(bad);
        bad = json::parse(text); bad["layout"][0]["hex"] = "0g"; reject(bad);
        bad = json::parse(text); bad["layout"][0]["hex"] = "0"; reject(bad);
        bad = json::parse(text); bad["layout"][0]["extra"] = 0; reject(bad);
        bad = json::parse(text); bad["layout"].back()["id"] = 221; reject(bad);
        bad = json::parse(text); bad["layout"].erase(1); reject(bad);
        bad = json::parse(text); bad["layout"].push_back(bad["layout"][1]); reject(bad);
        bad = json::parse(text); bad["layout"][0]["hex"] = std::string(2032, '0'); reject(bad);
        // Unknowns have only a source minor; do not send them to an older target.
        auto older = status; older[1] = 0;
        auto omitted = encode_intent_json(text, older);
        auto expected = tlv::body(base);
        expected[0].flags = 3;
        auto expectedChildren = tlv::parse(expected[0].value);
        auto expectedAttributes = tlv::parse(expectedChildren[1].value);
        std::reverse(expectedAttributes.begin(), expectedAttributes.end());
        expectedChildren[1].value = tlv::pack(expectedAttributes);
        expected[0].value = tlv::pack(expectedChildren);
        assert(omitted == tlv::transfer(base, expected));
        warnings.contains("opaque record type 225");
        warnings.contains("opaque record type 226");
        warnings.contains("opaque record type 227");
        warnings.contains("opaque record type 32 id 99");
        warnings.contains("opaque record type 16 id 220");
    }
    auto mismatch = status; mismatch[0] = 2;
    try { (void)encode_intent_json(decode_intent_json(base, status), mismatch); }
    catch (const std::invalid_argument &) { return; }
    assert(!"major mismatch accepted");
}
