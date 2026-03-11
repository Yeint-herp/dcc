#include <algorithm>
#include <cassert>
#include <format>
#include <ir/ir.hh>

namespace dcc::ir
{
    static uint64_t align_to(uint64_t offset, uint64_t alignment)
    {
        assert(alignment > 0);
        return (offset + alignment - 1) & ~(alignment - 1);
    }

    void StructType::compute_layout()
    {
        uint64_t offset = 0;
        uint64_t max_align = 1;

        for (auto& field : fields)
        {
            uint64_t fa = field.type->align_bytes();
            offset = align_to(offset, fa);
            field.offset = static_cast<uint32_t>(offset);
            offset += field.type->size_bytes();
            max_align = std::max(max_align, fa);
        }

        size = align_to(offset, max_align);
        align = max_align;
    }

    std::string FunctionType::to_string() const
    {
        std::string s = "fn(";
        for (std::size_t i = 0; i < param_types.size(); ++i)
        {
            if (i > 0)
                s += ", ";

            s += param_types[i]->to_string();
        }
        if (is_variadic)
        {
            if (!param_types.empty())
                s += ", ";

            s += "...";
        }
        s += ") -> ";
        s += return_type->to_string();
        return s;
    }

    TypeArena::TypeArena()
        : m_integers{
              ir::IntegerType{8, true},  ir::IntegerType{8, false},  ir::IntegerType{16, true}, ir::IntegerType{16, false},
              ir::IntegerType{32, true}, ir::IntegerType{32, false}, ir::IntegerType{64, true}, ir::IntegerType{64, false},
          }
    {
    }

    TypeRef TypeArena::integer_type(uint8_t width, bool is_signed)
    {
        int idx;
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
                assert(false && "unsupported integer width");
                return nullptr;
        }
        if (!is_signed)
            idx += 1;

        return &m_integers[idx];
    }

    TypeRef TypeArena::float_type(uint8_t width)
    {
        switch (width)
        {
            case 32:
                return &m_f32;
            case 64:
                return &m_f64;
            default:
                assert(false && "unsupported float width");
                return nullptr;
        }
    }

    std::size_t TypeArena::PtrHash::operator()(const PtrKey& k) const noexcept
    {
        return std::hash<const void*>{}(k.pointee);
    }

    std::size_t TypeArena::ArrHash::operator()(const ArrKey& k) const noexcept
    {
        auto h1 = std::hash<const void*>{}(k.element);
        auto h2 = std::hash<uint64_t>{}(k.length);

        return h1 ^ (h2 * 2654435761u);
    }

    TypeRef TypeArena::pointer_to(TypeRef pointee)
    {
        PtrKey key{pointee};
        auto it = m_pointer_cache.find(key);
        if (it != m_pointer_cache.end())
            return it->second;

        auto ptr = std::make_unique<PointerType>(pointee);
        TypeRef raw = ptr.get();
        m_arena.push_back(std::move(ptr));
        m_pointer_cache[key] = raw;
        return raw;
    }

    TypeRef TypeArena::array_of(TypeRef element, uint64_t length)
    {
        ArrKey key{element, length};
        auto it = m_array_cache.find(key);
        if (it != m_array_cache.end())
            return it->second;

        auto arr = std::make_unique<ArrayType>(element, length);
        TypeRef raw = arr.get();
        m_arena.push_back(std::move(arr));
        m_array_cache[key] = raw;
        return raw;
    }

    StructType* TypeArena::make_struct(std::string name)
    {
        auto sty = std::make_unique<StructType>(std::move(name));
        auto* raw = sty.get();
        m_arena.push_back(std::move(sty));

        return raw;
    }

    FunctionType* TypeArena::make_function(TypeRef ret, std::vector<TypeRef> params, bool variadic)
    {
        auto fty = std::make_unique<FunctionType>(ret, std::move(params), variadic);
        auto* raw = fty.get();
        m_arena.push_back(std::move(fty));

        return raw;
    }

    StructType* TypeArena::make_slice_type(TypeRef element)
    {
        auto* sty = make_struct("__slice_" + element->to_string());

        sty->fields.push_back({
            .name = "ptr",
            .type = pointer_to(element),
            .offset = 0,
        });
        sty->fields.push_back({
            .name = "len",
            .type = integer_type(64, false),
            .offset = 0,
        });

        sty->compute_layout();
        return sty;
    }

    StructType* TypeArena::make_enum_type(std::string name, TypeRef tag_type, uint64_t payload_size, uint64_t payload_align)
    {
        auto* sty = make_struct(std::move(name));

        sty->fields.push_back({
            .name = "tag",
            .type = tag_type,
            .offset = 0,
        });

        if (payload_size > 0)
        {
            auto* payload_type = array_of(integer_type(8, false), payload_size);

            sty->fields.push_back({
                .name = "payload",
                .type = payload_type,
                .offset = 0,
            });
        }

        uint64_t offset = tag_type->size_bytes();
        uint64_t max_align = tag_type->align_bytes();

        if (payload_size > 0)
        {
            max_align = std::max(max_align, payload_align);
            offset = align_to(offset, payload_align);
            sty->fields[1].offset = static_cast<uint32_t>(offset);
            offset += payload_size;
        }

        sty->size = align_to(offset, max_align);
        sty->align = max_align;

        return sty;
    }

    BasicBlock* Function::add_block(std::string label)
    {
        auto bb = std::make_unique<BasicBlock>(std::move(label));
        bb->parent = this;
        auto* raw = bb.get();
        blocks.push_back(std::move(bb));

        return raw;
    }

    Function* Module::add_function(std::string fname, FunctionType* ftype, Linkage linkage)
    {
        auto func = std::make_unique<Function>();
        func->name = std::move(fname);
        func->type = ftype;
        func->linkage = linkage;

        auto* raw = func.get();
        functions.push_back(std::move(func));
        return raw;
    }

    Function* Module::find_function(std::string_view fname) const
    {
        for (auto& f : functions)
            if (f->name == fname)
                return f.get();

        return nullptr;
    }

    void Module::add_global(Global g)
    {
        globals.push_back(std::move(g));
    }

    IRBuilder::IRBuilder(Function& func, TypeArena& types) : m_func{func}, m_types{types}, m_block{func.entry()} {}

    IRBuilder::IRBuilder(Function& func, TypeArena& types, BasicBlock* block) : m_func{func}, m_types{types}, m_block{block} {}

    ValueId IRBuilder::emit(Inst inst)
    {
        assert(m_block && "no current block");
        inst.dst = m_func.next_value();
        m_block->append(std::move(inst));
        return m_block->insts.back().dst;
    }

    void IRBuilder::emit_void(Inst inst)
    {
        assert(m_block && "no current block");
        inst.dst = kNoValue;
        m_block->append(std::move(inst));
    }

    ValueId IRBuilder::build_const_int(int64_t value, TypeRef type)
    {
        Inst inst;
        inst.opcode = Opcode::Const;
        inst.type = type;
        inst.payload = Inst::ConstData{.value = value};
        return emit(std::move(inst));
    }

    ValueId IRBuilder::build_const_uint(uint64_t value, TypeRef type)
    {
        Inst inst;
        inst.opcode = Opcode::Const;
        inst.type = type;
        inst.payload = Inst::ConstData{.value = value};
        return emit(std::move(inst));
    }

    ValueId IRBuilder::build_const_float(double value, TypeRef type)
    {
        Inst inst;
        inst.opcode = Opcode::Const;
        inst.type = type;
        inst.payload = Inst::ConstData{.value = value};
        return emit(std::move(inst));
    }

    ValueId IRBuilder::build_const_bool(bool value)
    {
        Inst inst;
        inst.opcode = Opcode::Const;
        inst.type = m_types.bool_type();
        inst.payload = Inst::ConstData{.value = value};
        return emit(std::move(inst));
    }

    ValueId IRBuilder::build_alloca(TypeRef alloc_type, TypeRef result_ptr_type, uint32_t count)
    {
        Inst inst;
        inst.opcode = Opcode::Alloca;
        inst.type = result_ptr_type;
        inst.payload = Inst::AllocaData{.alloc_type = alloc_type, .count = count};
        return emit(std::move(inst));
    }

    ValueId IRBuilder::build_load(ValueId ptr, TypeRef result_type)
    {
        Inst inst;
        inst.opcode = Opcode::Load;
        inst.type = result_type;
        inst.operands = {ptr};
        return emit(std::move(inst));
    }

    void IRBuilder::build_store(ValueId ptr, ValueId value)
    {
        Inst inst;
        inst.opcode = Opcode::Store;
        inst.operands = {ptr, value};
        emit_void(std::move(inst));
    }

#define DEFINE_BINOP(method_name, opcode_value)                                                                                                                \
    ValueId IRBuilder::method_name(ValueId lhs, ValueId rhs, TypeRef type)                                                                                     \
    {                                                                                                                                                          \
        Inst inst;                                                                                                                                             \
        inst.opcode = Opcode::opcode_value;                                                                                                                    \
        inst.type = type;                                                                                                                                      \
        inst.operands = {lhs, rhs};                                                                                                                            \
        return emit(std::move(inst));                                                                                                                          \
    }

    DEFINE_BINOP(build_add, Add)
    DEFINE_BINOP(build_sub, Sub)
    DEFINE_BINOP(build_mul, Mul)
    DEFINE_BINOP(build_div, Div)
    DEFINE_BINOP(build_mod, Mod)
    DEFINE_BINOP(build_bit_and, BitAnd)
    DEFINE_BINOP(build_bit_or, BitOr)
    DEFINE_BINOP(build_bit_xor, BitXor)
    DEFINE_BINOP(build_shl, Shl)
    DEFINE_BINOP(build_shr, Shr)

#undef DEFINE_BINOP

#define DEFINE_CMP(method_name, opcode_value)                                                                                                                  \
    ValueId IRBuilder::method_name(ValueId lhs, ValueId rhs, TypeRef)                                                                                          \
    {                                                                                                                                                          \
        Inst inst;                                                                                                                                             \
        inst.opcode = Opcode::opcode_value;                                                                                                                    \
        inst.type = m_types.bool_type();                                                                                                                       \
        inst.operands = {lhs, rhs};                                                                                                                            \
        return emit(std::move(inst));                                                                                                                          \
    }

    DEFINE_CMP(build_eq, Eq)
    DEFINE_CMP(build_ne, Ne)
    DEFINE_CMP(build_lt, Lt)
    DEFINE_CMP(build_le, Le)
    DEFINE_CMP(build_gt, Gt)
    DEFINE_CMP(build_ge, Ge)

#undef DEFINE_CMP

    ValueId IRBuilder::build_log_and(ValueId lhs, ValueId rhs)
    {
        Inst inst;
        inst.opcode = Opcode::LogAnd;
        inst.type = m_types.bool_type();
        inst.operands = {lhs, rhs};
        return emit(std::move(inst));
    }

    ValueId IRBuilder::build_log_or(ValueId lhs, ValueId rhs)
    {
        Inst inst;
        inst.opcode = Opcode::LogOr;
        inst.type = m_types.bool_type();
        inst.operands = {lhs, rhs};
        return emit(std::move(inst));
    }

    ValueId IRBuilder::build_neg(ValueId operand, TypeRef type)
    {
        Inst inst;
        inst.opcode = Opcode::Neg;
        inst.type = type;
        inst.operands = {operand};
        return emit(std::move(inst));
    }

    ValueId IRBuilder::build_bit_not(ValueId operand, TypeRef type)
    {
        Inst inst;
        inst.opcode = Opcode::BitNot;
        inst.type = type;
        inst.operands = {operand};
        return emit(std::move(inst));
    }

    ValueId IRBuilder::build_log_not(ValueId operand)
    {
        Inst inst;
        inst.opcode = Opcode::LogNot;
        inst.type = m_types.bool_type();
        inst.operands = {operand};
        return emit(std::move(inst));
    }

    ValueId IRBuilder::build_int_to_int(ValueId operand, TypeRef from, TypeRef to)
    {
        Inst inst;
        inst.opcode = Opcode::IntToInt;
        inst.type = to;
        inst.operands = {operand};
        inst.payload = Inst::CastData{.from_type = from};
        return emit(std::move(inst));
    }

    ValueId IRBuilder::build_int_to_float(ValueId operand, TypeRef from, TypeRef to)
    {
        Inst inst;
        inst.opcode = Opcode::IntToFloat;
        inst.type = to;
        inst.operands = {operand};
        inst.payload = Inst::CastData{.from_type = from};
        return emit(std::move(inst));
    }

    ValueId IRBuilder::build_float_to_int(ValueId operand, TypeRef from, TypeRef to)
    {
        Inst inst;
        inst.opcode = Opcode::FloatToInt;
        inst.type = to;
        inst.operands = {operand};
        inst.payload = Inst::CastData{.from_type = from};
        return emit(std::move(inst));
    }

    ValueId IRBuilder::build_float_to_float(ValueId operand, TypeRef from, TypeRef to)
    {
        Inst inst;
        inst.opcode = Opcode::FloatToFloat;
        inst.type = to;
        inst.operands = {operand};
        inst.payload = Inst::CastData{.from_type = from};
        return emit(std::move(inst));
    }

    ValueId IRBuilder::build_int_to_ptr(ValueId operand, TypeRef ptr_type)
    {
        Inst inst;
        inst.opcode = Opcode::IntToPtr;
        inst.type = ptr_type;
        inst.operands = {operand};
        return emit(std::move(inst));
    }

    ValueId IRBuilder::build_ptr_to_int(ValueId operand, TypeRef int_type)
    {
        Inst inst;
        inst.opcode = Opcode::PtrToInt;
        inst.type = int_type;
        inst.operands = {operand};
        return emit(std::move(inst));
    }

    ValueId IRBuilder::build_bitcast(ValueId operand, TypeRef to)
    {
        Inst inst;
        inst.opcode = Opcode::Bitcast;
        inst.type = to;
        inst.operands = {operand};
        return emit(std::move(inst));
    }

    ValueId IRBuilder::build_get_field_ptr(ValueId base_ptr, uint32_t field_index, TypeRef result_ptr_type)
    {
        Inst inst;
        inst.opcode = Opcode::GetFieldPtr;
        inst.type = result_ptr_type;
        inst.operands = {base_ptr};
        inst.payload = Inst::FieldData{.field_index = field_index};
        return emit(std::move(inst));
    }

    ValueId IRBuilder::build_get_element_ptr(ValueId base_ptr, ValueId index, TypeRef result_ptr_type)
    {
        Inst inst;
        inst.opcode = Opcode::GetElementPtr;
        inst.type = result_ptr_type;
        inst.operands = {base_ptr, index};
        return emit(std::move(inst));
    }

    ValueId IRBuilder::build_extract_value(ValueId aggregate, uint32_t field_index, TypeRef field_type)
    {
        Inst inst;
        inst.opcode = Opcode::ExtractValue;
        inst.type = field_type;
        inst.operands = {aggregate};
        inst.payload = Inst::FieldData{.field_index = field_index};
        return emit(std::move(inst));
    }

    ValueId IRBuilder::build_insert_value(ValueId aggregate, ValueId value, uint32_t field_index, TypeRef agg_type)
    {
        Inst inst;
        inst.opcode = Opcode::InsertValue;
        inst.type = agg_type;
        inst.operands = {aggregate, value};
        inst.payload = Inst::FieldData{.field_index = field_index};
        return emit(std::move(inst));
    }

    void IRBuilder::build_branch(BasicBlock* target)
    {
        Inst inst;
        inst.opcode = Opcode::Branch;
        inst.payload = Inst::BranchData{.target = target};
        emit_void(std::move(inst));
    }

    void IRBuilder::build_cond_branch(ValueId condition, BasicBlock* then_bb, BasicBlock* else_bb)
    {
        Inst inst;
        inst.opcode = Opcode::CondBranch;
        inst.operands = {condition};
        inst.payload = Inst::CondBranchData{.then_block = then_bb, .else_block = else_bb};
        emit_void(std::move(inst));
    }

    ValueId IRBuilder::build_call(const std::string& callee, Function* callee_func, std::span<const ValueId> args, TypeRef return_type)
    {
        Inst inst;
        inst.opcode = Opcode::Call;
        inst.type = return_type;
        inst.operands.assign(args.begin(), args.end());
        inst.payload = Inst::CallData{.callee = callee, .callee_func = callee_func};
        return emit(std::move(inst));
    }

    void IRBuilder::build_call_void(const std::string& callee, Function* callee_func, std::span<const ValueId> args)
    {
        Inst inst;
        inst.opcode = Opcode::Call;
        inst.operands.assign(args.begin(), args.end());
        inst.payload = Inst::CallData{.callee = callee, .callee_func = callee_func};
        emit_void(std::move(inst));
    }

    void IRBuilder::build_return(ValueId value)
    {
        Inst inst;
        inst.opcode = Opcode::Return;
        inst.operands = {value};
        emit_void(std::move(inst));
    }

    void IRBuilder::build_return_void()
    {
        Inst inst;
        inst.opcode = Opcode::Return;
        emit_void(std::move(inst));
    }

    static const char* opcode_name(Opcode op)
    {
        switch (op)
        {
            case Opcode::Alloca:
                return "alloca";
            case Opcode::Load:
                return "load";
            case Opcode::Store:
                return "store";
            case Opcode::Add:
                return "add";
            case Opcode::Sub:
                return "sub";
            case Opcode::Mul:
                return "mul";
            case Opcode::Div:
                return "div";
            case Opcode::Mod:
                return "mod";
            case Opcode::BitAnd:
                return "and";
            case Opcode::BitOr:
                return "or";
            case Opcode::BitXor:
                return "xor";
            case Opcode::Shl:
                return "shl";
            case Opcode::Shr:
                return "shr";
            case Opcode::Eq:
                return "eq";
            case Opcode::Ne:
                return "ne";
            case Opcode::Lt:
                return "lt";
            case Opcode::Le:
                return "le";
            case Opcode::Gt:
                return "gt";
            case Opcode::Ge:
                return "ge";
            case Opcode::LogAnd:
                return "log_and";
            case Opcode::LogOr:
                return "log_or";
            case Opcode::Neg:
                return "neg";
            case Opcode::BitNot:
                return "bit_not";
            case Opcode::LogNot:
                return "log_not";
            case Opcode::IntToInt:
                return "int_to_int";
            case Opcode::IntToFloat:
                return "int_to_float";
            case Opcode::FloatToInt:
                return "float_to_int";
            case Opcode::FloatToFloat:
                return "float_to_float";
            case Opcode::IntToPtr:
                return "int_to_ptr";
            case Opcode::PtrToInt:
                return "ptr_to_int";
            case Opcode::Bitcast:
                return "bitcast";
            case Opcode::GetFieldPtr:
                return "get_field_ptr";
            case Opcode::GetElementPtr:
                return "get_element_ptr";
            case Opcode::ExtractValue:
                return "extract_value";
            case Opcode::InsertValue:
                return "insert_value";
            case Opcode::Branch:
                return "br";
            case Opcode::CondBranch:
                return "cond_br";
            case Opcode::Call:
                return "call";
            case Opcode::Return:
                return "ret";
            case Opcode::Const:
                return "const";
            case Opcode::Phi:
                return "phi";
        }
        return "???";
    }

    static std::string value_str(ValueId v)
    {
        if (v == kNoValue)
            return "void";

        return "%" + std::to_string(v);
    }

    std::string print_inst(const Inst& inst)
    {
        std::string s = "    ";

        if (inst.has_value())
            s += value_str(inst.dst) + " = ";

        s += opcode_name(inst.opcode);

        if (inst.type)
            s += " " + inst.type->to_string();

        for (auto op : inst.operands)
            s += " " + value_str(op);

        switch (inst.opcode)
        {
            case Opcode::Const: {
                auto& cd = inst.as_const();
                std::visit(
                    [&](auto&& v) {
                        using T = std::decay_t<decltype(v)>;
                        if constexpr (std::is_same_v<T, int64_t>)
                            s += std::format(" {}", v);
                        else if constexpr (std::is_same_v<T, uint64_t>)
                            s += std::format(" {}", v);
                        else if constexpr (std::is_same_v<T, double>)
                            s += std::format(" {}", v);
                        else if constexpr (std::is_same_v<T, bool>)
                            s += v ? " true" : " false";
                    },
                    cd.value);
                break;
            }
            case Opcode::Alloca: {
                auto& ad = inst.as_alloca();
                s += " " + ad.alloc_type->to_string();
                if (ad.count > 1)
                    s += std::format(", count={}", ad.count);
                break;
            }
            case Opcode::Branch: {
                auto& bd = inst.as_branch();
                s += " " + bd.target->label;
                break;
            }
            case Opcode::CondBranch: {
                auto& cbd = inst.as_cond_branch();
                s += " then=" + cbd.then_block->label + " else=" + cbd.else_block->label;
                break;
            }
            case Opcode::Call: {
                auto& cd = inst.as_call();
                s += " @" + cd.callee;
                break;
            }
            case Opcode::GetFieldPtr:
            case Opcode::ExtractValue:
            case Opcode::InsertValue: {
                auto& fd = inst.as_field();
                s += std::format(" field={}", fd.field_index);
                break;
            }
            case Opcode::IntToInt:
            case Opcode::IntToFloat:
            case Opcode::FloatToInt:
            case Opcode::FloatToFloat: {
                auto& cd = inst.as_cast();
                s += " from=" + cd.from_type->to_string();
                break;
            }
            case Opcode::Phi: {
                auto& pd = inst.as_phi();
                for (auto& entry : pd.entries)
                    s += " [" + entry.block->label + ": " + value_str(entry.value) + "]";
                break;
            }
            default:
                break;
        }

        return s;
    }

    std::string print_function(const Function& func)
    {
        std::string s;
        std::string linkage_str;
        switch (func.linkage)
        {
            case Linkage::Internal:
                linkage_str = "internal ";
                break;
            case Linkage::External:
                linkage_str = "external ";
                break;
            case Linkage::ExternDecl:
                linkage_str = "extern ";
                break;
        }

        s += linkage_str + "fn @" + func.name + "(";

        for (std::size_t i = 0; i < func.params.size(); ++i)
        {
            if (i > 0)
                s += ", ";

            s += func.params[i].type->to_string() + " " + value_str(func.params[i].value);
        }

        s += ") -> " + func.type->return_type->to_string();

        if (func.is_declaration())
        {
            s += "\n";
            return s;
        }

        s += " {\n";
        for (auto& bb : func.blocks)
        {
            s += "  " + bb->label + ":\n";
            for (auto& inst : bb->insts)
                s += print_inst(inst) + "\n";
        }

        s += "}\n";
        return s;
    }

    std::string print_module(const Module& module)
    {
        std::string s;
        s += "; module " + module.name + "\n\n";

        for (auto& g : module.globals)
        {
            std::string link;
            switch (g.linkage)
            {
                case Linkage::Internal:
                    link = "internal";
                    break;
                case Linkage::External:
                    link = "external";
                    break;
                case Linkage::ExternDecl:
                    link = "extern";
                    break;
            }

            s += link + " global @" + g.name + " : " + g.type->to_string();
            if (g.is_const)
                s += " const";
            s += "\n";
        }

        if (!module.globals.empty())
            s += "\n";

        for (auto& f : module.functions)
            s += print_function(*f) + "\n";

        return s;
    }

} // namespace dcc::ir
