#include <ast/ambiguous.hh>
#include <ast/decl.hh>
#include <ast/expr.hh>
#include <ast/pattern.hh>
#include <ast/stmt.hh>
#include <ast/type.hh>
#include <format>
#include <sema/name_resolver.hh>

namespace dcc::sema
{
    NameResolver::NameResolver(TypeContext& types, ModuleLoader& modules, diag::DiagnosticPrinter& printer)
        : m_types{types}, m_modules{modules}, m_printer{printer}
    {
    }

    bool NameResolver::resolve(ast::TranslationUnit& tu)
    {
        m_global_scope = std::make_unique<Scope>(ScopeKind::Global);
        m_current_scope = m_global_scope.get();

        tu.accept(*this);

        return m_error_count == 0;
    }

    std::unique_ptr<Scope> NameResolver::take_global_scope() noexcept
    {
        return std::move(m_global_scope);
    }

    Scope* NameResolver::push_scope(ScopeKind kind, ast::Node* owner)
    {
        auto* child = m_current_scope->add_child(kind, owner);
        m_scope_stack_prev = m_current_scope;
        m_current_scope = child;

        return child;
    }

    void NameResolver::pop_scope()
    {
        m_current_scope = m_current_scope->parent();
    }

    Scope* NameResolver::get_or_create_namespace_chain(std::span<const si::InternedString> segments)
    {
        Scope* parent_scope = m_current_scope;

        for (std::size_t i = 0; i < segments.size(); ++i)
        {
            auto seg = segments[i];
            auto* existing = parent_scope->lookup_local(seg);

            if (existing && existing->is_namespace())
            {
                parent_scope = existing->namespace_scope();
                continue;
            }

            if (existing)
            {
                error(sm::SourceRange{}, std::format("'{}' already declared as a non-namespace symbol", std::string{seg.view()}));
                return nullptr;
            }

            auto* ns_scope = parent_scope->add_child(ScopeKind::Namespace);
            auto* ns_sym = parent_scope->declare(SymbolKind::Namespace, seg, nullptr, ast::Visibility::Public);
            if (ns_sym)
                ns_sym->set_namespace_scope(ns_scope);

            parent_scope = ns_scope;
        }

        return parent_scope;
    }

    Symbol* NameResolver::declare(SymbolKind kind, si::InternedString name, ast::Decl* decl, ast::Visibility vis, sm::SourceRange range)
    {
        auto* sym = m_current_scope->declare(kind, name, decl, vis);
        if (!sym)
        {
            auto* prev = m_current_scope->lookup_local(name);
            error_redeclared(name, range, prev ? prev->decl()->range() : sm::SourceRange{});
            return nullptr;
        }

        return sym;
    }

    Symbol* NameResolver::resolve_name(si::InternedString name, sm::SourceRange range)
    {
        auto* sym = m_current_scope->lookup(name);
        if (!sym)
        {
            error_undeclared(name, range);
            return nullptr;
        }

        return sym;
    }

    Symbol* NameResolver::resolve_qualified_path(std::span<const si::InternedString> path, sm::SourceRange range)
    {
        auto* sym = m_current_scope->lookup_qualified(path);
        if (!sym)
        {
            std::string path_str;
            for (std::size_t i = 0; i < path.size(); ++i)
            {
                if (i > 0)
                    path_str += "::";
                path_str += path[i].view();
            }

            error(range, std::format("undeclared qualified name '{}'", path_str));
            return nullptr;
        }

        return sym;
    }

    Symbol* Scope::lookup_dotted(std::span<const si::InternedString> segments) const noexcept
    {
        if (segments.empty())
            return nullptr;

        auto* sym = lookup(segments[0]);
        if (!sym)
            return nullptr;

        for (std::size_t i = 1; i < segments.size(); ++i)
        {
            if (!sym->is_namespace())
                return nullptr;

            auto* ns_scope = sym->namespace_scope();
            if (!ns_scope)
                return nullptr;

            sym = ns_scope->lookup_local(segments[i]);
            if (!sym)
                return nullptr;
        }

        return sym;
    }

    SemaType* NameResolver::resolve_type(const ast::TypeExpr& type_expr)
    {
        type_expr.accept(*this);
        auto it = m_type_resolutions.find(&type_expr);
        if (it != m_type_resolutions.end())
            return it->second;

        return m_types.error_type();
    }

    bool NameResolver::is_type_name(si::InternedString name) const noexcept
    {
        auto* sym = m_current_scope->lookup(name);
        return sym && sym->is_type();
    }

    void NameResolver::forward_declare_types(std::span<ast::Decl* const> decls)
    {
        for (auto* decl : decls)
        {
            if (auto* sd = dynamic_cast<ast::StructDecl*>(decl))
            {
                auto* sty = m_types.make_struct(sd->name());
                auto* sym = declare(SymbolKind::Type, sd->name(), sd, sd->visibility(), sd->range());
                if (sym)
                    sym->set_type(sty);
            }
            else if (auto* ud = dynamic_cast<ast::UnionDecl*>(decl))
            {
                auto* uty = m_types.make_union(ud->name());
                auto* sym = declare(SymbolKind::Type, ud->name(), ud, ud->visibility(), ud->range());
                if (sym)
                    sym->set_type(uty);
            }
            else if (auto* ed = dynamic_cast<ast::EnumDecl*>(decl))
            {
                auto* ety = m_types.make_enum(ed->name(), m_types.integer_type(32, true));
                auto* sym = declare(SymbolKind::Type, ed->name(), ed, ed->visibility(), ed->range());
                if (sym)
                    sym->set_type(ety);
            }
            else if (auto* alias = dynamic_cast<ast::UsingDecl*>(decl))
            {
                if (alias->kind() == ast::UsingKind::TypeAlias)
                {
                    auto* aty = m_types.make_alias(alias->name(), m_types.error_type());
                    auto* sym = declare(SymbolKind::Type, alias->name(), alias, alias->visibility(), alias->range());
                    if (sym)
                        sym->set_type(aty);
                }
            }
        }
    }

    void NameResolver::forward_declare_functions(std::span<ast::Decl* const> decls)
    {
        for (auto* decl : decls)
        {
            auto* fn = dynamic_cast<ast::FunctionDecl*>(decl);
            if (!fn)
                continue;

            if (m_current_scope->lookup_local(fn->name()))
                continue;

            auto* sym = declare(SymbolKind::Function, fn->name(), const_cast<ast::FunctionDecl*>(fn), fn->visibility(), fn->range());
            if (sym)
                sym->set_type(m_types.fresh_var());
        }
    }

    ast::Expr* NameResolver::disambiguate_expr(const ast::AmbiguousExpr& node)
    {
        for (auto* alt : node.alternatives())
        {
            if (auto* cast = dynamic_cast<ast::CastExpr*>(alt))
                if (auto* named = dynamic_cast<ast::NamedType*>(cast->target_type()))
                {
                    if (is_type_name(named->name()))
                    {
                        alt->accept(*this);
                        return alt;
                    }

                    continue;
                }

            alt->accept(*this);
            return alt;
        }

        if (!node.alternatives().empty())
        {
            node.alternatives()[0]->accept(*this);
            return node.alternatives()[0];
        }

        return nullptr;
    }

    ast::Stmt* NameResolver::disambiguate_stmt(const ast::AmbiguousStmt& node)
    {
        for (auto* alt : node.alternatives())
            if (auto* decl_stmt = dynamic_cast<ast::DeclStmt*>(alt))
                if (auto* var = dynamic_cast<ast::VarDecl*>(decl_stmt->decl()))
                    if (var->type())
                    {
                        const ast::TypeExpr* te = var->type();
                        while (auto* ptr = dynamic_cast<const ast::PointerType*>(te))
                            te = ptr->pointee();

                        if (auto* named = dynamic_cast<const ast::NamedType*>(te))
                        {
                            if (is_type_name(named->name()))
                            {
                                alt->accept(*this);
                                return alt;
                            }
                            continue;
                        }

                        alt->accept(*this);
                        return alt;
                    }

        for (auto* alt : node.alternatives())
        {
            if (dynamic_cast<ast::ExprStmt*>(alt))
            {
                alt->accept(*this);
                return alt;
            }
        }

        if (!node.alternatives().empty())
        {
            node.alternatives()[0]->accept(*this);
            return node.alternatives()[0];
        }

        return nullptr;
    }

    ast::Decl* NameResolver::disambiguate_decl(const ast::AmbiguousDecl& node)
    {
        if (!node.alternatives().empty())
        {
            auto* chosen = node.alternatives()[0];
            chosen->accept(*this);
            return chosen;
        }

        return nullptr;
    }

    void NameResolver::import_module_as_namespace(const ast::ImportDecl& import_decl, const ModuleInfo& mod)
    {
        if (!mod.export_scope)
            return;

        auto* target_ns = get_or_create_namespace_chain(import_decl.path());
        if (!target_ns)
            return;

        for (auto& [name, sym] : mod.export_scope->symbols())
        {
            if (!sym.is_public())
                continue;

            auto* existing = target_ns->lookup_local(name);
            if (existing)
                continue;

            auto* imported = target_ns->declare(sym.kind(), name, sym.decl(), sym.visibility());
            if (imported)
            {
                imported->set_type(sym.type());
                imported->set_quals(sym.quals());
                if (sym.is_namespace())
                    imported->set_namespace_scope(sym.namespace_scope());
            }
        }

        for (auto* exp_sym : mod.exported_usings)
        {
            if (!exp_sym)
                continue;

            auto* existing = target_ns->lookup_local(exp_sym->name());
            if (existing)
                continue;

            auto* imported = target_ns->declare(exp_sym->kind(), exp_sym->name(), exp_sym->decl(), ast::Visibility::Public);
            if (imported)
            {
                imported->set_type(exp_sym->type());
                imported->set_quals(exp_sym->quals());
                if (exp_sym->is_namespace())
                    imported->set_namespace_scope(exp_sym->namespace_scope());
            }
        }
    }

    Symbol* NameResolver::resolve_dotted_path(std::span<const si::InternedString> path, sm::SourceRange range)
    {
        auto* sym = m_current_scope->lookup_dotted(path);
        if (!sym)
        {
            std::string path_str;
            for (std::size_t i = 0; i < path.size(); ++i)
            {
                if (i > 0)
                    path_str += '.';
                path_str += path[i].view();
            }

            error(range, std::format("undeclared name '{}'", path_str));
            return nullptr;
        }

        return sym;
    }

    void NameResolver::bring_into_scope(std::span<const si::InternedString> path, si::InternedString local_name, sm::SourceRange range, bool is_export)
    {
        auto* sym = resolve_dotted_path(path, range);
        if (!sym)
            return;

        auto* existing = m_current_scope->lookup_local(local_name);
        if (existing)
        {
            error_redeclared(local_name, range, existing->decl() ? existing->decl()->range() : sm::SourceRange{});
            return;
        }

        auto vis = is_export ? ast::Visibility::Public : ast::Visibility::Private;
        auto* alias = m_current_scope->declare(sym->kind(), local_name, sym->decl(), vis);
        if (alias)
        {
            alias->set_type(sym->type());
            alias->set_quals(sym->quals());
            if (sym->is_namespace())
                alias->set_namespace_scope(sym->namespace_scope());

            if (sym->decl())
            {
                if (auto* ed = dynamic_cast<ast::EnumDecl*>(sym->decl()))
                {
                    Scope* found_enum_scope = nullptr;

                    auto find_in = [&](Scope* s) -> Scope* {
                        if (!s)
                            return nullptr;

                        for (auto* child : s->children())
                            if (child->owner() == ed)
                                return child;

                        return nullptr;
                    };

                    found_enum_scope = find_in(sym->scope());

                    if (!found_enum_scope)
                    {
                        for (auto* s = sym->scope(); s && !found_enum_scope; s = s->parent())
                        {
                            found_enum_scope = find_in(s);
                            if (!found_enum_scope)
                            {
                                for (auto* child : s->children())
                                {
                                    found_enum_scope = find_in(child);
                                    if (found_enum_scope)
                                        break;
                                }
                            }
                        }
                    }

                    if (!found_enum_scope && m_global_scope)
                    {
                        std::function<Scope*(Scope*)> deep_search = [&](Scope* s) -> Scope* {
                            for (auto* child : s->children())
                            {
                                if (child->owner() == ed)
                                    return child;

                                auto* result = deep_search(child);
                                if (result)
                                    return result;
                            }
                            return nullptr;
                        };

                        found_enum_scope = deep_search(m_global_scope.get());
                    }

                    if (!found_enum_scope)
                        found_enum_scope = m_modules.find_scope_for_node(ed);

                    if (found_enum_scope)
                    {
                        auto* enum_scope = m_current_scope->add_child(ScopeKind::Enum, ed);
                        for (auto& [vname, vsym] : found_enum_scope->symbols())
                        {
                            auto* new_vsym = enum_scope->declare(vsym.kind(), vname, vsym.decl(), vsym.visibility());
                            if (new_vsym)
                            {
                                new_vsym->set_type(vsym.type());
                                new_vsym->set_quals(vsym.quals());
                            }
                        }
                    }
                }
            }
        }

        if (is_export && alias)
            m_exported_usings.push_back(alias);
    }

    void NameResolver::import_module_symbols(const ModuleInfo& mod, sm::SourceRange import_site)
    {
        if (!mod.export_scope)
            return;

        for (auto& [name, sym] : mod.export_scope->symbols())
        {
            if (!sym.is_public())
                continue;

            auto* existing = m_current_scope->lookup_local(name);
            if (existing)
            {
                error(import_site, std::format("imported symbol '{}' from module '{}' conflicts with existing declaration", std::string{name.view()},
                                               mod.path.to_display()));
                continue;
            }

            auto* imported = m_current_scope->declare(sym.kind(), name, sym.decl(), sym.visibility());
            if (imported)
            {
                imported->set_type(sym.type());
                imported->set_quals(sym.quals());
            }
        }
    }

    void NameResolver::error(sm::SourceRange range, std::string message)
    {
        m_printer.emit(diag::error(std::move(message)).with_primary(range, ""));
        ++m_error_count;
    }

    void NameResolver::error_redeclared(si::InternedString name, sm::SourceRange new_range, sm::SourceRange prev_range)
    {
        auto d = diag::error(std::format("redeclaration of '{}'", std::string{name.view()})).with_primary(new_range, "redeclared here");
        if (prev_range.begin.offset != prev_range.end.offset)
            d.with_secondary(prev_range, "previously declared here");

        m_printer.emit(std::move(d));
        ++m_error_count;
    }

    void NameResolver::error_undeclared(si::InternedString name, sm::SourceRange range)
    {
        m_printer.emit(diag::error(std::format("use of undeclared identifier '{}'", std::string{name.view()})).with_primary(range, "not found in this scope"));
        ++m_error_count;
    }

    void NameResolver::visit(const ast::BuiltinType& node)
    {
        m_type_resolutions[&node] = m_types.from_primitive(node.kind());
    }

    void NameResolver::visit(const ast::NamedType& node)
    {
        auto* sym = resolve_name(node.name(), node.range());
        if (!sym || !sym->is_type())
        {
            if (sym)
                error(node.range(), std::format("'{}' does not name a type", std::string{node.name().view()}));

            m_type_resolutions[&node] = m_types.error_type();
            return;
        }

        m_resolutions[&node] = sym;
        m_type_resolutions[&node] = sym->type() ? sym->type() : m_types.error_type();
    }

    void NameResolver::visit(const ast::QualifiedType& node)
    {
        auto* inner = resolve_type(*node.inner());
        m_type_resolutions[&node] = inner;
    }

    void NameResolver::visit(const ast::DottedNamedType& node)
    {
        auto segments = node.segments();
        auto* sym = m_current_scope->lookup_dotted(segments);
        if (!sym || !sym->is_type())
        {
            std::string path_str;
            for (std::size_t i = 0; i < segments.size(); ++i)
            {
                if (i > 0)
                    path_str += '.';
                path_str += segments[i].view();
            }

            if (sym)
                error(node.range(), std::format("'{}' does not name a type", path_str));
            else
                error(node.range(), std::format("undeclared type '{}'", path_str));

            m_type_resolutions[&node] = m_types.error_type();
            return;
        }

        m_resolutions[&node] = sym;
        m_type_resolutions[&node] = sym->type() ? sym->type() : m_types.error_type();
    }

    void NameResolver::visit(const ast::PointerType& node)
    {
        ast::Qualifier pointee_quals = ast::Qualifier::None;
        const ast::TypeExpr* actual_pointee = node.pointee();

        if (auto* qt = dynamic_cast<const ast::QualifiedType*>(actual_pointee))
            pointee_quals = qt->quals();

        auto* pointee = resolve_type(*node.pointee());
        m_type_resolutions[&node] = m_types.pointer_to(pointee, pointee_quals);
    }

    void NameResolver::visit(const ast::SliceType& node)
    {
        auto* elem = resolve_type(*node.element());
        m_type_resolutions[&node] = m_types.slice_of(elem);
    }

    void NameResolver::visit(const ast::ArrayType& node)
    {
        auto* elem = resolve_type(*node.element());

        if (node.size())
            node.size()->accept(*this);

        m_type_resolutions[&node] = m_types.array_of(elem, 0);
    }

    void NameResolver::visit(const ast::FlexibleArrayType& node)
    {
        auto* elem = resolve_type(*node.element());
        m_type_resolutions[&node] = m_types.flexible_array_of(elem);
    }

    void NameResolver::visit(const ast::FunctionType& node)
    {
        auto* ret = resolve_type(*node.return_type());
        std::vector<SemaType*> params;
        params.reserve(node.param_types().size());
        for (auto* pt : node.param_types())
            params.push_back(resolve_type(*pt));

        m_type_resolutions[&node] = m_types.function_type(ret, std::move(params));
    }

    void NameResolver::visit(const ast::TemplateType& node)
    {
        resolve_type(*node.base());
        for (auto& arg : node.args())
            std::visit([this](auto* n) { n->accept(*this); }, arg.arg);

        auto it = m_type_resolutions.find(node.base());
        m_type_resolutions[&node] = (it != m_type_resolutions.end()) ? it->second : m_types.error_type();
    }

    void NameResolver::visit(const ast::TypeofType& node)
    {
        node.inner()->accept(*this);
        m_type_resolutions[&node] = m_types.fresh_var();
    }

    void NameResolver::visit(const ast::IntegerLiteral&) {}
    void NameResolver::visit(const ast::FloatLiteral&) {}
    void NameResolver::visit(const ast::StringLiteral&) {}
    void NameResolver::visit(const ast::CharLiteral&) {}
    void NameResolver::visit(const ast::BoolLiteral&) {}
    void NameResolver::visit(const ast::NullLiteral&) {}

    void NameResolver::visit(const ast::IdentifierExpr& node)
    {
        auto* sym = resolve_name(node.name(), node.range());
        if (sym)
            m_resolutions[&node] = sym;
    }

    void NameResolver::visit(const ast::GroupingExpr& node)
    {
        node.inner()->accept(*this);
    }

    void NameResolver::visit(const ast::BinaryExpr& node)
    {
        node.lhs()->accept(*this);
        node.rhs()->accept(*this);
    }

    void NameResolver::visit(const ast::UnaryExpr& node)
    {
        node.operand()->accept(*this);
    }

    void NameResolver::visit(const ast::AssignExpr& node)
    {
        node.target()->accept(*this);
        node.value()->accept(*this);
    }

    void NameResolver::visit(const ast::ConditionalExpr& node)
    {
        node.condition()->accept(*this);
        node.then_expr()->accept(*this);
        node.else_expr()->accept(*this);
    }

    void NameResolver::visit(const ast::CastExpr& node)
    {
        node.operand()->accept(*this);
        resolve_type(*node.target_type());
    }

    void NameResolver::visit(const ast::MemberAccessExpr& node)
    {
        std::vector<si::InternedString> chain;
        const ast::Expr* root = &node;

        while (auto* ma = dynamic_cast<const ast::MemberAccessExpr*>(root))
        {
            chain.push_back(ma->member());
            root = ma->object();
        }

        if (auto* id = dynamic_cast<const ast::IdentifierExpr*>(root))
        {
            chain.push_back(id->name());
            std::reverse(chain.begin(), chain.end());

            auto* sym = m_current_scope->lookup_dotted(chain);
            if (sym && !sym->is_namespace())
            {
                m_resolutions[&node] = sym;

                auto* root_sym = m_current_scope->lookup(chain[0]);
                if (root_sym)
                    m_resolutions[id] = root_sym;

                return;
            }

            for (std::size_t len = chain.size() - 1; len >= 1; --len)
            {
                auto prefix = std::span<const si::InternedString>(chain.data(), len);
                auto* prefix_sym = m_current_scope->lookup_dotted(prefix);
                if (prefix_sym)
                {
                    m_resolutions[id] = m_current_scope->lookup(chain[0]);

                    if (!prefix_sym->is_namespace())
                        break;
                }
            }
        }

        node.object()->accept(*this);
    }

    void NameResolver::visit(const ast::CallExpr& node)
    {
        node.callee()->accept(*this);
        for (auto& targ : node.template_args())
            std::visit([this](auto* n) { n->accept(*this); }, targ.arg);

        for (auto* arg : node.args())
            arg->accept(*this);

        if (auto* ma = dynamic_cast<const ast::MemberAccessExpr*>(node.callee()))
        {
            auto* func_sym = m_current_scope->lookup(ma->member());
            if (func_sym && func_sym->kind() == SymbolKind::Function)
                m_ufcs_calls[&node] = func_sym;
        }
    }

    void NameResolver::visit(const ast::IndexExpr& node)
    {
        node.object()->accept(*this);
        node.index()->accept(*this);
    }

    void NameResolver::visit(const ast::SliceExpr& node)
    {
        node.object()->accept(*this);
        if (node.begin_idx())
            node.begin_idx()->accept(*this);

        if (node.end_idx())
            node.end_idx()->accept(*this);
    }

    void NameResolver::visit(const ast::InitializerExpr& node)
    {
        if (node.type())
            resolve_type(*node.type());

        for (auto& field : node.fields())
            field.value->accept(*this);
    }

    void NameResolver::visit(const ast::BlockExpr& node)
    {
        push_scope(ScopeKind::Block, const_cast<ast::BlockExpr*>(&node));
        for (auto* stmt : node.stmts())
            stmt->accept(*this);

        if (node.tail())
            node.tail()->accept(*this);

        pop_scope();
    }

    void NameResolver::visit(const ast::MatchExpr& node)
    {
        node.scrutinee()->accept(*this);
        for (auto& arm : node.arms())
        {
            push_scope(ScopeKind::Block);
            arm.pattern->accept(*this);
            if (arm.guard)
                arm.guard->accept(*this);

            arm.body->accept(*this);
            pop_scope();
        }
    }

    void NameResolver::visit(const ast::SizeofExpr& node)
    {
        resolve_type(*node.operand());
    }

    void NameResolver::visit(const ast::AlignofExpr& node)
    {
        resolve_type(*node.operand());
    }

    void NameResolver::visit(const ast::MacroCallExpr& node)
    {
        for (auto* arg : node.args())
            arg->accept(*this);
    }

    void NameResolver::visit(const ast::ExprStmt& node)
    {
        node.expr()->accept(*this);
    }

    void NameResolver::visit(const ast::DeclStmt& node)
    {
        node.decl()->accept(*this);
    }

    void NameResolver::visit(const ast::BlockStmt& node)
    {
        push_scope(ScopeKind::Block, const_cast<ast::BlockStmt*>(&node));
        for (auto* stmt : node.stmts())
            stmt->accept(*this);

        pop_scope();
    }

    void NameResolver::visit(const ast::ReturnStmt& node)
    {
        if (node.value())
            node.value()->accept(*this);
    }

    void NameResolver::visit(const ast::IfStmt& node)
    {
        node.condition()->accept(*this);
        node.then_branch()->accept(*this);
        if (node.else_branch())
            node.else_branch()->accept(*this);
    }

    void NameResolver::visit(const ast::WhileStmt& node)
    {
        node.condition()->accept(*this);
        push_scope(ScopeKind::Loop, const_cast<ast::WhileStmt*>(&node));
        node.body()->accept(*this);
        pop_scope();
    }

    void NameResolver::visit(const ast::ForStmt& node)
    {
        push_scope(ScopeKind::Loop, const_cast<ast::ForStmt*>(&node));
        if (node.init())
            node.init()->accept(*this);

        if (node.condition())
            node.condition()->accept(*this);

        if (node.increment())
            node.increment()->accept(*this);

        node.body()->accept(*this);
        pop_scope();
    }

    void NameResolver::visit(const ast::DoWhileStmt& node)
    {
        push_scope(ScopeKind::Loop, const_cast<ast::DoWhileStmt*>(&node));
        node.body()->accept(*this);
        pop_scope();
        node.condition()->accept(*this);
    }

    void NameResolver::visit(const ast::BreakStmt& node)
    {
        if (!m_current_scope->is_in_loop())
            error(node.range(), "'break' statement not within a loop");
    }

    void NameResolver::visit(const ast::ContinueStmt& node)
    {
        if (!m_current_scope->is_in_loop())
            error(node.range(), "'continue' statement not within a loop");
    }

    void NameResolver::visit(const ast::DeferStmt& node)
    {
        node.body()->accept(*this);
    }

    void NameResolver::visit(const ast::MatchStmt& node)
    {
        node.scrutinee()->accept(*this);
        for (auto& arm : node.arms())
        {
            push_scope(ScopeKind::Block);
            arm.pattern->accept(*this);
            if (arm.guard)
                arm.guard->accept(*this);

            arm.body->accept(*this);
            pop_scope();
        }
    }

    void NameResolver::visit(const ast::EmptyStmt&) {}

    void NameResolver::visit(const ast::TemplateTypeParamDecl& node)
    {
        auto* sym = declare(SymbolKind::TemplateTypeParam, node.name(), const_cast<ast::TemplateTypeParamDecl*>(&node), ast::Visibility::Private, node.range());
        if (sym)
        {
            sym->set_type(m_types.fresh_var());
            m_resolutions[&node] = sym;
        }

        if (node.default_type())
            resolve_type(*node.default_type());
    }

    void NameResolver::visit(const ast::TemplateValueParamDecl& node)
    {
        resolve_type(*node.type());
        auto* sym =
            declare(SymbolKind::TemplateValueParam, node.name(), const_cast<ast::TemplateValueParamDecl*>(&node), ast::Visibility::Private, node.range());

        if (sym)
        {
            auto it = m_type_resolutions.find(node.type());
            sym->set_type(it != m_type_resolutions.end() ? it->second : m_types.error_type());
            m_resolutions[&node] = sym;
        }

        if (node.default_value())
            node.default_value()->accept(*this);
    }

    void NameResolver::visit(const ast::VarDecl& node)
    {
        if (node.type())
            resolve_type(*node.type());

        if (node.init())
            node.init()->accept(*this);

        auto* sym = declare(SymbolKind::Variable, node.name(), const_cast<ast::VarDecl*>(&node), ast::Visibility::Private, node.range());
        if (sym)
        {
            sym->set_quals(node.quals());
            if (node.type())
            {
                auto it = m_type_resolutions.find(node.type());
                sym->set_type(it != m_type_resolutions.end() ? it->second : m_types.fresh_var());
            }
            else
                sym->set_type(m_types.fresh_var());
        }
    }

    void NameResolver::visit(const ast::ParamDecl& node)
    {
        resolve_type(*node.type());
        if (node.default_value())
            node.default_value()->accept(*this);

        auto* sym = declare(SymbolKind::Parameter, node.name(), const_cast<ast::ParamDecl*>(&node), ast::Visibility::Private, node.range());
        if (sym)
        {
            auto it = m_type_resolutions.find(node.type());
            sym->set_type(it != m_type_resolutions.end() ? it->second : m_types.error_type());
        }
    }

    void NameResolver::visit(const ast::FieldDecl& node)
    {
        resolve_type(*node.type());
        if (node.default_value())
            node.default_value()->accept(*this);

        auto* sym = declare(SymbolKind::Field, node.name(), const_cast<ast::FieldDecl*>(&node), node.visibility(), node.range());
        if (sym)
        {
            auto it = m_type_resolutions.find(node.type());
            sym->set_type(it != m_type_resolutions.end() ? it->second : m_types.error_type());
        }
    }

    void NameResolver::visit(const ast::FunctionDecl& node)
    {
        auto* existing = m_current_scope->lookup_local(node.name());
        Symbol* sym = existing;
        if (!existing)
            sym = declare(SymbolKind::Function, node.name(), const_cast<ast::FunctionDecl*>(&node), node.visibility(), node.range());

        push_scope(ScopeKind::Function, const_cast<ast::FunctionDecl*>(&node));

        for (auto* tp : node.template_params())
            tp->accept(*this);

        SemaType* ret_type = m_types.void_type();
        if (node.return_type())
            ret_type = resolve_type(*node.return_type());

        std::vector<SemaType*> param_types;
        param_types.reserve(node.params().size());
        for (auto* param : node.params())
        {
            param->accept(*this);
            auto it = m_type_resolutions.find(param->type());
            param_types.push_back(it != m_type_resolutions.end() ? it->second : m_types.error_type());
        }

        auto* fn_type = m_types.function_type(ret_type, std::move(param_types));
        if (sym)
        {
            sym->set_type(fn_type);
            m_resolutions[&node] = sym;
        }

        if (node.body())
            node.body()->accept(*this);

        pop_scope();
    }

    void NameResolver::visit(const ast::StructDecl& node)
    {
        auto* sym = m_current_scope->lookup_local(node.name());
        if (sym)
            m_resolutions[&node] = sym;

        push_scope(ScopeKind::Struct, const_cast<ast::StructDecl*>(&node));

        for (auto* tp : node.template_params())
            tp->accept(*this);

        for (auto* field : node.fields())
            field->accept(*this);

        for (auto* method : node.methods())
            method->accept(*this);

        pop_scope();
    }

    void NameResolver::visit(const ast::UnionDecl& node)
    {
        auto* sym = m_current_scope->lookup_local(node.name());
        if (sym)
            m_resolutions[&node] = sym;

        push_scope(ScopeKind::Union, const_cast<ast::UnionDecl*>(&node));

        for (auto* field : node.fields())
            field->accept(*this);

        for (auto* method : node.methods())
            method->accept(*this);

        pop_scope();
    }

    void NameResolver::visit(const ast::EnumVariantDecl& node)
    {
        if (node.discriminant())
            node.discriminant()->accept(*this);

        for (auto* pt : node.payload_types())
            resolve_type(*pt);

        declare(SymbolKind::EnumVariant, node.name(), const_cast<ast::EnumVariantDecl*>(&node), ast::Visibility::Public, node.range());
    }

    void NameResolver::visit(const ast::EnumDecl& node)
    {
        auto* sym = m_current_scope->lookup_local(node.name());
        if (sym)
            m_resolutions[&node] = sym;

        if (node.underlying_type())
            resolve_type(*node.underlying_type());

        push_scope(ScopeKind::Enum, const_cast<ast::EnumDecl*>(&node));

        for (auto* variant : node.variants())
            variant->accept(*this);

        for (auto* method : node.methods())
            method->accept(*this);

        pop_scope();
    }

    void NameResolver::visit(const ast::ModuleDecl& node)
    {
        m_module_scope = push_scope(ScopeKind::Module, const_cast<ast::ModuleDecl*>(&node));
    }

    void NameResolver::visit(const ast::ImportDecl& node)
    {
        ModulePath mod_path;
        for (auto seg : node.path())
            mod_path.segments.push_back(seg);

        auto* mod = m_modules.load_module(mod_path, node.range());
        if (!mod)
        {
            ++m_error_count;
            return;
        }

        import_module_as_namespace(node, *mod);
    }

    void NameResolver::visit(const ast::UsingDecl& node)
    {
        switch (node.kind())
        {
            case ast::UsingKind::TypeAlias: {
                auto* resolved = resolve_type(*node.aliased_type());
                auto* sym = m_current_scope->lookup_local(node.name());

                if (sym && sym->type() && sym->type()->is_alias())
                    static_cast<AliasType*>(sym->type())->set_underlying(resolved);

                if (node.is_export() && sym)
                    m_exported_usings.push_back(sym);

                break;
            }

            case ast::UsingKind::Import: {
                auto path = node.import_path();
                if (path.empty())
                    break;

                bring_into_scope(path, node.name(), node.range(), node.is_export());
                break;
            }

            case ast::UsingKind::SymbolAlias: {
                auto target_path = node.import_path();
                if (target_path.empty())
                {
                    if (node.aliased_type())
                    {
                        auto* resolved = resolve_type(*node.aliased_type());
                        auto* sym = m_current_scope->lookup_local(node.name());
                        if (sym && sym->type() && sym->type()->is_alias())
                            static_cast<AliasType*>(sym->type())->set_underlying(resolved);
                    }
                    break;
                }

                bring_into_scope(target_path, node.name(), node.range(), node.is_export());
                break;
            }

            case ast::UsingKind::GroupImport: {
                auto base_path = node.import_path();
                auto names = node.group_names();

                for (auto imported_name : names)
                {
                    std::vector<si::InternedString> full_path;
                    full_path.reserve(base_path.size() + 1);
                    for (auto seg : base_path)
                        full_path.push_back(seg);

                    full_path.push_back(imported_name);

                    bring_into_scope(full_path, imported_name, node.range(), node.is_export());
                }
                break;
            }
        }
    }

    void NameResolver::visit(const ast::TranslationUnit& node)
    {
        if (node.module_decl())
            node.module_decl()->accept(*this);
        else
            m_module_scope = push_scope(ScopeKind::Module);

        for (auto* decl : node.decls())
        {
            if (auto* imp = dynamic_cast<ast::ImportDecl*>(decl))
                imp->accept(*this);
        }

        forward_declare_types(node.decls());
        forward_declare_functions(node.decls());

        for (auto* decl : node.decls())
        {
            if (dynamic_cast<ast::ModuleDecl*>(decl))
                continue;
            if (dynamic_cast<ast::ImportDecl*>(decl))
                continue;

            decl->accept(*this);
        }

        pop_scope();
    }

    void NameResolver::visit(const ast::LiteralPattern& node)
    {
        node.literal()->accept(*this);
    }

    void NameResolver::visit(const ast::BindingPattern& node)
    {
        declare(SymbolKind::Variable, node.name(), nullptr, ast::Visibility::Private, node.range());
        if (node.guard())
            node.guard()->accept(*this);
    }

    void NameResolver::visit(const ast::WildcardPattern&) {}

    void NameResolver::visit(const ast::EnumPattern& node)
    {
        if (!node.path().empty())
            resolve_qualified_path(node.path(), node.range());

        for (auto* sub : node.sub_patterns())
            sub->accept(*this);
    }

    void NameResolver::visit(const ast::StructPattern& node)
    {
        resolve_name(node.type_name(), node.range());
        for (auto& field : node.fields())
            field.binding->accept(*this);
    }

    void NameResolver::visit(const ast::RestPattern&) {}

    void NameResolver::visit(const ast::AmbiguousExpr& node)
    {
        auto* chosen = disambiguate_expr(node);
        if (chosen)
        {
            m_disambiguations[&node] = chosen;
            m_resolutions[&node] = m_resolutions.count(chosen) ? m_resolutions[chosen] : nullptr;
        }
    }

    void NameResolver::visit(const ast::AmbiguousStmt& node)
    {
        auto* chosen = disambiguate_stmt(node);
        if (chosen)
            m_disambiguations[&node] = chosen;
    }

    void NameResolver::visit(const ast::AmbiguousDecl& node)
    {
        auto* chosen = disambiguate_decl(node);
        if (chosen)
            m_disambiguations[&node] = chosen;
    }

} // namespace dcc::sema
