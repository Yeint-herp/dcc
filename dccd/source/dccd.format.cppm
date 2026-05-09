export module dccd.format;

import std;
import dcc.sm;
import dcc.si;
import dcc.lex;
import dccd.protocol;

export namespace dccd::format
{
    [[nodiscard]] std::optional<protocol::TextEdit> format_document(dcc::sm::SourceFile const& sf, dcc::si::string_interner& interner,
                                                                    protocol::FormattingOptions const& options);

} // namespace dccd::format

module :private;

namespace dccd::format
{
    namespace
    {
        using dcc::lex::TokenKind;

        [[nodiscard]] std::string make_indent(int level, protocol::FormattingOptions const& opts)
        {
            std::string s;
            if (opts.insertSpaces)
            {
                auto total = static_cast<std::size_t>(level) * static_cast<std::size_t>(opts.tabSize);
                s.reserve(total);
                for (std::size_t i = 0; i < total; ++i)
                    s += ' ';
            }
            else
            {
                s.reserve(static_cast<std::size_t>(level));
                for (int i = 0; i < level; ++i)
                    s += '\t';
            }
            return s;
        }

        [[nodiscard]] bool needs_space_before(TokenKind cur, TokenKind prev) noexcept
        {
            switch (cur)
            {
                case TokenKind::Comma:
                case TokenKind::Semicolon:
                case TokenKind::RParen:
                case TokenKind::RBracket:
                case TokenKind::RBrace:
                case TokenKind::Dot:
                case TokenKind::ColonColon:
                case TokenKind::DotDot:
                case TokenKind::Ellipsis:
                    return false;
                default:
                    break;
            }

            switch (prev)
            {
                case TokenKind::LParen:
                case TokenKind::LBracket:
                case TokenKind::LBrace:
                case TokenKind::Dot:
                case TokenKind::ColonColon:
                case TokenKind::Bang:
                case TokenKind::At:
                case TokenKind::Hash:
                case TokenKind::Dollar:
                    return false;
                default:
                    break;
            }

            return true;
        }

        [[nodiscard]] std::string_view token_spelling(dcc::sm::SourceFile const& sf, dcc::lex::Token const& tok)
        {
            if (!tok.interned.empty())
                return tok.interned;

            auto text = sf.text(tok.range);
            if (text)
                return *text;

            return {};
        }

    } // anonymous namespace

    std::optional<protocol::TextEdit> format_document(dcc::sm::SourceFile const& sf, dcc::si::string_interner& interner,
                                                      protocol::FormattingOptions const& options)
    {
        dcc::lex::Lexer lexer{sf, interner};
        std::vector<dcc::lex::Token> tokens;

        while (true)
        {
            auto tok = lexer.next();
            if (tok.kind == TokenKind::Invalid)
                return std::nullopt;

            tokens.push_back(tok);
            if (tok.kind == TokenKind::Eof)
                break;
        }

        std::string indent_str = make_indent(1, options);
        std::string result;
        result.reserve(sf.size() + sf.size() / 4);

        bool start_of_line = true;
        int indent = 0;
        int paren_depth = 0;
        int bracket_depth = 0;
        TokenKind prev_kind = TokenKind::Eof;

        for (std::size_t i = 0; i < tokens.size(); ++i)
        {
            auto const& tok = tokens[i];
            if (tok.kind == TokenKind::Eof)
                continue;

            TokenKind next_kind = TokenKind::Eof;
            if (i + 1 < tokens.size())
                next_kind = tokens[i + 1].kind;

            auto spelling = token_spelling(sf, tok);
            if (spelling.empty())
                return std::nullopt;

            switch (tok.kind)
            {
                case TokenKind::LParen:
                    paren_depth++;
                    break;
                case TokenKind::RParen:
                    if (paren_depth > 0)
                        paren_depth--;
                    break;
                case TokenKind::LBracket:
                    bracket_depth++;
                    break;
                case TokenKind::RBracket:
                    if (bracket_depth > 0)
                        bracket_depth--;
                    break;
                default:
                    break;
            }

            if (tok.kind == TokenKind::RBrace)
            {
                if (indent > 0)
                    indent--;

                if (!start_of_line)
                    result += '\n';

                start_of_line = true;
            }

            if (start_of_line)
            {
                result += make_indent(indent, options);
                start_of_line = false;
            }
            else if (needs_space_before(tok.kind, prev_kind))
                result += ' ';

            result += spelling;

            if (tok.kind == TokenKind::LBrace)
            {
                indent++;

                if (next_kind == TokenKind::RBrace)
                    ;
                else
                {
                    result += '\n';
                    start_of_line = true;
                }
            }
            else if (tok.kind == TokenKind::RBrace)
            {
                result += '\n';
                start_of_line = true;
            }
            else if (tok.kind == TokenKind::Semicolon && paren_depth <= 0 && bracket_depth <= 0)
            {
                result += '\n';
                start_of_line = true;
            }

            prev_kind = tok.kind;
        }

        if (!result.empty() && result.back() != '\n')
            result += '\n';

        auto end_offset = static_cast<dcc::sm::Offset>(sf.size());
        auto end_pos = sf.lsp_position(end_offset);
        if (!end_pos)
            return std::nullopt;

        protocol::TextEdit edit;
        edit.range.start.line = 0;
        edit.range.start.character = 0;
        edit.range.end.line = end_pos->line;
        edit.range.end.character = end_pos->character;
        edit.newText = std::move(result);

        return edit;
    }

} // namespace dccd::format
