#include <gtest/gtest.h>
#include <tuple>
#include <util/si.hh>

using namespace dcc::si;

namespace dcc::test
{
    TEST(StringInterner, InternAndLookup)
    {
        StringInterner interner;

        InternedString str1 = interner.intern("test");
        InternedString str2 = interner.intern("test");
        InternedString str3 = interner.intern("example");

        EXPECT_EQ(str1, str2);
        EXPECT_NE(str1, str3);
        EXPECT_EQ(str1.view(), "test");
        EXPECT_EQ(str3.view(), "example");
    }

    TEST(StringInterner, LookupNonExistent)
    {
        StringInterner interner;

        InternedString str1 = interner.lookup("test");
        EXPECT_FALSE(str1);

        InternedString str2 = interner.intern("test");
        InternedString str3 = interner.lookup("test");
        EXPECT_TRUE(str3);
        EXPECT_EQ(str2, str3);
    }

    TEST(StringInterner, LargeString)
    {
        StringInterner interner;
        std::string large_str(1024, 'a');

        InternedString str1 = interner.intern(large_str);
        InternedString str2 = interner.intern(large_str);

        EXPECT_EQ(str1, str2);
        EXPECT_EQ(str1.view(), large_str);
    }

    TEST(StringInterner, HashFunction)
    {
        StringInterner interner;
        std::string str1 = "test";
        std::string str2 = "ttse";

        InternedString interned1 = interner.intern(str1);
        InternedString interned2 = interner.intern(str2);

        EXPECT_NE(interned1, interned2);
    }

    TEST(StringInterner, InternMultipleStrings)
    {
        StringInterner interner;
        std::vector<std::string> strings = {"hello", "world", "test", "example", "interner"};

        std::vector<InternedString> interned_strings;
        for (const auto& str : strings)
        {
            interned_strings.push_back(interner.intern(str));
        }

        for (std::size_t i = 0; i < strings.size(); ++i)
        {
            InternedString interned_str = interner.lookup(strings[i]);
            EXPECT_TRUE(interned_str);
            EXPECT_EQ(interned_str.view(), strings[i]);
            EXPECT_EQ(interned_str, interned_strings[i]);
        }
    }

    TEST(StringInterner, Size)
    {
        StringInterner interner;
        EXPECT_EQ(interner.size(), 0);

        std::ignore = interner.intern("test");
        EXPECT_EQ(interner.size(), 1);

        std::ignore = interner.intern("example");
        EXPECT_EQ(interner.size(), 2);

        std::ignore = interner.intern("test");
        EXPECT_EQ(interner.size(), 2);
    }

    TEST(StringInterner, ArenaBytes)
    {
        StringInterner interner(128);
        std::ignore = interner.intern("test");
        std::ignore = interner.intern("example");
        std::ignore = interner.intern("interner");

        EXPECT_GE(interner.arena_bytes(), 128);
    }

} // namespace dcc::test
