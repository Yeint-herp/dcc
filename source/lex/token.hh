#ifndef DCC_LEXER_TOKEN_HH
#define DCC_LEXER_TOKEN_HH

#include <cstdint>
#include <optional>
#include <string_view>
#include <util/si.hh>
#include <util/source_manager.hh>

namespace dcc::lex
{
    enum class TokenKind : uint16_t
    {
        Eof,
        Invalid,

        Identifier,
        IntLiteral,
        FloatLiteral,
        StringLiteral,
        CharLiteral,

        Kwu8,
        Kwi8,
        Kwu16,
        Kwi16,
        Kwu32,
        Kwi32,
        Kwu64,
        Kwi64,
        Kwf32,
        Kwf64,
        KwVoid,
        KwBool,
        KwNullT,

        KwNull,
        KwTrue,
        KwFalse,

        KwStruct,
        KwEnum,
        KwUnion,
        KwUsing,
        KwConst,
        KwRestrict,
        KwVolatile,

        KwIf,
        KwElse,
        KwWhile,
        KwFor,
        KwDo,
        KwSwitch,
        KwMatch,
        KwCase,
        KwDefault,
        KwBreak,
        KwContinue,
        KwReturn,
        KwDefer,

        KwImport,
        KwModule,
        KwPublic,

        KwAs,
        KwSizeof,
        KwAlignof,
        KwStatic,
        KwExtern,

        LParen,
        RParen,
        LBracket,
        RBracket,
        LBrace,
        RBrace,

        Dot,
        DotDot,
        Ellipsis,
        Comma,
        Semicolon,
        Colon,
        ColonColon,
        Arrow,
        FatArrow,
        At,
        Hash,
        Tilde,
        Question,
        Dollar,

        Plus,
        Minus,
        Star,
        Slash,
        Percent,

        Amp,
        Pipe,
        Caret,
        Bang,
        AmpAmp,
        PipePipe,

        Eq,
        EqEq,
        BangEq,
        Lt,
        Gt,
        LtEq,
        GtEq,

        LtLt,
        GtGt,

        PlusEq,
        MinusEq,
        StarEq,
        SlashEq,
        PercentEq,
        AmpEq,
        PipeEq,
        CaretEq,
        LtLtEq,
        GtGtEq,
    };
    [[nodiscard]] std::string_view to_string(TokenKind kind) noexcept;

    [[nodiscard]] constexpr bool is_keyword(TokenKind k) noexcept
    {
        return k >= TokenKind::Kwu8 && k <= TokenKind::KwExtern;
    }

    [[nodiscard]] constexpr bool is_punctuation(TokenKind k) noexcept
    {
        return k >= TokenKind::LParen && k <= TokenKind::GtGtEq;
    }

    [[nodiscard]] constexpr bool is_assignment(TokenKind k) noexcept
    {
        return k == TokenKind::Eq || (k >= TokenKind::PlusEq && k <= TokenKind::GtGtEq);
    }

    [[nodiscard]] constexpr bool is_literal(TokenKind k) noexcept
    {
        return k >= TokenKind::IntLiteral && k <= TokenKind::CharLiteral;
    }

    struct TokenValue
    {
        enum class Kind
        {
            None,
            Int,
            Float,
            String,
            Char
        } kind = Kind::None;

        union
        {
            int64_t as_int;
            double as_float;
            std::string as_string;
            uint32_t as_char;
        };

        TokenValue() noexcept : kind(Kind::None) {}
        ~TokenValue() noexcept
        {
            if (kind == Kind::String)
            {
                as_string.~basic_string();
            }
        }

        TokenValue(TokenValue&& other) noexcept : kind(other.kind)
        {
            switch (kind)
            {
                case Kind::Int:
                    as_int = other.as_int;
                    break;
                case Kind::Float:
                    as_float = other.as_float;
                    break;
                case Kind::String:
                    new (&as_string) std::string(std::move(other.as_string));
                    break;
                case Kind::Char:
                    as_char = other.as_char;
                    break;
                case Kind::None:
                    break;
            }
            other.kind = Kind::None;
        }

        TokenValue& operator=(TokenValue&& other) noexcept
        {
            if (this != &other)
            {
                if (kind == Kind::String)
                {
                    as_string.~basic_string();
                }

                kind = other.kind;
                switch (kind)
                {
                    case Kind::Int:
                        as_int = other.as_int;
                        break;
                    case Kind::Float:
                        as_float = other.as_float;
                        break;
                    case Kind::String:
                        new (&as_string) std::string(std::move(other.as_string));
                        break;
                    case Kind::Char:
                        as_char = other.as_char;
                        break;
                    case Kind::None:
                        break;
                }
                other.kind = Kind::None;
            }
            return *this;
        }

        TokenValue(const TokenValue&) = delete;
        TokenValue& operator=(const TokenValue&) = delete;

        TokenValue(int64_t v) noexcept : kind(Kind::Int), as_int(v) {}
        TokenValue(double v) noexcept : kind(Kind::Float), as_float(v) {}
        TokenValue(std::string s) noexcept : kind(Kind::String), as_string(std::move(s)) {}
        TokenValue(uint32_t ch) noexcept : kind(Kind::Char), as_char(ch) {}

        [[nodiscard]] bool is_none() const noexcept { return kind == Kind::None; }
        [[nodiscard]] bool is_int() const noexcept { return kind == Kind::Int; }
        [[nodiscard]] bool is_float() const noexcept { return kind == Kind::Float; }
        [[nodiscard]] bool is_string() const noexcept { return kind == Kind::String; }
        [[nodiscard]] bool is_char() const noexcept { return kind == Kind::Char; }

        [[nodiscard]] int64_t as_int_value() const noexcept { return as_int; }
        [[nodiscard]] double as_float_value() const noexcept { return as_float; }
        [[nodiscard]] std::string_view as_string_value() const noexcept { return as_string; }
        [[nodiscard]] uint32_t as_char_value() const noexcept { return as_char; }
    };

    struct Token
    {
        TokenKind kind{TokenKind::Eof};
        sm::SourceRange range{};

        std::string_view text{};
        si::InternedString interned{};
        std::optional<TokenValue> value;

        [[nodiscard]] constexpr bool is(TokenKind k) const noexcept { return kind == k; }
        [[nodiscard]] constexpr bool is_not(TokenKind k) const noexcept { return kind != k; }

        template <typename... Kinds> [[nodiscard]] constexpr bool is_one_of(Kinds... kinds) const noexcept { return (is(kinds) || ...); }

        [[nodiscard]] constexpr bool eof() const noexcept { return kind == TokenKind::Eof; }
        [[nodiscard]] constexpr bool valid() const noexcept { return kind != TokenKind::Invalid; }

        [[nodiscard]] constexpr sm::Location begin_loc() const noexcept { return range.begin; }
        [[nodiscard]] constexpr sm::Location end_loc() const noexcept { return range.end; }
    };

    [[nodiscard]] TokenKind classify_identifier(std::string_view text) noexcept;

} // namespace dcc::lex

#endif /* DCC_LEXER_TOKEN_HH */
