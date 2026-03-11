#include <cassert>
#include <sema/type_context.hh>
#include <utility>

namespace dcc::sema
{
    TypeContext::TypeContext()
    : m_integers{
          IntegerType{8, true},
          IntegerType{8, false},
          IntegerType{16, true}, 
          IntegerType{16, false},
          IntegerType{32, true}, 
          IntegerType{32, false},
          IntegerType{64, true}, 
          IntegerType{64, false},
      },
      m_f32{32},
      m_f64{64}
    {
    }

    IntegerType* TypeContext::integer_type(uint8_t width, bool is_signed) noexcept
    {
        int idx = 0;

        switch (width)
        {
            case 8:
                idx = 0;
                break;
            case 16:
                idx = 2;
                break;
            case 32:
                idx = 4;
                break;
            case 64:
                idx = 6;
                break;
            default:
                std::unreachable();
        }

        if (!is_signed)
            idx += 1;

        return &m_integers[idx];
    }

    FloatType* TypeContext::float_type(uint8_t width) noexcept
    {
        switch (width)
        {
            case 32:
                return &m_f32;
            case 64:
                return &m_f64;
            default:
                std::unreachable();
        }
    }

    SemaType* TypeContext::from_primitive(ast::PrimitiveKind pk) noexcept
    {
        using PK = ast::PrimitiveKind;
        switch (pk)
        {
            case PK::I8:
                return integer_type(8, true);
            case PK::U8:
                return integer_type(8, false);
            case PK::I16:
                return integer_type(16, true);
            case PK::U16:
                return integer_type(16, false);
            case PK::I32:
                return integer_type(32, true);
            case PK::U32:
                return integer_type(32, false);
            case PK::I64:
                return integer_type(64, true);
            case PK::U64:
                return integer_type(64, false);
            case PK::F32:
                return float_type(32);
            case PK::F64:
                return float_type(64);
            case PK::Bool:
                return bool_type();
            case PK::Void:
                return void_type();
            case PK::NullT:
                return null_t_type();
        }
        return error_type();
    }

    template <typename T, typename... Args> T* TypeContext::alloc(Args&&... args)
    {
        auto ptr = std::make_unique<T>(std::forward<Args>(args)...);
        T* raw = ptr.get();
        m_arena.push_back(std::move(ptr));

        return raw;
    }

    uint64_t TypeContext::TypeHash::operator()(const SemaType* ty) const noexcept
    {
        return ty->structural_hash();
    }

    bool TypeContext::TypeEq::operator()(const SemaType* a, const SemaType* b) const noexcept
    {
        return a->structural_eq(*b);
    }

    SemaType* TypeContext::intern_or_create(std::unique_ptr<SemaType> candidate)
    {
        auto it = m_intern_map.find(candidate.get());
        if (it != m_intern_map.end())
            return it->second;

        SemaType* raw = candidate.get();
        m_arena.push_back(std::move(candidate));
        m_intern_map[raw] = raw;
        return raw;
    }

    TypeLayout TypeContext::layout_of(SemaType* ty)
    {
        ty = resolve(ty);
        auto it = m_layout_cache.find(ty);
        if (it != m_layout_cache.end())
            return it->second;

        auto layout = compute_layout(ty);
        m_layout_cache[ty] = layout;
        return layout;
    }

    TypeLayout TypeContext::compute_layout(SemaType* ty)
    {
        if (ty->is_bool())
            return TypeLayout::of(1, 1);
        if (ty->is_void())
            return TypeLayout::of(0, 1);
        if (ty->is_error())
            return TypeLayout::of(0, 1);
        if (ty->is_null_t())
            return TypeLayout::of(8, 8);

        if (ty->is_integer())
        {
            auto* ity = static_cast<IntegerType*>(ty);
            uint64_t bytes = ity->width() / 8;
            return TypeLayout::of(bytes, bytes);
        }

        if (ty->is_float())
        {
            auto* fty = static_cast<FloatType*>(ty);
            uint64_t bytes = fty->width() / 8;
            return TypeLayout::of(bytes, bytes);
        }

        if (ty->is_pointer())
            return TypeLayout::of(8, 8);

        if (ty->is_slice())
            return TypeLayout::of(16, 8);

        if (ty->is_array())
        {
            auto* aty = static_cast<ArraySemaType*>(ty);
            auto elem = layout_of(aty->element());
            return TypeLayout::of(elem.size * aty->length(), elem.alignment);
        }

        if (dynamic_cast<FlexibleArraySemaType*>(ty))
        {
            auto* faty = static_cast<FlexibleArraySemaType*>(ty);
            auto elem = layout_of(faty->element());
            return TypeLayout::of(0, elem.alignment);
        }

        if (ty->is_function())
            return TypeLayout::of(8, 8);

        if (auto* sty = dynamic_cast<StructSemaType*>(ty))
            return layout_struct(sty);
        if (auto* uty = dynamic_cast<UnionSemaType*>(ty))
            return layout_union(uty);
        if (auto* ety = dynamic_cast<EnumSemaType*>(ty))
            return layout_enum(ety);

        return TypeLayout::of(0, 1);
    }

    static uint64_t align_to(uint64_t offset, uint64_t alignment)
    {
        return (offset + alignment - 1) & ~(alignment - 1);
    }

    TypeLayout TypeContext::layout_struct(StructSemaType* sty)
    {
        assert(sty->is_complete() && "cannot layout incomplete struct");

        uint64_t offset = 0;
        uint64_t max_align = 1;

        for (auto& field : sty->fields())
        {
            auto fl = layout_of(field.type);
            offset = align_to(offset, fl.alignment);
            offset += fl.size;
            max_align = std::max(max_align, fl.alignment);
        }

        offset = align_to(offset, max_align);
        return TypeLayout::of(offset, max_align);
    }

    TypeLayout TypeContext::layout_union(UnionSemaType* uty)
    {
        assert(uty->is_complete() && "cannot layout incomplete union");

        uint64_t max_size = 0;
        uint64_t max_align = 1;

        for (auto& field : uty->fields())
        {
            auto fl = layout_of(field.type);
            max_size = std::max(max_size, fl.size);
            max_align = std::max(max_align, fl.alignment);
        }

        max_size = align_to(max_size, max_align);
        return TypeLayout::of(max_size, max_align);
    }

    TypeLayout TypeContext::layout_enum(EnumSemaType* ety)
    {
        assert(ety->is_complete() && "cannot layout incomplete enum");

        auto disc = layout_of(ety->underlying_type());
        uint64_t max_payload_size = 0;
        uint64_t max_align = disc.alignment;

        for (auto& variant : ety->variants())
        {
            uint64_t variant_size = 0;
            for (auto* pt : variant.payload_types)
            {
                auto pl = layout_of(pt);
                variant_size = align_to(variant_size, pl.alignment);
                variant_size += pl.size;
                max_align = std::max(max_align, pl.alignment);
            }
            max_payload_size = std::max(max_payload_size, variant_size);
        }

        uint64_t total = disc.size;
        if (max_payload_size > 0)
        {
            total = align_to(total, max_align);
            total += max_payload_size;
        }

        total = align_to(total, max_align);
        return TypeLayout::of(total, max_align);
    }

    PointerSemaType* TypeContext::pointer_to(SemaType* pointee, ast::Qualifier quals)
    {
        auto candidate = std::make_unique<PointerSemaType>(pointee, quals);
        return static_cast<PointerSemaType*>(intern_or_create(std::move(candidate)));
    }

    SliceSemaType* TypeContext::slice_of(SemaType* element)
    {
        auto candidate = std::make_unique<SliceSemaType>(element);
        return static_cast<SliceSemaType*>(intern_or_create(std::move(candidate)));
    }

    ArraySemaType* TypeContext::array_of(SemaType* element, uint64_t length)
    {
        auto candidate = std::make_unique<ArraySemaType>(element, length);
        return static_cast<ArraySemaType*>(intern_or_create(std::move(candidate)));
    }

    FlexibleArraySemaType* TypeContext::flexible_array_of(SemaType* element)
    {
        auto candidate = std::make_unique<FlexibleArraySemaType>(element);
        return static_cast<FlexibleArraySemaType*>(intern_or_create(std::move(candidate)));
    }

    FunctionSemaType* TypeContext::function_type(SemaType* ret, std::vector<SemaType*> params, bool variadic)
    {
        auto candidate = std::make_unique<FunctionSemaType>(ret, std::move(params), variadic);
        return static_cast<FunctionSemaType*>(intern_or_create(std::move(candidate)));
    }

    StructSemaType* TypeContext::make_struct(si::InternedString name)
    {
        return alloc<StructSemaType>(name);
    }

    UnionSemaType* TypeContext::make_union(si::InternedString name)
    {
        return alloc<UnionSemaType>(name);
    }

    EnumSemaType* TypeContext::make_enum(si::InternedString name, SemaType* underlying)
    {
        return alloc<EnumSemaType>(name, underlying);
    }

    AliasType* TypeContext::make_alias(si::InternedString name, SemaType* underlying)
    {
        return alloc<AliasType>(name, underlying);
    }

    TypeVar* TypeContext::fresh_var()
    {
        return alloc<TypeVar>(m_next_var_id++);
    }

    SemaType* TypeContext::find_root(SemaType* ty) noexcept
    {
        if (!ty)
            return nullptr;

        if (ty->kind() != SemaType::Kind::TypeVar)
            return ty;

        auto* var = static_cast<TypeVar*>(ty);
        if (!var->parent())
            return var;

        SemaType* root = find_root(var->parent());
        if (root != var->parent())
            var->set_parent(root);

        return root;
    }

    SemaType* TypeContext::resolve(SemaType* ty) noexcept
    {
        SemaType* resolved = find_root(ty);
        return strip_alias(resolved);
    }

    bool TypeContext::unify(SemaType* a, SemaType* b)
    {
        a = find_root(a);
        b = find_root(b);

        if (a == b)
            return true;

        if (a->is_error() || b->is_error())
            return true;

        if (a->is_type_var())
        {
            static_cast<TypeVar*>(a)->set_parent(b);
            return true;
        }

        if (b->is_type_var())
        {
            static_cast<TypeVar*>(b)->set_parent(a);
            return true;
        }

        SemaType* ca = strip_alias(a);
        SemaType* cb = strip_alias(b);
        if (ca == cb)
            return true;

        if (ca->kind() != cb->kind())
            return false;

        switch (ca->kind())
        {
            case SemaType::Kind::Pointer: {
                auto* pa = static_cast<PointerSemaType*>(ca);
                auto* pb = static_cast<PointerSemaType*>(cb);

                return pa->quals() == pb->quals() && unify(pa->pointee(), pb->pointee());
            }
            case SemaType::Kind::Slice: {
                auto* sa = static_cast<SliceSemaType*>(ca);
                auto* sb = static_cast<SliceSemaType*>(cb);

                return unify(sa->element(), sb->element());
            }
            case SemaType::Kind::Array: {
                auto* aa = static_cast<ArraySemaType*>(ca);
                auto* ab = static_cast<ArraySemaType*>(cb);

                return aa->length() == ab->length() && unify(aa->element(), ab->element());
            }
            case SemaType::Kind::Function: {
                auto* fa = static_cast<FunctionSemaType*>(ca);
                auto* fb = static_cast<FunctionSemaType*>(cb);

                if (fa->is_variadic() != fb->is_variadic())
                    return false;

                if (fa->param_types().size() != fb->param_types().size())
                    return false;

                if (!unify(fa->return_type(), fb->return_type()))
                    return false;

                for (std::size_t i = 0; i < fa->param_types().size(); ++i)
                    if (!unify(fa->param_types()[i], fb->param_types()[i]))
                        return false;

                return true;
            }
            default:
                return ca == cb;
        }
    }

    bool TypeContext::types_equal(SemaType* a, SemaType* b) noexcept
    {
        a = resolve(a);
        b = resolve(b);
        return a == b;
    }

    bool TypeContext::is_implicitly_convertible(SemaType* from, SemaType* to) const noexcept
    {
        from = strip_alias(from);
        to = strip_alias(to);

        if (from == to)
            return true;

        if (from->is_error() || to->is_error())
            return true;

        if (from->is_null_t() && to->is_pointer())
            return true;

        if (from->is_integer() && to->is_integer())
        {
            auto* fi = static_cast<const IntegerType*>(from);
            auto* ti = static_cast<const IntegerType*>(to);
            if (fi->is_signed() == ti->is_signed() && fi->width() <= ti->width())
                return true;

            if (!fi->is_signed() && ti->is_signed() && fi->width() < ti->width())
                return true;
        }

        if (from->is_float() && to->is_float())
        {
            auto* ff = static_cast<const FloatType*>(from);
            auto* tf = static_cast<const FloatType*>(to);
            if (ff->width() <= tf->width())
                return true;
        }

        if (from->is_array() && to->is_slice())
        {
            auto* fa = static_cast<const ArraySemaType*>(from);
            auto* ts = static_cast<const SliceSemaType*>(to);
            if (fa->element() == ts->element())
                return true;
        }

        if (from->is_array() && to->is_pointer())
        {
            auto* fa = static_cast<const ArraySemaType*>(from);
            auto* tp = static_cast<const PointerSemaType*>(to);
            if (fa->element() == tp->pointee())
                return true;
        }

        return false;
    }

    bool TypeContext::is_explicitly_castable(SemaType* from, SemaType* to) const noexcept
    {
        from = strip_alias(from);
        to = strip_alias(to);

        if (is_implicitly_convertible(from, to))
            return true;

        if (from->is_error() || to->is_error())
            return true;

        if (from->is_numeric() && to->is_numeric())
            return true;

        if (from->is_integer() && to->is_bool())
            return true;

        if (from->is_bool() && to->is_integer())
            return true;

        if (from->is_pointer() && to->is_pointer())
            return true;

        if (from->is_integer() && to->is_pointer())
            return true;

        if (from->is_pointer() && to->is_integer())
            return true;

        if (from->is_enum() && to->is_integer())
            return true;

        if (from->is_integer() && to->is_enum())
            return true;

        return false;
    }

    SemaType* TypeContext::common_arithmetic_type(SemaType* a, SemaType* b) noexcept
    {
        a = resolve(a);
        b = resolve(b);

        if (a->is_error())
            return a;

        if (b->is_error())
            return b;

        if (a->is_float() || b->is_float())
        {
            uint8_t wa = a->is_float() ? static_cast<FloatType*>(a)->width() : 0;
            uint8_t wb = b->is_float() ? static_cast<FloatType*>(b)->width() : 0;

            uint8_t w = std::max(wa, wb);

            if (w < 64 && (a->is_float() && b->is_float()))
                return float_type(std::max(wa, wb));

            if (a->is_float() && !b->is_float())
                return a;

            if (b->is_float() && !a->is_float())
                return b;

            return float_type(std::max(wa, wb));
        }

        return widen_integers(a, b);
    }

    SemaType* TypeContext::widen_integers(SemaType* a, SemaType* b) noexcept
    {
        a = resolve(a);
        b = resolve(b);

        if (!a->is_integer() || !b->is_integer())
            return nullptr;

        auto* ia = static_cast<IntegerType*>(a);
        auto* ib = static_cast<IntegerType*>(b);

        if (ia == ib)
            return ia;

        if (ia->is_signed() == ib->is_signed())
            return ia->width() >= ib->width() ? a : b;

        IntegerType* si_ty = ia->is_signed() ? ia : ib;
        IntegerType* ui_ty = ia->is_signed() ? ib : ia;

        if (ui_ty->width() < si_ty->width())
            return si_ty;

        return integer_type(std::max(ui_ty->width(), si_ty->width()), false);
    }

} // namespace dcc::sema
