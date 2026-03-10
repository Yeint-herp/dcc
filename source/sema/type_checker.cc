#include <ast/ambiguous.hh>
#include <ast/decl.hh>
#include <ast/expr.hh>
#include <ast/pattern.hh>
#include <ast/stmt.hh>
#include <ast/type.hh>
#include <format>
#include <sema/type_checker.hh>
#include <unordered_set>

namespace dcc::sema
{
    TypeChecker::TypeChecker(TypeContext& types, const ResolutionMap& resolutions, const TypeResolutionMap& type_resolutions,
                             const DisambiguationMap& disambiguations, diag::DiagnosticPrinter& printer)
        : m_types{types}, m_resolutions{resolutions}, m_type_resolutions{type_resolutions}, m_printer{printer}, m_disambiguations{disambiguations}
    {
    }

    bool TypeChecker::check(ast::TranslationUnit& tu)
    {
        tu.accept(*this);
        return m_error_count == 0;
    }

    SemaType* TypeChecker::type_of(const ast::Node* node) const noexcept
    {
        auto it = m_type_map.find(node);
        if (it != m_type_map.end())
            return m_types.resolve(it->second);

        return nullptr;
    }

    void TypeChecker::record_type(const ast::Node* node, SemaType* type)
    {
        m_type_map[node] = type;
    }

    SemaType* TypeChecker::check_expr(const ast::Expr& expr)
    {
        expr.accept(*this);
        auto it = m_type_map.find(&expr);
        if (it != m_type_map.end())
            return m_types.resolve(it->second);

        return m_types.error_type();
    }

    void TypeChecker::check_stmt(const ast::Stmt& stmt)
    {
        stmt.accept(*this);
    }

    SemaType* TypeChecker::eval_type(const ast::TypeExpr& type_expr)
    {
        auto it = m_type_resolutions.find(&type_expr);
        return it != m_type_resolutions.end() ? it->second : m_types.error_type();
    }

    void TypeChecker::error(sm::SourceRange range, std::string message)
    {
        m_printer.emit(diag::error(std::move(message)).with_primary(range, ""));
        ++m_error_count;
    }

    void TypeChecker::error_type_mismatch(SemaType* expected, SemaType* got, sm::SourceRange range)
    {
        m_printer.emit(diag::error(std::format("type mismatch: expected '{}', found '{}'", expected->to_string(), got->to_string()))
                           .with_primary(range, std::format("expected '{}'", expected->to_string())));

        ++m_error_count;
    }

    void TypeChecker::error_not_callable(SemaType* type, sm::SourceRange range)
    {
        m_printer.emit(diag::error(std::format("type '{}' is not callable", type->to_string())).with_primary(range, "expression is not a function"));
        ++m_error_count;
    }

    void TypeChecker::error_not_indexable(SemaType* type, sm::SourceRange range)
    {
        m_printer.emit(diag::error(std::format("type '{}' cannot be indexed", type->to_string())).with_primary(range, "not an array, slice, or pointer"));
        ++m_error_count;
    }

    void TypeChecker::error_no_member(SemaType* type, si::InternedString member, sm::SourceRange range)
    {
        m_printer.emit(
            diag::error(std::format("type '{}' has no member '{}'", type->to_string(), std::string{member.view()})).with_primary(range, "no such member"));

        ++m_error_count;
    }

    void TypeChecker::error_const_assign(sm::SourceRange range)
    {
        m_printer.emit(diag::error("cannot assign to a const-qualified variable").with_primary(range, "assignment to const"));
        ++m_error_count;
    }

    void TypeChecker::warning(sm::SourceRange range, std::string message)
    {
        m_printer.emit(diag::warning(std::move(message)).with_primary(range, ""));
    }

    void TypeChecker::note(sm::SourceRange range, std::string message)
    {
        m_printer.emit(diag::note(std::move(message)).with_primary(range, ""));
    }

    SemaType* TypeChecker::check_binary_arithmetic(ast::BinaryOp op, SemaType* lhs, SemaType* rhs, sm::SourceRange range)
    {
        lhs = materialize_type(lhs);
        rhs = materialize_type(rhs);

        if (lhs->is_error() || rhs->is_error())
            return m_types.error_type();

        auto* common = m_types.common_arithmetic_type(lhs, rhs);
        if (!common || (!common->is_numeric()))
        {
            error(range, std::format("invalid operands to binary '{}': '{}' and '{}'", static_cast<int>(op), lhs->to_string(), rhs->to_string()));
            return m_types.error_type();
        }

        return common;
    }

    SemaType* TypeChecker::check_binary_comparison(ast::BinaryOp, SemaType* lhs, SemaType* rhs, sm::SourceRange range)
    {
        lhs = materialize_type(lhs);
        rhs = materialize_type(rhs);

        if (lhs->is_error() || rhs->is_error())
            return m_types.bool_type();

        if (m_types.types_equal(lhs, rhs))
            return m_types.bool_type();

        if (lhs->is_numeric() && rhs->is_numeric())
            return m_types.bool_type();

        if (lhs->is_pointer() && rhs->is_pointer())
            return m_types.bool_type();

        if (lhs->is_pointer() && rhs->is_null_t())
            return m_types.bool_type();

        if (lhs->is_null_t() && rhs->is_pointer())
            return m_types.bool_type();

        if (lhs->is_enum() && rhs->is_enum() && m_types.types_equal(lhs, rhs))
            return m_types.bool_type();

        error(range, std::format("cannot compare '{}' and '{}'", lhs->to_string(), rhs->to_string()));
        return m_types.bool_type();
    }

    SemaType* TypeChecker::check_binary_logical(ast::BinaryOp, SemaType* lhs, SemaType* rhs, sm::SourceRange range)
    {
        lhs = materialize_type(lhs);
        rhs = materialize_type(rhs);

        if (!lhs->is_error() && !lhs->is_bool())
            error(range, std::format("logical operator requires 'bool', found '{}'", lhs->to_string()));

        if (!rhs->is_error() && !rhs->is_bool())
            error(range, std::format("logical operator requires 'bool', found '{}'", rhs->to_string()));

        return m_types.bool_type();
    }

    SemaType* TypeChecker::check_binary_bitwise(ast::BinaryOp op, SemaType* lhs, SemaType* rhs, sm::SourceRange range)
    {
        lhs = materialize_type(lhs);
        rhs = materialize_type(rhs);

        if (lhs->is_error() || rhs->is_error())
            return m_types.error_type();

        if (!lhs->is_integer() || !rhs->is_integer())
        {
            error(range, std::format("bitwise operator requires integer operands, found '{}' and '{}'", lhs->to_string(), rhs->to_string()));
            return m_types.error_type();
        }

        if (op == ast::BinaryOp::Shl || op == ast::BinaryOp::Shr)
            return lhs;

        return m_types.widen_integers(lhs, rhs);
    }

    SemaType* TypeChecker::check_unary(ast::UnaryOp op, SemaType* operand, sm::SourceRange range)
    {
        operand = materialize_type(operand);
        if (operand->is_error())
            return m_types.error_type();

        switch (op)
        {
            case ast::UnaryOp::Negate:
                if (!operand->is_numeric())
                {
                    error(range, std::format("cannot negate type '{}'", operand->to_string()));
                    return m_types.error_type();
                }
                return operand;

            case ast::UnaryOp::BitNot:
                if (!operand->is_integer())
                {
                    error(range, std::format("bitwise NOT requires integer type, found '{}'", operand->to_string()));
                    return m_types.error_type();
                }
                return operand;

            case ast::UnaryOp::LogNot:
                if (!operand->is_bool())
                {
                    error(range, std::format("logical NOT requires 'bool', found '{}'", operand->to_string()));
                    return m_types.error_type();
                }
                return m_types.bool_type();

            case ast::UnaryOp::Deref:
                if (!operand->is_pointer())
                {
                    error(range, std::format("cannot dereference non-pointer type '{}'", operand->to_string()));
                    return m_types.error_type();
                }
                return static_cast<PointerSemaType*>(operand)->pointee();

            case ast::UnaryOp::AddressOf:
                return m_types.pointer_to(operand);

            case ast::UnaryOp::PreInc:
            case ast::UnaryOp::PostInc:
                if (!operand->is_integer() && !operand->is_pointer())
                {
                    error(range, std::format("increment requires integer or pointer type, found '{}'", operand->to_string()));
                    return m_types.error_type();
                }
                goto return_operand;
            case ast::UnaryOp::PreDec:
            case ast::UnaryOp::PostDec:
                if (!operand->is_integer() && !operand->is_pointer())
                {
                    error(range, std::format("decrement requires integer or pointer type, found '{}'", operand->to_string()));
                    return m_types.error_type();
                }
            return_operand:
                return operand;
        }
        return m_types.error_type();
    }

    SemaType* TypeChecker::materialize_type(SemaType* ty)
    {
        ty = m_types.resolve(ty);
        if (ty->is_type_var())
        {
            auto def = m_literal_defaults.find(ty);
            if (def != m_literal_defaults.end())
            {
                std::ignore = m_types.unify(ty, def->second);
                return m_types.resolve(ty);
            }
        }
        return ty;
    }

    bool TypeChecker::check_assignment_compatible(SemaType* target, SemaType* value, sm::SourceRange range)
    {
        target = m_types.resolve(target);
        value = m_types.resolve(value);

        if (target->is_error() || value->is_error())
            return true;

        if (m_types.types_equal(target, value))
            return true;

        if (m_types.is_implicitly_convertible(value, target))
            return true;

        if (value->is_type_var())
        {
            auto def = m_literal_defaults.find(value);
            if (def != m_literal_defaults.end())
            {
                if (def->second->is_integer() && target->is_integer())
                    return m_types.unify(value, target);

                if (def->second->is_integer() && target->is_float())
                    return m_types.unify(value, target);

                if (def->second->is_float() && target->is_float())
                    return m_types.unify(value, target);

                std::ignore = m_types.unify(value, def->second);
                value = m_types.resolve(value);

                if (m_types.types_equal(target, value))
                    return true;

                if (m_types.is_implicitly_convertible(value, target))
                    return true;
            }
        }

        if (m_types.unify(target, value))
            return true;

        error_type_mismatch(target, value, range);
        return false;
    }

    SemaType* TypeChecker::check_call(const FunctionSemaType* fn_type, std::span<ast::Expr* const> args, sm::SourceRange range)
    {
        auto params = fn_type->param_types();
        std::size_t required = params.size();

        if (!fn_type->is_variadic() && args.size() != required)
        {
            error(range, std::format("expected {} argument{}, found {}", required, required == 1 ? "" : "s", args.size()));
            return fn_type->return_type();
        }

        if (fn_type->is_variadic() && args.size() < required)
        {
            error(range, std::format("expected at least {} argument{}, found {}", required, required == 1 ? "" : "s", args.size()));
            return fn_type->return_type();
        }

        for (std::size_t i = 0; i < std::min(args.size(), required); ++i)
        {
            auto* arg_type = check_expr(*args[i]);
            check_assignment_compatible(params[i], arg_type, args[i]->range());
        }

        for (std::size_t i = required; i < args.size(); ++i)
            check_expr(*args[i]);

        return fn_type->return_type();
    }

    SemaType* TypeChecker::coerce(SemaType* from, SemaType* to, sm::SourceRange range)
    {
        from = m_types.resolve(from);
        to = m_types.resolve(to);

        if (m_types.types_equal(from, to))
            return to;

        if (m_types.is_implicitly_convertible(from, to))
            return to;

        error_type_mismatch(to, from, range);
        return m_types.error_type();
    }

    bool TypeChecker::is_mutable_lvalue(const ast::Expr& expr) const noexcept
    {
        if (auto* ident = dynamic_cast<const ast::IdentifierExpr*>(&expr))
        {
            auto it = m_resolutions.find(ident);
            if (it != m_resolutions.end() && it->second)
                return !ast::has_qualifier(it->second->quals(), ast::Qualifier::Const);
        }

        if (auto* deref = dynamic_cast<const ast::UnaryExpr*>(&expr))
            if (deref->op() == ast::UnaryOp::Deref)
                return true;

        if (auto* member = dynamic_cast<const ast::MemberAccessExpr*>(&expr))
            return is_mutable_lvalue(*member->object());

        if (auto* idx = dynamic_cast<const ast::IndexExpr*>(&expr))
            return is_mutable_lvalue(*idx->object());

        return false;
    }

    void TypeChecker::check_pattern(const ast::Pattern& pat, SemaType* scrutinee_type)
    {
        auto* saved = m_match_scrutinee_type;
        m_match_scrutinee_type = scrutinee_type;
        pat.accept(*this);

        m_match_scrutinee_type = saved;
    }

    void TypeChecker::check_match_exhaustiveness(const EnumSemaType* enum_type, std::span<const ast::MatchArm> arms, sm::SourceRange range)
    {
        if (!enum_type || !enum_type->is_complete())
            return;

        std::unordered_set<si::InternedString> covered;
        bool has_wildcard = false;

        for (auto& arm : arms)
        {
            if (dynamic_cast<const ast::WildcardPattern*>(arm.pattern) || dynamic_cast<const ast::BindingPattern*>(arm.pattern))
            {
                has_wildcard = true;
                break;
            }

            if (auto* ep = dynamic_cast<const ast::EnumPattern*>(arm.pattern))
                if (!ep->path().empty())
                    covered.insert(ep->path().back());
        }

        if (has_wildcard)
            return;

        for (auto& variant : enum_type->variants())
            if (!covered.contains(variant.name))
                warning(range, std::format("non-exhaustive match: variant '{}' not covered", std::string{variant.name.view()}));
    }

    void TypeChecker::complete_struct(StructSemaType* sty, const ast::StructDecl& decl)
    {
        if (sty->is_complete())
            return;

        std::vector<FieldInfo> fields;
        uint32_t idx = 0;
        for (auto* field : decl.fields())
        {
            FieldInfo fi;
            fi.name = field->name();
            fi.type = eval_type(*field->type());
            fi.decl = const_cast<ast::FieldDecl*>(field);
            fi.visibility = field->visibility();
            fi.index = idx++;
            fields.push_back(std::move(fi));

            if (field->default_value())
                check_expr(*field->default_value());
        }
        sty->set_fields(std::move(fields));

        std::vector<MethodInfo> methods;
        for (auto* method : decl.methods())
        {
            MethodInfo mi;
            mi.name = method->name();
            mi.decl = const_cast<ast::FunctionDecl*>(method);
            mi.visibility = method->visibility();

            auto it = m_resolutions.find(method);
            if (it != m_resolutions.end() && it->second && it->second->type())
                mi.type = dynamic_cast<FunctionSemaType*>(it->second->type());

            methods.push_back(std::move(mi));
        }
        sty->set_methods(std::move(methods));
        sty->mark_complete();
    }

    void TypeChecker::complete_union(UnionSemaType* uty, const ast::UnionDecl& decl)
    {
        if (uty->is_complete())
            return;

        std::vector<FieldInfo> fields;
        uint32_t idx = 0;
        for (auto* field : decl.fields())
        {
            FieldInfo fi;
            fi.name = field->name();
            fi.type = eval_type(*field->type());
            fi.decl = const_cast<ast::FieldDecl*>(field);
            fi.visibility = field->visibility();
            fi.index = idx++;
            fields.push_back(std::move(fi));
        }
        uty->set_fields(std::move(fields));

        std::vector<MethodInfo> methods;
        for (auto* method : decl.methods())
        {
            MethodInfo mi;
            mi.name = method->name();
            mi.decl = const_cast<ast::FunctionDecl*>(method);
            mi.visibility = method->visibility();
            auto it = m_resolutions.find(method);
            if (it != m_resolutions.end() && it->second && it->second->type())
                mi.type = dynamic_cast<FunctionSemaType*>(it->second->type());

            methods.push_back(std::move(mi));
        }

        uty->set_methods(std::move(methods));
        uty->mark_complete();
    }

    void TypeChecker::complete_enum(EnumSemaType* ety, const ast::EnumDecl& decl)
    {
        if (ety->is_complete())
            return;

        std::vector<VariantInfo> variants;
        int64_t next_discriminant = 0;

        for (auto* variant : decl.variants())
        {
            VariantInfo vi;
            vi.name = variant->name();
            vi.decl = const_cast<ast::EnumVariantDecl*>(variant);

            if (variant->discriminant())
            {
                auto* disc_type = materialize_type(check_expr(*variant->discriminant()));
                if (!disc_type->is_error() && !disc_type->is_integer())
                    error(variant->discriminant()->range(), "enum discriminant must be an integer");

                if (auto* lit = dynamic_cast<const ast::IntegerLiteral*>(variant->discriminant()))
                    if (lit->value().is_int())
                        next_discriminant = lit->value().as_int_value();
            }
            vi.discriminant = next_discriminant++;

            for (auto* pt : variant->payload_types())
                vi.payload_types.push_back(eval_type(*pt));

            variants.push_back(std::move(vi));
        }
        ety->set_variants(std::move(variants));

        std::vector<MethodInfo> methods;
        for (auto* method : decl.methods())
        {
            MethodInfo mi;
            mi.name = method->name();
            mi.decl = const_cast<ast::FunctionDecl*>(method);
            mi.visibility = method->visibility();
            auto it = m_resolutions.find(method);
            if (it != m_resolutions.end() && it->second && it->second->type())
                mi.type = dynamic_cast<FunctionSemaType*>(it->second->type());

            methods.push_back(std::move(mi));
        }
        ety->set_methods(std::move(methods));
        ety->mark_complete();
    }

    void TypeChecker::visit(const ast::BuiltinType& node)
    {
        record_type(&node, m_types.from_primitive(node.kind()));
    }

    void TypeChecker::visit(const ast::NamedType& node)
    {
        record_type(&node, eval_type(node));
    }

    void TypeChecker::visit(const ast::QualifiedType& node)
    {
        record_type(&node, eval_type(node));
    }

    void TypeChecker::visit(const ast::DottedNamedType&) {}

    void TypeChecker::visit(const ast::PointerType& node)
    {
        record_type(&node, eval_type(node));
    }

    void TypeChecker::visit(const ast::SliceType& node)
    {
        record_type(&node, eval_type(node));
    }

    void TypeChecker::visit(const ast::ArrayType& node)
    {
        if (node.size())
        {
            auto* sz_type = check_expr(*node.size());
            if (!sz_type->is_error() && !sz_type->is_integer())
                error(node.size()->range(), "array size must be an integer constant");
        }

        record_type(&node, eval_type(node));
    }

    void TypeChecker::visit(const ast::FlexibleArrayType& node)
    {
        record_type(&node, eval_type(node));
    }

    void TypeChecker::visit(const ast::FunctionType& node)
    {
        record_type(&node, eval_type(node));
    }

    void TypeChecker::visit(const ast::TemplateType& node)
    {
        record_type(&node, eval_type(node));
    }

    void TypeChecker::visit(const ast::TypeofType& node)
    {
        auto* inner_type = check_expr(*node.inner());
        auto it = m_type_resolutions.find(&node);
        if (it != m_type_resolutions.end() && it->second->is_type_var())
            std::ignore = m_types.unify(it->second, inner_type);

        record_type(&node, inner_type);
    }

    void TypeChecker::visit(const ast::IntegerLiteral& node)
    {
        auto* var = m_types.fresh_var();
        m_literal_defaults[var] = m_types.integer_type(32, true);
        record_type(&node, var);
    }

    void TypeChecker::visit(const ast::FloatLiteral& node)
    {
        auto* var = m_types.fresh_var();
        m_literal_defaults[var] = m_types.float_type(64);
        record_type(&node, var);
    }

    void TypeChecker::visit(const ast::StringLiteral& node)
    {
        auto* u8_type = m_types.integer_type(8, false);
        record_type(&node, m_types.pointer_to(u8_type, ast::Qualifier::Const));
    }

    void TypeChecker::visit(const ast::CharLiteral& node)
    {
        record_type(&node, m_types.integer_type(32, false));
    }

    void TypeChecker::visit(const ast::BoolLiteral& node)
    {
        record_type(&node, m_types.bool_type());
    }

    void TypeChecker::visit(const ast::NullLiteral& node)
    {
        record_type(&node, m_types.null_t_type());
    }

    void TypeChecker::visit(const ast::IdentifierExpr& node)
    {
        auto it = m_resolutions.find(&node);
        if (it == m_resolutions.end() || !it->second)
        {
            record_type(&node, m_types.error_type());
            return;
        }

        auto* sym = it->second;
        auto* ty = sym->type() ? m_types.resolve(sym->type()) : m_types.error_type();
        record_type(&node, ty);
    }

    void TypeChecker::visit(const ast::GroupingExpr& node)
    {
        record_type(&node, check_expr(*node.inner()));
    }

    void TypeChecker::visit(const ast::BinaryExpr& node)
    {
        auto* lhs = check_expr(*node.lhs());
        auto* rhs = check_expr(*node.rhs());

        using BO = ast::BinaryOp;
        SemaType* result = nullptr;

        switch (node.op())
        {
            case BO::Add:
            case BO::Sub:
            case BO::Mul:
            case BO::Div:
            case BO::Mod:
                result = check_binary_arithmetic(node.op(), lhs, rhs, node.range());
                break;
            case BO::Eq:
            case BO::Ne:
            case BO::Lt:
            case BO::Le:
            case BO::Gt:
            case BO::Ge:
                result = check_binary_comparison(node.op(), lhs, rhs, node.range());
                break;
            case BO::LogAnd:
            case BO::LogOr:
                result = check_binary_logical(node.op(), lhs, rhs, node.range());
                break;
            case BO::BitAnd:
            case BO::BitOr:
            case BO::BitXor:
            case BO::Shl:
            case BO::Shr:
                result = check_binary_bitwise(node.op(), lhs, rhs, node.range());
                break;
        }

        record_type(&node, result ? result : m_types.error_type());
    }

    void TypeChecker::visit(const ast::UnaryExpr& node)
    {
        auto* operand = check_expr(*node.operand());

        switch (node.op())
        {
            case ast::UnaryOp::PreInc:
            case ast::UnaryOp::PostInc:
                if (!is_mutable_lvalue(*node.operand()))
                    error(node.range(), "operand of increment must be a mutable");
                break;
            case ast::UnaryOp::PreDec:
            case ast::UnaryOp::PostDec:
                if (!is_mutable_lvalue(*node.operand()))
                    error(node.range(), "operand of decrement must be a mutable");
                break;
            default:
                break;
        }

        record_type(&node, check_unary(node.op(), operand, node.range()));
    }

    void TypeChecker::visit(const ast::AssignExpr& node)
    {
        auto* target_type = check_expr(*node.target());
        auto* value_type = check_expr(*node.value());

        if (!is_mutable_lvalue(*node.target()))
            error_const_assign(node.target()->range());

        if (node.op() == ast::AssignOp::Simple)
            check_assignment_compatible(target_type, value_type, node.range());
        else
        {
            ast::BinaryOp bin_op = ast::BinaryOp::Add;
            switch (node.op())
            {
                case ast::AssignOp::Add:
                    bin_op = ast::BinaryOp::Add;
                    break;
                case ast::AssignOp::Sub:
                    bin_op = ast::BinaryOp::Sub;
                    break;
                case ast::AssignOp::Mul:
                    bin_op = ast::BinaryOp::Mul;
                    break;
                case ast::AssignOp::Div:
                    bin_op = ast::BinaryOp::Div;
                    break;
                case ast::AssignOp::Mod:
                    bin_op = ast::BinaryOp::Mod;
                    break;
                case ast::AssignOp::BitAnd:
                    bin_op = ast::BinaryOp::BitAnd;
                    break;
                case ast::AssignOp::BitOr:
                    bin_op = ast::BinaryOp::BitOr;
                    break;
                case ast::AssignOp::BitXor:
                    bin_op = ast::BinaryOp::BitXor;
                    break;
                case ast::AssignOp::Shl:
                    bin_op = ast::BinaryOp::Shl;
                    break;
                case ast::AssignOp::Shr:
                    bin_op = ast::BinaryOp::Shr;
                    break;
                default:
                    break;
            }

            check_binary_arithmetic(bin_op, target_type, value_type, node.range());
        }

        record_type(&node, target_type);
    }

    void TypeChecker::visit(const ast::ConditionalExpr& node)
    {
        auto* cond = materialize_type(check_expr(*node.condition()));
        if (!cond->is_error() && !cond->is_bool())
            error(node.condition()->range(), std::format("condition must be 'bool', found '{}'", cond->to_string()));

        auto* then_type = materialize_type(check_expr(*node.then_expr()));
        auto* else_type = materialize_type(check_expr(*node.else_expr()));

        if (m_types.types_equal(then_type, else_type))
        {
            record_type(&node, then_type);
            return;
        }

        if (m_types.is_implicitly_convertible(else_type, then_type))
        {
            record_type(&node, then_type);
            return;
        }

        if (m_types.is_implicitly_convertible(then_type, else_type))
        {
            record_type(&node, else_type);
            return;
        }

        error(node.range(), std::format("conditional branches have incompatible types: '{}' and '{}'", then_type->to_string(), else_type->to_string()));
        record_type(&node, then_type);
    }

    void TypeChecker::visit(const ast::CastExpr& node)
    {
        auto* operand_type = check_expr(*node.operand());
        auto* target_type = eval_type(*node.target_type());

        if (!operand_type->is_error() && !target_type->is_error())
            if (!m_types.is_explicitly_castable(operand_type, target_type))
                error(node.range(), std::format("cannot cast '{}' to '{}'", operand_type->to_string(), target_type->to_string()));

        record_type(&node, target_type);
    }

    void TypeChecker::visit(const ast::MemberAccessExpr& node)
    {
        auto* obj_type = check_expr(*node.object());
        obj_type = m_types.resolve(obj_type);

        if (obj_type->is_error())
        {
            record_type(&node, m_types.error_type());
            return;
        }

        if (obj_type->is_pointer())
            obj_type = m_types.resolve(static_cast<PointerSemaType*>(obj_type)->pointee());

        if (obj_type->is_aggregate())
        {
            if (auto* sty = dynamic_cast<StructSemaType*>(obj_type))
            {
                if (auto* fi = sty->find_field(node.member()))
                {
                    record_type(&node, fi->type);
                    return;
                }

                if (auto* mi = sty->find_method(node.member()))
                {
                    record_type(&node, mi->type);
                    return;
                }
            }
            else if (auto* uty = dynamic_cast<UnionSemaType*>(obj_type))
            {
                if (auto* fi = uty->find_field(node.member()))
                {
                    record_type(&node, fi->type);
                    return;
                }

                if (auto* mi = uty->find_method(node.member()))
                {
                    record_type(&node, mi->type);
                    return;
                }
            }
        }

        if (obj_type->is_enum())
        {
            auto* ety = static_cast<EnumSemaType*>(obj_type);
            if (auto* mi = ety->find_method(node.member()))
            {
                record_type(&node, mi->type);
                return;
            }
        }

        if (obj_type->is_slice())
        {
            auto* slice = static_cast<SliceSemaType*>(obj_type);
            if (std::string_view sv = node.member().view(); sv == "len")
            {
                record_type(&node, m_types.integer_type(64, false));
                return;
            }

            else if (sv == "ptr")
            {
                record_type(&node, m_types.pointer_to(slice->element()));
                return;
            }
        }

        error_no_member(obj_type, node.member(), node.range());
        record_type(&node, m_types.error_type());
    }

    void TypeChecker::visit(const ast::CallExpr& node)
    {
        auto* callee_type = check_expr(*node.callee());
        callee_type = m_types.resolve(callee_type);

        if (callee_type->is_error())
        {
            for (auto* arg : node.args())
                check_expr(*arg);

            record_type(&node, m_types.error_type());
            return;
        }

        if (!callee_type->is_function())
        {
            error_not_callable(callee_type, node.callee()->range());
            for (auto* arg : node.args())
                check_expr(*arg);

            record_type(&node, m_types.error_type());
            return;
        }

        auto* fn = static_cast<FunctionSemaType*>(callee_type);
        auto* ret = check_call(fn, node.args(), node.range());
        record_type(&node, ret);
    }

    void TypeChecker::visit(const ast::IndexExpr& node)
    {
        auto* obj_type = check_expr(*node.object());
        auto* idx_type = materialize_type(check_expr(*node.index()));
        obj_type = m_types.resolve(obj_type);

        if (obj_type->is_error())
        {
            record_type(&node, m_types.error_type());
            return;
        }

        if (!idx_type->is_error() && !idx_type->is_integer())
            error(node.index()->range(), std::format("index must be an integer, found '{}'", idx_type->to_string()));

        if (obj_type->is_array())
            record_type(&node, static_cast<ArraySemaType*>(obj_type)->element());
        else if (obj_type->is_slice())
            record_type(&node, static_cast<SliceSemaType*>(obj_type)->element());
        else if (obj_type->is_pointer())
            record_type(&node, static_cast<PointerSemaType*>(obj_type)->pointee());
        else
        {
            error_not_indexable(obj_type, node.object()->range());
            record_type(&node, m_types.error_type());
        }
    }

    void TypeChecker::visit(const ast::SliceExpr& node)
    {
        auto* obj_type = check_expr(*node.object());
        if (node.begin_idx())
        {
            auto* bt = materialize_type(check_expr(*node.begin_idx()));
            if (!bt->is_error() && !bt->is_integer())
                error(node.begin_idx()->range(), "slice index must be an integer");
        }

        if (node.end_idx())
        {
            auto* et = materialize_type(check_expr(*node.end_idx()));
            if (!et->is_error() && !et->is_integer())
                error(node.end_idx()->range(), "slice index must be an integer");
        }

        obj_type = m_types.resolve(obj_type);

        if (obj_type->is_array())
            record_type(&node, m_types.slice_of(static_cast<ArraySemaType*>(obj_type)->element()));
        else if (obj_type->is_slice())
            record_type(&node, obj_type);
        else if (obj_type->is_pointer())
            record_type(&node, m_types.slice_of(static_cast<PointerSemaType*>(obj_type)->pointee()));
        else
        {
            if (!obj_type->is_error())
                error(node.object()->range(), std::format("cannot slice type '{}'", obj_type->to_string()));

            record_type(&node, m_types.error_type());
        }
    }

    void TypeChecker::visit(const ast::InitializerExpr& node)
    {
        if (!node.type())
        {
            for (auto& fi : node.fields())
                check_expr(*fi.value);

            record_type(&node, m_types.error_type());
            return;
        }

        auto* target = eval_type(*node.type());
        target = m_types.resolve(target);

        if (target->is_error())
        {
            for (auto& fi : node.fields())
                check_expr(*fi.value);

            record_type(&node, m_types.error_type());
            return;
        }

        if (auto* sty = dynamic_cast<StructSemaType*>(target))
        {
            if (!sty->is_complete())
            {
                error(node.range(), std::format("cannot initialize incomplete type '{}'", sty->to_string()));
                record_type(&node, target);
                return;
            }

            for (auto& fi : node.fields())
            {
                auto* val_type = check_expr(*fi.value);
                auto* field_info = sty->find_field(fi.name);
                if (!field_info)
                {
                    error(fi.range, std::format("'{}' has no field '{}'", sty->to_string(), std::string{fi.name.view()}));
                    continue;
                }

                check_assignment_compatible(field_info->type, val_type, fi.range);
            }
        }
        else if (auto* uty = dynamic_cast<UnionSemaType*>(target))
        {
            if (!uty->is_complete())
            {
                error(node.range(), std::format("cannot initialize incomplete type '{}'", uty->to_string()));
                record_type(&node, target);
                return;
            }

            for (auto& fi : node.fields())
            {
                auto* val_type = check_expr(*fi.value);
                auto* field_info = uty->find_field(fi.name);
                if (!field_info)
                {
                    error(fi.range, std::format("'{}' has no field '{}'", uty->to_string(), std::string{fi.name.view()}));
                    continue;
                }

                check_assignment_compatible(field_info->type, val_type, fi.range);
            }
        }
        else
            error(node.range(), std::format("type '{}' cannot be used in an initializer expression", target->to_string()));

        record_type(&node, target);
    }

    void TypeChecker::visit(const ast::BlockExpr& node)
    {
        for (auto* stmt : node.stmts())
            check_stmt(*stmt);
        if (node.tail())
            record_type(&node, check_expr(*node.tail()));
        else
            record_type(&node, m_types.void_type());
    }

    void TypeChecker::visit(const ast::MatchExpr& node)
    {
        auto* scrutinee_type = check_expr(*node.scrutinee());
        scrutinee_type = m_types.resolve(scrutinee_type);

        SemaType* result_type = nullptr;

        for (auto& arm : node.arms())
        {
            check_pattern(*arm.pattern, scrutinee_type);
            if (arm.guard)
            {
                auto* guard_type = check_expr(*arm.guard);
                if (!guard_type->is_error() && !guard_type->is_bool())
                    error(arm.guard->range(), "match guard must be 'bool'");
            }

            SemaType* arm_type = nullptr;
            if (auto* expr = dynamic_cast<ast::Expr*>(arm.body))
                arm_type = check_expr(*expr);
            else if (auto* stmt = dynamic_cast<ast::Stmt*>(arm.body))
            {
                check_stmt(*stmt);
                arm_type = m_types.void_type();
            }

            if (!result_type)
                result_type = arm_type;
            else if (arm_type && !m_types.types_equal(result_type, arm_type))
            {
                if (!m_types.unify(result_type, arm_type))
                    error(arm.range,
                          std::format("match arm type '{}' is incompatible with previous arm type '{}'", arm_type->to_string(), result_type->to_string()));
            }
        }

        if (scrutinee_type->is_enum())
            check_match_exhaustiveness(static_cast<EnumSemaType*>(scrutinee_type), node.arms(), node.range());

        record_type(&node, result_type ? result_type : m_types.void_type());
    }

    void TypeChecker::visit(const ast::SizeofExpr& node)
    {
        eval_type(*node.operand());
        record_type(&node, m_types.integer_type(64, false));
    }

    void TypeChecker::visit(const ast::AlignofExpr& node)
    {
        eval_type(*node.operand());
        record_type(&node, m_types.integer_type(64, false));
    }

    void TypeChecker::visit(const ast::MacroCallExpr& node)
    {
        for (auto* arg : node.args())
            check_expr(*arg);

        record_type(&node, m_types.fresh_var());
    }

    void TypeChecker::visit(const ast::ExprStmt& node)
    {
        check_expr(*node.expr());
    }

    void TypeChecker::visit(const ast::DeclStmt& node)
    {
        node.decl()->accept(*this);
    }

    void TypeChecker::visit(const ast::BlockStmt& node)
    {
        for (auto* stmt : node.stmts())
            check_stmt(*stmt);
    }

    void TypeChecker::visit(const ast::ReturnStmt& node)
    {
        if (node.value())
        {
            auto* val_type = check_expr(*node.value());
            if (m_current_return_type)
                check_assignment_compatible(m_current_return_type, val_type, node.value()->range());
        }
        else
        {
            if (m_current_return_type && !m_current_return_type->is_void() && !m_current_return_type->is_error())
                error(node.range(), std::format("non-void function must return a value of type '{}'", m_current_return_type->to_string()));
        }
    }

    void TypeChecker::visit(const ast::IfStmt& node)
    {
        auto* cond = materialize_type(check_expr(*node.condition()));
        if (!cond->is_error() && !cond->is_bool())
        {
            error(node.condition()->range(), std::format("if condition must be 'bool', found '{}'", cond->to_string()));
        }

        check_stmt(*node.then_branch());
        if (node.else_branch())
            check_stmt(*node.else_branch());
    }

    void TypeChecker::visit(const ast::WhileStmt& node)
    {
        auto* cond = materialize_type(check_expr(*node.condition()));
        if (!cond->is_error() && !cond->is_bool())
            error(node.condition()->range(), std::format("while condition must be 'bool', found '{}'", cond->to_string()));

        check_stmt(*node.body());
    }

    void TypeChecker::visit(const ast::ForStmt& node)
    {
        if (node.init())
            check_stmt(*node.init());

        if (node.condition())
        {
            auto* cond = materialize_type(check_expr(*node.condition()));
            if (!cond->is_error() && !cond->is_bool())
                error(node.condition()->range(), std::format("for condition must be 'bool', found '{}'", cond->to_string()));
        }

        if (node.increment())
            check_expr(*node.increment());

        check_stmt(*node.body());
    }

    void TypeChecker::visit(const ast::DoWhileStmt& node)
    {
        check_stmt(*node.body());
        auto* cond = materialize_type(check_expr(*node.condition()));
        if (!cond->is_error() && !cond->is_bool())
            error(node.condition()->range(), std::format("do-while condition must be 'bool', found '{}'", cond->to_string()));
    }

    void TypeChecker::visit(const ast::BreakStmt&) {}
    void TypeChecker::visit(const ast::ContinueStmt&) {}

    void TypeChecker::visit(const ast::DeferStmt& node)
    {
        check_stmt(*node.body());
    }

    void TypeChecker::visit(const ast::MatchStmt& node)
    {
        auto* scrutinee_type = check_expr(*node.scrutinee());
        scrutinee_type = m_types.resolve(scrutinee_type);

        for (auto& arm : node.arms())
        {
            check_pattern(*arm.pattern, scrutinee_type);
            if (arm.guard)
            {
                auto* gt = check_expr(*arm.guard);
                if (!gt->is_error() && !gt->is_bool())
                    error(arm.guard->range(), "match guard must be 'bool'");
            }

            arm.body->accept(*this);
        }

        if (scrutinee_type->is_enum())
            check_match_exhaustiveness(static_cast<EnumSemaType*>(scrutinee_type), node.arms(), node.range());
    }

    void TypeChecker::visit(const ast::EmptyStmt&) {}
    void TypeChecker::visit(const ast::TemplateTypeParamDecl&) {}

    void TypeChecker::visit(const ast::TemplateValueParamDecl& node)
    {
        if (node.default_value())
            check_expr(*node.default_value());
    }

    void TypeChecker::visit(const ast::VarDecl& node)
    {
        SemaType* declared_type = nullptr;
        if (node.type())
            declared_type = eval_type(*node.type());

        SemaType* init_type = nullptr;
        if (node.init())
        {
            if (declared_type)
            {
                if (auto* init_expr = dynamic_cast<const ast::InitializerExpr*>(node.init()))
                {
                    if (!init_expr->type())
                    {
                        auto* target = m_types.resolve(declared_type);

                        if (!target->is_error())
                        {
                            if (auto* sty = dynamic_cast<StructSemaType*>(target))
                            {
                                if (!sty->is_complete())
                                    error(init_expr->range(), std::format("cannot initialize incomplete type '{}'", sty->to_string()));
                                else
                                {
                                    for (auto& fi : init_expr->fields())
                                    {
                                        auto* val_type = check_expr(*fi.value);
                                        auto* field_info = sty->find_field(fi.name);
                                        if (!field_info)
                                        {
                                            error(fi.range, std::format("'{}' has no field '{}'", sty->to_string(), std::string{fi.name.view()}));
                                            continue;
                                        }

                                        check_assignment_compatible(field_info->type, val_type, fi.range);
                                    }
                                }
                            }
                            else if (auto* uty = dynamic_cast<UnionSemaType*>(target))
                            {
                                if (!uty->is_complete())
                                {
                                    error(init_expr->range(), std::format("cannot initialize incomplete type '{}'", uty->to_string()));
                                }
                                else
                                {
                                    for (auto& fi : init_expr->fields())
                                    {
                                        auto* val_type = check_expr(*fi.value);
                                        auto* field_info = uty->find_field(fi.name);
                                        if (!field_info)
                                        {
                                            error(fi.range, std::format("'{}' has no field '{}'", uty->to_string(), std::string{fi.name.view()}));
                                            continue;
                                        }

                                        check_assignment_compatible(field_info->type, val_type, fi.range);
                                    }
                                }
                            }
                            else
                            {
                                for (auto& fi : init_expr->fields())
                                    check_expr(*fi.value);

                                error(init_expr->range(), std::format("type '{}' cannot be used in an initializer expression", target->to_string()));
                            }
                        }
                        else
                            for (auto& fi : init_expr->fields())
                                check_expr(*fi.value);

                        init_type = declared_type;

                        auto it = m_resolutions.find(&node);
                        Symbol* sym = (it != m_resolutions.end()) ? it->second : nullptr;
                        if (sym)
                        {
                            std::ignore = m_types.unify(sym->type(), declared_type);
                            record_type(&node, m_types.resolve(sym->type()));
                        }
                        return;
                    }
                }
            }

            init_type = check_expr(*node.init());
        }

        auto it = m_resolutions.find(&node);
        Symbol* sym = (it != m_resolutions.end()) ? it->second : nullptr;

        if (declared_type && init_type)
        {
            check_assignment_compatible(declared_type, init_type, node.init()->range());
            if (sym)
                std::ignore = m_types.unify(sym->type(), declared_type);
        }
        else if (declared_type)
        {
            if (sym)
                std::ignore = m_types.unify(sym->type(), declared_type);
        }
        else if (init_type)
        {
            if (sym)
                std::ignore = m_types.unify(sym->type(), init_type);
        }
        else
            error(node.range(), "variable declaration requires either a type annotation or an initializer");

        if (sym)
            record_type(&node, m_types.resolve(sym->type()));
    }

    void TypeChecker::visit(const ast::ParamDecl& node)
    {
        auto* param_type = eval_type(*node.type());
        if (node.default_value())
        {
            auto* def_type = check_expr(*node.default_value());
            check_assignment_compatible(param_type, def_type, node.default_value()->range());
        }

        record_type(&node, param_type);
    }

    void TypeChecker::visit(const ast::FieldDecl& node)
    {
        auto* field_type = eval_type(*node.type());
        if (node.default_value())
        {
            auto* def_type = check_expr(*node.default_value());
            check_assignment_compatible(field_type, def_type, node.default_value()->range());
        }

        record_type(&node, field_type);
    }

    void TypeChecker::visit(const ast::FunctionDecl& node)
    {
        auto it = m_resolutions.find(&node);
        Symbol* sym = (it != m_resolutions.end()) ? it->second : nullptr;
        FunctionSemaType* fn_type = sym ? dynamic_cast<FunctionSemaType*>(sym->type()) : nullptr;

        SemaType* ret = fn_type ? fn_type->return_type() : m_types.void_type();

        auto* saved_return = m_current_return_type;
        m_current_return_type = ret;

        for (auto* tp : node.template_params())
            tp->accept(*this);

        for (auto* param : node.params())
            param->accept(*this);

        if (node.body())
            node.body()->accept(*this);

        m_current_return_type = saved_return;

        if (fn_type)
            record_type(&node, fn_type);
    }

    void TypeChecker::visit(const ast::StructDecl& node)
    {
        auto it = m_resolutions.find(&node);
        Symbol* sym = (it != m_resolutions.end()) ? it->second : nullptr;

        if (sym && sym->type())
            if (auto* sty = dynamic_cast<StructSemaType*>(sym->type()))
                complete_struct(sty, node);

        for (auto* method : node.methods())
            method->accept(*this);
    }

    void TypeChecker::visit(const ast::UnionDecl& node)
    {
        auto it = m_resolutions.find(&node);
        Symbol* sym = (it != m_resolutions.end()) ? it->second : nullptr;

        if (sym && sym->type())
            if (auto* uty = dynamic_cast<UnionSemaType*>(sym->type()))
                complete_union(uty, node);

        for (auto* method : node.methods())
            method->accept(*this);
    }

    void TypeChecker::visit(const ast::EnumDecl& node)
    {
        auto it = m_resolutions.find(&node);
        Symbol* sym = (it != m_resolutions.end()) ? it->second : nullptr;

        if (sym && sym->type())
        {
            if (auto* ety = dynamic_cast<EnumSemaType*>(sym->type()))
            {
                if (node.underlying_type())
                {
                    auto* ut = eval_type(*node.underlying_type());
                    if (!ut->is_error() && !ut->is_integer())
                        error(node.underlying_type()->range(), "enum underlying type must be an integer type");
                }
                complete_enum(ety, node);
            }
        }

        for (auto* method : node.methods())
            method->accept(*this);
    }

    void TypeChecker::visit(const ast::EnumVariantDecl&) {}
    void TypeChecker::visit(const ast::ModuleDecl&) {}
    void TypeChecker::visit(const ast::ImportDecl&) {}

    void TypeChecker::visit(const ast::UsingDecl& node)
    {
        auto* aliased = eval_type(*node.aliased_type());
        record_type(&node, aliased);
    }

    void TypeChecker::visit(const ast::TranslationUnit& node)
    {
        for (auto* decl : node.decls())
            decl->accept(*this);
    }

    void TypeChecker::visit(const ast::LiteralPattern& node)
    {
        auto* lit_type = materialize_type(check_expr(*node.literal()));
        if (m_match_scrutinee_type && !m_match_scrutinee_type->is_error())
        {
            if (!m_types.is_implicitly_convertible(lit_type, m_match_scrutinee_type) && !m_types.types_equal(lit_type, m_match_scrutinee_type))
            {
                error(node.range(),
                      std::format("pattern type '{}' is incompatible with scrutinee type '{}'", lit_type->to_string(), m_match_scrutinee_type->to_string()));
            }
        }
    }

    void TypeChecker::visit(const ast::BindingPattern& node)
    {
        auto it = m_resolutions.find(&node);
        if (it != m_resolutions.end() && it->second && m_match_scrutinee_type)
            it->second->set_type(m_match_scrutinee_type);

        if (node.guard())
        {
            auto* gt = check_expr(*node.guard());
            if (!gt->is_error() && !gt->is_bool())
                error(node.guard()->range(), "pattern guard must be 'bool'");
        }
    }

    void TypeChecker::visit(const ast::WildcardPattern&) {}

    void TypeChecker::visit(const ast::EnumPattern& node)
    {
        if (!m_match_scrutinee_type || !m_match_scrutinee_type->is_enum())
        {
            if (m_match_scrutinee_type && !m_match_scrutinee_type->is_error())
                error(node.range(), std::format("enum pattern used with non-enum scrutinee type '{}'", m_match_scrutinee_type->to_string()));

            return;
        }

        auto* ety = static_cast<EnumSemaType*>(m_match_scrutinee_type);
        if (!node.path().empty())
        {
            auto* vi = ety->find_variant(node.path().back());
            if (!vi)
            {
                error(node.range(), std::format("enum '{}' has no variant '{}'", ety->to_string(), std::string{node.path().back().view()}));
                return;
            }

            if (node.sub_patterns().size() != vi->payload_types.size())
            {
                error(node.range(), std::format("variant '{}' expects {} payload {}, found {}", std::string{vi->name.view()}, vi->payload_types.size(),
                                                vi->payload_types.size() == 1 ? "" : "s", node.sub_patterns().size()));

                return;
            }

            for (std::size_t i = 0; i < node.sub_patterns().size(); ++i)
                check_pattern(*node.sub_patterns()[i], vi->payload_types[i]);
        }
    }

    void TypeChecker::visit(const ast::StructPattern& node)
    {
        if (!m_match_scrutinee_type || m_match_scrutinee_type->is_error())
            return;

        auto* sty = dynamic_cast<StructSemaType*>(m_match_scrutinee_type);
        if (!sty)
        {
            error(node.range(), std::format("struct pattern used with non-struct scrutinee type '{}'", m_match_scrutinee_type->to_string()));
            return;
        }

        for (auto& field : node.fields())
        {
            auto* fi = sty->find_field(field.name);
            if (!fi)
            {
                error(field.range, std::format("struct '{}' has no field '{}'", sty->to_string(), std::string{field.name.view()}));
                continue;
            }

            check_pattern(*field.binding, fi->type);
        }

        if (!node.has_rest())
        {
            for (auto& fi : sty->fields())
            {
                bool found = false;
                for (auto& pf : node.fields())
                {
                    if (pf.name == fi.name)
                    {
                        found = true;
                        break;
                    }
                }

                if (!found)
                    warning(node.range(), std::format("struct pattern does not cover field '{}'", std::string{fi.name.view()}));
            }
        }
    }

    void TypeChecker::visit(const ast::RestPattern&) {}

    void TypeChecker::visit(const ast::AmbiguousExpr& node)
    {
        auto it = m_disambiguations.find(&node);
        if (it != m_disambiguations.end())
        {
            auto* chosen = static_cast<ast::Expr*>(it->second);
            record_type(&node, check_expr(*chosen));
            return;
        }

        error(node.range(), "internal error: unresolved ambiguous expression reached type checker");
        record_type(&node, m_types.error_type());
    }

    void TypeChecker::visit(const ast::AmbiguousStmt& node)
    {
        auto it = m_disambiguations.find(&node);
        if (it != m_disambiguations.end())
        {
            it->second->accept(*this);
            return;
        }

        error(node.range(), "internal error: unresolved ambiguous statement reached type checker");
    }

    void TypeChecker::visit(const ast::AmbiguousDecl& node)
    {
        auto it = m_disambiguations.find(&node);
        if (it != m_disambiguations.end())
        {
            it->second->accept(*this);
            return;
        }

        error(node.range(), "internal error: unresolved ambiguous declaration reached type checker");
    }

} // namespace dcc::sema
