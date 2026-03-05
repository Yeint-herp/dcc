#include <algorithm>
#include <array>
#include <lex/token.hh>

namespace dcc::lex
{
    std::string_view to_string(TokenKind kind) noexcept
    {
        switch (kind)
        {
            case TokenKind::Eof:
                return "eof";
            case TokenKind::Invalid:
                return "invalid";
            case TokenKind::Identifier:
                return "identifier";
            case TokenKind::IntLiteral:
                return "integer literal";
            case TokenKind::FloatLiteral:
                return "float literal";
            case TokenKind::StringLiteral:
                return "string literal";
            case TokenKind::CharLiteral:
                return "char literal";

            case TokenKind::Kwu8:
                return "u8";
            case TokenKind::Kwi8:
                return "i8";
            case TokenKind::KWu16:
                return "u16";
            case TokenKind::KWi16:
                return "i16";
            case TokenKind::KWu32:
                return "u32";
            case TokenKind::Kwi32:
                return "i32";
            case TokenKind::Kwu64:
                return "u64";
            case TokenKind::Kwi64:
                return "i64";
            case TokenKind::Kwf32:
                return "f32";
            case TokenKind::Kwf64:
                return "f64";
            case TokenKind::KwVoid:
                return "void";
            case TokenKind::KwBool:
                return "bool";
            case TokenKind::KwNullT:
                return "null_t";

            case TokenKind::KwNull:
                return "null";
            case TokenKind::KwTrue:
                return "true";
            case TokenKind::KwFalse:
                return "false";

            case TokenKind::KwStruct:
                return "struct";
            case TokenKind::KwEnum:
                return "enum";
            case TokenKind::KwUnion:
                return "union";
            case TokenKind::KwUsing:
                return "using";
            case TokenKind::KwConst:
                return "const";
            case TokenKind::KwRestrict:
                return "restrict";
            case TokenKind::KwVolatile:
                return "volatile";

            case TokenKind::KwIf:
                return "if";
            case TokenKind::KwElse:
                return "else";
            case TokenKind::KwWhile:
                return "while";
            case TokenKind::KwFor:
                return "for";
            case TokenKind::KwDo:
                return "do";
            case TokenKind::KwSwitch:
                return "switch";
            case TokenKind::KwMatch:
                return "match";
            case TokenKind::KwCase:
                return "case";
            case TokenKind::KwDefault:
                return "default";
            case TokenKind::KwBreak:
                return "break";
            case TokenKind::KwContinue:
                return "continue";
            case TokenKind::KwReturn:
                return "return";
            case TokenKind::KwDefer:
                return "defer";

            case TokenKind::KwImport:
                return "import";
            case TokenKind::KwModule:
                return "module";
            case TokenKind::KwPublic:
                return "public";

            case TokenKind::KwAs:
                return "as";
            case TokenKind::KwSizeof:
                return "sizeof";
            case TokenKind::KwAlignof:
                return "alignof";
            case TokenKind::KwStatic:
                return "static";
            case TokenKind::KwExtern:
                return "extern";

            case TokenKind::LParen:
                return "(";
            case TokenKind::RParen:
                return ")";
            case TokenKind::LBracket:
                return "[";
            case TokenKind::RBracket:
                return "]";
            case TokenKind::LBrace:
                return "{";
            case TokenKind::RBrace:
                return "}";

            case TokenKind::Dot:
                return ".";
            case TokenKind::DotDot:
                return "..";
            case TokenKind::Ellipsis:
                return "...";
            case TokenKind::Comma:
                return ",";
            case TokenKind::Semicolon:
                return ";";
            case TokenKind::Colon:
                return ":";
            case TokenKind::ColonColon:
                return "::";
            case TokenKind::Arrow:
                return "->";
            case TokenKind::FatArrow:
                return "=>";
            case TokenKind::At:
                return "@";
            case TokenKind::Hash:
                return "#";
            case TokenKind::Tilde:
                return "~";
            case TokenKind::Question:
                return "?";
            case TokenKind::Dollar:
                return "$";

            case TokenKind::Plus:
                return "+";
            case TokenKind::Minus:
                return "-";
            case TokenKind::Star:
                return "*";
            case TokenKind::Slash:
                return "/";
            case TokenKind::Percent:
                return "%";

            case TokenKind::Amp:
                return "&";
            case TokenKind::Pipe:
                return "|";
            case TokenKind::Caret:
                return "^";
            case TokenKind::Bang:
                return "!";
            case TokenKind::AmpAmp:
                return "&&";
            case TokenKind::PipePipe:
                return "||";

            case TokenKind::Eq:
                return "=";
            case TokenKind::EqEq:
                return "==";
            case TokenKind::BangEq:
                return "!=";
            case TokenKind::Lt:
                return "<";
            case TokenKind::Gt:
                return ">";
            case TokenKind::LtEq:
                return "<=";
            case TokenKind::GtEq:
                return ">=";

            case TokenKind::LtLt:
                return "<<";
            case TokenKind::GtGt:
                return ">>";

            case TokenKind::PlusEq:
                return "+=";
            case TokenKind::MinusEq:
                return "-=";
            case TokenKind::StarEq:
                return "*=";
            case TokenKind::SlashEq:
                return "/=";
            case TokenKind::PercentEq:
                return "%=";
            case TokenKind::AmpEq:
                return "&=";
            case TokenKind::PipeEq:
                return "|=";
            case TokenKind::CaretEq:
                return "^=";
            case TokenKind::LtLtEq:
                return "<<=";
            case TokenKind::GtGtEq:
                return ">>=";
        }
        return "unknown";
    }

    struct KeywordEntry
    {
        std::string_view spelling;
        TokenKind kind;
    };

    static constexpr std::array<KeywordEntry, 45> keyword_table{{
        {"alignof", TokenKind::KwAlignof},
        {"as", TokenKind::KwAs},
        {"bool", TokenKind::KwBool},
        {"break", TokenKind::KwBreak},
        {"case", TokenKind::KwCase},
        {"const", TokenKind::KwConst},
        {"continue", TokenKind::KwContinue},
        {"default", TokenKind::KwDefault},
        {"defer", TokenKind::KwDefer},
        {"do", TokenKind::KwDo},
        {"else", TokenKind::KwElse},
        {"enum", TokenKind::KwEnum},
        {"extern", TokenKind::KwExtern},
        {"f32", TokenKind::Kwf32},
        {"f64", TokenKind::Kwf64},
        {"false", TokenKind::KwFalse},
        {"for", TokenKind::KwFor},
        {"i16", TokenKind::KWi16},
        {"i32", TokenKind::Kwi32},
        {"i64", TokenKind::Kwi64},
        {"i8", TokenKind::Kwi8},
        {"if", TokenKind::KwIf},
        {"import", TokenKind::KwImport},
        {"match", TokenKind::KwMatch},
        {"module", TokenKind::KwModule},
        {"null", TokenKind::KwNull},
        {"null_t", TokenKind::KwNullT},
        {"public", TokenKind::KwPublic},
        {"restrict", TokenKind::KwRestrict},
        {"return", TokenKind::KwReturn},
        {"sizeof", TokenKind::KwSizeof},
        {"static", TokenKind::KwStatic},
        {"struct", TokenKind::KwStruct},
        {"switch", TokenKind::KwSwitch},
        {"true", TokenKind::KwTrue},
        {"u16", TokenKind::KWu16},
        {"u32", TokenKind::KWu32},
        {"u64", TokenKind::Kwu64},
        {"u8", TokenKind::Kwu8},
        {"union", TokenKind::KwUnion},
        {"using", TokenKind::KwUsing},
        {"void", TokenKind::KwVoid},
        {"volatile", TokenKind::KwVolatile},
        {"while", TokenKind::KwWhile},
    }};

    TokenKind classify_identifier(std::string_view text) noexcept
    {
        auto it = std::ranges::lower_bound(keyword_table, text, {}, &KeywordEntry::spelling);
        if (it != keyword_table.end() && it->spelling == text)
            return it->kind;

        return TokenKind::Identifier;
    }

} // namespace dcc::lex
