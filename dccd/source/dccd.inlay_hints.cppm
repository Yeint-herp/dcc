export module dccd.inlay_hints;

import std;
import dcc.sm;
import dcc.ast;
import dcc.types;
import dcc.sema;
import dcc.sema.type_helpers;
import dccd.protocol;
import dcc.ast.visitor;

export namespace dccd::inlay_hints
{
    using TypeFormatter = std::function<std::string(dcc::types::Type const*)>;

    [[nodiscard]] std::vector<protocol::InlayHint> collect_inlay_hints(dcc::sm::SourceManager const& sm, dcc::ast::TranslationUnit const* tu,
                                                                       dcc::sm::SourceRange request_range, TypeFormatter format_type);

} // namespace dccd::inlay_hints

module :private;

namespace dccd::inlay_hints
{
    namespace
    {
        [[nodiscard]] bool range_overlaps(dcc::sm::SourceRange const& a, dcc::sm::SourceRange const& b) noexcept
        {
            if (!a.valid() || !b.valid())
                return false;

            if (a.begin.fileId != b.begin.fileId)
                return false;

            return a.begin.offset < b.end.offset && b.begin.offset < a.end.offset;
        }

        [[nodiscard]] bool range_before(dcc::sm::SourceRange const& a, dcc::sm::SourceRange const& b) noexcept
        {
            if (!a.valid() || !b.valid())
                return false;

            if (a.begin.fileId != b.begin.fileId)
                return false;

            return a.end.offset <= b.begin.offset;
        }

        [[nodiscard]] bool range_after(dcc::sm::SourceRange const& a, dcc::sm::SourceRange const& b) noexcept
        {
            if (!a.valid() || !b.valid())
                return false;

            if (a.begin.fileId != b.begin.fileId)
                return false;

            return a.begin.offset >= b.end.offset;
        }

        [[nodiscard]] std::optional<protocol::LspPosition> src_to_lsp(dcc::sm::SourceManager const& sm, dcc::sm::Location loc)
        {
            auto pos = sm.location_to_lsp_position(loc);
            if (!pos)
                return std::nullopt;

            protocol::LspPosition lsp;
            lsp.line = pos->line;
            lsp.character = pos->character;
            return lsp;
        }

        struct Collector : dcc::ast::RecursiveAstVisitor
        {
            dcc::sm::SourceManager const& sm;
            dcc::sm::SourceRange request_range;
            TypeFormatter format_type;
            std::vector<protocol::InlayHint> hints;

            Collector(dcc::sm::SourceManager const& s, dcc::sm::SourceRange req_range, TypeFormatter fmt)
                : sm{s}, request_range{req_range}, format_type{std::move(fmt)}
            {
            }

            void emit_type_hint(dcc::sm::Location pos, std::string label)
            {
                auto lsp_pos = src_to_lsp(sm, pos);
                if (!lsp_pos)
                    return;

                protocol::InlayHint h;
                h.position = *lsp_pos;
                h.label = std::move(label);
                h.kind = protocol::InlayHintKind::Type;
                h.paddingLeft = false;
                hints.push_back(std::move(h));
            }

            void emit_param_hint(dcc::sm::Location pos, std::string_view param_name)
            {
                auto lsp_pos = src_to_lsp(sm, pos);
                if (!lsp_pos)
                    return;

                protocol::InlayHint h;
                h.position = *lsp_pos;
                h.label = std::format("{}:", param_name);
                h.kind = protocol::InlayHintKind::Parameter;
                h.paddingRight = true;
                hints.push_back(std::move(h));
            }

            void visitDecl(dcc::ast::Decl const* decl) override;
            void visitStmt(dcc::ast::Stmt const* stmt) override;
            void visitExpr(dcc::ast::Expr const* expr) override;
            void visitBlock(dcc::ast::Block const& block) override;
            void visitMatchArm(dcc::ast::MatchArm const& arm) override;
        };

        void Collector::visitBlock(dcc::ast::Block const& block)
        {
            if (!range_overlaps(block.range, request_range))
                return;

            dcc::ast::RecursiveAstVisitor::visitBlock(block);
        }

        void Collector::visitMatchArm(dcc::ast::MatchArm const& arm)
        {
            if (arm.body)
                visitExpr(arm.body);
        }

        void Collector::visitExpr(dcc::ast::Expr const* expr)
        {
            if (!expr)
                return;

            if (range_before(expr->range, request_range) || range_after(expr->range, request_range))
                return;

            switch (expr->kind)
            {
                case dcc::ast::ExprKind::Call: {
                    auto* e = static_cast<dcc::ast::CallExpr const*>(expr);

                    dcc::ast::FuncDecl const* target = nullptr;

                    if (e->sema.ufcs_callee && e->sema.ufcs_callee->kind == dcc::ast::DeclKind::Func)
                        target = static_cast<dcc::ast::FuncDecl const*>(e->sema.ufcs_callee);
                    else if (e->sema.resolved_specialization)
                        target = e->sema.resolved_specialization;
                    else if (e->sema.resolved_decl && e->sema.resolved_decl->kind == dcc::ast::DeclKind::Func)
                        target = static_cast<dcc::ast::FuncDecl const*>(e->sema.resolved_decl);

                    if (!target && e->callee)
                    {
                        if (e->callee->sema.resolved_specialization)
                            target = e->callee->sema.resolved_specialization;
                        else if (e->callee->sema.resolved_decl && e->callee->sema.resolved_decl->kind == dcc::ast::DeclKind::Func)
                            target = static_cast<dcc::ast::FuncDecl const*>(e->callee->sema.resolved_decl);
                        else if (e->callee->sema.ufcs_callee && e->callee->sema.ufcs_callee->kind == dcc::ast::DeclKind::Func)
                            target = static_cast<dcc::ast::FuncDecl const*>(e->callee->sema.ufcs_callee);
                    }

                    bool is_ufcs = e->sema.ufcs_callee != nullptr;

                    if (target)
                    {
                        std::size_t param_offset = is_ufcs ? 1u : 0u;
                        for (std::size_t i = 0; i < e->args.size(); ++i)
                        {
                            std::size_t param_idx = i + param_offset;
                            if (param_idx >= target->params.size())
                                break;

                            auto const& fp = target->params[param_idx];
                            if (fp.name.empty())
                                continue;

                            if (e->args[i] && range_overlaps(e->args[i]->range, request_range))
                                emit_param_hint(e->args[i]->range.begin, fp.name);
                        }
                    }

                    if (e->callee)
                        visitExpr(e->callee);

                    for (auto* a : e->args)
                        if (a)
                            visitExpr(a);
                    break;
                }
                case dcc::ast::ExprKind::Match: {
                    auto* e2 = static_cast<dcc::ast::MatchExpr const*>(expr);
                    if (e2->operand)
                        visitExpr(e2->operand);
                    for (auto const& arm : e2->arms)
                        visitMatchArm(arm);
                    break;
                }
                case dcc::ast::ExprKind::StructLiteral: {
                    auto* e2 = static_cast<dcc::ast::StructLiteralExpr const*>(expr);
                    for (auto const& f : e2->fields)
                        if (f.value)
                            visitExpr(f.value);
                    break;
                }
                default:
                    dcc::ast::RecursiveAstVisitor::visitExpr(expr);
                    break;
            }
        }

        void Collector::visitStmt(dcc::ast::Stmt const* stmt)
        {
            if (!stmt)
                return;

            if (range_before(stmt->range, request_range) || range_after(stmt->range, request_range))
                return;

            switch (stmt->kind)
            {
                case dcc::ast::StmtKind::StaticMatch: {
                    auto* s = static_cast<dcc::ast::StaticMatchStmt const*>(stmt);
                    if (s->operand)
                        visitExpr(s->operand);
                    for (auto const& arm : s->arms)
                        visitMatchArm(arm);
                    break;
                }
                case dcc::ast::StmtKind::ForIn: {
                    auto* s = static_cast<dcc::ast::ForInStmt const*>(stmt);
                    if (s->iterable)
                        visitExpr(s->iterable);
                    visitBlock(s->body);
                    break;
                }
                default:
                    dcc::ast::RecursiveAstVisitor::visitStmt(stmt);
                    break;
            }
        }

        void Collector::visitDecl(dcc::ast::Decl const* decl)
        {
            if (!decl)
                return;

            if (range_before(decl->range, request_range) || range_after(decl->range, request_range))
                return;

            switch (decl->kind)
            {
                case dcc::ast::DeclKind::Var: {
                    auto* d = static_cast<dcc::ast::VarDecl const*>(decl);

                    if (d->type == nullptr)
                    {
                        dcc::types::Type const* inferred_type = nullptr;
                        if (d->init)
                            inferred_type = dcc::sema::get_resolved_type(d->init->sema);

                        if (inferred_type && d->name_range.valid() && range_overlaps(d->name_range, request_range))
                        {
                            auto type_str = format_type(inferred_type);
                            if (!type_str.empty())
                                emit_type_hint(d->name_range.end, std::format(": {}", type_str));
                        }
                    }

                    if (d->init)
                        visitExpr(d->init);
                    break;
                }
                case dcc::ast::DeclKind::Func: {
                    auto* d = static_cast<dcc::ast::FuncDecl const*>(decl);
                    if (d->body.has_value())
                        visitBlock(*d->body);
                    if (d->constraint)
                        visitExpr(d->constraint);
                    break;
                }
                case dcc::ast::DeclKind::Using: {
                    auto* d = static_cast<dcc::ast::UsingDecl const*>(decl);
                    if (d->target_expr)
                        visitExpr(d->target_expr);
                    break;
                }
                default:
                    break;
            }
        }

    } // anonymous namespace

    std::vector<protocol::InlayHint> collect_inlay_hints(dcc::sm::SourceManager const& sm, dcc::ast::TranslationUnit const* tu,
                                                         dcc::sm::SourceRange request_range, TypeFormatter format_type)
    {
        std::vector<protocol::InlayHint> result;
        if (!tu)
            return result;

        Collector c{sm, request_range, std::move(format_type)};

        if (tu->module_decl)
            c.visitDecl(tu->module_decl);

        for (auto* d : tu->imports)
            if (d)
                c.visitDecl(d);

        for (auto* d : tu->decls)
            if (d)
                c.visitDecl(d);

        result = std::move(c.hints);
        return result;
    }

} // namespace dccd::inlay_hints
