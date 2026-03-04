#include <string>
#include <util/utf8.hh>

namespace dcc::utf8
{
    std::string_view to_string(Error e) noexcept
    {
        switch (e)
        {
            case Error::InvalidByte:
                return "invalid byte";
            case Error::UnexpectedContinuation:
                return "unexpected continuation byte";
            case Error::OverlongEncoding:
                return "overlong encoding";
            case Error::InvalidCodepoint:
                return "invalid codepoint";
            case Error::TruncatedSequence:
                return "truncated sequence";
        }
        return "unknown";
    }

    int codepoint_width(Codepoint cp) noexcept
    {
        if (cp == 0)
            return 0;

        if (cp < 0x7f)
            return 1;

        if (cp < 0xa0)
            return 0;

        if (cp >= 0x300 && cp <= 0x36f)
            return 0;
        if (cp >= 0x610 && cp <= 0x61a)
            return 0;
        if (cp >= 0x64b && cp <= 0x65f)
            return 0;
        if (cp == 0x200b || cp == 0x200c || cp == 0x200d || cp == 0xfeff)
            return 0;
        if (cp >= 0x1ab0 && cp <= 0x1aff)
            return 0;
        if (cp >= 0x1dc0 && cp <= 0x1dff)
            return 0;
        if (cp >= 0x20d0 && cp <= 0x20ff)
            return 0;
        if (cp >= 0xfe20 && cp <= 0xfe2f)
            return 0;

        if (cp >= 0x1100 && cp <= 0x115f)
            return 2;
        if (cp >= 0x2e80 && cp <= 0x303e)
            return 2;
        if (cp >= 0x3041 && cp <= 0x33bf)
            return 2;
        if (cp >= 0x3400 && cp <= 0x4dbf)
            return 2;
        if (cp >= 0x4e00 && cp <= 0x9fff)
            return 2;
        if (cp >= 0xa000 && cp <= 0xa4cf)
            return 2;
        if (cp >= 0xa960 && cp <= 0xa97f)
            return 2;
        if (cp >= 0xac00 && cp <= 0xd7ff)
            return 2;
        if (cp >= 0xf900 && cp <= 0xfaff)
            return 2;
        if (cp >= 0xfe10 && cp <= 0xfe1f)
            return 2;
        if (cp >= 0xfe30 && cp <= 0xfe6f)
            return 2;
        if (cp >= 0xff01 && cp <= 0xff60)
            return 2;
        if (cp >= 0xffe0 && cp <= 0xffe6)
            return 2;
        if (cp >= 0x1b000 && cp <= 0x1b0ff)
            return 2;
        if (cp >= 0x1f004 && cp <= 0x1f0cf)
            return 2;
        if (cp >= 0x1f300 && cp <= 0x1f9ff)
            return 2;
        if (cp >= 0x20000 && cp <= 0x2fa1f)
            return 2;

        return 1;
    }

    [[nodiscard]] std::expected<DecodeResult, Error> decode_one(std::span<const char8_t> bytes) noexcept
    {
        if (bytes.empty())
            return std::unexpected{Error::TruncatedSequence};

        const auto b0 = static_cast<uint8_t>(bytes[0]);

        if ((b0 & 0xc0) == 0x80)
            return std::unexpected{Error::UnexpectedContinuation};

        int len = 0;
        uint32_t cp = 0;
        uint32_t min_cp = 0;

        if (b0 < 0x80)
        {
            return DecodeResult{static_cast<Codepoint>(b0), 1};
        }
        else if ((b0 & 0xe0) == 0xc0)
        {
            len = 2;
            cp = b0 & 0x1f;
            min_cp = 0x80;
        }
        else if ((b0 & 0xf0) == 0xe0)
        {
            len = 3;
            cp = b0 & 0xf;
            min_cp = 0x800;
        }
        else if ((b0 & 0xf8) == 0xf0)
        {
            len = 4;
            cp = b0 & 0x7;
            min_cp = 0x10000;
        }
        else
        {
            return std::unexpected{Error::InvalidByte};
        }

        if (static_cast<int>(bytes.size()) < len)
            return std::unexpected{Error::TruncatedSequence};

        for (int i = 1; i < len; ++i)
        {
            const auto b = static_cast<uint8_t>(bytes[i]);
            if ((b & 0xc0) != 0x80)
                return std::unexpected{Error::InvalidByte};

            cp = (cp << 6) | (b & 0x3f);
        }

        if (cp < min_cp)
            return std::unexpected{Error::OverlongEncoding};

        if ((cp >= 0xd800 && cp <= 0xdfff) || cp > 0x10ffff)
            return std::unexpected{Error::InvalidCodepoint};

        return DecodeResult{static_cast<Codepoint>(cp), len};
    }

    [[nodiscard]] std::expected<DecodeResult, Error> decode_one(std::string_view sv) noexcept
    {
        return decode_one(std::span<const char8_t>{reinterpret_cast<const char8_t*>(sv.data()), sv.size()});
    }

    auto CodepointRange::CodepointIterator::operator*() const noexcept -> value_type
    {
        return decode_one(m_sv.substr(m_pos));
    }

    CodepointRange::CodepointIterator& CodepointRange::CodepointIterator::operator++() noexcept
    {
        if (m_pos >= m_sv.size())
            return *this;

        auto r = decode_one(m_sv.substr(m_pos));
        m_pos += r ? r->bytes_consumed : 1;
        return *this;
    }

    CodepointRange::CodepointIterator CodepointRange::CodepointIterator::operator++(int) noexcept
    {
        auto copy = *this;
        ++*this;
        return copy;
    }

    [[nodiscard]] std::expected<int, Error> string_width(std::string_view sv) noexcept
    {
        int width = 0;
        std::size_t pos = 0;

        while (pos < sv.size())
        {
            auto r = decode_one(sv.substr(pos));
            if (!r)
                return std::unexpected{r.error()};

            width += codepoint_width(r->codepoint);
            pos += r->bytes_consumed;
        }

        return width;
    }

    [[nodiscard]] std::expected<WidthInfo, Error> string_width_info(std::string_view sv) noexcept
    {
        WidthInfo info{};

        while (info.bytes < sv.size())
        {
            auto r = decode_one(sv.substr(info.bytes));
            if (!r)
                return std::unexpected{r.error()};

            info.columns += codepoint_width(r->codepoint);
            info.codepoints += 1;
            info.bytes += static_cast<std::size_t>(r->bytes_consumed);
        }

        return info;
    }

    [[nodiscard]] std::string_view truncate_to_width(std::string_view sv, int max_columns) noexcept
    {
        int cols = 0;
        std::size_t pos = 0;

        while (pos < sv.size())
        {
            auto r = decode_one(sv.substr(pos));
            if (!r)
                break;

            int w = codepoint_width(r->codepoint);
            if (cols + w > max_columns)
                break;

            cols += w;
            pos += static_cast<std::size_t>(r->bytes_consumed);
        }

        return sv.substr(0, pos);
    }

    [[nodiscard]] std::string fit_to_width(std::string_view sv, int width, char fill)
    {
        std::string result;
        result.reserve(static_cast<std::size_t>(width));

        int cols = 0;
        std::size_t pos = 0;
        while (pos < sv.size() && cols < width)
        {
            auto r = decode_one(sv.substr(pos));
            if (!r)
                break;

            int w = codepoint_width(r->codepoint);
            if (cols + w > width)
            {
                result += fill;
                ++cols;
            }
            else
            {
                result.append(sv.data() + pos, static_cast<std::size_t>(r->bytes_consumed));
                cols += w;
            }

            pos += static_cast<std::size_t>(r->bytes_consumed);
        }

        while (cols < width)
        {
            result += fill;
            ++cols;
        }

        return result;
    }

} // namespace dcc::utf8
