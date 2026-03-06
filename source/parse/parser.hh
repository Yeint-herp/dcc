#ifndef DCC_PARSE_PARSER_HH
#define DCC_PARSE_PARSER_HH

#include <ast/ambiguous.hh>
#include <ast/common.hh>
#include <ast/decl.hh>
#include <ast/expr.hh>
#include <ast/node.hh>
#include <ast/pattern.hh>
#include <ast/stmt.hh>
#include <ast/type.hh>
#include <diagnostics.hh>
#include <lex/lexer.hh>
#include <lex/token.hh>
#include <parse/arena.hh>
#include <string>
#include <vector>

namespace dcc::parse
{
    class Parser
    {
    public:
        explicit Parser(lex::Lexer& lexer, AstArena& arena, diag::DiagnosticPrinter& diag) noexcept;

        ast::TranslationUnit* parse();
        [[nodiscard]] bool had_error() const noexcept { return m_had_error; }

    private:
        lex::Lexer& m_lexer;
        AstArena& m_arena;
        diag::DiagnosticPrinter& m_diag;

        std::vector<lex::Token> m_tokens;
        std::size_t m_pos{};
        sm::Location m_prev_end{};
        bool m_had_error{};
        bool m_panic{};
        bool m_speculative{};

        const lex::Token& peek(std::size_t ahead = 0);
        const lex::Token& advance();
        const lex::Token& previous() const;
        bool check(lex::TokenKind k);
        bool check_ahead(std::size_t n, lex::TokenKind k);
        bool match(lex::TokenKind k);
        const lex::Token& expect(lex::TokenKind k, std::string_view ctx);
        bool at_end();

        std::size_t save() const noexcept { return m_pos; }
        void restore(std::size_t pos) noexcept { m_pos = pos; }

        sm::Location loc();
        sm::SourceRange range_from(sm::Location begin);
        sm::SourceRange single_range();

        void error(sm::SourceRange range, std::string msg);
        void error_at_current(std::string msg);
        void synchronize();

        ast::ModuleDecl* parse_module_decl();
        ast::Decl* parse_top_level_decl();
        ast::ImportDecl* parse_import_decl(ast::Visibility vis);
        ast::StructDecl* parse_struct_decl(ast::Visibility vis);
        ast::UnionDecl* parse_union_decl(ast::Visibility vis);
        ast::EnumDecl* parse_enum_decl(ast::Visibility vis);
        ast::UsingDecl* parse_using_decl(ast::Visibility vis);
        ast::Decl* parse_func_or_var_decl(ast::Visibility vis);

        ast::FunctionDecl* parse_function_decl(ast::TypeExpr* ret, si::InternedString name, std::vector<ast::Decl*> tpl, ast::Visibility vis,
                                               sm::Location begin, ast::StorageClass sc);
        ast::VarDecl* parse_var_decl_rest(ast::TypeExpr* type, si::InternedString name, ast::Qualifier quals, sm::Location begin, ast::StorageClass sc);
        ast::ParamDecl* parse_param_decl();
        ast::FieldDecl* parse_field_decl();
        ast::EnumVariantDecl* parse_enum_variant();

        std::vector<ast::Decl*> parse_template_param_list();
        std::vector<ast::ParamDecl*> parse_param_list();
        std::vector<ast::FieldInit> parse_field_init_list();
        std::vector<ast::TemplateArg> parse_template_args();

        ast::Pattern* parse_pattern();
        ast::MatchArm parse_match_arm();

        ast::Stmt* parse_stmt();
        ast::BlockStmt* parse_block();
        ast::IfStmt* parse_if_stmt();
        ast::WhileStmt* parse_while_stmt();
        ast::ForStmt* parse_for_stmt();
        ast::DoWhileStmt* parse_do_while_stmt();
        ast::ReturnStmt* parse_return_stmt();
        ast::BreakStmt* parse_break_stmt();
        ast::ContinueStmt* parse_continue_stmt();
        ast::DeferStmt* parse_defer_stmt();
        ast::MatchStmt* parse_match_stmt();
        ast::Stmt* parse_decl_or_expr_stmt();
        ast::Stmt* parse_ambiguous_decl_or_expr() noexcept;

        ast::Expr* parse_expr();
        ast::Expr* parse_assignment();
        ast::Expr* parse_ternary();
        ast::Expr* parse_binary(int min_prec);
        ast::Expr* parse_unary();
        ast::Expr* parse_postfix(ast::Expr* lhs);
        ast::Expr* parse_primary();
        ast::Expr* parse_call_args(ast::Expr* callee, std::vector<ast::TemplateArg> tpl_args, sm::Location begin);

        ast::TypeExpr* parse_type();
        ast::TypeExpr* parse_base_type();
        ast::TypeExpr* parse_type_suffix(ast::TypeExpr* base);
        ast::PrimitiveKind to_primitive(lex::TokenKind k) const;

        static int binary_precedence(lex::TokenKind k) noexcept;
        static ast::BinaryOp to_binary_op(lex::TokenKind k) noexcept;
        static ast::AssignOp to_assign_op(lex::TokenKind k) noexcept;
        static ast::UnaryOp to_prefix_op(lex::TokenKind k) noexcept;

        bool is_type_keyword(lex::TokenKind k) const noexcept;
        bool is_ambiguous_decl_or_expr() noexcept;
        bool looks_like_local_decl();
        bool skip_balanced(lex::TokenKind open, lex::TokenKind close);
    };

} // namespace dcc::parse

#endif /* DCC_PARSE_PARSER_HH */
