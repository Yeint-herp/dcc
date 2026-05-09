#pragma once

namespace test
{
    struct Stats
    {
        int checks = 0;
        int failed = 0;
        bool case_failed = false;
    };

    Stats& stats()
    {
        static Stats s;
        return s;
    }

    struct Case
    {
        std::string_view section;
        std::string_view name;
        std::function<void()> fn;
    };

    std::vector<Case>& registry()
    {
        static std::vector<Case> cases;
        return cases;
    }

    std::string_view& current_section()
    {
        static std::string_view s;
        return s;
    }

    struct Register
    {
        Register(std::string_view name, void (*fn)()) { registry().push_back({current_section(), name, fn}); }
    };

    struct Section
    {
        Section(std::string_view name) { current_section() = name; }
    };

    void report_fail(std::string_view msg, std::source_location loc)
    {
        ++stats().checks;
        ++stats().failed;
        stats().case_failed = true;

        std::println(std::cerr, "    FAIL  {}  ({}:{})", msg, loc.file_name(), loc.line());
    }

    void report_pass()
    {
        ++stats().checks;
    }

    bool check(bool ok, std::string_view expr, std::source_location loc = std::source_location::current())
    {
        if (ok)
        {
            report_pass();
            return true;
        }
        report_fail(expr, loc);
        return false;
    }

    template <typename T>
    concept Printable = std::formattable<T, char>;

    template <typename A, typename B>
        requires std::equality_comparable_with<A, B>
    void check_eq(const A& a, const B& b, std::string_view expr_a, std::string_view expr_b, std::source_location loc = std::source_location::current())
    {
        ++stats().checks;
        if (a == b)
            return;

        ++stats().failed;
        stats().case_failed = true;

        std::println(std::cerr, "    FAIL  {} == {}  ({}:{})", expr_a, expr_b, loc.file_name(), loc.line());

        if constexpr (Printable<A> && Printable<B>)
        {
            std::println(std::cerr, "          lhs: {}", a);
            std::println(std::cerr, "          rhs: {}", b);
        }
    }

    template <typename A, typename B>
        requires std::equality_comparable_with<A, B>
    void check_ne(const A& a, const B& b, std::string_view expr_a, std::string_view expr_b, std::source_location loc = std::source_location::current())
    {
        ++stats().checks;
        if (a != b)
            return;

        ++stats().failed;
        stats().case_failed = true;

        std::println(std::cerr, "    FAIL  {} != {}  ({}:{})", expr_a, expr_b, loc.file_name(), loc.line());

        if constexpr (Printable<A>)
            std::println(std::cerr, "          both: {}", a);
    }

    template <typename A, typename B>
        requires std::totally_ordered_with<A, B>
    void check_lt(const A& a, const B& b, std::string_view expr_a, std::string_view expr_b, std::source_location loc = std::source_location::current())
    {
        ++stats().checks;
        if (a < b)
            return;

        ++stats().failed;
        stats().case_failed = true;

        std::println(std::cerr, "    FAIL  {} < {}  ({}:{})", expr_a, expr_b, loc.file_name(), loc.line());

        if constexpr (Printable<A> && Printable<B>)
        {
            std::println(std::cerr, "          lhs: {}", a);
            std::println(std::cerr, "          rhs: {}", b);
        }
    }

    int run()
    {
        int failed = 0;
        std::string_view last_section;

        for (const auto& [section, name, fn] : registry())
        {
            if (section != last_section)
            {
                if (!section.empty())
                    std::println(" --- {} --- ", section);

                last_section = section;
            }

            stats().case_failed = false;
            fn();

            if (stats().case_failed)
            {
                std::println(std::cerr, "  FAIL  {}", name);
                ++failed;
            }
        }

        if (failed > 0)
        {
            std::println(std::cerr, "  RESULT  {}/{} checks failed", stats().failed, stats().checks);
            return 1;
        }

        std::println("  RESULT  {}/{} checks passed", stats().checks, stats().checks);
        return 0;
    }

} // namespace test

#define TEST_CONCAT_(a, b) a##b
#define TEST_CONCAT(a, b) TEST_CONCAT_(a, b)

#define TEST_CASE(name) TEST_CASE_IMPL_(name, __LINE__)
#define TEST_CASE_IMPL_(n, l)                                                                                                                                  \
    static void TEST_CONCAT(test_fn_, l)();                                                                                                                    \
    static ::test::Register TEST_CONCAT(test_reg_, l)(n, TEST_CONCAT(test_fn_, l));                                                                            \
    static void TEST_CONCAT(test_fn_, l)()

#define SECTION(name) [[maybe_unused]] static const int TEST_CONCAT(test_sec_, __LINE__) = (::test::current_section() = (name), 0)

#define CHECK(expr) ::test::check(static_cast<bool>(expr), #expr)

#define REQUIRE(expr)                                                                                                                                          \
    do                                                                                                                                                         \
    {                                                                                                                                                          \
        if (!::test::check(static_cast<bool>(expr), #expr))                                                                                                    \
            return;                                                                                                                                            \
    } while (0)

#define CHECK_EQ(a, b) ::test::check_eq((a), (b), #a, #b)
#define CHECK_NE(a, b) ::test::check_ne((a), (b), #a, #b)
#define CHECK_LT(a, b) ::test::check_lt((a), (b), #a, #b)

int main()
{
    return ::test::run();
}
