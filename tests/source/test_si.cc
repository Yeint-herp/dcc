import std;
import dcc.si;

#include "harness.hh"

SECTION("string_interner: basic");

TEST_CASE("intern returns non-empty view")
{
    dcc::si::string_interner si;
    auto sv = si.intern("hello");
    CHECK_EQ(sv, "hello");
}

TEST_CASE("intern deduplicates identical strings")
{
    dcc::si::string_interner si;
    auto a = si.intern("hello");
    auto b = si.intern("hello");
    CHECK_EQ(a.data(), b.data());
    CHECK_EQ(si.size(), 1u);
}

TEST_CASE("intern distinguishes different strings")
{
    dcc::si::string_interner si;
    auto a = si.intern("hello");
    auto b = si.intern("world");
    CHECK(a.data() != b.data());
    CHECK_EQ(si.size(), 2u);
}

TEST_CASE("empty string returns empty view")
{
    dcc::si::string_interner si;
    auto sv = si.intern("");
    CHECK(sv.empty());
    CHECK_EQ(si.size(), 0u);
}

TEST_CASE("find returns interned view")
{
    dcc::si::string_interner si;
    std::ignore = si.intern("needle");
    auto found = si.find("needle");
    CHECK_EQ(found, "needle");
}

TEST_CASE("find returns empty for missing string")
{
    dcc::si::string_interner si;
    std::ignore = si.intern("present");
    auto found = si.find("absent");
    CHECK(found.empty());
}

TEST_CASE("contains reports correctly")
{
    dcc::si::string_interner si;
    std::ignore = si.intern("yes");
    CHECK(si.contains("yes"));
    CHECK(!si.contains("no"));
    CHECK(!si.contains(""));
}

SECTION("string_interner: capacity");

TEST_CASE("custom initial capacity")
{
    dcc::si::string_interner si{16};
    CHECK(si.capacity() >= 16u);
    CHECK_EQ(si.size(), 0u);
    CHECK(si.empty());
}

TEST_CASE("capacity rounds up to power of two")
{
    dcc::si::string_interner si{17};
    CHECK_EQ(si.capacity(), 32u);
}

TEST_CASE("grows under load")
{
    dcc::si::string_interner si{16};
    auto initial_cap = si.capacity();

    for (int i = 0; i < 64; ++i)
        std::ignore = si.intern(std::string("key_") + std::to_string(i));

    CHECK_EQ(si.size(), 64u);
    CHECK(si.capacity() > initial_cap);

    for (int i = 0; i < 64; ++i)
        CHECK(si.contains(std::string("key_") + std::to_string(i)));
}

SECTION("string_interner: arena");

TEST_CASE("arena_bytes tracks storage")
{
    dcc::si::string_interner si;
    CHECK_EQ(si.arena_bytes(), 0u);

    std::ignore = si.intern("abc");
    CHECK_EQ(si.arena_bytes(), 3u);

    std::ignore = si.intern("abc");
    CHECK_EQ(si.arena_bytes(), 3u);

    std::ignore = si.intern("defgh");
    CHECK_EQ(si.arena_bytes(), 8u);
}

TEST_CASE("oversized string gets its own chunk")
{
    dcc::si::string_interner si;
    std::string big(8192, 'x');
    auto sv = si.intern(big);
    CHECK_EQ(sv.size(), 8192u);
    CHECK_EQ(sv, big);
}

SECTION("string_interner: move semantics");

TEST_CASE("move construction transfers ownership")
{
    dcc::si::string_interner a;
    std::ignore = a.intern("moved");

    dcc::si::string_interner b{std::move(a)};
    CHECK_EQ(b.size(), 1u);
    CHECK(b.contains("moved"));
    CHECK_EQ(a.size(), 0u);
}

TEST_CASE("move assignment transfers ownership")
{
    dcc::si::string_interner a;
    std::ignore = a.intern("src");

    dcc::si::string_interner b;
    std::ignore = b.intern("dst");

    b = std::move(a);
    CHECK(b.contains("src"));
    CHECK(!b.contains("dst"));
}

SECTION("string_interner: prefix/collision resilience");

TEST_CASE("prefix strings are distinct")
{
    dcc::si::string_interner si;
    auto a = si.intern("test");
    auto b = si.intern("testing");
    CHECK(a.data() != b.data());
    CHECK_EQ(a, "test");
    CHECK_EQ(b, "testing");
    CHECK_EQ(si.size(), 2u);
}
