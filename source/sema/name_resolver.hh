#ifndef DCC_SEMA_NAME_RESOLVER_HH
#define DCC_SEMA_NAME_RESOLVER_HH

#include <ast/visitor.hh>
#include <diagnostics.hh>
#include <sema/module_loader.hh>
#include <sema/scope.hh>
#include <sema/type_context.hh>
#include <unordered_map>

namespace dcc::sema
{
    using ResolutionMap = std::unordered_map<const ast::Node*, Symbol*>;
    using TypeResolutionMap = std::unordered_map<const ast::TypeExpr*, SemaType*>;
    using DisambiguationMap = std::unordered_map<const ast::Node*, ast::Node*>;

    class NameResolver final : public ast::Visitor
    {
    public:
        explicit NameResolver(TypeContext& types, ModuleLoader& modules, diag::DiagnosticPrinter& printer);

        [[nodiscard]] bool resolve(ast::TranslationUnit& tu);

        [[nodiscard]] const ResolutionMap& resolution_map() const noexcept { return m_resolutions; }
        [[nodiscard]] const TypeResolutionMap& type_resolution_map() const noexcept { return m_type_resolutions; }
        [[nodiscard]] const DisambiguationMap& disambiguation_map() const noexcept { return m_disambiguations; }
        [[nodiscard]] Scope* global_scope() const noexcept { return m_global_scope.get(); }
        [[nodiscard]] Scope* module_scope() const noexcept { return m_module_scope; }
        [[nodiscard]] uint32_t error_count() const noexcept { return m_error_count; }

        [[nodiscard]] const std::vector<Symbol*>& exported_usings() const noexcept { return m_exported_usings; }

        [[nodiscard]] std::unique_ptr<Scope> take_global_scope() noexcept;

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
        ModuleLoader& m_modules;
        diag::DiagnosticPrinter& m_printer;

        std::unique_ptr<Scope> m_global_scope;
        Scope* m_module_scope{nullptr};
        Scope* m_current_scope{nullptr};

        std::vector<Symbol*> m_exported_usings;
        std::unordered_map<std::string, Scope*> m_module_namespaces;

        ResolutionMap m_resolutions;
        TypeResolutionMap m_type_resolutions;
        DisambiguationMap m_disambiguations;
        uint32_t m_error_count{};

        Scope* m_scope_stack_prev{nullptr};

        void forward_declare_types(std::span<ast::Decl* const> decls);
        void forward_declare_functions(std::span<ast::Decl* const> decls);

        Symbol* declare(SymbolKind kind, si::InternedString name, ast::Decl* decl, ast::Visibility vis, sm::SourceRange range);

        Symbol* resolve_name(si::InternedString name, sm::SourceRange range);
        Symbol* resolve_qualified_path(std::span<const si::InternedString> path, sm::SourceRange range);

        Symbol* resolve_dotted_path(std::span<const si::InternedString> path, sm::SourceRange range);

        SemaType* resolve_type(const ast::TypeExpr& type_expr);

        [[nodiscard]] bool is_type_name(si::InternedString name) const noexcept;

        [[nodiscard]] ast::Expr* disambiguate_expr(const ast::AmbiguousExpr& node);
        ast::Stmt* disambiguate_stmt(const ast::AmbiguousStmt& node);
        ast::Decl* disambiguate_decl(const ast::AmbiguousDecl& node);

        void import_module_as_namespace(const ast::ImportDecl& import_decl, const ModuleInfo& mod);
        void bring_into_scope(std::span<const si::InternedString> path, si::InternedString local_name, sm::SourceRange range, bool is_export);

        void import_module_symbols(const ModuleInfo& mod, sm::SourceRange import_site);

        void error(sm::SourceRange range, std::string message);
        void error_redeclared(si::InternedString name, sm::SourceRange new_range, sm::SourceRange prev_range);
        void error_undeclared(si::InternedString name, sm::SourceRange range);

        Scope* push_scope(ScopeKind kind, ast::Node* owner = nullptr);
        void pop_scope();

        Scope* get_or_create_namespace_chain(std::span<const si::InternedString> segments);
    };

} // namespace dcc::sema

#endif /* DCC_SEMA_NAME_RESOLVER_HH */
