// License: Apache 2.0. Copyright (C) 2026 RealSense, Inc.
#include "src/intent/intent-json.h"
#include "src/intent/contract/IntentProtocol.h"
#include "src/intent/contract/IntentStatus.h"
#include "src/intent/contract/IntentCapabilities.h"
#include <algorithm>
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
#include <rsutils/json.h>
extern "C" int fw_accept(const unsigned char *, unsigned);
extern "C" int fw_accept_on(const unsigned char *, unsigned, unsigned);
extern "C" void fw_template(unsigned, unsigned char *);
extern "C" int fw_has_rules();
extern "C" int fw_admission(const unsigned char *, unsigned, const unsigned *, unsigned char *);
extern "C" int fw_decode(const unsigned char *, unsigned, unsigned *, unsigned *);
using librealsense::decode_intent_json;
using librealsense::describe_intent_status;
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
static void nativeProfiles(const std::vector<uint8_t> &status) {
    if (!fw_has_rules()) return;
    unsigned tested = 0;
    for (const auto &p : rsintent::profiles) {
        std::vector<uint8_t> wire(rsintent::wire_size);
        fw_template(p.sku, wire.data());
        auto document = json::parse(decode_intent_json(wire, status));
        const char *name = nullptr;
        for (const auto &b : rsintent::branches)
            if (b.id == p.branch && b.sku == p.sku) name = b.name;
        assert(name);
        uint8_t bytes[4];
        rsintent::write32(bytes, p.format);
        std::string format(reinterpret_cast<char *>(bytes), 4U);
        while (!format.empty() && format.back() == ' ') format.pop_back();
        bool updated = false;
        for (auto &sensor : document["sensors"])
            for (auto &stream : sensor["streams"])
                if (stream["branch"] == name) {
                    stream = {{"branch", name}, {"format", format}, {"width", p.width},
                              {"height", p.height}, {"fps", p.fps}};
                    updated = true;
                }
        assert(updated);
        // No local rule-engine shortcut: SDK encodes, the actual FW T4 gate checks.
        wire = encode_intent_json(document.dump());
        assert(fw_accept_on(wire.data(), p.sku, p.transport) == rsintent::OK);
        assert(encode_intent_json(decode_intent_json(wire, status)) == wire);
        ++tested;
    }
    std::cout << "profiles: " << tested << " SDK-encoded native tuples accepted by FW\n";
}
static bool contains(const std::string &text, const char *part) {
    return text.find(part) != std::string::npos;
}
// The FW controller writes STATUS; the SDK decodes and names its REJECT_REASON list.
static void statusReasons() {
    std::vector<uint8_t> status(rsintent::status_size);
    status[0] = rsintent::major;
    status[3] = rsintent::STREAM_MISMATCH;
    status[rsintent::status_reason_count_offset] = 2U;
    rsintent::writeReason(status.data() + rsintent::status_header_size,
                          {rsintent::STREAM_MISMATCH, 1U, rsintent::no_record_index, 1U, 0U,
                           rsintent::MISMATCH_WIDTH | rsintent::MISMATCH_FPS});
    rsintent::writeReason(status.data() + rsintent::status_header_size + rsintent::reject_reason_size,
                          {rsintent::CONTROL_STATE, 2U, 3U, 7U, 0x101U, 2U});
    std::vector<uint8_t> wire(rsintent::wire_size);
    fw_template(0xc07, wire.data());
    auto reasons = json::parse(decode_intent_json(wire, status))["status"]["reasons"];
    assert(reasons.size() == 2U);
    assert((reasons[1] == json{{"kind", 7}, {"sensor", 2}, {"record_idx", 3}, {"branch", 7},
                               {"id", 0x101}, {"detail", 2}}));
    auto text = describe_intent_status(status);
    assert(contains(text, "last_result=STREAM_MISMATCH: STREAM_MISMATCH sensor=STEREO branch=DEPTH "
                          "mismatch=WIDTH|FPS; CONTROL_STATE sensor=COLOR branch=RGB record=3 id=257 "
                          "detail=2"));
    status[rsintent::status_reason_count_offset] = rsintent::max_reject_reasons + 1U;
    bool caught = false;
    try {
        decode_intent_json(wire, status);
    } catch (const std::invalid_argument &) {
        caught = true;
    }
    assert(caught && contains(describe_intent_status(status), "malformed reason count 125"));
    if (!fw_has_rules()) return;
    const rsintent::ProfileDefinition *depth = nullptr;
    for (const auto &p : rsintent::profiles)
        if (!depth && p.sku == 0xc07 && p.branch == 1U && p.transport == rsintent::USB3) depth = &p;
    assert(depth);
    uint8_t bytes[4];
    rsintent::write32(bytes, depth->format);
    std::string format(reinterpret_cast<char *>(bytes), 4U);
    while (!format.empty() && format.back() == ' ') format.pop_back();
    status[rsintent::status_reason_count_offset] = 0U;
    auto document = json::parse(decode_intent_json(wire, status));
    json *declared = nullptr;
    for (auto &sensor : document["sensors"])
        for (auto &stream : sensor["streams"])
            if (stream["branch"] == "DEPTH") {
                stream = {{"branch", "DEPTH"}, {"format", format}, {"width", depth->width},
                          {"height", depth->height}, {"fps", depth->fps}};
                declared = &stream;
            }
    assert(declared);
    wire = encode_intent_json(document.dump());
    // A1: one reason with every differing attribute.
    const unsigned changed[] = {1U, 1U, depth->format, depth->width + 2U, depth->height, depth->fps + 1U};
    assert(fw_admission(wire.data(), 0xc07, changed, status.data()) == rsintent::STREAM_MISMATCH);
    auto st = json::parse(decode_intent_json(wire, status))["status"];
    assert(st["last_result"] == rsintent::STREAM_MISMATCH && st["error_branch"] == 1);
    assert((st["reasons"] == json::array({json{{"kind", 6}, {"sensor", 1}, {"record_idx", 255},
                                              {"branch", 1}, {"id", 0}, {"detail", 8 | 32}}})));
    assert(contains(describe_intent_status(status), "branch=DEPTH mismatch=WIDTH|FPS"));
    // RGB (branch 7) does not exist on 2C, so it is undeclared.
    const unsigned rgb[] = {2U, 7U, depth->format, depth->width, depth->height, depth->fps};
    assert(fw_admission(wire.data(), 0xc07, rgb, status.data()) == rsintent::STREAM_MISMATCH);
    assert(contains(describe_intent_status(status), "sensor=COLOR branch=RGB mismatch=UNDECLARED"));
    // Admitted COMMIT clears the reasons.
    const unsigned same[] = {1U, 1U, depth->format, depth->width, depth->height, depth->fps};
    assert(fw_admission(wire.data(), 0xc07, same, status.data()) == rsintent::OK);
    assert(json::parse(decode_intent_json(wire, status))["status"]["reasons"].empty());
    // SET V1-V3: first error only (design 4.4.5).
    (*declared)["fps"] = 7;
    const auto bad = encode_intent_json(document.dump());
    assert(fw_admission(bad.data(), 0xc07, same, status.data()) == rsintent::CONSTRAINT);
    assert(status[rsintent::status_reason_count_offset] == 1U);
    assert(contains(describe_intent_status(status), "last_result=CONSTRAINT: CONSTRAINT"));
    std::cout << "status: FW REJECT_REASON list decoded and named by SDK\n";
}
static std::set<std::string> branchNames(const std::vector<uint8_t> &wire, const std::vector<uint8_t> &status) {
    std::set<std::string> names;
    const auto document = json::parse(decode_intent_json(wire, status));
    for (const auto &sensor : document["sensors"])
        for (const auto &stream : sensor["streams"]) names.insert(stream["branch"].get<std::string>());
    return names;
}
// Light D (design v0.4 section 7.10, HR-4): FW GET_CUR lists the sensors/branches it supports;
// the SDK omits anything else before SET.
static void lightD(const std::vector<uint8_t> &status) {
    std::vector<uint8_t> c3(rsintent::wire_size), c2(rsintent::wire_size);
    fw_template(0xc06, c3.data());
    fw_template(0xc07, c2.data());
    const auto document = decode_intent_json(c3, status);
    assert(encode_intent_json(document, status, {}) == encode_intent_json(document, status));
    // 3C declares sensor COLOR / branch RGB; a 2C FW does not report them.
    auto wire = encode_intent_json(document, status, c2);
    auto names = branchNames(wire, status);
    assert(!names.count("RGB") && names.count("DEPTH") && names.count("ACCEL"));
    unsigned detail = 0, offset = 0;
    assert(fw_decode(wire.data(), static_cast<unsigned>(wire.size()), &detail, &offset) == rsintent::OK);
    // A FW that reports STEREO without RGB_R: only that branch is omitted.
    auto model = rsintent::defaultModel(0xc07);
    size_t kept = 0;
    for (size_t i = 0; i < model.count; ++i)
        if (model.streams[i].branch != 6U) model.streams[kept++] = model.streams[i];
    model.count = kept;
    std::vector<uint8_t> partial(rsintent::wire_size);
    assert(rsintent::encode(model, rsintent::SET, partial.data(), partial.size()).code == rsintent::OK);
    wire = encode_intent_json(decode_intent_json(c2, status), status, partial);
    names = branchNames(wire, status);
    assert(!names.count("RGB_R") && names.count("RGB_L") && names.count("IR_LR"));
    assert(fw_decode(wire.data(), static_cast<unsigned>(wire.size()), &detail, &offset) == rsintent::OK);
    auto mismatch = c2;
    mismatch[0] = rsintent::major + 1U;
    bool caught = false;
    try {
        (void)encode_intent_json(document, status, mismatch);
    } catch (const std::invalid_argument &) {
        caught = true;
    }
    assert(caught);
    std::cout << "light D: items not reported by FW GET_CUR omitted before SET\n";
}
int main(int argc, char **argv) {
    compatibility();
    assert(argc == 2);
    std::vector<uint8_t> status(rsintent::status_size);
    status[0] = 1;
    goldenVectors(argv[1], status);
    nativeProfiles(status);
    statusReasons();
    lightD(status);
    json depth = {
        {"branch", "DEPTH"}, {"format", "Z16"}, {"width", 1280}, {"height", 720}, {"fps", 30}};
    json document = {
        {"version", "1.0.0"},
        {"op", "SET"},
        {"sensors", json::array({{{"sensor", "STEREO"}, {"streams", json::array({depth})}}})}};
    auto partial = encode_intent_json(document.dump());
    if (fw_has_rules()) assert(fw_accept(partial.data(), 0xc08) == rsintent::INCOMPLETE);
    document["sensors"][0]["streams"].push_back({{"branch", "IR_LR"}, {"state", "DISABLED"}});
    document["sensors"].push_back({{"sensor", "COLOR"}, {"streams", json::array({{{"branch", "RGB"}, {"state", "DISABLED"}}})}});
    document["sensors"].push_back({{"sensor", "MOTION"}, {"streams", json::array({
        {{"branch", "GYRO"}, {"state", "DISABLED"}}, {{"branch", "ACCEL"}, {"state", "DISABLED"}}})}});
    document["sensors"].push_back({{"sensor", "OBJECT_DETECTION"}, {"streams", json::array({
        {{"branch", "OD"}, {"state", "DISABLED"}}})}});
    document["sensors"].push_back({{"sensor", "DEPTH_MAPPING"}, {"streams", json::array({
        {{"branch", "DMAP"}, {"state", "DISABLED"}}})}});
    auto bytes = encode_intent_json(document.dump());
    assert(fw_accept(bytes.data(), 0xc08) == 0);
    for (unsigned code = 0; code <= 255U; ++code) {
        auto resultStatus = status;
        resultStatus[3] = static_cast<uint8_t>(code);
        assert(sdkRejects(bytes, resultStatus) == (code > rsintent::READ_ONLY));
        bool rejectedStatus = false;
        try { (void)encode_intent_json(document.dump(), resultStatus); }
        catch (const std::invalid_argument &) { rejectedStatus = true; }
        assert(rejectedStatus == (code > rsintent::READ_ONLY));
    }
    // All three bounded version components are required; newer minor/patch pass the FW gate.
    assert(bytes[7] == rsintent::patch);
    auto patched = document;
    patched["version"] = "1.255.255";
    auto patchedBytes = encode_intent_json(patched.dump());
    assert(patchedBytes[6] == 255 && patchedBytes[7] == 255 && fw_accept(patchedBytes.data(), 0xc08) == 0);
    assert(json::parse(decode_intent_json(patchedBytes, status)).at("version") == "1.255.255");
    auto bad = document;
    for (const json &value : {json(""), json("1"), json("1.0"), json("1.0.0.0"),
                              json("01.0.0"), json("1.00.0"), json("1.0.00"),
                              json("1.256.0"), json("1.0.256"), json("256.0.0"),
                              json("1.-1.0"), json("+1.0.0"), json(" 1.0.0"), json("1.0.0 "),
                              json("1..0"), json("1.0.0-beta"), json("1.0.0+build"),
                              json("1.0.99999999999999999999"), json("2.0.0"),
                              json(std::string("1.0.0\0", 6)), json(1), json(true), json(),
                              json::array(), json{{"major", 1}, {"minor", 0}, {"patch", 0}}}) {
        bad = document;
        bad["version"] = value;
        rejected(bad);
    }
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
    bad["sensors"][0]["streams"][0]["fps"] = 99;
    bytes = encode_intent_json(bad.dump());
    if (fw_has_rules())
        assert(fw_accept(bytes.data(), 0xc08) == 4); // FW rules authoritative.
    auto motion = document;
    motion["sensors"][2]["streams"][0] = {
        {"branch", "GYRO"}, {"format", "MXYZ"}, {"width", 0}, {"height", 0}, {"fps", 200}};
    bytes = encode_intent_json(motion.dump());
    assert(encode_intent_json(decode_intent_json(bytes, status)) == bytes);
    if (fw_has_rules()) assert(fw_accept(bytes.data(), 0xc08) == rsintent::OK);
    bad = motion;
    bad["sensors"][2]["streams"][0]["fps"] = 100;
    bytes = encode_intent_json(bad.dump());
    if (fw_has_rules()) assert(fw_accept(bytes.data(), 0xc08) == rsintent::CONSTRAINT);
    for (const char *field : {"width", "height"}) {
        bad = motion;
        bad["sensors"][2]["streams"][0][field] = 1;
        rejected(bad);
        bad = document;
        bad["sensors"][0]["streams"][0][field] = 0;
        rejected(bad);
    }
    bad = motion;
    bad["sensors"][2]["streams"][0]["fps"] = 0;
    rejected(bad);
    bad = document;
    bad["op"] = "VALIDATE_ONLY";
    assert(encode_intent_json(bad.dump())[1] == 1);
    bytes = encode_intent_json(R"({"op":"CLEAR"})");
    assert(bytes[1] == 2 && bytes[2] == 0);
    assert(encode_intent_json(R"({"version":"1.0.0","op":"CLEAR"})") == bytes);
    rejected(json{{"op", "CLEAR"}, {"sensors", json::array()}});
    fw_template(0xc07, bytes.data());
    bytes[6] = 1;
    assert(encode_intent_json(decode_intent_json(bytes, status)) == bytes);
    std::cout << "PASS: golden vectors both directions, JSON rejection and FW native profile validation\n";
}
