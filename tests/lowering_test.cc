#include <ast/ambiguous.hh>
#include <ast/common.hh>
#include <ast/decl.hh>
#include <ast/expr.hh>
#include <ast/pattern.hh>
#include <ast/stmt.hh>
#include <ast/type.hh>
#include <diagnostics.hh>
#include <gtest/gtest.h>
#include <ir/ir.hh>
#include <ir/lowering.hh>
#include <lex/lexer.hh>
#include <parse/arena.hh>
#include <parse/parser.hh>
#include <sema/sema.hh>
#include <sstream>
#include <string>

namespace dcc::test
{
    class IRLoweringTest : public ::testing::Test
    {
    protected:
        sm::SourceManager sm;
        si::StringInterner interner;
        parse::AstArena arena;
        std::ostringstream diag_out;

        struct LowerResult
        {
            std::unique_ptr<ir::Module> module;
            bool ok{false};
            std::string diagnostics;
            std::string ir_text;
        };

        LowerResult lower(std::string_view source)
        {
            auto fid = sm.add_synthetic("test.dcc", std::string(source));
            auto* file = sm.get(fid);
            lex::Lexer lexer{*file, interner};
            diag::DiagnosticPrinter printer{sm, diag_out};
            printer.set_color(false);
            parse::Parser parser{lexer, arena, printer};
            auto* tu = parser.parse();

            if (parser.had_error() || !tu)
                return {nullptr, false, diag_out.str(), {}};

            sema::Sema sema{sm, interner, printer};
            if (!sema.analyze(*tu))
                return {nullptr, false, diag_out.str(), {}};

            ir::IRLowering lowering{sema, printer};
            auto mod = lowering.lower(*tu);
            auto ir_text = mod ? ir::print_module(*mod) : std::string{};

            return {std::move(mod), true, diag_out.str(), std::move(ir_text)};
        }

        ir::Module* lower_ok(std::string_view source)
        {
            static std::vector<std::unique_ptr<ir::Module>> storage;
            auto r = lower(source);
            EXPECT_TRUE(r.ok) << "Lowering failed:\n" << r.diagnostics;
            EXPECT_NE(r.module, nullptr);
            storage.push_back(std::move(r.module));
            return storage.back().get();
        }

        ir::Function* find_func(ir::Module* mod, std::string_view name)
        {
            auto* f = mod->find_function(name);
            EXPECT_NE(f, nullptr) << "Function '" << name << "' not found in module";
            return f;
        }

        ir::Function* find_func_containing(ir::Module* mod, std::string_view substr)
        {
            for (auto& f : mod->functions)
                if (f->name.find(substr) != std::string::npos)
                    return f.get();

            ADD_FAILURE() << "No function containing '" << substr << "'";
            return nullptr;
        }

        ir::BasicBlock* find_block(ir::Function* fn, std::string_view label)
        {
            for (auto& bb : fn->blocks)
                if (bb->label == label)
                    return bb.get();

            ADD_FAILURE() << "Block '" << label << "' not found in function '" << fn->name << "'";
            return nullptr;
        }

        std::size_t count_opcode(ir::Function* fn, ir::Opcode opc)
        {
            std::size_t n = 0;
            for (auto& bb : fn->blocks)
                for (auto& inst : bb->insts)
                    if (inst.opcode == opc)
                        ++n;

            return n;
        }

        std::size_t count_opcode_in(ir::BasicBlock* bb, ir::Opcode opc)
        {
            std::size_t n = 0;
            for (auto& inst : bb->insts)
                if (inst.opcode == opc)
                    ++n;

            return n;
        }

        const ir::Inst* first_inst(ir::Function* fn, ir::Opcode opc)
        {
            for (auto& bb : fn->blocks)
                for (auto& inst : bb->insts)
                    if (inst.opcode == opc)
                        return &inst;

            return nullptr;
        }

        std::vector<const ir::Inst*> all_insts(ir::Function* fn, ir::Opcode opc)
        {
            std::vector<const ir::Inst*> out;
            for (auto& bb : fn->blocks)
                for (auto& inst : bb->insts)
                    if (inst.opcode == opc)
                        out.push_back(&inst);

            return out;
        }

        void expect_terminated(ir::BasicBlock* bb)
        {
            ASSERT_FALSE(bb->insts.empty()) << "Block '" << bb->label << "' is empty";
            EXPECT_TRUE(bb->insts.back().is_terminator()) << "Block '" << bb->label << "' is not terminated. Last opcode: " << ir::print_inst(bb->insts.back());
        }

        void expect_all_terminated(ir::Function* fn)
        {
            for (auto& bb : fn->blocks)
                expect_terminated(bb.get());
        }

        std::size_t block_count(ir::Function* fn) { return fn->blocks.size(); }

        void expect_param_count(ir::Function* fn, std::size_t n)
        {
            EXPECT_EQ(fn->params.size(), n) << "Expected " << n << " params for '" << fn->name << "', got " << fn->params.size();
        }

        const ir::Global* find_global(ir::Module* mod, std::string_view name)
        {
            for (auto& g : mod->globals)
                if (g.name == name)
                    return &g;

            return nullptr;
        }

        bool has_global(ir::Module* mod, std::string_view name) { return find_global(mod, name) != nullptr; }

        void expect_int_type(ir::TypeRef ty, uint8_t width, bool is_signed)
        {
            ASSERT_NE(ty, nullptr);
            ASSERT_TRUE(ty->is_integer()) << "Expected integer type, got: " << ty->to_string();
            auto* it = static_cast<const ir::IntegerType*>(ty);
            EXPECT_EQ(it->width, width);
            EXPECT_EQ(it->is_signed, is_signed);
        }

        void expect_bool_type(ir::TypeRef ty)
        {
            ASSERT_NE(ty, nullptr);
            EXPECT_TRUE(ty->is_bool()) << "Expected bool, got: " << ty->to_string();
        }

        void expect_void_type(ir::TypeRef ty)
        {
            ASSERT_NE(ty, nullptr);
            EXPECT_TRUE(ty->is_void()) << "Expected void, got: " << ty->to_string();
        }

        void expect_pointer_type(ir::TypeRef ty)
        {
            ASSERT_NE(ty, nullptr);
            EXPECT_TRUE(ty->is_pointer()) << "Expected pointer, got: " << ty->to_string();
        }

        void expect_float_type(ir::TypeRef ty, uint8_t width)
        {
            ASSERT_NE(ty, nullptr);
            ASSERT_TRUE(ty->is_float()) << "Expected float, got: " << ty->to_string();
            EXPECT_EQ(static_cast<const ir::FloatType*>(ty)->width, width);
        }
    };

    TEST_F(IRLoweringTest, EmptyFile)
    {
        auto* mod = lower_ok("");
        EXPECT_TRUE(mod->functions.empty());
        EXPECT_TRUE(mod->globals.empty());
    }

    TEST_F(IRLoweringTest, ModuleNameFromDecl)
    {
        auto* mod = lower_ok("module mymod;");
        EXPECT_EQ(mod->name, "mymod");
    }

    TEST_F(IRLoweringTest, DottedModuleName)
    {
        auto* mod = lower_ok("module my.app;");
        EXPECT_EQ(mod->name, "app");
    }

    TEST_F(IRLoweringTest, EmptyVoidFunction)
    {
        auto* mod = lower_ok("void f() {}");
        ASSERT_EQ(mod->functions.size(), 1u);
        auto* fn = mod->functions[0].get();
        EXPECT_FALSE(fn->is_declaration());
        expect_void_type(fn->type->return_type);
        expect_param_count(fn, 0);
        expect_all_terminated(fn);
    }

    TEST_F(IRLoweringTest, FunctionWithParams)
    {
        auto* mod = lower_ok("i32 add(i32 a, i32 b) { return a + b; }");
        ASSERT_EQ(mod->functions.size(), 1u);
        auto* fn = mod->functions[0].get();
        expect_param_count(fn, 2);
        expect_int_type(fn->type->return_type, 32, true);
        EXPECT_EQ(fn->type->param_types.size(), 2u);
    }

    TEST_F(IRLoweringTest, ExternFunctionIsDeclaration)
    {
        auto* mod = lower_ok("extern void puts();");
        ASSERT_EQ(mod->functions.size(), 1u);
        auto* fn = mod->functions[0].get();
        EXPECT_TRUE(fn->is_declaration());
        EXPECT_EQ(fn->linkage, ir::Linkage::ExternDecl);
    }

    TEST_F(IRLoweringTest, PublicFunctionHasExternalLinkage)
    {
        auto* mod = lower_ok("public i32 main() { return 0; }");
        auto* fn = mod->functions[0].get();
        EXPECT_EQ(fn->linkage, ir::Linkage::External);
    }

    TEST_F(IRLoweringTest, PrivateFunctionHasInternalLinkage)
    {
        auto* mod = lower_ok("void helper() {}");
        auto* fn = mod->functions[0].get();
        EXPECT_EQ(fn->linkage, ir::Linkage::Internal);
    }

    TEST_F(IRLoweringTest, ForwardCallBothFunctionsExist)
    {
        auto* mod = lower_ok(R"(
            void caller() { callee(); }
            void callee() {}
        )");
        EXPECT_NE(find_func_containing(mod, "caller"), nullptr);
        EXPECT_NE(find_func_containing(mod, "callee"), nullptr);
    }

    TEST_F(IRLoweringTest, EntryBlockExists)
    {
        auto* mod = lower_ok("void f() {}");
        auto* fn = mod->functions[0].get();
        EXPECT_NE(fn->entry(), nullptr);
        EXPECT_EQ(fn->entry()->label, "entry");
    }

    TEST_F(IRLoweringTest, ReturnBlockExists)
    {
        auto* mod = lower_ok("void f() {}");
        auto* fn = mod->functions[0].get();
        auto* ret_bb = find_block(fn, "return");
        ASSERT_NE(ret_bb, nullptr);
        ASSERT_FALSE(ret_bb->insts.empty());
        EXPECT_EQ(ret_bb->insts.back().opcode, ir::Opcode::Return);
    }

    TEST_F(IRLoweringTest, IntegerLiteral)
    {
        auto* mod = lower_ok("i32 f() { return 42; }");
        auto* fn = mod->functions[0].get();
        auto consts = all_insts(fn, ir::Opcode::Const);
        EXPECT_GE(consts.size(), 1u);

        bool found = false;
        for (auto* c : consts)
        {
            auto& cd = c->as_const();
            if (auto* iv = std::get_if<int64_t>(&cd.value))
            {
                if (*iv == 42)
                    found = true;
            }
        }
        EXPECT_TRUE(found) << "Constant 42 not found in IR";
    }

    TEST_F(IRLoweringTest, FloatLiteral)
    {
        auto* mod = lower_ok("f64 f() { return 3.14; }");
        auto* fn = mod->functions[0].get();
        auto consts = all_insts(fn, ir::Opcode::Const);
        bool found = false;
        for (auto* c : consts)
        {
            auto& cd = c->as_const();
            if (auto* fv = std::get_if<double>(&cd.value))
            {
                if (*fv == 3.14)
                    found = true;
            }
        }
        EXPECT_TRUE(found) << "Constant 3.14 not found";
    }

    TEST_F(IRLoweringTest, BoolLiterals)
    {
        auto* mod = lower_ok(R"(
            bool f() { return true; }
            bool g() { return false; }
        )");
        auto* fn_f = find_func_containing(mod, "f");
        auto* fn_g = find_func_containing(mod, "g");
        ASSERT_NE(fn_f, nullptr);
        ASSERT_NE(fn_g, nullptr);

        auto consts_f = all_insts(fn_f, ir::Opcode::Const);
        bool found_true = false;
        for (auto* c : consts_f)
            if (auto* bv = std::get_if<bool>(&c->as_const().value); bv && *bv)
                found_true = true;

        EXPECT_TRUE(found_true);

        auto consts_g = all_insts(fn_g, ir::Opcode::Const);
        bool found_false = false;
        for (auto* c : consts_g)
            if (auto* bv = std::get_if<bool>(&c->as_const().value); bv && !*bv)
                found_false = true;

        EXPECT_TRUE(found_false);
    }

    TEST_F(IRLoweringTest, NullLiteral)
    {
        auto* mod = lower_ok(R"(
            i32* f() { return null; }
        )");
        auto* fn = mod->functions[0].get();
        auto consts = all_insts(fn, ir::Opcode::Const);
        EXPECT_GE(consts.size(), 1u);

        bool found = false;
        for (auto* c : consts)
        {
            if (c->type && c->type->is_pointer())
                found = true;
        }
        EXPECT_TRUE(found) << "Null pointer constant not found";
    }

    TEST_F(IRLoweringTest, StringLiteralCreatesGlobal)
    {
        auto* mod = lower_ok(R"(
            const u8* f() {
                const u8* x = "world";
                return "hello";
            }
        )");

        bool found_str_global = false;
        for (auto& g : mod->globals)
            if (g.name.starts_with(".str.") && g.is_const)
                found_str_global = true;

        EXPECT_TRUE(found_str_global) << "String literal global not found";
    }

    TEST_F(IRLoweringTest, LocalVarAllocaAndStore)
    {
        auto* mod = lower_ok(R"(
            void f() { i32 x = 42; }
        )");
        auto* fn = mod->functions[0].get();
        EXPECT_GE(count_opcode(fn, ir::Opcode::Alloca), 1u);
        EXPECT_GE(count_opcode(fn, ir::Opcode::Store), 1u);
    }

    TEST_F(IRLoweringTest, LocalVarLoadedOnUse)
    {
        auto* mod = lower_ok(R"(
            i32 f() {
                i32 x = 42;
                return x;
            }
        )");
        auto* fn = mod->functions[0].get();
        EXPECT_GE(count_opcode(fn, ir::Opcode::Load), 1u);
    }

    TEST_F(IRLoweringTest, MultipleLocals)
    {
        auto* mod = lower_ok(R"(
            void f() {
                i32 a = 1;
                i32 b = 2;
                i32 c = 3;
            }
        )");
        auto* fn = mod->functions[0].get();
        EXPECT_GE(count_opcode(fn, ir::Opcode::Alloca), 3u);
    }

    TEST_F(IRLoweringTest, GlobalVarExists)
    {
        auto* mod = lower_ok("i32 count = 0;");
        EXPECT_TRUE(has_global(mod, "count"));
    }

    TEST_F(IRLoweringTest, GlobalVarConst)
    {
        auto* mod = lower_ok("const i32 MAX = 100;");
        auto* g = find_global(mod, "MAX");
        ASSERT_NE(g, nullptr);
        EXPECT_TRUE(g->is_const);
    }

    TEST_F(IRLoweringTest, GlobalVarLinkageInternal)
    {
        auto* mod = lower_ok("static i32 x = 0;");
        auto* g = find_global(mod, "x");
        ASSERT_NE(g, nullptr);
        EXPECT_EQ(g->linkage, ir::Linkage::Internal);
    }

    TEST_F(IRLoweringTest, GlobalVarInitValue)
    {
        auto* mod = lower_ok("i32 val = 42;");
        auto* g = find_global(mod, "val");
        ASSERT_NE(g, nullptr);
        ASSERT_TRUE(g->init.has_value());
        auto* iv = std::get_if<int64_t>(&*g->init);
        ASSERT_NE(iv, nullptr);
        EXPECT_EQ(*iv, 42);
    }

    TEST_F(IRLoweringTest, GlobalBoolInit)
    {
        auto* mod = lower_ok("bool flag = true;");
        auto* g = find_global(mod, "flag");
        ASSERT_NE(g, nullptr);
        ASSERT_TRUE(g->init.has_value());
        auto* bv = std::get_if<bool>(&*g->init);
        ASSERT_NE(bv, nullptr);
        EXPECT_TRUE(*bv);
    }

    TEST_F(IRLoweringTest, IntegerAdd)
    {
        auto* mod = lower_ok("i32 f(i32 a, i32 b) { return a + b; }");
        auto* fn = mod->functions[0].get();
        EXPECT_GE(count_opcode(fn, ir::Opcode::Add), 1u);
    }

    TEST_F(IRLoweringTest, IntegerSub)
    {
        auto* mod = lower_ok("i32 f(i32 a, i32 b) { return a - b; }");
        auto* fn = mod->functions[0].get();
        EXPECT_GE(count_opcode(fn, ir::Opcode::Sub), 1u);
    }

    TEST_F(IRLoweringTest, IntegerMul)
    {
        auto* mod = lower_ok("i32 f(i32 a, i32 b) { return a * b; }");
        auto* fn = mod->functions[0].get();
        EXPECT_GE(count_opcode(fn, ir::Opcode::Mul), 1u);
    }

    TEST_F(IRLoweringTest, IntegerDiv)
    {
        auto* mod = lower_ok("i32 f(i32 a, i32 b) { return a / b; }");
        auto* fn = mod->functions[0].get();
        EXPECT_GE(count_opcode(fn, ir::Opcode::Div), 1u);
    }

    TEST_F(IRLoweringTest, IntegerMod)
    {
        auto* mod = lower_ok("i32 f(i32 a, i32 b) { return a % b; }");
        auto* fn = mod->functions[0].get();
        EXPECT_GE(count_opcode(fn, ir::Opcode::Mod), 1u);
    }

    TEST_F(IRLoweringTest, FloatArithmetic)
    {
        auto* mod = lower_ok(R"(
            f64 f(f64 a, f64 b) {
                return a + b - a * b / a;
            }
        )");
        auto* fn = mod->functions[0].get();
        EXPECT_GE(count_opcode(fn, ir::Opcode::Add), 1u);
        EXPECT_GE(count_opcode(fn, ir::Opcode::Sub), 1u);
        EXPECT_GE(count_opcode(fn, ir::Opcode::Mul), 1u);
        EXPECT_GE(count_opcode(fn, ir::Opcode::Div), 1u);
    }

    TEST_F(IRLoweringTest, AllComparisonOps)
    {
        auto* mod = lower_ok(R"(
            void f(i32 a, i32 b) {
                bool r1 = a == b;
                bool r2 = a != b;
                bool r3 = a < b;
                bool r4 = a <= b;
                bool r5 = a > b;
                bool r6 = a >= b;
            }
        )");
        auto* fn = mod->functions[0].get();
        EXPECT_GE(count_opcode(fn, ir::Opcode::Eq), 1u);
        EXPECT_GE(count_opcode(fn, ir::Opcode::Ne), 1u);
        EXPECT_GE(count_opcode(fn, ir::Opcode::Lt), 1u);
        EXPECT_GE(count_opcode(fn, ir::Opcode::Le), 1u);
        EXPECT_GE(count_opcode(fn, ir::Opcode::Gt), 1u);
        EXPECT_GE(count_opcode(fn, ir::Opcode::Ge), 1u);
    }

    TEST_F(IRLoweringTest, ComparisonResultIsBool)
    {
        auto* mod = lower_ok("bool f(i32 a, i32 b) { return a < b; }");
        auto* fn = mod->functions[0].get();
        auto* cmp = first_inst(fn, ir::Opcode::Lt);
        ASSERT_NE(cmp, nullptr);
        expect_bool_type(cmp->type);
    }

    TEST_F(IRLoweringTest, BitwiseOps)
    {
        auto* mod = lower_ok(R"(
            void f(i32 a, i32 b) {
                i32 r1 = a & b;
                i32 r2 = a | b;
                i32 r3 = a ^ b;
                i32 r4 = a << 2;
                i32 r5 = a >> 1;
            }
        )");
        auto* fn = mod->functions[0].get();
        EXPECT_GE(count_opcode(fn, ir::Opcode::BitAnd), 1u);
        EXPECT_GE(count_opcode(fn, ir::Opcode::BitOr), 1u);
        EXPECT_GE(count_opcode(fn, ir::Opcode::BitXor), 1u);
        EXPECT_GE(count_opcode(fn, ir::Opcode::Shl), 1u);
        EXPECT_GE(count_opcode(fn, ir::Opcode::Shr), 1u);
    }

    TEST_F(IRLoweringTest, UnaryNegate)
    {
        auto* mod = lower_ok("i32 f(i32 x) { return -x; }");
        auto* fn = mod->functions[0].get();
        EXPECT_GE(count_opcode(fn, ir::Opcode::Neg), 1u);
    }

    TEST_F(IRLoweringTest, UnaryBitNot)
    {
        auto* mod = lower_ok("i32 f(i32 x) { return ~x; }");
        auto* fn = mod->functions[0].get();
        EXPECT_GE(count_opcode(fn, ir::Opcode::BitNot), 1u);
    }

    TEST_F(IRLoweringTest, UnaryLogNot)
    {
        auto* mod = lower_ok("bool f(bool x) { return !x; }");
        auto* fn = mod->functions[0].get();
        EXPECT_GE(count_opcode(fn, ir::Opcode::LogNot), 1u);
    }

    TEST_F(IRLoweringTest, DerefProducesLoad)
    {
        auto* mod = lower_ok(R"(
            i32 f(i32* p) { return *p; }
        )");
        auto* fn = mod->functions[0].get();
        EXPECT_GE(count_opcode(fn, ir::Opcode::Load), 1u);
    }

    TEST_F(IRLoweringTest, AddressOfDoesNotLoad)
    {
        auto* mod = lower_ok(R"(
            i32* f() {
                i32 x = 5;
                return &x;
            }
        )");
        auto* fn = mod->functions[0].get();
        EXPECT_GE(count_opcode(fn, ir::Opcode::Alloca), 1u);
        EXPECT_GE(count_opcode(fn, ir::Opcode::Store), 1u);
    }

    TEST_F(IRLoweringTest, PreIncrement)
    {
        auto* mod = lower_ok(R"(
            i32 f() {
                i32 x = 0;
                return ++x;
            }
        )");
        auto* fn = mod->functions[0].get();
        EXPECT_GE(count_opcode(fn, ir::Opcode::Add), 1u);
    }

    TEST_F(IRLoweringTest, PostIncrement)
    {
        auto* mod = lower_ok(R"(
            i32 f() {
                i32 x = 0;
                return x++;
            }
        )");
        auto* fn = mod->functions[0].get();
        EXPECT_GE(count_opcode(fn, ir::Opcode::Add), 1u);
    }

    TEST_F(IRLoweringTest, SimpleAssignment)
    {
        auto* mod = lower_ok(R"(
            void f() {
                i32 x = 0;
                x = 5;
            }
        )");
        auto* fn = mod->functions[0].get();
        EXPECT_GE(count_opcode(fn, ir::Opcode::Store), 2u);
    }

    TEST_F(IRLoweringTest, CompoundAddAssignment)
    {
        auto* mod = lower_ok(R"(
            void f() {
                i32 x = 10;
                x += 5;
            }
        )");
        auto* fn = mod->functions[0].get();
        EXPECT_GE(count_opcode(fn, ir::Opcode::Add), 1u);
        EXPECT_GE(count_opcode(fn, ir::Opcode::Load), 1u);
    }

    TEST_F(IRLoweringTest, AllCompoundAssignOps)
    {
        auto* mod = lower_ok(R"(
            void f() {
                i32 x = 100;
                x += 1; x -= 1; x *= 2; x /= 2; x %= 3;
                x &= 0xFF; x |= 0x01; x ^= 0x10;
                x <<= 1; x >>= 1;
            }
        )");
        auto* fn = mod->functions[0].get();
        EXPECT_GE(count_opcode(fn, ir::Opcode::Add), 1u);
        EXPECT_GE(count_opcode(fn, ir::Opcode::Sub), 1u);
        EXPECT_GE(count_opcode(fn, ir::Opcode::Mul), 1u);
        EXPECT_GE(count_opcode(fn, ir::Opcode::Div), 1u);
        EXPECT_GE(count_opcode(fn, ir::Opcode::Mod), 1u);
        EXPECT_GE(count_opcode(fn, ir::Opcode::BitAnd), 1u);
        EXPECT_GE(count_opcode(fn, ir::Opcode::BitOr), 1u);
        EXPECT_GE(count_opcode(fn, ir::Opcode::BitXor), 1u);
        EXPECT_GE(count_opcode(fn, ir::Opcode::Shl), 1u);
        EXPECT_GE(count_opcode(fn, ir::Opcode::Shr), 1u);
    }

    TEST_F(IRLoweringTest, LogicalAndShortCircuit)
    {
        auto* mod = lower_ok(R"(
            bool f(bool a, bool b) { return a && b; }
        )");
        auto* fn = mod->functions[0].get();
        EXPECT_GE(block_count(fn), 3u);
        EXPECT_GE(count_opcode(fn, ir::Opcode::Phi), 1u);
        EXPECT_GE(count_opcode(fn, ir::Opcode::CondBranch), 1u);
    }

    TEST_F(IRLoweringTest, LogicalOrShortCircuit)
    {
        auto* mod = lower_ok(R"(
            bool f(bool a, bool b) { return a || b; }
        )");
        auto* fn = mod->functions[0].get();
        EXPECT_GE(count_opcode(fn, ir::Opcode::Phi), 1u);
        EXPECT_GE(count_opcode(fn, ir::Opcode::CondBranch), 1u);
    }

    TEST_F(IRLoweringTest, PhiHasBoolType)
    {
        auto* mod = lower_ok("bool f(bool a, bool b) { return a && b; }");
        auto* fn = mod->functions[0].get();
        auto* phi = first_inst(fn, ir::Opcode::Phi);
        ASSERT_NE(phi, nullptr);
        expect_bool_type(phi->type);
    }

    TEST_F(IRLoweringTest, TernaryExpr)
    {
        auto* mod = lower_ok(R"(
            i32 f(bool c) { return c ? 1 : 0; }
        )");
        auto* fn = mod->functions[0].get();
        EXPECT_GE(count_opcode(fn, ir::Opcode::CondBranch), 1u);
        EXPECT_GE(count_opcode(fn, ir::Opcode::Phi), 1u);
        EXPECT_GE(block_count(fn), 4u);
    }

    TEST_F(IRLoweringTest, CastIntToFloat)
    {
        auto* mod = lower_ok("f64 f(i32 x) { return x as f64; }");
        auto* fn = mod->functions[0].get();
        EXPECT_GE(count_opcode(fn, ir::Opcode::IntToFloat), 1u);
    }

    TEST_F(IRLoweringTest, CastFloatToInt)
    {
        auto* mod = lower_ok("i32 f(f64 x) { return x as i32; }");
        auto* fn = mod->functions[0].get();
        EXPECT_GE(count_opcode(fn, ir::Opcode::FloatToInt), 1u);
    }

    TEST_F(IRLoweringTest, CastIntNarrowing)
    {
        auto* mod = lower_ok("i8 f(i32 x) { return x as i8; }");
        auto* fn = mod->functions[0].get();
        EXPECT_GE(count_opcode(fn, ir::Opcode::IntToInt), 1u);
    }

    TEST_F(IRLoweringTest, CastIntWidening)
    {
        auto* mod = lower_ok("i64 f(i32 x) { return x as i64; }");
        auto* fn = mod->functions[0].get();
        EXPECT_GE(count_opcode(fn, ir::Opcode::IntToInt), 1u);
    }

    TEST_F(IRLoweringTest, CastIntToBool)
    {
        auto* mod = lower_ok("bool f(i32 x) { return x as bool; }");
        auto* fn = mod->functions[0].get();
        EXPECT_GE(count_opcode(fn, ir::Opcode::IntToInt), 1u);
    }

    TEST_F(IRLoweringTest, CastBoolToInt)
    {
        auto* mod = lower_ok("i32 f(bool b) { return b as i32; }");
        auto* fn = mod->functions[0].get();
        EXPECT_GE(count_opcode(fn, ir::Opcode::IntToInt), 1u);
    }

    TEST_F(IRLoweringTest, CastPointerToPointer)
    {
        auto* mod = lower_ok("u8* f(i32* p) { return p as u8*; }");
        auto* fn = mod->functions[0].get();
        EXPECT_GE(count_opcode(fn, ir::Opcode::Bitcast), 1u);
    }

    TEST_F(IRLoweringTest, CastPointerToInt)
    {
        auto* mod = lower_ok("u64 f(i32* p) { return p as u64; }");
        auto* fn = mod->functions[0].get();
        EXPECT_GE(count_opcode(fn, ir::Opcode::PtrToInt), 1u);
    }

    TEST_F(IRLoweringTest, CastIntToPointer)
    {
        auto* mod = lower_ok("i32* f(u64 x) { return x as i32*; }");
        auto* fn = mod->functions[0].get();
        EXPECT_GE(count_opcode(fn, ir::Opcode::IntToPtr), 1u);
    }

    TEST_F(IRLoweringTest, CastFloatWidening)
    {
        auto* mod = lower_ok("f64 f(f32 x) { return x as f64; }");
        auto* fn = mod->functions[0].get();
        EXPECT_GE(count_opcode(fn, ir::Opcode::FloatToFloat), 1u);
    }

    TEST_F(IRLoweringTest, ChainedCast)
    {
        auto* mod = lower_ok("i64 f(f64 x) { return x as i32 as i64; }");
        auto* fn = mod->functions[0].get();
        EXPECT_GE(count_opcode(fn, ir::Opcode::FloatToInt), 1u);
        EXPECT_GE(count_opcode(fn, ir::Opcode::IntToInt), 1u);
    }

    TEST_F(IRLoweringTest, IdentityCastElided)
    {
        auto* mod = lower_ok("i32 f(i32 x) { return x as i32; }");
        auto* fn = mod->functions[0].get();
        EXPECT_EQ(count_opcode(fn, ir::Opcode::IntToInt), 0u);
        EXPECT_EQ(count_opcode(fn, ir::Opcode::Bitcast), 0u);
    }

    TEST_F(IRLoweringTest, IfThen)
    {
        auto* mod = lower_ok(R"(
            void f(bool c) {
                if c { return; }
            }
        )");
        auto* fn = mod->functions[0].get();
        EXPECT_GE(count_opcode(fn, ir::Opcode::CondBranch), 1u);
        expect_all_terminated(fn);
    }

    TEST_F(IRLoweringTest, IfThenElse)
    {
        auto* mod = lower_ok(R"(
            i32 f(bool c) {
                if c { return 1; } else { return 0; }
            }
        )");
        auto* fn = mod->functions[0].get();
        EXPECT_GE(count_opcode(fn, ir::Opcode::CondBranch), 1u);
        EXPECT_GE(block_count(fn), 4u);
        expect_all_terminated(fn);
    }

    TEST_F(IRLoweringTest, IfElseIfElse)
    {
        auto* mod = lower_ok(R"(
            i32 f(i32 x) {
                if x > 10 { return 2; }
                else if x > 0 { return 1; }
                else { return 0; }
            }
        )");
        auto* fn = mod->functions[0].get();
        EXPECT_GE(count_opcode(fn, ir::Opcode::CondBranch), 2u);
        expect_all_terminated(fn);
    }

    TEST_F(IRLoweringTest, WhileLoop)
    {
        auto* mod = lower_ok(R"(
            void f() {
                i32 x = 10;
                while x > 0 { x = x - 1; }
            }
        )");
        auto* fn = mod->functions[0].get();
        EXPECT_NE(find_block(fn, "while.cond"), nullptr);
        EXPECT_NE(find_block(fn, "while.body"), nullptr);
        EXPECT_NE(find_block(fn, "while.exit"), nullptr);
        expect_all_terminated(fn);
    }

    TEST_F(IRLoweringTest, ForLoop)
    {
        auto* mod = lower_ok(R"(
            void f() {
                for (i32 i = 0; i < 10; i = i + 1) {
                    ;
                }
            }
        )");
        auto* fn = mod->functions[0].get();
        EXPECT_NE(find_block(fn, "for.cond"), nullptr);
        EXPECT_NE(find_block(fn, "for.body"), nullptr);
        EXPECT_NE(find_block(fn, "for.incr"), nullptr);
        EXPECT_NE(find_block(fn, "for.exit"), nullptr);
        expect_all_terminated(fn);
    }

    TEST_F(IRLoweringTest, DoWhileLoop)
    {
        auto* mod = lower_ok(R"(
            void f() {
                i32 x = 10;
                do { x = x - 1; } while x > 0;
            }
        )");
        auto* fn = mod->functions[0].get();
        EXPECT_NE(find_block(fn, "do.body"), nullptr);
        EXPECT_NE(find_block(fn, "do.cond"), nullptr);
        EXPECT_NE(find_block(fn, "do.exit"), nullptr);
        expect_all_terminated(fn);
    }

    TEST_F(IRLoweringTest, BreakBranchesToExit)
    {
        auto* mod = lower_ok(R"(
            void f() {
                while true { break; }
            }
        )");
        auto* fn = mod->functions[0].get();
        auto* exit_bb = find_block(fn, "while.exit");
        ASSERT_NE(exit_bb, nullptr);
        auto branches = all_insts(fn, ir::Opcode::Branch);
        bool found = false;
        for (auto* b : branches)
            if (b->as_branch().target == exit_bb)
                found = true;

        EXPECT_TRUE(found) << "Break should branch to while.exit";
    }

    TEST_F(IRLoweringTest, ContinueBranchesToCond)
    {
        auto* mod = lower_ok(R"(
            void f() {
                while true { continue; }
            }
        )");
        auto* fn = mod->functions[0].get();
        auto* cond_bb = find_block(fn, "while.cond");
        ASSERT_NE(cond_bb, nullptr);
        auto branches = all_insts(fn, ir::Opcode::Branch);
        bool found = false;
        for (auto* b : branches)
            if (b->as_branch().target == cond_bb)
                found = true;

        EXPECT_TRUE(found) << "Continue should branch to while.cond";
    }

    TEST_F(IRLoweringTest, ForContinueBranchesToIncr)
    {
        auto* mod = lower_ok(R"(
            void f() {
                for (i32 i = 0; i < 10; i = i + 1) {
                    continue;
                }
            }
        )");
        auto* fn = mod->functions[0].get();
        auto* incr_bb = find_block(fn, "for.incr");
        ASSERT_NE(incr_bb, nullptr);
        auto branches = all_insts(fn, ir::Opcode::Branch);
        bool found = false;
        for (auto* b : branches)
            if (b->as_branch().target == incr_bb)
                found = true;

        EXPECT_TRUE(found) << "Continue in for should branch to for.incr";
    }

    TEST_F(IRLoweringTest, NestedLoopBreak)
    {
        auto* mod = lower_ok(R"(
            void f() {
                while true {
                    while true { break; }
                    break;
                }
            }
        )");
        auto* fn = mod->functions[0].get();
        expect_all_terminated(fn);
    }

    TEST_F(IRLoweringTest, ReturnValueStoresInSlot)
    {
        auto* mod = lower_ok("i32 f() { return 42; }");
        auto* fn = mod->functions[0].get();
        auto* ret_bb = find_block(fn, "return");
        ASSERT_NE(ret_bb, nullptr);
        EXPECT_GE(count_opcode_in(ret_bb, ir::Opcode::Load), 1u);
        EXPECT_EQ(ret_bb->insts.back().opcode, ir::Opcode::Return);
    }

    TEST_F(IRLoweringTest, ReturnVoidNoReturnSlot)
    {
        auto* mod = lower_ok("void f() { return; }");
        auto* fn = mod->functions[0].get();
        auto* ret_bb = find_block(fn, "return");
        ASSERT_NE(ret_bb, nullptr);
        EXPECT_EQ(count_opcode_in(ret_bb, ir::Opcode::Load), 0u);
        EXPECT_EQ(ret_bb->insts.back().opcode, ir::Opcode::Return);
        EXPECT_TRUE(ret_bb->insts.back().operands.empty());
    }

    TEST_F(IRLoweringTest, MultipleReturnsMerge)
    {
        auto* mod = lower_ok(R"(
            i32 f(bool c) {
                if c { return 1; }
                return 0;
            }
        )");
        auto* fn = mod->functions[0].get();
        auto* ret_bb = find_block(fn, "return");
        ASSERT_NE(ret_bb, nullptr);
        auto branches = all_insts(fn, ir::Opcode::Branch);
        std::size_t ret_branches = 0;
        for (auto* b : branches)
            if (b->as_branch().target == ret_bb)
                ++ret_branches;

        EXPECT_GE(ret_branches, 2u) << "Both return paths should branch to return block";
    }

    TEST_F(IRLoweringTest, SimpleCall)
    {
        auto* mod = lower_ok(R"(
            i32 get() { return 42; }
            i32 f() { return get(); }
        )");
        auto* fn = find_func_containing(mod, "f");
        ASSERT_NE(fn, nullptr);
        EXPECT_GE(count_opcode(fn, ir::Opcode::Call), 1u);
    }

    TEST_F(IRLoweringTest, CallWithArgs)
    {
        auto* mod = lower_ok(R"(
            i32 add(i32 a, i32 b) { return a + b; }
            i32 f() { return add(1, 2); }
        )");
        auto* fn = find_func_containing(mod, "f");
        ASSERT_NE(fn, nullptr);
        auto calls = all_insts(fn, ir::Opcode::Call);
        ASSERT_GE(calls.size(), 1u);
        EXPECT_EQ(calls[0]->operands.size(), 2u);
    }

    TEST_F(IRLoweringTest, VoidCallNoValue)
    {
        auto* mod = lower_ok(R"(
            void noop() {}
            void f() { noop(); }
        )");
        auto* fn = find_func_containing(mod, "f");
        ASSERT_NE(fn, nullptr);
        auto calls = all_insts(fn, ir::Opcode::Call);
        ASSERT_GE(calls.size(), 1u);
        EXPECT_FALSE(calls[0]->has_value());
    }

    TEST_F(IRLoweringTest, CallReturnHasValue)
    {
        auto* mod = lower_ok(R"(
            i32 get() { return 42; }
            void f() { i32 x = get(); }
        )");
        auto* fn = find_func_containing(mod, "f");
        ASSERT_NE(fn, nullptr);
        auto calls = all_insts(fn, ir::Opcode::Call);
        ASSERT_GE(calls.size(), 1u);
        EXPECT_TRUE(calls[0]->has_value());
    }

    TEST_F(IRLoweringTest, RecursiveCall)
    {
        auto* mod = lower_ok(R"(
            i32 fact(i32 n) {
                if n <= 1 { return 1; }
                return n * fact(n - 1);
            }
        )");
        auto* fn = mod->functions[0].get();
        auto calls = all_insts(fn, ir::Opcode::Call);
        ASSERT_GE(calls.size(), 1u);
        EXPECT_EQ(calls[0]->as_call().callee, fn->name);
    }

    TEST_F(IRLoweringTest, StructFieldAccess)
    {
        auto* mod = lower_ok(R"(
            struct Point { f32 x; f32 y; }
            f32 f() {
                Point p = { x: 1.0, y: 2.0 };
                return p.x;
            }
        )");
        auto* fn = mod->functions[0].get();
        EXPECT_GE(count_opcode(fn, ir::Opcode::GetFieldPtr), 1u);
    }

    TEST_F(IRLoweringTest, StructFieldAssignment)
    {
        auto* mod = lower_ok(R"(
            struct Point { f32 x; f32 y; }
            void f() {
                Point p = { x: 1.0, y: 2.0 };
                p.x = 3.0;
            }
        )");
        auto* fn = mod->functions[0].get();
        auto gfps = all_insts(fn, ir::Opcode::GetFieldPtr);
        EXPECT_GE(gfps.size(), 1u);
        EXPECT_GE(count_opcode(fn, ir::Opcode::Store), 1u);
    }

    TEST_F(IRLoweringTest, StructInitializerFieldStores)
    {
        auto* mod = lower_ok(R"(
            struct Vec2 { f32 x; f32 y; }
            void f() {
                Vec2 v = { x: 1.0, y: 2.0 };
            }
        )");
        auto* fn = mod->functions[0].get();
        auto gfps = all_insts(fn, ir::Opcode::GetFieldPtr);
        EXPECT_GE(gfps.size(), 2u);
    }

    TEST_F(IRLoweringTest, NestedStructAccess)
    {
        auto* mod = lower_ok(R"(
            struct Inner { i32 val; }
            struct Outer { Inner inner; }
            i32 f() {
                Inner i = { val: 42 };
                Outer o = { inner: i };
                return o.inner.val;
            }
        )");
        auto* fn = mod->functions[0].get();
        auto gfps = all_insts(fn, ir::Opcode::GetFieldPtr);
        EXPECT_GE(gfps.size(), 2u);
    }

    TEST_F(IRLoweringTest, PointerAutoDeref)
    {
        auto* mod = lower_ok(R"(
            struct Point { f32 x; f32 y; }
            f32 f() {
                Point p = { x: 1.0, y: 2.0 };
                Point* pp = &p;
                return pp.x;
            }
        )");
        auto* fn = mod->functions[0].get();
        EXPECT_GE(count_opcode(fn, ir::Opcode::Load), 1u);
        EXPECT_GE(count_opcode(fn, ir::Opcode::GetFieldPtr), 1u);
    }

    TEST_F(IRLoweringTest, DoublePointerAutoDeref)
    {
        auto* mod = lower_ok(R"(
            struct S { i32 val; }
            i32 f() {
                S s = { val: 42 };
                S* p = &s;
                S** pp = &p;
                return (*pp).val;
            }
        )");
        auto* fn = mod->functions[0].get();
        EXPECT_GE(count_opcode(fn, ir::Opcode::Load), 1u);
        EXPECT_GE(count_opcode(fn, ir::Opcode::GetFieldPtr), 1u);
        expect_all_terminated(fn);
    }

    TEST_F(IRLoweringTest, AssignThroughPointer)
    {
        auto* mod = lower_ok(R"(
            struct S { i32 val; }
            void f() {
                S s = { val: 0 };
                S* p = &s;
                p.val = 42;
            }
        )");
        auto* fn = mod->functions[0].get();
        EXPECT_GE(count_opcode(fn, ir::Opcode::GetFieldPtr), 1u);
        EXPECT_GE(count_opcode(fn, ir::Opcode::Store), 1u);
    }

    TEST_F(IRLoweringTest, UnionFieldAccessBitcast)
    {
        auto* mod = lower_ok(R"(
            union Value { i32 int_val; f64 float_val; }
            void f() {
                Value v = { int_val: 42 };
                i32 x = v.int_val;
            }
        )");
        auto* fn = mod->functions[0].get();
        EXPECT_GE(count_opcode(fn, ir::Opcode::GetFieldPtr), 1u);
        EXPECT_GE(count_opcode(fn, ir::Opcode::Bitcast), 1u);
    }

    TEST_F(IRLoweringTest, EnumMatchTagExtraction)
    {
        auto* mod = lower_ok(R"(
            enum Color { Red, Green, Blue }
            void f() {
                Color c;
                match c {
                    Color::Red => { return; },
                    Color::Green => { return; },
                    Color::Blue => { return; }
                }
            }
        )");
        auto* fn = find_func_containing(mod, "f");
        ASSERT_NE(fn, nullptr);
        EXPECT_GE(count_opcode(fn, ir::Opcode::GetFieldPtr), 1u);
        EXPECT_GE(count_opcode(fn, ir::Opcode::Eq), 1u);
        expect_all_terminated(fn);
    }

    TEST_F(IRLoweringTest, EnumPayloadMatch)
    {
        auto* mod = lower_ok(R"(
            enum Option { Some(i32), None }
            void f() {
                Option opt;
                match opt {
                    Option::Some(val) => { return; },
                    Option::None => { return; }
                }
            }
        )");
        auto* fn = find_func_containing(mod, "f");
        ASSERT_NE(fn, nullptr);
        EXPECT_GE(count_opcode(fn, ir::Opcode::GetFieldPtr), 2u);
        EXPECT_GE(count_opcode(fn, ir::Opcode::Eq), 1u);
        expect_all_terminated(fn);
    }

    TEST_F(IRLoweringTest, MatchLiteralPattern)
    {
        auto* mod = lower_ok(R"(
            void f(i32 x) {
                match x {
                    1 => { return; },
                    2 => { return; },
                    _ => { return; }
                }
            }
        )");
        auto* fn = mod->functions[0].get();
        EXPECT_GE(count_opcode(fn, ir::Opcode::Eq), 2u);
        EXPECT_GE(block_count(fn), 7u);
        expect_all_terminated(fn);
    }

    TEST_F(IRLoweringTest, MatchWildcardAlwaysBranches)
    {
        auto* mod = lower_ok(R"(
            void f(i32 x) {
                match x {
                    _ => { return; }
                }
            }
        )");
        auto* fn = mod->functions[0].get();
        EXPECT_EQ(count_opcode(fn, ir::Opcode::Eq), 0u);
        expect_all_terminated(fn);
    }

    TEST_F(IRLoweringTest, MatchBindingAlwaysBranches)
    {
        auto* mod = lower_ok(R"(
            void f(i32 x) {
                match x {
                    val => { return; }
                }
            }
        )");
        auto* fn = mod->functions[0].get();
        EXPECT_EQ(count_opcode(fn, ir::Opcode::Eq), 0u);
        EXPECT_GE(count_opcode(fn, ir::Opcode::Alloca), 1u);
        expect_all_terminated(fn);
    }

    TEST_F(IRLoweringTest, MatchWithGuard)
    {
        auto* mod = lower_ok(R"(
            void f(i32 x) {
                match x {
                    n if n > 0 => { return; },
                    _ => { return; }
                }
            }
        )");
        auto* fn = mod->functions[0].get();
        EXPECT_GE(count_opcode(fn, ir::Opcode::CondBranch), 1u);
        EXPECT_GE(count_opcode(fn, ir::Opcode::Gt), 1u);
        expect_all_terminated(fn);
    }

    TEST_F(IRLoweringTest, MatchStructPattern)
    {
        auto* mod = lower_ok(R"(
            struct Point { f32 x; f32 y; }
            void f() {
                Point p = { x: 1.0, y: 2.0 };
                match p {
                    Point { x: xv, y: yv } => { return; }
                }
            }
        )");
        auto* fn = mod->functions[0].get();
        EXPECT_GE(count_opcode(fn, ir::Opcode::GetFieldPtr), 2u);
        expect_all_terminated(fn);
    }

    TEST_F(IRLoweringTest, MatchMergeBlock)
    {
        auto* mod = lower_ok(R"(
            void f(i32 x) {
                match x {
                    1 => { ; },
                    _ => { ; }
                }
            }
        )");
        auto* fn = mod->functions[0].get();
        EXPECT_NE(find_block(fn, "match.merge"), nullptr);
        expect_all_terminated(fn);
    }

    TEST_F(IRLoweringTest, DeferCalledBeforeReturn)
    {
        auto* mod = lower_ok(R"(
            void cleanup() {}
            void f() {
                defer cleanup();
                return;
            }
        )");
        auto* fn = find_func_containing(mod, "f");
        ASSERT_NE(fn, nullptr);
        auto calls = all_insts(fn, ir::Opcode::Call);
        bool found = false;
        for (auto* c : calls)
            if (c->as_call().callee.find("cleanup") != std::string::npos)
                found = true;

        EXPECT_TRUE(found) << "Deferred cleanup() call not found";
    }

    TEST_F(IRLoweringTest, DeferReverseOrder)
    {
        auto* mod = lower_ok(R"(
            void a() {}
            void b() {}
            void f() {
                defer a();
                defer b();
            }
        )");
        auto* fn = find_func_containing(mod, "f");
        ASSERT_NE(fn, nullptr);
        auto calls = all_insts(fn, ir::Opcode::Call);
        std::vector<std::string> call_names;
        for (auto* c : calls)
            call_names.push_back(c->as_call().callee);

        int pos_a = -1, pos_b = -1;
        for (int i = 0; i < static_cast<int>(call_names.size()); ++i)
        {
            if (call_names[i].find("a") != std::string::npos && pos_a == -1)
                pos_a = i;
            if (call_names[i].find("b") != std::string::npos && pos_b == -1)
                pos_b = i;
        }
        if (pos_a >= 0 && pos_b >= 0)
            EXPECT_LT(pos_b, pos_a) << "Defers should execute in reverse order (b before a)";
    }

    TEST_F(IRLoweringTest, StaticIfTrueBranchOnly)
    {
        auto* mod = lower_ok(R"(
            void f() {
                static if true {
                    i32 x = 42;
                }
            }
        )");
        auto* fn = mod->functions[0].get();
        EXPECT_GE(count_opcode(fn, ir::Opcode::Alloca), 1u);
    }

    TEST_F(IRLoweringTest, StaticIfFalsePruned)
    {
        auto* mod = lower_ok(R"(
            void f() {
                static if false {
                    i32 x = 42;
                }
            }
        )");
        auto* fn = mod->functions[0].get();
        auto consts = all_insts(fn, ir::Opcode::Const);
        bool found42 = false;
        for (auto* c : consts)
        {
            if (auto* iv = std::get_if<int64_t>(&c->as_const().value))
                if (*iv == 42)
                    found42 = true;
        }
        EXPECT_FALSE(found42) << "Static if false body should be pruned";
    }

    TEST_F(IRLoweringTest, SizeofEmitsConst)
    {
        auto* mod = lower_ok(R"(
            u64 f() { return sizeof(i32); }
        )");
        auto* fn = mod->functions[0].get();
        auto consts = all_insts(fn, ir::Opcode::Const);
        bool found4 = false;
        for (auto* c : consts)
            if (auto* uv = std::get_if<uint64_t>(&c->as_const().value))
                if (*uv == 4)
                    found4 = true;

        EXPECT_TRUE(found4) << "sizeof(i32) should be 4";
    }

    TEST_F(IRLoweringTest, AlignofEmitsConst)
    {
        auto* mod = lower_ok(R"(
            u64 f() { return alignof(f64); }
        )");
        auto* fn = mod->functions[0].get();
        auto consts = all_insts(fn, ir::Opcode::Const);
        bool found8 = false;
        for (auto* c : consts)
            if (auto* uv = std::get_if<uint64_t>(&c->as_const().value))
                if (*uv == 8)
                    found8 = true;

        EXPECT_TRUE(found8) << "alignof(f64) should be 8";
    }

    TEST_F(IRLoweringTest, SizeofStruct)
    {
        auto* mod = lower_ok(R"(
            struct Pair { i32 a; i32 b; }
            u64 f() { return sizeof(Pair); }
        )");
        auto* fn = mod->functions[0].get();
        auto consts = all_insts(fn, ir::Opcode::Const);
        bool found8 = false;
        for (auto* c : consts)
            if (auto* uv = std::get_if<uint64_t>(&c->as_const().value))
                if (*uv == 8)
                    found8 = true;

        EXPECT_TRUE(found8) << "sizeof({i32,i32}) should be 8";
    }

    TEST_F(IRLoweringTest, ArrayIndexGEP)
    {
        auto* mod = lower_ok(R"(
            void f() {
                i32[10] arr;
                i32 x = arr[3];
            }
        )");
        auto* fn = mod->functions[0].get();
        EXPECT_GE(count_opcode(fn, ir::Opcode::GetElementPtr), 1u);
    }

    TEST_F(IRLoweringTest, SliceFieldAccess)
    {
        auto* mod = lower_ok(R"(
            void f() {
                []i32 s;
                u64 len = s.len;
                i32* ptr = s.ptr;
            }
        )");
        auto* fn = mod->functions[0].get();
        EXPECT_GE(count_opcode(fn, ir::Opcode::GetFieldPtr), 2u);
    }

    TEST_F(IRLoweringTest, UfcsBasicCall)
    {
        auto* mod = lower_ok(R"(
            struct Vec2 { f32 x; f32 y; }
            f32 length_sq(Vec2 v) {
                return v.x * v.x + v.y * v.y;
            }
            void f() {
                Vec2 v = { x: 3.0, y: 4.0 };
                f32 lsq = v.length_sq();
            }
        )");
        auto* fn = find_func_containing(mod, "f");
        ASSERT_NE(fn, nullptr);
        auto calls = all_insts(fn, ir::Opcode::Call);
        ASSERT_GE(calls.size(), 1u);
        EXPECT_GE(calls[0]->operands.size(), 1u);
    }

    TEST_F(IRLoweringTest, UfcsWithExtraArgs)
    {
        auto r = lower(R"(
        struct Vec2 { f32 x; f32 y; }
        Vec2 scale(Vec2 v, f32 factor) {
            Vec2 r = { x: v.x * factor, y: v.y * factor };
            return r;
        }
        void caller() {
            Vec2 v = { x: 1.0, y: 2.0 };
            Vec2 scaled = v.scale(2.0);
        }
    )");

        ASSERT_TRUE(r.ok) << "Lowering failed:\n" << r.diagnostics;

        auto* mod = r.module.get();
        auto* fn = find_func_containing(mod, "caller");
        ASSERT_NE(fn, nullptr);

        auto calls = all_insts(fn, ir::Opcode::Call);
        bool found = false;

        for (auto* c : calls)
        {
            if (c->as_call().callee.find("scale") != std::string::npos)
            {
                found = true;
                EXPECT_EQ(c->operands.size(), 2u);
            }
        }

        EXPECT_TRUE(found) << "UFCS scale call not found";
    }

    TEST_F(IRLoweringTest, UfcsChainedCalls)
    {
        auto* mod = lower_ok(R"(
            struct Builder { i32 val; }
            Builder with_val(Builder b, i32 v) {
                Builder r = { val: v };
                return r;
            }
            i32 build(Builder b) { return b.val; }
            void f() {
                Builder b = { val: 0 };
                i32 result = b.with_val(42).build();
            }
        )");
        auto* fn = find_func_containing(mod, "f");
        ASSERT_NE(fn, nullptr);
        auto calls = all_insts(fn, ir::Opcode::Call);
        EXPECT_GE(calls.size(), 2u) << "Expected at least 2 chained UFCS calls";
    }

    TEST_F(IRLoweringTest, BlockScoping)
    {
        auto* mod = lower_ok(R"(
            void f() {
                i32 x = 1;
                {
                    i32 y = 2;
                }
            }
        )");
        auto* fn = mod->functions[0].get();
        EXPECT_GE(count_opcode(fn, ir::Opcode::Alloca), 2u);
        expect_all_terminated(fn);
    }

    TEST_F(IRLoweringTest, ParamsAllocatedAndStored)
    {
        auto* mod = lower_ok(R"(
            i32 add(i32 a, i32 b) { return a + b; }
        )");
        auto* fn = mod->functions[0].get();
        EXPECT_GE(count_opcode(fn, ir::Opcode::Alloca), 2u);
        EXPECT_GE(count_opcode(fn, ir::Opcode::Store), 2u);
    }

    TEST_F(IRLoweringTest, ParamTypes)
    {
        auto* mod = lower_ok(R"(
            void f(i32 a, f64 b, bool c) {}
        )");
        auto* fn = mod->functions[0].get();
        ASSERT_EQ(fn->params.size(), 3u);
        expect_int_type(fn->params[0].type, 32, true);
        expect_float_type(fn->params[1].type, 64);
        expect_bool_type(fn->params[2].type);
    }

    TEST_F(IRLoweringTest, PrimitiveTypeSizes)
    {
        auto* mod = lower_ok("void f() {}");
        auto& ta = mod->types;
        EXPECT_EQ(ta.integer_type(8, true)->size_bytes(), 1u);
        EXPECT_EQ(ta.integer_type(16, true)->size_bytes(), 2u);
        EXPECT_EQ(ta.integer_type(32, true)->size_bytes(), 4u);
        EXPECT_EQ(ta.integer_type(64, true)->size_bytes(), 8u);
        EXPECT_EQ(ta.float_type(32)->size_bytes(), 4u);
        EXPECT_EQ(ta.float_type(64)->size_bytes(), 8u);
        EXPECT_EQ(ta.bool_type()->size_bytes(), 1u);
        EXPECT_EQ(ta.void_type()->size_bytes(), 0u);
    }

    TEST_F(IRLoweringTest, PointerSize)
    {
        auto* mod = lower_ok("void f() {}");
        auto& ta = mod->types;
        auto* ptr = ta.pointer_to(ta.integer_type(32, true));
        EXPECT_EQ(ptr->size_bytes(), 8u);
        EXPECT_EQ(ptr->align_bytes(), 8u);
    }

    TEST_F(IRLoweringTest, AllBlocksTerminatedSimple)
    {
        auto* mod = lower_ok("void f() {}");
        expect_all_terminated(mod->functions[0].get());
    }

    TEST_F(IRLoweringTest, AllBlocksTerminatedComplex)
    {
        auto* mod = lower_ok(R"(
            i32 f(i32 n) {
                i32 sum = 0;
                for (i32 i = 0; i < n; i = i + 1) {
                    if i % 2 == 0 { continue; }
                    if i > 50 { break; }
                    sum += i;
                }
                return sum;
            }
        )");
        auto* fn = mod->functions[0].get();
        expect_all_terminated(fn);
    }

    TEST_F(IRLoweringTest, AllBlocksTerminatedMultiReturn)
    {
        auto* mod = lower_ok(R"(
            i32 f(i32 x) {
                if x > 100 { return 3; }
                else if x > 10 { return 2; }
                else if x > 0 { return 1; }
                else { return 0; }
            }
        )");
        auto* fn = mod->functions[0].get();
        expect_all_terminated(fn);
    }

    TEST_F(IRLoweringTest, AllBlocksTerminatedNestedLoops)
    {
        auto* mod = lower_ok(R"(
            void f() {
                while true {
                    for (i32 i = 0; i < 10; i = i + 1) {
                        if i == 5 { break; }
                    }
                    break;
                }
            }
        )");
        auto* fn = mod->functions[0].get();
        expect_all_terminated(fn);
    }

    TEST_F(IRLoweringTest, PrintModuleNotEmpty)
    {
        auto r = lower("i32 f() { return 42; }");
        ASSERT_TRUE(r.ok);
        EXPECT_FALSE(r.ir_text.empty());
        EXPECT_NE(r.ir_text.find("fn @"), std::string::npos);
        EXPECT_NE(r.ir_text.find("ret"), std::string::npos);
    }

    TEST_F(IRLoweringTest, PrintModuleContainsGlobal)
    {
        auto r = lower("i32 x = 42;");
        ASSERT_TRUE(r.ok);
        EXPECT_NE(r.ir_text.find("global @x"), std::string::npos);
    }

    TEST_F(IRLoweringTest, FullProgramLinkedList)
    {
        auto* mod = lower_ok(R"(
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
        auto* fn = find_func_containing(mod, "f");
        ASSERT_NE(fn, nullptr);
        expect_all_terminated(fn);
        EXPECT_GE(count_opcode(fn, ir::Opcode::GetFieldPtr), 1u);
    }

    TEST_F(IRLoweringTest, FullProgramDotProduct)
    {
        auto* mod = lower_ok(R"(
            module math;
            struct Point { f32 x; f32 y; }
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
        EXPECT_EQ(mod->name, "math");
        auto* main_fn = find_func_containing(mod, "main");
        ASSERT_NE(main_fn, nullptr);
        EXPECT_EQ(main_fn->linkage, ir::Linkage::External);
        expect_all_terminated(main_fn);
    }

    TEST_F(IRLoweringTest, FullProgramFactorial)
    {
        auto* mod = lower_ok(R"(
            i32 factorial(i32 n) {
                if n <= 1 { return 1; }
                return n * factorial(n - 1);
            }
        )");
        auto* fn = mod->functions[0].get();
        expect_all_terminated(fn);
        EXPECT_GE(count_opcode(fn, ir::Opcode::Call), 1u);
        EXPECT_GE(count_opcode(fn, ir::Opcode::Mul), 1u);
        EXPECT_GE(count_opcode(fn, ir::Opcode::Le), 1u);
    }

    TEST_F(IRLoweringTest, FullProgramEnumOption)
    {
        auto* mod = lower_ok(R"(
            enum Option { Some(i32), None }
            i32 unwrap_or(Option o, i32 def) {
                match o {
                    Option::Some(val) => { return val; },
                    Option::None => { return def; }
                }
            }
        )");
        auto* fn = find_func_containing(mod, "unwrap_or");
        ASSERT_NE(fn, nullptr);
        expect_all_terminated(fn);
        EXPECT_GE(count_opcode(fn, ir::Opcode::Eq), 1u);
        EXPECT_GE(count_opcode(fn, ir::Opcode::GetFieldPtr), 1u);
    }

    TEST_F(IRLoweringTest, FullProgramWithDeferAndLoop)
    {
        auto* mod = lower_ok(R"(
            void cleanup() {}
            i32 f() {
                defer cleanup();
                i32 sum = 0;
                for (i32 i = 0; i < 10; i = i + 1) {
                    sum += i;
                }
                return sum;
            }
        )");
        auto* fn = find_func_containing(mod, "f");
        ASSERT_NE(fn, nullptr);
        expect_all_terminated(fn);
        auto calls = all_insts(fn, ir::Opcode::Call);
        bool found_cleanup = false;
        for (auto* c : calls)
            if (c->as_call().callee.find("cleanup") != std::string::npos)
                found_cleanup = true;

        EXPECT_TRUE(found_cleanup);
    }

    TEST_F(IRLoweringTest, FullProgramMultipleStructs)
    {
        auto* mod = lower_ok(R"(
            struct Vec2 { f32 x; f32 y; }
            struct Rect { Vec2 min; Vec2 max; }
            f32 width(Rect r) { return r.max.x - r.min.x; }
            void f() {
                Vec2 lo = { x: 0.0, y: 0.0 };
                Vec2 hi = { x: 10.0, y: 5.0 };
                Rect r = { min: lo, max: hi };
                f32 w = width(r);
            }
        )");
        auto* fn = find_func_containing(mod, "f");
        ASSERT_NE(fn, nullptr);
        expect_all_terminated(fn);
    }

    TEST_F(IRLoweringTest, ManyFunctionsAllTerminated)
    {
        auto* mod = lower_ok(R"(
            void f0() {}
            void f1() { f0(); }
            void f2() { f1(); }
            void f3() { f2(); }
            void f4() { f3(); }
        )");
        for (auto& fn : mod->functions)
            expect_all_terminated(fn.get());
    }

    TEST_F(IRLoweringTest, StressDeeplyNestedUFCS)
    {
        std::string code = R"(
            struct Val { i32 v; }
            Val inc(Val x) { Val r = { v: x.v + 1 }; return r; }
            void f() {
                Val v = { v: 0 };
                v = v)";

        const int depth = 10;
        for (int i = 0; i < depth; ++i)
            code += ".inc()";
        code += ";\n}";

        auto* mod = lower_ok(code);
        auto* fn = find_func_containing(mod, "f");
        ASSERT_NE(fn, nullptr);

        auto calls = all_insts(fn, ir::Opcode::Call);
        EXPECT_EQ(calls.size(), depth) << "Expected exactly " << depth << " chained UFCS calls";
        expect_all_terminated(fn);
    }

    TEST_F(IRLoweringTest, StressExtensiveMatchArms)
    {
        std::string code = "i32 f(i32 x) {\n    match x {\n";
        const int arms = 150;
        for (int i = 0; i < arms; ++i)
        {
            code += std::format("        {} => {{ return {}; }},\n", i, i);
        }
        code += "        _ => { return -1; }\n    }\n}";

        auto* mod = lower_ok(code);
        auto* fn = mod->functions[0].get();

        EXPECT_GE(block_count(fn), arms);
        EXPECT_GE(count_opcode(fn, ir::Opcode::Eq), arms);
        expect_all_terminated(fn);
    }

    TEST_F(IRLoweringTest, StressTemplateMonomorphizationDiversity)
    {
        std::string code = R"(
            T identity(T)(T x) { return x; }
            void f() {
                i8   v1 = identity!(i8)(0 as i8);
                i16  v2 = identity!(i16)(0 as i16);
                i32  v3 = identity!(i32)(0);
                i64  v4 = identity!(i64)(0 as i64);
                u8   v5 = identity!(u8)(0 as u8);
                u16  v6 = identity!(u16)(0 as u16);
                u32  v7 = identity!(u32)(0 as u32);
                u64  v8 = identity!(u64)(0 as u64);
                f32  v9 = identity!(f32)(0.0 as f32);
                f64 v10 = identity!(f64)(0.0);
                bool v11 = identity!bool(false);
            }
        )";

        auto* mod = lower_ok(code);
        auto* fn = find_func_containing(mod, "f");
        ASSERT_NE(fn, nullptr);

        auto calls = all_insts(fn, ir::Opcode::Call);
        EXPECT_EQ(calls.size(), 11u);
    }

    TEST_F(IRLoweringTest, StressGiantStructAndFieldAccess)
    {
        std::string code = "struct Giant {\n";
        const int fields = 200;
        for (int i = 0; i < fields; ++i)
            code += std::format("    i32 fxx{};\n", i);

        code += "}\ni32 extract(Giant* g) {\n";
        code += std::format("    return g.fxx{};\n", fields - 1);
        code += "}";

        auto* mod = lower_ok(code);
        auto* fn = mod->functions[0].get();

        auto gfps = all_insts(fn, ir::Opcode::GetFieldPtr);
        ASSERT_GE(gfps.size(), 1u);

        const auto* struct_ty = gfps[0]->type;
        while (struct_ty->is_pointer())
            struct_ty = static_cast<const ir::PointerType*>(struct_ty)->pointee;

        expect_all_terminated(fn);
    }

    TEST_F(IRLoweringTest, StressHeavyDeferChaining)
    {
        std::string code = "void tick() {}\nvoid f() {\n";
        const int defers = 100;
        for (int i = 0; i < defers; ++i)
        {
            code += "    defer tick();\n";
        }
        code += "}";

        auto* mod = lower_ok(code);
        auto* fn = find_func_containing(mod, "f");
        ASSERT_NE(fn, nullptr);

        auto calls = all_insts(fn, ir::Opcode::Call);
        EXPECT_EQ(calls.size(), defers) << "Expected exact number of deferred cleanup calls";
        expect_all_terminated(fn);
    }

    TEST_F(IRLoweringTest, StressDeeplyNestedScopes)
    {
        std::string code = "void f() {";
        const int depth = 50;
        for (int i = 0; i < depth; ++i)
            code += " { i32 x = " + std::to_string(i) + "; ";
        for (int i = 0; i < depth; ++i)
            code += "} ";
        code += "}";

        auto* mod = lower_ok(code);
        auto* fn = mod->functions[0].get();

        EXPECT_GE(count_opcode(fn, ir::Opcode::Alloca), depth);
        EXPECT_GE(count_opcode(fn, ir::Opcode::Store), depth);
        expect_all_terminated(fn);
    }

    TEST_F(IRLoweringTest, StressChainedTemplateDependencies)
    {
        std::string code = R"(
            void sink(T)(T val) {}
            void pass3(T)(T val) { sink!(T)(val); }
            void pass2(T)(T val) { pass3!(T)(val); }
            void pass1(T)(T val) { pass2!(T)(val); }

            void start() {
                pass1!(i32)(42);
                pass1!(f64)(3.14);
            }
        )";

        auto* mod = lower_ok(code);

        EXPECT_EQ(mod->functions.size(), 9u) << "Monomorphizer did not correctly traverse and flush transitively instantiated templates.";

        auto* fn = find_func_containing(mod, "start");
        expect_all_terminated(fn);
    }

} // namespace dcc::test
