#include <cassert>
#include <ir/mangle.hh>
#include <sema/types.hh>

namespace dcc::ir
{
    void Mangler::encode_identifier(std::string& out, std::string_view name)
    {
        out += std::to_string(name.size());
        out += name;
    }

    void Mangler::encode_module_path(std::string& out, std::span<const si::InternedString> path)
    {
        for (auto& seg : path)
            encode_identifier(out, seg.view());
    }

    void Mangler::encode_type(std::string& out, const sema::SemaType* ty)
    {
        while (ty->is_alias())
            ty = static_cast<const sema::AliasType*>(ty)->canonical();

        switch (ty->kind())
        {
            case sema::SemaType::Kind::Void:
                out += 'v';
                return;
            case sema::SemaType::Kind::Bool:
                out += 'b';
                return;
            case sema::SemaType::Kind::NullT:
                out += 'n';
                return;

            case sema::SemaType::Kind::Integer: {
                auto* it = static_cast<const sema::IntegerType*>(ty);
                switch (it->width())
                {
                    case 8:
                        out += it->is_signed() ? 'a' : 'h';
                        return;
                    case 16:
                        out += it->is_signed() ? 's' : 't';
                        return;
                    case 32:
                        out += it->is_signed() ? 'i' : 'j';
                        return;
                    case 64:
                        out += it->is_signed() ? 'l' : 'm';
                        return;
                    default:
                        assert(false && "unsupported integer width");
                        return;
                }
            }

            case sema::SemaType::Kind::Float: {
                auto* ft = static_cast<const sema::FloatType*>(ty);
                switch (ft->width())
                {
                    case 32:
                        out += 'f';
                        return;
                    case 64:
                        out += 'd';
                        return;
                    default:
                        assert(false && "unsupported float width");
                        return;
                }
            }

            case sema::SemaType::Kind::Pointer: {
                auto* pt = static_cast<const sema::PointerSemaType*>(ty);
                out += 'P';
                encode_type(out, pt->pointee());
                return;
            }

            case sema::SemaType::Kind::Slice: {
                auto* st = static_cast<const sema::SliceSemaType*>(ty);
                out += 'S';
                encode_type(out, st->element());
                return;
            }

            case sema::SemaType::Kind::Array: {
                auto* at = static_cast<const sema::ArraySemaType*>(ty);
                out += 'A';
                out += std::to_string(at->length());
                out += '_';
                encode_type(out, at->element());
                return;
            }

            case sema::SemaType::Kind::FlexibleArray: {
                auto* fa = static_cast<const sema::FlexibleArraySemaType*>(ty);
                out += "A0_";
                encode_type(out, fa->element());
                return;
            }

            case sema::SemaType::Kind::Struct: {
                auto* st = static_cast<const sema::StructSemaType*>(ty);
                encode_identifier(out, st->name().view());
                return;
            }

            case sema::SemaType::Kind::Union: {
                auto* ut = static_cast<const sema::UnionSemaType*>(ty);
                out += 'U';
                encode_identifier(out, ut->name().view());
                return;
            }

            case sema::SemaType::Kind::Enum: {
                auto* et = static_cast<const sema::EnumSemaType*>(ty);
                out += 'E';
                encode_identifier(out, et->name().view());
                return;
            }

            case sema::SemaType::Kind::Function: {
                auto* ft = static_cast<const sema::FunctionSemaType*>(ty);
                out += 'F';
                for (auto* p : ft->param_types())
                    encode_type(out, p);
                out += 'Z';
                encode_type(out, ft->return_type());
                return;
            }

            case sema::SemaType::Kind::Error:
            case sema::SemaType::Kind::TypeVar:
            case sema::SemaType::Kind::Alias:
                assert(false && "un-mangleable type");
                out += '?';
                return;
        }
    }

    void Mangler::encode_template_args(std::string& out, std::span<sema::SemaType* const> args)
    {
        if (args.empty())
            return;

        out += 'T';
        for (auto* arg : args)
            encode_type(out, arg);

        out += 'Z';
    }

    std::string Mangler::mangle_function(std::span<const si::InternedString> module_path, std::string_view func_name,
                                         std::span<sema::SemaType* const> param_types, std::span<sema::SemaType* const> template_args)
    {
        if (func_name == "main" && template_args.empty())
            return "main";

        std::string out = "_D";

        encode_module_path(out, module_path);
        out += '_';
        encode_identifier(out, func_name);
        encode_template_args(out, template_args);

        if (param_types.empty())
            out += 'v';
        else
            for (auto* pt : param_types)
                encode_type(out, pt);

        return out;
    }

    namespace
    {
        struct Demangler
        {
            std::string_view src;
            std::size_t pos{0};

            bool done() const { return pos >= src.size(); }
            char peek() const { return src[pos]; }
            char next() { return src[pos++]; }

            bool eat(char c)
            {
                if (!done() && peek() == c)
                {
                    ++pos;
                    return true;
                }
                return false;
            }

            bool is_digit() const { return !done() && peek() >= '0' && peek() <= '9'; }

            bool read_name(std::string& out)
            {
                if (!is_digit())
                    return false;

                std::size_t len = 0;
                while (is_digit())
                    len = len * 10 + (next() - '0');

                if (pos + len > src.size())
                    return false;

                out.append(src.data() + pos, len);
                pos += len;
                return true;
            }

            bool read_type(std::string& out)
            {
                if (done())
                    return false;

                char c = peek();

                switch (c)
                {
                    case 'v':
                        next();
                        out += "void";
                        return true;
                    case 'b':
                        next();
                        out += "bool";
                        return true;
                    case 'n':
                        next();
                        out += "null_t";
                        return true;
                    case 'a':
                        next();
                        out += "i8";
                        return true;
                    case 'h':
                        next();
                        out += "u8";
                        return true;
                    case 's':
                        next();
                        out += "i16";
                        return true;
                    case 't':
                        next();
                        out += "u16";
                        return true;
                    case 'i':
                        next();
                        out += "i32";
                        return true;
                    case 'j':
                        next();
                        out += "u32";
                        return true;
                    case 'l':
                        next();
                        out += "i64";
                        return true;
                    case 'm':
                        next();
                        out += "u64";
                        return true;
                    case 'f':
                        next();
                        out += "f32";
                        return true;
                    case 'd':
                        next();
                        out += "f64";
                        return true;
                    default:
                        break;
                }

                if (c == 'P')
                {
                    next();
                    out += '*';
                    return read_type(out);
                }

                if (c == 'S')
                {
                    next();
                    out += "[]";
                    return read_type(out);
                }

                if (c == 'A')
                {
                    next();
                    std::size_t len = 0;
                    while (is_digit())
                        len = len * 10 + (next() - '0');

                    if (!eat('_'))
                        return false;

                    std::string elem;
                    if (!read_type(elem))
                        return false;

                    if (len == 0)
                        out += elem + "[]";
                    else
                        out += elem + "[" + std::to_string(len) + "]";

                    return true;
                }

                if (c == 'U')
                {
                    next();
                    std::string name;
                    if (!read_name(name))
                        return false;

                    out += name;
                    return true;
                }

                if (c == 'E')
                {
                    next();
                    std::string name;
                    if (!read_name(name))
                        return false;

                    out += name;
                    return true;
                }

                if (c == 'F')
                {
                    next();
                    out += "fn(";
                    bool first = true;
                    while (!done() && peek() != 'Z')
                    {
                        if (!first)
                            out += ", ";

                        first = false;
                        if (!read_type(out))
                            return false;
                    }
                    if (!eat('Z'))
                        return false;

                    out += ") -> ";
                    return read_type(out);
                }

                if (is_digit())
                    return read_name(out);

                return false;
            }
        };

    } // anonymous namespace

    std::string Mangler::demangle(std::string_view mangled)
    {
        if (mangled.size() < 3 || mangled.substr(0, 2) != "_D")
            return std::string(mangled);

        Demangler d{mangled, 2};
        std::string result;

        bool first_seg = true;
        while (d.is_digit())
        {
            std::string seg;
            if (!d.read_name(seg))
                return std::string(mangled);

            if (d.eat('_'))
            {
                if (!first_seg)
                    result += "::";

                result += seg;
                break;
            }

            if (!first_seg)
                result += "::";

            result += seg;
            first_seg = false;
        }

        std::string func_name;
        if (!d.read_name(func_name))
            return std::string(mangled);

        if (!result.empty())
            result += "::";

        result += func_name;

        if (!d.done() && d.peek() == 'T')
        {
            d.next();
            result += "!(";
            bool first = true;

            while (!d.done() && d.peek() != 'Z')
            {
                if (!first)
                    result += ", ";

                first = false;
                if (!d.read_type(result))
                    return std::string(mangled);
            }
            if (!d.eat('Z'))
                return std::string(mangled);

            result += ")";
        }

        result += '(';

        if (!d.done() && d.peek() == 'v' && d.pos + 1 == d.src.size())
        {
            result += ')';
            return result;
        }

        bool first_param = true;
        while (!d.done())
        {
            if (!first_param)
                result += ", ";

            first_param = false;
            if (!d.read_type(result))
                return std::string(mangled);
        }

        result += ')';
        return result;
    }

} // namespace dcc::ir
