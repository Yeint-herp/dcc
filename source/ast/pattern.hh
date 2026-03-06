#ifndef DCC_AST_PATTERN_HH
#define DCC_AST_PATTERN_HH

#include <ast/common.hh>
#include <ast/node.hh>
#include <span>

namespace dcc::ast
{
    class Expr;
    class TypeExpr;

    class LiteralPattern final : public Pattern
    {
    public:
        explicit constexpr LiteralPattern(sm::SourceRange range, Expr* literal) noexcept : Pattern{range}, m_literal{literal} {}

        [[nodiscard]] constexpr Expr* literal() const noexcept { return m_literal; }

        void accept(Visitor& v) const override;

    private:
        Expr* m_literal;
    };

    class BindingPattern final : public Pattern
    {
    public:
        explicit constexpr BindingPattern(sm::SourceRange range, si::InternedString name, Expr* guard = nullptr) noexcept
            : Pattern{range}, m_name{name}, m_guard{guard}
        {
        }

        [[nodiscard]] constexpr si::InternedString name() const noexcept { return m_name; }
        [[nodiscard]] constexpr Expr* guard() const noexcept { return m_guard; }

        void accept(Visitor& v) const override;

    private:
        si::InternedString m_name;
        Expr* m_guard;
    };

    class WildcardPattern final : public Pattern
    {
    public:
        explicit constexpr WildcardPattern(sm::SourceRange range) noexcept : Pattern{range} {}

        void accept(Visitor& v) const override;
    };

    class EnumPattern final : public Pattern
    {
    public:
        explicit constexpr EnumPattern(sm::SourceRange range, std::span<const si::InternedString> path, std::span<Pattern* const> sub_patterns) noexcept
            : Pattern{range}, m_path{path}, m_sub_patterns{sub_patterns}
        {
        }

        [[nodiscard]] constexpr std::span<const si::InternedString> path() const noexcept { return m_path; }
        [[nodiscard]] constexpr std::span<Pattern* const> sub_patterns() const noexcept { return m_sub_patterns; }

        void accept(Visitor& v) const override;

    private:
        std::span<const si::InternedString> m_path;
        std::span<Pattern* const> m_sub_patterns;
    };

    struct StructPatternField
    {
        si::InternedString name;
        Pattern* binding;
        sm::SourceRange range;
    };

    class StructPattern final : public Pattern
    {
    public:
        explicit constexpr StructPattern(sm::SourceRange range, si::InternedString type_name, std::span<const StructPatternField> fields,
                                         bool has_rest) noexcept
            : Pattern{range}, m_type_name{type_name}, m_fields{fields}, m_has_rest{has_rest}
        {
        }

        [[nodiscard]] constexpr si::InternedString type_name() const noexcept { return m_type_name; }
        [[nodiscard]] constexpr std::span<const StructPatternField> fields() const noexcept { return m_fields; }
        [[nodiscard]] constexpr bool has_rest() const noexcept { return m_has_rest; }

        void accept(Visitor& v) const override;

    private:
        si::InternedString m_type_name;
        std::span<const StructPatternField> m_fields;
        bool m_has_rest;
    };

    class RestPattern final : public Pattern
    {
    public:
        explicit constexpr RestPattern(sm::SourceRange range) noexcept : Pattern{range} {}

        void accept(Visitor& v) const override;
    };

    struct MatchArm
    {
        Pattern* pattern;
        Expr* guard;
        Node* body;
        sm::SourceRange range;
    };

} // namespace dcc::ast

#endif /* DCC_AST_PATTERN_HH */
