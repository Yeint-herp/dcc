import std;
import dcc.sm;
import dcc.utf8;

#include "harness.hh"

namespace sm = dcc::sm;

SECTION("sm: lsp_position bounds checking");

TEST_CASE("lsp_position with offset past end returns OutOfRange")
{
    sm::SourceManager mgr;
    auto fid = mgr.open_in_memory("file:///test.dc", "hello world", 1);
    CHECK(fid != sm::FileId::Invalid);

    auto const* sf = mgr.get(fid);
    REQUIRE(sf != nullptr);
    CHECK_EQ(sf->size(), 11u);

    auto pos = sf->lsp_position(12);
    CHECK(!pos.has_value());
    CHECK_EQ(pos.error(), sm::Error::OutOfRange);

    pos = sf->lsp_position(9999);
    CHECK(!pos.has_value());
    CHECK_EQ(pos.error(), sm::Error::OutOfRange);
}

TEST_CASE("lsp_position with offset at size returns end position")
{
    sm::SourceManager mgr;
    auto fid = mgr.open_in_memory("file:///test.dc", "abc", 1);
    REQUIRE(fid != sm::FileId::Invalid);

    auto const* sf = mgr.get(fid);
    REQUIRE(sf != nullptr);
    CHECK_EQ(sf->size(), 3u);

    auto pos = sf->lsp_position(3);
    REQUIRE(pos.has_value());
    CHECK_EQ(pos->line, 0u);
    CHECK_EQ(pos->character, 3u);
}

TEST_CASE("lsp_position with valid offset returns Position")
{
    sm::SourceManager mgr;
    auto fid = mgr.open_in_memory("file:///test.dc", "hello\nworld", 1);
    REQUIRE(fid != sm::FileId::Invalid);

    auto const* sf = mgr.get(fid);
    REQUIRE(sf != nullptr);

    auto pos = sf->lsp_position(0);
    REQUIRE(pos.has_value());
    CHECK_EQ(pos->line, 0u);
    CHECK_EQ(pos->character, 0u);

    pos = sf->lsp_position(6);
    REQUIRE(pos.has_value());
    CHECK_EQ(pos->line, 1u);
    CHECK_EQ(pos->character, 0u);

    pos = sf->lsp_position(6);
    REQUIRE(pos.has_value());
    CHECK_EQ(pos->line, 1u);
    CHECK_EQ(pos->character, 0u);
}

TEST_CASE("lsp_position on empty file returns Position(0,0) for offset 0")
{
    sm::SourceManager mgr;
    auto fid = mgr.open_in_memory("file:///test.dc", "", 1);
    REQUIRE(fid != sm::FileId::Invalid);

    auto const* sf = mgr.get(fid);
    REQUIRE(sf != nullptr);
    CHECK_EQ(sf->size(), 0u);

    auto pos = sf->lsp_position(0);
    REQUIRE(pos.has_value());
    CHECK_EQ(pos->line, 0u);
    CHECK_EQ(pos->character, 0u);
}

TEST_CASE("offset_at_lsp_position with out-of-bounds line returns OutOfRange")
{
    sm::SourceManager mgr;
    auto fid = mgr.open_in_memory("file:///test.dc", "hello\nworld", 1);
    REQUIRE(fid != sm::FileId::Invalid);

    auto loc = mgr.lsp_position_to_location(fid, 100u, 0u);
    CHECK(!loc.has_value());
    CHECK_EQ(loc.error(), sm::Error::OutOfRange);
}

SECTION("sm: location_to_lsp_position robustness");

TEST_CASE("location_to_lsp_position on invalid FileId returns OutOfRange")
{
    sm::SourceManager mgr;
    sm::Location bad_loc{sm::FileId::Invalid, 0};
    auto pos = mgr.location_to_lsp_position(bad_loc);
    CHECK(!pos.has_value());
    CHECK_EQ(pos.error(), sm::Error::OutOfRange);
}

TEST_CASE("location_to_lsp_position on nonexistent FileId returns OutOfRange")
{
    sm::SourceManager mgr;
    sm::Location bad_loc{static_cast<sm::FileId>(999), 0};
    auto pos = mgr.location_to_lsp_position(bad_loc);
    CHECK(!pos.has_value());
    CHECK_EQ(pos.error(), sm::Error::OutOfRange);
}

SECTION("sm: semantic token cross-file range filtering");

TEST_CASE("SourceRanges from different files are distinguishable")
{
    sm::SourceManager mgr;
    auto fid_a = mgr.open_in_memory("file:///a.dc", "hello", 1);
    auto fid_b = mgr.open_in_memory("file:///b.dc", "world", 1);
    REQUIRE(fid_a != sm::FileId::Invalid);
    REQUIRE(fid_b != sm::FileId::Invalid);
    CHECK_NE(fid_a, fid_b);

    sm::SourceRange range_a{sm::Location{fid_a, 0}, sm::Location{fid_a, 1}};
    CHECK(range_a.valid());

    sm::SourceRange range_b{sm::Location{fid_b, 0}, sm::Location{fid_b, 1}};
    CHECK(range_b.valid());

    sm::FileId const requested = fid_a;
    CHECK_EQ(range_a.begin.fileId, requested);
    CHECK_EQ(range_a.end.fileId, requested);
    CHECK_NE(range_b.begin.fileId, requested);
    CHECK_NE(range_b.end.fileId, requested);
}

TEST_CASE("SourceRanges with invalid file ids are not valid")
{
    sm::SourceRange invalid_range{sm::Location{sm::FileId::Invalid, 0}, sm::Location{sm::FileId::Invalid, 1}};
    CHECK(!invalid_range.valid());

    sm::SourceRange mixed_range{sm::Location{sm::FileId::Invalid, 0}, sm::Location{static_cast<sm::FileId>(1), 1}};
    CHECK(!mixed_range.valid());
}

TEST_CASE("SourceRange with begin > end is not valid")
{
    sm::SourceManager mgr;
    auto fid = mgr.open_in_memory("file:///t.dc", "abcdef", 1);
    REQUIRE(fid != sm::FileId::Invalid);

    sm::SourceRange reversed{sm::Location{fid, 3}, sm::Location{fid, 1}};
    CHECK(!reversed.valid());
}

TEST_CASE("location_to_lsp_position returns OutOfRange for offset past end")
{
    sm::SourceManager mgr;
    auto fid = mgr.open_in_memory("file:///t.dc", "hi", 1);
    REQUIRE(fid != sm::FileId::Invalid);

    sm::Location bad{fid, 99};
    auto pos = mgr.location_to_lsp_position(bad);
    CHECK(!pos.has_value());
    CHECK_EQ(pos.error(), sm::Error::OutOfRange);

    sm::Location ok{fid, 1};
    pos = mgr.location_to_lsp_position(ok);
    REQUIRE(pos.has_value());
}

SECTION("sm: update_in_memory failure handling");

TEST_CASE("update_in_memory on nonexistent URI returns FileNotFound")
{
    sm::SourceManager mgr;
    auto result = mgr.update_in_memory("file:///nonexistent.dc", "content", 1);
    CHECK(!result.has_value());
    CHECK_EQ(result.error(), sm::Error::FileNotFound);
}

TEST_CASE("update_in_memory on disk-loaded file returns PermissionDenied")
{
    auto temp_file = std::filesystem::temp_directory_path() / "dcc_test_permission.tmp";

    std::error_code ec;
    std::filesystem::remove(temp_file, ec);

    {
        std::ofstream ofs(temp_file);
        REQUIRE(ofs.is_open());
        ofs << "module test;";
    }
    CHECK(std::filesystem::file_size(temp_file) > 0);

    sm::SourceManager mgr;
    auto load_result = mgr.load(temp_file);
    REQUIRE(load_result.has_value());
    sm::FileId fid = *load_result;

    auto const* sf = mgr.get(fid);
    REQUIRE(sf != nullptr);
    CHECK_EQ(sf->kind(), sm::FileKind::Disk);

    auto uri = sf->uri();
    CHECK(!uri.empty());

    auto result = mgr.update_in_memory(uri, "new content", 2);
    CHECK(!result.has_value());
    CHECK_EQ(result.error(), sm::Error::PermissionDenied);

    std::filesystem::remove(temp_file, ec);
}

TEST_CASE("open_in_memory and update_in_memory round-trip preserves content")
{
    sm::SourceManager mgr;
    auto fid = mgr.open_in_memory("file:///test.dc", "original", 1);
    REQUIRE(fid != sm::FileId::Invalid);

    auto const* sf = mgr.get(fid);
    REQUIRE(sf != nullptr);
    CHECK_EQ(sf->text(), "original");

    auto result = mgr.update_in_memory("file:///test.dc", "modified", 2);
    CHECK(result.has_value());

    CHECK_EQ(sf->text(), "modified");
    CHECK_EQ(sf->version(), 2);
}

TEST_CASE("close_in_memory preserves InMemory kind and allows subsequent update")
{
    sm::SourceManager mgr;
    auto fid = mgr.open_in_memory("file:///test.dc", "initial", 1);
    REQUIRE(fid != sm::FileId::Invalid);

    auto close_result = mgr.close_in_memory("file:///test.dc");
    CHECK(close_result.has_value());

    auto result = mgr.update_in_memory("file:///test.dc", "after-close", 2);
    CHECK(result.has_value());

    auto const* sf = mgr.get(fid);
    REQUIRE(sf != nullptr);
    CHECK_EQ(sf->text(), "after-close");
    CHECK_EQ(sf->version(), 2);
}
