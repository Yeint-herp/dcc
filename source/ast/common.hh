#ifndef DCC_AST_COMMON_HH
#define DCC_AST_COMMON_HH

#include <cstdint>
#include <lex/token.hh>
#include <span>
#include <string_view>
#include <util/si.hh>
#include <util/source_manager.hh>
#include <variant>

namespace dcc::ast
{
    class TypeExpr;
    class Expr;

    enum class PrimitiveKind : uint8_t
    {
        I8,
        U8,
        I16,
        U16,
        I32,
        U32,
        I64,
        U64,
        F32,
        F64,
        Bool,
        Void,
        NullT,
    };

    enum class Qualifier : uint8_t
    {
        None = 0,
        Const = 1u << 0,
        Restrict = 1u << 1,
        Volatile = 1u << 2,
    };

    [[nodiscard]] constexpr Qualifier operator|(Qualifier a, Qualifier b) noexcept
    {
        return static_cast<Qualifier>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
    }

    [[nodiscard]] constexpr Qualifier operator&(Qualifier a, Qualifier b) noexcept
    {
        return static_cast<Qualifier>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
    }

    [[nodiscard]] constexpr bool has_qualifier(Qualifier set, Qualifier q) noexcept
    {
        return (set & q) != Qualifier::None;
    }

    enum class Visibility : uint8_t
    {
        Private,
        Public
    };

    enum class BinaryOp : uint8_t
    {
        Add,
        Sub,
        Mul,
        Div,
        Mod,
        BitAnd,
        BitOr,
        BitXor,
        Shl,
        Shr,
        Eq,
        Ne,
        Lt,
        Le,
        Gt,
        Ge,
        LogAnd,
        LogOr,
    };

    enum class UnaryOp : uint8_t
    {
        Negate,
        BitNot,
        LogNot,
        Deref,
        AddressOf,
        PreInc,
        PreDec,
        PostInc,
        PostDec,
    };

    enum class AssignOp : uint8_t
    {
        Simple,
        Add,
        Sub,
        Mul,
        Div,
        Mod,
        BitAnd,
        BitOr,
        BitXor,
        Shl,
        Shr,
    };

    struct TemplateArg
    {
        std::variant<TypeExpr*, Expr*> arg;
        sm::SourceRange range;
    };

    struct FieldInit
    {
        si::InternedString name;
        Expr* value;
        sm::SourceRange range;
    };

    struct Attribute
    {
        std::span<const lex::Token> raw_tokens;
        std::span<std::string_view> entries;
        sm::SourceRange range;
    };

} // namespace dcc::ast

#endif /* DCC_AST_COMMON_HH */
