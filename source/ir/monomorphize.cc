#include <ir/mangle.hh>
#include <ir/monomorphize.hh>

namespace dcc::ir
{
    std::size_t MonoKeyHash::operator()(const MonoKey& k) const noexcept
    {
        auto h = std::hash<const void*>{}(k.decl);
        for (auto* t : k.type_args)
            h ^= std::hash<const void*>{}(t) * 2654435761u;

        return h;
    }

    std::string Monomorphizer::request(const ast::FunctionDecl& decl, std::span<sema::SemaType* const> type_args,
                                       std::span<const si::InternedString> module_path)
    {
        MonoKey key{&decl, {type_args.begin(), type_args.end()}};

        if (auto it = m_cache.find(key); it != m_cache.end())
            return it->second;

        auto name_sv = decl.name().view();
        auto mangled = Mangler::mangle_function(module_path, name_sv, {}, type_args);

        m_cache.emplace(key, mangled);
        m_pending.push_back({&decl, {type_args.begin(), type_args.end()}, mangled});
        return mangled;
    }

    void Monomorphizer::flush()
    {
        while (!m_pending.empty())
        {
            auto batch = std::move(m_pending);
            m_pending.clear();

            for (auto& entry : batch)
            {
                if (m_emitter)
                    m_emitter(*entry.decl, entry.type_args, entry.mangled_name);
            }
        }
    }

} // namespace dcc::ir
