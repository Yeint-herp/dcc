#ifndef DCC_IR_LOWERING_HH
#define DCC_IR_LOWERING_HH

#include <ast/pattern.hh>
#include <ast/visitor.hh>
#include <diagnostics.hh>
#include <ir/ir.hh>
#include <ir/monomorphize.hh>
#include <sema/name_resolver.hh>
#include <sema/sema.hh>
#include <sema/type_checker.hh>
#include <sema/type_context.hh>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace dcc::ir
{
    struct LowerScope
    {
        std::unordered_map<const ast::Decl*, ValueId> locals;
        std::unordered_map<const ast::Pattern*, ValueId> pattern_bindings;
        std::vector<const ast::Stmt*> defers;
        LowerScope* parent{nullptr};
    };

    struct ExprResult
    {
        ValueId value{kNoValue};
        TypeRef type{nullptr};
        bool is_lvalue{false};

        static ExprResult rvalue(ValueId v, TypeRef t) { return {v, t, false}; }
        static ExprResult lvalue(ValueId ptr, TypeRef pointee) { return {ptr, pointee, true}; }
    };

    class IRLowering final : public ast::Visitor
    {
    public:
        explicit IRLowering(sema::Sema& sema, diag::DiagnosticPrinter& printer);

        [[nodiscard]] std::unique_ptr<Module> lower(ast::TranslationUnit& tu);

    private:
        sema::Sema& m_sema;
        sema::TypeContext& m_types;
        diag::DiagnosticPrinter& m_printer;

        std::unique_ptr<Module> m_module;
        Function* m_current_func{nullptr};
        IRBuilder* m_builder{nullptr};
        std::unique_ptr<IRBuilder> m_builder_storage;

        std::vector<si::InternedString> m_module_path;

        LowerScope* m_scope{nullptr};
        std::vector<std::unique_ptr<LowerScope>> m_scope_pool;

        std::unordered_map<const sema::SemaType*, sema::SemaType*> m_mono_subst;
        std::unordered_map<const sema::SemaType*, TypeRef> m_type_cache;

        Monomorphizer m_mono;

        ExprResult m_expr_result;

        struct LoopTargets
        {
            BasicBlock* brk;
            BasicBlock* cont;
        };
        std::vector<LoopTargets> m_loop_stack;

        BasicBlock* m_return_block{nullptr};
        ValueId m_return_slot{kNoValue};

        TypeRef lower_type(const sema::SemaType* sty);
        TypeRef lower_primitive(ast::PrimitiveKind pk);
        TypeRef lower_function_type(const sema::FunctionSemaType* fty);
        StructType* lower_struct_type(const sema::StructSemaType* sty);
        StructType* lower_union_type(const sema::UnionSemaType* uty);
        StructType* lower_enum_type(const sema::EnumSemaType* ety);

        ExprResult lower_expr(const ast::Expr& expr);
        ValueId lower_expr_rvalue(const ast::Expr& expr);
        ValueId load_if_lvalue(ExprResult r);

        ExprResult lower_member_access(const ast::MemberAccessExpr& ma);
        ExprResult lower_call(const ast::CallExpr& call);
        ExprResult lower_ufcs_call(const ast::CallExpr& call, const ast::MemberAccessExpr& ma, sema::Symbol* target);
        ExprResult lower_index(const ast::IndexExpr& idx);
        ExprResult lower_match_expr(const ast::MatchExpr& match);
        ExprResult lower_initializer(const ast::InitializerExpr& init);
        ExprResult lower_block_expr(const ast::BlockExpr& blk);
        ExprResult lower_slice(const ast::SliceExpr& sl);
        ExprResult lower_cast(const ast::CastExpr& cast);

        void lower_binary_op(const ast::BinaryExpr& bin);
        void lower_unary_op(const ast::UnaryExpr& un);
        void lower_assign(const ast::AssignExpr& assign);
        void lower_conditional(const ast::ConditionalExpr& cond);
        void lower_short_circuit(ast::BinaryOp op, const ast::Expr& lhs, const ast::Expr& rhs, sm::SourceRange range);

        ValueId emit_binop(Opcode opc, ValueId lhs, ValueId rhs, TypeRef type);
        ValueId emit_cast(ValueId val, const sema::SemaType* from, const sema::SemaType* to);

        ValueId deref_to_struct_ptr(ValueId val, TypeRef ty, const sema::SemaType* sema_ty);

        void lower_match_arms(ValueId scrutinee_val, TypeRef scrutinee_ir_ty, const sema::SemaType* scrutinee_sema_ty, std::span<const ast::MatchArm> arms,
                              BasicBlock* merge_bb, ValueId result_slot);

        void lower_pattern_test(const ast::Pattern& pat, ValueId scrutinee, TypeRef scr_ty, const sema::SemaType* sema_ty, BasicBlock* match_bb,
                                BasicBlock* fail_bb);

        void bind_pattern(const ast::Pattern& pat, ValueId scrutinee, TypeRef scr_ty, const sema::SemaType* sema_ty);

        void lower_stmt(const ast::Stmt& stmt);
        void lower_decl(const ast::Decl& decl);
        void lower_function(const ast::FunctionDecl& fn, std::string mangled_name);
        void lower_var(const ast::VarDecl& var);
        void lower_global_var(const ast::VarDecl& var);
        void lower_block(const ast::BlockStmt& blk);
        void lower_match_stmt(const ast::MatchStmt& ms);

        bool evaluate_static_condition(const ast::Expr& cond);

        void emit_defers_to(LowerScope* stop_at = nullptr);
        void emit_scope_defers(LowerScope& scope);

        LowerScope* push_scope();
        void pop_scope();
        void declare_local(const ast::Decl* decl, ValueId slot);
        ValueId lookup_local(const ast::Decl* decl) const;
        void declare_pattern_local(const ast::Pattern* pat, ValueId slot);
        ValueId lookup_pattern_local(const ast::Pattern* pat) const;

        const sema::SemaType* sema_type_of(const ast::Node* n) const;
        sema::Symbol* symbol_of(const ast::Node* n) const;
        std::string mangle(const ast::FunctionDecl& fn);
        void error(sm::SourceRange range, std::string msg);

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
    };

} // namespace dcc::ir

#endif /* DCC_IR_LOWERING_HH */
