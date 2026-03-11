#include <gtest/gtest.h>
#include <ir/mangle.hh>
#include <sema/types.hh>
#include <util/si.hh>

using namespace dcc::ir;
using namespace dcc::sema;
using namespace dcc::si;

namespace dcc::test
{
    class MangleTest : public ::testing::Test
    {
    protected:
        StringInterner interner;

        IntegerType i8{8, true};
        IntegerType u8{8, false};
        IntegerType i16{16, true};
        IntegerType u16{16, false};
        IntegerType i32{32, true};
        IntegerType u32{32, false};
        IntegerType i64{64, true};
        IntegerType u64{64, false};
        FloatType f32{32};
        FloatType f64{64};
        BoolType bool_ty;
        VoidType void_ty;
        NullTType null_t_ty;

        InternedString intern(const char* s) { return interner.intern(s); }
    };

    TEST_F(MangleTest, MainIsUnmangled)
    {
        auto result = Mangler::mangle_function({}, "main", {});
        EXPECT_EQ(result, "main");
    }

    TEST_F(MangleTest, SimpleFunction)
    {
        InternedString mod = intern("math");
        std::span<const InternedString> path{&mod, 1};

        SemaType* params[] = {&i32, &i32};
        auto result = Mangler::mangle_function(path, "add", params);
        EXPECT_EQ(result, "_D4math_3addii");
    }

    TEST_F(MangleTest, NoParams)
    {
        InternedString mod = intern("core");
        std::span<const InternedString> path{&mod, 1};

        auto result = Mangler::mangle_function(path, "init", {});
        EXPECT_EQ(result, "_D4core_4initv");
    }

    TEST_F(MangleTest, AllPrimitiveTypes)
    {
        InternedString mod = intern("test");
        std::span<const InternedString> path{&mod, 1};

        {
            SemaType* p[] = {&i8};
            EXPECT_EQ(Mangler::mangle_function(path, "f", p), "_D4test_1fa");
        }
        {
            SemaType* p[] = {&u8};
            EXPECT_EQ(Mangler::mangle_function(path, "f", p), "_D4test_1fh");
        }
        {
            SemaType* p[] = {&i16};
            EXPECT_EQ(Mangler::mangle_function(path, "f", p), "_D4test_1fs");
        }
        {
            SemaType* p[] = {&u16};
            EXPECT_EQ(Mangler::mangle_function(path, "f", p), "_D4test_1ft");
        }
        {
            SemaType* p[] = {&i32};
            EXPECT_EQ(Mangler::mangle_function(path, "f", p), "_D4test_1fi");
        }
        {
            SemaType* p[] = {&u32};
            EXPECT_EQ(Mangler::mangle_function(path, "f", p), "_D4test_1fj");
        }
        {
            SemaType* p[] = {&i64};
            EXPECT_EQ(Mangler::mangle_function(path, "f", p), "_D4test_1fl");
        }
        {
            SemaType* p[] = {&u64};
            EXPECT_EQ(Mangler::mangle_function(path, "f", p), "_D4test_1fm");
        }
        {
            SemaType* p[] = {&f32};
            EXPECT_EQ(Mangler::mangle_function(path, "f", p), "_D4test_1ff");
        }
        {
            SemaType* p[] = {&f64};
            EXPECT_EQ(Mangler::mangle_function(path, "f", p), "_D4test_1fd");
        }
        {
            SemaType* p[] = {&bool_ty};
            EXPECT_EQ(Mangler::mangle_function(path, "f", p), "_D4test_1fb");
        }
        {
            SemaType* p[] = {&null_t_ty};
            EXPECT_EQ(Mangler::mangle_function(path, "f", p), "_D4test_1fn");
        }
    }

    TEST_F(MangleTest, PointerType)
    {
        InternedString mod = intern("mem");
        std::span<const InternedString> path{&mod, 1};

        PointerSemaType ptr_i32{&i32};
        SemaType* params[] = {&ptr_i32};
        EXPECT_EQ(Mangler::mangle_function(path, "free", params), "_D3mem_4freePi");
    }

    TEST_F(MangleTest, SliceType)
    {
        InternedString mod = intern("io");
        std::span<const InternedString> path{&mod, 1};

        SliceSemaType slice_u8{&u8};
        SemaType* params[] = {&slice_u8};
        EXPECT_EQ(Mangler::mangle_function(path, "write", params), "_D2io_5writeSh");
    }

    TEST_F(MangleTest, ArrayType)
    {
        InternedString mod = intern("buf");
        std::span<const InternedString> path{&mod, 1};

        ArraySemaType arr{&u8, 256};
        SemaType* params[] = {&arr};
        EXPECT_EQ(Mangler::mangle_function(path, "clear", params), "_D3buf_5clearA256_h");
    }

    TEST_F(MangleTest, FlexibleArrayType)
    {
        InternedString mod = intern("buf");
        std::span<const InternedString> path{&mod, 1};

        FlexibleArraySemaType fam{&i32};
        SemaType* params[] = {&fam};
        EXPECT_EQ(Mangler::mangle_function(path, "scan", params), "_D3buf_4scanA0_i");
    }

    TEST_F(MangleTest, StructType)
    {
        InternedString mod = intern("geom");
        std::span<const InternedString> path{&mod, 1};

        StructSemaType point{intern("Point")};
        PointerSemaType ptr_point{&point};
        SemaType* params[] = {&ptr_point, &f64};
        EXPECT_EQ(Mangler::mangle_function(path, "scale", params), "_D4geom_5scaleP5Pointd");
    }

    TEST_F(MangleTest, UnionType)
    {
        InternedString mod = intern("sys");
        std::span<const InternedString> path{&mod, 1};

        UnionSemaType val{intern("Value")};
        SemaType* params[] = {&val};
        EXPECT_EQ(Mangler::mangle_function(path, "inspect", params), "_D3sys_7inspectU5Value");
    }

    TEST_F(MangleTest, EnumType)
    {
        InternedString mod = intern("opt");
        std::span<const InternedString> path{&mod, 1};

        EnumSemaType color{intern("Color"), &u8};
        SemaType* params[] = {&color};
        EXPECT_EQ(Mangler::mangle_function(path, "print", params), "_D3opt_5printE5Color");
    }

    TEST_F(MangleTest, FunctionPointerType)
    {
        InternedString mod = intern("cb");
        std::span<const InternedString> path{&mod, 1};

        std::vector<SemaType*> fn_params = {&i32, &i32};
        FunctionSemaType fn_ty{&bool_ty, fn_params};
        SemaType* params[] = {&fn_ty};
        EXPECT_EQ(Mangler::mangle_function(path, "apply", params), "_D2cb_5applyFiiZb");
    }

    TEST_F(MangleTest, NestedModulePath)
    {
        InternedString segs[] = {intern("std"), intern("io"), intern("net")};
        std::span<const InternedString> path{segs};

        SemaType* params[] = {&i32};
        EXPECT_EQ(Mangler::mangle_function(path, "connect", params), "_D3std2io3net_7connecti");
    }

    TEST_F(MangleTest, TemplateArgs)
    {
        InternedString mod = intern("algo");
        std::span<const InternedString> path{&mod, 1};

        SliceSemaType slice_i32{&i32};
        SemaType* params[] = {&slice_i32};
        SemaType* targs[] = {&i32};
        EXPECT_EQ(Mangler::mangle_function(path, "sort", params, targs), "_D4algo_4sortTiZSi");
    }

    TEST_F(MangleTest, MultipleTemplateArgs)
    {
        InternedString mod = intern("map");
        std::span<const InternedString> path{&mod, 1};

        StructSemaType map_ty{intern("Map")};
        PointerSemaType ptr_map{&map_ty};
        SemaType* params[] = {&ptr_map, &u64, &f32};
        SemaType* targs[] = {&u64, &f32};
        EXPECT_EQ(Mangler::mangle_function(path, "insert", params, targs), "_D3map_6insertTmfZP3Mapmf");
    }

    TEST_F(MangleTest, UFCSIsJustAFunction)
    {
        InternedString mod = intern("collections");
        std::span<const InternedString> path{&mod, 1};

        StructSemaType vec_ty{intern("Vec")};
        PointerSemaType ptr_vec{&vec_ty};
        SemaType* params[] = {&ptr_vec, &i32};
        EXPECT_EQ(Mangler::mangle_function(path, "push", params), "_D11collections_4pushP3Veci");
    }

    TEST_F(MangleTest, OverloadDistinction)
    {
        InternedString mod = intern("math");
        std::span<const InternedString> path{&mod, 1};

        SemaType* params_ii[] = {&i32, &i32};
        SemaType* params_ff[] = {&f64, &f64};

        auto mangled_int = Mangler::mangle_function(path, "add", params_ii);
        auto mangled_float = Mangler::mangle_function(path, "add", params_ff);

        EXPECT_NE(mangled_int, mangled_float);
        EXPECT_EQ(mangled_int, "_D4math_3addii");
        EXPECT_EQ(mangled_float, "_D4math_3adddd");
    }

    TEST_F(MangleTest, DemangleSimple)
    {
        EXPECT_EQ(Mangler::demangle("_D4math_3addii"), "math::add(i32, i32)");
    }

    TEST_F(MangleTest, DemangleNoParams)
    {
        EXPECT_EQ(Mangler::demangle("_D4core_4initv"), "core::init()");
    }

    TEST_F(MangleTest, DemanglePointer)
    {
        EXPECT_EQ(Mangler::demangle("_D3mem_4freePi"), "mem::free(*i32)");
    }

    TEST_F(MangleTest, DemangleSlice)
    {
        EXPECT_EQ(Mangler::demangle("_D2io_5writeSh"), "io::write([]u8)");
    }

    TEST_F(MangleTest, DemangleArray)
    {
        EXPECT_EQ(Mangler::demangle("_D3buf_5clearA256_h"), "buf::clear(u8[256])");
    }

    TEST_F(MangleTest, DemangleNestedModule)
    {
        EXPECT_EQ(Mangler::demangle("_D3std2io3net_7connecti"), "std::io::net::connect(i32)");
    }

    TEST_F(MangleTest, DemangleTemplate)
    {
        EXPECT_EQ(Mangler::demangle("_D4algo_4sortTiZSi"), "algo::sort!(i32)([]i32)");
    }

    TEST_F(MangleTest, DemangleMultipleTemplateArgs)
    {
        EXPECT_EQ(Mangler::demangle("_D3map_6insertTmfZP3Mapmf"), "map::insert!(u64, f32)(*Map, u64, f32)");
    }

    TEST_F(MangleTest, DemangleFunctionPointer)
    {
        EXPECT_EQ(Mangler::demangle("_D2cb_5applyFiiZb"), "cb::apply(fn(i32, i32) -> bool)");
    }

    TEST_F(MangleTest, DemanglePassthroughUnmangled)
    {
        EXPECT_EQ(Mangler::demangle("main"), "main");
        EXPECT_EQ(Mangler::demangle("printf"), "printf");
        EXPECT_EQ(Mangler::demangle("_Z"), "_Z");
    }

    TEST_F(MangleTest, DemangleFAM)
    {
        EXPECT_EQ(Mangler::demangle("_D3buf_4scanA0_i"), "buf::scan(i32[])");
    }

} // namespace dcc::test
