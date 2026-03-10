#include <ast/decl.hh>
#include <sema/scope.hh>

namespace dcc::sema
{
    bool Symbol::is_extern() const noexcept
    {
        if (m_kind != SymbolKind::Function && m_kind != SymbolKind::Variable)
            return false;

        if (auto* fn = dynamic_cast<ast::FunctionDecl*>(m_decl))
            return fn->storage_class() == ast::StorageClass::Extern;

        if (auto* var = dynamic_cast<ast::VarDecl*>(m_decl))
            return var->storage_class() == ast::StorageClass::Extern;

        return false;
    }

    Symbol* Scope::declare(SymbolKind kind, si::InternedString name, ast::Decl* decl, ast::Visibility vis)
    {
        auto [it, inserted] = m_symbols.try_emplace(name, Symbol{kind, name, decl, this, vis});
        if (!inserted)
            return nullptr;

        return &it->second;
    }

    Symbol* Scope::lookup_local(si::InternedString name) const noexcept
    {
        auto it = m_symbols.find(name);
        if (it != m_symbols.end())
            return const_cast<Symbol*>(&it->second);

        return nullptr;
    }

    Symbol* Scope::lookup(si::InternedString name) const noexcept
    {
        if (auto* sym = lookup_local(name))
            return sym;

        if (m_parent)
            return m_parent->lookup(name);

        return nullptr;
    }

    Symbol* Scope::lookup_qualified(std::span<const si::InternedString> path) const noexcept
    {
        if (path.empty())
            return nullptr;

        if (path.size() == 1)
            return lookup(path[0]);

        Symbol* first = lookup(path[0]);
        if (!first || !first->is_type())
            return nullptr;

        const Scope* current = nullptr;
        for (auto* child : m_children)
            if (child->owner() == first->decl())
            {
                current = child;
                break;
            }

        if (!current && m_parent)
        {
            Scope* first_scope = first->scope();
            for (auto* child : first_scope->children())
                if (child->owner() == first->decl())
                {
                    current = child;
                    break;
                }
        }

        if (!current)
            return nullptr;

        for (std::size_t i = 1; i < path.size() - 1; ++i)
        {
            Symbol* seg = current->lookup_local(path[i]);
            if (!seg || !seg->is_type())
                return nullptr;

            const Scope* next = nullptr;
            for (auto* child : current->children())
                if (child->owner() == seg->decl())
                {
                    next = child;
                    break;
                }

            if (!next)
                return nullptr;

            current = next;
        }

        return current->lookup_local(path.back());
    }

    Scope* Scope::add_child(ScopeKind kind, ast::Node* owner)
    {
        auto child = std::make_unique<Scope>(kind, this, owner);
        Scope* raw = child.get();
        m_children_storage.push_back(std::move(child));
        m_children.push_back(raw);

        return raw;
    }

    Scope* Scope::enclosing(ScopeKind target) noexcept
    {
        for (Scope* s = this; s; s = s->m_parent)
            if (s->m_kind == target)
                return s;

        return nullptr;
    }

    const Scope* Scope::enclosing(ScopeKind target) const noexcept
    {
        for (const Scope* s = this; s; s = s->m_parent)
            if (s->m_kind == target)
                return s;

        return nullptr;
    }

    Scope* Scope::enclosing_function() noexcept
    {
        return enclosing(ScopeKind::Function);
    }

    bool Scope::is_in_loop() const noexcept
    {
        return enclosing(ScopeKind::Loop) != nullptr;
    }

} // namespace dcc::sema
