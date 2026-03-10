// ============================================================================
// sema/module_loader.hh - Add scope ownership to ModuleInfo
// ============================================================================
#ifndef DCC_SEMA_MODULE_LOADER_HH
#define DCC_SEMA_MODULE_LOADER_HH

#include <cstdint>
#include <diagnostics.hh>
#include <filesystem>
#include <functional>
#include <memory>
#include <sema/scope.hh>
#include <string>
#include <unordered_map>
#include <util/si.hh>
#include <util/source_manager.hh>
#include <vector>

namespace dcc::sema
{
    class TypeContext;

    struct ModulePath
    {
        std::vector<si::InternedString> segments;

        [[nodiscard]] std::string to_key() const;
        [[nodiscard]] std::string to_display() const;

        [[nodiscard]] bool operator==(const ModulePath& other) const noexcept;
    };

    struct ModulePathHash
    {
        std::size_t operator()(const ModulePath& p) const noexcept;
    };

    enum class ModuleState : uint8_t
    {
        Unloaded,
        Parsing,
        Parsed,
        Analyzing,
        Ready,
        Failed,
    };

    struct ModuleInfo
    {
        ModulePath path;
        std::filesystem::path file_path;
        ast::TranslationUnit* translation_unit{nullptr};
        Scope* export_scope{nullptr};
        ModuleState state{ModuleState::Unloaded};
        sm::FileId file_id{};
        std::vector<Symbol*> exported_usings;
        std::unique_ptr<Scope> owned_scope;
    };

    using ParseModuleFn = std::function<ast::TranslationUnit*(const std::filesystem::path& file_path, sm::SourceManager& sm)>;
    using AnalyzeModuleFn = std::function<bool(ModuleInfo& module)>;

    class ModuleLoader
    {
    public:
        explicit ModuleLoader(sm::SourceManager& sm);

        void add_search_path(std::filesystem::path path);
        void set_parse_callback(ParseModuleFn fn);
        void set_analyze_callback(AnalyzeModuleFn fn);

        [[nodiscard]] std::filesystem::path resolve_path(const ModulePath& mod_path) const;

        [[nodiscard]] Scope* find_scope_for_node(const ast::Node* node) const noexcept;

        [[nodiscard]] ModuleInfo* load_module(const ModulePath& path, sm::SourceRange import_site);

        [[nodiscard]] ModuleInfo* find_module(const ModulePath& path) const noexcept;

        ModuleInfo* register_module(ModulePath path, std::filesystem::path file_path, ast::TranslationUnit* tu);

        [[nodiscard]] std::span<const diag::Diagnostic> diagnostics() const noexcept;
        [[nodiscard]] bool has_errors() const noexcept;

    private:
        sm::SourceManager& m_sm;
        ParseModuleFn m_parse_fn;
        AnalyzeModuleFn m_analyze_fn;

        std::vector<std::filesystem::path> m_search_paths;
        std::unordered_map<std::string, std::unique_ptr<ModuleInfo>> m_modules;

        std::vector<std::string> m_loading_stack;

        std::vector<diag::Diagnostic> m_diagnostics;

        void emit(diag::Diagnostic diag);
        [[nodiscard]] bool is_circular(const std::string& key) const noexcept;
    };

} // namespace dcc::sema

#endif /* DCC_SEMA_MODULE_LOADER_HH */
