#include <ast/ambiguous.hh>
#include <ast/common.hh>
#include <ast/decl.hh>
#include <ast/expr.hh>
#include <ast/pattern.hh>
#include <ast/stmt.hh>
#include <ast/type.hh>
#include <diagnostics.hh>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <lex/lexer.hh>
#include <parse/arena.hh>
#include <parse/parser.hh>
#include <sema/sema.hh>
#include <sstream>

namespace dcc::test
{
    class ModuleImportTest : public ::testing::Test
    {
    protected:
        sm::SourceManager sm;
        si::StringInterner interner;
        parse::AstArena arena;
        std::ostringstream diag_out;
        std::filesystem::path tmp_dir;

        void SetUp() override
        {
            tmp_dir = std::filesystem::temp_directory_path() / "dcc_module_test";
            std::filesystem::create_directories(tmp_dir);
        }

        void TearDown() override
        {
            std::error_code ec;
            std::filesystem::remove_all(tmp_dir, ec);
        }

        void write_module(const std::filesystem::path& rel_path, std::string_view source)
        {
            auto full = tmp_dir / rel_path;
            std::filesystem::create_directories(full.parent_path());
            std::ofstream ofs{full};
            ASSERT_TRUE(ofs.is_open()) << "Failed to create: " << full;
            ofs << source;
        }

        struct Result
        {
            bool ok;
            std::string diagnostics;
            std::unique_ptr<sema::Sema> sema;
            ast::TranslationUnit* tu;
        };

        Result analyze_with_modules(std::string_view main_source)
        {
            diag_out.str({});
            diag_out.clear();

            auto fid = sm.add_synthetic("main.dcc", std::string(main_source));
            auto* file = sm.get(fid);
            lex::Lexer lexer{*file, interner};
            diag::DiagnosticPrinter printer{sm, diag_out};
            printer.set_color(false);
            parse::Parser parser{lexer, arena, printer};
            auto* tu = parser.parse();

            if (parser.had_error() || !tu)
                return {false, diag_out.str(), nullptr, tu};

            auto sema_ptr = std::make_unique<sema::Sema>(sm, interner, printer);
            sema_ptr->add_search_path(tmp_dir);

            sema_ptr->set_parse_callback([this, &printer](const std::filesystem::path& file_path, sm::SourceManager& src_mgr) -> ast::TranslationUnit* {
                auto mod_fid = src_mgr.load(file_path);
                if (!mod_fid)
                    return nullptr;

                auto* mod_file = src_mgr.get(*mod_fid);
                if (!mod_file)
                    return nullptr;

                lex::Lexer mod_lexer{*mod_file, interner};
                parse::Parser mod_parser{mod_lexer, arena, printer};
                auto* mod_tu = mod_parser.parse();
                if (mod_parser.had_error())
                    return nullptr;

                return mod_tu;
            });

            bool ok = sema_ptr->analyze(*tu);
            return {ok, diag_out.str(), std::move(sema_ptr), tu};
        }

        Result analyze_ok_modules(std::string_view main_source)
        {
            auto r = analyze_with_modules(main_source);
            EXPECT_TRUE(r.ok) << "Expected success.\nDiagnostics:\n" << r.diagnostics;
            return r;
        }

        void analyze_err_modules(std::string_view main_source)
        {
            auto r = analyze_with_modules(main_source);
            EXPECT_FALSE(r.ok) << "Expected failure for:\n" << main_source;
        }
    };

    TEST_F(ModuleImportTest, QualifiedFunctionCall)
    {
        write_module("math.dcc", R"(
            module math;
            public i32 add(i32 a, i32 b) { return a + b; }
        )");

        analyze_ok_modules(R"(
            module main;
            import math;

            i32 f() {
                return math.add(1, 2);
            }
        )");
    }

    TEST_F(ModuleImportTest, QualifiedStructAccess)
    {
        write_module("geom.dcc", R"(
            module geom;
            public struct Point { f32 x; f32 y; }
        )");

        analyze_ok_modules(R"(
            module main;
            import geom;

            void f() {
                geom.Point p = { x: 1.0, y: 2.0 };
                f32 xv = p.x;
            }
        )");
    }

    TEST_F(ModuleImportTest, QualifiedEnumAccess)
    {
        write_module("color.dcc", R"(
            module color;
            public enum Color { Red, Green, Blue }
        )");

        analyze_ok_modules(R"(
            module main;
            import color;

            void f() {
                color.Color c;
            }
        )");
    }

    TEST_F(ModuleImportTest, UsingImportFunction)
    {
        write_module("math.dcc", R"(
            module math;
            public i32 square(i32 x) { return x * x; }
        )");

        analyze_ok_modules(R"(
            module main;
            import math;
            using math.square;

            i32 f() {
                return square(5);
            }
        )");
    }

    TEST_F(ModuleImportTest, UsingImportStruct)
    {
        write_module("geom.dcc", R"(
            module geom;
            public struct Vec2 { f32 x; f32 y; }
        )");

        analyze_ok_modules(R"(
            module main;
            import geom;
            using geom.Vec2;

            void f() {
                Vec2 v = { x: 3.0, y: 4.0 };
            }
        )");
    }

    TEST_F(ModuleImportTest, UsingImportMultipleSymbols)
    {
        write_module("math.dcc", R"(
            module math;
            public i32 add(i32 a, i32 b) { return a + b; }
            public i32 mul(i32 a, i32 b) { return a * b; }
        )");

        analyze_ok_modules(R"(
            module main;
            import math;
            using math.add;
            using math.mul;

            i32 f() {
                return add(mul(2, 3), 4);
            }
        )");
    }

    TEST_F(ModuleImportTest, SymbolAliasForFunction)
    {
        write_module("math.dcc", R"(
            module math;
            public i32 add(i32 a, i32 b) { return a + b; }
        )");

        analyze_ok_modules(R"(
            module main;
            import math;
            using plus = math.add;

            i32 f() {
                return plus(1, 2);
            }
        )");
    }

    TEST_F(ModuleImportTest, SymbolAliasForType)
    {
        write_module("geom.dcc", R"(
            module geom;
            public struct Point { f32 x; f32 y; }
        )");

        analyze_ok_modules(R"(
            module main;
            import geom;
            using Pos = geom.Point;

            void f() {
                Pos p = { x: 1.0, y: 2.0 };
            }
        )");
    }

    TEST_F(ModuleImportTest, UsingExportReexportsSymbol)
    {
        write_module("core.dcc", R"(
            module core;
            public struct CoreData { i32 val; }
        )");

        write_module("facade.dcc", R"(
            module facade;
            import core;
            using public core.CoreData;

            public i32 extract(CoreData d) {
                return d.val;
            }
        )");

        analyze_ok_modules(R"(
            module main;
            import facade;
            using facade.CoreData;
            using facade.extract;

            void f() {
                CoreData d = { val: 99 };
                i32 v = extract(d);
            }
        )");
    }

    TEST_F(ModuleImportTest, TypeAliasStillWorks)
    {
        analyze_ok_modules(R"(
            module main;
            using ID = u64;

            ID f() {
                ID x = 42;
                return x;
            }
        )");
    }

    TEST_F(ModuleImportTest, TypeAliasExportStillWorks)
    {
        write_module("types.dcc", R"(
            module types;
            public using public ID = u64;
        )");

        analyze_ok_modules(R"(
            module main;
            import types;
            using types.ID;

            void f() {
                ID x = 42;
            }
        )");
    }
    TEST_F(ModuleImportTest, MixedQualifiedAndShort)
    {
        write_module("util.dcc", R"(
            module util;

            public struct Pair { i32 first; i32 second; }
            public i32 sum_pair(Pair p) { return p.first + p.second; }
        )");

        analyze_ok_modules(R"(
            module main;
            import util;
            using util.Pair;

            i32 f() {
                Pair p = { first: 10, second: 20 };
                return util.sum_pair(p);
            }
        )");
    }

    TEST_F(ModuleImportTest, NestedModuleQualifiedAccess)
    {
        std::filesystem::create_directories(tmp_dir / "collections");
        write_module("collections/list.dcc", R"(
            module collections.list;

            public struct ListNode {
                i32 data;
                ListNode* next;
            }

            public ListNode make_node(i32 val) {
                ListNode n = { data: val, next: null };
                return n;
            }
        )");

        analyze_ok_modules(R"(
            module main;
            import collections.list;

            void f() {
                collections.list.ListNode n = collections.list.make_node(10);
                i32 d = n.data;
            }
        )");
    }

    TEST_F(ModuleImportTest, NestedModuleWithUsing)
    {
        std::filesystem::create_directories(tmp_dir / "collections");
        write_module("collections/list.dcc", R"(
            module collections.list;

            public struct ListNode {
                i32 data;
                ListNode* next;
            }
        )");

        analyze_ok_modules(R"(
            module main;
            import collections.list;
            using collections.list.ListNode;

            void f() {
                ListNode n = { data: 10, next: null };
            }
        )");
    }

    TEST_F(ModuleImportTest, TransitiveImportChain)
    {
        write_module("base.dcc", R"(
            module base;
            public struct Id { u64 val; }
        )");

        write_module("entity.dcc", R"(
            module entity;
            import base;
            using base.Id;

            public struct Entity {
                Id id;
                i32 health;
            }
        )");

        analyze_ok_modules(R"(
            module main;
            import base;
            import entity;
            using base.Id;
            using entity.Entity;

            void f() {
                Id eid = { val: 1 };
                Entity e = { id: eid, health: 100 };
            }
        )");
    }

    TEST_F(ModuleImportTest, DiamondDependency)
    {
        write_module("base.dcc", R"(
            module base;
            public struct Coord { f32 x; f32 y; }
        )");

        write_module("renderer.dcc", R"(
            module renderer;
            import base;
            using base.Coord;

            public void draw(Coord c) {}
        )");

        write_module("physics.dcc", R"(
            module physics;
            import base;
            using base.Coord;

            public Coord step(Coord c) {
                Coord next = { x: c.x, y: c.y };
                return next;
            }
        )");

        analyze_ok_modules(R"(
            module main;
            import base;
            import renderer;
            import physics;
            using base.Coord;
            using renderer.draw;
            using physics.step;

            void f() {
                Coord c = { x: 0.0, y: 0.0 };
                Coord next = step(c);
                draw(next);
            }
        )");
    }

    TEST_F(ModuleImportTest, ErrorBareNameWithoutUsing)
    {
        write_module("math.dcc", R"(
            module math;
            public i32 add(i32 a, i32 b) { return a + b; }
        )");

        analyze_err_modules(R"(
            module main;
            import math;

            i32 f() {
                return add(1, 2);
            }
        )");
    }

    TEST_F(ModuleImportTest, ErrorBareTypeWithoutUsing)
    {
        write_module("geom.dcc", R"(
            module geom;
            public struct Point { f32 x; f32 y; }
        )");

        analyze_err_modules(R"(
            module main;
            import geom;

            void f() {
                Point p = { x: 1.0, y: 2.0 };
            }
        )");
    }

    TEST_F(ModuleImportTest, ErrorImportNonexistentModule)
    {
        analyze_err_modules(R"(
            module main;
            import nonexistent;
            void f() {}
        )");
    }

    TEST_F(ModuleImportTest, ErrorUsingWithoutImport)
    {
        analyze_err_modules(R"(
            module main;
            using math.add;
            void f() {}
        )");
    }

    TEST_F(ModuleImportTest, ErrorUsePrivateSymbol)
    {
        write_module("secret.dcc", R"(
            module secret;
            void hidden() {}
            public void visible() {}
        )");

        analyze_err_modules(R"(
            module main;
            import secret;
            using secret.hidden;
            void f() { hidden(); }
        )");
    }

    TEST_F(ModuleImportTest, ErrorCircularImport)
    {
        write_module("a.dcc", R"(
            module a;
            import b;
            public i32 from_a() { return 1; }
        )");

        write_module("b.dcc", R"(
            module b;
            import a;
            public i32 from_b() { return 2; }
        )");

        analyze_err_modules(R"(
            module main;
            import a;
            void f() { i32 x = a.from_a(); }
        )");
    }

    TEST_F(ModuleImportTest, ErrorWrongArgTypesCrossModule)
    {
        write_module("math.dcc", R"(
            module math;
            public i32 add(i32 a, i32 b) { return a + b; }
        )");

        analyze_err_modules(R"(
            module main;
            import math;
            using math.add;

            void f() {
                add(true, false);
            }
        )");
    }

    TEST_F(ModuleImportTest, ErrorTypeMismatchCrossModule)
    {
        write_module("types.dcc", R"(
            module types;
            public struct Foo { i32 x; }
        )");

        write_module("other.dcc", R"(
            module other;
            public struct Bar { i32 x; }
        )");

        analyze_err_modules(R"(
            module main;
            import types;
            import other;
            using types.Foo;
            using other.Bar;

            void f() {
                Foo a = { x: 1 };
                Bar b = a;
            }
        )");
    }

    TEST_F(ModuleImportTest, PointerToQualifiedStruct)
    {
        write_module("node.dcc", R"(
            module node;

            public struct Node {
                i32 value;
                Node* next;
            }
        )");

        analyze_ok_modules(R"(
            module main;
            import node;
            using node.Node;

            void f() {
                Node n = { value: 42, next: null };
                Node* p = &n;
                i32 v = p.value;
            }
        )");
    }

    TEST_F(ModuleImportTest, SizeofImportedType)
    {
        write_module("data.dcc", R"(
            module data;
            public struct Packet { u32 header; u64 payload; }
        )");

        analyze_ok_modules(R"(
            module main;
            import data;
            using data.Packet;

            u64 f() {
                return sizeof(Packet);
            }
        )");
    }

    TEST_F(ModuleImportTest, MultipleModulesMultipleUsings)
    {
        write_module("math.dcc", R"(
            module math;
            public i32 add(i32 a, i32 b) { return a + b; }
        )");

        write_module("io.dcc", R"(
            module io;
            public void print_i32(i32 val) {}
        )");

        write_module("types.dcc", R"(
            module types;
            public using Size = u64;
        )");

        analyze_ok_modules(R"(
            module main;
            import math;
            import io;
            import types;
            using math.add;
            using io.print_i32;
            using types.Size;

            void f() {
                i32 result = add(2, 3);
                print_i32(result);
                Size sz = 1024;
            }
        )");
    }

    TEST_F(ModuleImportTest, UsingExportChain)
    {
        write_module("core.dcc", R"(
            module core;
            public struct Vec2 { f32 x; f32 y; }
        )");

        write_module("graphics.dcc", R"(
            module graphics;
            import core;
            using public core.Vec2;

            public void draw(Vec2 v) {}
        )");

        analyze_ok_modules(R"(
            module main;
            import graphics;
            using graphics.Vec2;
            using graphics.draw;

            void f() {
                Vec2 v = { x: 1.0, y: 2.0 };
                draw(v);
            }
        )");
    }

    TEST_F(ModuleImportTest, SymbolAliasRename)
    {
        write_module("verbose.dcc", R"(
            module verbose;
            public i32 very_long_function_name(i32 x) { return x * 2; }
        )");

        analyze_ok_modules(R"(
            module main;
            import verbose;
            using dbl = verbose.very_long_function_name;

            i32 f() {
                return dbl(21);
            }
        )");
    }

    TEST_F(ModuleImportTest, EnumVariantsQualifiedMatch)
    {
        write_module("option.dcc", R"(
            module option;

            public enum Option {
                Some(i32),
                None
            }
        )");

        analyze_ok_modules(R"(
            module main;
            import option;
            using option.Option;

            void f() {
                Option opt;
                match opt {
                    Option::Some(val) => { return; },
                    Option::None => { return; }
                }
            }
        )");
    }

    TEST_F(ModuleImportTest, UnionViaQualifiedImport)
    {
        write_module("variant.dcc", R"(
            module variant;
            public union Value { i32 int_val; f64 float_val; }
        )");

        analyze_ok_modules(R"(
            module main;
            import variant;
            using variant.Value;

            void f() {
                Value v = { int_val: 42 };
                i32 x = v.int_val;
            }
        )");
    }

    TEST_F(ModuleImportTest, GroupImportFunctions)
    {
        write_module("math.dcc", R"(
            module math;
            public i32 add(i32 a, i32 b) { return a + b; }
            public i32 sub(i32 a, i32 b) { return a - b; }
            public i32 mul(i32 a, i32 b) { return a * b; }
        )");

        analyze_ok_modules(R"(
            module main;
            import math;
            using math.{add, sub, mul};

            i32 f() {
                return add(mul(2, 3), sub(10, 4));
            }
        )");
    }

    TEST_F(ModuleImportTest, GroupImportTypes)
    {
        write_module("geom.dcc", R"(
            module geom;
            public struct Point { f32 x; f32 y; }
            public struct Rect { f32 x; f32 y; f32 w; f32 h; }
        )");

        analyze_ok_modules(R"(
            module main;
            import geom;
            using geom.{Point, Rect};

            void f() {
                Point p = { x: 1.0, y: 2.0 };
                Rect r = { x: 0.0, y: 0.0, w: 10.0, h: 5.0 };
            }
        )");
    }

    TEST_F(ModuleImportTest, GroupImportMixedSymbols)
    {
        write_module("lib.dcc", R"(
            module lib;
            public struct Data { i32 val; }
            public i32 process(Data d) { return d.val; }
            public using ID = u64;
        )");

        analyze_ok_modules(R"(
            module main;
            import lib;
            using lib.{Data, process, ID};

            void f() {
                Data d = { val: 42 };
                i32 v = process(d);
                ID id = 100;
            }
        )");
    }

    TEST_F(ModuleImportTest, GroupImportSingleItem)
    {
        write_module("math.dcc", R"(
            module math;
            public i32 add(i32 a, i32 b) { return a + b; }
        )");

        analyze_ok_modules(R"(
            module main;
            import math;
            using math.{add};

            i32 f() {
                return add(1, 2);
            }
        )");
    }

    TEST_F(ModuleImportTest, GroupImportWithTrailingComma)
    {
        write_module("math.dcc", R"(
            module math;
            public i32 add(i32 a, i32 b) { return a + b; }
            public i32 sub(i32 a, i32 b) { return a - b; }
        )");

        analyze_ok_modules(R"(
            module main;
            import math;
            using math.{add, sub,};

            i32 f() {
                return add(1, sub(3, 2));
            }
        )");
    }

    TEST_F(ModuleImportTest, GroupImportNestedModule)
    {
        std::filesystem::create_directories(tmp_dir / "collections");
        write_module("collections/list.dcc", R"(
            module collections.list;

            public struct ListNode {
                i32 data;
                ListNode* next;
            }

            public ListNode make_node(i32 val) {
                ListNode n = { data: val, next: null };
                return n;
            }
        )");

        analyze_ok_modules(R"(
            module main;
            import collections.list;
            using collections.list.{ListNode, make_node};

            void f() {
                ListNode n = make_node(10);
                i32 d = n.data;
            }
        )");
    }

    TEST_F(ModuleImportTest, GroupImportWithExport)
    {
        write_module("core.dcc", R"(
            module core;
            public struct Vec2 { f32 x; f32 y; }
            public struct Vec3 { f32 x; f32 y; f32 z; }
        )");

        write_module("graphics.dcc", R"(
            module graphics;
            import core;
            using public core.{Vec2, Vec3};

            public void draw2d(Vec2 v) {}
            public void draw3d(Vec3 v) {}
        )");

        analyze_ok_modules(R"(
            module main;
            import graphics;
            using graphics.{Vec2, Vec3, draw2d, draw3d};

            void f() {
                Vec2 v2 = { x: 1.0, y: 2.0 };
                Vec3 v3 = { x: 1.0, y: 2.0, z: 3.0 };
                draw2d(v2);
                draw3d(v3);
            }
        )");
    }

    TEST_F(ModuleImportTest, GroupImportEnumAndStruct)
    {
        write_module("types.dcc", R"(
            module types;

            public enum Color { Red, Green, Blue }
            public struct Pixel { i32 x; i32 y; }
        )");

        analyze_ok_modules(R"(
            module main;
            import types;
            using types.{Color, Pixel};

            void f() {
                Color c;
                Pixel px = { x: 10, y: 20 };
            }
        )");
    }

    TEST_F(ModuleImportTest, ErrorGroupImportNonexistentSymbol)
    {
        write_module("math.dcc", R"(
            module math;
            public i32 add(i32 a, i32 b) { return a + b; }
        )");

        analyze_err_modules(R"(
            module main;
            import math;
            using math.{add, nonexistent};

            void f() {
                add(1, 2);
            }
        )");
    }

    TEST_F(ModuleImportTest, ErrorGroupImportPrivateSymbol)
    {
        write_module("lib.dcc", R"(
            module lib;
            public i32 pub_fn() { return 1; }
            i32 priv_fn() { return 2; }
        )");

        analyze_err_modules(R"(
            module main;
            import lib;
            using lib.{pub_fn, priv_fn};

            void f() {
                pub_fn();
            }
        )");
    }

    TEST_F(ModuleImportTest, GroupImportAndQualifiedMixed)
    {
        write_module("util.dcc", R"(
            module util;
            public struct Pair { i32 first; i32 second; }
            public i32 sum_pair(Pair p) { return p.first + p.second; }
            public i32 max_pair(Pair p) { return p.first > p.second ? p.first : p.second; }
        )");

        analyze_ok_modules(R"(
            module main;
            import util;
            using util.{Pair, sum_pair};

            i32 f() {
                Pair p = { first: 10, second: 20 };
                i32 s = sum_pair(p);
                i32 m = util.max_pair(p);
                return s + m;
            }
        )");
    }

    TEST_F(ModuleImportTest, MultipleGroupImports)
    {
        write_module("math.dcc", R"(
            module math;
            public i32 add(i32 a, i32 b) { return a + b; }
            public i32 sub(i32 a, i32 b) { return a - b; }
        )");

        write_module("io.dcc", R"(
            module io;
            public void print_i32(i32 val) {}
            public void print_bool(bool val) {}
        )");

        analyze_ok_modules(R"(
            module main;
            import math;
            import io;
            using math.{add, sub};
            using io.{print_i32, print_bool};

            void f() {
                i32 result = add(1, sub(5, 3));
                print_i32(result);
                print_bool(true);
            }
        )");
    }

} // namespace dcc::test
