// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

//#cmake:add-file ../../common/assistant/assistant-sse-parser.cpp

#include <unit-tests/catch.h>
#include <common/assistant/assistant-sse-parser.h>

using rs2::assistant::sse_frame_parser;
using rs2::assistant::sse_event;
using rs2::assistant::sse_event_type;


namespace {

std::vector<sse_event> feed_and_collect(sse_frame_parser& parser, const std::string& data)
{
    std::vector<sse_event> events;
    parser.feed(data.data(), data.size(), [&](const sse_event& e) { events.push_back(e); });
    return events;
}

}  // namespace


TEST_CASE("assistant-sse-parser/single-frame", "[assistant]")
{
    sse_frame_parser parser;
    auto events = feed_and_collect(parser, "data: {\"type\":\"chunk\",\"content\":\"hi\"}\n\n");

    REQUIRE(events.size() == 1);
    CHECK(events[0].type == sse_event_type::chunk);
    CHECK(events[0].payload.value("content", std::string()) == "hi");
}

TEST_CASE("assistant-sse-parser/frame-split-across-feeds", "[assistant]")
{
    sse_frame_parser parser;
    std::vector<sse_event> events;
    auto on_event = [&](const sse_event& e) { events.push_back(e); };

    std::string part1 = "data: {\"type\":\"cha";
    std::string part2 = "t\",\"content\":\"hi\"}\n\n";

    parser.feed(part1.data(), part1.size(), on_event);
    CHECK(events.empty()); // no full frame yet

    parser.feed(part2.data(), part2.size(), on_event);
    REQUIRE(events.size() == 1);
    CHECK(events[0].payload.value("content", std::string()) == "hi");
}

TEST_CASE("assistant-sse-parser/multiple-frames-one-feed", "[assistant]")
{
    sse_frame_parser parser;
    auto events = feed_and_collect(parser,
        "data: {\"type\":\"chunk\",\"content\":\"a\"}\n\n"
        "data: {\"type\":\"chunk\",\"content\":\"b\"}\n\n"
        "data: {\"type\":\"done\"}\n\n");

    REQUIRE(events.size() == 3);
    CHECK(events[0].payload.value("content", std::string()) == "a");
    CHECK(events[1].payload.value("content", std::string()) == "b");
    CHECK(events[2].type == sse_event_type::done);
}

TEST_CASE("assistant-sse-parser/unknown-type", "[assistant]")
{
    sse_frame_parser parser;
    auto events = feed_and_collect(parser, "data: {\"type\":\"somethingNew\"}\n\n");

    REQUIRE(events.size() == 1);
    CHECK(events[0].type == sse_event_type::unknown);
}

TEST_CASE("assistant-sse-parser/malformed-json-does-not-crash", "[assistant]")
{
    sse_frame_parser parser;
    // Malformed frame is skipped (logged, not delivered); a valid frame right after it still is.
    auto events = feed_and_collect(parser,
        "data: {not valid json\n\n"
        "data: {\"type\":\"done\"}\n\n");

    REQUIRE(events.size() == 1);
    CHECK(events[0].type == sse_event_type::done);
}

TEST_CASE("assistant-sse-parser/buffer-overflow-cap", "[assistant]")
{
    sse_frame_parser parser;
    // A stalled/malformed reply that never sends "\n\n" must not grow the buffer forever.
    std::string junk(3 * 1024 * 1024, 'x'); // 3MB, over the 2MB cap, no frame terminator
    std::vector<sse_event> events;
    auto on_event = [&](const sse_event& e) { events.push_back(e); };

    bool ok = parser.feed(junk.data(), junk.size(), on_event);
    CHECK_FALSE(ok);
    CHECK(parser.overflowed());
    CHECK(events.empty());

    // Once overflowed, further feed() calls stay rejected until reset().
    ok = parser.feed("more", 4, on_event);
    CHECK_FALSE(ok);
}

TEST_CASE("assistant-sse-parser/reset-clears-overflow", "[assistant]")
{
    sse_frame_parser parser;
    std::string junk(3 * 1024 * 1024, 'x');
    parser.feed(junk.data(), junk.size(), [](const sse_event&) {});
    REQUIRE(parser.overflowed());

    parser.reset();
    CHECK_FALSE(parser.overflowed());

    auto events = feed_and_collect(parser, "data: {\"type\":\"done\"}\n\n");
    REQUIRE(events.size() == 1);
    CHECK(events[0].type == sse_event_type::done);
}
