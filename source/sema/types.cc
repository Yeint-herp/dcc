#include <algorithm>
#include <format>
#include <functional>
#include <sema/types.hh>

namespace dcc::sema
{
    std::string ErrorType::to_string() const
    {
        return "<error>";
    }

    bool ErrorType::structural_eq(const SemaType& other) const
    {
        return other.kind() == Kind::Error;
    }

    uint64_t ErrorType::structural_hash() const
    {
        return std::hash<uint8_t>{}(static_cast<uint8_t>(Kind::Error));
    }

    std::string VoidType::to_string() const
    {
        return "void";
    }

    bool VoidType::structural_eq(const SemaType& other) const
    {
        return other.kind() == Kind::Void;
    }

    uint64_t VoidType::structural_hash() const
    {
        return std::hash<uint8_t>{}(static_cast<uint8_t>(Kind::Void));
    }

    std::string BoolType::to_string() const
    {
        return "bool";
    }

    bool BoolType::structural_eq(const SemaType& other) const
    {
        return other.kind() == Kind::Bool;
    }

    uint64_t BoolType::structural_hash() const
    {
        return std::hash<uint8_t>{}(static_cast<uint8_t>(Kind::Bool));
    }

    std::string NullTType::to_string() const
    {
        return "null_t";
    }

    bool NullTType::structural_eq(const SemaType& other) const
    {
        return other.kind() == Kind::NullT;
    }

    uint64_t NullTType::structural_hash() const
    {
        return std::hash<uint8_t>{}(static_cast<uint8_t>(Kind::NullT));
    }

    std::string IntegerType::to_string() const
    {
        return std::format("{}{}", m_signed ? 'i' : 'u', m_width);
    }

    bool IntegerType::structural_eq(const SemaType& other) const
    {
        if (other.kind() != Kind::Integer)
            return false;

        auto& o = static_cast<const IntegerType&>(other);
        return m_width == o.m_width && m_signed == o.m_signed;
    }

    uint64_t IntegerType::structural_hash() const
    {
        uint64_t h = std::hash<uint8_t>{}(static_cast<uint8_t>(Kind::Integer));
        h ^= std::hash<uint8_t>{}(m_width) * 2654435761u;
        h ^= std::hash<bool>{}(m_signed) * 40503u;

        return h;
    }

    std::string FloatType::to_string() const
    {
        return std::format("f{}", m_width);
    }

    bool FloatType::structural_eq(const SemaType& other) const
    {
        if (other.kind() != Kind::Float)
            return false;

        return m_width == static_cast<const FloatType&>(other).m_width;
    }

    uint64_t FloatType::structural_hash() const
    {
        uint64_t h = std::hash<uint8_t>{}(static_cast<uint8_t>(Kind::Float));
        h ^= std::hash<uint8_t>{}(m_width) * 2654435761u;

        return h;
    }

    std::string PointerSemaType::to_string() const
    {
        std::string result = "*";
        if (ast::has_qualifier(m_quals, ast::Qualifier::Const))
            result += "const ";

        if (ast::has_qualifier(m_quals, ast::Qualifier::Volatile))
            result += "volatile ";

        if (ast::has_qualifier(m_quals, ast::Qualifier::Restrict))
            result += "restrict ";

        result += m_pointee->to_string();
        return result;
    }

    bool PointerSemaType::structural_eq(const SemaType& other) const
    {
        if (other.kind() != Kind::Pointer)
            return false;

        auto& o = static_cast<const PointerSemaType&>(other);
        return m_pointee == o.m_pointee && m_quals == o.m_quals;
    }

    uint64_t PointerSemaType::structural_hash() const
    {
        uint64_t h = std::hash<uint8_t>{}(static_cast<uint8_t>(Kind::Pointer));
        h ^= std::hash<const void*>{}(m_pointee) * 2654435761u;
        h ^= std::hash<uint8_t>{}(static_cast<uint8_t>(m_quals)) * 40503u;

        return h;
    }

    std::string SliceSemaType::to_string() const
    {
        return std::format("[]{}", m_element->to_string());
    }

    bool SliceSemaType::structural_eq(const SemaType& other) const
    {
        if (other.kind() != Kind::Slice)
            return false;

        return m_element == static_cast<const SliceSemaType&>(other).m_element;
    }

    uint64_t SliceSemaType::structural_hash() const
    {
        uint64_t h = std::hash<uint8_t>{}(static_cast<uint8_t>(Kind::Slice));
        h ^= std::hash<const void*>{}(m_element) * 2654435761u;

        return h;
    }

    std::string ArraySemaType::to_string() const
    {
        return std::format("[{}]{}", m_length, m_element->to_string());
    }

    bool ArraySemaType::structural_eq(const SemaType& other) const
    {
        if (other.kind() != Kind::Array)
            return false;

        auto& o = static_cast<const ArraySemaType&>(other);
        return m_element == o.m_element && m_length == o.m_length;
    }

    uint64_t ArraySemaType::structural_hash() const
    {
        uint64_t h = std::hash<uint8_t>{}(static_cast<uint8_t>(Kind::Array));
        h ^= std::hash<const void*>{}(m_element) * 2654435761u;
        h ^= std::hash<uint64_t>{}(m_length) * 40503u;

        return h;
    }

    std::string FlexibleArraySemaType::to_string() const
    {
        return std::format("[]{}", m_element->to_string());
    }

    bool FlexibleArraySemaType::structural_eq(const SemaType& other) const
    {
        if (other.kind() != Kind::FlexibleArray)
            return false;

        return m_element == static_cast<const FlexibleArraySemaType&>(other).m_element;
    }

    uint64_t FlexibleArraySemaType::structural_hash() const
    {
        uint64_t h = std::hash<uint8_t>{}(static_cast<uint8_t>(Kind::FlexibleArray));
        h ^= std::hash<const void*>{}(m_element) * 2654435761u;

        return h;
    }

    std::string FunctionSemaType::to_string() const
    {
        std::string result = "fn(";
        for (std::size_t i = 0; i < m_params.size(); ++i)
        {
            if (i > 0)
                result += ", ";
            result += m_params[i]->to_string();
        }

        if (m_variadic)
        {
            if (!m_params.empty())
                result += ", ";
            result += "...";
        }

        result += ") -> ";
        result += m_return->to_string();
        return result;
    }

    bool FunctionSemaType::structural_eq(const SemaType& other) const
    {
        if (other.kind() != Kind::Function)
            return false;

        auto& o = static_cast<const FunctionSemaType&>(other);
        if (m_return != o.m_return)
            return false;

        if (m_variadic != o.m_variadic)
            return false;

        if (m_params.size() != o.m_params.size())
            return false;

        for (std::size_t i = 0; i < m_params.size(); ++i)
            if (m_params[i] != o.m_params[i])
                return false;

        return true;
    }

    uint64_t FunctionSemaType::structural_hash() const
    {
        uint64_t h = std::hash<uint8_t>{}(static_cast<uint8_t>(Kind::Function));
        h ^= std::hash<const void*>{}(m_return) * 2654435761u;
        h ^= std::hash<bool>{}(m_variadic) * 40503u;

        for (auto* p : m_params)
        {
            h ^= std::hash<const void*>{}(p) * 2654435761u;
            h = (h << 7) | (h >> 57);
        }

        return h;
    }

    void StructSemaType::set_fields(std::vector<FieldInfo> fields) noexcept
    {
        m_fields = std::move(fields);
    }

    void StructSemaType::set_methods(std::vector<MethodInfo> methods) noexcept
    {
        m_methods = std::move(methods);
    }

    const FieldInfo* StructSemaType::find_field(si::InternedString name) const noexcept
    {
        for (auto& f : m_fields)
            if (f.name == name)
                return &f;

        return nullptr;
    }

    const MethodInfo* StructSemaType::find_method(si::InternedString name) const noexcept
    {
        for (auto& m : m_methods)
            if (m.name == name)
                return &m;

        return nullptr;
    }

    std::string StructSemaType::to_string() const
    {
        return std::string{m_name.view()};
    }

    bool StructSemaType::structural_eq(const SemaType& other) const
    {
        return this == &other;
    }

    uint64_t StructSemaType::structural_hash() const
    {
        return std::hash<const void*>{}(this);
    }

    void UnionSemaType::set_fields(std::vector<FieldInfo> fields) noexcept
    {
        m_fields = std::move(fields);
    }

    void UnionSemaType::set_methods(std::vector<MethodInfo> methods) noexcept
    {
        m_methods = std::move(methods);
    }

    const FieldInfo* UnionSemaType::find_field(si::InternedString name) const noexcept
    {
        for (auto& f : m_fields)
            if (f.name == name)
                return &f;

        return nullptr;
    }

    const MethodInfo* UnionSemaType::find_method(si::InternedString name) const noexcept
    {
        for (auto& m : m_methods)
            if (m.name == name)
                return &m;

        return nullptr;
    }

    std::string UnionSemaType::to_string() const
    {
        return std::string{m_name.view()};
    }

    bool UnionSemaType::structural_eq(const SemaType& other) const
    {
        return this == &other;
    }

    uint64_t UnionSemaType::structural_hash() const
    {
        return std::hash<const void*>{}(this);
    }

    void EnumSemaType::set_variants(std::vector<VariantInfo> variants) noexcept
    {
        m_variants = std::move(variants);
    }

    void EnumSemaType::set_methods(std::vector<MethodInfo> methods) noexcept
    {
        m_methods = std::move(methods);
    }

    const VariantInfo* EnumSemaType::find_variant(si::InternedString name) const noexcept
    {
        for (auto& v : m_variants)
            if (v.name == name)
                return &v;

        return nullptr;
    }

    const MethodInfo* EnumSemaType::find_method(si::InternedString name) const noexcept
    {
        for (auto& m : m_methods)
            if (m.name == name)
                return &m;

        return nullptr;
    }

    bool EnumSemaType::has_payloads() const noexcept
    {
        return std::ranges::any_of(m_variants, [](const VariantInfo& v) { return !v.payload_types.empty(); });
    }

    std::string EnumSemaType::to_string() const
    {
        return std::string{m_name.view()};
    }

    bool EnumSemaType::structural_eq(const SemaType& other) const
    {
        return this == &other;
    }

    uint64_t EnumSemaType::structural_hash() const
    {
        return std::hash<const void*>{}(this);
    }

    SemaType* AliasType::canonical() const noexcept
    {
        SemaType* ty = m_underlying;
        while (ty && ty->is_alias())
            ty = static_cast<AliasType*>(ty)->underlying();

        return ty;
    }

    std::string AliasType::to_string() const
    {
        return std::string{m_name.view()};
    }

    bool AliasType::structural_eq(const SemaType& other) const
    {
        return this == &other;
    }

    uint64_t AliasType::structural_hash() const
    {
        return std::hash<const void*>{}(this);
    }

    std::string TypeVar::to_string() const
    {
        if (m_parent)
            return m_parent->to_string();

        return std::format("?T{}", m_id);
    }

    bool TypeVar::structural_eq(const SemaType& other) const
    {
        if (other.kind() != Kind::TypeVar)
            return false;

        return m_id == static_cast<const TypeVar&>(other).m_id;
    }

    uint64_t TypeVar::structural_hash() const
    {
        uint64_t h = std::hash<uint8_t>{}(static_cast<uint8_t>(Kind::TypeVar));
        h ^= std::hash<uint32_t>{}(m_id) * 2654435761u;

        return h;
    }

    SemaType* strip_alias(SemaType* ty) noexcept
    {
        while (ty && ty->is_alias())
            ty = static_cast<AliasType*>(ty)->underlying();

        return ty;
    }

} // namespace dcc::sema
