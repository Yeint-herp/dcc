#ifndef DCC_SEMA_TYPES_HH
#define DCC_SEMA_TYPES_HH

#include <ast/common.hh>
#include <ast/decl.hh>
#include <ast/node.hh>
#include <cstdint>
#include <span>
#include <string>
#include <util/si.hh>
#include <vector>

namespace dcc::sema
{
    class SemaType
    {
    public:
        enum class Kind : uint8_t
        {
            Error,
            Void,
            Bool,
            NullT,
            Integer,
            Float,
            Pointer,
            Slice,
            Array,
            FlexibleArray,
            Function,
            Struct,
            Union,
            Enum,
            Alias,
            TypeVar,
        };

        explicit constexpr SemaType(Kind kind) noexcept : m_kind{kind} {}
        virtual ~SemaType() = default;

        SemaType(const SemaType&) = delete;
        SemaType& operator=(const SemaType&) = delete;

        [[nodiscard]] constexpr Kind kind() const noexcept { return m_kind; }

        [[nodiscard]] bool is_error() const noexcept { return m_kind == Kind::Error; }
        [[nodiscard]] bool is_null_t() const noexcept { return m_kind == Kind::NullT; }
        [[nodiscard]] bool is_void() const noexcept { return m_kind == Kind::Void; }
        [[nodiscard]] bool is_bool() const noexcept { return m_kind == Kind::Bool; }
        [[nodiscard]] bool is_integer() const noexcept { return m_kind == Kind::Integer; }
        [[nodiscard]] bool is_float() const noexcept { return m_kind == Kind::Float; }
        [[nodiscard]] bool is_numeric() const noexcept { return is_integer() || is_float(); }
        [[nodiscard]] bool is_pointer() const noexcept { return m_kind == Kind::Pointer; }
        [[nodiscard]] bool is_slice() const noexcept { return m_kind == Kind::Slice; }
        [[nodiscard]] bool is_array() const noexcept { return m_kind == Kind::Array; }
        [[nodiscard]] bool is_function() const noexcept { return m_kind == Kind::Function; }
        [[nodiscard]] bool is_aggregate() const noexcept { return m_kind == Kind::Struct || m_kind == Kind::Union; }
        [[nodiscard]] bool is_enum() const noexcept { return m_kind == Kind::Enum; }
        [[nodiscard]] bool is_type_var() const noexcept { return m_kind == Kind::TypeVar; }
        [[nodiscard]] bool is_alias() const noexcept { return m_kind == Kind::Alias; }

        [[nodiscard]] virtual std::string to_string() const = 0;

        [[nodiscard]] virtual bool structural_eq(const SemaType& other) const = 0;
        [[nodiscard]] virtual uint64_t structural_hash() const = 0;

    private:
        Kind m_kind;
    };

    class ErrorType final : public SemaType
    {
    public:
        ErrorType() noexcept : SemaType{Kind::Error} {}
        [[nodiscard]] std::string to_string() const override;
        [[nodiscard]] bool structural_eq(const SemaType& other) const override;
        [[nodiscard]] uint64_t structural_hash() const override;
    };

    class VoidType final : public SemaType
    {
    public:
        VoidType() noexcept : SemaType{Kind::Void} {}
        [[nodiscard]] std::string to_string() const override;
        [[nodiscard]] bool structural_eq(const SemaType& other) const override;
        [[nodiscard]] uint64_t structural_hash() const override;
    };

    class BoolType final : public SemaType
    {
    public:
        BoolType() noexcept : SemaType{Kind::Bool} {}
        [[nodiscard]] std::string to_string() const override;
        [[nodiscard]] bool structural_eq(const SemaType& other) const override;
        [[nodiscard]] uint64_t structural_hash() const override;
    };

    class NullTType final : public SemaType
    {
    public:
        NullTType() noexcept : SemaType{Kind::NullT} {}
        [[nodiscard]] std::string to_string() const override;
        [[nodiscard]] bool structural_eq(const SemaType& other) const override;
        [[nodiscard]] uint64_t structural_hash() const override;
    };

    class IntegerType final : public SemaType
    {
    public:
        explicit constexpr IntegerType(uint8_t width, bool is_signed) noexcept : SemaType{Kind::Integer}, m_width{width}, m_signed{is_signed} {}

        [[nodiscard]] constexpr uint8_t width() const noexcept { return m_width; }
        [[nodiscard]] constexpr bool is_signed() const noexcept { return m_signed; }

        [[nodiscard]] std::string to_string() const override;
        [[nodiscard]] bool structural_eq(const SemaType& other) const override;
        [[nodiscard]] uint64_t structural_hash() const override;

    private:
        uint8_t m_width;
        bool m_signed;
    };

    class FloatType final : public SemaType
    {
    public:
        explicit constexpr FloatType(uint8_t width) noexcept : SemaType{Kind::Float}, m_width{width} {}

        [[nodiscard]] constexpr uint8_t width() const noexcept { return m_width; }

        [[nodiscard]] std::string to_string() const override;
        [[nodiscard]] bool structural_eq(const SemaType& other) const override;
        [[nodiscard]] uint64_t structural_hash() const override;

    private:
        uint8_t m_width;
    };

    class PointerSemaType final : public SemaType
    {
    public:
        explicit constexpr PointerSemaType(SemaType* pointee, ast::Qualifier quals = ast::Qualifier::None) noexcept
            : SemaType{Kind::Pointer}, m_pointee{pointee}, m_quals{quals}
        {
        }

        [[nodiscard]] constexpr SemaType* pointee() const noexcept { return m_pointee; }
        [[nodiscard]] constexpr ast::Qualifier quals() const noexcept { return m_quals; }

        [[nodiscard]] std::string to_string() const override;
        [[nodiscard]] bool structural_eq(const SemaType& other) const override;
        [[nodiscard]] uint64_t structural_hash() const override;

    private:
        SemaType* m_pointee;
        ast::Qualifier m_quals;
    };

    class SliceSemaType final : public SemaType
    {
    public:
        explicit constexpr SliceSemaType(SemaType* element) noexcept : SemaType{Kind::Slice}, m_element{element} {}

        [[nodiscard]] constexpr SemaType* element() const noexcept { return m_element; }

        [[nodiscard]] std::string to_string() const override;
        [[nodiscard]] bool structural_eq(const SemaType& other) const override;
        [[nodiscard]] uint64_t structural_hash() const override;

    private:
        SemaType* m_element;
    };

    class ArraySemaType final : public SemaType
    {
    public:
        explicit constexpr ArraySemaType(SemaType* element, uint64_t length) noexcept : SemaType{Kind::Array}, m_element{element}, m_length{length} {}

        [[nodiscard]] constexpr SemaType* element() const noexcept { return m_element; }
        [[nodiscard]] constexpr uint64_t length() const noexcept { return m_length; }

        [[nodiscard]] std::string to_string() const override;
        [[nodiscard]] bool structural_eq(const SemaType& other) const override;
        [[nodiscard]] uint64_t structural_hash() const override;

    private:
        SemaType* m_element;
        uint64_t m_length;
    };

    class FlexibleArraySemaType final : public SemaType
    {
    public:
        explicit constexpr FlexibleArraySemaType(SemaType* element) noexcept : SemaType{Kind::FlexibleArray}, m_element{element} {}

        [[nodiscard]] constexpr SemaType* element() const noexcept { return m_element; }

        [[nodiscard]] std::string to_string() const override;
        [[nodiscard]] bool structural_eq(const SemaType& other) const override;
        [[nodiscard]] uint64_t structural_hash() const override;

    private:
        SemaType* m_element;
    };

    class FunctionSemaType final : public SemaType
    {
    public:
        explicit FunctionSemaType(SemaType* return_type, std::vector<SemaType*> param_types, bool is_variadic = false) noexcept
            : SemaType{Kind::Function}, m_return{return_type}, m_params{std::move(param_types)}, m_variadic{is_variadic}
        {
        }

        [[nodiscard]] SemaType* return_type() const noexcept { return m_return; }
        [[nodiscard]] std::span<SemaType* const> param_types() const noexcept { return m_params; }
        [[nodiscard]] bool is_variadic() const noexcept { return m_variadic; }

        [[nodiscard]] std::string to_string() const override;
        [[nodiscard]] bool structural_eq(const SemaType& other) const override;
        [[nodiscard]] uint64_t structural_hash() const override;

    private:
        SemaType* m_return;
        std::vector<SemaType*> m_params;
        bool m_variadic;
    };

    struct FieldInfo
    {
        si::InternedString name;
        SemaType* type{};
        ast::FieldDecl* decl{};
        ast::Visibility visibility{ast::Visibility::Private};
        uint32_t index{};
    };

    struct MethodInfo
    {
        si::InternedString name;
        FunctionSemaType* type{};
        ast::FunctionDecl* decl{};
        ast::Visibility visibility{ast::Visibility::Private};
    };

    struct VariantInfo
    {
        si::InternedString name;
        int64_t discriminant{};
        std::vector<SemaType*> payload_types;
        ast::EnumVariantDecl* decl{};
    };

    class StructSemaType final : public SemaType
    {
    public:
        explicit StructSemaType(si::InternedString name) noexcept : SemaType{Kind::Struct}, m_name{name} {}

        [[nodiscard]] si::InternedString name() const noexcept { return m_name; }
        [[nodiscard]] std::span<const FieldInfo> fields() const noexcept { return m_fields; }
        [[nodiscard]] std::span<const MethodInfo> methods() const noexcept { return m_methods; }
        [[nodiscard]] bool is_complete() const noexcept { return m_complete; }

        void set_fields(std::vector<FieldInfo> fields) noexcept;
        void set_methods(std::vector<MethodInfo> methods) noexcept;
        void mark_complete() noexcept { m_complete = true; }

        [[nodiscard]] const FieldInfo* find_field(si::InternedString name) const noexcept;
        [[nodiscard]] const MethodInfo* find_method(si::InternedString name) const noexcept;

        [[nodiscard]] std::string to_string() const override;
        [[nodiscard]] bool structural_eq(const SemaType& other) const override;
        [[nodiscard]] uint64_t structural_hash() const override;

    private:
        si::InternedString m_name;
        std::vector<FieldInfo> m_fields;
        std::vector<MethodInfo> m_methods;
        bool m_complete{false};
    };

    class UnionSemaType final : public SemaType
    {
    public:
        explicit UnionSemaType(si::InternedString name) noexcept : SemaType{Kind::Union}, m_name{name} {}

        [[nodiscard]] si::InternedString name() const noexcept { return m_name; }
        [[nodiscard]] std::span<const FieldInfo> fields() const noexcept { return m_fields; }
        [[nodiscard]] std::span<const MethodInfo> methods() const noexcept { return m_methods; }
        [[nodiscard]] bool is_complete() const noexcept { return m_complete; }

        void set_fields(std::vector<FieldInfo> fields) noexcept;
        void set_methods(std::vector<MethodInfo> methods) noexcept;
        void mark_complete() noexcept { m_complete = true; }

        [[nodiscard]] const FieldInfo* find_field(si::InternedString name) const noexcept;
        [[nodiscard]] const MethodInfo* find_method(si::InternedString name) const noexcept;

        [[nodiscard]] std::string to_string() const override;
        [[nodiscard]] bool structural_eq(const SemaType& other) const override;
        [[nodiscard]] uint64_t structural_hash() const override;

    private:
        si::InternedString m_name;
        std::vector<FieldInfo> m_fields;
        std::vector<MethodInfo> m_methods;
        bool m_complete{false};
    };

    class EnumSemaType final : public SemaType
    {
    public:
        explicit EnumSemaType(si::InternedString name, SemaType* underlying) noexcept : SemaType{Kind::Enum}, m_name{name}, m_underlying{underlying} {}

        [[nodiscard]] si::InternedString name() const noexcept { return m_name; }
        [[nodiscard]] SemaType* underlying_type() const noexcept { return m_underlying; }
        [[nodiscard]] std::span<const VariantInfo> variants() const noexcept { return m_variants; }
        [[nodiscard]] std::span<const MethodInfo> methods() const noexcept { return m_methods; }
        [[nodiscard]] bool is_complete() const noexcept { return m_complete; }

        void set_variants(std::vector<VariantInfo> variants) noexcept;
        void set_methods(std::vector<MethodInfo> methods) noexcept;
        void mark_complete() noexcept { m_complete = true; }

        [[nodiscard]] const VariantInfo* find_variant(si::InternedString name) const noexcept;
        [[nodiscard]] const MethodInfo* find_method(si::InternedString name) const noexcept;

        [[nodiscard]] bool has_payloads() const noexcept;

        [[nodiscard]] std::string to_string() const override;
        [[nodiscard]] bool structural_eq(const SemaType& other) const override;
        [[nodiscard]] uint64_t structural_hash() const override;

    private:
        si::InternedString m_name;
        SemaType* m_underlying;
        std::vector<VariantInfo> m_variants;
        std::vector<MethodInfo> m_methods;
        bool m_complete{false};
    };

    class AliasType final : public SemaType
    {
    public:
        explicit constexpr AliasType(si::InternedString name, SemaType* underlying) noexcept : SemaType{Kind::Alias}, m_name{name}, m_underlying{underlying} {}

        [[nodiscard]] constexpr si::InternedString name() const noexcept { return m_name; }
        [[nodiscard]] constexpr SemaType* underlying() const noexcept { return m_underlying; }
        void set_underlying(SemaType* ty) noexcept { m_underlying = ty; }

        [[nodiscard]] SemaType* canonical() const noexcept;

        [[nodiscard]] std::string to_string() const override;
        [[nodiscard]] bool structural_eq(const SemaType& other) const override;
        [[nodiscard]] uint64_t structural_hash() const override;

    private:
        si::InternedString m_name;
        SemaType* m_underlying;
    };

    class TypeVar final : public SemaType
    {
    public:
        explicit TypeVar(uint32_t id) noexcept : SemaType{Kind::TypeVar}, m_id{id} {}

        [[nodiscard]] constexpr uint32_t id() const noexcept { return m_id; }
        [[nodiscard]] SemaType* parent() const noexcept { return m_parent; }

        void set_parent(SemaType* parent) noexcept { m_parent = parent; }

        [[nodiscard]] std::string to_string() const override;
        [[nodiscard]] bool structural_eq(const SemaType& other) const override;
        [[nodiscard]] uint64_t structural_hash() const override;

    private:
        uint32_t m_id;
        SemaType* m_parent{nullptr};
    };

    [[nodiscard]] SemaType* strip_alias(SemaType* ty) noexcept;

} // namespace dcc::sema

#endif /* DCC_SEMA_TYPES_HH */
