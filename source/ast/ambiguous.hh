#ifndef DCC_AST_AMBIGUOUS_HH
#define DCC_AST_AMBIGUOUS_HH

#include <ast/node.hh>
#include <vector>

namespace dcc::ast
{
    class AmbiguousExpr final : public Expr
    {
    public:
        explicit AmbiguousExpr(sm::SourceRange range, std::vector<Expr*> alternatives) noexcept : Expr{range}, m_alternatives{std::move(alternatives)} {}

        [[nodiscard]] std::span<Expr* const> alternatives() const noexcept { return m_alternatives; }

        void accept(Visitor& v) const override;

    private:
        std::vector<Expr*> m_alternatives;
    };

    class AmbiguousStmt final : public Stmt
    {
    public:
        explicit AmbiguousStmt(sm::SourceRange range, std::vector<Stmt*> alternatives) noexcept : Stmt{range}, m_alternatives{std::move(alternatives)} {}

        [[nodiscard]] std::span<Stmt* const> alternatives() const noexcept { return m_alternatives; }

        void accept(Visitor& v) const override;

    private:
        std::vector<Stmt*> m_alternatives;
    };

    class AmbiguousDecl final : public Decl
    {
    public:
        explicit AmbiguousDecl(sm::SourceRange range, std::vector<Decl*> alternatives) noexcept : Decl{range}, m_alternatives{std::move(alternatives)} {}

        [[nodiscard]] std::span<Decl* const> alternatives() const noexcept { return m_alternatives; }

        void accept(Visitor& v) const override;

    private:
        std::vector<Decl*> m_alternatives;
    };

} // namespace dcc::ast

#endif /* DCC_AST_AMBIGUOUS_HH */
