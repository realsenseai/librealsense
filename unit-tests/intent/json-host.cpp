// License: Apache 2.0. Copyright (C) 2026 RealSense, Inc.
#include "src/intent/intent-json.h"
#include "src/intent/contract/IntentProtocol.h"
#include <algorithm>
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
#include <rsutils/json.h>
extern "C" int fw_accept(const unsigned char *, unsigned);
extern "C" void fw_template(unsigned, unsigned char *);
extern "C" int fw_has_rules();
extern "C" int fw_decode(const unsigned char *, unsigned, unsigned *, unsigned *);
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
static std::vector<uint8_t> unhex(const std::string &text, size_t size) {
    std::vector<uint8_t> bytes(size);
    assert(text.size() % 2U == 0U && text.size() / 2U <= size);
    for (size_t i = 0; i < text.size() / 2U; ++i)
        bytes[i] = static_cast<uint8_t>(std::stoul(text.substr(2U * i, 2U), nullptr, 16));
    return bytes;
}
static bool sdkRejects(const std::vector<uint8_t> &wire, const std::vector<uint8_t> &status) {
    try {
        (void)decode_intent_json(wire, status);
    } catch (const std::exception &) {
        return true;
    }
    return false;
}
// Compare only the contract part of a decoded document (status is device state).
static void stripLayout(json &j) {
    if (j.is_object()) {
        j.erase("layout");
        for (auto &child : j) stripLayout(child);
    } else if (j.is_array()) for (auto &child : j) stripLayout(child);
}
static void sameDocument(const std::string &name, json decoded, const json &expected) {
    stripLayout(decoded);
    if (decoded.at("version") != expected.at("version") ||
        decoded.at("sensors") != expected.at("sensors")) {
        std::cerr << name << ": SDK decoded " << decoded.dump() << "\nexpected " << expected.dump()
                  << "\n";
        assert(false);
    }
}
// Golden vectors from the FW contract export (independent Python encoder).
static void goldenVectors(const char *path, std::vector<uint8_t> &status) {
    std::ifstream file(path);
    const json doc = json::parse(std::string((std::istreambuf_iterator<char>(file)), {}));
    assert(doc.at("major") == rsintent::major && doc.at("wire_size") == rsintent::wire_size);
    unsigned sdk_encoded = 0, fw_rejected = 0, templates = 0, stored = 0;
    for (const auto &v : doc.at("vectors")) {
        const std::string name = v.at("name");
        const std::string kind = v.at("kind");
        const json &expect = v.at("expect");
        if (kind == "decode") {
            const size_t size = v.at("size");
            auto bytes = unhex(v.at("hex"), std::max<size_t>(size, 1U));
            bytes.resize(size);
            unsigned detail = 0, offset = 0;
            const int fw = fw_decode(bytes.data(), static_cast<unsigned>(size), &detail, &offset);
            rsintent::Request request = {};
            const auto sdk = rsintent::decode(bytes.data(), size, request);
            if (fw != expect.at("code").get<int>() || sdk.code != fw ||
                (fw != 0 && (detail != expect.at("detail") || offset != expect.at("offset") ||
                             sdk.detail != detail || sdk.offset != offset))) {
                std::cerr << name << ": FW " << fw << "/" << detail << "@" << offset << " SDK "
                          << sdk.code << "/" << sdk.detail << "@" << sdk.offset << "\n";
                assert(false);
            }
            if (fw != 0) {
                ++fw_rejected;
                if (detail != rsintent::PARSE_UNKNOWN_CRITICAL && name != "sensor_id_out_of_range" &&
                    name != "branch_out_of_range" && name != "too_many_streams" && !sdkRejects(bytes, status)) {
                    std::cerr << name << ": SDK unexpectedly accepted malformed payload\n"; assert(false);
                }
                continue;
            }
            if (!v.contains("sdk_document"))
                continue;
            // SDK JSON encoder must produce exactly the golden transfer FW accepts.
            const json &document = v.at("sdk_document");
            const auto encoded = encode_intent_json(document.dump());
            if (encoded != bytes) {
                std::cerr << name << ": SDK encoding differs from golden\n";
                assert(false);
            }
            ++sdk_encoded;
            if (document.at("op") == "SET")
                sameDocument(name, json::parse(decode_intent_json(bytes, status)), document);
        } else if (kind == "controller") {
            const auto bytes = unhex(v.at("hex"), rsintent::wire_size);
            if (fw_has_rules()) assert(fw_accept(bytes.data(), v.at("sku")) == expect.at("code"));
        } else {
            const unsigned sku = v.at("sku");
            const auto golden = unhex(v.at("hex"), rsintent::wire_size);
            if (kind == "template") {
                std::vector<uint8_t> fw(rsintent::wire_size);
                fw_template(sku, fw.data());
                assert(fw == golden);
                ++templates;
            } else {
                assert(kind == "stored");
                ++stored;
            }
            // FW GET response -> SDK JSON -> SDK encode is lossless.
            const json decoded = json::parse(decode_intent_json(golden, status));
            sameDocument(name, decoded, v.at("sdk_document"));
            assert(encode_intent_json(v.at("sdk_document").dump()) == golden);
            assert(fw_accept(golden.data(), sku) == 0);
        }
    }
    assert(sdk_encoded >= 8U && fw_rejected >= 40U && templates == 3U && stored == 3U);
    std::cout << "golden: " << sdk_encoded << " SDK-encoded accepted by FW, " << fw_rejected
              << " rejected identically by FW and SDK codec, " << templates << " templates, "
              << stored << " stored GET decoded by SDK\n";
}
void compatibility();
int main(int argc, char **argv) {
    compatibility();
    assert(argc == 2);
    std::vector<uint8_t> status(rsintent::status_size);
    status[0] = 1;
    goldenVectors(argv[1], status);
    json depth = {
        {"branch", "DEPTH"}, {"format", "Z16"}, {"width", 1280}, {"height", 720}, {"fps", 30}};
    json document = {
        {"version", {{"major", 1}, {"minor", 0}}},
        {"op", "SET"},
        {"sensors", json::array({{{"sensor", "STEREO"}, {"streams", json::array({depth})}}})}};
    auto partial = encode_intent_json(document.dump());
    if (fw_has_rules()) assert(fw_accept(partial.data(), 0xc08) == rsintent::INCOMPLETE);
    document["sensors"][0]["streams"].push_back({{"branch", "IR_LR"}, {"state", "DISABLED"}});
    document["sensors"].push_back({{"sensor", "COLOR"}, {"streams", json::array({{{"branch", "RGB"}, {"state", "DISABLED"}}})}});
    auto bytes = encode_intent_json(document.dump());
    assert(fw_accept(bytes.data(), 0xc08) == 0);
    // ver_patch (byte 7): optional in JSON, passes the FW version gate, round-trips on GET.
    assert(bytes[7] == rsintent::patch);
    auto patched = document;
    patched["version"]["patch"] = 9;
    auto patchedBytes = encode_intent_json(patched.dump());
    assert(patchedBytes[7] == 9 && fw_accept(patchedBytes.data(), 0xc08) == 0);
    assert(json::parse(decode_intent_json(patchedBytes, status)).at("version").at("patch") == 9);
    auto bad = document;
    bad["version"]["patch"] = 256;
    rejected(bad);
    bad = document;
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
    if (fw_has_rules())
        assert(fw_accept(bytes.data(), 0xc08) == 4); // FW rules authoritative.
    bad = document;
    bad["op"] = "VALIDATE_ONLY";
    assert(encode_intent_json(bad.dump())[1] == 1);
    bytes = encode_intent_json(R"({"op":"CLEAR"})");
    assert(bytes[1] == 2 && bytes[2] == 0);
    rejected(json{{"op", "CLEAR"}, {"sensors", json::array()}});
    fw_template(0xc07, bytes.data());
    bytes[6] = 1;
    assert(encode_intent_json(decode_intent_json(bytes, status)) == bytes);
    std::cout << "PASS: golden vectors both directions, JSON rejection and FW admission rules\n";
}
