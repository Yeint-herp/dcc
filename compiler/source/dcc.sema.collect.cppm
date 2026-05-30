export module dcc.sema.collect;

import std;
import dcc.ast;
import dcc.diag;
import dcc.sema.scope;

export namespace dcc::sema
{
    class DeclCollector
    {
    public:
        DeclCollector(ModuleInfo& mod, diag::DiagnosticEngine& diag, std::pmr::polymorphic_allocator<> a) : m_mod{mod}, m_diag{diag}, m_alloc{a} {}

        void run()
        {
            if (m_mod.state >= ModuleState::Collected)
                return;

            m_mod.own_scope = make_scope(ScopeKind::Module, nullptr);
            m_mod.export_scope = make_scope(ScopeKind::Module, nullptr);
            m_mod.ufcs_scope = make_scope(ScopeKind::Module, nullptr);

            for (auto* d : m_mod.tu->decls)
                collect_top_level(d);

            m_mod.state = ModuleState::Collected;
        }

    private:
        ModuleInfo& m_mod;
        diag::DiagnosticEngine& m_diag;
        std::pmr::polymorphic_allocator<> m_alloc;

        Scope* make_scope(ScopeKind k, Scope const* parent)
        {
            auto* p = m_alloc.allocate_object<Scope>();
            return std::construct_at(p, k, parent, m_alloc);
        }

        void collect_top_level(ast::Decl* d)
        {
            switch (d->kind)
            {
                case ast::DeclKind::Struct:
                    register_type(d, ast::node_cast<ast::StructDecl>(d)->name, SymbolKind::Struct);
                    break;
                case ast::DeclKind::Union:
                    register_type(d, ast::node_cast<ast::UnionDecl>(d)->name, SymbolKind::Union);
                    break;
                case ast::DeclKind::Enum:
                    register_enum(static_cast<ast::EnumDecl*>(d));
                    break;
                case ast::DeclKind::Func:
                    register_function(static_cast<ast::FuncDecl*>(d));
                    break;
                case ast::DeclKind::Var:
                    register_variable(static_cast<ast::VarDecl*>(d));
                    break;
                case ast::DeclKind::Using:
                    m_mod.using_worklist.push_back(static_cast<ast::UsingDecl*>(d));
                    break;
                case ast::DeclKind::Module:
                case ast::DeclKind::Import:
                    break;
            }
        }

        Symbol make_symbol(ast::Decl* d, std::string_view name, SymbolKind kind)
        {
            Symbol s{};

            s.name = name;
            s.kind = kind;
            s.decl = d;
            s.module = &m_mod;
            s.is_exported = d->is_public;
            s.definition_range = d->range;

            return s;
        }

        void register_type(ast::Decl* d, std::string_view name, SymbolKind kind)
        {
            if (name.empty())
                return;

            auto s = make_symbol(d, name, kind);
            install_type(*m_mod.own_scope, s, d);
            if (d->is_public)
            {
                d->sema.exported = true;
                install_type(*m_mod.export_scope, s, d);
            }
        }

        void register_function(ast::FuncDecl* d)
        {
            if (d->name.empty())
                return;

            auto s = make_symbol(d, d->name, SymbolKind::Function);
            install_function(*m_mod.own_scope, s, d);
            install_function(*m_mod.ufcs_scope, s, d);
            if (d->is_public)
            {
                d->sema.exported = true;
                install_function(*m_mod.export_scope, s, d);
            }
        }

        void register_variable(ast::VarDecl* d)
        {
            if (d->name.empty())
                return;

            auto s = make_symbol(d, d->name, SymbolKind::Variable);
            install_variable(*m_mod.own_scope, s, d);
            if (d->is_public)
            {
                d->sema.exported = true;
                install_variable(*m_mod.export_scope, s, d);
            }
        }

        void register_enum(ast::EnumDecl* d)
        {
            if (d->name.empty())
                return;

            auto type_sym = make_symbol(d, d->name, SymbolKind::Enum);
            install_type(*m_mod.own_scope, type_sym, d);
            if (d->is_public)
            {
                d->sema.exported = true;
                install_type(*m_mod.export_scope, type_sym, d);
            }

            install_variant_scope(*m_mod.own_scope, d, type_sym);
            if (d->is_public)
                install_variant_scope(*m_mod.export_scope, d, type_sym);
        }

        void install_variant_scope(Scope& scope, ast::EnumDecl* d, Symbol const& anchor)
        {
            auto* variant_scope = scope.ensure_namespace(d->name, anchor, ScopeKind::Enum);
            for (auto& v : d->variants)
            {
                Symbol vs{};
                vs.name = v.name;
                vs.kind = SymbolKind::EnumVariant;
                vs.decl = d;
                vs.module = &m_mod;
                vs.is_exported = d->is_public;
                vs.definition_range = v.range;

                Symbol const* existing = nullptr;
                auto r = variant_scope->define_variable(vs, &existing);
                if (r == DefineResult::Conflict)
                {
                    m_diag.error(v.range, "duplicate enum variant `{}`", v.name);
                    if (existing)
                        m_diag.note(existing->definition_range, "previous variant was here");
                }
            }
        }

        void install_type(Scope& scope, Symbol const& s, ast::Decl* d)
        {
            Symbol const* existing = nullptr;
            auto r = scope.define_type(s, &existing);
            if (r == DefineResult::Conflict)
            {
                m_diag.error(d->range, "redefinition of type `{}`", s.name);
                if (existing)
                    m_diag.note(existing->definition_range, "previous definition was here");
            }
        }

        void install_function(Scope& scope, Symbol const& s, ast::Decl* d)
        {
            Symbol const* existing = nullptr;
            auto r = scope.add_function_overload(s, &existing);
            if (r == DefineResult::Conflict)
            {
                m_diag.error(d->range, "name `{}` already declared as a variable", s.name);
                if (existing)
                    m_diag.note(existing->definition_range, "previous definition was here");
            }
        }

        void install_variable(Scope& scope, Symbol const& s, ast::Decl* d)
        {
            Symbol const* existing = nullptr;
            auto r = scope.define_variable(s, &existing);
            if (r == DefineResult::Conflict)
            {
                m_diag.error(d->range, "redefinition of `{}`", s.name);
                if (existing)
                    m_diag.note(existing->definition_range, "previous definition was here");
            }
        }
    };

    void collect_all(std::span<std::unique_ptr<ModuleInfo> const> modules, diag::DiagnosticEngine& diag, std::pmr::polymorphic_allocator<> alloc)
    {
        for (auto const& m : modules)
        {
            if (!m->tu || m->state >= ModuleState::Collected)
                continue;

            DeclCollector{*m, diag, alloc}.run();
        }
    }

} // namespace dcc::sema
