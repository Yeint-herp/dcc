#include <cctype>
#include <cmath>
#include <cstdlib>
#include <lex/lexer.hh>
#include <utility>

namespace dcc::lex
{
    Lexer::Lexer(const sm::SourceFile& file, si::StringInterner& interner) noexcept : m_file{file}, m_interner{interner}, m_source{file.text()} {}

    bool Lexer::is_digit_base(char c, int base) noexcept
    {
        switch (base)
        {
            case 2:
                return c == '0' || c == '1';
            case 8:
                return c >= '0' && c <= '7';
            case 10:
                return c >= '0' && c <= '9';
            case 16:
                return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
            default:
                return false;
        }
    }

    std::optional<int64_t> Lexer::parse_integer_literal(std::string_view spelling, int base) noexcept
    {
        std::string_view digits = spelling;
        if (base == 16 && (digits.starts_with("0x") || digits.starts_with("0X")))
            digits.remove_prefix(2);
        else if (base == 8 && (digits.starts_with("0o") || digits.starts_with("0O")))
            digits.remove_prefix(2);
        else if (base == 2 && (digits.starts_with("0b") || digits.starts_with("0B")))
            digits.remove_prefix(2);

        std::string clean;
        clean.reserve(digits.size());
        for (char c : digits)
        {
            if (c != '_')
                clean.push_back(c);
        }

        if (clean.empty())
            return std::nullopt;

        const char* begin = clean.data();
        char* end = nullptr;
        int64_t val;

        switch (base)
        {
            case 2:
            case 8:
            case 10:
                val = std::strtoll(begin, &end, base);
                break;
            case 16:
                val = std::strtoll(begin, &end, 16);
                break;
            default:
                return std::nullopt;
        }

        if (end == begin || *end != '\0')
            return std::nullopt;

        if (val == LLONG_MAX || val == LLONG_MIN)
        {
            if (std::string_view(begin).size() > 1 && *(end - 1) != '0')
                return std::nullopt;
        }

        return val;
    }

    std::optional<double> Lexer::parse_float_literal(std::string_view spelling) noexcept
    {
        std::string clean;
        clean.reserve(spelling.size());
        for (char c : spelling)
        {
            if (c != '_')
                clean.push_back(c);
        }

        const char* begin = clean.data();
        char* end = nullptr;
        double val = std::strtod(begin, &end);

        if (end == begin || *end != '\0')
            return std::nullopt;

        if (std::isinf(val) || std::isnan(val))
            return std::nullopt;

        return val;
    }

    std::optional<uint32_t> Lexer::parse_char_literal(std::string_view content) noexcept
    {
        if (content.empty())
            return std::nullopt;

        uint32_t codepoint = 0;
        std::size_t i = 0;

        auto read_hex_escape = [&](std::size_t len) -> std::optional<uint32_t> {
            std::string hex;
            hex.reserve(len);

            for (std::size_t j = 0; j < len && i + j < content.size(); ++j)
            {
                char c = content[i + j];
                if (std::isxdigit(c))
                    hex.push_back(c);
                else
                    return std::nullopt;
            }
            if (hex.size() != len)
                return std::nullopt;

            return static_cast<uint32_t>(std::stoul(hex, nullptr, 16));
        };

        if (content[i] == '\\')
        {
            ++i;
            if (i >= content.size())
                return std::nullopt;

            switch (content[i])
            {
                case 'n':
                    codepoint = '\n';
                    ++i;
                    break;
                case 't':
                    codepoint = '\t';
                    ++i;
                    break;
                case 'r':
                    codepoint = '\r';
                    ++i;
                    break;
                case '0':
                    codepoint = '\0';
                    ++i;
                    break;
                case '\\':
                    codepoint = '\\';
                    ++i;
                    break;
                case '"':
                    codepoint = '"';
                    ++i;
                    break;
                case '\'':
                    codepoint = '\'';
                    ++i;
                    break;
                case 'x':
                    ++i;
                    if (i + 2 > content.size())
                        return std::nullopt;

                    if (auto v = read_hex_escape(2); v)
                        codepoint = *v;
                    else
                        return std::nullopt;

                    i += 2;
                    break;
                case 'u': {
                    ++i;
                    if (i >= content.size() || content[i] != '{')
                        return std::nullopt;

                    ++i;
                    std::size_t start = i;
                    while (i < content.size() && content[i] != '}')
                        ++i;

                    if (i >= content.size())
                        return std::nullopt;

                    std::string hex = std::string(content.substr(start, i - start));
                    if (hex.empty())
                        return std::nullopt;

                    try
                    {
                        codepoint = static_cast<uint32_t>(std::stoul(hex, nullptr, 16));
                    }
                    catch (...)
                    {
                        return std::nullopt;
                    }
                    ++i;
                    break;
                }
                default:
                    return std::nullopt;
            }
        }
        else
        {
            codepoint = static_cast<uint8_t>(content[i]);
            ++i;
        }

        if (i != content.size())
            return std::nullopt;

        if (codepoint > 0x10FFFF)
            return std::nullopt;

        return codepoint;
    }

    std::string_view Lexer::strip_suffix(std::string_view spelling) noexcept
    {
        if (spelling.ends_with("u8"))
            spelling.remove_suffix(2);
        else if (spelling.ends_with("u16"))
            spelling.remove_suffix(3);
        else if (spelling.ends_with("u32"))
            spelling.remove_suffix(3);
        else if (spelling.ends_with("u64"))
            spelling.remove_suffix(3);
        else if (spelling.ends_with("i8"))
            spelling.remove_suffix(2);
        else if (spelling.ends_with("i16"))
            spelling.remove_suffix(3);
        else if (spelling.ends_with("i32"))
            spelling.remove_suffix(3);
        else if (spelling.ends_with("i64"))
            spelling.remove_suffix(3);
        else if (spelling.ends_with("f32"))
            spelling.remove_suffix(3);
        else if (spelling.ends_with("f64"))
            spelling.remove_suffix(3);

        return spelling;
    }

    bool Lexer::is_literal_suffix(std::string_view sv) noexcept
    {
        return sv == "u8" || sv == "u16" || sv == "u32" || sv == "u64" || sv == "i8" || sv == "i16" || sv == "i32" || sv == "i64" || sv == "f32" || sv == "f64";
    }

    Token Lexer::next()
    {
        if (m_peek)
        {
            Token tok = std::move(*m_peek);
            m_peek.reset();
            return tok;
        }

        return lex();
    }

    const Token& Lexer::peek()
    {
        if (!m_peek)
            m_peek = lex();

        return *m_peek;
    }

    bool Lexer::eat(TokenKind kind)
    {
        if (peek().is(kind))
        {
            std::ignore = next();
            return true;
        }

        return false;
    }

    std::optional<Token> Lexer::eat_token(TokenKind kind)
    {
        if (peek().is(kind))
            return next();

        return std::nullopt;
    }

    bool Lexer::at_end()
    {
        return peek().eof();
    }

    char Lexer::current() const noexcept
    {
        if (m_pos >= m_source.size())
            return '\0';

        return m_source[m_pos];
    }

    char Lexer::lookahead(sm::Offset ahead) const noexcept
    {
        auto idx = m_pos + ahead;
        if (idx >= m_source.size())
            return '\0';

        return m_source[idx];
    }

    char Lexer::advance() noexcept
    {
        if (m_pos >= m_source.size())
            return '\0';

        return m_source[m_pos++];
    }

    bool Lexer::match(char expected) noexcept
    {
        if (current() == expected)
        {
            ++m_pos;
            return true;
        }

        return false;
    }

    bool Lexer::at_eof() const noexcept
    {
        return m_pos >= m_source.size();
    }

    Token Lexer::make(TokenKind kind, sm::Offset start) const noexcept
    {
        sm::FileId fid = m_file.id();
        return Token{.kind = kind,
                     .range =
                         sm::SourceRange{
                             .begin = {fid, start},
                             .end = {fid, m_pos},
                         },
                     .text = m_source.substr(start, m_pos - start),
                     .interned = {},
                     .value = {}};
    }

    bool Lexer::is_dec(char c) noexcept
    {
        return c >= '0' && c <= '9';
    }

    bool Lexer::is_hex(char c) noexcept
    {
        return is_dec(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    }

    bool Lexer::is_bin(char c) noexcept
    {
        return c == '0' || c == '1';
    }

    bool Lexer::is_oct(char c) noexcept
    {
        return c >= '0' && c <= '7';
    }

    bool Lexer::is_ident_head(char c) noexcept
    {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
    }

    bool Lexer::is_ident_body(char c) noexcept
    {
        return is_ident_head(c) || is_dec(c);
    }

    void Lexer::skip_trivia()
    {
        for (;;)
        {
            char c = current();
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
            {
                advance();
                continue;
            }

            if (c == '/' && lookahead() == '/')
            {
                skip_line_comment();
                continue;
            }

            if (c == '/' && lookahead() == '*')
            {
                skip_block_comment();
                continue;
            }
            break;
        }
    }

    void Lexer::skip_line_comment() noexcept
    {
        m_pos += 2;
        while (!at_eof() && current() != '\n')
            advance();
    }

    bool Lexer::skip_block_comment() noexcept
    {
        m_pos += 2;
        int depth = 1;

        while (!at_eof() && depth > 0)
        {
            char c = current();
            if (c == '/' && lookahead() == '*')
            {
                m_pos += 2;
                ++depth;
            }
            else if (c == '*' && lookahead() == '/')
            {
                m_pos += 2;
                --depth;
            }
            else
                advance();
        }
        return depth == 0;
    }

    void Lexer::consume_digits(bool (*pred)(char)) noexcept
    {
        while (!at_eof())
        {
            char c = current();
            if (pred(c))
                advance();
            else if (c == '_' && pred(lookahead()))
                advance();
            else
                break;
        }
    }

    Token Lexer::lex_number()
    {
        sm::Offset start = m_pos;
        bool is_float = false;

        auto munch_suffix = [&] {
            while (!at_eof() && is_ident_body(current()))
                advance();
        };

        if (current() == '0')
        {
            char p = lookahead();
            if (p == 'x' || p == 'X')
            {
                m_pos += 2;
                consume_digits(is_hex);
                munch_suffix();
                Token tok = make(TokenKind::IntLiteral, start);
                tok.value = parse_literal_value(tok);
                return tok;
            }
            if (p == 'o' || p == 'O')
            {
                m_pos += 2;
                consume_digits(is_oct);
                munch_suffix();
                Token tok = make(TokenKind::IntLiteral, start);
                tok.value = parse_literal_value(tok);
                return tok;
            }
            if (p == 'b' || p == 'B')
            {
                m_pos += 2;
                consume_digits(is_bin);
                munch_suffix();
                Token tok = make(TokenKind::IntLiteral, start);
                tok.value = parse_literal_value(tok);
                return tok;
            }
        }

        consume_digits(is_dec);

        if (current() == '.' && is_dec(lookahead()))
        {
            is_float = true;
            advance();
            consume_digits(is_dec);
        }

        if (char e = current(); e == 'e' || e == 'E')
        {
            is_float = true;
            advance();
            if (char s = current(); s == '+' || s == '-')
                advance();

            consume_digits(is_dec);
        }

        munch_suffix();

        Token tok = make(is_float ? TokenKind::FloatLiteral : TokenKind::IntLiteral, start);
        tok.value = parse_literal_value(tok);
        return tok;
    }

    Token Lexer::lex_identifier()
    {
        sm::Offset start = m_pos;
        advance();

        while (!at_eof() && is_ident_body(current()))
            advance();

        auto spelling = m_source.substr(start, m_pos - start);
        TokenKind kind = classify_identifier(spelling);

        Token tok = make(kind, start);
        tok.interned = m_interner.intern(spelling);

        return tok;
    }

    Token Lexer::lex_string()
    {
        sm::Offset start = m_pos;
        advance();

        while (!at_eof())
        {
            char c = current();
            if (c == '"')
            {
                advance();
                Token tok = make(TokenKind::StringLiteral, start);
                tok.value = parse_literal_value(tok);
                return tok;
            }

            if (c == '\\')
            {
                advance();
                if (!at_eof())
                    advance();

                continue;
            }
            if (c == '\n')
                break;

            advance();
        }

        return make(TokenKind::Invalid, start);
    }

    Token Lexer::lex_char()
    {
        sm::Offset start = m_pos;
        advance();

        while (!at_eof())
        {
            char c = current();
            if (c == '\'')
            {
                advance();
                Token tok = make(TokenKind::CharLiteral, start);
                tok.value = parse_literal_value(tok);
                return tok;
            }

            if (c == '\\')
            {
                advance();
                if (!at_eof())
                    advance();

                continue;
            }

            if (c == '\n')
                break;
            advance();
        }

        return make(TokenKind::Invalid, start);
    }

    std::optional<std::string> Lexer::parse_string_literal(std::string_view content) noexcept
    {
        std::string decoded;
        decoded.reserve(content.size());
        std::size_t i = 0;

        auto append_hex = [&](std::size_t len) -> bool {
            std::string hex;
            hex.reserve(len);

            for (std::size_t j = 0; j < len && i + j < content.size(); ++j)
            {
                char c = content[i + j];
                if (std::isxdigit(c))
                    hex.push_back(c);
                else
                    return false;
            }

            if (hex.size() != len)
                return false;

            uint8_t val = static_cast<uint8_t>(std::stoul(hex, nullptr, 16));
            decoded.push_back(static_cast<char>(val));
            i += len;
            return true;
        };

        while (i < content.size())
        {
            char c = content[i];
            if (c == '\\' && i + 1 < content.size())
            {
                ++i;
                char esc = content[i];
                switch (esc)
                {
                    case 'n':
                        decoded.push_back('\n');
                        break;
                    case 't':
                        decoded.push_back('\t');
                        break;
                    case 'r':
                        decoded.push_back('\r');
                        break;
                    case '0':
                        decoded.push_back('\0');
                        break;
                    case '\\':
                        decoded.push_back('\\');
                        break;
                    case '"':
                        decoded.push_back('"');
                        break;
                    case '\'':
                        decoded.push_back('\'');
                        break;
                    case 'x':
                        ++i;
                        if (!append_hex(2))
                            return std::nullopt;

                        break;
                    case 'u': {
                        ++i;
                        if (content[i] != '{')
                            return std::nullopt;

                        ++i;
                        std::size_t start = i;
                        while (i < content.size() && content[i] != '}')
                            ++i;

                        if (i >= content.size())
                            return std::nullopt;

                        std::string hex = std::string(content.substr(start, i - start));
                        if (hex.empty())
                            return std::nullopt;

                        uint32_t cp;
                        try
                        {
                            cp = static_cast<uint32_t>(std::stoul(hex, nullptr, 16));
                        }
                        catch (...)
                        {
                            return std::nullopt;
                        }

                        if (cp <= 0x7F)
                            decoded.push_back(static_cast<char>(cp));
                        else if (cp <= 0x7FF)
                        {
                            decoded.push_back(static_cast<char>(0xC0 | (cp >> 6)));
                            decoded.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
                        }
                        else if (cp <= 0xFFFF)
                        {
                            decoded.push_back(static_cast<char>(0xE0 | (cp >> 12)));
                            decoded.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
                            decoded.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
                        }
                        else
                        {
                            decoded.push_back(static_cast<char>(0xF0 | (cp >> 18)));
                            decoded.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
                            decoded.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
                            decoded.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
                        }
                        ++i;
                        break;
                    }
                    default:
                        return std::nullopt;
                }
                ++i;
            }
            else
            {
                decoded.push_back(c);
                ++i;
            }
        }

        return decoded;
    }

    TokenValue Lexer::parse_literal_value(const Token& tok) noexcept
    {
        if (!is_literal(tok.kind))
            return {};

        std::string_view spelling = tok.text;
        if (tok.kind == TokenKind::IntLiteral)
        {
            spelling = strip_suffix(spelling);
            if (spelling.starts_with("0b") || spelling.starts_with("0B"))
                return TokenValue(parse_integer_literal(spelling, 2).value_or(0));
            else if (spelling.starts_with("0o") || spelling.starts_with("0O"))
                return TokenValue(parse_integer_literal(spelling, 8).value_or(0));
            else if (spelling.starts_with("0x") || spelling.starts_with("0X"))
                return TokenValue(parse_integer_literal(spelling, 16).value_or(0));
            else
                return TokenValue(parse_integer_literal(spelling, 10).value_or(0));
        }
        else if (tok.kind == TokenKind::FloatLiteral)
        {
            spelling = strip_suffix(spelling);
            if (auto val = parse_float_literal(spelling))
                return TokenValue(*val);

            return TokenValue(0.0);
        }
        else if (tok.kind == TokenKind::StringLiteral)
        {
            std::string_view content = tok.text.substr(1, tok.text.size() - 2);
            if (auto v = parse_string_literal(content))
                return TokenValue(*v);

            return std::string("");
        }
        else if (tok.kind == TokenKind::CharLiteral)
        {
            std::string_view content = tok.text.substr(1, tok.text.size() - 2);
            if (auto v = parse_char_literal(content))
                return TokenValue(static_cast<uint32_t>(*v));

            return TokenValue(static_cast<uint32_t>(0xFFFD));
        }

        return {};
    }

    Token Lexer::lex()
    {
        skip_trivia();

        if (at_eof())
            return make(TokenKind::Eof, m_pos);

        sm::Offset start = m_pos;
        char c = advance();

        if (is_ident_head(c))
        {
            m_pos = start;
            return lex_identifier();
        }

        if (is_dec(c))
        {
            m_pos = start;
            return lex_number();
        }

        if (c == '.' && is_dec(current()))
        {
            m_pos = start;
            return lex_number();
        }

        if (c == '"')
        {
            m_pos = start;
            return lex_string();
        }

        if (c == '\'')
        {
            m_pos = start;
            return lex_char();
        }

        switch (c)
        {
            case '(':
                return make(TokenKind::LParen, start);
            case ')':
                return make(TokenKind::RParen, start);
            case '[':
                return make(TokenKind::LBracket, start);
            case ']':
                return make(TokenKind::RBracket, start);
            case '{':
                return make(TokenKind::LBrace, start);
            case '}':
                return make(TokenKind::RBrace, start);
            case ',':
                return make(TokenKind::Comma, start);
            case ';':
                return make(TokenKind::Semicolon, start);
            case '~':
                return make(TokenKind::Tilde, start);
            case '?':
                return make(TokenKind::Question, start);
            case '@':
                return make(TokenKind::At, start);
            case '#':
                return make(TokenKind::Hash, start);
            case '$':
                return make(TokenKind::Dollar, start);

            case '.':
                if (match('.'))
                    return match('.') ? make(TokenKind::Ellipsis, start) : make(TokenKind::DotDot, start);
                return make(TokenKind::Dot, start);

            case ':':
                return match(':') ? make(TokenKind::ColonColon, start) : make(TokenKind::Colon, start);

            case '+':
                if (match('+'))
                    return make(TokenKind::Increment, start);
                return match('=') ? make(TokenKind::PlusEq, start) : make(TokenKind::Plus, start);

            case '*':
                return match('=') ? make(TokenKind::StarEq, start) : make(TokenKind::Star, start);

            case '/':
                return match('=') ? make(TokenKind::SlashEq, start) : make(TokenKind::Slash, start);

            case '%':
                return match('=') ? make(TokenKind::PercentEq, start) : make(TokenKind::Percent, start);

            case '&':
                if (match('&'))
                    return make(TokenKind::AmpAmp, start);
                if (match('='))
                    return make(TokenKind::AmpEq, start);
                return make(TokenKind::Amp, start);

            case '|':
                if (match('|'))
                    return make(TokenKind::PipePipe, start);
                if (match('='))
                    return make(TokenKind::PipeEq, start);
                return make(TokenKind::Pipe, start);

            case '-':
                if (match('-'))
                    return make(TokenKind::Decrement, start);
                if (match('='))
                    return make(TokenKind::MinusEq, start);
                if (match('>'))
                    return make(TokenKind::Arrow, start);
                return make(TokenKind::Minus, start);

            case '^':
                return match('=') ? make(TokenKind::CaretEq, start) : make(TokenKind::Caret, start);

            case '!':
                return match('=') ? make(TokenKind::BangEq, start) : make(TokenKind::Bang, start);

            case '=':
                if (match('='))
                    return make(TokenKind::EqEq, start);
                if (match('>'))
                    return make(TokenKind::FatArrow, start);
                return make(TokenKind::Eq, start);

            case '<':
                if (match('<'))
                    return match('=') ? make(TokenKind::LtLtEq, start) : make(TokenKind::LtLt, start);
                return match('=') ? make(TokenKind::LtEq, start) : make(TokenKind::Lt, start);

            case '>':
                if (match('>'))
                    return match('=') ? make(TokenKind::GtGtEq, start) : make(TokenKind::GtGt, start);
                return match('=') ? make(TokenKind::GtEq, start) : make(TokenKind::Gt, start);

            default:
                break;
        }

        return make(TokenKind::Invalid, start);
    }

} // namespace dcc::lex
