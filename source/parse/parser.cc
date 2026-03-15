#include <cassert>
#include <format>
#include <parse/parser.hh>
#include <utility>

namespace dcc::parse
{
    using TK = lex::TokenKind;

    Parser::Parser(lex::Lexer& lexer, AstArena& arena, diag::DiagnosticPrinter& diag) noexcept : m_lexer{lexer}, m_arena{arena}, m_diag{diag} {}

    const lex::Token& Parser::peek(std::size_t ahead)
    {
        while (m_pos + ahead >= m_tokens.size())
            m_tokens.push_back(m_lexer.next());

        return m_tokens[m_pos + ahead];
    }

    const lex::Token& Parser::advance()
    {
        const auto& tok = peek();
        m_prev_end = tok.range.end;
        ++m_pos;

        return m_tokens[m_pos - 1];
    }

    const lex::Token& Parser::previous() const
    {
        return m_tokens[m_pos - 1];
    }

    bool Parser::check(TK k)
    {
        return peek().kind == k;
    }

    bool Parser::check_ahead(std::size_t n, TK k)
    {
        return peek(n).kind == k;
    }

    bool Parser::match(TK k)
    {
        if (!check(k))
            return false;

        advance();
        return true;
    }

    const lex::Token& Parser::expect(TK k, std::string_view ctx)
    {
        if (check(k))
            return advance();

        error_at_current(std::format("expected {} {}", lex::to_string(k), ctx));

        return peek();
    }

    bool Parser::at_end()
    {
        return peek().kind == TK::Eof;
    }

    sm::Location Parser::loc()
    {
        return peek().range.begin;
    }

    sm::SourceRange Parser::range_from(sm::Location begin)
    {
        return {begin, m_prev_end};
    }

    sm::SourceRange Parser::single_range()
    {
        return peek().range;
    }

    void Parser::error(sm::SourceRange range, std::string msg)
    {
        if (m_panic || m_speculative)
            return;

        m_panic = true;
        m_had_error = true;
        m_diag.emit(diag::error(std::move(msg)).with_primary(range, "here"));
    }

    void Parser::error_at_current(std::string msg)
    {
        error(peek().range, std::move(msg));
    }

    void Parser::synchronize()
    {
        m_panic = false;
        while (!at_end())
        {
            if (previous().kind == TK::Semicolon)
                return;

            switch (peek().kind)
            {
                case TK::KwStruct:
                case TK::KwEnum:
                case TK::KwUnion:
                case TK::KwUsing:
                case TK::KwIf:
                case TK::KwWhile:
                case TK::KwFor:
                case TK::KwDo:
                case TK::KwReturn:
                case TK::KwDefer:
                case TK::KwMatch:
                case TK::KwImport:
                case TK::KwModule:
                case TK::KwPublic:
                case TK::KwExtern:
                case TK::RBrace:
                    return;
                default:
                    advance();
            }
        }
    }

    int Parser::binary_precedence(TK k) noexcept
    {
        switch (k)
        {
            case TK::PipePipe:
                return 1;
            case TK::AmpAmp:
                return 2;
            case TK::Pipe:
                return 3;
            case TK::Caret:
                return 4;
            case TK::Amp:
                return 5;
            case TK::EqEq:
            case TK::BangEq:
                return 6;
            case TK::Lt:
            case TK::Gt:
            case TK::LtEq:
            case TK::GtEq:
                return 7;
            case TK::LtLt:
            case TK::GtGt:
                return 8;
            case TK::Plus:
            case TK::Minus:
                return 9;
            case TK::Star:
            case TK::Slash:
            case TK::Percent:
                return 10;
            default:
                return -1;
        }
    }

    ast::BinaryOp Parser::to_binary_op(TK k) noexcept
    {
        switch (k)
        {
            case TK::Plus:
                return ast::BinaryOp::Add;
            case TK::Minus:
                return ast::BinaryOp::Sub;
            case TK::Star:
                return ast::BinaryOp::Mul;
            case TK::Slash:
                return ast::BinaryOp::Div;
            case TK::Percent:
                return ast::BinaryOp::Mod;
            case TK::Amp:
                return ast::BinaryOp::BitAnd;
            case TK::Pipe:
                return ast::BinaryOp::BitOr;
            case TK::Caret:
                return ast::BinaryOp::BitXor;
            case TK::LtLt:
                return ast::BinaryOp::Shl;
            case TK::GtGt:
                return ast::BinaryOp::Shr;
            case TK::EqEq:
                return ast::BinaryOp::Eq;
            case TK::BangEq:
                return ast::BinaryOp::Ne;
            case TK::Lt:
                return ast::BinaryOp::Lt;
            case TK::LtEq:
                return ast::BinaryOp::Le;
            case TK::Gt:
                return ast::BinaryOp::Gt;
            case TK::GtEq:
                return ast::BinaryOp::Ge;
            case TK::AmpAmp:
                return ast::BinaryOp::LogAnd;
            case TK::PipePipe:
                return ast::BinaryOp::LogOr;
            default:
                std::unreachable();
        }
    }

    ast::AssignOp Parser::to_assign_op(TK k) noexcept
    {
        switch (k)
        {
            case TK::Eq:
                return ast::AssignOp::Simple;
            case TK::PlusEq:
                return ast::AssignOp::Add;
            case TK::MinusEq:
                return ast::AssignOp::Sub;
            case TK::StarEq:
                return ast::AssignOp::Mul;
            case TK::SlashEq:
                return ast::AssignOp::Div;
            case TK::PercentEq:
                return ast::AssignOp::Mod;
            case TK::AmpEq:
                return ast::AssignOp::BitAnd;
            case TK::PipeEq:
                return ast::AssignOp::BitOr;
            case TK::CaretEq:
                return ast::AssignOp::BitXor;
            case TK::LtLtEq:
                return ast::AssignOp::Shl;
            case TK::GtGtEq:
                return ast::AssignOp::Shr;
            default:
                std::unreachable();
        }
    }

    ast::UnaryOp Parser::to_prefix_op(TK k) noexcept
    {
        switch (k)
        {
            case TK::Minus:
                return ast::UnaryOp::Negate;
            case TK::Tilde:
                return ast::UnaryOp::BitNot;
            case TK::Bang:
                return ast::UnaryOp::LogNot;
            case TK::Star:
                return ast::UnaryOp::Deref;
            case TK::Amp:
                return ast::UnaryOp::AddressOf;
            default:
                std::unreachable();
        }
    }

    bool Parser::is_type_keyword(TK k) const noexcept
    {
        switch (k)
        {
            case TK::Kwi8:
            case TK::Kwu8:
            case TK::Kwi16:
            case TK::Kwu16:
            case TK::Kwi32:
            case TK::Kwu32:
            case TK::Kwi64:
            case TK::Kwu64:
            case TK::Kwf32:
            case TK::Kwf64:
            case TK::KwBool:
            case TK::KwVoid:
            case TK::KwNullT:
                return true;
            default:
                return false;
        }
    }

    ast::PrimitiveKind Parser::to_primitive(TK k) const
    {
        switch (k)
        {
            case TK::Kwi8:
                return ast::PrimitiveKind::I8;
            case TK::Kwu8:
                return ast::PrimitiveKind::U8;
            case TK::Kwi16:
                return ast::PrimitiveKind::I16;
            case TK::Kwu16:
                return ast::PrimitiveKind::U16;
            case TK::Kwi32:
                return ast::PrimitiveKind::I32;
            case TK::Kwu32:
                return ast::PrimitiveKind::U32;
            case TK::Kwi64:
                return ast::PrimitiveKind::I64;
            case TK::Kwu64:
                return ast::PrimitiveKind::U64;
            case TK::Kwf32:
                return ast::PrimitiveKind::F32;
            case TK::Kwf64:
                return ast::PrimitiveKind::F64;
            case TK::KwBool:
                return ast::PrimitiveKind::Bool;
            case TK::KwVoid:
                return ast::PrimitiveKind::Void;
            case TK::KwNullT:
                return ast::PrimitiveKind::NullT;
            default:
                std::unreachable();
        }
    }

    bool Parser::is_ambiguous_decl_or_expr() noexcept
    {
        if (!check(TK::Identifier) || is_type_keyword(peek().kind))
            return false;

        std::size_t i = 1;

        while (peek(i).is(TK::Dot) && peek(i + 1).is(TK::Identifier))
            i += 2;

        if (peek(i).is(TK::LParen))
        {
            ++i;

            int depth = 1;
            while (depth > 0 && !peek(i).is(TK::Eof))
            {
                if (peek(i).is(TK::LParen))
                    ++depth;
                if (peek(i).is(TK::RParen))
                    --depth;

                ++i;
            }
        }

        if (!peek(i).is(TK::Star))
            return false;

        while (peek(i).is(TK::Star))
            ++i;

        if (!peek(i).is(TK::Identifier))
            return false;

        auto after = peek(i + 1).kind;
        return after == TK::Semicolon || after == TK::Eq;
    }

    bool Parser::looks_like_local_decl()
    {
        std::size_t i = 0;

        while (peek(i).is_one_of(TK::KwConst, TK::KwVolatile, TK::KwRestrict))
            ++i;

        if (peek(i).is(TK::LBracket) && peek(i + 1).is(TK::RBracket))
            return true;

        if (is_type_keyword(peek(i).kind))
            ++i;
        else if (peek(i).is(TK::Identifier))
        {
            ++i;
            while (peek(i).is(TK::Dot) && peek(i + 1).is(TK::Identifier))
                i += 2;
        }
        else
            return false;

        if (peek(i).is(TK::LParen))
        {
            ++i;

            int depth = 1;
            while (depth > 0 && !peek(i).is(TK::Eof))
            {
                if (peek(i).is(TK::LParen))
                    ++depth;
                if (peek(i).is(TK::RParen))
                    --depth;
                ++i;
            }
        }

        while (peek(i).is(TK::Star))
            ++i;

        while (peek(i).is(TK::LBracket))
        {
            ++i;

            int depth = 1;
            while (depth > 0 && !peek(i).is(TK::Eof))
            {
                if (peek(i).is(TK::LBracket))
                    ++depth;
                if (peek(i).is(TK::RBracket))
                    --depth;

                ++i;
            }
        }

        if (!peek(i).is(TK::Identifier))
            return false;

        auto after = peek(i + 1).kind;
        return after == TK::Semicolon || after == TK::Eq;
    }

    bool Parser::skip_balanced(TK open, TK close)
    {
        if (!match(open))
            return false;

        int depth = 1;
        while (depth > 0 && !at_end())
        {
            if (check(open))
                ++depth;
            if (check(close))
                --depth;

            advance();
        }
        return true;
    }

    std::vector<ast::Attribute> Parser::parse_attribute_list()
    {
        std::vector<ast::Attribute> attrs;

        while (check(TK::Hash) && check_ahead(1, TK::LBracket))
        {
            auto begin = loc();
            advance();
            advance();

            std::vector<lex::Token> raw;
            int depth = 1;
            while (!at_end() && depth > 0)
            {
                if (check(TK::LBracket))
                    ++depth;
                else if (check(TK::RBracket))
                {
                    --depth;
                    if (depth == 0)
                        break;
                }

                raw.push_back(peek());
                advance();
            }
            expect(TK::RBracket, "to close attribute");

            std::vector<std::string_view> entries;
            std::size_t entry_start = 0;
            int paren_depth = 0;

            for (std::size_t i = 0; i < raw.size(); ++i)
            {
                const auto& t = raw[i];
                if (t.is_one_of(TK::LParen, TK::LBrace, TK::LBracket))
                    ++paren_depth;
                else if (t.is_one_of(TK::RParen, TK::RBrace, TK::RBracket))
                    --paren_depth;
                else if (t.is(TK::Comma) && paren_depth == 0)
                {
                    if (entry_start < i)
                    {
                        const auto& first = raw[entry_start];
                        const auto& last = raw[i - 1];
                        entries.push_back({first.text.data(), static_cast<std::size_t>(last.text.data() + last.text.size() - first.text.data())});
                    }
                    entry_start = i + 1;
                }
            }

            if (entry_start < raw.size())
            {
                const auto& first = raw[entry_start];
                const auto& last = raw[raw.size() - 1];
                entries.push_back({first.text.data(), static_cast<std::size_t>(last.text.data() + last.text.size() - first.text.data())});
            }

            auto raw_span = m_arena.to_const_span(raw);
            auto entries_span = m_arena.to_span(entries);
            attrs.push_back(ast::Attribute{raw_span, entries_span, range_from(begin)});
        }

        return attrs;
    }

    ast::TranslationUnit* Parser::parse()
    {
        auto begin = loc();
        auto* mod = parse_module_decl();

        std::vector<ast::Decl*> decls;
        if (mod)
            decls.push_back(mod);

        while (!at_end())
        {
            auto* d = parse_top_level_decl();
            if (d)
                decls.push_back(d);
            else
                synchronize();
        }

        return m_arena.create<ast::TranslationUnit>(range_from(begin), mod, m_arena.to_span(decls));
    }

    ast::ModuleDecl* Parser::parse_module_decl()
    {
        if (!check(TK::KwModule))
            return nullptr;

        auto begin = loc();
        advance();

        std::vector<si::InternedString> path;
        path.push_back(expect(TK::Identifier, "in module path").interned);
        while (match(TK::Dot))
            path.push_back(expect(TK::Identifier, "in module path").interned);

        expect(TK::Semicolon, "after module declaration");
        return m_arena.create<ast::ModuleDecl>(range_from(begin), m_arena.to_const_span(path));
    }

    ast::Decl* Parser::parse_top_level_decl()
    {
        auto attrs = parse_attribute_list();

        auto vis = ast::Visibility::Private;
        if (match(TK::KwPublic))
            vis = ast::Visibility::Public;

        switch (peek().kind)
        {
            case TK::KwImport:
                return parse_import_decl(vis);
            case TK::KwStruct:
                return parse_struct_decl(vis, std::move(attrs));
            case TK::KwUnion:
                return parse_union_decl(vis, std::move(attrs));
            case TK::KwEnum:
                return parse_enum_decl(vis, std::move(attrs));
            case TK::KwUsing:
                return parse_using_decl(vis);
            default:
                return parse_func_or_var_decl(vis, std::move(attrs));
        }
    }

    ast::ImportDecl* Parser::parse_import_decl(ast::Visibility vis)
    {
        auto begin = loc();
        expect(TK::KwImport, "internal error");

        std::vector<si::InternedString> path;
        path.push_back(expect(TK::Identifier, "in import path").interned);
        while (match(TK::Dot))
            path.push_back(expect(TK::Identifier, "in import path").interned);

        expect(TK::Semicolon, "after import");
        return m_arena.create<ast::ImportDecl>(range_from(begin), m_arena.to_const_span(path), vis);
    }

    ast::StructDecl* Parser::parse_struct_decl(ast::Visibility vis, std::vector<ast::Attribute> attrs)
    {
        auto begin = loc();
        expect(TK::KwStruct, "internal error");
        auto name = expect(TK::Identifier, "for struct name").interned;

        std::vector<ast::Decl*> tpl_params;
        if (check(TK::LParen))
            tpl_params = parse_template_param_list();

        expect(TK::LBrace, "to open struct body");

        std::vector<ast::FieldDecl*> fields;
        std::vector<ast::FunctionDecl*> methods;

        while (!check(TK::RBrace) && !at_end())
        {
            auto member_begin = loc();

            auto member_vis = ast::Visibility::Private;
            if (match(TK::KwPublic))
                member_vis = ast::Visibility::Public;

            auto saved = save();
            bool saved_panic = m_panic;
            bool saved_error = m_had_error;
            m_speculative = true;
            m_panic = false;
            m_had_error = false;

            bool is_method = false;
            ast::StorageClass sc = ast::StorageClass::None;

            if (match(TK::KwStatic))
                sc = ast::StorageClass::Static;
            else if (match(TK::KwExtern))
                sc = ast::StorageClass::Extern;

            auto* maybe_type = parse_type();
            if (maybe_type && check(TK::Identifier))
            {
                advance();
                is_method = check(TK::LParen);
            }

            m_speculative = false;
            m_panic = saved_panic;
            m_had_error = saved_error;
            restore(saved);

            if (is_method)
            {
                if (match(TK::KwStatic))
                    sc = ast::StorageClass::Static;
                else if (match(TK::KwExtern))
                    sc = ast::StorageClass::Extern;

                auto* ret_type = parse_type();
                auto method_name = expect(TK::Identifier, "for method name").interned;

                std::vector<ast::Decl*> tpl;
                if (check(TK::LParen))
                {
                    auto inner_saved = save();
                    skip_balanced(TK::LParen, TK::RParen);
                    bool two_parens = check(TK::LParen);
                    restore(inner_saved);
                    if (two_parens)
                        tpl = parse_template_param_list();
                }

                auto* method = parse_function_decl(ret_type, method_name, std::move(tpl), member_vis, member_begin, sc);
                methods.push_back(method);
            }
            else
            {
                bool field_panic = m_panic;
                m_panic = false;

                auto* field = parse_field_decl();
                if (field)
                    fields.push_back(field);

                if (m_panic)
                {
                    m_panic = false;
                    while (!at_end() && !check(TK::RBrace))
                    {
                        if (previous().kind == TK::Semicolon)
                            break;

                        if (is_type_keyword(peek().kind) || peek().is(TK::Identifier) ||
                            peek().is_one_of(TK::KwPublic, TK::KwStatic, TK::KwExtern, TK::KwConst))
                            break;

                        if (check(TK::LBrace))
                        {
                            skip_balanced(TK::LBrace, TK::RBrace);
                            break;
                        }

                        advance();
                    }
                }
                else
                    m_panic = m_panic || field_panic;
            }
        }

        expect(TK::RBrace, "to close struct body");

        return m_arena.create<ast::StructDecl>(range_from(begin), name, m_arena.to_span(fields), m_arena.to_span(methods), m_arena.to_span(tpl_params),
                                               m_arena.to_const_span(attrs), vis);
    }

    ast::UnionDecl* Parser::parse_union_decl(ast::Visibility vis, std::vector<ast::Attribute> attrs)
    {
        auto begin = loc();
        expect(TK::KwUnion, "internal error");
        auto name = expect(TK::Identifier, "for union name").interned;

        expect(TK::LBrace, "to open union body");

        std::vector<ast::FieldDecl*> fields;
        std::vector<ast::FunctionDecl*> methods;

        while (!check(TK::RBrace) && !at_end())
        {
            auto member_begin = loc();

            auto member_vis = ast::Visibility::Private;
            if (match(TK::KwPublic))
                member_vis = ast::Visibility::Public;

            auto saved = save();
            bool saved_panic = m_panic;
            bool saved_error = m_had_error;
            m_speculative = true;
            m_panic = false;
            m_had_error = false;

            bool is_method = false;
            ast::StorageClass sc = ast::StorageClass::None;

            if (match(TK::KwStatic))
                sc = ast::StorageClass::Static;
            else if (match(TK::KwExtern))
                sc = ast::StorageClass::Extern;

            auto* maybe_type = parse_type();
            if (maybe_type && check(TK::Identifier))
            {
                advance();
                is_method = check(TK::LParen);
            }

            m_speculative = false;
            m_panic = saved_panic;
            m_had_error = saved_error;
            restore(saved);

            if (is_method)
            {
                if (match(TK::KwStatic))
                    sc = ast::StorageClass::Static;
                else if (match(TK::KwExtern))
                    sc = ast::StorageClass::Extern;

                auto* ret_type = parse_type();
                auto method_name = expect(TK::Identifier, "for method name").interned;

                std::vector<ast::Decl*> tpl;
                if (check(TK::LParen))
                {
                    auto inner_saved = save();
                    skip_balanced(TK::LParen, TK::RParen);
                    bool two_parens = check(TK::LParen);
                    restore(inner_saved);
                    if (two_parens)
                        tpl = parse_template_param_list();
                }

                auto* method = parse_function_decl(ret_type, method_name, std::move(tpl), member_vis, member_begin, sc);
                methods.push_back(method);
            }
            else
            {
                bool field_panic = m_panic;
                m_panic = false;

                auto* field = parse_field_decl();
                if (field)
                    fields.push_back(field);

                if (m_panic)
                {
                    m_panic = false;
                    while (!at_end() && !check(TK::RBrace))
                    {
                        if (previous().kind == TK::Semicolon)
                            break;

                        if (is_type_keyword(peek().kind) || peek().is(TK::Identifier) ||
                            peek().is_one_of(TK::KwPublic, TK::KwStatic, TK::KwExtern, TK::KwConst))
                            break;

                        if (check(TK::LBrace))
                        {
                            skip_balanced(TK::LBrace, TK::RBrace);
                            break;
                        }

                        advance();
                    }
                }
                else
                    m_panic = m_panic || field_panic;
            }
        }

        expect(TK::RBrace, "to close union body");

        return m_arena.create<ast::UnionDecl>(range_from(begin), name, m_arena.to_span(fields), m_arena.to_span(methods), m_arena.to_const_span(attrs), vis);
    }

    ast::EnumDecl* Parser::parse_enum_decl(ast::Visibility vis, std::vector<ast::Attribute> attrs)
    {
        auto begin = loc();
        expect(TK::KwEnum, "internal error");
        auto name = expect(TK::Identifier, "for enum name").interned;

        ast::TypeExpr* underlying = nullptr;
        if (match(TK::Colon))
            underlying = parse_type();

        expect(TK::LBrace, "to open enum body");

        std::vector<ast::EnumVariantDecl*> variants;
        std::vector<ast::FunctionDecl*> methods;

        while (!check(TK::RBrace) && !at_end())
        {
            variants.push_back(parse_enum_variant());
            match(TK::Comma);
        }

        expect(TK::RBrace, "to close enum body");

        return m_arena.create<ast::EnumDecl>(range_from(begin), name, underlying, m_arena.to_span(variants), m_arena.to_span(methods),
                                             m_arena.to_const_span(attrs), vis);
    }

    ast::EnumVariantDecl* Parser::parse_enum_variant()
    {
        auto begin = loc();
        auto name = expect(TK::Identifier, "for enum variant").interned;

        ast::Expr* disc = nullptr;
        if (match(TK::Eq))
            disc = parse_expr();

        std::vector<ast::TypeExpr*> payload;
        if (match(TK::LParen))
        {
            if (!check(TK::RParen))
            {
                payload.push_back(parse_type());
                while (match(TK::Comma))
                    payload.push_back(parse_type());
            }
            expect(TK::RParen, "after variant payload");
        }

        return m_arena.create<ast::EnumVariantDecl>(range_from(begin), name, disc, m_arena.to_span(payload));
    }

    ast::UsingDecl* Parser::parse_using_decl(ast::Visibility vis)
    {
        auto begin = loc();
        expect(TK::KwUsing, "internal error");

        bool is_export = false;
        if (match(TK::KwPublic))
            is_export = true;

        if (check(TK::Identifier) && check_ahead(1, TK::Eq))
        {
            auto name = advance().interned;
            advance();

            auto saved = save();
            bool saved_panic = m_panic;
            bool saved_error = m_had_error;
            m_speculative = true;
            m_panic = false;
            m_had_error = false;

            std::vector<si::InternedString> target_path;
            bool is_dotted_symbol = false;

            if (check(TK::Identifier))
            {
                target_path.push_back(advance().interned);
                while (match(TK::Dot))
                    if (check(TK::Identifier))
                        target_path.push_back(advance().interned);
                    else
                        break;

                if (target_path.size() > 1 && check(TK::Semicolon))
                    is_dotted_symbol = true;
            }

            m_speculative = false;
            m_panic = saved_panic;
            m_had_error = saved_error;
            restore(saved);

            if (is_dotted_symbol)
            {
                auto path = parse_dotted_path();
                expect(TK::Semicolon, "after using declaration");
                return m_arena.create<ast::UsingDecl>(range_from(begin), name, m_arena.to_const_span(path), nullptr, vis, is_export);
            }
            else
            {
                auto* aliased = parse_type();
                expect(TK::Semicolon, "after using declaration");
                return m_arena.create<ast::UsingDecl>(range_from(begin), name, aliased, vis, is_export);
            }
        }

        std::vector<si::InternedString> path;
        path.push_back(expect(TK::Identifier, "in using path").interned);

        while (match(TK::Dot))
        {
            if (check(TK::LBrace))
            {
                advance();

                std::vector<si::InternedString> names;
                if (!check(TK::RBrace))
                {
                    names.push_back(expect(TK::Identifier, "in group import").interned);
                    while (match(TK::Comma))
                    {
                        if (check(TK::RBrace))
                            break;

                        names.push_back(expect(TK::Identifier, "in group import").interned);
                    }
                }

                expect(TK::RBrace, "after group import list");
                expect(TK::Semicolon, "after using group import");

                return m_arena.create<ast::UsingDecl>(range_from(begin), m_arena.to_const_span(path), m_arena.to_const_span(names), vis, is_export);
            }

            path.push_back(expect(TK::Identifier, "in using path").interned);
        }

        expect(TK::Semicolon, "after using import");

        return m_arena.create<ast::UsingDecl>(range_from(begin), m_arena.to_const_span(path), vis, is_export);
    }

    ast::Decl* Parser::parse_func_or_var_decl(ast::Visibility vis, std::vector<ast::Attribute> attrs)
    {
        auto begin = loc();
        ast::StorageClass sc = ast::StorageClass::None;
        if (match(TK::KwStatic))
            sc = ast::StorageClass::Static;
        else if (match(TK::KwExtern))
            sc = ast::StorageClass::Extern;

        ast::Qualifier var_quals = ast::Qualifier::None;
        if (check(TK::KwConst))
            var_quals = var_quals | ast::Qualifier::Const;

        auto* type = parse_type();
        if (!type)
        {
            error_at_current("expected type in declaration");
            return nullptr;
        }

        auto name = expect(TK::Identifier, "for declaration name").interned;

        if (check(TK::LParen))
        {
            auto saved = save();
            skip_balanced(TK::LParen, TK::RParen);
            bool two_parens = check(TK::LParen);
            restore(saved);

            if (two_parens)
            {
                auto tpl = parse_template_param_list();
                return parse_function_decl(type, name, std::move(tpl), vis, begin, sc, std::move(attrs));
            }

            saved = save();
            skip_balanced(TK::LParen, TK::RParen);
            bool has_body = check(TK::LBrace);
            bool has_semi = check(TK::Semicolon);
            restore(saved);

            if (has_body)
                return parse_function_decl(type, name, {}, vis, begin, sc, std::move(attrs));

            if (has_semi && sc == ast::StorageClass::Extern)
                return parse_function_decl(type, name, {}, vis, begin, sc, std::move(attrs));
        }

        return parse_var_decl_rest(type, name, var_quals, begin, sc);
    }

    ast::FunctionDecl* Parser::parse_function_decl(ast::TypeExpr* ret, si::InternedString name, std::vector<ast::Decl*> tpl, ast::Visibility vis,
                                                   sm::Location begin, ast::StorageClass sc, std::vector<ast::Attribute> attrs)
    {
        auto params = parse_param_list();

        ast::BlockStmt* body = nullptr;
        if (sc == ast::StorageClass::Extern && check(TK::Semicolon))
            expect(TK::Semicolon, "after extern function declaration");
        else
            body = parse_block();

        auto param_span = m_arena.to_span(params);
        std::span<ast::ParamDecl* const> p_span{param_span.data(), param_span.size()};
        auto tpl_span = m_arena.to_span(tpl);

        return m_arena.create<ast::FunctionDecl>(range_from(begin), name, ret, p_span, tpl_span, body, vis, sc, m_arena.to_const_span(attrs));
    }

    std::vector<ast::Decl*> Parser::parse_template_param_list()
    {
        expect(TK::LParen, "to open template parameter list");
        std::vector<ast::Decl*> params;
        if (!check(TK::RParen))
        {
            do
            {
                auto tbegin = loc();
                if (check(TK::Identifier) && (check_ahead(1, TK::Comma) || check_ahead(1, TK::RParen)))
                {
                    auto pname = advance().interned;
                    params.push_back(m_arena.create<ast::TemplateTypeParamDecl>(range_from(tbegin), pname));
                }
                else
                {
                    auto* ptype = parse_type();
                    auto pname = expect(TK::Identifier, "for template value parameter").interned;
                    params.push_back(m_arena.create<ast::TemplateValueParamDecl>(range_from(tbegin), pname, ptype));
                }
            } while (match(TK::Comma));
        }
        expect(TK::RParen, "to close template parameter list");
        return params;
    }

    std::vector<ast::ParamDecl*> Parser::parse_param_list()
    {
        expect(TK::LParen, "to open parameter list");
        std::vector<ast::ParamDecl*> params;
        if (!check(TK::RParen))
            do
                params.push_back(parse_param_decl());
            while (match(TK::Comma));

        expect(TK::RParen, "to close parameter list");
        return params;
    }

    ast::ParamDecl* Parser::parse_param_decl()
    {
        auto begin = loc();
        auto* type = parse_type();
        auto name = expect(TK::Identifier, "for parameter name").interned;

        ast::Expr* def = nullptr;
        if (match(TK::Eq))
            def = parse_expr();

        return m_arena.create<ast::ParamDecl>(range_from(begin), name, type, def);
    }

    ast::FieldDecl* Parser::parse_field_decl()
    {
        auto begin = loc();
        auto vis = ast::Visibility::Private;
        if (match(TK::KwPublic))
            vis = ast::Visibility::Public;

        auto* type = parse_type();
        auto name = expect(TK::Identifier, "for field name").interned;

        ast::Expr* def = nullptr;
        if (match(TK::Eq))
            def = parse_expr();

        expect(TK::Semicolon, "after field declaration");
        return m_arena.create<ast::FieldDecl>(range_from(begin), name, type, def, vis);
    }

    ast::VarDecl* Parser::parse_var_decl_rest(ast::TypeExpr* type, si::InternedString name, ast::Qualifier quals, sm::Location begin, ast::StorageClass sc)
    {
        ast::Expr* init = nullptr;
        if (match(TK::Eq))
            init = parse_expr();

        expect(TK::Semicolon, "after variable declaration");
        return m_arena.create<ast::VarDecl>(range_from(begin), name, type, init, quals, sc);
    }

    ast::Stmt* Parser::parse_stmt()
    {
        switch (peek().kind)
        {
            case TK::LBrace:
                return parse_block();
            case TK::KwStatic:
                if (check_ahead(1, TK::KwIf))
                    return parse_if_stmt();
                return parse_decl_or_expr_stmt();
            case TK::KwIf:
                return parse_if_stmt();
            case TK::KwWhile:
                return parse_while_stmt();
            case TK::KwFor:
                return parse_for_stmt();
            case TK::KwDo:
                return parse_do_while_stmt();
            case TK::KwReturn:
                return parse_return_stmt();
            case TK::KwBreak:
                return parse_break_stmt();
            case TK::KwContinue:
                return parse_continue_stmt();
            case TK::KwDefer:
                return parse_defer_stmt();
            case TK::KwMatch:
                return parse_match_stmt();
            case TK::Semicolon: {
                auto r = single_range();
                advance();
                return m_arena.create<ast::EmptyStmt>(r);
            }
            default:
                return parse_decl_or_expr_stmt();
        }
    }

    ast::BlockStmt* Parser::parse_block()
    {
        auto begin = loc();
        expect(TK::LBrace, "to open block");

        std::vector<ast::Stmt*> stmts;
        while (!check(TK::RBrace) && !at_end())
        {
            auto* s = parse_stmt();
            if (s)
                stmts.push_back(s);
            else
                synchronize();
        }

        expect(TK::RBrace, "to close block");
        return m_arena.create<ast::BlockStmt>(range_from(begin), m_arena.to_span(stmts));
    }

    ast::IfStmt* Parser::parse_if_stmt()
    {
        auto begin = loc();
        bool is_static = false;

        if (check(TK::KwStatic) && check_ahead(1, TK::KwIf))
        {
            advance();
            is_static = true;
        }

        expect(TK::KwIf, "internal error");

        bool has_paren = match(TK::LParen);
        auto* cond = parse_expr();
        if (has_paren)
            expect(TK::RParen, "after if condition");

        auto* then_branch = parse_block();

        ast::Stmt* else_branch = nullptr;
        if (match(TK::KwElse))
        {
            if (check(TK::KwIf))
                else_branch = parse_if_stmt();
            else
                else_branch = parse_block();
        }

        return m_arena.create<ast::IfStmt>(range_from(begin), cond, then_branch, else_branch, is_static);
    }

    ast::WhileStmt* Parser::parse_while_stmt()
    {
        auto begin = loc();
        expect(TK::KwWhile, "internal error");

        bool has_paren = match(TK::LParen);
        auto* cond = parse_expr();
        if (has_paren)
            expect(TK::RParen, "after while condition");

        auto* body = parse_block();
        return m_arena.create<ast::WhileStmt>(range_from(begin), cond, body);
    }

    ast::ForStmt* Parser::parse_for_stmt()
    {
        auto begin = loc();
        expect(TK::KwFor, "internal error");
        expect(TK::LParen, "after 'for'");

        ast::Stmt* init = nullptr;
        if (!check(TK::Semicolon))
            init = parse_decl_or_expr_stmt();
        else
        {
            init = m_arena.create<ast::EmptyStmt>(single_range());
            advance();
        }

        ast::Expr* cond = nullptr;
        if (!check(TK::Semicolon))
            cond = parse_expr();
        expect(TK::Semicolon, "after for condition");

        ast::Expr* incr = nullptr;
        if (!check(TK::RParen))
            incr = parse_expr();
        expect(TK::RParen, "after for clauses");

        auto* body = parse_block();
        return m_arena.create<ast::ForStmt>(range_from(begin), init, cond, incr, body);
    }

    ast::DoWhileStmt* Parser::parse_do_while_stmt()
    {
        auto begin = loc();
        expect(TK::KwDo, "internal error");
        auto* body = parse_block();
        expect(TK::KwWhile, "after do block");

        bool has_paren = match(TK::LParen);
        auto* cond = parse_expr();
        if (has_paren)
            expect(TK::RParen, "after do-while condition");

        expect(TK::Semicolon, "after do-while");
        return m_arena.create<ast::DoWhileStmt>(range_from(begin), body, cond);
    }

    ast::ReturnStmt* Parser::parse_return_stmt()
    {
        auto begin = loc();
        expect(TK::KwReturn, "internal error");
        ast::Expr* val = nullptr;
        if (!check(TK::Semicolon))
            val = parse_expr();

        expect(TK::Semicolon, "after return");
        return m_arena.create<ast::ReturnStmt>(range_from(begin), val);
    }

    ast::BreakStmt* Parser::parse_break_stmt()
    {
        auto begin = loc();
        expect(TK::KwBreak, "internal error");
        expect(TK::Semicolon, "after break");
        return m_arena.create<ast::BreakStmt>(range_from(begin));
    }

    ast::ContinueStmt* Parser::parse_continue_stmt()
    {
        auto begin = loc();
        expect(TK::KwContinue, "internal error");
        expect(TK::Semicolon, "after continue");
        return m_arena.create<ast::ContinueStmt>(range_from(begin));
    }

    ast::DeferStmt* Parser::parse_defer_stmt()
    {
        auto begin = loc();
        expect(TK::KwDefer, "internal error");
        auto* body = parse_stmt();
        return m_arena.create<ast::DeferStmt>(range_from(begin), body);
    }

    ast::MatchStmt* Parser::parse_match_stmt()
    {
        auto begin = loc();
        expect(TK::KwMatch, "internal error");
        auto* scrutinee = parse_expr();
        expect(TK::LBrace, "to open match body");

        std::vector<ast::MatchArm> arms;
        while (!check(TK::RBrace) && !at_end())
        {
            arms.push_back(parse_match_arm());
            match(TK::Comma);
        }

        expect(TK::RBrace, "to close match body");
        return m_arena.create<ast::MatchStmt>(range_from(begin), scrutinee, m_arena.to_const_span(arms));
    }

    ast::Stmt* Parser::parse_decl_or_expr_stmt()
    {
        if (check(TK::KwStatic) || check(TK::KwExtern))
        {
            auto begin = loc();
            ast::StorageClass sc = ast::StorageClass::None;
            if (match(TK::KwStatic))
                sc = ast::StorageClass::Static;
            else if (match(TK::KwExtern))
                sc = ast::StorageClass::Extern;

            auto* type = parse_type();
            auto name = expect(TK::Identifier, "for variable name").interned;
            auto* var = parse_var_decl_rest(type, name, ast::Qualifier::None, begin, sc);

            return m_arena.create<ast::DeclStmt>(var->range(), var);
        }

        if (is_type_keyword(peek().kind) || check(TK::KwConst) || check(TK::KwVolatile) || check(TK::KwRestrict))
        {
            auto begin = loc();

            ast::Qualifier var_quals = ast::Qualifier::None;
            if (check(TK::KwConst))
                var_quals = var_quals | ast::Qualifier::Const;

            auto* type = parse_type();
            auto name = expect(TK::Identifier, "for variable name").interned;
            auto* var = parse_var_decl_rest(type, name, var_quals, begin, ast::StorageClass::None);

            return m_arena.create<ast::DeclStmt>(var->range(), var);
        }

        if (looks_like_local_decl())
        {
            if (is_ambiguous_decl_or_expr())
                return parse_ambiguous_decl_or_expr();

            auto begin = loc();
            auto* type = parse_type();
            auto name = expect(TK::Identifier, "for variable name").interned;
            auto* var = parse_var_decl_rest(type, name, ast::Qualifier::None, begin, ast::StorageClass::None);

            return m_arena.create<ast::DeclStmt>(var->range(), var);
        }

        auto begin = loc();
        auto* expr = parse_expr();

        expect(TK::Semicolon, "after expression statement");
        return m_arena.create<ast::ExprStmt>(range_from(begin), expr);
    }

    ast::Stmt* Parser::parse_ambiguous_decl_or_expr() noexcept
    {
        auto begin = loc();
        auto saved_pos = save();
        bool saved_panic = m_panic;
        bool saved_error = m_had_error;

        m_speculative = true;
        m_panic = false;
        m_had_error = false;

        auto* type = parse_type();
        auto dname = expect(TK::Identifier, "for variable name").interned;
        auto* var = parse_var_decl_rest(type, dname, ast::Qualifier::None, begin, ast::StorageClass::None);
        auto* decl_stmt = m_arena.create<ast::DeclStmt>(var->range(), var);

        bool decl_failed = m_had_error;
        auto decl_end = save();

        m_panic = false;
        m_had_error = false;
        restore(saved_pos);

        auto* expr = parse_expr();
        expect(TK::Semicolon, "after expression statement");
        auto* expr_stmt = m_arena.create<ast::ExprStmt>(range_from(begin), expr);

        bool expr_failed = m_had_error;
        auto expr_end = save();

        m_speculative = false;
        m_panic = saved_panic;
        m_had_error = saved_error;

        if (decl_failed && expr_failed)
        {
            restore(saved_pos);
            auto* e = parse_expr();
            expect(TK::Semicolon, "after expression statement");
            return m_arena.create<ast::ExprStmt>(range_from(begin), e);
        }

        if (decl_failed)
        {
            restore(expr_end);
            return expr_stmt;
        }

        if (expr_failed)
        {
            restore(decl_end);
            return decl_stmt;
        }

        restore(decl_end);
        std::vector<ast::Stmt*> alts{decl_stmt, expr_stmt};
        return m_arena.create<ast::AmbiguousStmt>(range_from(begin), std::move(alts));
    }

    ast::Expr* Parser::parse_expr()
    {
        return parse_assignment();
    }

    ast::Expr* Parser::parse_assignment()
    {
        auto* lhs = parse_ternary();

        if (lex::is_assignment(peek().kind))
        {
            auto begin = lhs->begin_loc();
            auto op = to_assign_op(advance().kind);
            auto* rhs = parse_assignment();

            return m_arena.create<ast::AssignExpr>(range_from(begin), op, lhs, rhs);
        }
        return lhs;
    }

    ast::Expr* Parser::parse_ternary()
    {
        auto* cond = parse_binary(1);

        if (match(TK::Question))
        {
            auto begin = cond->begin_loc();
            auto* then_expr = parse_expr();
            expect(TK::Colon, "in ternary expression");
            auto* else_expr = parse_ternary();
            return m_arena.create<ast::ConditionalExpr>(range_from(begin), cond, then_expr, else_expr);
        }

        return cond;
    }

    ast::Expr* Parser::parse_binary(int min_prec)
    {
        auto* lhs = parse_unary();

        while (true)
        {
            if (check(TK::KwAs))
            {
                auto begin = lhs->begin_loc();
                advance();
                auto* target = parse_type();
                lhs = m_arena.create<ast::CastExpr>(range_from(begin), lhs, target);
                continue;
            }

            int prec = binary_precedence(peek().kind);
            if (prec < min_prec)
                break;

            auto begin = lhs->begin_loc();
            auto op = to_binary_op(advance().kind);
            auto* rhs = parse_binary(prec + 1);

            lhs = m_arena.create<ast::BinaryExpr>(range_from(begin), op, lhs, rhs);
        }

        return lhs;
    }

    ast::Expr* Parser::parse_unary()
    {
        if (check(TK::Increment))
        {
            auto begin = loc();
            advance();
            auto* operand = parse_unary();
            return m_arena.create<ast::UnaryExpr>(range_from(begin), ast::UnaryOp::PreInc, operand);
        }

        if (check(TK::Decrement))
        {
            auto begin = loc();
            advance();
            auto* operand = parse_unary();
            return m_arena.create<ast::UnaryExpr>(range_from(begin), ast::UnaryOp::PreDec, operand);
        }

        if (peek().is_one_of(TK::Minus, TK::Tilde, TK::Bang, TK::Star, TK::Amp))
        {
            auto begin = loc();

            auto op = to_prefix_op(advance().kind);
            auto* operand = parse_unary();

            return m_arena.create<ast::UnaryExpr>(range_from(begin), op, operand);
        }

        auto* expr = parse_primary();
        return parse_postfix(expr);
    }

    ast::Expr* Parser::parse_postfix(ast::Expr* lhs)
    {
        while (true)
        {
            auto begin = lhs->begin_loc();

            if (check(TK::Increment))
            {
                advance();
                lhs = m_arena.create<ast::UnaryExpr>(range_from(begin), ast::UnaryOp::PostInc, lhs);
                continue;
            }

            if (check(TK::Decrement))
            {
                advance();
                lhs = m_arena.create<ast::UnaryExpr>(range_from(begin), ast::UnaryOp::PostDec, lhs);
                continue;
            }

            if (match(TK::Dot))
            {
                auto member = expect(TK::Identifier, "after '.'").interned;
                lhs = m_arena.create<ast::MemberAccessExpr>(range_from(begin), lhs, member);

                if (check(TK::Bang) && check_ahead(1, TK::LParen))
                {
                    auto tpl_args = parse_template_args();
                    lhs = parse_call_args(lhs, std::move(tpl_args), begin);
                }
                else if (check(TK::LParen))
                    lhs = parse_call_args(lhs, {}, begin);

                continue;
            }

            if (check(TK::Bang))
            {
                auto tpl_args = parse_template_args();
                lhs = parse_call_args(lhs, std::move(tpl_args), begin);
                continue;
            }

            if (check(TK::LParen))
            {
                lhs = parse_call_args(lhs, {}, begin);
                continue;
            }

            if (match(TK::LBracket))
            {
                auto* first = parse_expr();
                if (match(TK::DotDot))
                {
                    auto* end_expr = parse_expr();
                    expect(TK::RBracket, "after slice expression");
                    lhs = m_arena.create<ast::SliceExpr>(range_from(begin), lhs, first, end_expr);
                }
                else
                {
                    expect(TK::RBracket, "after index expression");
                    lhs = m_arena.create<ast::IndexExpr>(range_from(begin), lhs, first);
                }
                continue;
            }

            break;
        }
        return lhs;
    }

    ast::Expr* Parser::parse_call_args(ast::Expr* callee, std::vector<ast::TemplateArg> tpl_args, sm::Location begin)
    {
        expect(TK::LParen, "for function call");
        std::vector<ast::Expr*> args;
        if (!check(TK::RParen))
            do
                args.push_back(parse_expr());
            while (match(TK::Comma));

        expect(TK::RParen, "after arguments");

        auto arg_span = m_arena.to_span(args);
        auto tpl_span = m_arena.to_const_span(tpl_args);

        if (auto* id = dynamic_cast<ast::IdentifierExpr*>(callee); id && tpl_args.empty())
        {
            auto range = range_from(begin);
            auto* call = m_arena.create<ast::CallExpr>(range, callee, arg_span, tpl_span);

            if (args.size() == 1)
            {
                auto* type = m_arena.create<ast::NamedType>(id->range(), id->name());
                std::vector<ast::FieldInit> empty_fields;
                auto* cast = m_arena.create<ast::CastExpr>(range, args[0], type);

                std::vector<ast::Expr*> alts{call, cast};
                return m_arena.create<ast::AmbiguousExpr>(range, std::move(alts));
            }

            return call;
        }

        return m_arena.create<ast::CallExpr>(range_from(begin), callee, arg_span, tpl_span);
    }

    std::vector<ast::TemplateArg> Parser::parse_template_args()
    {
        if (!check(TK::Bang))
            return {};

        advance();
        bool parenthesis = match(TK::LParen);

        std::vector<ast::TemplateArg> args;
        if (!check(TK::RParen))
            do
            {
                auto tbegin = loc();
                auto* type = parse_type();
                args.push_back(ast::TemplateArg{type, range_from(tbegin)});
            } while (match(TK::Comma));

        if (parenthesis)
            expect(TK::RParen, "after template arguments");

        return args;
    }

    std::vector<si::InternedString> Parser::parse_dotted_path()
    {
        std::vector<si::InternedString> path;
        path.push_back(expect(TK::Identifier, "in dotted path").interned);
        while (match(TK::Dot))
            path.push_back(expect(TK::Identifier, "in dotted path").interned);

        return path;
    }

    ast::Expr* Parser::parse_primary()
    {
        auto begin = loc();

        if (check(TK::IntLiteral))
        {
            auto& tok = advance();
            return m_arena.create<ast::IntegerLiteral>(range_from(begin), tok.value ? *tok.value : lex::TokenValue{});
        }

        if (check(TK::FloatLiteral))
        {
            auto& tok = advance();
            return m_arena.create<ast::FloatLiteral>(range_from(begin), tok.value ? *tok.value : lex::TokenValue{});
        }

        if (check(TK::StringLiteral))
        {
            auto& tok = advance();
            return m_arena.create<ast::StringLiteral>(range_from(begin), tok.interned);
        }

        if (check(TK::CharLiteral))
        {
            auto& tok = advance();
            return m_arena.create<ast::CharLiteral>(range_from(begin), tok.value ? *tok.value : lex::TokenValue{});
        }

        if (match(TK::KwTrue))
            return m_arena.create<ast::BoolLiteral>(range_from(begin), true);
        if (match(TK::KwFalse))
            return m_arena.create<ast::BoolLiteral>(range_from(begin), false);

        if (match(TK::KwNull))
            return m_arena.create<ast::NullLiteral>(range_from(begin));

        if (match(TK::KwSizeof))
        {
            expect(TK::LParen, "after sizeof");
            auto* operand = parse_type();
            expect(TK::RParen, "after sizeof operand");
            return m_arena.create<ast::SizeofExpr>(range_from(begin), operand);
        }

        if (match(TK::KwAlignof))
        {
            expect(TK::LParen, "after alignof");
            auto* operand = parse_type();
            expect(TK::RParen, "after alignof operand");
            return m_arena.create<ast::AlignofExpr>(range_from(begin), operand);
        }

        if (match(TK::LParen))
        {
            auto* inner = parse_expr();
            expect(TK::RParen, "after grouping expression");
            return m_arena.create<ast::GroupingExpr>(range_from(begin), inner);
        }

        if (check(TK::LBrace))
        {
            if (check_ahead(1, TK::Identifier))
            {
                auto saved = save();
                advance();
                advance();
                bool is_field_init = check(TK::Colon);
                restore(saved);

                if (is_field_init)
                {
                    advance();
                    auto fields = parse_field_init_list();
                    expect(TK::RBrace, "after initializer");
                    return m_arena.create<ast::InitializerExpr>(range_from(begin), nullptr, m_arena.to_const_span(fields));
                }
            }

            auto* block = parse_block();
            return m_arena.create<ast::BlockExpr>(range_from(begin), block->stmts(), nullptr);
        }

        if (check(TK::Identifier))
        {
            auto& tok = advance();
            return m_arena.create<ast::IdentifierExpr>(range_from(begin), tok.interned);
        }

        error_at_current(std::format("unexpected token '{}'", lex::to_string(peek().kind)));
        advance();
        return m_arena.create<ast::NullLiteral>(range_from(begin));
    }

    std::vector<ast::FieldInit> Parser::parse_field_init_list()
    {
        std::vector<ast::FieldInit> fields;
        do
        {
            if (check(TK::RBrace))
                break;

            auto fbegin = loc();
            auto name = expect(TK::Identifier, "for field name").interned;
            expect(TK::Colon, "after field name in initializer");
            auto* value = parse_expr();
            fields.push_back(ast::FieldInit{name, value, range_from(fbegin)});
        } while (match(TK::Comma));

        return fields;
    }

    ast::TypeExpr* Parser::parse_type()
    {
        auto begin = loc();

        ast::Qualifier quals = ast::Qualifier::None;
        while (true)
        {
            if (match(TK::KwConst))
                quals = quals | ast::Qualifier::Const;
            else if (match(TK::KwVolatile))
                quals = quals | ast::Qualifier::Volatile;
            else if (match(TK::KwRestrict))
                quals = quals | ast::Qualifier::Restrict;
            else
                break;
        }

        auto* base = parse_base_type();
        if (!base)
            return nullptr;

        if (quals != ast::Qualifier::None)
            base = m_arena.create<ast::QualifiedType>(range_from(begin), quals, base);

        base = parse_type_suffix(base);

        return base;
    }

    ast::TypeExpr* Parser::parse_base_type()
    {
        auto begin = loc();

        if (check(TK::LBracket) && check_ahead(1, TK::RBracket))
        {
            advance();
            advance();
            auto* element = parse_type();
            return m_arena.create<ast::SliceType>(range_from(begin), element);
        }

        if (is_type_keyword(peek().kind))
        {
            auto kind = to_primitive(advance().kind);
            return m_arena.create<ast::BuiltinType>(range_from(begin), kind);
        }

        if (check(TK::Identifier))
        {
            auto name = advance().interned;

            if (check(TK::Dot))
            {
                auto saved = save();
                bool saved_speculative = m_speculative;
                bool saved_panic = m_panic;
                bool saved_error = m_had_error;
                m_speculative = true;
                m_panic = false;
                m_had_error = false;

                std::vector<si::InternedString> segments;
                segments.push_back(name);

                bool valid_dotted_type = true;
                while (check(TK::Dot))
                {
                    advance();
                    if (!check(TK::Identifier))
                    {
                        valid_dotted_type = false;
                        break;
                    }
                    segments.push_back(advance().interned);
                }

                bool looks_like_type = false;
                if (valid_dotted_type && segments.size() > 1)
                {
                    auto next = peek().kind;
                    looks_like_type = next == TK::Star || next == TK::LBracket || next == TK::Identifier || next == TK::LBrace || next == TK::RParen ||
                                      next == TK::Comma || next == TK::Semicolon || next == TK::Gt;
                }

                m_speculative = saved_speculative;
                m_panic = saved_panic;
                m_had_error = saved_error;

                if (looks_like_type)
                {
                    restore(saved);
                    std::vector<si::InternedString> real_segments;
                    real_segments.push_back(name);

                    real_segments.clear();
                    real_segments.push_back(name);
                    while (match(TK::Dot))
                        if (check(TK::Identifier))
                            real_segments.push_back(advance().interned);
                        else
                            break;

                    return m_arena.create<ast::DottedNamedType>(range_from(begin), m_arena.to_const_span(real_segments));
                }
                else
                    restore(saved);
            }

            auto* named = m_arena.create<ast::NamedType>(range_from(begin), name);

            if (check(TK::LParen))
            {
                auto saved = save();
                bool was_speculative = m_speculative;
                bool was_panic = m_panic;
                bool was_error = m_had_error;
                m_speculative = true;
                m_panic = false;

                advance();

                std::vector<ast::TemplateArg> targs;
                bool valid = true;

                if (!check(TK::RParen))
                    do
                    {
                        auto tbegin = loc();
                        auto* t = parse_type();
                        if (!t)
                        {
                            valid = false;
                            break;
                        }

                        targs.push_back(ast::TemplateArg{t, range_from(tbegin)});
                    } while (match(TK::Comma));

                if (valid && check(TK::RParen))
                {
                    advance();
                    m_speculative = was_speculative;
                    m_panic = was_panic;
                    m_had_error = was_error;
                    return m_arena.create<ast::TemplateType>(range_from(begin), named, m_arena.to_const_span(targs));
                }

                m_speculative = was_speculative;
                m_panic = was_panic;
                m_had_error = was_error;
                restore(saved);
            }

            return named;
        }

        error_at_current("expected type");
        return nullptr;
    }

    ast::TypeExpr* Parser::parse_type_suffix(ast::TypeExpr* base)
    {
        while (true)
        {
            auto begin = base->begin_loc();

            if (match(TK::Star))
            {
                base = m_arena.create<ast::PointerType>(range_from(begin), base);

                ast::Qualifier ptr_quals = ast::Qualifier::None;
                while (true)
                {
                    if (match(TK::KwConst))
                        ptr_quals = ptr_quals | ast::Qualifier::Const;
                    else if (match(TK::KwVolatile))
                        ptr_quals = ptr_quals | ast::Qualifier::Volatile;
                    else if (match(TK::KwRestrict))
                        ptr_quals = ptr_quals | ast::Qualifier::Restrict;
                    else
                        break;
                }

                if (ptr_quals != ast::Qualifier::None)
                    base = m_arena.create<ast::QualifiedType>(range_from(begin), ptr_quals, base);

                continue;
            }

            if (check(TK::LBracket))
            {
                advance();
                if (match(TK::RBracket))
                    base = m_arena.create<ast::FlexibleArrayType>(range_from(begin), base);
                else
                {
                    auto* size = parse_expr();
                    expect(TK::RBracket, "after array size");
                    base = m_arena.create<ast::ArrayType>(range_from(begin), base, size);
                }
                continue;
            }

            break;
        }
        return base;
    }

    ast::Pattern* Parser::parse_pattern()
    {
        auto begin = loc();

        if (check(TK::Identifier) && peek().text == "_")
        {
            advance();
            return m_arena.create<ast::WildcardPattern>(range_from(begin));
        }

        if (match(TK::DotDot))
            return m_arena.create<ast::RestPattern>(range_from(begin));

        if (check(TK::IntLiteral) || check(TK::FloatLiteral) || check(TK::StringLiteral) || check(TK::CharLiteral) || check(TK::KwTrue) || check(TK::KwFalse) ||
            check(TK::KwNull))
        {
            auto* lit = parse_primary();
            return m_arena.create<ast::LiteralPattern>(range_from(begin), lit);
        }

        if (check(TK::Minus) && (check_ahead(1, TK::IntLiteral) || check_ahead(1, TK::FloatLiteral)))
        {
            auto* lit = parse_unary();
            return m_arena.create<ast::LiteralPattern>(range_from(begin), lit);
        }

        if (check(TK::Identifier))
        {
            std::vector<si::InternedString> path;
            path.push_back(advance().interned);

            while (match(TK::ColonColon))
                path.push_back(expect(TK::Identifier, "in pattern path").interned);

            if (match(TK::LParen))
            {
                std::vector<ast::Pattern*> sub;
                if (!check(TK::RParen))
                    do
                        sub.push_back(parse_pattern());
                    while (match(TK::Comma));

                expect(TK::RParen, "after enum pattern");
                return m_arena.create<ast::EnumPattern>(range_from(begin), m_arena.to_const_span(path), m_arena.to_span(sub));
            }

            if (match(TK::LBrace))
            {
                std::vector<ast::StructPatternField> fields;
                bool has_rest = false;
                while (!check(TK::RBrace) && !at_end())
                {
                    if (match(TK::DotDot))
                    {
                        has_rest = true;
                        break;
                    }

                    auto fbegin = loc();
                    auto fname = expect(TK::Identifier, "in struct pattern field").interned;
                    expect(TK::Colon, "in struct pattern field");

                    auto* pat = parse_pattern();

                    fields.push_back(ast::StructPatternField{fname, pat, range_from(fbegin)});

                    if (!match(TK::Comma))
                        break;
                }

                expect(TK::RBrace, "after struct pattern");
                return m_arena.create<ast::StructPattern>(range_from(begin), path.front(), m_arena.to_const_span(fields), has_rest);
            }

            if (path.size() == 1)
                return m_arena.create<ast::BindingPattern>(range_from(begin), path.front());

            std::vector<ast::Pattern*> empty_sub;
            return m_arena.create<ast::EnumPattern>(range_from(begin), m_arena.to_const_span(path), m_arena.to_span(empty_sub));
        }

        error_at_current("expected pattern");
        advance();
        return m_arena.create<ast::WildcardPattern>(range_from(begin));
    }

    ast::MatchArm Parser::parse_match_arm()
    {
        auto begin = loc();
        auto* pattern = parse_pattern();

        ast::Expr* guard = nullptr;
        if (match(TK::KwIf))
            guard = parse_expr();

        expect(TK::FatArrow, "in match arm");

        ast::Node* body = nullptr;
        if (check(TK::LBrace))
            body = parse_block();
        else
            body = parse_expr();

        return ast::MatchArm{pattern, guard, body, range_from(begin)};
    }

} // namespace dcc::parse
