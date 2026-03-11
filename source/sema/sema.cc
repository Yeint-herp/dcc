#include <ast/decl.hh>
#include <sema/sema.hh>

namespace dcc::sema
{

    Sema::Sema(sm::SourceManager& sm, si::StringInterner& interner, diag::DiagnosticPrinter& printer) : m_interner{interner}, m_printer{printer}, m_modules{sm}
    {
        m_modules.set_analyze_callback([this](ModuleInfo& mod) -> bool { return analyze_module(mod); });
    }

    void Sema::add_search_path(std::filesystem::path path)
    {
        m_modules.add_search_path(std::move(path));
    }

    void Sema::set_parse_callback(ParseModuleFn fn)
    {
        m_modules.set_parse_callback(std::move(fn));
    }

    bool Sema::analyze(ast::TranslationUnit& tu)
    {
        if (!run_name_resolution(tu))
            return false;

        if (m_modules.has_errors())
        {
            for (auto& diag : m_modules.diagnostics())
                m_printer.emit(diag);

            return false;
        }

        if (!run_type_checking(tu))
            return false;

        return m_error_count == 0;
    }

    bool Sema::analyze_module(ModuleInfo& mod)
    {
        if (!mod.translation_unit)
            return false;

        NameResolver resolver{m_types, m_modules, m_printer};
        if (!resolver.resolve(*mod.translation_unit))
        {
            m_error_count += resolver.error_count();
            return false;
        }

        auto* module_scope = resolver.module_scope();
        mod.export_scope = module_scope;

        mod.exported_usings = resolver.exported_usings();

        for (auto& [node, sym] : resolver.resolution_map())
            m_resolutions[node] = sym;

        for (auto& [node, ty] : resolver.type_resolution_map())
            m_type_resolutions[node] = ty;

        for (auto& [node, chosen] : resolver.disambiguation_map())
            m_disambiguations[node] = chosen;

        mod.owned_scope = resolver.take_global_scope();

        TypeChecker checker{m_types, m_resolutions, m_type_resolutions, m_disambiguations, m_ufcs_candidates, m_printer};
        if (!checker.check(*mod.translation_unit))
        {
            m_error_count += checker.error_count();
            return false;
        }

        for (auto& [node, ty] : checker.type_map())
            m_type_map[node] = ty;

        return true;
    }

    bool Sema::run_name_resolution(ast::TranslationUnit& tu)
    {
        NameResolver resolver{m_types, m_modules, m_printer};

        if (!resolver.resolve(tu))
        {
            m_error_count += resolver.error_count();
            if (m_modules.has_errors())
            {
                for (auto& diag : m_modules.diagnostics())
                    m_printer.emit(diag);
            }
            return false;
        }

        if (m_modules.has_errors())
        {
            for (auto& diag : m_modules.diagnostics())
                m_printer.emit(diag);

            m_error_count++;
            return false;
        }

        m_resolutions = resolver.resolution_map();
        m_type_resolutions = resolver.type_resolution_map();
        m_disambiguations = resolver.disambiguation_map();
        m_global_scope = resolver.take_global_scope();
        m_ufcs_candidates = resolver.ufcs_map();

        populate_builtins();

        build_scope_map(m_global_scope.get());

        return true;
    }

    bool Sema::run_type_checking(ast::TranslationUnit& tu)
    {
        TypeChecker checker{m_types, m_resolutions, m_type_resolutions, m_disambiguations, m_ufcs_candidates, m_printer};

        if (!checker.check(tu))
        {
            m_error_count += checker.error_count();
            return false;
        }

        m_type_map = checker.type_map();
        m_confirmed_ufcs = checker.confirmed_ufcs();
        return true;
    }

    SemaType* Sema::type_of(const ast::Node* node) const noexcept
    {
        auto it = m_type_map.find(node);
        return it != m_type_map.end() ? it->second : nullptr;
    }

    Symbol* Sema::symbol_of(const ast::Node* node) const noexcept
    {
        auto it = m_resolutions.find(node);
        return it != m_resolutions.end() ? it->second : nullptr;
    }

    Scope* Sema::scope_of(const ast::Node* node) const noexcept
    {
        auto it = m_scope_map.find(node);
        return it != m_scope_map.end() ? it->second : nullptr;
    }

    bool Sema::is_ufcs_call(const ast::CallExpr* node) const noexcept
    {
        return m_confirmed_ufcs.contains(node);
    }

    Symbol* Sema::ufcs_target(const ast::CallExpr* node) const noexcept
    {
        auto it = m_confirmed_ufcs.find(node);
        return it != m_confirmed_ufcs.end() ? it->second : nullptr;
    }

    void Sema::populate_builtins()
    {
        if (!m_global_scope)
            return;

        struct
        {
            std::string_view name;
            SemaType* type;
        } builtins[] = {
            {"i8", m_types.integer_type(8, true)},   {"u8", m_types.integer_type(8, false)},
            {"i16", m_types.integer_type(16, true)}, {"u16", m_types.integer_type(16, false)},
            {"i32", m_types.integer_type(32, true)}, {"u32", m_types.integer_type(32, false)},
            {"i64", m_types.integer_type(64, true)}, {"u64", m_types.integer_type(64, false)},
            {"f32", m_types.float_type(32)},         {"f64", m_types.float_type(64)},
            {"bool", m_types.bool_type()},           {"void", m_types.void_type()},
            {"null_t", m_types.null_t_type()},
        };

        for (auto& [name, type] : builtins)
        {
            auto interned = m_interner.intern(name);
            if (!m_global_scope->lookup_local(interned))
            {
                auto* sym = m_global_scope->declare(SymbolKind::Type, interned, nullptr, ast::Visibility::Public);
                if (sym)
                    sym->set_type(type);
            }
        }
    }

    void Sema::build_scope_map(Scope* scope)
    {
        if (!scope)
            return;

        if (scope->owner())
            m_scope_map[scope->owner()] = scope;

        for (auto* child : scope->children())
            build_scope_map(child);
    }

} // namespace dcc::sema
