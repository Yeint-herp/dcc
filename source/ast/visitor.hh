#ifndef DCC_AST_VISITOR_HH
#define DCC_AST_VISITOR_HH

namespace dcc::ast
{
    class BuiltinType;
    class NamedType;
    class QualifiedType;
    class PointerType;
    class SliceType;
    class ArrayType;
    class FlexibleArrayType;
    class FunctionType;
    class TemplateType;
    class TypeofType;

    class IntegerLiteral;
    class FloatLiteral;
    class StringLiteral;
    class CharLiteral;
    class BoolLiteral;
    class NullLiteral;
    class IdentifierExpr;
    class GroupingExpr;
    class BinaryExpr;
    class UnaryExpr;
    class AssignExpr;
    class ConditionalExpr;
    class CastExpr;
    class MemberAccessExpr;
    class CallExpr;
    class IndexExpr;
    class SliceExpr;
    class InitializerExpr;
    class BlockExpr;
    class MatchExpr;
    class SizeofExpr;
    class AlignofExpr;
    class MacroCallExpr;

    class ExprStmt;
    class DeclStmt;
    class BlockStmt;
    class ReturnStmt;
    class IfStmt;
    class WhileStmt;
    class ForStmt;
    class DoWhileStmt;
    class BreakStmt;
    class ContinueStmt;
    class DeferStmt;
    class MatchStmt;
    class EmptyStmt;

    class TemplateTypeParamDecl;
    class TemplateValueParamDecl;
    class VarDecl;
    class ParamDecl;
    class FieldDecl;
    class FunctionDecl;
    class StructDecl;
    class UnionDecl;
    class EnumVariantDecl;
    class EnumDecl;
    class ModuleDecl;
    class ImportDecl;
    class UsingDecl;
    class TranslationUnit;

    class LiteralPattern;
    class BindingPattern;
    class WildcardPattern;
    class EnumPattern;
    class StructPattern;
    class RestPattern;

    class AmbiguousExpr;
    class AmbiguousStmt;
    class AmbiguousDecl;

    class Visitor
    {
    public:
        virtual ~Visitor() = default;

        virtual void visit(const BuiltinType&) = 0;
        virtual void visit(const NamedType&) = 0;
        virtual void visit(const QualifiedType&) = 0;
        virtual void visit(const PointerType&) = 0;
        virtual void visit(const SliceType&) = 0;
        virtual void visit(const ArrayType&) = 0;
        virtual void visit(const FlexibleArrayType&) = 0;
        virtual void visit(const FunctionType&) = 0;
        virtual void visit(const TemplateType&) = 0;
        virtual void visit(const TypeofType&) = 0;

        virtual void visit(const IntegerLiteral&) = 0;
        virtual void visit(const FloatLiteral&) = 0;
        virtual void visit(const StringLiteral&) = 0;
        virtual void visit(const CharLiteral&) = 0;
        virtual void visit(const BoolLiteral&) = 0;
        virtual void visit(const NullLiteral&) = 0;
        virtual void visit(const IdentifierExpr&) = 0;
        virtual void visit(const GroupingExpr&) = 0;
        virtual void visit(const BinaryExpr&) = 0;
        virtual void visit(const UnaryExpr&) = 0;
        virtual void visit(const AssignExpr&) = 0;
        virtual void visit(const ConditionalExpr&) = 0;
        virtual void visit(const CastExpr&) = 0;
        virtual void visit(const MemberAccessExpr&) = 0;
        virtual void visit(const CallExpr&) = 0;
        virtual void visit(const IndexExpr&) = 0;
        virtual void visit(const SliceExpr&) = 0;
        virtual void visit(const InitializerExpr&) = 0;
        virtual void visit(const BlockExpr&) = 0;
        virtual void visit(const MatchExpr&) = 0;
        virtual void visit(const SizeofExpr&) = 0;
        virtual void visit(const AlignofExpr&) = 0;
        virtual void visit(const MacroCallExpr&) = 0;

        virtual void visit(const ExprStmt&) = 0;
        virtual void visit(const DeclStmt&) = 0;
        virtual void visit(const BlockStmt&) = 0;
        virtual void visit(const ReturnStmt&) = 0;
        virtual void visit(const IfStmt&) = 0;
        virtual void visit(const WhileStmt&) = 0;
        virtual void visit(const ForStmt&) = 0;
        virtual void visit(const DoWhileStmt&) = 0;
        virtual void visit(const BreakStmt&) = 0;
        virtual void visit(const ContinueStmt&) = 0;
        virtual void visit(const DeferStmt&) = 0;
        virtual void visit(const MatchStmt&) = 0;
        virtual void visit(const EmptyStmt&) = 0;

        virtual void visit(const TemplateTypeParamDecl&) = 0;
        virtual void visit(const TemplateValueParamDecl&) = 0;
        virtual void visit(const VarDecl&) = 0;
        virtual void visit(const ParamDecl&) = 0;
        virtual void visit(const FieldDecl&) = 0;
        virtual void visit(const FunctionDecl&) = 0;
        virtual void visit(const StructDecl&) = 0;
        virtual void visit(const UnionDecl&) = 0;
        virtual void visit(const EnumVariantDecl&) = 0;
        virtual void visit(const EnumDecl&) = 0;
        virtual void visit(const ModuleDecl&) = 0;
        virtual void visit(const ImportDecl&) = 0;
        virtual void visit(const UsingDecl&) = 0;
        virtual void visit(const TranslationUnit&) = 0;

        virtual void visit(const LiteralPattern&) = 0;
        virtual void visit(const BindingPattern&) = 0;
        virtual void visit(const WildcardPattern&) = 0;
        virtual void visit(const EnumPattern&) = 0;
        virtual void visit(const StructPattern&) = 0;
        virtual void visit(const RestPattern&) = 0;

        virtual void visit(const AmbiguousExpr&) = 0;
        virtual void visit(const AmbiguousStmt&) = 0;
        virtual void visit(const AmbiguousDecl&) = 0;
    };

} // namespace dcc::ast

#endif /* DCC_AST_VISITOR_HH */
