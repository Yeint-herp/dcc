#ifndef DCC_SEMA_SEMA_HH
#define DCC_SEMA_SEMA_HH

#include <diagnostics.hh>
#include <filesystem>
#include <sema/module_loader.hh>
#include <sema/name_resolver.hh>
#include <sema/scope.hh>
#include <sema/type_checker.hh>
#include <sema/type_context.hh>
#include <util/si.hh>
#include <util/source_manager.hh>

namespace dcc::sema
{
    class Sema
    {
    public:
        explicit Sema(sm::SourceManager& sm, si::StringInterner& interner, diag::DiagnosticPrinter& printer);

        ~Sema() = default;

        Sema(const Sema&) = delete;
        Sema& operator=(const Sema&) = delete;

        void add_search_path(std::filesystem::path path);
        void set_parse_callback(ParseModuleFn fn);

        [[nodiscard]] bool analyze(ast::TranslationUnit& tu);

        [[nodiscard]] bool analyze_module(ModuleInfo& mod);

        [[nodiscard]] SemaType* type_of(const ast::Node* node) const noexcept;
        [[nodiscard]] Symbol* symbol_of(const ast::Node* node) const noexcept;
        [[nodiscard]] Scope* scope_of(const ast::Node* node) const noexcept;

        [[nodiscard]] ast::Node* disambiguated(const ast::Node* node) const noexcept
        {
            auto it = m_disambiguations.find(node);
            return it != m_disambiguations.end() ? it->second : nullptr;
        }

        [[nodiscard]] SemaType* resolve_type_expr(const ast::TypeExpr* te) const noexcept
        {
            auto it = m_type_resolutions.find(te);
            return it != m_type_resolutions.end() ? it->second : nullptr;
        }

        [[nodiscard]] TypeContext& types() noexcept { return m_types; }
        [[nodiscard]] ModuleLoader& modules() noexcept { return m_modules; }
        [[nodiscard]] Scope* global_scope() const noexcept { return m_global_scope.get(); }

        [[nodiscard]] bool is_ufcs_call(const ast::CallExpr* node) const noexcept;
        [[nodiscard]] Symbol* ufcs_target(const ast::CallExpr* node) const noexcept;

        [[nodiscard]] bool has_errors() const noexcept { return m_error_count > 0; }
        [[nodiscard]] uint32_t error_count() const noexcept { return m_error_count; }

    private:
        si::StringInterner& m_interner;
        diag::DiagnosticPrinter& m_printer;

        TypeContext m_types;
        ModuleLoader m_modules;
        DisambiguationMap m_disambiguations;

        std::unique_ptr<Scope> m_global_scope;

        ResolutionMap m_resolutions;
        TypeResolutionMap m_type_resolutions;
        TypeMap m_type_map;
        UfcsMap m_ufcs_candidates;
        ConfirmedUfcsMap m_confirmed_ufcs;

        std::unordered_map<const ast::Node*, Scope*> m_scope_map;

        uint32_t m_error_count{};

        [[nodiscard]] bool run_name_resolution(ast::TranslationUnit& tu);
        [[nodiscard]] bool run_type_checking(ast::TranslationUnit& tu);

        void populate_builtins();
        void build_scope_map(Scope* scope);
    };

} // namespace dcc::sema

#endif /* DCC_SEMA_SEMA_HH */
