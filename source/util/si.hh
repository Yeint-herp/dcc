#ifndef DCC_UTIL_SI_HH
#define DCC_UTIL_SI_HH

#include <cstddef>
#include <functional>
#include <memory>
#include <string_view>
#include <vector>

namespace dcc::si
{
    class InternedString
    {
    public:
        constexpr InternedString() noexcept = default;

        [[nodiscard]] constexpr std::string_view view() const noexcept { return m_sv; }
        [[nodiscard]] constexpr const char* data() const noexcept { return m_sv.data(); }
        [[nodiscard]] constexpr std::size_t size() const noexcept { return m_sv.size(); }
        [[nodiscard]] constexpr bool empty() const noexcept { return m_sv.empty(); }

        [[nodiscard]] constexpr explicit operator bool() const noexcept { return m_sv.data() != nullptr; }
        [[nodiscard]] constexpr operator std::string_view() const noexcept { return m_sv; }

        [[nodiscard]] constexpr bool operator==(InternedString other) const noexcept { return m_sv.data() == other.m_sv.data(); }

        [[nodiscard]] constexpr auto operator<=>(InternedString other) const noexcept { return m_sv.data() <=> other.m_sv.data(); }

    private:
        friend class StringInterner;
        std::string_view m_sv{};

        explicit constexpr InternedString(std::string_view sv) noexcept : m_sv{sv} {}
    };

    class StringInterner
    {
    public:
        explicit StringInterner(std::size_t page_size = 4096);
        ~StringInterner() = default;

        StringInterner(const StringInterner&) = delete;
        StringInterner& operator=(const StringInterner&) = delete;
        StringInterner(StringInterner&&) noexcept;
        StringInterner& operator=(StringInterner&&) noexcept;

        [[nodiscard]] InternedString intern(std::string_view text);
        [[nodiscard]] InternedString lookup(std::string_view text) const noexcept;

        [[nodiscard]] std::size_t size() const noexcept { return m_size; }
        [[nodiscard]] std::size_t arena_bytes() const noexcept;

    private:
        struct Page
        {
            std::unique_ptr<char[]> data;
            std::size_t capacity;
            std::size_t used;
        };

        std::vector<Page> m_pages;
        std::size_t m_page_size;

        [[nodiscard]] char* arena_alloc(std::size_t bytes);

        struct Slot
        {
            std::string_view key{};
            bool occupied{false};
        };

        std::vector<Slot> m_slots;
        std::size_t m_size{};

        static constexpr double max_load = 0.75;

        [[nodiscard]] static std::size_t hash(std::string_view sv) noexcept;
        [[nodiscard]] const Slot* find_slot(std::string_view text) const noexcept;
        void grow();
    };

} // namespace dcc::si

template <> struct std::hash<dcc::si::InternedString>
{
    std::size_t operator()(dcc::si::InternedString s) const noexcept { return std::hash<const void*>{}(s.data()); }
};

#endif /* DCC_UTIL_SI_HH */
