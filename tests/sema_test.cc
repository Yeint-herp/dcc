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
#include <sema/sema.hh>
#include <sstream>

namespace dcc::test
{
    class SemaTest : public ::testing::Test
    {
    protected:
        sm::SourceManager sm;
        si::StringInterner interner;
        parse::AstArena arena;
        std::ostringstream diag_out;

        struct SemaResult
        {
            ast::TranslationUnit* tu;
            bool parse_error;
            bool sema_error;
            std::string diagnostics;
        };

        SemaResult analyze(std::string_view source)
        {
            auto fid = sm.add_synthetic("test.dcc", std::string(source));
            auto* file = sm.get(fid);
            lex::Lexer lexer{*file, interner};
            diag::DiagnosticPrinter printer{sm, diag_out};
            printer.set_color(false);
            parse::Parser parser{lexer, arena, printer};
            auto* tu = parser.parse();
            bool parse_err = parser.had_error();

            if (parse_err || !tu)
                return {tu, parse_err, true, diag_out.str()};

            sema::Sema sema{sm, interner, printer};
            bool ok = sema.analyze(*tu);

            return {tu, false, !ok, diag_out.str()};
        }

        ast::TranslationUnit* analyze_ok(std::string_view source)
        {
            auto r = analyze(source);
            EXPECT_FALSE(r.parse_error) << "Parse failed:\n" << r.diagnostics;
            EXPECT_FALSE(r.sema_error) << "Sema failed for:\n" << source << "\nDiagnostics:\n" << r.diagnostics;
            return r.tu;
        }

        void analyze_err(std::string_view source)
        {
            auto r = analyze(source);
            EXPECT_FALSE(r.parse_error) << "Parse failed (expected sema error):\n" << r.diagnostics;
            EXPECT_TRUE(r.sema_error) << "Expected sema error for:\n" << source;
        }

        struct FullResult
        {
            ast::TranslationUnit* tu;
            std::unique_ptr<sema::Sema> sema;
            bool ok;
            std::string diagnostics;
        };

        FullResult analyze_full(std::string_view source)
        {
            auto fid = sm.add_synthetic("test.dcc", std::string(source));
            auto* file = sm.get(fid);
            lex::Lexer lexer{*file, interner};
            diag::DiagnosticPrinter printer{sm, diag_out};
            printer.set_color(false);
            parse::Parser parser{lexer, arena, printer};
            auto* tu = parser.parse();
            if (parser.had_error() || !tu)
                return {tu, nullptr, false, diag_out.str()};

            auto s = std::make_unique<sema::Sema>(sm, interner, printer);
            bool ok = s->analyze(*tu);
            return {tu, std::move(s), ok, diag_out.str()};
        }

        ast::Decl* top_decl(ast::TranslationUnit* tu, std::size_t n)
        {
            std::size_t idx = 0;
            for (auto* d : tu->decls())
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
            EXPECT_NE(result, nullptr) << "Expected " << typeid(T).name();
            return result;
        }
    };

    TEST_F(SemaTest, EmptyFile)
    {
        analyze_ok("");
    }

    TEST_F(SemaTest, EmptyFunction)
    {
        analyze_ok("void f() {}");
    }

    TEST_F(SemaTest, ReturnIntLiteral)
    {
        analyze_ok("i32 f() { return 42; }");
    }

    TEST_F(SemaTest, ReturnFloatLiteral)
    {
        analyze_ok("f64 f() { return 3.14; }");
    }

    TEST_F(SemaTest, ReturnBoolTrue)
    {
        analyze_ok("bool f() { return true; }");
    }

    TEST_F(SemaTest, ReturnBoolFalse)
    {
        analyze_ok("bool f() { return false; }");
    }

    TEST_F(SemaTest, ReturnNull)
    {
        analyze_ok("i32* f() { return null; }");
    }

    TEST_F(SemaTest, ReturnVoidImplicit)
    {
        analyze_ok("void f() { return; }");
    }

    TEST_F(SemaTest, ReturnVoidNoReturn)
    {
        analyze_ok("void f() {}");
    }

    TEST_F(SemaTest, LocalVarWithInit)
    {
        analyze_ok(R"(
        void f() {
            i32 x = 10;
        }
    )");
    }

    TEST_F(SemaTest, LocalVarExplicitType)
    {
        analyze_ok(R"(
        void f() {
            f64 pi = 3.14;
        }
    )");
    }

    TEST_F(SemaTest, ConstLocalVar)
    {
        analyze_ok(R"(
        void f() {
            const i32 x = 42;
        }
    )");
    }

    TEST_F(SemaTest, GlobalVar)
    {
        analyze_ok("i32 count = 0;");
    }

    TEST_F(SemaTest, GlobalVarNoInit)
    {
        analyze_ok("f64 value;");
    }

    TEST_F(SemaTest, StaticGlobalVar)
    {
        analyze_ok("static i32 x = 0;");
    }

    TEST_F(SemaTest, ExternGlobalVar)
    {
        analyze_ok("extern i32 x;");
    }

    TEST_F(SemaTest, StaticLocalVar)
    {
        analyze_ok(R"(
        void f() {
            static i32 count = 0;
        }
    )");
    }

    TEST_F(SemaTest, AllBuiltinTypes)
    {
        analyze_ok(R"(
        void f() {
            i8  a = 0;
            u8  b = 0;
            i16 c = 0;
            u16 d = 0;
            i32 e = 0;
            u32 f_ = 0;
            i64 g = 0;
            u64 h = 0;
            f32 i_ = 0.0;
            f64 j = 0.0;
            bool k = true;
        }
    )");
    }

    TEST_F(SemaTest, IntWideningSameSigned)
    {
        analyze_ok(R"(
        void f() {
            i8 a = 1;
            i16 b = a;
            i32 c = b;
            i64 d = c;
        }
    )");
    }

    TEST_F(SemaTest, IntWideningUnsigned)
    {
        analyze_ok(R"(
        void f() {
            u8 a = 1;
            u16 b = a;
            u32 c = b;
            u64 d = c;
        }
    )");
    }

    TEST_F(SemaTest, UnsignedToWiderSigned)
    {
        analyze_ok(R"(
        void f() {
            u8 a = 1;
            i16 b = a;
            u16 c = 1;
            i32 d = c;
        }
    )");
    }

    TEST_F(SemaTest, FloatWidening)
    {
        analyze_ok(R"(
        void f() {
            f32 a = 1.0;
            f64 b = a;
        }
    )");
    }

    TEST_F(SemaTest, NullToPointer)
    {
        analyze_ok(R"(
        void f() {
            i32* p = null;
        }
    )");
    }

    TEST_F(SemaTest, NullToDoublePointer)
    {
        analyze_ok(R"(
        void f() {
            i32** p = null;
        }
    )");
    }

    TEST_F(SemaTest, AddressOfAndDeref)
    {
        analyze_ok(R"(
        void f() {
            i32 x = 5;
            i32* p = &x;
            i32 y = *p;
        }
    )");
    }

    TEST_F(SemaTest, IntegerArithmetic)
    {
        analyze_ok(R"(
        i32 f() {
            i32 a = 10;
            i32 b = 20;
            return a + b - a * b / a % b;
        }
    )");
    }

    TEST_F(SemaTest, FloatArithmetic)
    {
        analyze_ok(R"(
        f64 f() {
            f64 a = 1.0;
            f64 b = 2.0;
            return a + b * a - b / a;
        }
    )");
    }

    TEST_F(SemaTest, MixedIntegerArithmetic)
    {
        analyze_ok(R"(
        void f() {
            i32 a = 1;
            i64 b = 2;
            i64 c = a + b;
        }
    )");
    }

    TEST_F(SemaTest, UnaryNegate)
    {
        analyze_ok(R"(
        i32 f() {
            i32 x = 5;
            return -x;
        }
    )");
    }

    TEST_F(SemaTest, UnaryNegateFloat)
    {
        analyze_ok(R"(
        f64 f() {
            f64 x = 3.14;
            return -x;
        }
    )");
    }

    TEST_F(SemaTest, UnaryBitNot)
    {
        analyze_ok(R"(
        i32 f() {
            i32 x = 0;
            return ~x;
        }
    )");
    }

    TEST_F(SemaTest, UnaryLogNot)
    {
        analyze_ok(R"(
        bool f() {
            bool x = true;
            return !x;
        }
    )");
    }

    TEST_F(SemaTest, IntegerComparison)
    {
        analyze_ok(R"(
        bool f() {
            i32 a = 1;
            i32 b = 2;
            return a == b;
        }
    )");
    }

    TEST_F(SemaTest, AllComparisonOps)
    {
        analyze_ok(R"(
        void f() {
            i32 a = 1;
            i32 b = 2;
            bool r1 = a == b;
            bool r2 = a != b;
            bool r3 = a < b;
            bool r4 = a <= b;
            bool r5 = a > b;
            bool r6 = a >= b;
        }
    )");
    }

    TEST_F(SemaTest, PointerComparison)
    {
        analyze_ok(R"(
        bool f() {
            i32* a = null;
            i32* b = null;
            return a == b;
        }
    )");
    }

    TEST_F(SemaTest, PointerNullComparison)
    {
        analyze_ok(R"(
        bool f() {
            i32* p = null;
            return p == null;
        }
    )");
    }

    TEST_F(SemaTest, MixedNumericComparison)
    {
        analyze_ok(R"(
        bool f() {
            i32 a = 1;
            i64 b = 2;
            return a < b;
        }
    )");
    }

    TEST_F(SemaTest, LogicalAnd)
    {
        analyze_ok(R"(
        bool f() {
            bool a = true;
            bool b = false;
            return a && b;
        }
    )");
    }

    TEST_F(SemaTest, LogicalOr)
    {
        analyze_ok(R"(
        bool f() {
            bool a = true;
            bool b = false;
            return a || b;
        }
    )");
    }

    TEST_F(SemaTest, BitwiseOps)
    {
        analyze_ok(R"(
        void f() {
            i32 a = 0xFF;
            i32 b = 0x0F;
            i32 r1 = a & b;
            i32 r2 = a | b;
            i32 r3 = a ^ b;
        }
    )");
    }

    TEST_F(SemaTest, ShiftOps)
    {
        analyze_ok(R"(
        void f() {
            i32 a = 1;
            i32 r1 = a << 2;
            i32 r2 = a >> 1;
        }
    )");
    }

    TEST_F(SemaTest, SimpleAssignment)
    {
        analyze_ok(R"(
        void f() {
            i32 x = 0;
            x = 5;
        }
    )");
    }

    TEST_F(SemaTest, CompoundAssignmentAll)
    {
        analyze_ok(R"(
        void f() {
            i32 x = 10;
            x += 1;
            x -= 1;
            x *= 2;
            x /= 2;
            x %= 3;
            x &= 0xFF;
            x |= 0x01;
            x ^= 0x10;
            x <<= 1;
            x >>= 1;
        }
    )");
    }

    TEST_F(SemaTest, AssignWidening)
    {
        analyze_ok(R"(
        void f() {
            i32 a = 1;
            i64 b = 0;
            b = a;
        }
    )");
    }

    TEST_F(SemaTest, TernaryExpr)
    {
        analyze_ok(R"(
        i32 f() {
            bool cond = true;
            return cond ? 1 : 0;
        }
    )");
    }

    TEST_F(SemaTest, TernaryWithSameTypes)
    {
        analyze_ok(R"(
        f64 f() {
            bool c = false;
            f64 a = 1.0;
            f64 b = 2.0;
            return c ? a : b;
        }
    )");
    }

    TEST_F(SemaTest, CastIntToFloat)
    {
        analyze_ok(R"(
        f64 f() {
            i32 x = 42;
            return x as f64;
        }
    )");
    }

    TEST_F(SemaTest, CastFloatToInt)
    {
        analyze_ok(R"(
        i32 f() {
            f64 x = 3.14;
            return x as i32;
        }
    )");
    }

    TEST_F(SemaTest, CastIntNarrowing)
    {
        analyze_ok(R"(
        i8 f() {
            i32 x = 127;
            return x as i8;
        }
    )");
    }

    TEST_F(SemaTest, CastIntToBool)
    {
        analyze_ok(R"(
        bool f() {
            i32 x = 1;
            return x as bool;
        }
    )");
    }

    TEST_F(SemaTest, CastBoolToInt)
    {
        analyze_ok(R"(
        i32 f() {
            bool b = true;
            return b as i32;
        }
    )");
    }

    TEST_F(SemaTest, CastPointerToPointer)
    {
        analyze_ok(R"(
        void f() {
            i32* p = null;
            u8* q = p as u8*;
        }
    )");
    }

    TEST_F(SemaTest, CastPointerToInt)
    {
        analyze_ok(R"(
        u64 f() {
            i32* p = null;
            return p as u64;
        }
    )");
    }

    TEST_F(SemaTest, CastIntToPointer)
    {
        analyze_ok(R"(
        void f() {
            u64 addr = 0;
            i32* p = addr as i32*;
        }
    )");
    }

    TEST_F(SemaTest, ChainedCast)
    {
        analyze_ok(R"(
        i64 f() {
            f64 x = 3.14;
            return x as i32 as i64;
        }
    )");
    }

    TEST_F(SemaTest, FunctionCallNoArgs)
    {
        analyze_ok(R"(
        i32 get() { return 42; }
        void f() {
            i32 x = get();
        }
    )");
    }

    TEST_F(SemaTest, FunctionCallWithArgs)
    {
        analyze_ok(R"(
        i32 add(i32 a, i32 b) { return a + b; }
        void f() {
            i32 x = add(1, 2);
        }
    )");
    }

    TEST_F(SemaTest, FunctionCallImplicitWidening)
    {
        analyze_ok(R"(
        i64 widen(i64 x) { return x; }
        void f() {
            i32 val = 42;
            i64 result = widen(val);
        }
    )");
    }

    TEST_F(SemaTest, FunctionCallVoidReturn)
    {
        analyze_ok(R"(
        void noop() {}
        void f() {
            noop();
        }
    )");
    }

    TEST_F(SemaTest, PublicFunction)
    {
        analyze_ok("public i32 main() { return 0; }");
    }

    TEST_F(SemaTest, StaticFunction)
    {
        analyze_ok("static void helper() {}");
    }

    TEST_F(SemaTest, ExternFunction)
    {
        analyze_ok("extern void external_fn();");
    }

    TEST_F(SemaTest, FunctionForwardReference)
    {
        analyze_ok(R"(
        void caller() { callee(); }
        void callee() {}
    )");
    }

    TEST_F(SemaTest, RecursiveFunction)
    {
        analyze_ok(R"(
        i32 factorial(i32 n) {
            if n <= 1 { return 1; }
            return n * factorial(n - 1);
        }
    )");
    }

    TEST_F(SemaTest, MutuallyRecursiveFunctions)
    {
        analyze_ok(R"(
        bool is_even(i32 n) {
            if n == 0 { return true; }
            return is_odd(n - 1);
        }

        bool is_odd(i32 n) {
            if n == 0 { return false; }
            return is_even(n - 1);
        }
    )");
    }

    TEST_F(SemaTest, EmptyStruct)
    {
        analyze_ok("struct Empty {}");
    }

    TEST_F(SemaTest, StructWithFields)
    {
        analyze_ok(R"(
        struct Point {
            f32 x;
            f32 y;
        }
    )");
    }

    TEST_F(SemaTest, StructFieldAccess)
    {
        analyze_ok(R"(
        struct Point { f32 x; f32 y; }
        f32 f() {
            Point p = { x: 1.0, y: 2.0 };
            return p.x;
        }
    )");
    }

    TEST_F(SemaTest, StructFieldAssignment)
    {
        analyze_ok(R"(
        struct Point { f32 x; f32 y; }
        void f() {
            Point p = { x: 1.0, y: 2.0 };
            p.x = 3.0;
        }
    )");
    }

    TEST_F(SemaTest, StructInitializerAllFields)
    {
        analyze_ok(R"(
        struct Vec2 { f32 x; f32 y; }
        void f() {
            Vec2 v = { x: 1.0, y: 2.0 };
        }
    )");
    }

    TEST_F(SemaTest, NestedStructAccess)
    {
        analyze_ok(R"(
        struct Inner { i32 val; }
        struct Outer { Inner inner; }
        i32 f() {
            Inner i = { val: 42 };
            Outer o = { inner: i };
            return o.inner.val;
        }
    )");
    }

    TEST_F(SemaTest, StructPointerAutoDeref)
    {
        analyze_ok(R"(
        struct Point { f32 x; f32 y; }
        f32 f() {
            Point p = { x: 1.0, y: 2.0 };
            Point* pp = &p;
            return pp.x;
        }
    )");
    }

    TEST_F(SemaTest, PublicStruct)
    {
        analyze_ok(R"(
        public struct Visible { i32 x; }
    )");
    }

    TEST_F(SemaTest, StructUsedBeforeDefinitionInPointer)
    {
        analyze_ok(R"(
        struct Node {
            i32 value;
            Node* next;
        }
    )");
    }

    TEST_F(SemaTest, SimpleUnion)
    {
        analyze_ok(R"(
        union Value {
            i32 int_val;
            f64 float_val;
        }
    )");
    }

    TEST_F(SemaTest, UnionFieldAccess)
    {
        analyze_ok(R"(
        union Value { i32 int_val; f64 float_val; }
        void f() {
            Value v = { int_val: 42 };
            i32 x = v.int_val;
        }
    )");
    }

    TEST_F(SemaTest, SimpleEnum)
    {
        analyze_ok(R"(
        enum Color {
            Red,
            Green,
            Blue
        }
    )");
    }

    TEST_F(SemaTest, EnumWithUnderlying)
    {
        analyze_ok(R"(
        enum Status : i32 {
            Ok = 0,
            Error = 1
        }
    )");
    }

    TEST_F(SemaTest, EnumWithPayload)
    {
        analyze_ok(R"(
        enum Option {
            Some(i32),
            None
        }
    )");
    }

    TEST_F(SemaTest, EnumWithMultiplePayloads)
    {
        analyze_ok(R"(
        enum Result {
            Ok(i32),
            Err(i32, u8)
        }
    )");
    }

    TEST_F(SemaTest, UsingSimpleAlias)
    {
        analyze_ok(R"(
        using ID = u64;
        ID f() {
            ID x = 42;
            return x;
        }
    )");
    }

    TEST_F(SemaTest, UsingPointerAlias)
    {
        analyze_ok(R"(
        using IntPtr = i32*;
        void f() {
            IntPtr p = null;
        }
    )");
    }

    TEST_F(SemaTest, UsingAliasToStruct)
    {
        analyze_ok(R"(
        struct Point { f32 x; f32 y; }
        using Vec2 = Point;
    )");
    }

    TEST_F(SemaTest, IfStmt)
    {
        analyze_ok(R"(
        void f() {
            bool cond = true;
            if cond {
                return;
            }
        }
    )");
    }

    TEST_F(SemaTest, IfElse)
    {
        analyze_ok(R"(
        i32 f() {
            bool cond = true;
            if cond {
                return 1;
            } else {
                return 0;
            }
        }
    )");
    }

    TEST_F(SemaTest, IfElseIf)
    {
        analyze_ok(R"(
        i32 f() {
            i32 x = 5;
            if x > 10 {
                return 2;
            } else if x > 0 {
                return 1;
            } else {
                return 0;
            }
        }
    )");
    }

    TEST_F(SemaTest, WhileLoop)
    {
        analyze_ok(R"(
        void f() {
            i32 x = 10;
            while x > 0 {
                x = x - 1;
            }
        }
    )");
    }

    TEST_F(SemaTest, ForLoop)
    {
        analyze_ok(R"(
        void f() {
            for (i32 i = 0; i < 10; i = i + 1) {
                ;
            }
        }
    )");
    }

    TEST_F(SemaTest, DoWhileLoop)
    {
        analyze_ok(R"(
        void f() {
            i32 x = 10;
            do {
                x = x - 1;
            } while x > 0;
        }
    )");
    }

    TEST_F(SemaTest, BreakInWhile)
    {
        analyze_ok(R"(
        void f() {
            while true {
                break;
            }
        }
    )");
    }

    TEST_F(SemaTest, ContinueInWhile)
    {
        analyze_ok(R"(
        void f() {
            while true {
                continue;
            }
        }
    )");
    }

    TEST_F(SemaTest, BreakInFor)
    {
        analyze_ok(R"(
        void f() {
            for (i32 i = 0; i < 10; i = i + 1) {
                break;
            }
        }
    )");
    }

    TEST_F(SemaTest, BreakInDoWhile)
    {
        analyze_ok(R"(
        void f() {
            do {
                break;
            } while true;
        }
    )");
    }

    TEST_F(SemaTest, NestedLoopBreak)
    {
        analyze_ok(R"(
        void f() {
            while true {
                while true {
                    break;
                }
                break;
            }
        }
    )");
    }

    TEST_F(SemaTest, DeferStatement)
    {
        analyze_ok(R"(
        void close() {}
        void f() {
            defer close();
        }
    )");
    }

    TEST_F(SemaTest, DeferBlock)
    {
        analyze_ok(R"(
        void close() {}
        void free(i32* p) {}
        void f() {
            defer {
                close();
            }
        }
    )");
    }

    TEST_F(SemaTest, MatchIntegerLiteral)
    {
        analyze_ok(R"(
        void f() {
            i32 x = 5;
            match x {
                1 => { return; },
                2 => { return; },
                _ => { return; }
            }
        }
    )");
    }

    TEST_F(SemaTest, MatchWithBinding)
    {
        analyze_ok(R"(
        void f() {
            i32 x = 5;
            match x {
                val => { return; }
            }
        }
    )");
    }

    TEST_F(SemaTest, MatchWildcard)
    {
        analyze_ok(R"(
        void f() {
            i32 x = 5;
            match x {
                _ => { return; }
            }
        }
    )");
    }

    TEST_F(SemaTest, MatchWithGuard)
    {
        analyze_ok(R"(
        void f() {
            i32 x = 5;
            match x {
                n if n > 0 => { return; },
                _ => { return; }
            }
        }
    )");
    }

    TEST_F(SemaTest, MatchEnum)
    {
        analyze_ok(R"(
        enum Option {
            Some(i32),
            None
        }
        void f() {
            Option opt;
            match opt {
                Option::Some(val) => { return; },
                Option::None => { return; }
            }
        }
    )");
    }

    TEST_F(SemaTest, MatchStructPattern)
    {
        analyze_ok(R"(
        struct Point { f32 x; f32 y; }
        void f() {
            Point p = { x: 1.0, y: 2.0 };
            match p {
                Point { x: xv, y: yv } => { return; }
            }
        }
    )");
    }

    TEST_F(SemaTest, MatchStructPatternRest)
    {
        analyze_ok(R"(
        struct Point { f32 x; f32 y; }
        void f() {
            Point p = { x: 1.0, y: 2.0 };
            match p {
                Point { x: xv, .. } => { return; }
            }
        }
    )");
    }

    TEST_F(SemaTest, SizeofBuiltin)
    {
        analyze_ok(R"(
        u64 f() {
            return sizeof(i32);
        }
    )");
    }

    TEST_F(SemaTest, AlignofBuiltin)
    {
        analyze_ok(R"(
        u64 f() {
            return alignof(f64);
        }
    )");
    }

    TEST_F(SemaTest, SizeofStruct)
    {
        analyze_ok(R"(
        struct Point { f32 x; f32 y; }
        u64 f() {
            return sizeof(Point);
        }
    )");
    }

    TEST_F(SemaTest, ArrayIndex)
    {
        analyze_ok(R"(
        void f() {
            i32[10] arr;
            i32 x = arr[0];
        }
    )");
    }

    TEST_F(SemaTest, PointerIndex)
    {
        analyze_ok(R"(
        void f() {
            i32* p = null;
            i32 x = p[0];
        }
    )");
    }

    TEST_F(SemaTest, SliceExpr)
    {
        analyze_ok(R"(
        void f() {
            i32[10] arr;
            []i32 s = arr[1..5];
        }
    )");
    }

    TEST_F(SemaTest, SliceIndex)
    {
        analyze_ok(R"(
        void f() {
            []i32 s;
            i32 x = s[0];
        }
    )");
    }

    TEST_F(SemaTest, SliceLenAndPtr)
    {
        analyze_ok(R"(
        void f() {
            []i32 s;
            u64 length = s.len;
            i32* ptr = s.ptr;
        }
    )");
    }

    TEST_F(SemaTest, ArrayToSliceCoercion)
    {
        analyze_ok(R"(
        void f() {
            i32[10] arr;
            []i32 s = arr;
        }
    )");
    }

    TEST_F(SemaTest, ArrayToPointerCoercion)
    {
        analyze_ok(R"(
        void f() {
            i32[10] arr;
            i32* p = arr;
        }
    )");
    }

    TEST_F(SemaTest, BlockScoping)
    {
        analyze_ok(R"(
        void f() {
            i32 x = 1;
            {
                i32 y = 2;
                i32 z = x + y;
            }
        }
    )");
    }

    TEST_F(SemaTest, ShadowingInNestedBlock)
    {
        analyze_ok(R"(
        void f() {
            i32 x = 1;
            {
                i32 x = 2;
            }
        }
    )");
    }

    TEST_F(SemaTest, ForLoopScopeVariable)
    {
        analyze_ok(R"(
        void f() {
            for (i32 i = 0; i < 10; i = i + 1) {
                i32 x = i;
            }
        }
    )");
    }

    TEST_F(SemaTest, FullProgramSimple)
    {
        analyze_ok(R"(
        module main;

        struct Point {
            f32 x;
            f32 y;
        }

        using Vec2 = Point;

        f32 dot(Point a, Point b) {
            return a.x * b.x + a.y * b.y;
        }

        public i32 main() {
            Point p = { x: 1.0, y: 2.0 };
            Point q = { x: 3.0, y: 4.0 };
            f32 d = dot(p, q);
            return 0;
        }
    )");
    }

    TEST_F(SemaTest, LinkedListStruct)
    {
        analyze_ok(R"(
        struct Node {
            i32 value;
            Node* next;
        }

        void f() {
            Node n = { value: 42, next: null };
            Node* p = &n;
            i32 v = p.value;
        }
    )");
    }

    TEST_F(SemaTest, EnumWithMethodsAndMatch)
    {
        analyze_ok(R"(
        enum Color {
            Red,
            Green,
            Blue
        }

        void f() {
            Color c;
            match c {
                Red => { return; },
                Green => { return; },
                Blue => { return; }
            }
        }
    )");
    }

    TEST_F(SemaTest, ComplexExpressionNesting)
    {
        analyze_ok(R"(
        i32 f() {
            i32 a = 1;
            i32 b = 2;
            i32 c = 3;
            return (a + b) * c - (a / b) % c + ~a & (b | c) ^ a;
        }
    )");
    }

    TEST_F(SemaTest, MultipleReturnsInBranches)
    {
        analyze_ok(R"(
        i32 f(i32 x) {
            if x > 100 {
                return 3;
            } else if x > 10 {
                return 2;
            } else if x > 0 {
                return 1;
            } else {
                return 0;
            }
        }
    )");
    }

    TEST_F(SemaTest, LoopWithAllControlFlow)
    {
        analyze_ok(R"(
        void f() {
            i32 sum = 0;
            for (i32 i = 0; i < 100; i = i + 1) {
                if i % 2 == 0 {
                    continue;
                }
                if i > 50 {
                    break;
                }
                sum += i;
            }
        }
    )");
    }

    TEST_F(SemaTest, DeeplyNestedStructAccess)
    {
        analyze_ok(R"(
        struct A { i32 val; }
        struct B { A a; }
        struct C { B b; }

        i32 f() {
            A a = { val: 42 };
            B b = { a: a };
            C c = { b: b };
            return c.b.a.val;
        }
    )");
    }

    TEST_F(SemaTest, FunctionCallingChain)
    {
        analyze_ok(R"(
        i32 a() { return 1; }
        i32 b(i32 x) { return x + a(); }
        i32 c(i32 x, i32 y) { return b(x) + b(y); }
        i32 f() { return c(1, 2); }
    )");
    }

    TEST_F(SemaTest, CastInComplexExpr)
    {
        analyze_ok(R"(
        i32 f() {
            f64 x = 3.14;
            i32 y = 10;
            return x as i32 + y;
        }
    )");
    }

    TEST_F(SemaTest, MultipleStructsInteracting)
    {
        analyze_ok(R"(
        struct Vec2 { f32 x; f32 y; }
        struct Rect { Vec2 min; Vec2 max; }

        f32 width(Rect r) {
            return r.max.x - r.min.x;
        }

        f32 height(Rect r) {
            return r.max.y - r.min.y;
        }

        void f() {
            Vec2 lo = { x: 0.0, y: 0.0 };
            Vec2 hi = { x: 10.0, y: 5.0 };
            Rect r = { min: lo, max: hi };
            f32 w = width(r);
            f32 h = height(r);
        }
    )");
    }

    TEST_F(SemaTest, StaticIfCompiles)
    {
        analyze_ok(R"(
        void f() {
            static if true {
                i32 x = 42;
            }
        }
    )");
    }

    TEST_F(SemaTest, AmbiguousStarResolvedAsDecl)
    {
        analyze_ok(R"(
        struct Point { f32 x; f32 y; }
        void f() {
            Point * p;
        }
    )");
    }

    TEST_F(SemaTest, AmbiguousStarResolvedAsExpr)
    {
        analyze_ok(R"(
        void f() {
            i32 a = 2;
            i32 b = 3;
            a * b;
        }
    )");
    }

    TEST_F(SemaTest, AmbiguousSingleArgCallResolved)
    {
        analyze_ok(R"(
        void foo(i32 x) {}
        void f() {
            foo(5);
        }
    )");
    }

    TEST_F(SemaTest, ErrorUndeclaredVariable)
    {
        analyze_err(R"(
        void f() {
            i32 x = y;
        }
    )");
    }

    TEST_F(SemaTest, ErrorUndeclaredFunction)
    {
        analyze_err(R"(
        void f() {
            nonexistent();
        }
    )");
    }

    TEST_F(SemaTest, ErrorUndeclaredType)
    {
        analyze_err(R"(
        void f() {
            Nonexistent x;
        }
    )");
    }

    TEST_F(SemaTest, ErrorUseAfterScopeEnd)
    {
        analyze_err(R"(
        void f() {
            {
                i32 x = 1;
            }
            i32 y = x;
        }
    )");
    }

    TEST_F(SemaTest, ErrorRedeclaredVariable)
    {
        analyze_err(R"(
        void f() {
            i32 x = 1;
            i32 x = 2;
        }
    )");
    }

    TEST_F(SemaTest, ErrorRedeclaredStruct)
    {
        analyze_err(R"(
        struct Foo { i32 x; }
        struct Foo { f32 y; }
    )");
    }

    TEST_F(SemaTest, ErrorRedeclaredEnum)
    {
        analyze_err(R"(
        enum A { X }
        enum A { Y }
    )");
    }

    TEST_F(SemaTest, ErrorReturnTypeMismatch)
    {
        analyze_err(R"(
        i32 f() {
            return true;
        }
    )");
    }

    TEST_F(SemaTest, ErrorReturnVoidWithValue)
    {
        analyze_err(R"(
        void f() {
            return 42;
        }
    )");
    }

    TEST_F(SemaTest, ErrorReturnMissingValueNonVoid)
    {
        analyze_err(R"(
        i32 f() {
            return;
        }
    )");
    }

    TEST_F(SemaTest, ErrorAssignTypeMismatch)
    {
        analyze_err(R"(
        void f() {
            i32 x = 0;
            x = true;
        }
    )");
    }

    TEST_F(SemaTest, ErrorVarInitTypeMismatch)
    {
        analyze_err(R"(
        void f() {
            bool x = 42;
        }
    )");
    }

    TEST_F(SemaTest, ErrorNarrowingImplicit)
    {
        analyze_err(R"(
        void f() {
            i64 a = 1;
            i32 b = a;
        }
    )");
    }

    TEST_F(SemaTest, ErrorFloatNarrowing)
    {
        analyze_err(R"(
        void f() {
            f64 a = 1.0;
            f32 b = a;
        }
    )");
    }

    TEST_F(SemaTest, ErrorAddBooleans)
    {
        analyze_err(R"(
        void f() {
            bool a = true;
            bool b = false;
            bool c = a + b;
        }
    )");
    }

    TEST_F(SemaTest, ErrorLogicalOnInt)
    {
        analyze_err(R"(
        void f() {
            i32 a = 1;
            i32 b = 2;
            bool c = a && b;
        }
    )");
    }

    TEST_F(SemaTest, ErrorBitwiseOnFloat)
    {
        analyze_err(R"(
        void f() {
            f64 a = 1.0;
            f64 b = 2.0;
            f64 c = a & b;
        }
    )");
    }

    TEST_F(SemaTest, ErrorLogNotOnInt)
    {
        analyze_err(R"(
        void f() {
            i32 x = 1;
            bool y = !x;
        }
    )");
    }

    TEST_F(SemaTest, ErrorBitNotOnFloat)
    {
        analyze_err(R"(
        void f() {
            f64 x = 1.0;
            f64 y = ~x;
        }
    )");
    }

    TEST_F(SemaTest, ErrorNegateBoolean)
    {
        analyze_err(R"(
        void f() {
            bool x = true;
            bool y = -x;
        }
    )");
    }

    TEST_F(SemaTest, ErrorDerefNonPointer)
    {
        analyze_err(R"(
        void f() {
            i32 x = 5;
            i32 y = *x;
        }
    )");
    }

    TEST_F(SemaTest, ErrorTooFewArgs)
    {
        analyze_err(R"(
        i32 add(i32 a, i32 b) { return a + b; }
        void f() {
            add(1);
        }
    )");
    }

    TEST_F(SemaTest, ErrorTooManyArgs)
    {
        analyze_err(R"(
        i32 add(i32 a, i32 b) { return a + b; }
        void f() {
            add(1, 2, 3);
        }
    )");
    }

    TEST_F(SemaTest, ErrorArgTypeMismatch)
    {
        analyze_err(R"(
        void takes_bool(bool b) {}
        void f() {
            takes_bool(42);
        }
    )");
    }

    TEST_F(SemaTest, ErrorCallNonFunction)
    {
        analyze_err(R"(
        void f() {
            i32 x = 5;
            x();
        }
    )");
    }

    TEST_F(SemaTest, ErrorNoSuchField)
    {
        analyze_err(R"(
        struct Point { f32 x; f32 y; }
        void f() {
            Point p = { x: 1.0, y: 2.0 };
            f32 z = p.z;
        }
    )");
    }

    TEST_F(SemaTest, ErrorMemberOnPrimitive)
    {
        analyze_err(R"(
        void f() {
            i32 x = 5;
            i32 y = x.value;
        }
    )");
    }

    TEST_F(SemaTest, ErrorIndexNonIndexable)
    {
        analyze_err(R"(
        void f() {
            i32 x = 5;
            i32 y = x[0];
        }
    )");
    }

    TEST_F(SemaTest, ErrorIndexWithBool)
    {
        analyze_err(R"(
        void f() {
            i32[10] arr;
            i32 x = arr[true];
        }
    )");
    }

    TEST_F(SemaTest, ErrorCastStructToInt)
    {
        analyze_err(R"(
        struct Point { f32 x; f32 y; }
        void f() {
            Point p = { x: 1.0, y: 2.0 };
            i32 x = p as i32;
        }
    )");
    }

    TEST_F(SemaTest, ErrorCastBoolToFloat)
    {
        analyze_err(R"(
        void f() {
            bool b = true;
            f64 x = b as f64;
        }
    )");
    }

    TEST_F(SemaTest, ErrorAssignToConst)
    {
        analyze_err(R"(
        void f() {
            const i32 x = 5;
            x = 10;
        }
    )");
    }

    TEST_F(SemaTest, ErrorBreakOutsideLoop)
    {
        analyze_err(R"(
        void f() {
            break;
        }
    )");
    }

    TEST_F(SemaTest, ErrorContinueOutsideLoop)
    {
        analyze_err(R"(
        void f() {
            continue;
        }
    )");
    }

    TEST_F(SemaTest, ErrorIfCondNotBool)
    {
        analyze_err(R"(
        void f() {
            if 42 {
                return;
            }
        }
    )");
    }

    TEST_F(SemaTest, ErrorWhileCondNotBool)
    {
        analyze_err(R"(
        void f() {
            while 1 {
                break;
            }
        }
    )");
    }

    TEST_F(SemaTest, ErrorForCondNotBool)
    {
        analyze_err(R"(
        void f() {
            for (; 1; ) {
                break;
            }
        }
    )");
    }

    TEST_F(SemaTest, ErrorDoWhileCondNotBool)
    {
        analyze_err(R"(
        void f() {
            do {
                break;
            } while 0;
        }
    )");
    }

    TEST_F(SemaTest, ErrorTernaryCondNotBool)
    {
        analyze_err(R"(
        i32 f() {
            return 1 ? 2 : 3;
        }
    )");
    }

    TEST_F(SemaTest, ErrorTernaryBranchTypeMismatch)
    {
        analyze_err(R"(
        void f() {
            bool c = true;
            i32 x = c ? 1 : true;
        }
    )");
    }

    TEST_F(SemaTest, ErrorInitNonexistentField)
    {
        analyze_err(R"(
        struct Point { f32 x; f32 y; }
        void f() {
            Point p = { x: 1.0, z: 2.0 };
        }
    )");
    }

    TEST_F(SemaTest, ErrorInitFieldTypeMismatch)
    {
        analyze_err(R"(
        struct Point { f32 x; f32 y; }
        void f() {
            Point p = { x: true, y: 2.0 };
        }
    )");
    }

    TEST_F(SemaTest, ErrorInitNonStruct)
    {
        analyze_err(R"(
        void f() {
            i32 x = { val: 5 };
        }
    )");
    }

    TEST_F(SemaTest, ErrorEnumNonIntUnderlying)
    {
        analyze_err(R"(
        enum Bad : f32 {
            X
        }
    )");
    }

    TEST_F(SemaTest, ErrorMatchGuardNotBool)
    {
        analyze_err(R"(
        void f() {
            i32 x = 5;
            match x {
                n if 42 => { return; }
            }
        }
    )");
    }

    TEST_F(SemaTest, ErrorStructPatternWrongField)
    {
        analyze_err(R"(
        struct Point { f32 x; f32 y; }
        void f() {
            Point p = { x: 1.0, y: 2.0 };
            match p {
                Point { x: xv, z: zv } => { return; }
            }
        }
    )");
    }

    TEST_F(SemaTest, ErrorVarNoTypeNoInit)
    {
        auto r = analyze("void f() { x; }");
        EXPECT_TRUE(r.sema_error);
    }

    TEST_F(SemaTest, ErrorCompareBoolAndInt)
    {
        analyze_err(R"(
        void f() {
            bool a = true;
            i32 b = 1;
            bool c = a == b;
        }
    )");
    }

    TEST_F(SemaTest, ErrorCompareStructs)
    {
        analyze_err(R"(
        struct A { i32 x; }
        struct B { i32 x; }
        void f() {
            A a = { x: 1 };
            B b = { x: 1 };
            bool eq = a == b;
        }
    )");
    }

    TEST_F(SemaTest, ErrorSliceNonSliceable)
    {
        analyze_err(R"(
        void f() {
            i32 x = 5;
            []i32 s = x[1..3];
        }
    )");
    }

    TEST_F(SemaTest, ErrorSliceIndexNotInt)
    {
        analyze_err(R"(
        void f() {
            i32[10] arr;
            []i32 s = arr[true..5];
        }
    )");
    }

    TEST_F(SemaTest, ErrorIncrementFloat)
    {
        analyze_ok(R"(
        void f() {
            i32 x = 0;
            x += 1;
        }
    )");
    }

    TEST_F(SemaTest, QueryBoolLiteralType)
    {
        auto r = analyze_full(R"(
        bool f() { return true; }
    )");
        ASSERT_TRUE(r.ok) << r.diagnostics;

        auto* fn = as<ast::FunctionDecl>(top_decl(r.tu, 0));
        auto* ret = as<ast::ReturnStmt>(func_stmt(fn, 0));
        auto* ty = r.sema->type_of(ret->value());
        ASSERT_NE(ty, nullptr);
        EXPECT_TRUE(ty->is_bool());
    }

    TEST_F(SemaTest, QueryNullLiteralType)
    {
        auto r = analyze_full(R"(
        void f() {
            i32* p = null;
        }
    )");
        ASSERT_TRUE(r.ok) << r.diagnostics;
    }

    TEST_F(SemaTest, QueryStringLiteralType)
    {
        auto r = analyze_full(R"(
        void f() {
            return "hello";
        }
    )");
        auto r2 = analyze_full(R"(
        u8* f() { return "hello"; }
    )");
    }

    TEST_F(SemaTest, QueryComparisonResultIsBool)
    {
        auto r = analyze_full(R"(
        bool f() {
            i32 a = 1;
            i32 b = 2;
            return a < b;
        }
    )");
        ASSERT_TRUE(r.ok) << r.diagnostics;

        auto* fn = as<ast::FunctionDecl>(top_decl(r.tu, 0));
        auto* ret = as<ast::ReturnStmt>(func_stmt(fn, 2));
        auto* ty = r.sema->type_of(ret->value());
        ASSERT_NE(ty, nullptr);
        EXPECT_TRUE(ty->is_bool());
    }

    TEST_F(SemaTest, QuerySizeofResultType)
    {
        auto r = analyze_full(R"(
        u64 f() { return sizeof(i32); }
    )");
        ASSERT_TRUE(r.ok) << r.diagnostics;

        auto* fn = as<ast::FunctionDecl>(top_decl(r.tu, 0));
        auto* ret = as<ast::ReturnStmt>(func_stmt(fn, 0));
        auto* ty = r.sema->type_of(ret->value());
        ASSERT_NE(ty, nullptr);
        EXPECT_TRUE(ty->is_integer());
        auto* int_ty = dynamic_cast<sema::IntegerType*>(ty);
        ASSERT_NE(int_ty, nullptr);
        EXPECT_EQ(int_ty->width(), 64);
        EXPECT_FALSE(int_ty->is_signed());
    }

    TEST_F(SemaTest, QuerySymbolLookup)
    {
        auto r = analyze_full(R"(
        struct Point { f32 x; f32 y; }
        void f() {
            Point p = { x: 1.0, y: 2.0 };
        }
    )");
        ASSERT_TRUE(r.ok) << r.diagnostics;

        auto* sd = as<ast::StructDecl>(top_decl(r.tu, 0));
        auto* sym = r.sema->symbol_of(sd);
        std::ignore = sym;
    }

    TEST_F(SemaTest, EmptyStructUsable)
    {
        analyze_ok(R"(
        struct Empty {}
        void f() {
            Empty e;
        }
    )");
    }

    TEST_F(SemaTest, VoidFunctionCalledAsStatement)
    {
        analyze_ok(R"(
        void noop() {}
        void f() { noop(); }
    )");
    }

    TEST_F(SemaTest, NestedFunctionCalls)
    {
        analyze_ok(R"(
        i32 a() { return 1; }
        i32 b(i32 x) { return x; }
        void f() {
            i32 r = b(a());
        }
    )");
    }

    TEST_F(SemaTest, AssignToFieldThroughPointer)
    {
        analyze_ok(R"(
        struct S { i32 val; }
        void f() {
            S s = { val: 0 };
            S* p = &s;
            p.val = 42;
        }
    )");
    }

    TEST_F(SemaTest, CompoundAssignToField)
    {
        analyze_ok(R"(
        struct Counter { i32 val; }
        void f() {
            Counter c = { val: 0 };
            c.val += 1;
        }
    )");
    }

    TEST_F(SemaTest, MultipleEnumsCoexist)
    {
        analyze_ok(R"(
        enum A { X, Y }
        enum B { P, Q }
    )");
    }

    TEST_F(SemaTest, StructAndFunctionSameScopeLevel)
    {
        analyze_ok(R"(
        struct Data { i32 x; }

        Data make() {
            Data d = { x: 42 };
            return d;
        }

        i32 use(Data d) {
            return d.x;
        }
    )");
    }

    TEST_F(SemaTest, WhileTrueLoop)
    {
        analyze_ok(R"(
        void f() {
            while true {
                break;
            }
        }
    )");
    }

    TEST_F(SemaTest, EmptyForLoop)
    {
        analyze_ok(R"(
        void f() {
            for (;;) {
                break;
            }
        }
    )");
    }

    TEST_F(SemaTest, NullComparisonBothDirections)
    {
        analyze_ok(R"(
        void f() {
            i32* p = null;
            bool a = p == null;
            bool b = null == p;
        }
    )");
    }

    TEST_F(SemaTest, CastPreservesValue)
    {
        analyze_ok(R"(
        void f() {
            i32 x = 42;
            i64 y = x as i64;
            i32 z = y as i32;
        }
    )");
    }

    TEST_F(SemaTest, ConstantExpressionInArraySize)
    {
        analyze_ok(R"(
        void f() {
            i32[100] arr;
        }
    )");
    }

    TEST_F(SemaTest, EnumDiscriminantValues)
    {
        analyze_ok(R"(
        enum Flags : u32 {
            A = 1,
            B = 2,
            C = 4
        }
    )");
    }

    TEST_F(SemaTest, UsingMultipleAliases)
    {
        analyze_ok(R"(
        using Byte = u8;
        using Word = u16;
        using DWord = u32;
        using QWord = u64;

        void f() {
            Byte  a = 0;
            Word  b = 0;
            DWord c = 0;
            QWord d = 0;
        }
    )");
    }

    TEST_F(SemaTest, ChainedFieldAccess)
    {
        analyze_ok(R"(
        struct Inner { i32 val; }
        struct Middle { Inner inner; }
        struct Outer { Middle mid; }

        i32 f() {
            Inner i = { val: 10 };
            Middle m = { inner: i };
            Outer o = { mid: m };
            return o.mid.inner.val;
        }
    )");
    }

    TEST_F(SemaTest, PointerToPointerDeref)
    {
        analyze_ok(R"(
        void f() {
            i32 x = 42;
            i32* p = &x;
            i32** pp = &p;
            i32 y = **pp;
        }
    )");
    }

    TEST_F(SemaTest, BooleanExpressionsInConditions)
    {
        analyze_ok(R"(
        void f() {
            i32 a = 1;
            i32 b = 2;
            bool c = true;

            if a > 0 && b < 10 || c {
                return;
            }
        }
    )");
    }

    TEST_F(SemaTest, ExprStmtDiscardResult)
    {
        analyze_ok(R"(
        i32 compute() { return 42; }
        void f() {
            compute();
        }
    )");
    }

    TEST_F(SemaTest, ModuleDeclaration)
    {
        analyze_ok(R"(
        module my.app;

        void f() {}
    )");
    }

    TEST_F(SemaTest, PublicAndPrivateDecls)
    {
        analyze_ok(R"(
        public struct Pub { i32 x; }
        struct Priv { i32 y; }
        public void pub_fn() {}
        void priv_fn() {}
    )");
    }

    TEST_F(SemaTest, ManyVariableDecls)
    {
        analyze_ok(R"(
        void f() {
            i32 a0 = 0; i32 a1 = 1; i32 a2 = 2; i32 a3 = 3; i32 a4 = 4;
            i32 a5 = 5; i32 a6 = 6; i32 a7 = 7; i32 a8 = 8; i32 a9 = 9;
            i32 sum = a0 + a1 + a2 + a3 + a4 + a5 + a6 + a7 + a8 + a9;
        }
    )");
    }

    TEST_F(SemaTest, ManyFunctions)
    {
        analyze_ok(R"(
        void f0() {}
        void f1() { f0(); }
        void f2() { f1(); }
        void f3() { f2(); }
        void f4() { f3(); }
        void f5() { f4(); }
        void f6() { f5(); }
        void f7() { f6(); }
        void f8() { f7(); }
        void f9() { f8(); }
    )");
    }

    TEST_F(SemaTest, ManyStructs)
    {
        analyze_ok(R"(
        struct S0 { i32 x; }
        struct S1 { S0 s; }
        struct S2 { S1 s; }
        struct S3 { S2 s; }
        struct S4 { S3 s; }
    )");
    }

    TEST_F(SemaTest, ErrorDoesNotPreventOtherChecking)
    {
        auto r = analyze(R"(
        void f() {
            i32 x = undeclared_thing;
            i32 y = 42;
            i32 z = y + 1;
        }
    )");
        EXPECT_TRUE(r.sema_error);
    }

    TEST_F(SemaTest, MultipleErrorsReported)
    {
        auto r = analyze(R"(
        void f() {
            i32 x = undeclared1;
            i32 y = undeclared2;
        }
    )");
        EXPECT_TRUE(r.sema_error);

        auto count = 0u;
        auto pos = r.diagnostics.find("error");
        while (pos != std::string::npos)
        {
            ++count;
            pos = r.diagnostics.find("error", pos + 1);
        }

        EXPECT_GE(count, 2u) << "Expected at least 2 errors in:\n" << r.diagnostics;
    }

} // namespace dcc::test
