#ifndef DCC_AST_TYPE_HH
#define DCC_AST_TYPE_HH

#include <ast/common.hh>
#include <ast/node.hh>
#include <span>

namespace dcc::ast
{
    class BuiltinType final : public TypeExpr
    {
    public:
        explicit constexpr BuiltinType(sm::SourceRange range, PrimitiveKind kind) noexcept : TypeExpr{range}, m_kind{kind} {}

        [[nodiscard]] constexpr PrimitiveKind kind() const noexcept { return m_kind; }

        void accept(Visitor& v) const override;

    private:
        PrimitiveKind m_kind;
    };

    class NamedType final : public TypeExpr
    {
    public:
        explicit constexpr NamedType(sm::SourceRange range, si::InternedString name) noexcept : TypeExpr{range}, m_name{name} {}

        [[nodiscard]] constexpr si::InternedString name() const noexcept { return m_name; }

        void accept(Visitor& v) const override;

    private:
        si::InternedString m_name;
    };

    class QualifiedType final : public TypeExpr
    {
    public:
        explicit constexpr QualifiedType(sm::SourceRange range, Qualifier quals, TypeExpr* inner) noexcept : TypeExpr{range}, m_quals{quals}, m_inner{inner} {}

        [[nodiscard]] constexpr Qualifier quals() const noexcept { return m_quals; }
        [[nodiscard]] constexpr TypeExpr* inner() const noexcept { return m_inner; }

        void accept(Visitor& v) const override;

    private:
        Qualifier m_quals;
        TypeExpr* m_inner;
    };

    class PointerType final : public TypeExpr
    {
    public:
        explicit constexpr PointerType(sm::SourceRange range, TypeExpr* pointee) noexcept : TypeExpr{range}, m_pointee{pointee} {}

        [[nodiscard]] constexpr TypeExpr* pointee() const noexcept { return m_pointee; }

        void accept(Visitor& v) const override;

    private:
        TypeExpr* m_pointee;
    };

    class SliceType final : public TypeExpr
    {
    public:
        explicit constexpr SliceType(sm::SourceRange range, TypeExpr* element) noexcept : TypeExpr{range}, m_element{element} {}

        [[nodiscard]] constexpr TypeExpr* element() const noexcept { return m_element; }

        void accept(Visitor& v) const override;

    private:
        TypeExpr* m_element;
    };

    class ArrayType final : public TypeExpr
    {
    public:
        explicit constexpr ArrayType(sm::SourceRange range, TypeExpr* element, Expr* size) noexcept : TypeExpr{range}, m_element{element}, m_size{size} {}

        [[nodiscard]] constexpr TypeExpr* element() const noexcept { return m_element; }
        [[nodiscard]] constexpr Expr* size() const noexcept { return m_size; }

        void accept(Visitor& v) const override;

    private:
        TypeExpr* m_element;
        Expr* m_size;
    };

    class FlexibleArrayType final : public TypeExpr
    {
    public:
        explicit constexpr FlexibleArrayType(sm::SourceRange range, TypeExpr* element) noexcept : TypeExpr{range}, m_element{element} {}

        [[nodiscard]] constexpr TypeExpr* element() const noexcept { return m_element; }

        void accept(Visitor& v) const override;

    private:
        TypeExpr* m_element;
    };

    class FunctionType final : public TypeExpr
    {
    public:
        explicit constexpr FunctionType(sm::SourceRange range, TypeExpr* return_type, std::span<TypeExpr* const> param_types) noexcept
            : TypeExpr{range}, m_return_type{return_type}, m_param_types{param_types}
        {
        }

        [[nodiscard]] constexpr TypeExpr* return_type() const noexcept { return m_return_type; }
        [[nodiscard]] constexpr std::span<TypeExpr* const> param_types() const noexcept { return m_param_types; }

        void accept(Visitor& v) const override;

    private:
        TypeExpr* m_return_type;
        std::span<TypeExpr* const> m_param_types;
    };

    class TemplateType final : public TypeExpr
    {
    public:
        explicit constexpr TemplateType(sm::SourceRange range, TypeExpr* base, std::span<const TemplateArg> args) noexcept
            : TypeExpr{range}, m_base{base}, m_args{args}
        {
        }

        [[nodiscard]] constexpr TypeExpr* base() const noexcept { return m_base; }
        [[nodiscard]] constexpr std::span<const TemplateArg> args() const noexcept { return m_args; }

        void accept(Visitor& v) const override;

    private:
        TypeExpr* m_base;
        std::span<const TemplateArg> m_args;
    };

    class TypeofType final : public TypeExpr
    {
    public:
        explicit constexpr TypeofType(sm::SourceRange range, Expr* inner) noexcept : TypeExpr{range}, m_inner{inner} {}

        [[nodiscard]] constexpr Expr* inner() const noexcept { return m_inner; }

        void accept(Visitor& v) const override;

    private:
        Expr* m_inner;
    };

} // namespace dcc::ast

#endif /* DCC_AST_TYPE_HH */
