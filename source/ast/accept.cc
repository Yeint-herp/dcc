#include <ast/ambiguous.hh>
#include <ast/decl.hh>
#include <ast/expr.hh>
#include <ast/pattern.hh>
#include <ast/stmt.hh>
#include <ast/type.hh>
#include <ast/visitor.hh>

namespace dcc::ast
{
    void BuiltinType::accept(Visitor& v) const
    {
        v.visit(*this);
    }
    void NamedType::accept(Visitor& v) const
    {
        v.visit(*this);
    }
    void QualifiedType::accept(Visitor& v) const
    {
        v.visit(*this);
    }
    void DottedNamedType::accept(Visitor& v) const
    {
        v.visit(*this);
    }
    void PointerType::accept(Visitor& v) const
    {
        v.visit(*this);
    }
    void SliceType::accept(Visitor& v) const
    {
        v.visit(*this);
    }
    void ArrayType::accept(Visitor& v) const
    {
        v.visit(*this);
    }
    void FlexibleArrayType::accept(Visitor& v) const
    {
        v.visit(*this);
    }
    void FunctionType::accept(Visitor& v) const
    {
        v.visit(*this);
    }
    void TemplateType::accept(Visitor& v) const
    {
        v.visit(*this);
    }
    void TypeofType::accept(Visitor& v) const
    {
        v.visit(*this);
    }

    void IntegerLiteral::accept(Visitor& v) const
    {
        v.visit(*this);
    }
    void FloatLiteral::accept(Visitor& v) const
    {
        v.visit(*this);
    }
    void StringLiteral::accept(Visitor& v) const
    {
        v.visit(*this);
    }
    void CharLiteral::accept(Visitor& v) const
    {
        v.visit(*this);
    }
    void BoolLiteral::accept(Visitor& v) const
    {
        v.visit(*this);
    }
    void NullLiteral::accept(Visitor& v) const
    {
        v.visit(*this);
    }
    void IdentifierExpr::accept(Visitor& v) const
    {
        v.visit(*this);
    }
    void GroupingExpr::accept(Visitor& v) const
    {
        v.visit(*this);
    }
    void BinaryExpr::accept(Visitor& v) const
    {
        v.visit(*this);
    }
    void UnaryExpr::accept(Visitor& v) const
    {
        v.visit(*this);
    }
    void AssignExpr::accept(Visitor& v) const
    {
        v.visit(*this);
    }
    void ConditionalExpr::accept(Visitor& v) const
    {
        v.visit(*this);
    }
    void CastExpr::accept(Visitor& v) const
    {
        v.visit(*this);
    }
    void MemberAccessExpr::accept(Visitor& v) const
    {
        v.visit(*this);
    }
    void CallExpr::accept(Visitor& v) const
    {
        v.visit(*this);
    }
    void IndexExpr::accept(Visitor& v) const
    {
        v.visit(*this);
    }
    void SliceExpr::accept(Visitor& v) const
    {
        v.visit(*this);
    }
    void InitializerExpr::accept(Visitor& v) const
    {
        v.visit(*this);
    }
    void BlockExpr::accept(Visitor& v) const
    {
        v.visit(*this);
    }
    void MatchExpr::accept(Visitor& v) const
    {
        v.visit(*this);
    }
    void SizeofExpr::accept(Visitor& v) const
    {
        v.visit(*this);
    }
    void AlignofExpr::accept(Visitor& v) const
    {
        v.visit(*this);
    }
    void MacroCallExpr::accept(Visitor& v) const
    {
        v.visit(*this);
    }

    void ExprStmt::accept(Visitor& v) const
    {
        v.visit(*this);
    }
    void DeclStmt::accept(Visitor& v) const
    {
        v.visit(*this);
    }
    void BlockStmt::accept(Visitor& v) const
    {
        v.visit(*this);
    }
    void ReturnStmt::accept(Visitor& v) const
    {
        v.visit(*this);
    }
    void IfStmt::accept(Visitor& v) const
    {
        v.visit(*this);
    }
    void WhileStmt::accept(Visitor& v) const
    {
        v.visit(*this);
    }
    void ForStmt::accept(Visitor& v) const
    {
        v.visit(*this);
    }
    void DoWhileStmt::accept(Visitor& v) const
    {
        v.visit(*this);
    }
    void BreakStmt::accept(Visitor& v) const
    {
        v.visit(*this);
    }
    void ContinueStmt::accept(Visitor& v) const
    {
        v.visit(*this);
    }
    void DeferStmt::accept(Visitor& v) const
    {
        v.visit(*this);
    }
    void MatchStmt::accept(Visitor& v) const
    {
        v.visit(*this);
    }
    void EmptyStmt::accept(Visitor& v) const
    {
        v.visit(*this);
    }

    void TemplateTypeParamDecl::accept(Visitor& v) const
    {
        v.visit(*this);
    }
    void TemplateValueParamDecl::accept(Visitor& v) const
    {
        v.visit(*this);
    }
    void VarDecl::accept(Visitor& v) const
    {
        v.visit(*this);
    }
    void ParamDecl::accept(Visitor& v) const
    {
        v.visit(*this);
    }
    void FieldDecl::accept(Visitor& v) const
    {
        v.visit(*this);
    }
    void FunctionDecl::accept(Visitor& v) const
    {
        v.visit(*this);
    }
    void StructDecl::accept(Visitor& v) const
    {
        v.visit(*this);
    }
    void UnionDecl::accept(Visitor& v) const
    {
        v.visit(*this);
    }
    void EnumVariantDecl::accept(Visitor& v) const
    {
        v.visit(*this);
    }
    void EnumDecl::accept(Visitor& v) const
    {
        v.visit(*this);
    }
    void ModuleDecl::accept(Visitor& v) const
    {
        v.visit(*this);
    }
    void ImportDecl::accept(Visitor& v) const
    {
        v.visit(*this);
    }
    void UsingDecl::accept(Visitor& v) const
    {
        v.visit(*this);
    }
    void TranslationUnit::accept(Visitor& v) const
    {
        v.visit(*this);
    }

    void LiteralPattern::accept(Visitor& v) const
    {
        v.visit(*this);
    }
    void BindingPattern::accept(Visitor& v) const
    {
        v.visit(*this);
    }
    void WildcardPattern::accept(Visitor& v) const
    {
        v.visit(*this);
    }
    void EnumPattern::accept(Visitor& v) const
    {
        v.visit(*this);
    }
    void StructPattern::accept(Visitor& v) const
    {
        v.visit(*this);
    }
    void RestPattern::accept(Visitor& v) const
    {
        v.visit(*this);
    }

    void AmbiguousExpr::accept(Visitor& v) const
    {
        v.visit(*this);
    }
    void AmbiguousStmt::accept(Visitor& v) const
    {
        v.visit(*this);
    }
    void AmbiguousDecl::accept(Visitor& v) const
    {
        v.visit(*this);
    }

} // namespace dcc::ast
