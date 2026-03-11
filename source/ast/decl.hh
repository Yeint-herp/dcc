#ifndef DCC_AST_DECL_HH
#define DCC_AST_DECL_HH

#include <ast/common.hh>
#include <ast/node.hh>
#include <span>

namespace dcc::ast
{
    class TypeExpr;
    class Expr;
    class Stmt;
    class BlockStmt;

    enum class StorageClass : std::uint8_t
    {
        None,
        Static,
        Extern,
    };

    class TemplateTypeParamDecl final : public Decl
    {
    public:
        explicit constexpr TemplateTypeParamDecl(sm::SourceRange range, si::InternedString name, TypeExpr* default_type = nullptr) noexcept
            : Decl{range}, m_name{name}, m_default{default_type}
        {
        }

        [[nodiscard]] constexpr si::InternedString name() const noexcept { return m_name; }
        [[nodiscard]] constexpr TypeExpr* default_type() const noexcept { return m_default; }

        void accept(Visitor& v) const override;

    private:
        si::InternedString m_name;
        TypeExpr* m_default;
    };

    class TemplateValueParamDecl final : public Decl
    {
    public:
        explicit constexpr TemplateValueParamDecl(sm::SourceRange range, si::InternedString name, TypeExpr* type, Expr* default_value = nullptr) noexcept
            : Decl{range}, m_name{name}, m_type{type}, m_default{default_value}
        {
        }

        [[nodiscard]] constexpr si::InternedString name() const noexcept { return m_name; }
        [[nodiscard]] constexpr TypeExpr* type() const noexcept { return m_type; }
        [[nodiscard]] constexpr Expr* default_value() const noexcept { return m_default; }

        void accept(Visitor& v) const override;

    private:
        si::InternedString m_name;
        TypeExpr* m_type;
        Expr* m_default;
    };

    class VarDecl final : public Decl
    {
    public:
        explicit constexpr VarDecl(sm::SourceRange range, si::InternedString name, TypeExpr* type, Expr* init, Qualifier quals = Qualifier::None,
                                   StorageClass sc = StorageClass::None) noexcept
            : Decl{range}, m_name{name}, m_type{type}, m_init{init}, m_quals{quals}, m_sc{sc}
        {
        }

        [[nodiscard]] constexpr si::InternedString name() const noexcept { return m_name; }
        [[nodiscard]] constexpr TypeExpr* type() const noexcept { return m_type; }
        [[nodiscard]] constexpr Expr* init() const noexcept { return m_init; }
        [[nodiscard]] constexpr Qualifier quals() const noexcept { return m_quals; }
        [[nodiscard]] constexpr StorageClass storage_class() const noexcept { return m_sc; }

        void accept(Visitor& v) const override;

    private:
        si::InternedString m_name;
        TypeExpr* m_type;
        Expr* m_init;
        Qualifier m_quals;
        StorageClass m_sc;
    };

    class ParamDecl final : public Decl
    {
    public:
        explicit constexpr ParamDecl(sm::SourceRange range, si::InternedString name, TypeExpr* type, Expr* default_value = nullptr) noexcept
            : Decl{range}, m_name{name}, m_type{type}, m_default{default_value}
        {
        }

        [[nodiscard]] constexpr si::InternedString name() const noexcept { return m_name; }
        [[nodiscard]] constexpr TypeExpr* type() const noexcept { return m_type; }
        [[nodiscard]] constexpr Expr* default_value() const noexcept { return m_default; }

        void accept(Visitor& v) const override;

    private:
        si::InternedString m_name;
        TypeExpr* m_type;
        Expr* m_default;
    };

    class FieldDecl final : public Decl
    {
    public:
        explicit constexpr FieldDecl(sm::SourceRange range, si::InternedString name, TypeExpr* type, Expr* default_value = nullptr,
                                     Visibility vis = Visibility::Private) noexcept
            : Decl{range}, m_name{name}, m_type{type}, m_default{default_value}, m_vis{vis}
        {
        }

        [[nodiscard]] constexpr si::InternedString name() const noexcept { return m_name; }
        [[nodiscard]] constexpr TypeExpr* type() const noexcept { return m_type; }
        [[nodiscard]] constexpr Expr* default_value() const noexcept { return m_default; }
        [[nodiscard]] constexpr Visibility visibility() const noexcept { return m_vis; }

        void accept(Visitor& v) const override;

    private:
        si::InternedString m_name;
        TypeExpr* m_type;
        Expr* m_default;
        Visibility m_vis;
    };

    class FunctionDecl final : public Decl
    {
    public:
        explicit constexpr FunctionDecl(sm::SourceRange range, si::InternedString name, TypeExpr* return_type, std::span<ParamDecl* const> params,
                                        std::span<Decl* const> template_params, BlockStmt* body, Visibility vis = Visibility::Private,
                                        StorageClass sc = StorageClass::None, std::span<const Attribute> attributes = {}) noexcept
            : Decl{range}, m_name{name}, m_return_type{return_type}, m_params{params}, m_template_params{template_params}, m_body{body}, m_vis{vis}, m_sc{sc},
              m_attributes{attributes}, no_mangle(false)
        {
        }

        [[nodiscard]] constexpr si::InternedString name() const noexcept { return m_name; }
        [[nodiscard]] constexpr TypeExpr* return_type() const noexcept { return m_return_type; }
        [[nodiscard]] constexpr std::span<ParamDecl* const> params() const noexcept { return m_params; }
        [[nodiscard]] constexpr std::span<Decl* const> template_params() const noexcept { return m_template_params; }
        [[nodiscard]] constexpr BlockStmt* body() const noexcept { return m_body; }
        [[nodiscard]] constexpr Visibility visibility() const noexcept { return m_vis; }
        [[nodiscard]] constexpr StorageClass storage_class() const noexcept { return m_sc; }
        [[nodiscard]] constexpr bool is_static() const noexcept { return m_sc == StorageClass::Static; }
        [[nodiscard]] constexpr bool is_extern() const noexcept { return m_sc == StorageClass::Extern; }
        [[nodiscard]] constexpr bool should_mangle() const noexcept { return !no_mangle; }
        [[nodiscard]] constexpr std::span<const Attribute> attributes() const noexcept { return m_attributes; }

        void accept(Visitor& v) const override;

    private:
        si::InternedString m_name;
        TypeExpr* m_return_type;
        std::span<ParamDecl* const> m_params;
        std::span<Decl* const> m_template_params;
        BlockStmt* m_body;
        Visibility m_vis;
        StorageClass m_sc;
        std::span<const Attribute> m_attributes;
        bool no_mangle;
    };

    class StructDecl final : public Decl
    {
    public:
        explicit constexpr StructDecl(sm::SourceRange range, si::InternedString name, std::span<FieldDecl* const> fields,
                                      std::span<FunctionDecl* const> methods, std::span<Decl* const> template_params, std::span<const Attribute> attributes,
                                      Visibility vis = Visibility::Private) noexcept
            : Decl{range}, m_name{name}, m_fields{fields}, m_methods{methods}, m_template_params{template_params}, m_attributes{attributes}, m_vis{vis}
        {
        }

        [[nodiscard]] constexpr si::InternedString name() const noexcept { return m_name; }
        [[nodiscard]] constexpr std::span<FieldDecl* const> fields() const noexcept { return m_fields; }
        [[nodiscard]] constexpr std::span<FunctionDecl* const> methods() const noexcept { return m_methods; }
        [[nodiscard]] constexpr std::span<Decl* const> template_params() const noexcept { return m_template_params; }
        [[nodiscard]] constexpr std::span<const Attribute> attributes() const noexcept { return m_attributes; }
        [[nodiscard]] constexpr Visibility visibility() const noexcept { return m_vis; }

        void accept(Visitor& v) const override;

    private:
        si::InternedString m_name;
        std::span<FieldDecl* const> m_fields;
        std::span<FunctionDecl* const> m_methods;
        std::span<Decl* const> m_template_params;
        std::span<const Attribute> m_attributes;
        Visibility m_vis;
    };

    class UnionDecl final : public Decl
    {
    public:
        explicit constexpr UnionDecl(sm::SourceRange range, si::InternedString name, std::span<FieldDecl* const> fields, std::span<FunctionDecl* const> methods,
                                     std::span<const Attribute> attributes, Visibility vis = Visibility::Private) noexcept
            : Decl{range}, m_name{name}, m_fields{fields}, m_methods{methods}, m_attributes{attributes}, m_vis{vis}
        {
        }

        [[nodiscard]] constexpr si::InternedString name() const noexcept { return m_name; }
        [[nodiscard]] constexpr std::span<FieldDecl* const> fields() const noexcept { return m_fields; }
        [[nodiscard]] constexpr std::span<FunctionDecl* const> methods() const noexcept { return m_methods; }
        [[nodiscard]] constexpr std::span<const Attribute> attributes() const noexcept { return m_attributes; }
        [[nodiscard]] constexpr Visibility visibility() const noexcept { return m_vis; }

        void accept(Visitor& v) const override;

    private:
        si::InternedString m_name;
        std::span<FieldDecl* const> m_fields;
        std::span<FunctionDecl* const> m_methods;
        std::span<const Attribute> m_attributes;
        Visibility m_vis;
    };

    class EnumVariantDecl final : public Decl
    {
    public:
        explicit constexpr EnumVariantDecl(sm::SourceRange range, si::InternedString name, Expr* discriminant,
                                           std::span<TypeExpr* const> payload_types) noexcept
            : Decl{range}, m_name{name}, m_discriminant{discriminant}, m_payload_types{payload_types}
        {
        }

        [[nodiscard]] constexpr si::InternedString name() const noexcept { return m_name; }
        [[nodiscard]] constexpr Expr* discriminant() const noexcept { return m_discriminant; }
        [[nodiscard]] constexpr std::span<TypeExpr* const> payload_types() const noexcept { return m_payload_types; }

        void accept(Visitor& v) const override;

    private:
        si::InternedString m_name;
        Expr* m_discriminant;
        std::span<TypeExpr* const> m_payload_types;
    };

    class EnumDecl final : public Decl
    {
    public:
        explicit constexpr EnumDecl(sm::SourceRange range, si::InternedString name, TypeExpr* underlying_type, std::span<EnumVariantDecl* const> variants,
                                    std::span<FunctionDecl* const> methods, std::span<const Attribute> attributes,
                                    Visibility vis = Visibility::Private) noexcept
            : Decl{range}, m_name{name}, m_underlying{underlying_type}, m_variants{variants}, m_methods{methods}, m_attributes{attributes}, m_vis{vis}
        {
        }

        [[nodiscard]] constexpr si::InternedString name() const noexcept { return m_name; }
        [[nodiscard]] constexpr TypeExpr* underlying_type() const noexcept { return m_underlying; }
        [[nodiscard]] constexpr std::span<EnumVariantDecl* const> variants() const noexcept { return m_variants; }
        [[nodiscard]] constexpr std::span<FunctionDecl* const> methods() const noexcept { return m_methods; }
        [[nodiscard]] constexpr std::span<const Attribute> attributes() const noexcept { return m_attributes; }
        [[nodiscard]] constexpr Visibility visibility() const noexcept { return m_vis; }

        void accept(Visitor& v) const override;

    private:
        si::InternedString m_name;
        TypeExpr* m_underlying;
        std::span<EnumVariantDecl* const> m_variants;
        std::span<FunctionDecl* const> m_methods;
        std::span<const Attribute> m_attributes;
        Visibility m_vis;
    };

    class ModuleDecl final : public Decl
    {
    public:
        explicit constexpr ModuleDecl(sm::SourceRange range, std::span<const si::InternedString> path) noexcept : Decl{range}, m_path{path} {}

        [[nodiscard]] constexpr std::span<const si::InternedString> path() const noexcept { return m_path; }

        void accept(Visitor& v) const override;

    private:
        std::span<const si::InternedString> m_path;
    };

    class ImportDecl final : public Decl
    {
    public:
        explicit constexpr ImportDecl(sm::SourceRange range, std::span<const si::InternedString> path, Visibility vis = Visibility::Private) noexcept
            : Decl{range}, m_path{path}, m_vis{vis}
        {
        }

        [[nodiscard]] constexpr std::span<const si::InternedString> path() const noexcept { return m_path; }
        [[nodiscard]] constexpr Visibility visibility() const noexcept { return m_vis; }

        void accept(Visitor& v) const override;

    private:
        std::span<const si::InternedString> m_path;
        Visibility m_vis;
    };

    enum class UsingKind : uint8_t
    {
        TypeAlias,
        Import,
        GroupImport,
        SymbolAlias,
    };

    class UsingDecl final : public Decl
    {
    public:
        explicit constexpr UsingDecl(sm::SourceRange range, si::InternedString name, TypeExpr* aliased_type, Visibility vis = Visibility::Private,
                                     bool is_export = false) noexcept
            : Decl{range}, m_name{name}, m_aliased{aliased_type}, m_vis{vis}, m_is_export{is_export}, m_kind{UsingKind::TypeAlias}
        {
        }

        explicit constexpr UsingDecl(sm::SourceRange range, std::span<const si::InternedString> path, Visibility vis = Visibility::Private,
                                     bool is_export = false) noexcept
            : Decl{range}, m_name{path.empty() ? si::InternedString{} : path[path.size() - 1]}, m_aliased{nullptr}, m_import_path{path}, m_vis{vis},
              m_is_export{is_export}, m_kind{UsingKind::Import}
        {
        }

        explicit constexpr UsingDecl(sm::SourceRange range, si::InternedString name, std::span<const si::InternedString> target_path, TypeExpr* aliased_type,
                                     Visibility vis = Visibility::Private, bool is_export = false) noexcept
            : Decl{range}, m_name{name}, m_aliased{aliased_type}, m_import_path{target_path}, m_vis{vis}, m_is_export{is_export}, m_kind{UsingKind::SymbolAlias}
        {
        }

        explicit constexpr UsingDecl(sm::SourceRange range, std::span<const si::InternedString> base_path, std::span<const si::InternedString> imported_names,
                                     Visibility vis = Visibility::Private, bool is_export = false) noexcept
            : Decl{range}, m_name{}, m_aliased{nullptr}, m_import_path{base_path}, m_group_names{imported_names}, m_vis{vis}, m_is_export{is_export},
              m_kind{UsingKind::GroupImport}
        {
        }

        [[nodiscard]] constexpr si::InternedString name() const noexcept { return m_name; }
        [[nodiscard]] constexpr TypeExpr* aliased_type() const noexcept { return m_aliased; }
        [[nodiscard]] constexpr Visibility visibility() const noexcept { return m_vis; }
        [[nodiscard]] constexpr bool is_export() const noexcept { return m_is_export; }
        [[nodiscard]] constexpr UsingKind kind() const noexcept { return m_kind; }
        [[nodiscard]] constexpr std::span<const si::InternedString> import_path() const noexcept { return m_import_path; }
        [[nodiscard]] constexpr std::span<const si::InternedString> group_names() const noexcept { return m_group_names; }

        void accept(Visitor& v) const override;

    private:
        si::InternedString m_name;
        TypeExpr* m_aliased;
        std::span<const si::InternedString> m_import_path;
        std::span<const si::InternedString> m_group_names;
        Visibility m_vis;
        bool m_is_export;
        UsingKind m_kind;
    };

    class TranslationUnit final : public Decl
    {
    public:
        explicit constexpr TranslationUnit(sm::SourceRange range, ModuleDecl* module_decl, std::span<Decl* const> decls) noexcept
            : Decl{range}, m_module{module_decl}, m_decls{decls}
        {
        }

        [[nodiscard]] constexpr ModuleDecl* module_decl() const noexcept { return m_module; }
        [[nodiscard]] constexpr std::span<Decl* const> decls() const noexcept { return m_decls; }

        void accept(Visitor& v) const override;

    private:
        ModuleDecl* m_module;
        std::span<Decl* const> m_decls;
    };

} // namespace dcc::ast

#endif /* DCC_AST_DECL_HH */
