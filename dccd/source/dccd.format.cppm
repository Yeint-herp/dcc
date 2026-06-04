export module dccd.format;

import std;
import dcc.sm;
import dcc.si;
import dcc.lex;
import dcc.lex.tokens;
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
        using dcc::sm::Offset;

        [[nodiscard]] bool source_contains_comments(std::string_view src) noexcept
        {
            bool in_string = false;
            bool in_char = false;
            std::size_t i = 0;
            while (i < src.size())
            {
                char c = src[i];

                if (in_string)
                {
                    if (c == '\\')
                    {
                        i += 2;
                        continue;
                    }

                    if (c == '"')
                        in_string = false;
                }
                else if (in_char)
                {
                    if (c == '\\')
                    {
                        i += 2;
                        continue;
                    }

                    if (c == '\'')
                        in_char = false;
                }
                else
                {
                    if (c == '/' && i + 1 < src.size())
                    {
                        if (src[i + 1] == '/')
                            return true;
                        if (src[i + 1] == '*')
                            return true;
                    }

                    if (c == '"')
                        in_string = true;
                    else if (c == '\'')
                        in_char = true;

                    else if (c == 'u' && i + 1 < src.size())
                    {
                        if (src[i + 1] == '"')
                        {
                            in_string = true;
                            ++i;
                        }
                        else if (src[i + 1] == '\'')
                        {
                            in_char = true;
                            ++i;
                        }
                    }
                }
                ++i;
            }
            return false;
        }

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

        [[nodiscard]] std::string_view token_spelling(dcc::sm::SourceFile const& sf, dcc::lex::Token const& tok)
        {
            if (!tok.interned.empty())
                return tok.interned;

            auto text = sf.text(tok.range);
            if (text)
                return *text;

            return {};
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
                case TokenKind::Bang:
                case TokenKind::Colon:
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
                case TokenKind::Tilde:
                    return false;
                default:
                    break;
            }

            if (cur == TokenKind::Star)
            {
                switch (prev)
                {
                    case TokenKind::Star:
                    case TokenKind::Identifier:
                    case TokenKind::Kwu8:
                    case TokenKind::Kwi8:
                    case TokenKind::Kwu16:
                    case TokenKind::Kwi16:
                    case TokenKind::Kwu32:
                    case TokenKind::Kwi32:
                    case TokenKind::Kwu64:
                    case TokenKind::Kwi64:
                    case TokenKind::Kwf32:
                    case TokenKind::Kwf64:
                    case TokenKind::KwChar:
                    case TokenKind::KwVoid:
                    case TokenKind::KwBool:
                    case TokenKind::KwNullT:
                    case TokenKind::KwConst:
                    case TokenKind::KwRestrict:
                    case TokenKind::KwVolatile:
                    case TokenKind::RParen:
                    case TokenKind::RBracket:
                    case TokenKind::RBrace:
                        return false;
                    default:
                        break;
                }
            }

            if (cur == TokenKind::Increment || cur == TokenKind::Decrement)
            {
                switch (prev)
                {
                    case TokenKind::Identifier:
                    case TokenKind::RParen:
                    case TokenKind::RBracket:
                    case TokenKind::IntLiteral:
                    case TokenKind::FloatLiteral:
                        return false;
                    default:
                        break;
                }
            }

            if (cur == TokenKind::LBracket)
            {
                switch (prev)
                {
                    case TokenKind::Star:
                    case TokenKind::Identifier:
                    case TokenKind::Kwu8:
                    case TokenKind::Kwi8:
                    case TokenKind::Kwu16:
                    case TokenKind::Kwi16:
                    case TokenKind::Kwu32:
                    case TokenKind::Kwi32:
                    case TokenKind::Kwu64:
                    case TokenKind::Kwi64:
                    case TokenKind::Kwf32:
                    case TokenKind::Kwf64:
                    case TokenKind::KwChar:
                    case TokenKind::KwVoid:
                    case TokenKind::KwBool:
                    case TokenKind::KwNullT:
                    case TokenKind::KwConst:
                    case TokenKind::KwRestrict:
                    case TokenKind::KwVolatile:
                    case TokenKind::RBracket:
                    case TokenKind::RParen:
                    case TokenKind::RBrace:
                        return false;
                    default:
                        break;
                }
            }

            if (cur == TokenKind::LParen)
            {
                switch (prev)
                {
                    case TokenKind::Identifier:
                    case TokenKind::Bang:
                    case TokenKind::RParen:
                    case TokenKind::RBracket:
                    case TokenKind::Kwu8:
                    case TokenKind::Kwi8:
                    case TokenKind::Kwu16:
                    case TokenKind::Kwi16:
                    case TokenKind::Kwu32:
                    case TokenKind::Kwi32:
                    case TokenKind::Kwu64:
                    case TokenKind::Kwi64:
                    case TokenKind::Kwf32:
                    case TokenKind::Kwf64:
                    case TokenKind::KwChar:
                    case TokenKind::KwVoid:
                    case TokenKind::KwBool:
                    case TokenKind::KwNullT:
                    case TokenKind::KwConst:
                    case TokenKind::KwRestrict:
                    case TokenKind::KwVolatile:
                        return false;
                    default:
                        break;
                }
            }

            return true;
        }

        [[nodiscard]] bool is_major_decl_start(TokenKind k) noexcept
        {
            switch (k)
            {
                case TokenKind::KwModule:
                case TokenKind::KwImport:
                case TokenKind::KwStruct:
                case TokenKind::KwUnion:
                case TokenKind::KwEnum:
                    return true;
                default:
                    return false;
            }
        }

        [[nodiscard]] int count_source_newlines(std::string_view src, Offset begin, Offset end) noexcept
        {
            int n = 0;
            auto limit = std::min(static_cast<std::size_t>(end), src.size());
            for (auto off = static_cast<std::size_t>(begin); off < limit; ++off)
                if (src[off] == '\n')
                    ++n;

            return n;
        }

        [[nodiscard]] int desired_newlines(int src_nl, int indent, TokenKind cur_kind, TokenKind prev_kind) noexcept
        {
            if (src_nl == 0)
                return 0;

            int target = (src_nl >= 4) ? 3 : src_nl;

            if (indent == 0 && target < 2)
            {
                if (prev_kind == TokenKind::Semicolon || prev_kind == TokenKind::RBrace)
                    if (is_major_decl_start(cur_kind) || cur_kind == TokenKind::Identifier)
                        target = 2;
            }

            if (indent > 0 && target > 2)
                target = 2;

            return target;
        }

        [[nodiscard]] std::optional<std::string> format_tokens_to_string(dcc::sm::SourceFile const& sf, dcc::si::string_interner& interner,
                                                                         protocol::FormattingOptions const& options)
        {
            auto src_text = sf.text();

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

            static constexpr std::size_t kMaxFuncParamLineWidth = 80;
            std::vector<bool> is_func_paren(tokens.size(), false);
            {
                for (std::size_t i = 0; i + 1 < tokens.size(); ++i)
                {
                    if (tokens[i].kind == TokenKind::Identifier && tokens[i + 1].kind == TokenKind::LParen)
                    {
                        int pd = 1;
                        bool has_nl = false;
                        auto lparen_end = tokens[i + 1].range.end.offset;
                        std::size_t rparen_idx = 0;
                        for (std::size_t j = i + 2; j < tokens.size() && pd > 0; ++j)
                        {
                            if (tokens[j].kind == TokenKind::LParen)
                                pd++;
                            else if (tokens[j].kind == TokenKind::RParen)
                            {
                                pd--;
                                if (pd == 0)
                                {
                                    rparen_idx = j;
                                    if (count_source_newlines(src_text, lparen_end, tokens[j].range.begin.offset) > 0)
                                        has_nl = true;
                                    break;
                                }
                            }
                        }
                        if (has_nl)
                        {
                            is_func_paren[i + 1] = true;
                        }
                        else if (rparen_idx > 0)
                        {
                            std::size_t start_idx = i;

                            if (i >= 2 && tokens[i - 1].kind == TokenKind::Identifier)
                                start_idx = i - 1;
                            else if (i >= 1)
                                start_idx = i;

                            std::size_t line_len = 0;
                            for (std::size_t k = start_idx; k <= rparen_idx; ++k)
                            {
                                auto const& tk = tokens[k];
                                auto sp = token_spelling(sf, tk);
                                if (k > start_idx)
                                    line_len++;

                                line_len += sp.size();
                            }

                            if (line_len > kMaxFuncParamLineWidth)
                                is_func_paren[i + 1] = true;
                        }
                    }
                }
            }

            std::string result;
            result.reserve(sf.size() + sf.size() / 4);

            int indent = 0;
            int paren_depth = 0;
            int bracket_depth = 0;
            TokenKind prev_kind = TokenKind::Eof;
            Offset prev_end_offset = 0;

            int prev_post_nl = 0;

            bool func_paren_active = false;
            int func_param_indent = 0;
            int func_param_entry_depth = 0;
            int compact_brace_depth = 0;

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

                if (tok.kind == TokenKind::RBrace && indent > 0)
                    indent--;

                if (tok.kind == TokenKind::LParen && i < is_func_paren.size() && is_func_paren[i])
                {
                    func_paren_active = true;
                    func_param_indent = indent;
                    func_param_entry_depth = paren_depth;
                }

                int need_newlines = 0;

                if (i > 0)
                {
                    int src_nl = count_source_newlines(src_text, prev_end_offset, tok.range.begin.offset);

                    int base = prev_post_nl;

                    if (tok.kind == TokenKind::RBrace && prev_kind != TokenKind::LBrace)
                        if (src_nl == 0 && base == 0 && compact_brace_depth == 0)
                            base = 1;

                    if (tok.kind == TokenKind::Semicolon && prev_kind == TokenKind::RBrace)
                        base = 0;

                    if (tok.kind == TokenKind::Comma && prev_kind == TokenKind::RBrace)
                        base = 0;

                    if (tok.kind == TokenKind::KwElse && prev_kind == TokenKind::RBrace)
                        base = 0;

                    if (src_nl > 0)
                    {
                        need_newlines = desired_newlines(src_nl, indent, tok.kind, prev_kind);
                        if (need_newlines < base)
                            need_newlines = base;
                    }
                    else if (base > 0)
                        need_newlines = base;
                    else
                        need_newlines = 0;

                    if (need_newlines >= 2 && tok.kind == TokenKind::KwImport && prev_kind == TokenKind::Semicolon && i >= 2)
                    {
                        for (std::size_t k = i - 1; k > 0; --k)
                        {
                            if (tokens[k - 1].kind == TokenKind::KwImport)
                            {
                                need_newlines = 1;
                                break;
                            }
                            if (tokens[k - 1].kind == TokenKind::Semicolon || tokens[k - 1].kind == TokenKind::RBrace ||
                                tokens[k - 1].kind == TokenKind::KwModule || tokens[k - 1].kind == TokenKind::KwStruct ||
                                tokens[k - 1].kind == TokenKind::KwEnum || tokens[k - 1].kind == TokenKind::KwUnion)
                                break;
                        }
                    }

                    if (func_paren_active && paren_depth == func_param_entry_depth)
                        if (prev_kind == TokenKind::LParen || prev_kind == TokenKind::Comma)
                            if (need_newlines < 1)
                                need_newlines = 1;

                    if (tok.kind == TokenKind::RParen && func_paren_active)
                        if (need_newlines < 1)
                            need_newlines = 1;
                }
                else
                    need_newlines = 0;

                for (int j = prev_post_nl; j < need_newlines; ++j)
                    result += '\n';

                if (need_newlines > 0)
                {
                    int emit_indent = indent;

                    if (func_paren_active && paren_depth == func_param_entry_depth && tok.kind != TokenKind::RParen)
                        emit_indent = indent + 1;

                    if (tok.kind == TokenKind::RParen && func_paren_active)
                        emit_indent = func_param_indent;
                    result += make_indent(emit_indent, options);
                }
                else if (i > 0 && needs_space_before(tok.kind, prev_kind))
                {
                    bool skip_space = false;
                    if (tok.kind == TokenKind::LParen && prev_kind == TokenKind::Star && i >= 2)
                    {
                        auto const& prev2 = tokens[i - 2];
                        switch (prev2.kind)
                        {
                            case TokenKind::Kwu8:
                            case TokenKind::Kwi8:
                            case TokenKind::Kwu16:
                            case TokenKind::Kwi16:
                            case TokenKind::Kwu32:
                            case TokenKind::Kwi32:
                            case TokenKind::Kwu64:
                            case TokenKind::Kwi64:
                            case TokenKind::Kwf32:
                            case TokenKind::Kwf64:
                            case TokenKind::KwChar:
                            case TokenKind::KwVoid:
                            case TokenKind::KwBool:
                            case TokenKind::KwNullT:
                            case TokenKind::KwConst:
                            case TokenKind::KwRestrict:
                            case TokenKind::KwVolatile:
                            case TokenKind::LParen:
                            case TokenKind::RParen:
                            case TokenKind::RBracket:
                            case TokenKind::Star:
                            case TokenKind::Comma:
                                skip_space = true;
                                break;
                            default:
                                break;
                        }
                    }

                    if (!skip_space && prev_kind == TokenKind::Amp)
                    {
                        bool is_unary = false;
                        if (i >= 2)
                        {
                            switch (tokens[i - 2].kind)
                            {
                                case TokenKind::LParen:
                                case TokenKind::LBracket:
                                case TokenKind::LBrace:
                                case TokenKind::Comma:
                                case TokenKind::Eq:
                                case TokenKind::FatArrow:
                                case TokenKind::KwReturn:
                                case TokenKind::Semicolon:
                                case TokenKind::Bang:
                                case TokenKind::Tilde:
                                case TokenKind::Colon:
                                case TokenKind::Question:
                                case TokenKind::Plus:
                                case TokenKind::Minus:
                                case TokenKind::Star:
                                case TokenKind::Amp:
                                case TokenKind::Pipe:
                                case TokenKind::Caret:
                                    is_unary = true;
                                    break;
                                default:
                                    break;
                            }
                        }
                        else if (i == 1)
                            is_unary = true;

                        if (is_unary)
                            skip_space = true;
                    }

                    if (!skip_space && (prev_kind == TokenKind::Increment || prev_kind == TokenKind::Decrement))
                    {
                        bool is_prefix = false;
                        if (i >= 2)
                        {
                            switch (tokens[i - 2].kind)
                            {
                                case TokenKind::LParen:
                                case TokenKind::LBracket:
                                case TokenKind::LBrace:
                                case TokenKind::Comma:
                                case TokenKind::Eq:
                                case TokenKind::FatArrow:
                                case TokenKind::KwReturn:
                                case TokenKind::Semicolon:
                                    is_prefix = true;
                                    break;
                                default:
                                    break;
                            }
                        }
                        else if (i == 1)
                            is_prefix = true;

                        if (is_prefix)
                            skip_space = true;
                    }

                    if (!skip_space)
                        result += ' ';
                }

                if (tok.kind == TokenKind::Colon && i > 0 && need_newlines == 0)
                {
                    bool is_enum = false;
                    for (std::size_t k = i; k > 0; --k)
                    {
                        auto pk = tokens[k - 1].kind;
                        if (pk == TokenKind::KwEnum)
                        {
                            is_enum = true;
                            break;
                        }
                        if (pk == TokenKind::LBrace || pk == TokenKind::RBrace || pk == TokenKind::Semicolon || pk == TokenKind::KwStruct ||
                            pk == TokenKind::KwUnion || pk == TokenKind::KwModule)
                            break;
                    }
                    if (is_enum)
                        result += ' ';
                }

                if (tok.kind == TokenKind::RBrace && compact_brace_depth > 0)
                    result += ' ';

                result += spelling;

                prev_post_nl = 0;

                if (tok.kind == TokenKind::LBrace)
                {
                    indent++;
                    bool compact_brace = (next_kind == TokenKind::RBrace);
                    if (!compact_brace)
                    {
                        bool is_block_context = false;
                        switch (prev_kind)
                        {
                            case TokenKind::RParen: {
                                bool found_compiles = false;
                                for (std::size_t k = i; k > 0; --k)
                                {
                                    auto pk = tokens[k - 1].kind;
                                    if (pk == TokenKind::KwCompiles)
                                    {
                                        found_compiles = true;
                                        break;
                                    }
                                    if (pk == TokenKind::Semicolon || pk == TokenKind::LBrace || pk == TokenKind::Eq || pk == TokenKind::KwStruct ||
                                        pk == TokenKind::KwEnum || pk == TokenKind::KwUnion)
                                        break;
                                }
                                if (!found_compiles)
                                    is_block_context = true;
                                break;
                            }
                            case TokenKind::Identifier: {
                                bool found_equals_or_comma = false;
                                for (std::size_t k = i; k > 0; --k)
                                {
                                    auto pk = tokens[k - 1].kind;
                                    if (pk == TokenKind::Eq || pk == TokenKind::Colon || pk == TokenKind::Comma || pk == TokenKind::LParen ||
                                        pk == TokenKind::KwReturn || pk == TokenKind::FatArrow)
                                    {
                                        found_equals_or_comma = true;
                                        break;
                                    }

                                    if (pk == TokenKind::Semicolon || pk == TokenKind::LBrace || pk == TokenKind::RBrace || pk == TokenKind::KwStruct ||
                                        pk == TokenKind::KwEnum || pk == TokenKind::KwUnion || pk == TokenKind::KwModule || pk == TokenKind::KwImport)
                                        break;

                                    if (pk == TokenKind::Identifier || pk == TokenKind::Dot || (pk >= TokenKind::Kwu8 && pk <= TokenKind::KwNullT) ||
                                        pk == TokenKind::KwConst || pk == TokenKind::KwRestrict || pk == TokenKind::KwVolatile)
                                        continue;

                                    break;
                                }
                                if (!found_equals_or_comma)
                                    is_block_context = true;
                                break;
                            }
                            case TokenKind::Kwu8:
                            case TokenKind::Kwi8:
                            case TokenKind::Kwu16:
                            case TokenKind::Kwi16:
                            case TokenKind::Kwu32:
                            case TokenKind::Kwi32:
                            case TokenKind::Kwu64:
                            case TokenKind::Kwi64:
                            case TokenKind::Kwf32:
                            case TokenKind::Kwf64:
                            case TokenKind::KwChar:
                            case TokenKind::KwVoid:
                            case TokenKind::KwBool:
                            case TokenKind::KwNullT:
                            case TokenKind::KwConst:
                            case TokenKind::KwRestrict:
                            case TokenKind::KwVolatile:
                                is_block_context = true;
                                break;
                            default:
                                break;
                        }
                        if (!is_block_context)
                        {
                            int bd = 1;
                            auto brace_begin = tok.range.end.offset;
                            for (std::size_t j = i + 1; j < tokens.size() && bd > 0; ++j)
                            {
                                if (tokens[j].kind == TokenKind::LBrace)
                                    bd++;
                                else if (tokens[j].kind == TokenKind::RBrace)
                                {
                                    bd--;
                                    if (bd == 0 && count_source_newlines(src_text, brace_begin, tokens[j].range.begin.offset) == 0)
                                        compact_brace = true;
                                    break;
                                }
                            }
                        }
                    }
                    if (compact_brace)
                    {
                        if (next_kind != TokenKind::RBrace)
                        {
                            compact_brace_depth++;
                            result += ' ';
                        }
                    }
                    else
                    {
                        result += '\n';
                        prev_post_nl = 1;
                    }
                }
                else if (tok.kind == TokenKind::RBrace)
                {
                    if (compact_brace_depth > 0)
                        compact_brace_depth--;

                    if (next_kind != TokenKind::Semicolon && next_kind != TokenKind::Comma && next_kind != TokenKind::KwElse)
                    {
                        result += '\n';
                        prev_post_nl = 1;
                    }
                }
                else if (tok.kind == TokenKind::Semicolon && paren_depth <= 0 && bracket_depth <= 0 && compact_brace_depth == 0)
                {
                    result += '\n';
                    prev_post_nl = 1;
                }

                if (tok.kind == TokenKind::RParen && func_paren_active && paren_depth == func_param_entry_depth - 1)
                    func_paren_active = false;

                prev_kind = tok.kind;
                prev_end_offset = tok.range.end.offset;
            }

            if (result.empty() || result.back() != '\n')
                result += '\n';

            return result;
        }

        [[nodiscard]] static bool source_is_simple_module_or_import(std::string_view src) noexcept
        {
            if (src.find("struct ") != std::string_view::npos || src.find("enum ") != std::string_view::npos || src.find("union ") != std::string_view::npos ||
                src.find("using ") != std::string_view::npos || src.find("public ") != std::string_view::npos || src.find("let ") != std::string_view::npos ||
                src.find('{') != std::string_view::npos || src.find('(') != std::string_view::npos)
                return false;

            return true;
        }

        [[nodiscard]] std::optional<std::string> try_format_ast(dcc::sm::SourceFile const& sf, dcc::si::string_interner&, protocol::FormattingOptions const&)
        {
            auto src = sf.text();
            if (!source_is_simple_module_or_import(src))
                return std::nullopt;

            struct Line
            {
                enum Kind
                {
                    Module,
                    Import,
                    Other
                };

                Kind kind;
                std::string text;
            };

            std::vector<Line> lines;
            bool has_module = false;

            std::size_t pos = 0;
            while (pos < src.size())
            {
                while (pos < src.size() && (src[pos] == ' ' || src[pos] == '\t' || src[pos] == '\r'))
                    ++pos;
                if (pos >= src.size())
                    break;

                auto line_start = pos;
                while (pos < src.size() && src[pos] != '\n')
                    ++pos;
                std::string_view line = src.substr(line_start, pos - line_start);
                if (pos < src.size())
                    ++pos;

                while (!line.empty() && (line.back() == ' ' || line.back() == '\t' || line.back() == '\r'))
                    line.remove_suffix(1);
                if (line.empty())
                    continue;

                if (line.starts_with("module ") && line.ends_with(';'))
                {
                    if (has_module)
                        return std::nullopt;
                    has_module = true;
                    lines.push_back({Line::Module, std::string{line}});
                }
                else if (line.starts_with("import ") && line.ends_with(';'))
                    lines.push_back({Line::Import, std::string{line}});
                else
                    return std::nullopt;
            }

            std::string result;
            for (std::size_t i = 0; i < lines.size(); ++i)
            {
                if (i > 0)
                {
                    if (lines[i - 1].kind != lines[i].kind)
                        result += '\n';
                }
                result += lines[i].text;
                result += '\n';
            }

            if (!result.empty() && result.back() != '\n')
                result += '\n';
            if (result.empty())
                return std::nullopt;
            return result;
        }

    } // anonymous namespace

    std::optional<protocol::TextEdit> format_document(dcc::sm::SourceFile const& sf, dcc::si::string_interner& interner,
                                                      protocol::FormattingOptions const& options)
    {
        auto src_text = sf.text();
        if (source_contains_comments(src_text))
            return std::nullopt;

        auto ast_result = try_format_ast(sf, interner, options);
        if (ast_result.has_value())
        {
            auto end_offset = static_cast<dcc::sm::Offset>(sf.size());
            auto end_pos = sf.lsp_position(end_offset);
            if (!end_pos)
                return std::nullopt;

            protocol::TextEdit edit;
            edit.range.start.line = 0;
            edit.range.start.character = 0;
            edit.range.end.line = end_pos->line;
            edit.range.end.character = end_pos->character;
            edit.newText = std::move(*ast_result);
            return edit;
        }

        auto token_result = format_tokens_to_string(sf, interner, options);
        if (!token_result.has_value())
            return std::nullopt;

        auto end_offset = static_cast<dcc::sm::Offset>(sf.size());
        auto end_pos = sf.lsp_position(end_offset);
        if (!end_pos)
            return std::nullopt;

        protocol::TextEdit edit;
        edit.range.start.line = 0;
        edit.range.start.character = 0;
        edit.range.end.line = end_pos->line;
        edit.range.end.character = end_pos->character;
        edit.newText = std::move(*token_result);

        return edit;
    }

} // namespace dccd::format
