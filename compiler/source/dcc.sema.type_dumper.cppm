export module dcc.sema.type_dumper;

import std;
import dcc.ast;
import dcc.types;
import dcc.sema.scope;

export namespace dcc::sema
{
    class TypeDumper
    {
    public:
        [[nodiscard]] static std::string dump(ModuleInfo const& mod)
        {
            TypeDumper d;
            d.line_fmt("Module {}", mod.canonical_path.str());
            d.m_indent++;
            if (mod.tu)
                d.print_tu(*mod.tu);

            return d.m_out;
        }

    private:
        std::string m_out;
        std::size_t m_indent{};

        void pad() { m_out.append(m_indent * 2, ' '); }

        void line(std::string_view s)
        {
            pad();
            m_out += s;
            m_out += '\n';
        }

        template <typename... A> void line_fmt(std::format_string<A...> fmt, A&&... args)
        {
            pad();
            std::format_to(std::back_inserter(m_out), fmt, std::forward<A>(args)...);
            m_out += '\n';
        }

        void print_tu(ast::TranslationUnit const& tu)
        {
            for (auto* d : tu.decls)
                print_decl(*d);
        }

        void print_decl(ast::Decl const& d)
        {
            switch (d.kind)
            {
                case ast::DeclKind::Struct:
                    print_struct(static_cast<ast::StructDecl const&>(d));
                    break;
                case ast::DeclKind::Union:
                    print_union(static_cast<ast::UnionDecl const&>(d));
                    break;
                case ast::DeclKind::Enum:
                    print_enum(static_cast<ast::EnumDecl const&>(d));
                    break;
                case ast::DeclKind::Func:
                    print_func(static_cast<ast::FuncDecl const&>(d));
                    break;
                case ast::DeclKind::Var:
                    print_var(static_cast<ast::VarDecl const&>(d));
                    break;
                case ast::DeclKind::Using:
                    print_using(static_cast<ast::UsingDecl const&>(d));
                    break;
                case ast::DeclKind::Module:
                case ast::DeclKind::Import:
                    break;
            }
        }

        void print_template_params(std::pmr::vector<ast::TemplateParam> const& params)
        {
            if (params.empty())
                return;

            std::string joined;
            for (std::size_t i = 0; i < params.size(); ++i)
            {
                if (i > 0)
                    joined += ", ";

                joined += params[i].name;
                if (params[i].value_type)
                {
                    joined += ": ";
                    joined += type_str(params[i].value_type->sema.canonical);
                }
            }
            line_fmt("TemplateParams {}", joined);
        }

        void print_struct(ast::StructDecl const& d)
        {
            line_fmt("Struct {}", d.name);
            m_indent++;
            print_template_params(d.template_params);
            for (auto const& f : d.fields)
                line_fmt("Field {}: {}", f.name, type_str(f.type ? f.type->sema.canonical : nullptr));

            m_indent--;
        }

        void print_union(ast::UnionDecl const& d)
        {
            line_fmt("Union {}", d.name);
            m_indent++;
            for (auto const& f : d.fields)
                line_fmt("Field {}: {}", f.name, type_str(f.type ? f.type->sema.canonical : nullptr));

            m_indent--;
        }

        void print_enum(ast::EnumDecl const& d)
        {
            line_fmt("Enum {}", d.name);
            m_indent++;
            print_template_params(d.template_params);
            if (d.backing_type)
                line_fmt("Backing: {}", type_str(d.backing_type->sema.canonical));

            for (auto const& v : d.variants)
                if (!v.payload.empty())
                {
                    std::string joined;
                    for (std::size_t i = 0; i < v.payload.size(); ++i)
                    {
                        if (i > 0)
                            joined += ", ";

                        joined += type_str(v.payload[i] ? v.payload[i]->sema.canonical : nullptr);
                    }
                    line_fmt("Variant {}({})", v.name, joined);
                }
                else
                    line_fmt("Variant {}", v.name);
            m_indent--;
        }

        void print_func(ast::FuncDecl const& d)
        {
            line_fmt("Func {}", d.name);
            m_indent++;
            print_template_params(d.template_params);
            line_fmt("Return: {}", type_str(d.return_type ? d.return_type->sema.canonical : nullptr));
            for (auto const& p : d.params)
                line_fmt("Param {}: {}", p.name, type_str(p.type ? p.type->sema.canonical : nullptr));

            m_indent--;
        }

        void print_var(ast::VarDecl const& d) { line_fmt("Var {}: {}", d.name, type_str(d.type ? d.type->sema.canonical : nullptr)); }

        void print_using(ast::UsingDecl const& d)
        {
            line_fmt("Using {}", d.alias_path.is_empty() ? std::string_view{"<anon>"} : d.alias_path.segments.back().name);
            m_indent++;
            print_template_params(d.template_params);
            switch (d.using_kind)
            {
                case ast::UsingKind::Alias:
                    line_fmt("Alias: {}", type_str(d.target_type ? d.target_type->sema.canonical : nullptr));
                    break;
                case ast::UsingKind::Concept:
                    if (auto* c = ast::node_cast<ast::CompilesExpr>(d.target_expr))
                        for (auto const& p : c->params)
                            line_fmt("CompilesParam {}: {}", p.name, type_str(p.type ? p.type->sema.canonical : nullptr));
                    break;
                case ast::UsingKind::BareImport:
                case ast::UsingKind::Wildcard:
                case ast::UsingKind::List:
                    break;
            }
            m_indent--;
        }

        [[nodiscard]] std::string type_str(void const* t) const
        {
            if (!t)
                return "<unresolved>";

            auto const* ty = reinterpret_cast<types::Type const*>(t);

            switch (ty->kind)
            {
                case types::TypeKind::Void:
                    return "void";
                case types::TypeKind::Bool:
                    return "bool";
                case types::TypeKind::Int:
                    return int_str(*static_cast<types::IntType const*>(ty));
                case types::TypeKind::Float:
                    return float_str(*static_cast<types::FloatType const*>(ty));
                case types::TypeKind::Char:
                    return "char";
                case types::TypeKind::NullT:
                    return "null_t";
                case types::TypeKind::Pointer:
                    return pointer_str(*static_cast<types::PointerType const*>(ty));
                case types::TypeKind::Array:
                    return array_str(*static_cast<types::ArrayType const*>(ty));
                case types::TypeKind::Slice:
                    return slice_str(*static_cast<types::SliceType const*>(ty));
                case types::TypeKind::Range:
                    return std::format("range({})", type_str(static_cast<types::RangeType const*>(ty)->element));
                case types::TypeKind::RangeInclusive:
                    return std::format("range_inclusive({})", type_str(static_cast<types::RangeInclusiveType const*>(ty)->element));
                case types::TypeKind::Fam:
                    return fam_str(*static_cast<types::FamType const*>(ty));
                case types::TypeKind::FuncPtr:
                    return funcptr_str(*static_cast<types::FuncPtrType const*>(ty));
                case types::TypeKind::Struct:
                    return nominal_str("struct", *static_cast<types::StructType const*>(ty));
                case types::TypeKind::Union:
                    return nominal_str("union", *static_cast<types::UnionType const*>(ty));
                case types::TypeKind::Enum:
                    return nominal_str("enum", *static_cast<types::EnumType const*>(ty));
                case types::TypeKind::TemplateParam:
                    return template_param_str(*static_cast<types::TemplateParamType const*>(ty));
                case types::TypeKind::Error:
                    return "<error>";
            }
            return "<type>";
        }

        [[nodiscard]] static std::string qual_str(types::Qual q)
        {
            std::string s;
            if (types::has_qual(q, types::Qual::Const))
                s += "const ";
            if (types::has_qual(q, types::Qual::Volatile))
                s += "volatile ";
            if (types::has_qual(q, types::Qual::Restrict))
                s += "restrict ";
            return s;
        }

        [[nodiscard]] static std::string int_str(types::IntType const& t) { return std::format("{}{}", t.is_signed ? 'i' : 'u', unsigned(t.bits)); }

        [[nodiscard]] static std::string float_str(types::FloatType const& t) { return std::format("f{}", unsigned(t.bits)); }

        [[nodiscard]] std::string pointer_str(types::PointerType const& t) const
        {
            return std::format("ptr({}{})", qual_str(t.pointee_quals), type_str(t.pointee));
        }

        [[nodiscard]] std::string array_str(types::ArrayType const& t) const { return std::format("[{}; {}]", type_str(t.element), t.count); }

        [[nodiscard]] std::string slice_str(types::SliceType const& t) const
        {
            return std::format("slice({}{})", qual_str(t.element_quals), type_str(t.element));
        }

        [[nodiscard]] std::string fam_str(types::FamType const& t) const { return std::format("fam({})", type_str(t.element)); }

        [[nodiscard]] std::string funcptr_str(types::FuncPtrType const& t) const
        {
            std::string params;
            for (std::size_t i = 0; i < t.params.size(); ++i)
            {
                if (i > 0)
                    params += ", ";

                params += type_str(t.params[i]);
            }

            return std::format("fn({}) -> {}", params, type_str(t.return_type));
        }

        template <typename T> [[nodiscard]] std::string nominal_str(std::string_view kind, T const& t) const
        {
            std::string s = std::format("{} {}", kind, decl_name(t.decl));
            if (!t.template_args.empty())
            {
                s += "<";
                for (std::size_t i = 0; i < t.template_args.size(); ++i)
                {
                    if (i > 0)
                        s += ", ";

                    s += type_str(t.template_args[i]);
                }

                s += ">";
            }
            return s;
        }

        [[nodiscard]] static std::string template_param_str(types::TemplateParamType const& t) { return std::format("{}#{}", t.name, t.index); }

        [[nodiscard]] static std::string decl_name(void const* d)
        {
            if (!d)
                return "<null>";

            auto const* dd = reinterpret_cast<ast::Decl const*>(d);

            switch (dd->kind)
            {
                case ast::DeclKind::Struct:
                    return std::string{static_cast<ast::StructDecl const*>(dd)->name};
                case ast::DeclKind::Union:
                    return std::string{static_cast<ast::UnionDecl const*>(dd)->name};
                case ast::DeclKind::Enum:
                    return std::string{static_cast<ast::EnumDecl const*>(dd)->name};
                case ast::DeclKind::Func:
                    return std::string{static_cast<ast::FuncDecl const*>(dd)->name};
                case ast::DeclKind::Var:
                    return std::string{static_cast<ast::VarDecl const*>(dd)->name};
                case ast::DeclKind::Using:
                    return static_cast<ast::UsingDecl const*>(dd)->alias_path.is_empty()
                               ? std::string{"<using>"}
                               : std::string{static_cast<ast::UsingDecl const*>(dd)->alias_path.segments.back().name};
                case ast::DeclKind::Module:
                    return "module";
                case ast::DeclKind::Import:
                    return "import";
            }
            return "<decl>";
        }
    };

} // namespace dcc::sema
