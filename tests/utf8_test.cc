#include <gtest/gtest.h>
#include <string_view>
#include <util/utf8.hh>

using namespace dcc::utf8;

TEST(Utf8, DecodeValid)
{
    auto r = decode_one("A");
    ASSERT_TRUE(r);
    EXPECT_EQ(r->codepoint, 0x0041);
    EXPECT_EQ(r->bytes_consumed, 1);

    r = decode_one("\xC2\xA2");
    ASSERT_TRUE(r);
    EXPECT_EQ(r->codepoint, 0x00A2);
    EXPECT_EQ(r->bytes_consumed, 2);

    r = decode_one("\xE2\x82\xAC");
    ASSERT_TRUE(r);
    EXPECT_EQ(r->codepoint, 0x20AC);
    EXPECT_EQ(r->bytes_consumed, 3);

    r = decode_one("\xF0\x90\x8D\x88");
    ASSERT_TRUE(r);
    EXPECT_EQ(r->codepoint, 0x10348);
    EXPECT_EQ(r->bytes_consumed, 4);
}

TEST(Utf8, DecodeErrors)
{
    EXPECT_EQ(decode_one("").error(), Error::TruncatedSequence);
    EXPECT_EQ(decode_one("\xC2").error(), Error::TruncatedSequence);
    EXPECT_EQ(decode_one("\xE2\x82").error(), Error::TruncatedSequence);

    EXPECT_EQ(decode_one("\x80").error(), Error::UnexpectedContinuation);
    EXPECT_EQ(decode_one("\xBF").error(), Error::UnexpectedContinuation);

    EXPECT_EQ(decode_one("\xFF").error(), Error::InvalidByte);
    EXPECT_EQ(decode_one("\xFE").error(), Error::InvalidByte);

    EXPECT_EQ(decode_one("\xC0\xAF").error(), Error::OverlongEncoding);
    EXPECT_EQ(decode_one("\xE0\x80\xAF").error(), Error::OverlongEncoding);

    EXPECT_EQ(decode_one("\xED\xA0\x80").error(), Error::InvalidCodepoint);
    EXPECT_EQ(decode_one("\xED\xBF\xBF").error(), Error::InvalidCodepoint);

    EXPECT_EQ(decode_one("\xF4\x90\x80\x80").error(), Error::InvalidCodepoint);
}

TEST(Utf8, CodepointWidth)
{
    EXPECT_EQ(codepoint_width(0x0041), 1);

    EXPECT_EQ(codepoint_width(0x0300), 0);

    EXPECT_EQ(codepoint_width(0x4E2D), 2);

    EXPECT_EQ(codepoint_width(0x1F600), 2);
}

TEST(Utf8, StringWidth)
{
    std::string_view sv = "A\xE4\xB8\xAD\xF0\x9F\x98\x80";

    auto width_res = string_width(sv);
    ASSERT_TRUE(width_res);
    EXPECT_EQ(*width_res, 5);

    auto info_res = string_width_info(sv);
    ASSERT_TRUE(info_res);
    EXPECT_EQ(info_res->columns, 5);
    EXPECT_EQ(info_res->codepoints, 3);
    EXPECT_EQ(info_res->bytes, 1 + 3 + 4);
}

TEST(Utf8, StringWidthError)
{
    std::string_view sv = "A\x80";
    auto width_res = string_width(sv);
    ASSERT_FALSE(width_res);
    EXPECT_EQ(width_res.error(), Error::UnexpectedContinuation);
}

TEST(Utf8, Iterator)
{
    std::string_view sv = "A\xC2\xA2\xE2\x82\xAC";
    auto range = codepoints(sv);
    auto it = range.begin();

    ASSERT_NE(it, range.end());
    EXPECT_TRUE(*it);
    EXPECT_EQ((*it)->codepoint, 0x0041);
    EXPECT_EQ(it.byte_offset(), 0);

    ++it;
    ASSERT_NE(it, range.end());
    EXPECT_TRUE(*it);
    EXPECT_EQ((*it)->codepoint, 0x00A2);
    EXPECT_EQ(it.byte_offset(), 1);

    ++it;
    ASSERT_NE(it, range.end());
    EXPECT_TRUE(*it);
    EXPECT_EQ((*it)->codepoint, 0x20AC);
    EXPECT_EQ(it.byte_offset(), 3);

    ++it;
    EXPECT_EQ(it, range.end());
}

TEST(Utf8, TruncateToWidth)
{
    std::string_view sv = "A\xE4\xB8\xAD\xF0\x9F\x98\x80";

    EXPECT_EQ(truncate_to_width(sv, 0), "");
    EXPECT_EQ(truncate_to_width(sv, 1), "A");
    EXPECT_EQ(truncate_to_width(sv, 2), "A");
    EXPECT_EQ(truncate_to_width(sv, 3), "A\xE4\xB8\xAD");
    EXPECT_EQ(truncate_to_width(sv, 4), "A\xE4\xB8\xAD");
    EXPECT_EQ(truncate_to_width(sv, 5), sv);
    EXPECT_EQ(truncate_to_width(sv, 10), sv);
}

TEST(Utf8, FitToWidth)
{
    std::string_view sv = "A\xE4\xB8\xAD";

    EXPECT_EQ(fit_to_width(sv, 3, '.'), "A\xE4\xB8\xAD");

    EXPECT_EQ(fit_to_width(sv, 5, '.'), "A\xE4\xB8\xAD..");

    EXPECT_EQ(fit_to_width(sv, 2, '.'), "A.");
}
