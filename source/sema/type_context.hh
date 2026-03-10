#ifndef DCC_SEMA_TYPE_CONTEXT_HH
#define DCC_SEMA_TYPE_CONTEXT_HH

#include <ast/common.hh>
#include <cstdint>
#include <deque>
#include <memory>
#include <sema/types.hh>
#include <unordered_map>
#include <util/si.hh>
#include <vector>

namespace dcc::sema
{
    class TypeContext
    {
    public:
        TypeContext();
        ~TypeContext() = default;

        TypeContext(const TypeContext&) = delete;
        TypeContext& operator=(const TypeContext&) = delete;

        [[nodiscard]] ErrorType* error_type() noexcept { return &m_error; }
        [[nodiscard]] VoidType* void_type() noexcept { return &m_void; }
        [[nodiscard]] BoolType* bool_type() noexcept { return &m_bool; }
        [[nodiscard]] NullTType* null_t_type() noexcept { return &m_null_t; }

        [[nodiscard]] IntegerType* integer_type(uint8_t width, bool is_signed) noexcept;
        [[nodiscard]] FloatType* float_type(uint8_t width) noexcept;

        [[nodiscard]] SemaType* from_primitive(ast::PrimitiveKind pk) noexcept;

        [[nodiscard]] PointerSemaType* pointer_to(SemaType* pointee, ast::Qualifier quals = ast::Qualifier::None);
        [[nodiscard]] SliceSemaType* slice_of(SemaType* element);
        [[nodiscard]] ArraySemaType* array_of(SemaType* element, uint64_t length);
        [[nodiscard]] FlexibleArraySemaType* flexible_array_of(SemaType* element);
        [[nodiscard]] FunctionSemaType* function_type(SemaType* ret, std::vector<SemaType*> params, bool variadic = false);

        [[nodiscard]] StructSemaType* make_struct(si::InternedString name);
        [[nodiscard]] UnionSemaType* make_union(si::InternedString name);
        [[nodiscard]] EnumSemaType* make_enum(si::InternedString name, SemaType* underlying);
        [[nodiscard]] AliasType* make_alias(si::InternedString name, SemaType* underlying);

        [[nodiscard]] TypeVar* fresh_var();

        [[nodiscard]] SemaType* resolve(SemaType* ty) noexcept;

        [[nodiscard]] bool unify(SemaType* a, SemaType* b);

        [[nodiscard]] bool is_implicitly_convertible(SemaType* from, SemaType* to) const noexcept;
        [[nodiscard]] bool is_explicitly_castable(SemaType* from, SemaType* to) const noexcept;

        [[nodiscard]] bool types_equal(SemaType* a, SemaType* b) noexcept;

        [[nodiscard]] SemaType* common_arithmetic_type(SemaType* a, SemaType* b) noexcept;
        [[nodiscard]] SemaType* widen_integers(SemaType* a, SemaType* b) noexcept;

    private:
        ErrorType m_error;
        VoidType m_void;
        BoolType m_bool;
        NullTType m_null_t;

        IntegerType m_integers[8];
        FloatType m_f32;
        FloatType m_f64;

        std::deque<std::unique_ptr<SemaType>> m_arena;

        template <typename T, typename... Args> T* alloc(Args&&... args);

        struct TypeHash
        {
            uint64_t operator()(const SemaType* ty) const noexcept;
        };
        struct TypeEq
        {
            bool operator()(const SemaType* a, const SemaType* b) const noexcept;
        };
        std::unordered_map<const SemaType*, SemaType*, TypeHash, TypeEq> m_intern_map;

        SemaType* intern_or_create(std::unique_ptr<SemaType> candidate);

        uint32_t m_next_var_id{};

        SemaType* find_root(SemaType* ty) noexcept;
    };

} // namespace dcc::sema

#endif /* DCC_SEMA_TYPE_CONTEXT_HH */
