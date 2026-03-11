#ifndef DCC_SEMA_TYPE_CHECKER_HH
#define DCC_SEMA_TYPE_CHECKER_HH

#include <ast/pattern.hh>
#include <ast/visitor.hh>
#include <diagnostics.hh>
#include <gtest/gtest.h>
#include <sema/name_resolver.hh>
#include <sema/scope.hh>
#include <sema/type_context.hh>

namespace dcc::sema
{
    using TypeMap = std::unordered_map<const ast::Node*, SemaType*>;
    using ConfirmedUfcsMap = std::unordered_map<const ast::CallExpr*, Symbol*>;

    class TypeChecker final : public ast::Visitor
    {
    public:
        explicit TypeChecker(TypeContext& types, const ResolutionMap& resolutions, const TypeResolutionMap& type_resolutions,
                             const DisambiguationMap& disambiguations, const UfcsMap& ufcs_candidates, diag::DiagnosticPrinter& printer);

        [[nodiscard]] bool check(ast::TranslationUnit& tu);

        [[nodiscard]] const TypeMap& type_map() const noexcept { return m_type_map; }
        [[nodiscard]] uint32_t error_count() const noexcept { return m_error_count; }

        [[nodiscard]] const ConfirmedUfcsMap& confirmed_ufcs() const noexcept { return m_confirmed_ufcs; }

        [[nodiscard]] SemaType* type_of(const ast::Node* node) const noexcept;

        void visit(const ast::BuiltinType&) override;
        void visit(const ast::NamedType&) override;
        void visit(const ast::QualifiedType&) override;
        void visit(const ast::DottedNamedType&) override;
        void visit(const ast::PointerType&) override;
        void visit(const ast::SliceType&) override;
        void visit(const ast::ArrayType&) override;
        void visit(const ast::FlexibleArrayType&) override;
        void visit(const ast::FunctionType&) override;
        void visit(const ast::TemplateType&) override;
        void visit(const ast::TypeofType&) override;

        void visit(const ast::IntegerLiteral&) override;
        void visit(const ast::FloatLiteral&) override;
        void visit(const ast::StringLiteral&) override;
        void visit(const ast::CharLiteral&) override;
        void visit(const ast::BoolLiteral&) override;
        void visit(const ast::NullLiteral&) override;
        void visit(const ast::IdentifierExpr&) override;
        void visit(const ast::GroupingExpr&) override;
        void visit(const ast::BinaryExpr&) override;
        void visit(const ast::UnaryExpr&) override;
        void visit(const ast::AssignExpr&) override;
        void visit(const ast::ConditionalExpr&) override;
        void visit(const ast::CastExpr&) override;
        void visit(const ast::MemberAccessExpr&) override;
        void visit(const ast::CallExpr&) override;
        void visit(const ast::IndexExpr&) override;
        void visit(const ast::SliceExpr&) override;
        void visit(const ast::InitializerExpr&) override;
        void visit(const ast::BlockExpr&) override;
        void visit(const ast::MatchExpr&) override;
        void visit(const ast::SizeofExpr&) override;
        void visit(const ast::AlignofExpr&) override;
        void visit(const ast::MacroCallExpr&) override;

        void visit(const ast::ExprStmt&) override;
        void visit(const ast::DeclStmt&) override;
        void visit(const ast::BlockStmt&) override;
        void visit(const ast::ReturnStmt&) override;
        void visit(const ast::IfStmt&) override;
        void visit(const ast::WhileStmt&) override;
        void visit(const ast::ForStmt&) override;
        void visit(const ast::DoWhileStmt&) override;
        void visit(const ast::BreakStmt&) override;
        void visit(const ast::ContinueStmt&) override;
        void visit(const ast::DeferStmt&) override;
        void visit(const ast::MatchStmt&) override;
        void visit(const ast::EmptyStmt&) override;

        void visit(const ast::TemplateTypeParamDecl&) override;
        void visit(const ast::TemplateValueParamDecl&) override;
        void visit(const ast::VarDecl&) override;
        void visit(const ast::ParamDecl&) override;
        void visit(const ast::FieldDecl&) override;
        void visit(const ast::FunctionDecl&) override;
        void visit(const ast::StructDecl&) override;
        void visit(const ast::UnionDecl&) override;
        void visit(const ast::EnumVariantDecl&) override;
        void visit(const ast::EnumDecl&) override;
        void visit(const ast::ModuleDecl&) override;
        void visit(const ast::ImportDecl&) override;
        void visit(const ast::UsingDecl&) override;
        void visit(const ast::TranslationUnit&) override;

        void visit(const ast::LiteralPattern&) override;
        void visit(const ast::BindingPattern&) override;
        void visit(const ast::WildcardPattern&) override;
        void visit(const ast::EnumPattern&) override;
        void visit(const ast::StructPattern&) override;
        void visit(const ast::RestPattern&) override;

        void visit(const ast::AmbiguousExpr&) override;
        void visit(const ast::AmbiguousStmt&) override;
        void visit(const ast::AmbiguousDecl&) override;

    private:
        TypeContext& m_types;
        const ResolutionMap& m_resolutions;
        const TypeResolutionMap& m_type_resolutions;
        diag::DiagnosticPrinter& m_printer;

        TypeMap m_type_map;
        const DisambiguationMap& m_disambiguations;
        const UfcsMap& m_ufcs_candidates;
        ConfirmedUfcsMap m_confirmed_ufcs;
        std::unordered_map<SemaType*, SemaType*> m_literal_defaults;
        std::unordered_map<const ast::Node*, uint64_t> m_sizeof_values;

        uint32_t m_error_count{};
        uint32_t m_loop_depth{};

        SemaType* m_current_return_type{nullptr};
        SemaType* m_match_scrutinee_type{nullptr};

        void record_type(const ast::Node* node, SemaType* type);

        SemaType* check_expr(const ast::Expr& expr);
        void check_stmt(const ast::Stmt& stmt);

        SemaType* eval_type(const ast::TypeExpr& type_expr);

        SemaType* check_binary_arithmetic(ast::BinaryOp op, SemaType* lhs, SemaType* rhs, sm::SourceRange range);
        SemaType* check_binary_comparison(ast::BinaryOp op, SemaType* lhs, SemaType* rhs, sm::SourceRange range);
        SemaType* check_binary_logical(ast::BinaryOp op, SemaType* lhs, SemaType* rhs, sm::SourceRange range);
        SemaType* check_binary_bitwise(ast::BinaryOp op, SemaType* lhs, SemaType* rhs, sm::SourceRange range);
        SemaType* check_unary(ast::UnaryOp op, SemaType* operand, sm::SourceRange range);
        SemaType* materialize_type(SemaType* ty);

        bool check_assignment_compatible(SemaType* target_type, SemaType* value_type, sm::SourceRange range);

        SemaType* check_call(const FunctionSemaType* fn_type, std::span<ast::Expr* const> args, sm::SourceRange range);
        SemaType* coerce(SemaType* from, SemaType* to, sm::SourceRange range);

        void check_pattern(const ast::Pattern& pat, SemaType* scrutinee_type);

        void check_match_exhaustiveness(const EnumSemaType* enum_type, std::span<const ast::MatchArm> arms, sm::SourceRange range);
        SemaType* try_ufcs_call(const ast::CallExpr& node, const ast::MemberAccessExpr& ma);

        [[nodiscard]] bool is_mutable_lvalue(const ast::Expr& expr) const noexcept;

        void complete_struct(StructSemaType* sty, const ast::StructDecl& decl);
        void complete_union(UnionSemaType* uty, const ast::UnionDecl& decl);
        void complete_enum(EnumSemaType* ety, const ast::EnumDecl& decl);

        void error(sm::SourceRange range, std::string message);
        void error_type_mismatch(SemaType* expected, SemaType* got, sm::SourceRange range);
        void error_not_callable(SemaType* type, sm::SourceRange range);
        void error_not_indexable(SemaType* type, sm::SourceRange range);
        void error_no_member(SemaType* type, si::InternedString member, sm::SourceRange range);
        void error_const_assign(sm::SourceRange range);
        void warning(sm::SourceRange range, std::string message);
        void note(sm::SourceRange range, std::string message);
    };

} // namespace dcc::sema

#endif /* DCC_SEMA_TYPE_CHECKER_HH */
