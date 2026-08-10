// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2026 RealSense, Inc. All Rights Reserved.

//#cmake:add-file ../../common/assistant/assistant-markdown-parser.cpp

#include <unit-tests/catch.h>
#include <common/assistant/assistant-markdown.h>

using rs2::assistant_detail::parse_markdown;
using rs2::assistant_detail::markdown_line;


namespace {

std::string words_to_text(const markdown_line& line)
{
    std::string out;
    for (auto&& w : line.words)
    {
        if (!out.empty()) out += " ";
        out += w.text;
    }
    return out;
}

}  // namespace


TEST_CASE("assistant-markdown/heading", "[assistant]")
{
    auto lines = parse_markdown("# Title");
    REQUIRE(lines.size() == 1);
    CHECK(lines[0].kind == markdown_line::kind::heading);
    CHECK(words_to_text(lines[0]) == "Title");
}

TEST_CASE("assistant-markdown/bullet", "[assistant]")
{
    auto lines = parse_markdown("- item text");
    REQUIRE(lines.size() == 1);
    CHECK(lines[0].kind == markdown_line::kind::bullet);
    CHECK(words_to_text(lines[0]) == "item text"); // bullet marker itself isn't a word
}

TEST_CASE("assistant-markdown/rule", "[assistant]")
{
    auto lines = parse_markdown("---");
    REQUIRE(lines.size() == 1);
    CHECK(lines[0].kind == markdown_line::kind::rule);
}

TEST_CASE("assistant-markdown/whitespace-only-line-is-blank-not-rule", "[assistant]")
{
    // Regression: a line of pure spaces used to satisfy the same "-*_ " character class as a
    // rule (---), rendering an unintended ImGui::Separator() where the model meant a blank gap.
    auto lines = parse_markdown("text one\n\n   \ntext two");
    for (auto&& line : lines)
        CHECK(line.kind != markdown_line::kind::rule);
}

TEST_CASE("assistant-markdown/blank-line", "[assistant]")
{
    auto lines = parse_markdown("");
    REQUIRE(lines.size() == 1);
    CHECK(lines[0].kind == markdown_line::kind::blank);
}

TEST_CASE("assistant-markdown/soft-wrap-merges-into-one-line", "[assistant]")
{
    // A '\n' with no blank/heading/rule/bullet on either side is a soft mid-sentence wrap, not a
    // paragraph break - it should merge into one logical line so word-wrap re-flows it cleanly.
    auto lines = parse_markdown("line one\nline two");
    REQUIRE(lines.size() == 1);
    CHECK(lines[0].kind == markdown_line::kind::text);
    CHECK(words_to_text(lines[0]) == "line one line two");
}

TEST_CASE("assistant-markdown/blank-line-prevents-merge", "[assistant]")
{
    auto lines = parse_markdown("para one\n\npara two");
    REQUIRE(lines.size() == 3);
    CHECK(words_to_text(lines[0]) == "para one");
    CHECK(lines[1].kind == markdown_line::kind::blank);
    CHECK(words_to_text(lines[2]) == "para two");
}

TEST_CASE("assistant-markdown/markdown-link", "[assistant]")
{
    auto lines = parse_markdown("[label](https://example.com)");
    REQUIRE(lines.size() == 1);
    REQUIRE(lines[0].words.size() == 1);
    CHECK(lines[0].words[0].text == "label");
    CHECK(lines[0].words[0].is_link);
    CHECK(lines[0].words[0].url == "https://example.com");
}

TEST_CASE("assistant-markdown/bare-url", "[assistant]")
{
    auto lines = parse_markdown("see https://example.com for details");
    REQUIRE(lines.size() == 1);

    bool found_link = false;
    for (auto&& w : lines[0].words)
    {
        if (w.is_link)
        {
            found_link = true;
            CHECK(w.text == "https://example.com");
            CHECK(w.url == "https://example.com");
        }
    }
    CHECK(found_link);
}

TEST_CASE("assistant-markdown/bold-markers-stripped-not-styled", "[assistant]")
{
    auto lines = parse_markdown("**bold** text");
    REQUIRE(lines.size() == 1);
    CHECK(words_to_text(lines[0]) == "bold text");
    for (auto&& w : lines[0].words)
        CHECK_FALSE(w.is_link);
}
