import std;
import dcc.utf8;

#include "harness.hh"

namespace utf8 = dcc::utf8;

SECTION("utf8: decode_one");

TEST_CASE("ascii byte")
{
    auto r = utf8::decode_one("A");
    REQUIRE(r.has_value());
    CHECK_EQ(r->codepoint, U'A');
    CHECK_EQ(r->bytes_consumed, 1);
}

TEST_CASE("two-byte sequence")
{
    auto r = utf8::decode_one("\xc3\xa9");
    REQUIRE(r.has_value());
    CHECK_EQ(r->codepoint, U'\u00e9');
    CHECK_EQ(r->bytes_consumed, 2);
}

TEST_CASE("three-byte sequence")
{
    auto r = utf8::decode_one("\xe4\xb8\xad");
    REQUIRE(r.has_value());
    CHECK_EQ(r->codepoint, U'\u4e2d');
    CHECK_EQ(r->bytes_consumed, 3);
}

TEST_CASE("four-byte sequence")
{
    auto r = utf8::decode_one("\xf0\x9f\x98\x80");
    REQUIRE(r.has_value());
    CHECK_EQ(r->codepoint, U'\U0001f600');
    CHECK_EQ(r->bytes_consumed, 4);
}

TEST_CASE("overlong two-byte rejected")
{
    auto r = utf8::decode_one("\xc0\xaf");
    REQUIRE(!r.has_value());
    CHECK_EQ(r.error(), utf8::Error::OverlongEncoding);
}

TEST_CASE("surrogate half rejected")
{
    auto r = utf8::decode_one("\xed\xa0\x80");
    REQUIRE(!r.has_value());
    CHECK_EQ(r.error(), utf8::Error::InvalidCodepoint);
}

TEST_CASE("unexpected continuation byte")
{
    auto r = utf8::decode_one("\x80");
    REQUIRE(!r.has_value());
    CHECK_EQ(r.error(), utf8::Error::UnexpectedContinuation);
}

TEST_CASE("truncated sequence")
{
    auto r = utf8::decode_one("\xc3");
    REQUIRE(!r.has_value());
    CHECK_EQ(r.error(), utf8::Error::TruncatedSequence);
}

TEST_CASE("invalid lead byte 0xff")
{
    auto r = utf8::decode_one("\xff");
    REQUIRE(!r.has_value());
    CHECK_EQ(r.error(), utf8::Error::InvalidByte);
}

SECTION("utf8: codepoint_width");

TEST_CASE("ascii is width 1")
{
    CHECK_EQ(utf8::codepoint_width(U'A'), 1);
    CHECK_EQ(utf8::codepoint_width(U'~'), 1);
}

TEST_CASE("null is width 0")
{
    CHECK_EQ(utf8::codepoint_width(U'\0'), 0);
}

TEST_CASE("CJK is width 2")
{
    CHECK_EQ(utf8::codepoint_width(U'\u4e2d'), 2);
    CHECK_EQ(utf8::codepoint_width(U'\u3042'), 2);
}

TEST_CASE("combining marks are width 0")
{
    CHECK_EQ(utf8::codepoint_width(U'\u0301'), 0);
    CHECK_EQ(utf8::codepoint_width(U'\u0308'), 0);
}

TEST_CASE("zero-width joiners are width 0")
{
    CHECK_EQ(utf8::codepoint_width(U'\u200b'), 0);
    CHECK_EQ(utf8::codepoint_width(U'\u200d'), 0);
    CHECK_EQ(utf8::codepoint_width(U'\ufeff'), 0);
}

TEST_CASE("emoji is width 2")
{
    CHECK_EQ(utf8::codepoint_width(U'\U0001f600'), 2);
}

SECTION("utf8: string_width");

TEST_CASE("ascii string width")
{
    auto w = utf8::string_width("hello");
    REQUIRE(w.has_value());
    CHECK_EQ(*w, 5);
}

TEST_CASE("mixed ascii and CJK")
{
    auto w = utf8::string_width("a\xe4\xb8\xad");
    REQUIRE(w.has_value());
    CHECK_EQ(*w, 3);
}

TEST_CASE("empty string width is 0")
{
    auto w = utf8::string_width("");
    REQUIRE(w.has_value());
    CHECK_EQ(*w, 0);
}

TEST_CASE("invalid utf8 returns error")
{
    auto w = utf8::string_width("\xff\xfe");
    CHECK(!w.has_value());
}

SECTION("utf8: string_width_info");

TEST_CASE("width info counts everything")
{
    auto info = utf8::string_width_info("a\xc3\xa9");
    REQUIRE(info.has_value());
    CHECK_EQ(info->columns, 2);
    CHECK_EQ(info->codepoints, 2u);
    CHECK_EQ(info->bytes, 3u);
}

SECTION("utf8: truncate_to_width");

TEST_CASE("truncates ascii at column limit")
{
    auto sv = utf8::truncate_to_width("abcdef", 3);
    CHECK_EQ(sv, "abc");
}

TEST_CASE("does not split multibyte character")
{
    auto sv = utf8::truncate_to_width("a\xe4\xb8\xad", 2);
    CHECK_EQ(sv, "a");
}

TEST_CASE("exact fit includes character")
{
    auto sv = utf8::truncate_to_width("a\xe4\xb8\xad", 3);
    CHECK_EQ(sv, "a\xe4\xb8\xad");
}

SECTION("utf8: fit_to_width");

TEST_CASE("pads short string")
{
    auto s = utf8::fit_to_width("ab", 5);
    CHECK_EQ(s, "ab   ");
}

TEST_CASE("truncates long string")
{
    auto s = utf8::fit_to_width("abcdef", 3);
    CHECK_EQ(s, "abc");
}

SECTION("utf8: CodepointRange iteration");

TEST_CASE("iterates all codepoints")
{
    std::string_view input = "a\xc3\xa9\xe4\xb8\xad";
    int count = 0;
    for (auto r : utf8::codepoints(input))
    {
        REQUIRE(r.has_value());
        ++count;
    }

    CHECK_EQ(count, 3);
}

TEST_CASE("empty range yields no iterations")
{
    int count = 0;
    for ([[maybe_unused]] auto r : utf8::codepoints(""))
        ++count;

    CHECK_EQ(count, 0);
}

SECTION("utf8: error string coverage");

TEST_CASE("all error variants have names")
{
    CHECK(!utf8::to_string(utf8::Error::InvalidByte).empty());
    CHECK(!utf8::to_string(utf8::Error::UnexpectedContinuation).empty());
    CHECK(!utf8::to_string(utf8::Error::OverlongEncoding).empty());
    CHECK(!utf8::to_string(utf8::Error::InvalidCodepoint).empty());
    CHECK(!utf8::to_string(utf8::Error::TruncatedSequence).empty());
}
