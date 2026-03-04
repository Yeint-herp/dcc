#ifndef DCC_UTIL_UTF8_HH
#define DCC_UTIL_UTF8_HH

#include <cstdint>
#include <expected>
#include <span>
#include <string_view>

namespace dcc::utf8
{
    enum class Error : uint8_t
    {
        InvalidByte,
        UnexpectedContinuation,
        OverlongEncoding,
        InvalidCodepoint,
        TruncatedSequence,
    };
    [[nodiscard]] std::string_view to_string(Error e) noexcept;

    using Codepoint = char32_t;
    [[nodiscard]] int codepoint_width(Codepoint cp) noexcept;

    struct DecodeResult
    {
        Codepoint codepoint;
        int bytes_consumed;
    };

    [[nodiscard]] std::expected<DecodeResult, Error> decode_one(std::span<const char8_t> bytes) noexcept;
    [[nodiscard]] std::expected<DecodeResult, Error> decode_one(std::string_view sv) noexcept;

    class CodepointRange
    {
    public:
        class CodepointIterator
        {
        public:
            using value_type = std::expected<DecodeResult, Error>;
            using difference_type = std::ptrdiff_t;
            using iterator_category = std::forward_iterator_tag;

            explicit constexpr CodepointIterator(std::string_view sv, std::size_t pos = 0) noexcept : m_sv{sv}, m_pos{pos} {}

            [[nodiscard]] value_type operator*() const noexcept;
            CodepointIterator& operator++() noexcept;
            CodepointIterator operator++(int) noexcept;

            [[nodiscard]] constexpr bool operator==(const CodepointIterator&) const noexcept = default;
            [[nodiscard]] constexpr std::size_t byte_offset() const noexcept { return m_pos; }

        private:
            std::string_view m_sv;
            std::size_t m_pos{};
        };

        explicit constexpr CodepointRange(std::string_view sv) noexcept : m_sv{sv} {}
        [[nodiscard]] constexpr CodepointIterator begin() const noexcept { return CodepointIterator{m_sv}; }
        [[nodiscard]] constexpr CodepointIterator end() const noexcept { return CodepointIterator{m_sv, m_sv.size()}; }

    private:
        std::string_view m_sv;
    };

    [[nodiscard]] constexpr CodepointRange codepoints(std::string_view sv) noexcept
    {
        return CodepointRange{sv};
    }

    [[nodiscard]] std::expected<int, Error> string_width(std::string_view sv) noexcept;

    struct WidthInfo
    {
        int columns;
        std::size_t codepoints;
        std::size_t bytes;
    };

    [[nodiscard]] std::expected<WidthInfo, Error> string_width_info(std::string_view sv) noexcept;
    [[nodiscard]] std::string_view truncate_to_width(std::string_view sv, int max_columns) noexcept;
    [[nodiscard]] std::string fit_to_width(std::string_view sv, int width, char fill = ' ');

} // namespace dcc::utf8

#endif /* DCC_UTIL_UTF8_HH */
