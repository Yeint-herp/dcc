#include <cassert>
#include <ir/lowering.hh>
#include <sema/types.hh>

namespace dcc::ir
{
    TypeRef IRLowering::lower_primitive(ast::PrimitiveKind pk)
    {
        auto& ta = m_module->types;
        switch (pk)
        {
            case ast::PrimitiveKind::I8:
                return ta.integer_type(8, true);
            case ast::PrimitiveKind::U8:
                return ta.integer_type(8, false);
            case ast::PrimitiveKind::I16:
                return ta.integer_type(16, true);
            case ast::PrimitiveKind::U16:
                return ta.integer_type(16, false);
            case ast::PrimitiveKind::I32:
                return ta.integer_type(32, true);
            case ast::PrimitiveKind::U32:
                return ta.integer_type(32, false);
            case ast::PrimitiveKind::I64:
                return ta.integer_type(64, true);
            case ast::PrimitiveKind::U64:
                return ta.integer_type(64, false);
            case ast::PrimitiveKind::F32:
                return ta.float_type(32);
            case ast::PrimitiveKind::F64:
                return ta.float_type(64);
            case ast::PrimitiveKind::Bool:
                return ta.bool_type();
            case ast::PrimitiveKind::Void:
                return ta.void_type();
            case ast::PrimitiveKind::NullT:
                return ta.pointer_to(ta.void_type());
        }
        return ta.void_type();
    }

    TypeRef IRLowering::lower_type(const sema::SemaType* sty)
    {
        if (!sty)
            return m_module->types.void_type();

        sty = m_types.resolve(const_cast<sema::SemaType*>(sty));

        if (auto it = m_mono_subst.find(sty); it != m_mono_subst.end())
            return lower_type(it->second);

        if (auto it = m_type_cache.find(sty); it != m_type_cache.end())
            return it->second;

        auto& ta = m_module->types;
        TypeRef result = nullptr;

        switch (sty->kind())
        {
            case sema::SemaType::Kind::Error:
                result = ta.void_type();
                break;

            case sema::SemaType::Kind::Void:
                result = ta.void_type();
                break;

            case sema::SemaType::Kind::Bool:
                result = ta.bool_type();
                break;

            case sema::SemaType::Kind::NullT:
                result = ta.pointer_to(ta.void_type());
                break;

            case sema::SemaType::Kind::Integer: {
                auto* it = static_cast<const sema::IntegerType*>(sty);
                result = ta.integer_type(it->width(), it->is_signed());
                break;
            }

            case sema::SemaType::Kind::Float: {
                auto* ft = static_cast<const sema::FloatType*>(sty);
                result = ta.float_type(ft->width());
                break;
            }

            case sema::SemaType::Kind::Pointer: {
                auto* pt = static_cast<const sema::PointerSemaType*>(sty);
                result = ta.pointer_to(lower_type(pt->pointee()));
                break;
            }

            case sema::SemaType::Kind::Slice: {
                auto* st = static_cast<const sema::SliceSemaType*>(sty);
                result = ta.make_slice_type(lower_type(st->element()));
                break;
            }

            case sema::SemaType::Kind::Array: {
                auto* at = static_cast<const sema::ArraySemaType*>(sty);
                result = ta.array_of(lower_type(at->element()), at->length());
                break;
            }

            case sema::SemaType::Kind::FlexibleArray: {
                auto* fa = static_cast<const sema::FlexibleArraySemaType*>(sty);
                result = ta.array_of(lower_type(fa->element()), 0);
                break;
            }

            case sema::SemaType::Kind::Function: {
                auto* ft = static_cast<const sema::FunctionSemaType*>(sty);
                result = lower_function_type(ft);
                break;
            }

            case sema::SemaType::Kind::Struct: {
                auto* st = static_cast<const sema::StructSemaType*>(sty);
                result = lower_struct_type(st);
                break;
            }

            case sema::SemaType::Kind::Union: {
                auto* ut = static_cast<const sema::UnionSemaType*>(sty);
                result = lower_union_type(ut);
                break;
            }

            case sema::SemaType::Kind::Enum: {
                auto* et = static_cast<const sema::EnumSemaType*>(sty);
                result = lower_enum_type(et);
                break;
            }

            case sema::SemaType::Kind::Alias:
                result = lower_type(static_cast<const sema::AliasType*>(sty)->underlying());
                break;

            case sema::SemaType::Kind::TypeVar: {
                return m_module->types.integer_type(32, true);
            }
        }

        m_type_cache[sty] = result;
        return result;
    }

    TypeRef IRLowering::lower_function_type(const sema::FunctionSemaType* fty)
    {
        auto ret = lower_type(fty->return_type());

        std::vector<TypeRef> params;
        params.reserve(fty->param_types().size());
        for (auto* p : fty->param_types())
            params.push_back(lower_type(p));

        return m_module->types.make_function(ret, std::move(params), fty->is_variadic());
    }

    StructType* IRLowering::lower_struct_type(const sema::StructSemaType* sty)
    {
        auto name = std::string{sty->name().view()};

        auto* ir_struct = m_module->types.make_struct(name);
        m_type_cache[sty] = ir_struct;

        for (auto& f : sty->fields())
        {
            ir_struct->fields.push_back({
                .name = std::string{f.name.view()},
                .type = lower_type(f.type),
                .offset = 0,
            });
        }

        ir_struct->compute_layout();
        return ir_struct;
    }

    StructType* IRLowering::lower_union_type(const sema::UnionSemaType* uty)
    {
        auto name = std::string{uty->name().view()};

        auto* ir_union = m_module->types.make_struct(name);
        m_type_cache[uty] = ir_union;

        uint64_t max_size = 0;
        uint64_t max_align = 1;

        for (auto& f : uty->fields())
        {
            auto* ft = lower_type(f.type);
            max_size = std::max(max_size, ft->size_bytes());
            max_align = std::max(max_align, ft->align_bytes());
        }

        auto* payload = m_module->types.array_of(m_module->types.integer_type(8, false), max_size);

        ir_union->fields.push_back({.name = "data", .type = payload, .offset = 0});
        ir_union->size = max_size;
        ir_union->align = max_align;
        return ir_union;
    }

    StructType* IRLowering::lower_enum_type(const sema::EnumSemaType* ety)
    {
        auto* tag_ir = lower_type(ety->underlying_type());

        uint64_t payload_size = 0;
        uint64_t payload_align = 1;

        for (auto& v : ety->variants())
        {
            uint64_t variant_size = 0;
            uint64_t variant_align = 1;

            for (auto* pt : v.payload_types)
            {
                auto* irt = lower_type(pt);
                uint64_t a = irt->align_bytes();
                variant_size = ((variant_size + a - 1) & ~(a - 1)) + irt->size_bytes();
                variant_align = std::max(variant_align, a);
            }

            payload_size = std::max(payload_size, variant_size);
            payload_align = std::max(payload_align, variant_align);
        }

        auto name = std::string{ety->name().view()};
        auto* ir_enum = m_module->types.make_enum_type(std::move(name), tag_ir, payload_size, payload_align);

        m_type_cache[ety] = ir_enum;
        return ir_enum;
    }

} // namespace dcc::ir
