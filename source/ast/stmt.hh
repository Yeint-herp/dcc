#ifndef DCC_AST_STMT_HH
#define DCC_AST_STMT_HH

#include <ast/node.hh>
#include <span>

namespace dcc::ast
{
    class Expr;
    class Decl;
    struct MatchArm;

    class ExprStmt final : public Stmt
    {
    public:
        explicit constexpr ExprStmt(sm::SourceRange range, Expr* expr) noexcept : Stmt{range}, m_expr{expr} {}

        [[nodiscard]] constexpr Expr* expr() const noexcept { return m_expr; }

        void accept(Visitor& v) const override;

    private:
        Expr* m_expr;
    };

    class DeclStmt final : public Stmt
    {
    public:
        explicit constexpr DeclStmt(sm::SourceRange range, Decl* decl) noexcept : Stmt{range}, m_decl{decl} {}

        [[nodiscard]] constexpr Decl* decl() const noexcept { return m_decl; }

        void accept(Visitor& v) const override;

    private:
        Decl* m_decl;
    };

    class BlockStmt final : public Stmt
    {
    public:
        explicit constexpr BlockStmt(sm::SourceRange range, std::span<Stmt* const> stmts) noexcept : Stmt{range}, m_stmts{stmts} {}

        [[nodiscard]] constexpr std::span<Stmt* const> stmts() const noexcept { return m_stmts; }

        void accept(Visitor& v) const override;

    private:
        std::span<Stmt* const> m_stmts;
    };

    class ReturnStmt final : public Stmt
    {
    public:
        explicit constexpr ReturnStmt(sm::SourceRange range, Expr* value = nullptr) noexcept : Stmt{range}, m_value{value} {}

        [[nodiscard]] constexpr Expr* value() const noexcept { return m_value; }

        void accept(Visitor& v) const override;

    private:
        Expr* m_value;
    };

    class IfStmt final : public Stmt
    {
    public:
        explicit constexpr IfStmt(sm::SourceRange range, Expr* condition, Stmt* then_branch, Stmt* else_branch = nullptr, bool is_static = false) noexcept
            : Stmt{range}, m_cond{condition}, m_then{then_branch}, m_else{else_branch}, m_is_static{is_static}
        {
        }

        [[nodiscard]] constexpr Expr* condition() const noexcept { return m_cond; }
        [[nodiscard]] constexpr Stmt* then_branch() const noexcept { return m_then; }
        [[nodiscard]] constexpr Stmt* else_branch() const noexcept { return m_else; }
        [[nodiscard]] constexpr bool is_static() const noexcept { return m_is_static; }

        void accept(Visitor& v) const override;

    private:
        Expr* m_cond;
        Stmt* m_then;
        Stmt* m_else;
        bool m_is_static;
    };

    class WhileStmt final : public Stmt
    {
    public:
        explicit constexpr WhileStmt(sm::SourceRange range, Expr* condition, Stmt* body) noexcept : Stmt{range}, m_cond{condition}, m_body{body} {}

        [[nodiscard]] constexpr Expr* condition() const noexcept { return m_cond; }
        [[nodiscard]] constexpr Stmt* body() const noexcept { return m_body; }

        void accept(Visitor& v) const override;

    private:
        Expr* m_cond;
        Stmt* m_body;
    };

    class ForStmt final : public Stmt
    {
    public:
        explicit constexpr ForStmt(sm::SourceRange range, Stmt* init, Expr* condition, Expr* increment, Stmt* body) noexcept
            : Stmt{range}, m_init{init}, m_cond{condition}, m_incr{increment}, m_body{body}
        {
        }

        [[nodiscard]] constexpr Stmt* init() const noexcept { return m_init; }
        [[nodiscard]] constexpr Expr* condition() const noexcept { return m_cond; }
        [[nodiscard]] constexpr Expr* increment() const noexcept { return m_incr; }
        [[nodiscard]] constexpr Stmt* body() const noexcept { return m_body; }

        void accept(Visitor& v) const override;

    private:
        Stmt* m_init;
        Expr* m_cond;
        Expr* m_incr;
        Stmt* m_body;
    };

    class DoWhileStmt final : public Stmt
    {
    public:
        explicit constexpr DoWhileStmt(sm::SourceRange range, Stmt* body, Expr* condition) noexcept : Stmt{range}, m_body{body}, m_cond{condition} {}

        [[nodiscard]] constexpr Stmt* body() const noexcept { return m_body; }
        [[nodiscard]] constexpr Expr* condition() const noexcept { return m_cond; }

        void accept(Visitor& v) const override;

    private:
        Stmt* m_body;
        Expr* m_cond;
    };

    class BreakStmt final : public Stmt
    {
    public:
        explicit constexpr BreakStmt(sm::SourceRange range) noexcept : Stmt{range} {}

        void accept(Visitor& v) const override;
    };

    class ContinueStmt final : public Stmt
    {
    public:
        explicit constexpr ContinueStmt(sm::SourceRange range) noexcept : Stmt{range} {}

        void accept(Visitor& v) const override;
    };

    class DeferStmt final : public Stmt
    {
    public:
        explicit constexpr DeferStmt(sm::SourceRange range, Stmt* body) noexcept : Stmt{range}, m_body{body} {}

        [[nodiscard]] constexpr Stmt* body() const noexcept { return m_body; }

        void accept(Visitor& v) const override;

    private:
        Stmt* m_body;
    };

    class MatchStmt final : public Stmt
    {
    public:
        explicit constexpr MatchStmt(sm::SourceRange range, Expr* scrutinee, std::span<const MatchArm> arms) noexcept
            : Stmt{range}, m_scrutinee{scrutinee}, m_arms{arms}
        {
        }

        [[nodiscard]] constexpr Expr* scrutinee() const noexcept { return m_scrutinee; }
        [[nodiscard]] constexpr std::span<const MatchArm> arms() const noexcept { return m_arms; }

        void accept(Visitor& v) const override;

    private:
        Expr* m_scrutinee;
        std::span<const MatchArm> m_arms;
    };

    class EmptyStmt final : public Stmt
    {
    public:
        explicit constexpr EmptyStmt(sm::SourceRange range) noexcept : Stmt{range} {}

        void accept(Visitor& v) const override;
    };

} // namespace dcc::ast

#endif /* DCC_AST_STMT_HH */
