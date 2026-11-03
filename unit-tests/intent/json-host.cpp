// License: Apache 2.0. Copyright (C) 2026 RealSense, Inc.
#include "src/intent/intent-json.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
#include <rsutils/json.h>
extern "C" int fw_accept(const unsigned char *, unsigned);
extern "C" void fw_template(unsigned, unsigned char *);
using librealsense::decode_intent_json;
using librealsense::encode_intent_json;
using rsutils::json;
static void rejected(const json &value) {
    bool caught = false;
    try {
        (void)encode_intent_json(value.dump());
    } catch (const std::exception &) {
        caught = true;
    }
    assert(caught);
}
int main(int argc, char **argv) {
    assert(argc == 2);
    std::ifstream file(argv[1], std::ios::binary);
    std::vector<uint8_t> golden((std::istreambuf_iterator<char>(file)), {});
    json depth = {
        {"branch", "DEPTH"}, {"format", "Z16"}, {"width", 1280}, {"height", 720}, {"fps", 30}};
    json document = {
        {"version", {{"major", 1}, {"minor", 0}}},
        {"op", "SET"},
        {"sensors", json::array({{{"sensor", "STEREO"}, {"streams", json::array({depth})}}})}};
    auto bytes = encode_intent_json(document.dump());
    assert(bytes == golden); // Golden created independently with Python struct.pack.
    assert(fw_accept(bytes.data(), 0xc04) == 0 && fw_accept(bytes.data(), 0xc05) == 0);
    std::vector<uint8_t> status(32);
    status[0] = 1;
    assert(encode_intent_json(decode_intent_json(bytes, status)) == bytes);
    for (unsigned sku : {0xc04U, 0xc05U}) {
        fw_template(sku, bytes.data());
        assert(encode_intent_json(decode_intent_json(bytes, status)) == bytes);
        assert((unsigned(bytes[2]) | (unsigned(bytes[3]) << 8)) == (sku == 0xc04U ? 112U : 104U));
    }
    auto bad = document;
    bad["version"]["major"] = 2;
    rejected(bad);
    bad = document;
    bad["op"] = "UNKNOWN";
    rejected(bad);
    bad = document;
    bad["extra"] = 1;
    rejected(bad);
    bad = document;
    bad.erase("version");
    rejected(bad);
    for (const json &value : {json(-1), json(65536), json(1.5), json("30"), json(true),
                              json(18446744073709551615ULL)}) {
        bad = document;
        bad["sensors"][0]["streams"][0]["fps"] = value;
        rejected(bad);
    }
    bad = document;
    bad["sensors"][0]["streams"].push_back(depth);
    rejected(bad);
    bad = document;
    bad["sensors"].push_back(bad["sensors"][0]);
    rejected(bad);
    bad = document;
    bad["sensors"][0]["streams"][0]["state"] = "DISABLED";
    rejected(bad);
    bad = document;
    bad["sensors"][0]["sensor"] = "COLOR";
    rejected(bad);
    bad = document;
    bad["sensors"][0]["streams"][0]["branch"] = "FUTURE";
    rejected(bad);
    bad = document;
    bad["sensors"][0]["streams"][0]["fps"] = 15;
    bytes = encode_intent_json(bad.dump());
    assert(fw_accept(bytes.data(), 0xc04) == 4); // FW rules authoritative.
    bad = document;
    bad["op"] = "VALIDATE_ONLY";
    assert(encode_intent_json(bad.dump())[1] == 1);
    bytes = encode_intent_json(R"({"op":"CLEAR"})");
    assert(bytes[1] == 2 && bytes[2] == 0);
    rejected(json{{"op", "CLEAR"}, {"sensors", json::array()}});
    fw_template(0xc04, bytes.data());
    bytes[6] = 1;
    bool newer = false;
    try {
        decode_intent_json(bytes, status);
    } catch (const std::exception &) {
        newer = true;
    }
    assert(newer);
    std::cout << "PASS: independent golden vector, FW/SDK round trips, JSON rejection and FW "
                 "admission rules\n";
}
