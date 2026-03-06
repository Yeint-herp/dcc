#ifndef DCC_AST_EXPR_HH
#define DCC_AST_EXPR_HH

#include <ast/common.hh>
#include <ast/node.hh>
#include <lex/token.hh>
#include <span>

namespace dcc::ast
{
    class TypeExpr;
    class Pattern;
    class Stmt;
    struct MatchArm;

    class IntegerLiteral final : public Expr
    {
    public:
        explicit constexpr IntegerLiteral(sm::SourceRange range, lex::TokenValue value) noexcept : Expr{range}, m_value{value} {}

        [[nodiscard]] constexpr lex::TokenValue value() const noexcept { return m_value; }

        void accept(Visitor& v) const override;

    private:
        lex::TokenValue m_value;
    };

    class FloatLiteral final : public Expr
    {
    public:
        explicit constexpr FloatLiteral(sm::SourceRange range, lex::TokenValue value) noexcept : Expr{range}, m_value{value} {}

        [[nodiscard]] constexpr lex::TokenValue value() const noexcept { return m_value; }

        void accept(Visitor& v) const override;

    private:
        lex::TokenValue m_value;
    };

    class StringLiteral final : public Expr
    {
    public:
        explicit constexpr StringLiteral(sm::SourceRange range, si::InternedString value) noexcept : Expr{range}, m_value{value} {}

        [[nodiscard]] constexpr si::InternedString value() const noexcept { return m_value; }

        void accept(Visitor& v) const override;

    private:
        si::InternedString m_value;
    };

    class CharLiteral final : public Expr
    {
    public:
        explicit constexpr CharLiteral(sm::SourceRange range, lex::TokenValue value) noexcept : Expr{range}, m_value{value} {}

        [[nodiscard]] constexpr lex::TokenValue value() const noexcept { return m_value; }

        void accept(Visitor& v) const override;

    private:
        lex::TokenValue m_value;
    };

    class BoolLiteral final : public Expr
    {
    public:
        explicit constexpr BoolLiteral(sm::SourceRange range, bool value) noexcept : Expr{range}, m_value{value} {}

        [[nodiscard]] constexpr bool value() const noexcept { return m_value; }

        void accept(Visitor& v) const override;

    private:
        bool m_value;
    };

    class NullLiteral final : public Expr
    {
    public:
        explicit constexpr NullLiteral(sm::SourceRange range) noexcept : Expr{range} {}

        void accept(Visitor& v) const override;
    };

    class IdentifierExpr final : public Expr
    {
    public:
        explicit constexpr IdentifierExpr(sm::SourceRange range, si::InternedString name) noexcept : Expr{range}, m_name{name} {}

        [[nodiscard]] constexpr si::InternedString name() const noexcept { return m_name; }

        void accept(Visitor& v) const override;

    private:
        si::InternedString m_name;
    };

    class GroupingExpr final : public Expr
    {
    public:
        explicit constexpr GroupingExpr(sm::SourceRange range, Expr* inner) noexcept : Expr{range}, m_inner{inner} {}

        [[nodiscard]] constexpr Expr* inner() const noexcept { return m_inner; }

        void accept(Visitor& v) const override;

    private:
        Expr* m_inner;
    };

    class BinaryExpr final : public Expr
    {
    public:
        explicit constexpr BinaryExpr(sm::SourceRange range, BinaryOp op, Expr* lhs, Expr* rhs) noexcept : Expr{range}, m_op{op}, m_lhs{lhs}, m_rhs{rhs} {}

        [[nodiscard]] constexpr BinaryOp op() const noexcept { return m_op; }
        [[nodiscard]] constexpr Expr* lhs() const noexcept { return m_lhs; }
        [[nodiscard]] constexpr Expr* rhs() const noexcept { return m_rhs; }

        void accept(Visitor& v) const override;

    private:
        BinaryOp m_op;
        Expr* m_lhs;
        Expr* m_rhs;
    };

    class UnaryExpr final : public Expr
    {
    public:
        explicit constexpr UnaryExpr(sm::SourceRange range, UnaryOp op, Expr* operand) noexcept : Expr{range}, m_op{op}, m_operand{operand} {}

        [[nodiscard]] constexpr UnaryOp op() const noexcept { return m_op; }
        [[nodiscard]] constexpr Expr* operand() const noexcept { return m_operand; }

        void accept(Visitor& v) const override;

    private:
        UnaryOp m_op;
        Expr* m_operand;
    };

    class AssignExpr final : public Expr
    {
    public:
        explicit constexpr AssignExpr(sm::SourceRange range, AssignOp op, Expr* target, Expr* value) noexcept
            : Expr{range}, m_op{op}, m_target{target}, m_value{value}
        {
        }

        [[nodiscard]] constexpr AssignOp op() const noexcept { return m_op; }
        [[nodiscard]] constexpr Expr* target() const noexcept { return m_target; }
        [[nodiscard]] constexpr Expr* value() const noexcept { return m_value; }

        void accept(Visitor& v) const override;

    private:
        AssignOp m_op;
        Expr* m_target;
        Expr* m_value;
    };

    class ConditionalExpr final : public Expr
    {
    public:
        explicit constexpr ConditionalExpr(sm::SourceRange range, Expr* condition, Expr* then_expr, Expr* else_expr) noexcept
            : Expr{range}, m_cond{condition}, m_then{then_expr}, m_else{else_expr}
        {
        }

        [[nodiscard]] constexpr Expr* condition() const noexcept { return m_cond; }
        [[nodiscard]] constexpr Expr* then_expr() const noexcept { return m_then; }
        [[nodiscard]] constexpr Expr* else_expr() const noexcept { return m_else; }

        void accept(Visitor& v) const override;

    private:
        Expr* m_cond;
        Expr* m_then;
        Expr* m_else;
    };

    class CastExpr final : public Expr
    {
    public:
        explicit constexpr CastExpr(sm::SourceRange range, Expr* operand, TypeExpr* target_type) noexcept
            : Expr{range}, m_operand{operand}, m_target{target_type}
        {
        }

        [[nodiscard]] constexpr Expr* operand() const noexcept { return m_operand; }
        [[nodiscard]] constexpr TypeExpr* target_type() const noexcept { return m_target; }

        void accept(Visitor& v) const override;

    private:
        Expr* m_operand;
        TypeExpr* m_target;
    };

    class MemberAccessExpr final : public Expr
    {
    public:
        explicit constexpr MemberAccessExpr(sm::SourceRange range, Expr* object, si::InternedString member) noexcept
            : Expr{range}, m_object{object}, m_member{member}
        {
        }

        [[nodiscard]] constexpr Expr* object() const noexcept { return m_object; }
        [[nodiscard]] constexpr si::InternedString member() const noexcept { return m_member; }

        void accept(Visitor& v) const override;

    private:
        Expr* m_object;
        si::InternedString m_member;
    };

    class CallExpr final : public Expr
    {
    public:
        explicit constexpr CallExpr(sm::SourceRange range, Expr* callee, std::span<Expr* const> args, std::span<const TemplateArg> template_args = {}) noexcept
            : Expr{range}, m_callee{callee}, m_args{args}, m_template_args{template_args}
        {
        }

        [[nodiscard]] constexpr Expr* callee() const noexcept { return m_callee; }
        [[nodiscard]] constexpr std::span<Expr* const> args() const noexcept { return m_args; }
        [[nodiscard]] constexpr std::span<const TemplateArg> template_args() const noexcept { return m_template_args; }

        void accept(Visitor& v) const override;

    private:
        Expr* m_callee;
        std::span<Expr* const> m_args;
        std::span<const TemplateArg> m_template_args;
    };

    class IndexExpr final : public Expr
    {
    public:
        explicit constexpr IndexExpr(sm::SourceRange range, Expr* object, Expr* index) noexcept : Expr{range}, m_object{object}, m_index{index} {}

        [[nodiscard]] constexpr Expr* object() const noexcept { return m_object; }
        [[nodiscard]] constexpr Expr* index() const noexcept { return m_index; }

        void accept(Visitor& v) const override;

    private:
        Expr* m_object;
        Expr* m_index;
    };

    class SliceExpr final : public Expr
    {
    public:
        explicit constexpr SliceExpr(sm::SourceRange range, Expr* object, Expr* begin, Expr* end) noexcept
            : Expr{range}, m_object{object}, m_begin{begin}, m_end{end}
        {
        }

        [[nodiscard]] constexpr Expr* object() const noexcept { return m_object; }
        [[nodiscard]] constexpr Expr* begin_idx() const noexcept { return m_begin; }
        [[nodiscard]] constexpr Expr* end_idx() const noexcept { return m_end; }

        void accept(Visitor& v) const override;

    private:
        Expr* m_object;
        Expr* m_begin;
        Expr* m_end;
    };

    class InitializerExpr final : public Expr
    {
    public:
        explicit constexpr InitializerExpr(sm::SourceRange range, TypeExpr* type, std::span<const FieldInit> fields) noexcept
            : Expr{range}, m_type{type}, m_fields{fields}
        {
        }

        [[nodiscard]] constexpr TypeExpr* type() const noexcept { return m_type; }
        [[nodiscard]] constexpr std::span<const FieldInit> fields() const noexcept { return m_fields; }

        void accept(Visitor& v) const override;

    private:
        TypeExpr* m_type;
        std::span<const FieldInit> m_fields;
    };

    class BlockExpr final : public Expr
    {
    public:
        explicit constexpr BlockExpr(sm::SourceRange range, std::span<Stmt* const> stmts, Expr* tail) noexcept : Expr{range}, m_stmts{stmts}, m_tail{tail} {}

        [[nodiscard]] constexpr std::span<Stmt* const> stmts() const noexcept { return m_stmts; }
        [[nodiscard]] constexpr Expr* tail() const noexcept { return m_tail; }

        void accept(Visitor& v) const override;

    private:
        std::span<Stmt* const> m_stmts;
        Expr* m_tail;
    };

    class MatchExpr final : public Expr
    {
    public:
        explicit constexpr MatchExpr(sm::SourceRange range, Expr* scrutinee, std::span<const MatchArm> arms) noexcept
            : Expr{range}, m_scrutinee{scrutinee}, m_arms{arms}
        {
        }

        [[nodiscard]] constexpr Expr* scrutinee() const noexcept { return m_scrutinee; }
        [[nodiscard]] constexpr std::span<const MatchArm> arms() const noexcept { return m_arms; }

        void accept(Visitor& v) const override;

    private:
        Expr* m_scrutinee;
        std::span<const MatchArm> m_arms;
    };

    class SizeofExpr final : public Expr
    {
    public:
        explicit constexpr SizeofExpr(sm::SourceRange range, TypeExpr* operand) noexcept : Expr{range}, m_operand{operand} {}

        [[nodiscard]] constexpr TypeExpr* operand() const noexcept { return m_operand; }

        void accept(Visitor& v) const override;

    private:
        TypeExpr* m_operand;
    };

    class AlignofExpr final : public Expr
    {
    public:
        explicit constexpr AlignofExpr(sm::SourceRange range, TypeExpr* operand) noexcept : Expr{range}, m_operand{operand} {}

        [[nodiscard]] constexpr TypeExpr* operand() const noexcept { return m_operand; }

        void accept(Visitor& v) const override;

    private:
        TypeExpr* m_operand;
    };

    class MacroCallExpr final : public Expr
    {
    public:
        explicit constexpr MacroCallExpr(sm::SourceRange range, si::InternedString name, std::span<Expr* const> args) noexcept
            : Expr{range}, m_name{name}, m_args{args}
        {
        }

        [[nodiscard]] constexpr si::InternedString name() const noexcept { return m_name; }
        [[nodiscard]] constexpr std::span<Expr* const> args() const noexcept { return m_args; }

        void accept(Visitor& v) const override;

    private:
        si::InternedString m_name;
        std::span<Expr* const> m_args;
    };

} // namespace dcc::ast

#endif /* DCC_AST_EXPR_HH */
