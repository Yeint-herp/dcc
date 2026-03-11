#include <ast/ambiguous.hh>
#include <ast/common.hh>
#include <ast/decl.hh>
#include <ast/expr.hh>
#include <ast/pattern.hh>
#include <ast/stmt.hh>
#include <ast/type.hh>
#include <diagnostics.hh>
#include <gtest/gtest.h>
#include <lex/lexer.hh>
#include <parse/arena.hh>
#include <parse/parser.hh>
#include <sstream>

namespace dcc::test
{
    class ParserTest : public ::testing::Test
    {
    protected:
        sm::SourceManager sm;
        si::StringInterner interner;
        parse::AstArena arena;
        std::ostringstream diag_out;

        struct ParseResult
        {
            ast::TranslationUnit* tu;
            bool had_error;
        };

        ParseResult parse(std::string_view source)
        {
            auto fid = sm.add_synthetic("test.dcc", std::string(source));
            auto* file = sm.get(fid);
            lex::Lexer lexer{*file, interner};
            diag::DiagnosticPrinter printer{sm, diag_out};
            printer.set_color(false);
            parse::Parser parser{lexer, arena, printer};
            auto* tu = parser.parse();
            return {tu, parser.had_error()};
        }

        ast::TranslationUnit* parse_ok(std::string_view source)
        {
            auto [tu, err] = parse(source);
            EXPECT_FALSE(err) << "Parse failed for:\n" << source << "\nDiagnostics:\n" << diag_out.str();
            EXPECT_NE(tu, nullptr);
            return tu;
        }

        ast::TranslationUnit* parse_err(std::string_view source)
        {
            auto [tu, err] = parse(source);
            EXPECT_TRUE(err) << "Expected parse error for:\n" << source;
            return tu;
        }

        ast::Decl* top_decl(ast::TranslationUnit* tu, std::size_t n)
        {
            auto decls = tu->decls();
            std::size_t idx = 0;
            for (auto* d : decls)
            {
                if (dynamic_cast<ast::ModuleDecl*>(d))
                    continue;

                if (idx == n)
                    return d;

                ++idx;
            }
            return nullptr;
        }

        ast::Stmt* func_stmt(ast::FunctionDecl* fn, std::size_t n)
        {
            if (!fn || !fn->body())
                return nullptr;
            auto stmts = fn->body()->stmts();
            return n < stmts.size() ? stmts[n] : nullptr;
        }

        template <typename T, typename U> T* as(U* node)
        {
            EXPECT_NE(node, nullptr);
            auto* result = dynamic_cast<T*>(node);
            EXPECT_NE(result, nullptr) << "Expected " << typeid(T).name() << " but got different node type";
            return result;
        }
    };

    TEST_F(ParserTest, ModuleDecl)
    {
        auto* tu = parse_ok("module foo.bar;");
        ASSERT_NE(tu->module_decl(), nullptr);
        auto path = tu->module_decl()->path();
        ASSERT_EQ(path.size(), 2u);
        EXPECT_EQ(path[0].view(), "foo");
        EXPECT_EQ(path[1].view(), "bar");
    }

    TEST_F(ParserTest, ModuleDeclSimple)
    {
        auto* tu = parse_ok("module main;");
        ASSERT_NE(tu->module_decl(), nullptr);
        EXPECT_EQ(tu->module_decl()->path().size(), 1u);
        EXPECT_EQ(tu->module_decl()->path()[0].view(), "main");
    }

    TEST_F(ParserTest, ImportDecl)
    {
        auto* tu = parse_ok("import std.io;");
        auto* imp = as<ast::ImportDecl>(top_decl(tu, 0));
        ASSERT_EQ(imp->path().size(), 2u);
        EXPECT_EQ(imp->path()[0].view(), "std");
        EXPECT_EQ(imp->path()[1].view(), "io");
    }

    TEST_F(ParserTest, PublicImport)
    {
        auto* tu = parse_ok("public import core;");
        auto* imp = as<ast::ImportDecl>(top_decl(tu, 0));
        EXPECT_EQ(imp->visibility(), ast::Visibility::Public);
    }

    TEST_F(ParserTest, EmptyStruct)
    {
        auto* tu = parse_ok("struct Empty {}");
        auto* s = as<ast::StructDecl>(top_decl(tu, 0));
        EXPECT_EQ(s->name().view(), "Empty");
        EXPECT_EQ(s->fields().size(), 0u);
        EXPECT_EQ(s->visibility(), ast::Visibility::Private);
    }

    TEST_F(ParserTest, StructWithFields)
    {
        auto* tu = parse_ok(R"(
            struct Point {
                f32 x;
                f32 y;
            }
        )");

        auto* s = as<ast::StructDecl>(top_decl(tu, 0));
        EXPECT_EQ(s->name().view(), "Point");
        ASSERT_EQ(s->fields().size(), 2u);
        EXPECT_EQ(s->fields()[0]->name().view(), "x");
        EXPECT_EQ(s->fields()[1]->name().view(), "y");
    }

    TEST_F(ParserTest, StructFieldDefaultValue)
    {
        auto* tu = parse_ok(R"(
            struct Config {
                i32 width = 800;
            }
        )");

        auto* s = as<ast::StructDecl>(top_decl(tu, 0));
        ASSERT_EQ(s->fields().size(), 1u);
        EXPECT_NE(s->fields()[0]->default_value(), nullptr);
    }

    TEST_F(ParserTest, PublicStruct)
    {
        auto* tu = parse_ok("public struct Vec2 { f32 x; f32 y; }");
        auto* s = as<ast::StructDecl>(top_decl(tu, 0));
        EXPECT_EQ(s->visibility(), ast::Visibility::Public);
    }

    TEST_F(ParserTest, TemplatedStruct)
    {
        auto* tu = parse_ok("struct Pair(T, U) { T first; U second; }");
        auto* s = as<ast::StructDecl>(top_decl(tu, 0));
        EXPECT_EQ(s->template_params().size(), 2u);
        EXPECT_EQ(s->fields().size(), 2u);
    }

    TEST_F(ParserTest, UnionDecl)
    {
        auto* tu = parse_ok(R"(
            union Value {
                i32 int_val;
                f64 float_val;
            }
        )");

        auto* u = as<ast::UnionDecl>(top_decl(tu, 0));
        EXPECT_EQ(u->name().view(), "Value");
        EXPECT_EQ(u->fields().size(), 2u);
    }

    TEST_F(ParserTest, SimpleEnum)
    {
        auto* tu = parse_ok(R"(
            enum Color {
                Red,
                Green,
                Blue
            }
        )");

        auto* e = as<ast::EnumDecl>(top_decl(tu, 0));
        EXPECT_EQ(e->name().view(), "Color");
        ASSERT_EQ(e->variants().size(), 3u);
        EXPECT_EQ(e->variants()[0]->name().view(), "Red");
        EXPECT_EQ(e->variants()[1]->name().view(), "Green");
        EXPECT_EQ(e->variants()[2]->name().view(), "Blue");
    }

    TEST_F(ParserTest, EnumWithDiscriminants)
    {
        auto* tu = parse_ok(R"(
            enum Status : i32 {
                Ok = 0,
                Error = 1
            }
        )");

        auto* e = as<ast::EnumDecl>(top_decl(tu, 0));
        EXPECT_NE(e->underlying_type(), nullptr);
        ASSERT_EQ(e->variants().size(), 2u);
        EXPECT_NE(e->variants()[0]->discriminant(), nullptr);
        EXPECT_NE(e->variants()[1]->discriminant(), nullptr);
    }

    TEST_F(ParserTest, EnumWithPayload)
    {
        auto* tu = parse_ok(R"(
            enum Option {
                Some(i32),
                None
            }
        )");

        auto* e = as<ast::EnumDecl>(top_decl(tu, 0));
        ASSERT_EQ(e->variants().size(), 2u);
        EXPECT_EQ(e->variants()[0]->payload_types().size(), 1u);
        EXPECT_EQ(e->variants()[1]->payload_types().size(), 0u);
    }

    TEST_F(ParserTest, UsingAlias)
    {
        auto* tu = parse_ok("using ID = u64;");
        auto* u = as<ast::UsingDecl>(top_decl(tu, 0));
        EXPECT_EQ(u->name().view(), "ID");
        auto* aliased = as<ast::BuiltinType>(u->aliased_type());
        EXPECT_EQ(aliased->kind(), ast::PrimitiveKind::U64);
    }

    TEST_F(ParserTest, UsingPointerAlias)
    {
        auto* tu = parse_ok("using IntPtr = i32*;");
        auto* u = as<ast::UsingDecl>(top_decl(tu, 0));
        as<ast::PointerType>(u->aliased_type());
    }

    TEST_F(ParserTest, SimpleFunction)
    {
        auto* tu = parse_ok(R"(
            i32 add(i32 a, i32 b) {
                return a + b;
            }
        )");

        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        EXPECT_EQ(fn->name().view(), "add");
        EXPECT_EQ(fn->params().size(), 2u);
        EXPECT_EQ(fn->params()[0]->name().view(), "a");
        EXPECT_EQ(fn->params()[1]->name().view(), "b");
        EXPECT_NE(fn->body(), nullptr);
    }

    TEST_F(ParserTest, VoidFunction)
    {
        auto* tu = parse_ok("void noop() {}");
        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        EXPECT_EQ(fn->params().size(), 0u);
        auto* ret = as<ast::BuiltinType>(fn->return_type());
        EXPECT_EQ(ret->kind(), ast::PrimitiveKind::Void);
    }

    TEST_F(ParserTest, PublicFunction)
    {
        auto* tu = parse_ok("public void greet() {}");
        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        EXPECT_EQ(fn->visibility(), ast::Visibility::Public);
    }

    TEST_F(ParserTest, TemplateFunctionDecl)
    {
        auto* tu = parse_ok(R"(
            T clamp(T)(T value, T min, T max) {
                return value;
            }
        )");

        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        EXPECT_EQ(fn->name().view(), "clamp");
        EXPECT_EQ(fn->template_params().size(), 1u);
        EXPECT_EQ(fn->params().size(), 3u);
    }

    TEST_F(ParserTest, FunctionReturningPointer)
    {
        auto* tu = parse_ok("i32* get_ptr() { return null; }");
        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        as<ast::PointerType>(fn->return_type());
    }

    TEST_F(ParserTest, GlobalVarDecl)
    {
        auto* tu = parse_ok("i32 count = 0;");
        auto* v = as<ast::VarDecl>(top_decl(tu, 0));
        EXPECT_EQ(v->name().view(), "count");
        EXPECT_NE(v->init(), nullptr);
    }

    TEST_F(ParserTest, GlobalVarNoInit)
    {
        auto* tu = parse_ok("f64 value;");
        auto* v = as<ast::VarDecl>(top_decl(tu, 0));
        EXPECT_EQ(v->init(), nullptr);
    }

    TEST_F(ParserTest, LocalVarDecl)
    {
        auto* tu = parse_ok(R"(
            void f() {
                i32 x = 10;
            }
        )");

        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        auto* ds = as<ast::DeclStmt>(func_stmt(fn, 0));
        auto* v = as<ast::VarDecl>(ds->decl());
        EXPECT_EQ(v->name().view(), "x");
    }

    TEST_F(ParserTest, ConstVarDecl)
    {
        auto* tu = parse_ok(R"(
            void f() {
                const i32 y = 20;
            }
        )");

        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        auto* ds = as<ast::DeclStmt>(func_stmt(fn, 0));
        auto* v = as<ast::VarDecl>(ds->decl());
        EXPECT_EQ(v->name().view(), "y");
        EXPECT_NE(v->quals() & ast::Qualifier::Const, ast::Qualifier::None);
    }

    TEST_F(ParserTest, ReturnStmt)
    {
        auto* tu = parse_ok(R"(
            i32 f() {
                return 42;
            }
        )");

        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        auto* ret = as<ast::ReturnStmt>(func_stmt(fn, 0));
        EXPECT_NE(ret->value(), nullptr);
    }

    TEST_F(ParserTest, ReturnVoid)
    {
        auto* tu = parse_ok(R"(
            void f() {
                return;
            }
        )");

        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        auto* ret = as<ast::ReturnStmt>(func_stmt(fn, 0));
        EXPECT_EQ(ret->value(), nullptr);
    }

    TEST_F(ParserTest, BreakStmt)
    {
        auto* tu = parse_ok(R"(
            void f() {
                while true { break; }
            }
        )");

        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        auto* ws = as<ast::WhileStmt>(func_stmt(fn, 0));
        auto* blk = as<ast::BlockStmt>(ws->body());
        as<ast::BreakStmt>(blk->stmts()[0]);
    }

    TEST_F(ParserTest, ContinueStmt)
    {
        auto* tu = parse_ok(R"(
            void f() {
                while true { continue; }
            }
        )");

        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        auto* ws = as<ast::WhileStmt>(func_stmt(fn, 0));
        auto* blk = as<ast::BlockStmt>(ws->body());
        as<ast::ContinueStmt>(blk->stmts()[0]);
    }

    TEST_F(ParserTest, EmptyStmt)
    {
        auto* tu = parse_ok(R"(
            void f() {
                ;
            }
        )");

        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        as<ast::EmptyStmt>(func_stmt(fn, 0));
    }

    TEST_F(ParserTest, IfStmtNoParen)
    {
        auto* tu = parse_ok(R"(
            void f() {
                if x > 10 {
                    return;
                }
            }
        )");

        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        auto* ifs = as<ast::IfStmt>(func_stmt(fn, 0));
        EXPECT_NE(ifs->condition(), nullptr);
        EXPECT_NE(ifs->then_branch(), nullptr);
        EXPECT_EQ(ifs->else_branch(), nullptr);
    }

    TEST_F(ParserTest, IfStmtWithParen)
    {
        auto* tu = parse_ok(R"(
            void f() {
                if (x > 10) {
                    return;
                }
            }
        )");

        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        as<ast::IfStmt>(func_stmt(fn, 0));
    }

    TEST_F(ParserTest, IfElse)
    {
        auto* tu = parse_ok(R"(
            void f() {
                if x > 0 {
                    return;
                } else {
                    return;
                }
            }
        )");

        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        auto* ifs = as<ast::IfStmt>(func_stmt(fn, 0));
        EXPECT_NE(ifs->else_branch(), nullptr);
    }

    TEST_F(ParserTest, IfElseIf)
    {
        auto* tu = parse_ok(R"(
            void f() {
                if x > 0 {
                    return;
                } else if x < 0 {
                    return;
                } else {
                    return;
                }
            }
        )");

        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        auto* ifs = as<ast::IfStmt>(func_stmt(fn, 0));
        auto* elif = as<ast::IfStmt>(ifs->else_branch());
        EXPECT_NE(elif->else_branch(), nullptr);
    }

    TEST_F(ParserTest, StaticIf)
    {
        auto* tu = parse_ok(R"(
            void f() {
                static if true {
                    return;
                }
            }
        )");

        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        auto* ifs = as<ast::IfStmt>(func_stmt(fn, 0));
        EXPECT_TRUE(ifs->is_static());
    }

    TEST_F(ParserTest, WhileStmt)
    {
        auto* tu = parse_ok(R"(
            void f() {
                while x > 0 {
                    x = x - 1;
                }
            }
        )");

        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        auto* ws = as<ast::WhileStmt>(func_stmt(fn, 0));
        EXPECT_NE(ws->condition(), nullptr);
        EXPECT_NE(ws->body(), nullptr);
    }

    TEST_F(ParserTest, WhileWithParens)
    {
        auto* tu = parse_ok(R"(
            void f() {
                while (x > 0) {
                    x = x - 1;
                }
            }
        )");

        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        as<ast::WhileStmt>(func_stmt(fn, 0));
    }

    TEST_F(ParserTest, ForStmt)
    {
        auto* tu = parse_ok(R"(
            void f() {
                for (i32 i = 0; i < 10; i = i + 1) {
                    x = x + 1;
                }
            }
        )");

        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        auto* fs = as<ast::ForStmt>(func_stmt(fn, 0));
        EXPECT_NE(fs->init(), nullptr);
        EXPECT_NE(fs->condition(), nullptr);
        EXPECT_NE(fs->increment(), nullptr);
        EXPECT_NE(fs->body(), nullptr);
    }

    TEST_F(ParserTest, ForStmtEmptyInit)
    {
        auto* tu = parse_ok(R"(
            void f() {
                for (; i < 10; i = i + 1) {}
            }
        )");

        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        auto* fs = as<ast::ForStmt>(func_stmt(fn, 0));
        as<ast::EmptyStmt>(fs->init());
    }

    TEST_F(ParserTest, DoWhileStmt)
    {
        auto* tu = parse_ok(R"(
            void f() {
                do {
                    x = x - 1;
                } while x > 0;
            }
        )");

        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        auto* dw = as<ast::DoWhileStmt>(func_stmt(fn, 0));
        EXPECT_NE(dw->body(), nullptr);
        EXPECT_NE(dw->condition(), nullptr);
    }

    TEST_F(ParserTest, DeferStmt)
    {
        auto* tu = parse_ok(R"(
            void f() {
                defer close_file();
            }
        )");

        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        auto* d = as<ast::DeferStmt>(func_stmt(fn, 0));
        EXPECT_NE(d->body(), nullptr);
    }

    TEST_F(ParserTest, DeferBlock)
    {
        auto* tu = parse_ok(R"(
            void f() {
                defer {
                    close_file();
                    free(ptr);
                }
            }
        )");

        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        auto* d = as<ast::DeferStmt>(func_stmt(fn, 0));
        as<ast::BlockStmt>(d->body());
    }

    TEST_F(ParserTest, MatchStmtLiterals)
    {
        auto* tu = parse_ok(R"(
            void f() {
                match code {
                    200 => { ok(); },
                    404 => { not_found(); },
                    _ => { unknown(); }
                }
            }
        )");

        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        auto* ms = as<ast::MatchStmt>(func_stmt(fn, 0));
        EXPECT_NE(ms->scrutinee(), nullptr);
        ASSERT_EQ(ms->arms().size(), 3u);
    }

    TEST_F(ParserTest, MatchArmWildcard)
    {
        auto* tu = parse_ok(R"(
            void f() {
                match x {
                    _ => { return; }
                }
            }
        )");

        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        auto* ms = as<ast::MatchStmt>(func_stmt(fn, 0));
        ASSERT_EQ(ms->arms().size(), 1u);
        as<ast::WildcardPattern>(ms->arms()[0].pattern);
    }

    TEST_F(ParserTest, MatchArmBinding)
    {
        auto* tu = parse_ok(R"(
            void f() {
                match x {
                    val => { return; }
                }
            }
        )");

        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        auto* ms = as<ast::MatchStmt>(func_stmt(fn, 0));
        ASSERT_EQ(ms->arms().size(), 1u);
        auto* bp = as<ast::BindingPattern>(ms->arms()[0].pattern);
        EXPECT_EQ(bp->name().view(), "val");
    }

    TEST_F(ParserTest, MatchArmGuard)
    {
        auto* tu = parse_ok(R"(
            void f() {
                match x {
                    n if n > 0 => { return; }
                }
            }
        )");

        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        auto* ms = as<ast::MatchStmt>(func_stmt(fn, 0));
        ASSERT_EQ(ms->arms().size(), 1u);
        EXPECT_NE(ms->arms()[0].guard, nullptr);
    }

    TEST_F(ParserTest, MatchEnumPattern)
    {
        auto* tu = parse_ok(R"(
            void f() {
                match opt {
                    Some(val) => { return; },
                    None => { return; }
                }
            }
        )");

        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        auto* ms = as<ast::MatchStmt>(func_stmt(fn, 0));
        ASSERT_EQ(ms->arms().size(), 2u);
        auto* ep = as<ast::EnumPattern>(ms->arms()[0].pattern);
        EXPECT_EQ(ep->sub_patterns().size(), 1u);
    }

    TEST_F(ParserTest, MatchStructPattern)
    {
        auto* tu = parse_ok(R"(
            void f() {
                match pt {
                    Point { x: xv, y: yv } => { return; }
                }
            }
        )");

        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        auto* ms = as<ast::MatchStmt>(func_stmt(fn, 0));
        ASSERT_EQ(ms->arms().size(), 1u);
        auto* sp = as<ast::StructPattern>(ms->arms()[0].pattern);
        EXPECT_EQ(sp->type_name().view(), "Point");
        EXPECT_EQ(sp->fields().size(), 2u);
    }

    TEST_F(ParserTest, MatchStructPatternRest)
    {
        auto* tu = parse_ok(R"(
            void f() {
                match pt {
                    Point { x: xv, .. } => { return; }
                }
            }
        )");

        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        auto* ms = as<ast::MatchStmt>(func_stmt(fn, 0));
        auto* sp = as<ast::StructPattern>(ms->arms()[0].pattern);
        EXPECT_TRUE(sp->has_rest());
    }

    TEST_F(ParserTest, MatchNegativeLiteral)
    {
        auto* tu = parse_ok(R"(
            void f() {
                match x {
                    -1 => { return; }
                }
            }
        )");

        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        auto* ms = as<ast::MatchStmt>(func_stmt(fn, 0));
        as<ast::LiteralPattern>(ms->arms()[0].pattern);
    }

    TEST_F(ParserTest, IntegerLiteral)
    {
        auto* tu = parse_ok("void f() { return 42; }");
        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        auto* ret = as<ast::ReturnStmt>(func_stmt(fn, 0));
        as<ast::IntegerLiteral>(ret->value());
    }

    TEST_F(ParserTest, FloatLiteral)
    {
        auto* tu = parse_ok("void f() { return 3.14; }");
        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        auto* ret = as<ast::ReturnStmt>(func_stmt(fn, 0));
        as<ast::FloatLiteral>(ret->value());
    }

    TEST_F(ParserTest, BoolLiterals)
    {
        auto* tu = parse_ok("void f() { return true; }");
        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        auto* ret = as<ast::ReturnStmt>(func_stmt(fn, 0));
        auto* bl = as<ast::BoolLiteral>(ret->value());
        EXPECT_TRUE(bl->value());
    }

    TEST_F(ParserTest, NullLiteral)
    {
        auto* tu = parse_ok("void f() { return null; }");
        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        auto* ret = as<ast::ReturnStmt>(func_stmt(fn, 0));
        as<ast::NullLiteral>(ret->value());
    }

    TEST_F(ParserTest, StringLiteral)
    {
        auto* tu = parse_ok(R"(void f() { return "hello"; })");
        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        auto* ret = as<ast::ReturnStmt>(func_stmt(fn, 0));
        as<ast::StringLiteral>(ret->value());
    }

    TEST_F(ParserTest, CharLiteral)
    {
        auto* tu = parse_ok("void f() { return 'a'; }");
        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        auto* ret = as<ast::ReturnStmt>(func_stmt(fn, 0));
        as<ast::CharLiteral>(ret->value());
    }

    TEST_F(ParserTest, BinaryAdd)
    {
        auto* tu = parse_ok("void f() { return a + b; }");
        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        auto* ret = as<ast::ReturnStmt>(func_stmt(fn, 0));
        auto* bin = as<ast::BinaryExpr>(ret->value());
        EXPECT_EQ(bin->op(), ast::BinaryOp::Add);
    }

    TEST_F(ParserTest, BinaryPrecedence)
    {
        auto* tu = parse_ok("void f() { return a + b * c; }");
        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        auto* ret = as<ast::ReturnStmt>(func_stmt(fn, 0));
        auto* add = as<ast::BinaryExpr>(ret->value());
        EXPECT_EQ(add->op(), ast::BinaryOp::Add);
        as<ast::IdentifierExpr>(add->lhs());
        auto* mul = as<ast::BinaryExpr>(add->rhs());
        EXPECT_EQ(mul->op(), ast::BinaryOp::Mul);
    }

    TEST_F(ParserTest, BinaryLeftAssoc)
    {
        auto* tu = parse_ok("void f() { return a - b - c; }");
        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        auto* ret = as<ast::ReturnStmt>(func_stmt(fn, 0));
        auto* outer = as<ast::BinaryExpr>(ret->value());
        EXPECT_EQ(outer->op(), ast::BinaryOp::Sub);
        auto* inner = as<ast::BinaryExpr>(outer->lhs());
        EXPECT_EQ(inner->op(), ast::BinaryOp::Sub);
        as<ast::IdentifierExpr>(outer->rhs());
    }

    TEST_F(ParserTest, LogicalOperators)
    {
        auto* tu = parse_ok("void f() { return a && b || c; }");
        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        auto* ret = as<ast::ReturnStmt>(func_stmt(fn, 0));
        auto* lor = as<ast::BinaryExpr>(ret->value());
        EXPECT_EQ(lor->op(), ast::BinaryOp::LogOr);
        auto* land = as<ast::BinaryExpr>(lor->lhs());
        EXPECT_EQ(land->op(), ast::BinaryOp::LogAnd);
    }

    TEST_F(ParserTest, ComparisonOperators)
    {
        auto* tu = parse_ok("void f() { return a == b; }");
        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        auto* ret = as<ast::ReturnStmt>(func_stmt(fn, 0));
        auto* cmp = as<ast::BinaryExpr>(ret->value());
        EXPECT_EQ(cmp->op(), ast::BinaryOp::Eq);
    }

    TEST_F(ParserTest, BitwiseOperators)
    {
        auto* tu = parse_ok("void f() { return a & b | c ^ d; }");
        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        auto* ret = as<ast::ReturnStmt>(func_stmt(fn, 0));
        auto* bor = as<ast::BinaryExpr>(ret->value());
        EXPECT_EQ(bor->op(), ast::BinaryOp::BitOr);
    }

    TEST_F(ParserTest, ShiftOperators)
    {
        auto* tu = parse_ok("void f() { return a << 2; }");
        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        auto* ret = as<ast::ReturnStmt>(func_stmt(fn, 0));
        auto* shl = as<ast::BinaryExpr>(ret->value());
        EXPECT_EQ(shl->op(), ast::BinaryOp::Shl);
    }

    TEST_F(ParserTest, UnaryNegate)
    {
        auto* tu = parse_ok("void f() { return -x; }");
        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        auto* ret = as<ast::ReturnStmt>(func_stmt(fn, 0));
        auto* un = as<ast::UnaryExpr>(ret->value());
        EXPECT_EQ(un->op(), ast::UnaryOp::Negate);
    }

    TEST_F(ParserTest, UnaryLogNot)
    {
        auto* tu = parse_ok("void f() { return !x; }");
        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        auto* ret = as<ast::ReturnStmt>(func_stmt(fn, 0));
        auto* un = as<ast::UnaryExpr>(ret->value());
        EXPECT_EQ(un->op(), ast::UnaryOp::LogNot);
    }

    TEST_F(ParserTest, UnaryBitNot)
    {
        auto* tu = parse_ok("void f() { return ~x; }");
        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        auto* ret = as<ast::ReturnStmt>(func_stmt(fn, 0));
        auto* un = as<ast::UnaryExpr>(ret->value());
        EXPECT_EQ(un->op(), ast::UnaryOp::BitNot);
    }

    TEST_F(ParserTest, UnaryDeref)
    {
        auto* tu = parse_ok("void f() { return *ptr; }");
        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        auto* ret = as<ast::ReturnStmt>(func_stmt(fn, 0));
        auto* un = as<ast::UnaryExpr>(ret->value());
        EXPECT_EQ(un->op(), ast::UnaryOp::Deref);
    }

    TEST_F(ParserTest, UnaryAddressOf)
    {
        auto* tu = parse_ok("void f() { return &x; }");
        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        auto* ret = as<ast::ReturnStmt>(func_stmt(fn, 0));
        auto* un = as<ast::UnaryExpr>(ret->value());
        EXPECT_EQ(un->op(), ast::UnaryOp::AddressOf);
    }

    TEST_F(ParserTest, SimpleAssignment)
    {
        auto* tu = parse_ok("void f() { x = 5; }");
        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        auto* es = as<ast::ExprStmt>(func_stmt(fn, 0));
        auto* asg = as<ast::AssignExpr>(es->expr());
        EXPECT_EQ(asg->op(), ast::AssignOp::Simple);
    }

    TEST_F(ParserTest, CompoundAssignment)
    {
        auto* tu = parse_ok("void f() { x += 1; }");
        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        auto* es = as<ast::ExprStmt>(func_stmt(fn, 0));
        auto* asg = as<ast::AssignExpr>(es->expr());
        EXPECT_EQ(asg->op(), ast::AssignOp::Add);
    }

    TEST_F(ParserTest, AssignmentRightAssoc)
    {
        auto* tu = parse_ok("void f() { a = b = c; }");
        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        auto* es = as<ast::ExprStmt>(func_stmt(fn, 0));
        auto* outer = as<ast::AssignExpr>(es->expr());
        as<ast::AssignExpr>(outer->value());
    }

    TEST_F(ParserTest, TernaryExpr)
    {
        auto* tu = parse_ok("void f() { return x > 0 ? 1 : 0; }");
        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        auto* ret = as<ast::ReturnStmt>(func_stmt(fn, 0));
        auto* cond = as<ast::ConditionalExpr>(ret->value());
        EXPECT_NE(cond->condition(), nullptr);
        EXPECT_NE(cond->then_expr(), nullptr);
        EXPECT_NE(cond->else_expr(), nullptr);
    }

    TEST_F(ParserTest, CastExpr)
    {
        auto* tu = parse_ok("void f() { return x as i32; }");
        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        auto* ret = as<ast::ReturnStmt>(func_stmt(fn, 0));
        auto* cast = as<ast::CastExpr>(ret->value());
        as<ast::BuiltinType>(cast->target_type());
    }

    TEST_F(ParserTest, ChainedCast)
    {
        auto* tu = parse_ok("void f() { return x as i32 as i64; }");
        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        auto* ret = as<ast::ReturnStmt>(func_stmt(fn, 0));
        auto* outer = as<ast::CastExpr>(ret->value());
        as<ast::CastExpr>(outer->operand());
    }

    TEST_F(ParserTest, MemberAccess)
    {
        auto* tu = parse_ok("void f() { return p.x; }");
        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        auto* ret = as<ast::ReturnStmt>(func_stmt(fn, 0));
        auto* ma = as<ast::MemberAccessExpr>(ret->value());
        EXPECT_EQ(ma->member().view(), "x");
    }

    TEST_F(ParserTest, ChainedMemberAccess)
    {
        auto* tu = parse_ok("void f() { return a.b.c; }");
        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        auto* ret = as<ast::ReturnStmt>(func_stmt(fn, 0));
        auto* outer = as<ast::MemberAccessExpr>(ret->value());
        EXPECT_EQ(outer->member().view(), "c");
        auto* inner = as<ast::MemberAccessExpr>(outer->object());
        EXPECT_EQ(inner->member().view(), "b");
    }

    TEST_F(ParserTest, FunctionCall)
    {
        auto* tu = parse_ok("void f() { foo(1, 2); }");
        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        auto* es = as<ast::ExprStmt>(func_stmt(fn, 0));
        auto* call = as<ast::CallExpr>(es->expr());
        EXPECT_EQ(call->args().size(), 2u);
    }

    TEST_F(ParserTest, FunctionCallNoArgs)
    {
        auto* tu = parse_ok("void f() { foo(); }");
        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        auto* es = as<ast::ExprStmt>(func_stmt(fn, 0));
        auto* call = as<ast::CallExpr>(es->expr());
        EXPECT_EQ(call->args().size(), 0u);
    }

    TEST_F(ParserTest, MethodCallUFCS)
    {
        auto* tu = parse_ok("void f() { p.scale(5.0); }");
        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        auto* es = as<ast::ExprStmt>(func_stmt(fn, 0));
        auto* call = as<ast::CallExpr>(es->expr());
        auto* ma = as<ast::MemberAccessExpr>(call->callee());
        EXPECT_EQ(ma->member().view(), "scale");
    }

    TEST_F(ParserTest, IndexExpr)
    {
        auto* tu = parse_ok("void f() { return arr[0]; }");
        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        auto* ret = as<ast::ReturnStmt>(func_stmt(fn, 0));
        auto* idx = as<ast::IndexExpr>(ret->value());
        EXPECT_NE(idx->object(), nullptr);
        EXPECT_NE(idx->index(), nullptr);
    }

    TEST_F(ParserTest, SliceExpr)
    {
        auto* tu = parse_ok("void f() { return arr[1..5]; }");
        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        auto* ret = as<ast::ReturnStmt>(func_stmt(fn, 0));
        auto* sl = as<ast::SliceExpr>(ret->value());
        EXPECT_NE(sl->begin_idx(), nullptr);
        EXPECT_NE(sl->end_idx(), nullptr);
    }

    TEST_F(ParserTest, SizeofExpr)
    {
        auto* tu = parse_ok("void f() { return sizeof(i32); }");
        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        auto* ret = as<ast::ReturnStmt>(func_stmt(fn, 0));
        auto* so = as<ast::SizeofExpr>(ret->value());
        as<ast::BuiltinType>(so->operand());
    }

    TEST_F(ParserTest, AlignofExpr)
    {
        auto* tu = parse_ok("void f() { return alignof(f64); }");
        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        auto* ret = as<ast::ReturnStmt>(func_stmt(fn, 0));
        auto* ao = as<ast::AlignofExpr>(ret->value());
        as<ast::BuiltinType>(ao->operand());
    }

    TEST_F(ParserTest, GroupingExpr)
    {
        auto* tu = parse_ok("void f() { return (a + b) * c; }");
        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        auto* ret = as<ast::ReturnStmt>(func_stmt(fn, 0));
        auto* mul = as<ast::BinaryExpr>(ret->value());
        EXPECT_EQ(mul->op(), ast::BinaryOp::Mul);
        as<ast::GroupingExpr>(mul->lhs());
    }

    TEST_F(ParserTest, StructInitializer)
    {
        auto* tu = parse_ok(R"(
            void f() {
                return { x: 1.0, y: 2.0 };
            }
        )");

        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        auto* ret = as<ast::ReturnStmt>(func_stmt(fn, 0));
        auto* init = as<ast::InitializerExpr>(ret->value());
        ASSERT_EQ(init->fields().size(), 2u);
        EXPECT_EQ(init->fields()[0].name.view(), "x");
        EXPECT_EQ(init->fields()[1].name.view(), "y");
    }

    TEST_F(ParserTest, BuiltinTypes)
    {
        auto* tu = parse_ok("i8 a; u8 b; i16 c; u16 d; i32 e; u32 f; i64 g; u64 h; f32 i; f64 j; bool k;");
        EXPECT_EQ(tu->decls().size(), 11u);
    }

    TEST_F(ParserTest, PointerType)
    {
        auto* tu = parse_ok("i32* p;");
        auto* v = as<ast::VarDecl>(top_decl(tu, 0));
        auto* pt = as<ast::PointerType>(v->type());
        as<ast::BuiltinType>(pt->pointee());
    }

    TEST_F(ParserTest, DoublePointer)
    {
        auto* tu = parse_ok("i32** pp;");
        auto* v = as<ast::VarDecl>(top_decl(tu, 0));
        auto* outer = as<ast::PointerType>(v->type());
        auto* inner = as<ast::PointerType>(outer->pointee());
        as<ast::BuiltinType>(inner->pointee());
    }

    TEST_F(ParserTest, ArrayType)
    {
        auto* tu = parse_ok("i32[10] arr;");
        auto* v = as<ast::VarDecl>(top_decl(tu, 0));
        auto* at = as<ast::ArrayType>(v->type());
        as<ast::BuiltinType>(at->element());
        EXPECT_NE(at->size(), nullptr);
    }

    TEST_F(ParserTest, SliceType)
    {
        auto* tu = parse_ok("[]u8 data;");
        auto* v = as<ast::VarDecl>(top_decl(tu, 0));
        auto* st = as<ast::SliceType>(v->type());
        as<ast::BuiltinType>(st->element());
    }

    TEST_F(ParserTest, ConstType)
    {
        auto* tu = parse_ok(R"(
            void f() {
                const i32 x = 5;
            }
        )");

        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        auto* ds = as<ast::DeclStmt>(func_stmt(fn, 0));
        auto* v = as<ast::VarDecl>(ds->decl());
        EXPECT_NE(v->quals() & ast::Qualifier::Const, ast::Qualifier::None);
    }

    TEST_F(ParserTest, ConstPointerSemantics)
    {
        auto* tu = parse_ok("const i32* p;");
        auto* v = as<ast::VarDecl>(top_decl(tu, 0));
        auto* ptr = as<ast::PointerType>(v->type());
        auto* qual = as<ast::QualifiedType>(ptr->pointee());
        EXPECT_NE(qual->quals() & ast::Qualifier::Const, ast::Qualifier::None);
        as<ast::BuiltinType>(qual->inner());
    }

    TEST_F(ParserTest, NamedType)
    {
        auto* tu = parse_ok("MyStruct s;");
        auto* v = as<ast::VarDecl>(top_decl(tu, 0));
        auto* nt = as<ast::NamedType>(v->type());
        EXPECT_EQ(nt->name().view(), "MyStruct");
    }

    TEST_F(ParserTest, FlexibleArrayType)
    {
        auto* tu = parse_ok("i32[] buf;");
        auto* v = as<ast::VarDecl>(top_decl(tu, 0));
        as<ast::FlexibleArrayType>(v->type());
    }

    TEST_F(ParserTest, TemplateTypeField)
    {
        auto* tu = parse_ok(R"(
            struct Container {
                Vec(i32) items;
            }
        )");

        auto* s = as<ast::StructDecl>(top_decl(tu, 0));
        ASSERT_EQ(s->fields().size(), 1u);
        auto* tt = as<ast::TemplateType>(s->fields()[0]->type());
        auto* base = as<ast::NamedType>(tt->base());
        EXPECT_EQ(base->name().view(), "Vec");
        ASSERT_EQ(tt->args().size(), 1u);
    }

    TEST_F(ParserTest, TemplateTypePointer)
    {
        auto* tu = parse_ok("Vec(i32)* p;");
        auto* v = as<ast::VarDecl>(top_decl(tu, 0));
        auto* ptr = as<ast::PointerType>(v->type());
        as<ast::TemplateType>(ptr->pointee());
    }

    TEST_F(ParserTest, TemplateTypeMultipleArgs)
    {
        auto* tu = parse_ok("Map(i32, f64) m;");
        auto* v = as<ast::VarDecl>(top_decl(tu, 0));
        auto* tt = as<ast::TemplateType>(v->type());
        EXPECT_EQ(tt->args().size(), 2u);
    }

    TEST_F(ParserTest, AmbiguousSingleArgCall)
    {
        auto* tu = parse_ok("void f() { foo(x); }");
        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        auto* es = as<ast::ExprStmt>(func_stmt(fn, 0));
        auto* amb = as<ast::AmbiguousExpr>(es->expr());
        ASSERT_EQ(amb->alternatives().size(), 2u);
        as<ast::CallExpr>(amb->alternatives()[0]);
        as<ast::CastExpr>(amb->alternatives()[1]);
    }

    TEST_F(ParserTest, MultiArgCallNotAmbiguous)
    {
        auto* tu = parse_ok("void f() { foo(x, y); }");
        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        auto* es = as<ast::ExprStmt>(func_stmt(fn, 0));
        as<ast::CallExpr>(es->expr());
    }

    TEST_F(ParserTest, AmbiguousStarDeclOrExpr)
    {
        auto* tu = parse_ok("void f() { foo * bar; }");
        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        auto* amb = as<ast::AmbiguousStmt>(func_stmt(fn, 0));
        ASSERT_EQ(amb->alternatives().size(), 2u);
        as<ast::DeclStmt>(amb->alternatives()[0]);
        as<ast::ExprStmt>(amb->alternatives()[1]);
    }

    TEST_F(ParserTest, AmbiguousStarDeclWithInit)
    {
        auto* tu = parse_ok("void f() { foo * bar = 5; }");
        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        as<ast::AmbiguousStmt>(func_stmt(fn, 0));
    }

    TEST_F(ParserTest, AmbiguousDoublePointer)
    {
        auto* tu = parse_ok("void f() { foo ** bar; }");
        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        as<ast::AmbiguousStmt>(func_stmt(fn, 0));
    }

    TEST_F(ParserTest, NotAmbiguousMulMul)
    {
        auto* tu = parse_ok("void f() { foo * bar * baz; }");
        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        as<ast::ExprStmt>(func_stmt(fn, 0));
    }

    TEST_F(ParserTest, NotAmbiguousMulLiteral)
    {
        auto* tu = parse_ok("void f() { foo * 42; }");
        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        as<ast::ExprStmt>(func_stmt(fn, 0));
    }

    TEST_F(ParserTest, UnambiguousUserTypeDecl)
    {
        auto* tu = parse_ok("void f() { Foo bar; }");
        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        as<ast::DeclStmt>(func_stmt(fn, 0));
    }

    TEST_F(ParserTest, AmbiguousTemplateStarIdent)
    {
        auto* tu = parse_ok("void f() { foo(u64) * x; }");
        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        as<ast::AmbiguousStmt>(func_stmt(fn, 0));
    }

    TEST_F(ParserTest, UnambiguousTemplateDecl)
    {
        auto* tu = parse_ok("void f() { Foo(i32) bar; }");
        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        auto* ds = as<ast::DeclStmt>(func_stmt(fn, 0));
        auto* v = as<ast::VarDecl>(ds->decl());
        as<ast::TemplateType>(v->type());
    }

    TEST_F(ParserTest, TemplateCallExplicit)
    {
        auto* tu = parse_ok("void f() { clamp!(i32)(x, lo, hi); }");
        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        auto* es = as<ast::ExprStmt>(func_stmt(fn, 0));
        auto* call = as<ast::CallExpr>(es->expr());
        EXPECT_EQ(call->template_args().size(), 1u);
        EXPECT_EQ(call->args().size(), 3u);
    }

    TEST_F(ParserTest, TemplateCallMethod)
    {
        auto* tu = parse_ok("void f() { obj.method!(f32)(1.0); }");
        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        auto* es = as<ast::ExprStmt>(func_stmt(fn, 0));
        auto* call = as<ast::CallExpr>(es->expr());
        EXPECT_EQ(call->template_args().size(), 1u);
    }

    TEST_F(ParserTest, FullProgram)
    {
        auto* tu = parse_ok(R"(
            module main;

            import std.io;

            using ID = u64;

            public struct Point {
                f32 x;
                f32 y;
            }

            void scale(Point p, const f32 factor) {
                p.x = p.x * factor;
                p.y = p.y * factor;
            }

            public i32 main() {
                Point pt = { x: 1.0, y: 2.0 };
                pt.scale(5.0);

                if pt.x > 10.0 {
                    return 1;
                } else {
                    return 0;
                }
            }
        )");

        EXPECT_NE(tu->module_decl(), nullptr);
        EXPECT_GE(tu->decls().size(), 5u);
    }

    TEST_F(ParserTest, NestedBlocks)
    {
        auto* tu = parse_ok(R"(
            void f() {
                {
                    {
                        return;
                    }
                }
            }
        )");

        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        auto* outer = as<ast::BlockStmt>(func_stmt(fn, 0));
        auto* inner = as<ast::BlockStmt>(outer->stmts()[0]);
        as<ast::ReturnStmt>(inner->stmts()[0]);
    }

    TEST_F(ParserTest, ComplexExpression)
    {
        auto* tu = parse_ok(R"(
            void f() {
                return (a + b) * c - d / e % f;
            }
        )");

        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        auto* ret = as<ast::ReturnStmt>(func_stmt(fn, 0));

        auto* sub = as<ast::BinaryExpr>(ret->value());
        EXPECT_EQ(sub->op(), ast::BinaryOp::Sub);

        auto* mul = as<ast::BinaryExpr>(sub->lhs());
        EXPECT_EQ(mul->op(), ast::BinaryOp::Mul);

        auto* mod = as<ast::BinaryExpr>(sub->rhs());
        EXPECT_EQ(mod->op(), ast::BinaryOp::Mod);
    }

    TEST_F(ParserTest, DeferWithReturn)
    {
        auto* tu = parse_ok(R"(
            void f() {
                defer close();
                if err {
                    return;
                }
            }
        )");

        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        as<ast::DeferStmt>(func_stmt(fn, 0));
        as<ast::IfStmt>(func_stmt(fn, 1));
    }

    TEST_F(ParserTest, MatchExpression)
    {
        auto* tu = parse_ok(R"(
            void f() {
                match status_code {
                    200 => { ok(); },
                    404 => { not_found(); },
                    500 => { error(); },
                    _ => { unknown(); }
                }
            }
        )");

        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        auto* ms = as<ast::MatchStmt>(func_stmt(fn, 0));
        EXPECT_EQ(ms->arms().size(), 4u);
    }

    TEST_F(ParserTest, MissingSemicolon)
    {
        parse_err("i32 x = 5");
    }

    TEST_F(ParserTest, MissingBrace)
    {
        parse_err("struct S { i32 x;");
    }

    TEST_F(ParserTest, UnexpectedToken)
    {
        parse_err("void f() { @@@ }");
    }

    TEST_F(ParserTest, RecoveryAfterError)
    {
        auto [tu, err] = parse(R"(
            i32 x =;
            i32 y = 5;
        )");

        EXPECT_TRUE(err);
        EXPECT_NE(tu, nullptr);
        EXPECT_GE(tu->decls().size(), 1u);
    }

    TEST_F(ParserTest, MissingModuleSemicolon)
    {
        parse_err("module foo");
    }

    TEST_F(ParserTest, EmptyFile)
    {
        auto* tu = parse_ok("");
        EXPECT_EQ(tu->module_decl(), nullptr);
        EXPECT_EQ(tu->decls().size(), 0u);
    }

    TEST_F(ParserTest, MultipleStructs)
    {
        auto* tu = parse_ok(R"(
            struct A { i32 x; }
            struct B { f32 y; }
        )");

        auto* a = as<ast::StructDecl>(top_decl(tu, 0));
        auto* b = as<ast::StructDecl>(top_decl(tu, 1));
        EXPECT_EQ(a->name().view(), "A");
        EXPECT_EQ(b->name().view(), "B");
    }

    TEST_F(ParserTest, MultipleFunctions)
    {
        auto* tu = parse_ok(R"(
            void a() {}
            void b() {}
            void c() {}
        )");

        EXPECT_EQ(tu->decls().size(), 3u);
    }

    TEST_F(ParserTest, TrailingCommaInCall)
    {
        parse_err("void f() { foo(1, 2,); }");
    }

    TEST_F(ParserTest, NestedCalls)
    {
        auto* tu = parse_ok("void f() { foo(bar(baz())); }");
        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        auto* es = as<ast::ExprStmt>(func_stmt(fn, 0));
        auto* amb = as<ast::AmbiguousExpr>(es->expr());
        auto* call = as<ast::CallExpr>(amb->alternatives()[0]);
        EXPECT_EQ(call->args().size(), 1u);
    }

    TEST_F(ParserTest, IndexThenMember)
    {
        auto* tu = parse_ok("void f() { return arr[0].field; }");
        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        auto* ret = as<ast::ReturnStmt>(func_stmt(fn, 0));
        auto* ma = as<ast::MemberAccessExpr>(ret->value());
        EXPECT_EQ(ma->member().view(), "field");
        as<ast::IndexExpr>(ma->object());
    }

    TEST_F(ParserTest, QualifiedEnumPath)
    {
        auto* tu = parse_ok(R"(
            void f() {
                match x {
                    Foo::Bar => { return; }
                }
            }
        )");

        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        auto* ms = as<ast::MatchStmt>(func_stmt(fn, 0));
        auto* ep = as<ast::EnumPattern>(ms->arms()[0].pattern);
        EXPECT_EQ(ep->path().size(), 2u);
    }

    TEST_F(ParserTest, CastInBinaryExpr)
    {
        auto* tu = parse_ok("void f() { return x as i32 + y; }");
        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        auto* ret = as<ast::ReturnStmt>(func_stmt(fn, 0));

        auto* add = as<ast::BinaryExpr>(ret->value());
        EXPECT_EQ(add->op(), ast::BinaryOp::Add);
        as<ast::CastExpr>(add->lhs());
    }

    TEST_F(ParserTest, StaticFunction)
    {
        auto* tu = parse_ok("static void f() {}");
        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));

        EXPECT_TRUE(fn->is_static());
        EXPECT_FALSE(fn->is_extern());
        EXPECT_EQ(fn->storage_class(), ast::StorageClass::Static);
    }

    TEST_F(ParserTest, ExternFunction)
    {
        auto* tu = parse_ok("extern void f();");
        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        EXPECT_TRUE(fn->is_extern());
        EXPECT_FALSE(fn->is_static());
        EXPECT_EQ(fn->storage_class(), ast::StorageClass::Extern);
    }

    TEST_F(ParserTest, StaticGlobalVar)
    {
        auto* tu = parse_ok("static i32 x = 0;");
        auto* v = as<ast::VarDecl>(top_decl(tu, 0));
        EXPECT_EQ(v->storage_class(), ast::StorageClass::Static);
    }

    TEST_F(ParserTest, ExternGlobalVar)
    {
        auto* tu = parse_ok("extern i32 x;");
        auto* v = as<ast::VarDecl>(top_decl(tu, 0));
        EXPECT_EQ(v->storage_class(), ast::StorageClass::Extern);
    }

    TEST_F(ParserTest, StaticLocalVar)
    {
        auto* tu = parse_ok(R"(
            void f() {
                static i32 count = 0;
            }
        )");

        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        auto* ds = as<ast::DeclStmt>(func_stmt(fn, 0));
        auto* v = as<ast::VarDecl>(ds->decl());
        EXPECT_EQ(v->storage_class(), ast::StorageClass::Static);
    }

    TEST_F(ParserTest, AttributeOnFunction_Single)
    {
        auto* tu = parse_ok("#[inline]\nvoid f() {}");
        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        ASSERT_EQ(fn->attributes().size(), 1u);
        ASSERT_EQ(fn->attributes()[0].entries.size(), 1u);
        EXPECT_EQ(fn->attributes()[0].entries[0], "inline");
        ASSERT_EQ(fn->attributes()[0].raw_tokens.size(), 1u);
        EXPECT_EQ(fn->attributes()[0].raw_tokens[0].kind, lex::TokenKind::Identifier);
    }

    TEST_F(ParserTest, AttributeOnFunction_Multiple)
    {
        auto* tu = parse_ok("#[nomangle, optimize(release)]\nvoid f() {}");
        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        ASSERT_EQ(fn->attributes().size(), 1u);
        auto attr = fn->attributes()[0];
        ASSERT_EQ(attr.entries.size(), 2u);
        EXPECT_EQ(attr.entries[0], "nomangle");
        EXPECT_EQ(attr.entries[1], "optimize(release)");
    }

    TEST_F(ParserTest, AttributeOnFunction_ManyAttrs)
    {
        auto* tu = parse_ok("#[debug]\n#[nomangle]\nvoid g() {}");
        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        ASSERT_EQ(fn->attributes().size(), 2u);
        EXPECT_EQ(fn->attributes()[0].entries[0], "debug");
        EXPECT_EQ(fn->attributes()[1].entries[0], "nomangle");
    }

    TEST_F(ParserTest, AttributeOnFunction_RawTokens)
    {
        auto* tu = parse_ok("#[debug, nomangle, optimize(release)]\nvoid f() {}");
        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        ASSERT_EQ(fn->attributes().size(), 1u);

        EXPECT_EQ(fn->attributes()[0].raw_tokens.size(), 8u);
        EXPECT_EQ(fn->attributes()[0].entries.size(), 3u);
        EXPECT_EQ(fn->attributes()[0].entries[0], "debug");
        EXPECT_EQ(fn->attributes()[0].entries[1], "nomangle");
        EXPECT_EQ(fn->attributes()[0].entries[2], "optimize(release)");
    }

    TEST_F(ParserTest, AttributeOnStruct)
    {
        auto* tu = parse_ok("#[repr(C)]\nstruct Foo { i32 x; }");
        auto* s = as<ast::StructDecl>(top_decl(tu, 0));
        ASSERT_EQ(s->attributes().size(), 1u);
        ASSERT_EQ(s->attributes()[0].entries.size(), 1u);
        EXPECT_EQ(s->attributes()[0].entries[0], "repr(C)");
    }

    TEST_F(ParserTest, AttributeOnStruct_MultipleEntries)
    {
        auto* tu = parse_ok("#[packed, align(8)]\nstruct Bar { u8 b; }");
        auto* s = as<ast::StructDecl>(top_decl(tu, 0));
        ASSERT_EQ(s->attributes().size(), 1u);
        auto attr = s->attributes()[0];
        ASSERT_EQ(attr.entries.size(), 2u);
        EXPECT_EQ(attr.entries[0], "packed");
        EXPECT_EQ(attr.entries[1], "align(8)");
    }

    TEST_F(ParserTest, AttributeOnEnum)
    {
        auto* tu = parse_ok("#[repr(u8)]\nenum Color { Red, Green, Blue }");
        auto* e = as<ast::EnumDecl>(top_decl(tu, 0));
        ASSERT_EQ(e->attributes().size(), 1u);
        EXPECT_EQ(e->attributes()[0].entries[0], "repr(u8)");
    }

    TEST_F(ParserTest, AttributeOnUnion)
    {
        auto* tu = parse_ok("#[nomangle]\nunion U { i32 a; f32 b; }");
        auto* u = as<ast::UnionDecl>(top_decl(tu, 0));
        ASSERT_EQ(u->attributes().size(), 1u);
        EXPECT_EQ(u->attributes()[0].entries[0], "nomangle");
    }

    TEST_F(ParserTest, AttributeEmpty)
    {
        auto* tu = parse_ok("#[]\nvoid f() {}");
        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        ASSERT_EQ(fn->attributes().size(), 1u);
        EXPECT_EQ(fn->attributes()[0].raw_tokens.size(), 0u);
        EXPECT_EQ(fn->attributes()[0].entries.size(), 0u);
    }

    TEST_F(ParserTest, AttributeNestedParens)
    {
        auto* tu = parse_ok("#[foo(a, b, c)]\nvoid f() {}");
        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        ASSERT_EQ(fn->attributes().size(), 1u);
        ASSERT_EQ(fn->attributes()[0].entries.size(), 1u);
        EXPECT_EQ(fn->attributes()[0].entries[0], "foo(a, b, c)");
    }

    TEST_F(ParserTest, AttributePublicFunction)
    {
        auto* tu = parse_ok("#[inline]\npublic void h() {}");
        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        ASSERT_EQ(fn->attributes().size(), 1u);
        EXPECT_EQ(fn->attributes()[0].entries[0], "inline");
        EXPECT_EQ(fn->visibility(), ast::Visibility::Public);
    }

    TEST_F(ParserTest, NoAttributeOnFunction)
    {
        auto* tu = parse_ok("void f() {}");
        auto* fn = as<ast::FunctionDecl>(top_decl(tu, 0));
        EXPECT_EQ(fn->attributes().size(), 0u);
    }

    TEST_F(ParserTest, NoAttributeOnStruct)
    {
        auto* tu = parse_ok("struct Foo { i32 x; }");
        auto* s = as<ast::StructDecl>(top_decl(tu, 0));
        EXPECT_EQ(s->attributes().size(), 0u);
    }

} // namespace dcc::test
