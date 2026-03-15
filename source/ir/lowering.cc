#include <ast/ambiguous.hh>
#include <ast/common.hh>
#include <ast/decl.hh>
#include <ast/expr.hh>
#include <ast/pattern.hh>
#include <ast/stmt.hh>
#include <ast/type.hh>
#include <cassert>
#include <format>
#include <ir/lowering.hh>
#include <ir/mangle.hh>
#include <sema/types.hh>

namespace dcc::ir
{
    IRLowering::IRLowering(sema::Sema& sema, diag::DiagnosticPrinter& printer) : m_sema{sema}, m_types{sema.types()}, m_printer{printer}, m_mono{}
    {
        m_mono.set_emitter([this](const ast::FunctionDecl& decl, std::span<sema::SemaType* const> type_args, const std::string& mangled) {
            auto saved_subst = std::move(m_mono_subst);
            m_mono_subst.clear();

            for (std::size_t i = 0; i < decl.template_params().size() && i < type_args.size(); ++i)
            {
                auto* sym = m_sema.symbol_of(decl.template_params()[i]);
                if (sym && sym->type())
                    m_mono_subst[sym->type()] = type_args[i];
            }

            lower_function(decl, mangled);
            m_mono_subst = std::move(saved_subst);
        });
    }

    std::unique_ptr<Module> IRLowering::lower(ast::TranslationUnit& tu)
    {
        m_module = std::make_unique<Module>();
        m_module->name = "main";

        tu.accept(*this);

        m_mono.flush();

        return std::move(m_module);
    }

    LowerScope* IRLowering::push_scope()
    {
        auto s = std::make_unique<LowerScope>();
        s->parent = m_scope;
        auto* raw = s.get();
        m_scope_pool.push_back(std::move(s));
        m_scope = raw;
        return raw;
    }

    void IRLowering::pop_scope()
    {
        assert(m_scope);
        emit_scope_defers(*m_scope);
        m_scope = m_scope->parent;
    }

    void IRLowering::declare_local(const ast::Decl* decl, ValueId slot)
    {
        assert(m_scope);
        m_scope->locals[decl] = slot;
    }

    ValueId IRLowering::lookup_local(const ast::Decl* decl) const
    {
        for (auto* s = m_scope; s; s = s->parent)
            if (auto it = s->locals.find(decl); it != s->locals.end())
                return it->second;

        return kNoValue;
    }

    void IRLowering::emit_scope_defers(LowerScope& scope)
    {
        for (auto it = scope.defers.rbegin(); it != scope.defers.rend(); ++it)
            lower_stmt(**it);
    }

    void IRLowering::emit_defers_to(LowerScope* stop_at)
    {
        for (auto* s = m_scope; s && s != stop_at; s = s->parent)
            emit_scope_defers(*s);
    }

    bool IRLowering::evaluate_static_condition(const ast::Expr& cond)
    {
        if (auto* bl = dynamic_cast<const ast::BoolLiteral*>(&cond))
            return bl->value();

        error(cond.range(), "static if condition must be a compile-time constant");
        return false;
    }

    const sema::SemaType* IRLowering::sema_type_of(const ast::Node* n) const
    {
        return m_sema.type_of(n);
    }

    sema::Symbol* IRLowering::symbol_of(const ast::Node* n) const
    {
        return m_sema.symbol_of(n);
    }

    std::string IRLowering::mangle(const ast::FunctionDecl& fn)
    {
        if (!fn.should_mangle())
            return std::string{fn.name().view()};

        auto* sty = sema_type_of(&fn);
        if (!sty)
            return std::string{fn.name().view()};

        auto* fty = dynamic_cast<const sema::FunctionSemaType*>(sty);
        if (!fty)
            return std::string{fn.name().view()};

        std::vector<sema::SemaType*> param_types;
        param_types.reserve(fty->param_types().size());
        for (auto* pt : fty->param_types())
        {
            auto* resolved = m_types.resolve(const_cast<sema::SemaType*>(pt));
            if (resolved->is_type_var())
                return std::string{fn.name().view()};

            param_types.push_back(resolved);
        }

        return Mangler::mangle_function(m_module_path, fn.name().view(), param_types);
    }

    void IRLowering::error(sm::SourceRange range, std::string msg)
    {
        m_printer.emit(diag::error(std::move(msg)).with_primary(range, ""));
    }

    void IRLowering::visit(const ast::TranslationUnit& tu)
    {
        if (auto* mod = tu.module_decl())
        {
            for (auto seg : mod->path())
                m_module_path.push_back(seg);

            m_module->name = std::string{mod->path().back().view()};
        }

        for (auto* d : tu.decls())
        {
            if (auto* fn = dynamic_cast<const ast::FunctionDecl*>(d))
            {
                if (!fn->template_params().empty())
                    continue;

                auto mangled = mangle(*fn);
                auto* fty_sema = dynamic_cast<const sema::FunctionSemaType*>(sema_type_of(fn));
                if (!fty_sema)
                    continue;

                auto* fty_ir = static_cast<FunctionType*>(const_cast<Type*>(lower_function_type(fty_sema)));

                auto linkage = fn->is_extern() ? Linkage::ExternDecl : fn->visibility() == ast::Visibility::Public ? Linkage::External : Linkage::Internal;

                m_module->add_function(mangled, fty_ir, linkage);
            }
            else if (auto* var = dynamic_cast<const ast::VarDecl*>(d))
                lower_global_var(*var);
        }

        for (auto* d : tu.decls())
            if (auto* fn = dynamic_cast<const ast::FunctionDecl*>(d))
                if (fn->template_params().empty() && fn->body())
                    lower_function(*fn, mangle(*fn));
    }

    void IRLowering::lower_function(const ast::FunctionDecl& fn, std::string mangled_name)
    {
        auto* ir_func = m_module->find_function(mangled_name);
        if (!ir_func)
        {
            auto* fty_sema = dynamic_cast<const sema::FunctionSemaType*>(sema_type_of(&fn));
            if (!fty_sema)
                return;

            auto* fty_ir = static_cast<FunctionType*>(const_cast<Type*>(lower_function_type(fty_sema)));

            auto linkage = fn.is_extern() ? Linkage::ExternDecl : fn.visibility() == ast::Visibility::Public ? Linkage::External : Linkage::Internal;

            ir_func = m_module->add_function(mangled_name, fty_ir, linkage);
        }

        if (!fn.body())
            return;

        m_current_func = ir_func;
        auto* entry = ir_func->add_block("entry");
        m_builder_storage = std::make_unique<IRBuilder>(*ir_func, m_module->types, entry);
        m_builder = m_builder_storage.get();

        push_scope();

        for (std::size_t i = 0; i < fn.params().size(); ++i)
        {
            auto* param = fn.params()[i];
            auto* param_sty = sema_type_of(param);
            auto param_ir = lower_type(param_sty);
            auto ptr_ty = m_module->types.pointer_to(param_ir);

            auto slot = m_builder->build_alloca(param_ir, ptr_ty);
            auto val = ir_func->next_value();

            ir_func->params.push_back({
                .name = std::string{param->name().view()},
                .type = param_ir,
                .value = val,
            });

            m_builder->build_store(slot, val);
            declare_local(param, slot);
        }

        auto* fty_sema = dynamic_cast<const sema::FunctionSemaType*>(sema_type_of(&fn));
        auto ret_ir = fty_sema ? lower_type(fty_sema->return_type()) : m_module->types.void_type();
        bool returns_void = ret_ir->is_void();

        m_return_block = ir_func->add_block("return");

        if (!returns_void)
        {
            auto ret_ptr_ty = m_module->types.pointer_to(ret_ir);
            m_return_slot = m_builder->build_alloca(ret_ir, ret_ptr_ty);
        }

        fn.body()->accept(*this);

        if (m_builder->current_block() && !m_builder->current_block()->is_terminated())
            m_builder->build_branch(m_return_block);

        m_builder->set_block(m_return_block);
        emit_defers_to(nullptr);

        if (returns_void)
            m_builder->build_return_void();
        else
        {
            auto ret_val = m_builder->build_load(m_return_slot, ret_ir);
            m_builder->build_return(ret_val);
        }

        pop_scope();
        m_current_func = nullptr;
        m_builder = nullptr;
        m_builder_storage.reset();
        m_return_block = nullptr;
        m_return_slot = kNoValue;
    }

    void IRLowering::lower_var(const ast::VarDecl& var)
    {
        auto* sty = sema_type_of(&var);
        auto ir_ty = lower_type(sty);
        auto ptr_ty = m_module->types.pointer_to(ir_ty);
        auto slot = m_builder->build_alloca(ir_ty, ptr_ty);

        declare_local(&var, slot);

        if (var.init())
        {
            auto val = lower_expr_rvalue(*var.init());
            m_builder->build_store(slot, val);
        }
    }

    void IRLowering::lower_global_var(const ast::VarDecl& var)
    {
        auto* sty = sema_type_of(&var);
        auto ir_ty = lower_type(sty);
        auto linkage = var.storage_class() == ast::StorageClass::Extern   ? Linkage::ExternDecl
                       : var.storage_class() == ast::StorageClass::Static ? Linkage::Internal
                                                                          : Linkage::External;

        Global g{
            .name = std::string{var.name().view()},
            .type = ir_ty,
            .linkage = linkage,
            .is_const = ast::has_qualifier(var.quals(), ast::Qualifier::Const),
            .init = std::nullopt,
        };

        if (var.init())
        {
            if (auto* il = dynamic_cast<const ast::IntegerLiteral*>(var.init()))
                g.init = static_cast<int64_t>(il->value().as_int_value());
            else if (auto* bl = dynamic_cast<const ast::BoolLiteral*>(var.init()))
                g.init = bl->value();
        }

        m_module->add_global(std::move(g));
    }

    void IRLowering::lower_stmt(const ast::Stmt& stmt)
    {
        stmt.accept(*this);
    }

    void IRLowering::lower_decl(const ast::Decl& decl)
    {
        decl.accept(*this);
    }

    void IRLowering::visit(const ast::ExprStmt& s)
    {
        if (s.expr())
            lower_expr(*s.expr());
    }

    void IRLowering::visit(const ast::DeclStmt& s)
    {
        s.decl()->accept(*this);
    }

    void IRLowering::visit(const ast::EmptyStmt&) {}

    void IRLowering::visit(const ast::BlockStmt& s)
    {
        push_scope();
        for (auto* stmt : s.stmts())
            stmt->accept(*this);
        pop_scope();
    }

    void IRLowering::visit(const ast::ReturnStmt& s)
    {
        if (s.value())
        {
            auto val = lower_expr_rvalue(*s.value());
            m_builder->build_store(m_return_slot, val);
        }
        emit_defers_to(nullptr);
        m_builder->build_branch(m_return_block);

        auto* dead = m_current_func->add_block("post_return");
        m_builder->set_block(dead);
    }

    void IRLowering::visit(const ast::IfStmt& s)
    {
        if (s.is_static())
        {
            bool cond = evaluate_static_condition(*s.condition());
            if (cond)
                s.then_branch()->accept(*this);
            else if (s.else_branch())
                s.else_branch()->accept(*this);
            return;
        }

        auto cond = lower_expr_rvalue(*s.condition());

        auto* then_bb = m_current_func->add_block("if.then");
        auto* else_bb = s.else_branch() ? m_current_func->add_block("if.else") : nullptr;
        auto* merge_bb = m_current_func->add_block("if.merge");

        m_builder->build_cond_branch(cond, then_bb, else_bb ? else_bb : merge_bb);

        m_builder->set_block(then_bb);
        s.then_branch()->accept(*this);
        if (!m_builder->current_block()->is_terminated())
            m_builder->build_branch(merge_bb);

        if (else_bb)
        {
            m_builder->set_block(else_bb);
            s.else_branch()->accept(*this);
            if (!m_builder->current_block()->is_terminated())
                m_builder->build_branch(merge_bb);
        }

        m_builder->set_block(merge_bb);
    }

    void IRLowering::visit(const ast::WhileStmt& s)
    {
        auto* cond_bb = m_current_func->add_block("while.cond");
        auto* body_bb = m_current_func->add_block("while.body");
        auto* exit_bb = m_current_func->add_block("while.exit");

        m_loop_stack.push_back({.brk = exit_bb, .cont = cond_bb});

        m_builder->build_branch(cond_bb);
        m_builder->set_block(cond_bb);
        auto cv = lower_expr_rvalue(*s.condition());
        m_builder->build_cond_branch(cv, body_bb, exit_bb);

        m_builder->set_block(body_bb);
        push_scope();
        s.body()->accept(*this);
        pop_scope();
        if (!m_builder->current_block()->is_terminated())
            m_builder->build_branch(cond_bb);

        m_builder->set_block(exit_bb);
        m_loop_stack.pop_back();
    }

    void IRLowering::visit(const ast::ForStmt& s)
    {
        push_scope();

        if (s.init())
            s.init()->accept(*this);

        auto* cond_bb = m_current_func->add_block("for.cond");
        auto* body_bb = m_current_func->add_block("for.body");
        auto* incr_bb = m_current_func->add_block("for.incr");
        auto* exit_bb = m_current_func->add_block("for.exit");

        m_loop_stack.push_back({.brk = exit_bb, .cont = incr_bb});

        m_builder->build_branch(cond_bb);
        m_builder->set_block(cond_bb);

        if (s.condition())
        {
            auto cv = lower_expr_rvalue(*s.condition());
            m_builder->build_cond_branch(cv, body_bb, exit_bb);
        }
        else
            m_builder->build_branch(body_bb);

        m_builder->set_block(body_bb);
        s.body()->accept(*this);
        if (!m_builder->current_block()->is_terminated())
            m_builder->build_branch(incr_bb);

        m_builder->set_block(incr_bb);
        if (s.increment())
            lower_expr(*s.increment());
        m_builder->build_branch(cond_bb);

        m_builder->set_block(exit_bb);
        m_loop_stack.pop_back();
        pop_scope();
    }

    void IRLowering::visit(const ast::DoWhileStmt& s)
    {
        auto* body_bb = m_current_func->add_block("do.body");
        auto* cond_bb = m_current_func->add_block("do.cond");
        auto* exit_bb = m_current_func->add_block("do.exit");

        m_loop_stack.push_back({.brk = exit_bb, .cont = cond_bb});

        m_builder->build_branch(body_bb);
        m_builder->set_block(body_bb);
        push_scope();
        s.body()->accept(*this);
        pop_scope();
        if (!m_builder->current_block()->is_terminated())
            m_builder->build_branch(cond_bb);

        m_builder->set_block(cond_bb);
        auto cv = lower_expr_rvalue(*s.condition());
        m_builder->build_cond_branch(cv, body_bb, exit_bb);

        m_builder->set_block(exit_bb);
        m_loop_stack.pop_back();
    }

    void IRLowering::visit(const ast::BreakStmt&)
    {
        assert(!m_loop_stack.empty());
        emit_defers_to(nullptr);
        m_builder->build_branch(m_loop_stack.back().brk);

        auto* dead = m_current_func->add_block("post_break");
        m_builder->set_block(dead);
    }

    void IRLowering::visit(const ast::ContinueStmt&)
    {
        assert(!m_loop_stack.empty());
        emit_defers_to(nullptr);
        m_builder->build_branch(m_loop_stack.back().cont);

        auto* dead = m_current_func->add_block("post_continue");
        m_builder->set_block(dead);
    }

    void IRLowering::visit(const ast::DeferStmt& s)
    {
        assert(m_scope);
        m_scope->defers.push_back(s.body());
    }

    void IRLowering::visit(const ast::MatchStmt& s)
    {
        lower_match_stmt(s);
    }

    void IRLowering::lower_match_stmt(const ast::MatchStmt& ms)
    {
        auto scrutinee = lower_expr_rvalue(*ms.scrutinee());
        auto* scr_sema = sema_type_of(ms.scrutinee());
        auto scr_ir = lower_type(scr_sema);

        auto* merge_bb = m_current_func->add_block("match.merge");

        lower_match_arms(scrutinee, scr_ir, scr_sema, ms.arms(), merge_bb, kNoValue);

        m_builder->set_block(merge_bb);
    }

    void IRLowering::visit(const ast::VarDecl& v)
    {
        lower_var(v);
    }

    void IRLowering::visit(const ast::FunctionDecl& fn)
    {
        if (fn.template_params().empty() && fn.body())
            lower_function(fn, mangle(fn));
    }

    void IRLowering::visit(const ast::StructDecl&) {}
    void IRLowering::visit(const ast::UnionDecl&) {}
    void IRLowering::visit(const ast::EnumDecl&) {}
    void IRLowering::visit(const ast::EnumVariantDecl&) {}
    void IRLowering::visit(const ast::ModuleDecl&) {}
    void IRLowering::visit(const ast::ImportDecl&) {}
    void IRLowering::visit(const ast::UsingDecl&) {}
    void IRLowering::visit(const ast::FieldDecl&) {}
    void IRLowering::visit(const ast::ParamDecl&) {}
    void IRLowering::visit(const ast::TemplateTypeParamDecl&) {}
    void IRLowering::visit(const ast::TemplateValueParamDecl&) {}

    void IRLowering::visit(const ast::AmbiguousExpr& a)
    {
        auto* chosen = m_sema.disambiguated(&a);
        if (chosen)
        {
            chosen->accept(*this);
            return;
        }

        error(a.range(), "internal: ambiguous expr reached lowering");
    }

    void IRLowering::visit(const ast::AmbiguousStmt& a)
    {
        auto* chosen = m_sema.disambiguated(&a);
        if (chosen)
        {
            chosen->accept(*this);
            return;
        }

        error(a.range(), "internal: ambiguous stmt reached lowering");
    }

    void IRLowering::visit(const ast::AmbiguousDecl& a)
    {
        auto* chosen = m_sema.disambiguated(&a);
        if (chosen)
        {
            chosen->accept(*this);
            return;
        }

        error(a.range(), "internal: ambiguous decl reached lowering");
    }

    void IRLowering::visit(const ast::BuiltinType&) {}
    void IRLowering::visit(const ast::NamedType&) {}
    void IRLowering::visit(const ast::QualifiedType&) {}
    void IRLowering::visit(const ast::DottedNamedType&) {}
    void IRLowering::visit(const ast::PointerType&) {}
    void IRLowering::visit(const ast::SliceType&) {}
    void IRLowering::visit(const ast::ArrayType&) {}
    void IRLowering::visit(const ast::FlexibleArrayType&) {}
    void IRLowering::visit(const ast::FunctionType&) {}
    void IRLowering::visit(const ast::TemplateType&) {}
    void IRLowering::visit(const ast::TypeofType&) {}

    void IRLowering::visit(const ast::LiteralPattern&) {}
    void IRLowering::visit(const ast::BindingPattern&) {}
    void IRLowering::visit(const ast::WildcardPattern&) {}
    void IRLowering::visit(const ast::EnumPattern&) {}
    void IRLowering::visit(const ast::StructPattern&) {}
    void IRLowering::visit(const ast::RestPattern&) {}

    ExprResult IRLowering::lower_expr(const ast::Expr& expr)
    {
        expr.accept(*this);
        return m_expr_result;
    }

    ValueId IRLowering::lower_expr_rvalue(const ast::Expr& expr)
    {
        return load_if_lvalue(lower_expr(expr));
    }

    ValueId IRLowering::load_if_lvalue(ExprResult r)
    {
        if (r.is_lvalue)
            return m_builder->build_load(r.value, r.type);

        return r.value;
    }

    void IRLowering::visit(const ast::IntegerLiteral& lit)
    {
        auto* sty = sema_type_of(&lit);
        auto ir = lower_type(sty);

        if (ir->is_integer())
        {
            auto* ity = static_cast<const IntegerType*>(ir);
            if (ity->is_signed)
                m_expr_result = ExprResult::rvalue(m_builder->build_const_int(lit.value().as_int_value(), ir), ir);
            else
                m_expr_result = ExprResult::rvalue(m_builder->build_const_uint(lit.value().as_int_value(), ir), ir);
        }
        else
            m_expr_result = ExprResult::rvalue(m_builder->build_const_int(lit.value().as_int_value(), ir), ir);
    }

    void IRLowering::visit(const ast::FloatLiteral& lit)
    {
        auto* sty = sema_type_of(&lit);
        auto ir = lower_type(sty);
        m_expr_result = ExprResult::rvalue(m_builder->build_const_float(lit.value().as_float_value(), ir), ir);
    }

    void IRLowering::visit(const ast::StringLiteral& lit)
    {
        auto* u8_ty = m_module->types.integer_type(8, false);
        auto sv = lit.value().view();
        auto* arr_ty = m_module->types.array_of(u8_ty, sv.size() + 1);

        auto gname = std::format(".str.{}", m_module->globals.size());
        Global g{.name = gname, .type = arr_ty, .linkage = Linkage::Internal, .is_const = true, .init = {}};
        m_module->add_global(std::move(g));

        auto* ptr_ty = m_module->types.pointer_to(u8_ty);
        auto val = m_builder->build_const_uint(0, ptr_ty);
        m_expr_result = ExprResult::rvalue(val, ptr_ty);
    }

    void IRLowering::visit(const ast::CharLiteral& lit)
    {
        auto* ir = m_module->types.integer_type(8, false);
        m_expr_result = ExprResult::rvalue(m_builder->build_const_uint(static_cast<uint64_t>(lit.value().as_int_value()), ir), ir);
    }

    void IRLowering::visit(const ast::BoolLiteral& lit)
    {
        m_expr_result = ExprResult::rvalue(m_builder->build_const_bool(lit.value()), m_module->types.bool_type());
    }

    void IRLowering::visit(const ast::NullLiteral&)
    {
        auto* ptr_ty = m_module->types.pointer_to(m_module->types.void_type());
        m_expr_result = ExprResult::rvalue(m_builder->build_const_uint(0, ptr_ty), ptr_ty);
    }

    void IRLowering::visit(const ast::IdentifierExpr& id)
    {
        auto* sym = symbol_of(&id);
        if (!sym)
        {
            error(id.range(), "unresolved identifier in lowering");
            return;
        }

        auto* decl = sym->decl();
        auto slot = lookup_local(decl);

        if (slot != kNoValue)
        {
            auto* sty = sema_type_of(&id);
            auto ir = lower_type(sty);
            m_expr_result = ExprResult::lvalue(slot, ir);
        }
        else
        {
            auto* sty = sema_type_of(&id);
            auto ir = lower_type(sty);
            m_expr_result = ExprResult::rvalue(kNoValue, ir);
        }
    }

    void IRLowering::declare_pattern_local(const ast::Pattern* pat, ValueId slot)
    {
        assert(m_scope);
        m_scope->pattern_bindings[pat] = slot;
    }

    ValueId IRLowering::lookup_pattern_local(const ast::Pattern* pat) const
    {
        for (auto* s = m_scope; s; s = s->parent)
            if (auto it = s->pattern_bindings.find(pat); it != s->pattern_bindings.end())
                return it->second;

        return kNoValue;
    }

    void IRLowering::visit(const ast::GroupingExpr& g)
    {
        m_expr_result = lower_expr(*g.inner());
    }

    void IRLowering::visit(const ast::BinaryExpr& bin)
    {
        lower_binary_op(bin);
    }

    void IRLowering::lower_binary_op(const ast::BinaryExpr& bin)
    {
        if (bin.op() == ast::BinaryOp::LogAnd || bin.op() == ast::BinaryOp::LogOr)
        {
            lower_short_circuit(bin.op(), *bin.lhs(), *bin.rhs(), bin.range());
            return;
        }

        auto lv = lower_expr_rvalue(*bin.lhs());
        auto rv = lower_expr_rvalue(*bin.rhs());
        auto* res_sema = sema_type_of(&bin);
        auto res_ir = lower_type(res_sema);

        Opcode opc;
        switch (bin.op())
        {
            case ast::BinaryOp::Add:
                opc = Opcode::Add;
                break;
            case ast::BinaryOp::Sub:
                opc = Opcode::Sub;
                break;
            case ast::BinaryOp::Mul:
                opc = Opcode::Mul;
                break;
            case ast::BinaryOp::Div:
                opc = Opcode::Div;
                break;
            case ast::BinaryOp::Mod:
                opc = Opcode::Mod;
                break;
            case ast::BinaryOp::BitAnd:
                opc = Opcode::BitAnd;
                break;
            case ast::BinaryOp::BitOr:
                opc = Opcode::BitOr;
                break;
            case ast::BinaryOp::BitXor:
                opc = Opcode::BitXor;
                break;
            case ast::BinaryOp::Shl:
                opc = Opcode::Shl;
                break;
            case ast::BinaryOp::Shr:
                opc = Opcode::Shr;
                break;
            case ast::BinaryOp::Eq:
                opc = Opcode::Eq;
                break;
            case ast::BinaryOp::Ne:
                opc = Opcode::Ne;
                break;
            case ast::BinaryOp::Lt:
                opc = Opcode::Lt;
                break;
            case ast::BinaryOp::Le:
                opc = Opcode::Le;
                break;
            case ast::BinaryOp::Gt:
                opc = Opcode::Gt;
                break;
            case ast::BinaryOp::Ge:
                opc = Opcode::Ge;
                break;
            default:
                return;
        }

        auto val = emit_binop(opc, lv, rv, res_ir);
        m_expr_result = ExprResult::rvalue(val, res_ir);
    }

    ValueId IRLowering::emit_binop(Opcode opc, ValueId lhs, ValueId rhs, TypeRef type)
    {
        switch (opc)
        {
            case Opcode::Add:
                return m_builder->build_add(lhs, rhs, type);
            case Opcode::Sub:
                return m_builder->build_sub(lhs, rhs, type);
            case Opcode::Mul:
                return m_builder->build_mul(lhs, rhs, type);
            case Opcode::Div:
                return m_builder->build_div(lhs, rhs, type);
            case Opcode::Mod:
                return m_builder->build_mod(lhs, rhs, type);
            case Opcode::BitAnd:
                return m_builder->build_bit_and(lhs, rhs, type);
            case Opcode::BitOr:
                return m_builder->build_bit_or(lhs, rhs, type);
            case Opcode::BitXor:
                return m_builder->build_bit_xor(lhs, rhs, type);
            case Opcode::Shl:
                return m_builder->build_shl(lhs, rhs, type);
            case Opcode::Shr:
                return m_builder->build_shr(lhs, rhs, type);
            case Opcode::Eq:
                return m_builder->build_eq(lhs, rhs, type);
            case Opcode::Ne:
                return m_builder->build_ne(lhs, rhs, type);
            case Opcode::Lt:
                return m_builder->build_lt(lhs, rhs, type);
            case Opcode::Le:
                return m_builder->build_le(lhs, rhs, type);
            case Opcode::Gt:
                return m_builder->build_gt(lhs, rhs, type);
            case Opcode::Ge:
                return m_builder->build_ge(lhs, rhs, type);
            default:
                return kNoValue;
        }
    }

    void IRLowering::lower_short_circuit(ast::BinaryOp op, const ast::Expr& lhs, const ast::Expr& rhs, sm::SourceRange)
    {
        auto lv = lower_expr_rvalue(lhs);
        auto* rhs_bb = m_current_func->add_block(op == ast::BinaryOp::LogAnd ? "and.rhs" : "or.rhs");
        auto* merge_bb = m_current_func->add_block("sc.merge");

        auto* entry_bb = m_builder->current_block();

        if (op == ast::BinaryOp::LogAnd)
            m_builder->build_cond_branch(lv, rhs_bb, merge_bb);
        else
            m_builder->build_cond_branch(lv, merge_bb, rhs_bb);

        m_builder->set_block(rhs_bb);
        auto rv = lower_expr_rvalue(rhs);
        auto* rhs_exit_bb = m_builder->current_block();
        m_builder->build_branch(merge_bb);

        m_builder->set_block(merge_bb);

        Inst phi;
        phi.opcode = Opcode::Phi;
        phi.type = m_module->types.bool_type();
        phi.payload = Inst::PhiData{.entries = {{entry_bb, lv}, {rhs_exit_bb, rv}}};
        phi.dst = m_current_func->next_value();
        m_builder->current_block()->append(std::move(phi));
        auto phi_val = m_builder->current_block()->insts.back().dst;

        m_expr_result = ExprResult::rvalue(phi_val, m_module->types.bool_type());
    }

    void IRLowering::visit(const ast::UnaryExpr& un)
    {
        lower_unary_op(un);
    }

    void IRLowering::lower_unary_op(const ast::UnaryExpr& un)
    {
        auto* res_sema = sema_type_of(&un);
        auto res_ir = lower_type(res_sema);

        switch (un.op())
        {
            case ast::UnaryOp::Negate: {
                auto v = lower_expr_rvalue(*un.operand());
                m_expr_result = ExprResult::rvalue(m_builder->build_neg(v, res_ir), res_ir);
                break;
            }
            case ast::UnaryOp::BitNot: {
                auto v = lower_expr_rvalue(*un.operand());
                m_expr_result = ExprResult::rvalue(m_builder->build_bit_not(v, res_ir), res_ir);
                break;
            }
            case ast::UnaryOp::LogNot: {
                auto v = lower_expr_rvalue(*un.operand());
                m_expr_result = ExprResult::rvalue(m_builder->build_log_not(v), m_module->types.bool_type());
                break;
            }
            case ast::UnaryOp::Deref: {
                auto v = lower_expr_rvalue(*un.operand());
                m_expr_result = ExprResult::lvalue(v, res_ir);
                break;
            }
            case ast::UnaryOp::AddressOf: {
                auto r = lower_expr(*un.operand());
                assert(r.is_lvalue && "address-of requires lvalue");
                m_expr_result = ExprResult::rvalue(r.value, res_ir);
                break;
            }
            case ast::UnaryOp::PreInc:
            case ast::UnaryOp::PreDec: {
                auto r = lower_expr(*un.operand());
                assert(r.is_lvalue);
                auto cur = m_builder->build_load(r.value, r.type);
                auto one = m_builder->build_const_int(1, r.type);
                auto nv = (un.op() == ast::UnaryOp::PreInc) ? m_builder->build_add(cur, one, r.type) : m_builder->build_sub(cur, one, r.type);
                m_builder->build_store(r.value, nv);
                m_expr_result = ExprResult::rvalue(nv, r.type);
                break;
            }
            case ast::UnaryOp::PostInc:
            case ast::UnaryOp::PostDec: {
                auto r = lower_expr(*un.operand());
                assert(r.is_lvalue);
                auto cur = m_builder->build_load(r.value, r.type);
                auto one = m_builder->build_const_int(1, r.type);
                auto nv = (un.op() == ast::UnaryOp::PostInc) ? m_builder->build_add(cur, one, r.type) : m_builder->build_sub(cur, one, r.type);
                m_builder->build_store(r.value, nv);
                m_expr_result = ExprResult::rvalue(cur, r.type);
                break;
            }
        }
    }

    void IRLowering::visit(const ast::AssignExpr& a)
    {
        lower_assign(a);
    }

    void IRLowering::lower_assign(const ast::AssignExpr& a)
    {
        auto target = lower_expr(*a.target());
        assert(target.is_lvalue && "assignment target must be lvalue");

        auto val = lower_expr_rvalue(*a.value());

        if (a.op() != ast::AssignOp::Simple)
        {
            auto cur = m_builder->build_load(target.value, target.type);
            Opcode opc;
            switch (a.op())
            {
                case ast::AssignOp::Add:
                    opc = Opcode::Add;
                    break;
                case ast::AssignOp::Sub:
                    opc = Opcode::Sub;
                    break;
                case ast::AssignOp::Mul:
                    opc = Opcode::Mul;
                    break;
                case ast::AssignOp::Div:
                    opc = Opcode::Div;
                    break;
                case ast::AssignOp::Mod:
                    opc = Opcode::Mod;
                    break;
                case ast::AssignOp::BitAnd:
                    opc = Opcode::BitAnd;
                    break;
                case ast::AssignOp::BitOr:
                    opc = Opcode::BitOr;
                    break;
                case ast::AssignOp::BitXor:
                    opc = Opcode::BitXor;
                    break;
                case ast::AssignOp::Shl:
                    opc = Opcode::Shl;
                    break;
                case ast::AssignOp::Shr:
                    opc = Opcode::Shr;
                    break;
                default:
                    return;
            }
            val = emit_binop(opc, cur, val, target.type);
        }

        m_builder->build_store(target.value, val);
        m_expr_result = ExprResult::rvalue(val, target.type);
    }

    void IRLowering::visit(const ast::ConditionalExpr& c)
    {
        lower_conditional(c);
    }

    void IRLowering::lower_conditional(const ast::ConditionalExpr& c)
    {
        auto cv = lower_expr_rvalue(*c.condition());

        auto* then_bb = m_current_func->add_block("cond.then");
        auto* else_bb = m_current_func->add_block("cond.else");
        auto* merge_bb = m_current_func->add_block("cond.merge");

        m_builder->build_cond_branch(cv, then_bb, else_bb);

        m_builder->set_block(then_bb);
        auto tv = lower_expr_rvalue(*c.then_expr());
        auto* then_exit = m_builder->current_block();
        m_builder->build_branch(merge_bb);

        m_builder->set_block(else_bb);
        auto ev = lower_expr_rvalue(*c.else_expr());
        auto* else_exit = m_builder->current_block();
        m_builder->build_branch(merge_bb);

        m_builder->set_block(merge_bb);
        auto* res_sema = sema_type_of(&c);
        auto res_ir = lower_type(res_sema);

        Inst phi;
        phi.opcode = Opcode::Phi;
        phi.type = res_ir;
        phi.payload = Inst::PhiData{.entries = {{then_exit, tv}, {else_exit, ev}}};
        phi.dst = m_current_func->next_value();
        m_builder->current_block()->append(std::move(phi));

        m_expr_result = ExprResult::rvalue(m_builder->current_block()->insts.back().dst, res_ir);
    }

    void IRLowering::visit(const ast::CastExpr& c)
    {
        m_expr_result = lower_cast(c);
    }

    ExprResult IRLowering::lower_cast(const ast::CastExpr& c)
    {
        auto val = lower_expr_rvalue(*c.operand());
        auto* from_sema = sema_type_of(c.operand());
        auto* to_sema = sema_type_of(&c);
        auto to_ir = lower_type(to_sema);
        auto casted = emit_cast(val, from_sema, to_sema);

        return ExprResult::rvalue(casted, to_ir);
    }

    ValueId IRLowering::emit_cast(ValueId val, const sema::SemaType* from, const sema::SemaType* to)
    {
        auto from_ir = lower_type(from);
        auto to_ir = lower_type(to);

        if (from_ir == to_ir)
            return val;

        bool from_int = from_ir->is_integer() || from_ir->is_bool();
        bool from_flt = from_ir->is_float();
        bool from_ptr = from_ir->is_pointer();
        bool to_int = to_ir->is_integer() || to_ir->is_bool();
        bool to_flt = to_ir->is_float();
        bool to_ptr = to_ir->is_pointer();

        if (from_int && to_int)
            return m_builder->build_int_to_int(val, from_ir, to_ir);
        if (from_int && to_flt)
            return m_builder->build_int_to_float(val, from_ir, to_ir);
        if (from_flt && to_int)
            return m_builder->build_float_to_int(val, from_ir, to_ir);
        if (from_flt && to_flt)
            return m_builder->build_float_to_float(val, from_ir, to_ir);
        if (from_int && to_ptr)
            return m_builder->build_int_to_ptr(val, to_ir);
        if (from_ptr && to_int)
            return m_builder->build_ptr_to_int(val, to_ir);
        if (from_ptr && to_ptr)
            return m_builder->build_bitcast(val, to_ir);

        return m_builder->build_bitcast(val, to_ir);
    }

    void IRLowering::visit(const ast::MemberAccessExpr& ma)
    {
        m_expr_result = lower_member_access(ma);
    }

    ValueId IRLowering::deref_to_struct_ptr(ValueId val, TypeRef, const sema::SemaType* sema_ty)
    {
        while (sema_ty->is_pointer())
        {
            auto* ptr_sty = static_cast<const sema::PointerSemaType*>(sema_ty);
            sema_ty = ptr_sty->pointee();
            auto pointee_ir = lower_type(sema_ty);
            val = m_builder->build_load(val, m_module->types.pointer_to(pointee_ir));
        }
        return val;
    }

    ExprResult IRLowering::lower_member_access(const ast::MemberAccessExpr& ma)
    {
        auto obj = lower_expr(*ma.object());
        auto* obj_sema = sema_type_of(ma.object());

        ValueId base_ptr = obj.value;
        const sema::SemaType* base_sema = obj_sema;

        if (obj.is_lvalue)
        {
            if (base_sema->is_pointer())
            {
                auto obj_ir = lower_type(base_sema);
                base_ptr = m_builder->build_load(base_ptr, obj_ir);
                auto* ptr_sty = static_cast<const sema::PointerSemaType*>(base_sema);
                base_sema = ptr_sty->pointee();

                while (base_sema->is_pointer())
                {
                    auto inner_ir = lower_type(base_sema);
                    base_ptr = m_builder->build_load(base_ptr, inner_ir);
                    auto* inner_ptr = static_cast<const sema::PointerSemaType*>(base_sema);
                    base_sema = inner_ptr->pointee();
                }
            }
        }
        else
        {
            if (base_sema->is_pointer())
            {
                auto* ptr_sty = static_cast<const sema::PointerSemaType*>(base_sema);
                base_sema = ptr_sty->pointee();
                base_ptr = obj.value;

                while (base_sema->is_pointer())
                {
                    auto inner_ir = lower_type(base_sema);
                    base_ptr = m_builder->build_load(base_ptr, inner_ir);
                    auto* inner_ptr = static_cast<const sema::PointerSemaType*>(base_sema);
                    base_sema = inner_ptr->pointee();
                }
            }
            else
            {
                auto ir_ty = lower_type(base_sema);
                auto ptr_ty = m_module->types.pointer_to(ir_ty);
                auto slot = m_builder->build_alloca(ir_ty, ptr_ty);
                m_builder->build_store(slot, base_ptr);
                base_ptr = slot;
            }
        }

        if (base_sema->kind() == sema::SemaType::Kind::Struct)
        {
            auto* ssty = static_cast<const sema::StructSemaType*>(base_sema);
            auto fields = ssty->fields();
            for (uint32_t i = 0; i < fields.size(); ++i)
            {
                if (fields[i].name == ma.member())
                {
                    auto* field_ir = lower_type(fields[i].type);
                    auto ptr_ty = m_module->types.pointer_to(field_ir);
                    auto gfp = m_builder->build_get_field_ptr(base_ptr, i, ptr_ty);
                    return ExprResult::lvalue(gfp, field_ir);
                }
            }
        }
        else if (base_sema->kind() == sema::SemaType::Kind::Union)
        {
            auto* usty = static_cast<const sema::UnionSemaType*>(base_sema);
            auto fields = usty->fields();
            for (uint32_t i = 0; i < fields.size(); ++i)
            {
                if (fields[i].name == ma.member())
                {
                    auto* field_ir = lower_type(fields[i].type);
                    auto ptr_ty = m_module->types.pointer_to(field_ir);
                    auto data_ptr = m_builder->build_get_field_ptr(base_ptr, 0, ptr_ty);
                    auto casted = m_builder->build_bitcast(data_ptr, ptr_ty);
                    return ExprResult::lvalue(casted, field_ir);
                }
            }
        }
        else if (base_sema->is_slice())
        {
            auto* slice_sema = static_cast<const sema::SliceSemaType*>(base_sema);
            if (ma.member().view() == "ptr")
            {
                auto* elem_ir = lower_type(slice_sema->element());
                auto* fld_ty = m_module->types.pointer_to(elem_ir);
                auto ptr_ty = m_module->types.pointer_to(fld_ty);
                auto gfp = m_builder->build_get_field_ptr(base_ptr, 0, ptr_ty);
                return ExprResult::lvalue(gfp, fld_ty);
            }
            else if (ma.member().view() == "len")
            {
                auto* len_ty = m_module->types.integer_type(64, false);
                auto ptr_ty = m_module->types.pointer_to(len_ty);
                auto gfp = m_builder->build_get_field_ptr(base_ptr, 1, ptr_ty);
                return ExprResult::lvalue(gfp, len_ty);
            }
        }

        error(ma.range(), std::format("no field '{}' on type", std::string{ma.member().view()}));
        return ExprResult::rvalue(kNoValue, m_module->types.void_type());
    }

    void IRLowering::visit(const ast::CallExpr& c)
    {
        m_expr_result = lower_call(c);
    }

    ExprResult IRLowering::lower_call(const ast::CallExpr& call)
    {
        if (m_sema.is_ufcs_call(&call))
        {
            auto* ufcs_sym = m_sema.ufcs_target(&call);
            if (auto* ma = dynamic_cast<const ast::MemberAccessExpr*>(call.callee()))
                return lower_ufcs_call(call, *ma, ufcs_sym);
        }

        if (!call.template_args().empty())
        {
            if (auto* callee_id = dynamic_cast<const ast::IdentifierExpr*>(call.callee()))
            {
                auto* sym = symbol_of(callee_id);
                if (sym && sym->decl())
                {
                    if (auto* fn_decl = dynamic_cast<const ast::FunctionDecl*>(sym->decl()))
                    {
                        std::vector<sema::SemaType*> type_args;
                        for (auto& ta : call.template_args())
                        {
                            if (auto* const* te = std::get_if<ast::TypeExpr*>(&ta.arg))
                            {
                                auto* resolved = m_sema.resolve_type_expr(*te);
                                if (resolved)
                                {
                                    resolved = m_types.resolve(const_cast<sema::SemaType*>(resolved));
                                    if (auto it = m_mono_subst.find(resolved); it != m_mono_subst.end())
                                        resolved = it->second;
                                }
                                if (!resolved || resolved->is_type_var())
                                {
                                    for (auto* a : call.args())
                                        lower_expr_rvalue(*a);

                                    auto* ret_sema = sema_type_of(&call);
                                    auto ret_ir = lower_type(ret_sema);
                                    return ExprResult::rvalue(kNoValue, ret_ir);
                                }

                                type_args.push_back(resolved);
                            }
                        }

                        auto mangled = m_mono.request(*fn_decl, type_args, m_module_path);

                        std::vector<ValueId> args;
                        for (auto* a : call.args())
                            args.push_back(lower_expr_rvalue(*a));

                        auto* ret_sema = sema_type_of(&call);
                        auto ret_ir = lower_type(ret_sema);
                        auto* callee_func = m_module->find_function(mangled);

                        if (ret_ir->is_void())
                        {
                            m_builder->build_call_void(mangled, callee_func, args);
                            return ExprResult::rvalue(kNoValue, ret_ir);
                        }
                        auto rv = m_builder->build_call(mangled, callee_func, args, ret_ir);
                        return ExprResult::rvalue(rv, ret_ir);
                    }
                }
            }
        }

        if (auto* ma = dynamic_cast<const ast::MemberAccessExpr*>(call.callee()))
        {
            auto* obj_sema = sema_type_of(ma->object());
            const sema::SemaType* base_sema = obj_sema;
            if (base_sema && base_sema->is_pointer())
                base_sema = static_cast<const sema::PointerSemaType*>(base_sema)->pointee();

            bool found_member = false;
            if (base_sema)
            {
                if (auto* sty = dynamic_cast<const sema::StructSemaType*>(base_sema))
                    found_member = sty->find_method(ma->member()) || sty->find_field(ma->member());
                else if (auto* uty = dynamic_cast<const sema::UnionSemaType*>(base_sema))
                    found_member = uty->find_method(ma->member()) || uty->find_field(ma->member());
                else if (auto* ety = dynamic_cast<const sema::EnumSemaType*>(base_sema))
                    found_member = ety->find_method(ma->member());
            }

            if (!found_member)
            {
                auto member_sv = ma->member().view();
                auto self_val = lower_expr_rvalue(*ma->object());

                for (auto& fn : m_module->functions)
                {
                    if (fn->name.find(member_sv) != std::string::npos && !fn->type->param_types.empty())
                    {
                        std::vector<ValueId> args;
                        args.push_back(self_val);
                        for (auto* a : call.args())
                            args.push_back(lower_expr_rvalue(*a));

                        auto* ret_sema = sema_type_of(&call);
                        auto ret_ir = lower_type(ret_sema);

                        if (ret_ir->is_void())
                        {
                            m_builder->build_call_void(fn->name, fn.get(), args);
                            return ExprResult::rvalue(kNoValue, ret_ir);
                        }
                        auto rv = m_builder->build_call(fn->name, fn.get(), args, ret_ir);
                        return ExprResult::rvalue(rv, ret_ir);
                    }
                }
            }
        }

        std::string callee_name;
        Function* callee_func = nullptr;

        if (auto* callee_id = dynamic_cast<const ast::IdentifierExpr*>(call.callee()))
        {
            auto* sym = symbol_of(callee_id);
            if (sym && sym->decl())
            {
                if (auto* fn_decl = dynamic_cast<const ast::FunctionDecl*>(sym->decl()))
                {
                    callee_name = mangle(*fn_decl);
                    callee_func = m_module->find_function(callee_name);
                }
            }
            if (callee_name.empty())
                callee_name = std::string{callee_id->name().view()};
        }

        std::vector<ValueId> args;
        for (auto* a : call.args())
            args.push_back(lower_expr_rvalue(*a));

        auto* ret_sema = sema_type_of(&call);
        auto ret_ir = lower_type(ret_sema);

        if (ret_ir->is_void())
        {
            m_builder->build_call_void(callee_name, callee_func, args);
            return ExprResult::rvalue(kNoValue, ret_ir);
        }

        auto rv = m_builder->build_call(callee_name, callee_func, args, ret_ir);
        return ExprResult::rvalue(rv, ret_ir);
    }

    ExprResult IRLowering::lower_ufcs_call(const ast::CallExpr& call, const ast::MemberAccessExpr& ma, sema::Symbol* target)
    {
        auto self_val = lower_expr_rvalue(*ma.object());

        std::vector<ValueId> args;
        args.push_back(self_val);
        for (auto* a : call.args())
            args.push_back(lower_expr_rvalue(*a));

        auto* fn_decl = dynamic_cast<const ast::FunctionDecl*>(target->decl());
        std::string callee_name = fn_decl ? mangle(*fn_decl) : std::string{ma.member().view()};
        auto* callee_func = m_module->find_function(callee_name);

        auto* ret_sema = sema_type_of(&call);
        auto ret_ir = lower_type(ret_sema);

        if (ret_ir->is_void())
        {
            m_builder->build_call_void(callee_name, callee_func, args);
            return ExprResult::rvalue(kNoValue, ret_ir);
        }

        auto rv = m_builder->build_call(callee_name, callee_func, args, ret_ir);
        return ExprResult::rvalue(rv, ret_ir);
    }

    void IRLowering::visit(const ast::IndexExpr& idx)
    {
        m_expr_result = lower_index(idx);
    }

    ExprResult IRLowering::lower_index(const ast::IndexExpr& idx)
    {
        auto obj = lower_expr(*idx.object());
        auto index_val = lower_expr_rvalue(*idx.index());

        auto* obj_sema = sema_type_of(idx.object());
        auto* res_sema = sema_type_of(&idx);
        auto res_ir = lower_type(res_sema);
        auto ptr_ty = m_module->types.pointer_to(res_ir);

        ValueId base_ptr = obj.is_lvalue ? obj.value : obj.value;

        if (obj_sema->is_slice())
        {
            if (!obj.is_lvalue)
            {
                auto ir_ty = lower_type(obj_sema);
                auto sp = m_module->types.pointer_to(ir_ty);
                auto slot = m_builder->build_alloca(ir_ty, sp);
                m_builder->build_store(slot, base_ptr);
                base_ptr = slot;
            }

            auto data_ptr_ty = m_module->types.pointer_to(ptr_ty);
            auto data_ptr = m_builder->build_get_field_ptr(base_ptr, 0, data_ptr_ty);
            base_ptr = m_builder->build_load(data_ptr, ptr_ty);
        }

        auto gep = m_builder->build_get_element_ptr(base_ptr, index_val, ptr_ty);
        return ExprResult::lvalue(gep, res_ir);
    }

    void IRLowering::visit(const ast::SliceExpr& sl)
    {
        m_expr_result = lower_slice(sl);
    }

    ExprResult IRLowering::lower_slice(const ast::SliceExpr& sl)
    {
        auto obj = lower_expr(*sl.object());
        auto begin_val = sl.begin_idx() ? lower_expr_rvalue(*sl.begin_idx()) : m_builder->build_const_uint(0, m_module->types.integer_type(64, false));
        auto end_val = lower_expr_rvalue(*sl.end_idx());

        auto* res_sema = sema_type_of(&sl);
        auto res_ir = lower_type(res_sema);
        auto ptr_ty = m_module->types.pointer_to(res_ir);

        auto slot = m_builder->build_alloca(res_ir, ptr_ty);

        auto* elem_sema = static_cast<const sema::SliceSemaType*>(res_sema)->element();
        auto elem_ir = lower_type(elem_sema);
        auto elem_ptr = m_module->types.pointer_to(elem_ir);

        ValueId base = obj.is_lvalue ? obj.value : obj.value;
        auto new_ptr = m_builder->build_get_element_ptr(base, begin_val, elem_ptr);

        auto* u64 = m_module->types.integer_type(64, false);
        auto len = m_builder->build_sub(end_val, begin_val, u64);

        auto ptr_field_ptr = m_builder->build_get_field_ptr(slot, 0, m_module->types.pointer_to(elem_ptr));
        m_builder->build_store(ptr_field_ptr, new_ptr);

        auto len_field_ptr = m_builder->build_get_field_ptr(slot, 1, m_module->types.pointer_to(u64));
        m_builder->build_store(len_field_ptr, len);

        auto result = m_builder->build_load(slot, res_ir);
        return ExprResult::rvalue(result, res_ir);
    }

    void IRLowering::visit(const ast::InitializerExpr& init)
    {
        m_expr_result = lower_initializer(init);
    }

    ExprResult IRLowering::lower_initializer(const ast::InitializerExpr& init)
    {
        auto* sty = sema_type_of(&init);
        auto ir = lower_type(sty);
        auto ptr = m_module->types.pointer_to(ir);
        auto slot = m_builder->build_alloca(ir, ptr);

        if (sty->kind() == sema::SemaType::Kind::Struct)
        {
            auto* ssty = static_cast<const sema::StructSemaType*>(sty);
            for (auto& fi : init.fields())
            {
                auto fields = ssty->fields();
                for (uint32_t i = 0; i < fields.size(); ++i)
                {
                    if (fields[i].name == fi.name)
                    {
                        auto val = lower_expr_rvalue(*fi.value);
                        auto* fty = lower_type(fields[i].type);
                        auto fp = m_module->types.pointer_to(fty);
                        auto gfp = m_builder->build_get_field_ptr(slot, i, fp);
                        m_builder->build_store(gfp, val);
                        break;
                    }
                }
            }
        }

        auto result = m_builder->build_load(slot, ir);
        return ExprResult::rvalue(result, ir);
    }

    void IRLowering::visit(const ast::BlockExpr& b)
    {
        m_expr_result = lower_block_expr(b);
    }

    ExprResult IRLowering::lower_block_expr(const ast::BlockExpr& b)
    {
        push_scope();
        for (auto* s : b.stmts())
            s->accept(*this);

        ExprResult result;
        if (b.tail())
            result = ExprResult::rvalue(lower_expr_rvalue(*b.tail()), lower_type(sema_type_of(b.tail())));
        else
            result = ExprResult::rvalue(kNoValue, m_module->types.void_type());

        pop_scope();
        return result;
    }

    void IRLowering::visit(const ast::MatchExpr& m)
    {
        m_expr_result = lower_match_expr(m);
    }

    ExprResult IRLowering::lower_match_expr(const ast::MatchExpr& match)
    {
        auto scrutinee = lower_expr_rvalue(*match.scrutinee());
        auto* scr_sema = sema_type_of(match.scrutinee());
        auto scr_ir = lower_type(scr_sema);
        auto* res_sema = sema_type_of(&match);
        auto res_ir = lower_type(res_sema);

        auto ptr_ty = m_module->types.pointer_to(res_ir);
        auto result_slot = res_ir->is_void() ? kNoValue : m_builder->build_alloca(res_ir, ptr_ty);
        auto* merge_bb = m_current_func->add_block("match.merge");

        lower_match_arms(scrutinee, scr_ir, scr_sema, match.arms(), merge_bb, result_slot);

        m_builder->set_block(merge_bb);

        if (res_ir->is_void())
            return ExprResult::rvalue(kNoValue, res_ir);

        auto val = m_builder->build_load(result_slot, res_ir);
        return ExprResult::rvalue(val, res_ir);
    }

    void IRLowering::lower_match_arms(ValueId scrutinee_val, TypeRef scr_ir, const sema::SemaType* scr_sema, std::span<const ast::MatchArm> arms,
                                      BasicBlock* merge_bb, ValueId result_slot)
    {
        for (std::size_t i = 0; i < arms.size(); ++i)
        {
            auto& arm = arms[i];
            auto* test_bb = m_current_func->add_block(std::format("match.arm{}.test", i));
            auto* body_bb = m_current_func->add_block(std::format("match.arm{}.body", i));
            auto* next_bb = (i + 1 < arms.size()) ? m_current_func->add_block(std::format("match.arm{}.next", i)) : merge_bb;

            if (!m_builder->current_block()->is_terminated())
                m_builder->build_branch(test_bb);

            m_builder->set_block(test_bb);
            lower_pattern_test(*arm.pattern, scrutinee_val, scr_ir, scr_sema, body_bb, next_bb);

            m_builder->set_block(body_bb);
            push_scope();
            bind_pattern(*arm.pattern, scrutinee_val, scr_ir, scr_sema);

            if (arm.guard)
            {
                auto gv = lower_expr_rvalue(*arm.guard);
                auto* guarded_bb = m_current_func->add_block(std::format("match.arm{}.guarded", i));
                m_builder->build_cond_branch(gv, guarded_bb, next_bb);
                m_builder->set_block(guarded_bb);
            }

            if (auto* expr = dynamic_cast<const ast::Expr*>(arm.body))
            {
                auto val = lower_expr_rvalue(*expr);
                if (result_slot != kNoValue)
                    m_builder->build_store(result_slot, val);
            }
            else if (auto* stmt = dynamic_cast<const ast::Stmt*>(arm.body))
                stmt->accept(*this);

            pop_scope();
            if (!m_builder->current_block()->is_terminated())
                m_builder->build_branch(merge_bb);

            m_builder->set_block(next_bb);
        }
    }

    void IRLowering::lower_pattern_test(const ast::Pattern& pat, ValueId scrutinee, TypeRef scr_ty, const sema::SemaType* sema_ty, BasicBlock* match_bb,
                                        BasicBlock* fail_bb)
    {
        if (dynamic_cast<const ast::WildcardPattern*>(&pat) || dynamic_cast<const ast::BindingPattern*>(&pat) || dynamic_cast<const ast::RestPattern*>(&pat))
        {
            m_builder->build_branch(match_bb);
            return;
        }

        if (auto* lp = dynamic_cast<const ast::LiteralPattern*>(&pat))
        {
            auto lit_val = lower_expr_rvalue(*lp->literal());
            auto cmp = m_builder->build_eq(scrutinee, lit_val, scr_ty);
            m_builder->build_cond_branch(cmp, match_bb, fail_bb);
            return;
        }

        if (auto* ep = dynamic_cast<const ast::EnumPattern*>(&pat))
        {
            if (sema_ty->is_enum())
            {
                auto* ety = static_cast<const sema::EnumSemaType*>(sema_ty);

                auto ptr_ty = m_module->types.pointer_to(scr_ty);
                auto slot = m_builder->build_alloca(scr_ty, ptr_ty);
                m_builder->build_store(slot, scrutinee);

                auto* tag_ty = lower_type(ety->underlying_type());
                auto tag_ptr_ty = m_module->types.pointer_to(tag_ty);
                auto tag_ptr = m_builder->build_get_field_ptr(slot, 0, tag_ptr_ty);
                auto tag_val = m_builder->build_load(tag_ptr, tag_ty);

                auto variant_name = ep->path().back();
                for (auto& v : ety->variants())
                {
                    if (v.name == variant_name)
                    {
                        auto disc = m_builder->build_const_int(v.discriminant, tag_ty);
                        auto cmp = m_builder->build_eq(tag_val, disc, tag_ty);
                        m_builder->build_cond_branch(cmp, match_bb, fail_bb);
                        return;
                    }
                }
            }

            m_builder->build_branch(fail_bb);
            return;
        }

        if (dynamic_cast<const ast::StructPattern*>(&pat))
        {
            m_builder->build_branch(match_bb);
            return;
        }

        m_builder->build_branch(match_bb);
    }

    void IRLowering::bind_pattern(const ast::Pattern& pat, ValueId scrutinee, TypeRef scr_ty, const sema::SemaType* sema_ty)
    {
        if (dynamic_cast<const ast::BindingPattern*>(&pat))
        {
            auto ptr_ty = m_module->types.pointer_to(scr_ty);
            auto slot = m_builder->build_alloca(scr_ty, ptr_ty);
            m_builder->build_store(slot, scrutinee);

            declare_pattern_local(&pat, slot);
            return;
        }

        if (auto* ep = dynamic_cast<const ast::EnumPattern*>(&pat))
        {
            if (sema_ty->is_enum())
                return;

            auto* ety = static_cast<const sema::EnumSemaType*>(sema_ty);
            auto variant_name = ep->path().back();

            for (auto& v : ety->variants())
            {
                if (v.name != variant_name)
                    continue;
                if (v.payload_types.empty() || ep->sub_patterns().empty())
                    break;

                auto ptr_ty = m_module->types.pointer_to(scr_ty);
                auto slot = m_builder->build_alloca(scr_ty, ptr_ty);
                m_builder->build_store(slot, scrutinee);

                auto* u8_ty = m_module->types.integer_type(8, false);
                auto* payload_ptr_ty = m_module->types.pointer_to(u8_ty);
                auto payload_ptr = m_builder->build_get_field_ptr(slot, 1, payload_ptr_ty);

                uint64_t offset = 0;
                for (std::size_t i = 0; i < ep->sub_patterns().size() && i < v.payload_types.size(); ++i)
                {
                    auto* field_sty = v.payload_types[i];
                    auto field_ir = lower_type(field_sty);
                    auto field_ptr = m_module->types.pointer_to(field_ir);

                    uint64_t a = field_ir->align_bytes();
                    offset = ((offset + a - 1) & ~(a - 1));

                    auto off_val = m_builder->build_const_uint(offset, m_module->types.integer_type(64, false));
                    auto elem_ptr = m_builder->build_get_element_ptr(payload_ptr, off_val, field_ptr);
                    auto casted = m_builder->build_bitcast(elem_ptr, field_ptr);

                    auto field_val = m_builder->build_load(casted, field_ir);
                    bind_pattern(*ep->sub_patterns()[i], field_val, field_ir, field_sty);

                    offset += field_ir->size_bytes();
                }
                break;
            }
            return;
        }

        if (auto* sp = dynamic_cast<const ast::StructPattern*>(&pat))
        {
            if (sema_ty->kind() != sema::SemaType::Kind::Struct)
                return;
            auto* ssty = static_cast<const sema::StructSemaType*>(sema_ty);

            auto ptr_ty = m_module->types.pointer_to(scr_ty);
            auto slot = m_builder->build_alloca(scr_ty, ptr_ty);
            m_builder->build_store(slot, scrutinee);

            for (auto& field_pat : sp->fields())
            {
                auto fields = ssty->fields();
                for (uint32_t i = 0; i < fields.size(); ++i)
                {
                    if (fields[i].name == field_pat.name)
                    {
                        auto* fty = lower_type(fields[i].type);
                        auto fp = m_module->types.pointer_to(fty);
                        auto gfp = m_builder->build_get_field_ptr(slot, i, fp);
                        auto fv = m_builder->build_load(gfp, fty);
                        bind_pattern(*field_pat.binding, fv, fty, fields[i].type);
                        break;
                    }
                }
            }
            return;
        }
    }

    void IRLowering::visit(const ast::SizeofExpr& s)
    {
        auto* resolved_sty = m_sema.resolve_type_expr(s.operand());
        auto ir_ty = lower_type(resolved_sty);
        auto* u64 = m_module->types.integer_type(64, false);
        auto val = m_builder->build_const_uint(ir_ty->size_bytes(), u64);
        m_expr_result = ExprResult::rvalue(val, u64);
    }

    void IRLowering::visit(const ast::AlignofExpr& a)
    {
        auto* resolved_sty = m_sema.resolve_type_expr(a.operand());
        auto ir_ty = lower_type(resolved_sty);
        auto* u64 = m_module->types.integer_type(64, false);
        auto val = m_builder->build_const_uint(ir_ty->align_bytes(), u64);
        m_expr_result = ExprResult::rvalue(val, u64);
    }

    void IRLowering::visit(const ast::MacroCallExpr& mc)
    {
        error(mc.range(), "macro calls should be expanded before lowering");
        m_expr_result = ExprResult::rvalue(kNoValue, m_module->types.void_type());
    }

} // namespace dcc::ir
