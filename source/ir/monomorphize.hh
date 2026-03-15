#ifndef DCC_IR_MONOMORPHIZE_HH
#define DCC_IR_MONOMORPHIZE_HH

#include <ast/decl.hh>
#include <functional>
#include <sema/type_context.hh>
#include <string>
#include <unordered_map>
#include <vector>

namespace dcc::ir
{
    struct Module;

    struct MonoKey
    {
        const ast::FunctionDecl* decl;
        std::vector<sema::SemaType*> type_args;

        bool operator==(const MonoKey&) const = default;
    };

    struct MonoKeyHash
    {
        std::size_t operator()(const MonoKey& k) const noexcept;
    };

    using EmitSpecializationFn =
        std::function<void(const ast::FunctionDecl& decl, std::span<sema::SemaType* const> type_args, const std::string& mangled_name)>;

    class Monomorphizer
    {
    public:
        explicit Monomorphizer() {}

        void set_emitter(EmitSpecializationFn fn) { m_emitter = std::move(fn); }

        [[nodiscard]] std::string request(const ast::FunctionDecl& decl, std::span<sema::SemaType* const> type_args,
                                          std::span<const si::InternedString> module_path);

        void flush();

        [[nodiscard]] bool has_pending() const noexcept { return !m_pending.empty(); }

    private:
        EmitSpecializationFn m_emitter;

        std::unordered_map<MonoKey, std::string, MonoKeyHash> m_cache;

        struct PendingEntry
        {
            const ast::FunctionDecl* decl;
            std::vector<sema::SemaType*> type_args;
            std::string mangled_name;
        };
        std::vector<PendingEntry> m_pending;
    };

} // namespace dcc::ir

#endif /* DCC_IR_MONOMORPHIZE_HH */
