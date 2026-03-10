#ifndef DCC_SEMA_SCOPE_HH
#define DCC_SEMA_SCOPE_HH

#include <ast/common.hh>
#include <ast/decl.hh>
#include <ast/node.hh>
#include <cstdint>
#include <sema/types.hh>
#include <unordered_map>
#include <util/si.hh>
#include <util/source_manager.hh>
#include <vector>

namespace dcc::sema
{
    enum class SymbolKind : uint8_t
    {
        Variable,
        Parameter,
        Field,
        Function,
        Type,
        EnumVariant,
        Module,
        Namespace,
        TemplateTypeParam,
        TemplateValueParam,
    };

    class Scope;

    class Symbol
    {
    public:
        explicit Symbol(SymbolKind kind, si::InternedString name, ast::Decl* decl, Scope* owning_scope, ast::Visibility vis = ast::Visibility::Private) noexcept
            : m_kind{kind}, m_name{name}, m_decl{decl}, m_scope{owning_scope}, m_vis{vis}
        {
        }

        [[nodiscard]] SymbolKind kind() const noexcept { return m_kind; }
        [[nodiscard]] si::InternedString name() const noexcept { return m_name; }
        [[nodiscard]] ast::Decl* decl() const noexcept { return m_decl; }
        [[nodiscard]] Scope* scope() const noexcept { return m_scope; }
        [[nodiscard]] ast::Visibility visibility() const noexcept { return m_vis; }

        [[nodiscard]] SemaType* type() const noexcept { return m_type; }
        void set_type(SemaType* ty) noexcept { m_type = ty; }

        [[nodiscard]] ast::Qualifier quals() const noexcept { return m_quals; }
        void set_quals(ast::Qualifier q) noexcept { m_quals = q; }
        [[nodiscard]] ast::StorageClass storage_class() const noexcept { return m_sc; }

        [[nodiscard]] bool is_extern() const noexcept;
        [[nodiscard]] bool is_public() const noexcept { return m_vis == ast::Visibility::Public; }
        [[nodiscard]] bool is_type() const noexcept { return m_kind == SymbolKind::Type; }
        [[nodiscard]] bool is_namespace() const noexcept { return m_kind == SymbolKind::Namespace; }

        [[nodiscard]] Scope* namespace_scope() const noexcept { return m_namespace_scope; }
        void set_namespace_scope(Scope* s) noexcept { m_namespace_scope = s; }

    private:
        SymbolKind m_kind;
        si::InternedString m_name;
        ast::Decl* m_decl;
        Scope* m_scope;
        ast::Visibility m_vis;
        SemaType* m_type{nullptr};
        ast::Qualifier m_quals{ast::Qualifier::None};
        ast::StorageClass m_sc{};
        Scope* m_namespace_scope{nullptr};
    };

    enum class ScopeKind : uint8_t
    {
        Global,
        Module,
        Namespace,
        Function,
        Block,
        Struct,
        Union,
        Enum,
        Loop,
    };

    class Scope
    {
    public:
        explicit Scope(ScopeKind kind, Scope* parent = nullptr, ast::Node* owner = nullptr) noexcept : m_kind{kind}, m_parent{parent}, m_owner{owner} {}

        [[nodiscard]] ScopeKind kind() const noexcept { return m_kind; }
        [[nodiscard]] Scope* parent() const noexcept { return m_parent; }
        [[nodiscard]] ast::Node* owner() const noexcept { return m_owner; }

        Symbol* declare(SymbolKind kind, si::InternedString name, ast::Decl* decl, ast::Visibility vis = ast::Visibility::Private);

        [[nodiscard]] Symbol* lookup_local(si::InternedString name) const noexcept;
        [[nodiscard]] Symbol* lookup(si::InternedString name) const noexcept;
        [[nodiscard]] Symbol* lookup_qualified(std::span<const si::InternedString> path) const noexcept;

        [[nodiscard]] Symbol* lookup_dotted(std::span<const si::InternedString> segments) const noexcept;

        using SymbolMap = std::unordered_map<si::InternedString, Symbol>;

        [[nodiscard]] const SymbolMap& symbols() const noexcept { return m_symbols; }
        [[nodiscard]] std::span<Scope* const> children() const noexcept { return m_children; }

        Scope* add_child(ScopeKind kind, ast::Node* owner = nullptr);

        [[nodiscard]] Scope* enclosing(ScopeKind kind) noexcept;
        [[nodiscard]] const Scope* enclosing(ScopeKind kind) const noexcept;

        [[nodiscard]] Scope* enclosing_function() noexcept;

        [[nodiscard]] bool is_in_loop() const noexcept;

    private:
        ScopeKind m_kind;
        Scope* m_parent;
        ast::Node* m_owner;
        SymbolMap m_symbols;
        std::vector<std::unique_ptr<Scope>> m_children_storage;
        std::vector<Scope*> m_children;
    };

    class ScopeGuard
    {
    public:
        ScopeGuard(Scope*& current, Scope* entering) noexcept : m_slot{current}, m_previous{current} { current = entering; }
        ~ScopeGuard() { m_slot = m_previous; }

        ScopeGuard(const ScopeGuard&) = delete;
        ScopeGuard& operator=(const ScopeGuard&) = delete;

    private:
        Scope*& m_slot;
        Scope* m_previous;
    };

} // namespace dcc::sema

#endif /* DCC_SEMA_SCOPE_HH */
