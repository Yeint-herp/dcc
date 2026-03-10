#include <algorithm>
#include <format>
#include <sema/module_loader.hh>
#include <sema/type_context.hh>

namespace dcc::sema
{
    std::string ModulePath::to_key() const
    {
        std::string result;
        for (std::size_t i = 0; i < segments.size(); ++i)
        {
            if (i > 0)
                result += '\0';

            result += segments[i].view();
        }

        return result;
    }

    std::string ModulePath::to_display() const
    {
        std::string result;
        for (std::size_t i = 0; i < segments.size(); ++i)
        {
            if (i > 0)
                result += '.';

            result += segments[i].view();
        }

        return result;
    }

    bool ModulePath::operator==(const ModulePath& other) const noexcept
    {
        if (segments.size() != other.segments.size())
            return false;

        for (std::size_t i = 0; i < segments.size(); ++i)
            if (segments[i] != other.segments[i])
                return false;

        return true;
    }

    std::size_t ModulePathHash::operator()(const ModulePath& p) const noexcept
    {
        std::size_t h = 0;
        for (auto& seg : p.segments)
        {
            h ^= std::hash<const void*>{}(seg.data()) * 2654435761u;
            h = (h << 7) | (h >> (sizeof(std::size_t) * 8 - 7));
        }

        return h;
    }

    ModuleLoader::ModuleLoader(sm::SourceManager& sm) : m_sm{sm} {}

    void ModuleLoader::add_search_path(std::filesystem::path path)
    {
        m_search_paths.push_back(std::move(path));
    }

    void ModuleLoader::set_parse_callback(ParseModuleFn fn)
    {
        m_parse_fn = std::move(fn);
    }

    void ModuleLoader::set_analyze_callback(AnalyzeModuleFn fn)
    {
        m_analyze_fn = std::move(fn);
    }

    std::filesystem::path ModuleLoader::resolve_path(const ModulePath& mod_path) const
    {
        std::filesystem::path relative;
        for (auto& seg : mod_path.segments)
            relative /= std::string{seg.view()};

        relative += ".dcc";

        for (auto& search : m_search_paths)
        {
            auto candidate = search / relative;
            if (std::filesystem::exists(candidate))
                return candidate;
        }

        return {};
    }

    Scope* ModuleLoader::find_scope_for_node(const ast::Node* node) const noexcept
    {
        std::function<Scope*(Scope*)> search = [&](Scope* s) -> Scope* {
            if (!s)
                return nullptr;

            for (auto* child : s->children())
            {
                if (child->owner() == node)
                    return child;

                auto* result = search(child);
                if (result)
                    return result;
            }
            return nullptr;
        };

        for (auto& [key, mod] : m_modules)
            if (mod->owned_scope)
            {
                auto* result = search(mod->owned_scope.get());
                if (result)
                    return result;
            }

        return nullptr;
    }

    ModuleInfo* ModuleLoader::load_module(const ModulePath& path, sm::SourceRange import_site)
    {
        std::string key = path.to_key();

        if (auto it = m_modules.find(key); it != m_modules.end())
        {
            auto* mod = it->second.get();
            if (mod->state == ModuleState::Failed)
            {
                emit(diag::error(std::format("module '{}' previously failed to load", path.to_display())).with_primary(import_site, "imported here"));
                return nullptr;
            }

            if (mod->state == ModuleState::Parsing || mod->state == ModuleState::Analyzing)
            {
                emit(diag::error(std::format("circular import of module '{}'", path.to_display())).with_primary(import_site, "import cycle detected here"));
                return nullptr;
            }

            return mod;
        }

        if (is_circular(key))
        {
            emit(diag::error(std::format("circular import of module '{}'", path.to_display())).with_primary(import_site, "import cycle detected here"));
            return nullptr;
        }

        auto file_path = resolve_path(path);
        if (file_path.empty())
        {
            emit(diag::error(std::format("cannot find module '{}'", path.to_display()))
                     .with_primary(import_site, "imported here")
                     .with_note("searched in the following directories:")
                     .with_help([&] {
                         std::string dirs;
                         for (auto& sp : m_search_paths)
                             dirs += std::format("  {}\n", sp.string());

                         return dirs.empty() ? "no search paths configured" : dirs;
                     }()));

            return nullptr;
        }

        auto mod = std::make_unique<ModuleInfo>();
        mod->path = path;
        mod->file_path = file_path;
        mod->state = ModuleState::Parsing;

        ModuleInfo* raw = mod.get();
        m_modules[key] = std::move(mod);
        m_loading_stack.push_back(key);

        if (!m_parse_fn)
        {
            raw->state = ModuleState::Failed;
            m_loading_stack.pop_back();
            emit(diag::error("no parse callback configured for module loading").with_primary(import_site, "while importing this module"));

            return nullptr;
        }

        raw->translation_unit = m_parse_fn(file_path, m_sm);
        if (!raw->translation_unit)
        {
            raw->state = ModuleState::Failed;
            m_loading_stack.pop_back();
            emit(diag::error(std::format("failed to parse module '{}'", path.to_display())).with_primary(import_site, "imported here"));

            return nullptr;
        }
        raw->state = ModuleState::Parsed;

        raw->state = ModuleState::Analyzing;
        if (m_analyze_fn)
        {
            if (!m_analyze_fn(*raw))
            {
                raw->state = ModuleState::Failed;
                m_loading_stack.pop_back();
                emit(diag::error(std::format("failed to analyze module '{}'", path.to_display())).with_primary(import_site, "imported here"));

                return nullptr;
            }
        }

        raw->state = ModuleState::Ready;
        m_loading_stack.pop_back();
        return raw;
    }

    ModuleInfo* ModuleLoader::find_module(const ModulePath& path) const noexcept
    {
        std::string key = path.to_key();
        auto it = m_modules.find(key);
        if (it != m_modules.end())
            return it->second.get();

        return nullptr;
    }

    ModuleInfo* ModuleLoader::register_module(ModulePath path, std::filesystem::path file_path, ast::TranslationUnit* tu)
    {
        std::string key = path.to_key();
        auto mod = std::make_unique<ModuleInfo>();
        mod->path = std::move(path);
        mod->file_path = std::move(file_path);
        mod->translation_unit = tu;
        mod->state = ModuleState::Analyzing;

        ModuleInfo* raw = mod.get();
        m_modules[key] = std::move(mod);
        return raw;
    }

    std::span<const diag::Diagnostic> ModuleLoader::diagnostics() const noexcept
    {
        return m_diagnostics;
    }

    bool ModuleLoader::has_errors() const noexcept
    {
        return std::ranges::any_of(m_diagnostics, [](const diag::Diagnostic& d) { return d.severity() == diag::Severity::Error; });
    }

    void ModuleLoader::emit(diag::Diagnostic diag)
    {
        m_diagnostics.push_back(std::move(diag));
    }

    bool ModuleLoader::is_circular(const std::string& key) const noexcept
    {
        return std::ranges::find(m_loading_stack, key) != m_loading_stack.end();
    }

} // namespace dcc::sema
