#ifndef DCC_IR_IR_HH
#define DCC_IR_IR_HH

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

namespace dcc::ir
{
    struct Type;
    struct Function;
    struct BasicBlock;
    struct Inst;
    struct Module;

    using TypeRef = const Type*;
    using InstRef = Inst*;
    using BlockRef = BasicBlock*;

    enum class TypeKind : uint8_t
    {
        Void,
        Bool,
        Integer,
        Float,
        Pointer,
        Array,
        Struct,
        Function,
    };

    struct Type
    {
        TypeKind kind;

        [[nodiscard]] bool is_void() const noexcept { return kind == TypeKind::Void; }
        [[nodiscard]] bool is_bool() const noexcept { return kind == TypeKind::Bool; }
        [[nodiscard]] bool is_integer() const noexcept { return kind == TypeKind::Integer; }
        [[nodiscard]] bool is_float() const noexcept { return kind == TypeKind::Float; }
        [[nodiscard]] bool is_pointer() const noexcept { return kind == TypeKind::Pointer; }
        [[nodiscard]] bool is_array() const noexcept { return kind == TypeKind::Array; }
        [[nodiscard]] bool is_struct() const noexcept { return kind == TypeKind::Struct; }
        [[nodiscard]] bool is_function() const noexcept { return kind == TypeKind::Function; }
        [[nodiscard]] bool is_numeric() const noexcept { return is_integer() || is_float(); }

        [[nodiscard]] virtual uint64_t size_bytes() const noexcept = 0;
        [[nodiscard]] virtual uint64_t align_bytes() const noexcept = 0;
        [[nodiscard]] virtual std::string to_string() const = 0;

        virtual ~Type() = default;

    protected:
        explicit Type(TypeKind k) : kind{k} {}
    };

    struct VoidType final : Type
    {
        VoidType() : Type(TypeKind::Void) {}
        uint64_t size_bytes() const noexcept override { return 0; }
        uint64_t align_bytes() const noexcept override { return 1; }
        std::string to_string() const override { return "void"; }
    };

    struct BoolType final : Type
    {
        BoolType() : Type(TypeKind::Bool) {}
        uint64_t size_bytes() const noexcept override { return 1; }
        uint64_t align_bytes() const noexcept override { return 1; }
        std::string to_string() const override { return "bool"; }
    };

    struct IntegerType final : Type
    {
        uint8_t width;
        bool is_signed;

        IntegerType(uint8_t w, bool s) : Type(TypeKind::Integer), width{w}, is_signed{s} {}
        uint64_t size_bytes() const noexcept override { return width / 8; }
        uint64_t align_bytes() const noexcept override { return width / 8; }
        std::string to_string() const override { return (is_signed ? "i" : "u") + std::to_string(width); }
    };

    struct FloatType final : Type
    {
        uint8_t width;

        explicit FloatType(uint8_t w) : Type(TypeKind::Float), width{w} {}
        uint64_t size_bytes() const noexcept override { return width / 8; }
        uint64_t align_bytes() const noexcept override { return width / 8; }
        std::string to_string() const override { return "f" + std::to_string(width); }
    };

    struct PointerType final : Type
    {
        TypeRef pointee;

        explicit PointerType(TypeRef p) : Type(TypeKind::Pointer), pointee{p} {}
        uint64_t size_bytes() const noexcept override { return 8; }
        uint64_t align_bytes() const noexcept override { return 8; }
        std::string to_string() const override { return "*" + pointee->to_string(); }
    };

    struct ArrayType final : Type
    {
        TypeRef element;
        uint64_t length;

        ArrayType(TypeRef e, uint64_t n) : Type(TypeKind::Array), element{e}, length{n} {}
        uint64_t size_bytes() const noexcept override { return element->size_bytes() * length; }
        uint64_t align_bytes() const noexcept override { return element->align_bytes(); }
        std::string to_string() const override { return "[" + std::to_string(length) + "]" + element->to_string(); }
    };

    struct StructType final : Type
    {
        struct Field
        {
            std::string name;
            TypeRef type;
            uint32_t offset;
        };

        std::string name;
        std::vector<Field> fields;
        uint64_t size{0};
        uint64_t align{1};

        explicit StructType(std::string n) : Type(TypeKind::Struct), name{std::move(n)} {}

        uint64_t size_bytes() const noexcept override { return size; }
        uint64_t align_bytes() const noexcept override { return align; }
        std::string to_string() const override { return name; }

        void compute_layout();
    };

    struct FunctionType final : Type
    {
        TypeRef return_type;
        std::vector<TypeRef> param_types;
        bool is_variadic{false};

        FunctionType(TypeRef ret, std::vector<TypeRef> params, bool variadic = false)
            : Type(TypeKind::Function), return_type{ret}, param_types{std::move(params)}, is_variadic{variadic}
        {
        }

        uint64_t size_bytes() const noexcept override { return 8; }
        uint64_t align_bytes() const noexcept override { return 8; }
        std::string to_string() const override;
    };

    class TypeArena
    {
    public:
        TypeArena();

        TypeRef void_type() const noexcept { return &m_void; }
        TypeRef bool_type() const noexcept { return &m_bool; }

        TypeRef integer_type(uint8_t width, bool is_signed);
        TypeRef float_type(uint8_t width);

        TypeRef pointer_to(TypeRef pointee);
        TypeRef array_of(TypeRef element, uint64_t length);

        StructType* make_struct(std::string name);
        FunctionType* make_function(TypeRef ret, std::vector<TypeRef> params, bool variadic = false);

        StructType* make_slice_type(TypeRef element);

        StructType* make_enum_type(std::string name, TypeRef tag_type, uint64_t payload_size, uint64_t payload_align);

    private:
        VoidType m_void;
        BoolType m_bool;

        ir::IntegerType m_integers[8];
        ir::FloatType m_f32{32};
        ir::FloatType m_f64{64};

        std::vector<std::unique_ptr<Type>> m_arena;

        struct PtrKey
        {
            TypeRef pointee;
            bool operator==(const PtrKey&) const = default;
        };

        struct ArrKey
        {
            TypeRef element;
            uint64_t length;
            bool operator==(const ArrKey&) const = default;
        };

        struct PtrHash
        {
            std::size_t operator()(const PtrKey& k) const noexcept;
        };

        struct ArrHash
        {
            std::size_t operator()(const ArrKey& k) const noexcept;
        };

        std::unordered_map<PtrKey, TypeRef, PtrHash> m_pointer_cache;
        std::unordered_map<ArrKey, TypeRef, ArrHash> m_array_cache;
    };

    using ValueId = uint32_t;
    constexpr ValueId kNoValue = 0;

    enum class Opcode : uint8_t
    {
        Alloca,
        Load,
        Store,

        Add,
        Sub,
        Mul,
        Div,
        Mod,

        BitAnd,
        BitOr,
        BitXor,
        BitNot,
        Shl,
        Shr,

        Eq,
        Ne,
        Lt,
        Le,
        Gt,
        Ge,

        LogAnd,
        LogOr,
        LogNot,

        Neg,

        IntToInt,
        IntToFloat,
        FloatToInt,
        FloatToFloat,
        IntToPtr,
        PtrToInt,
        Bitcast,

        GetFieldPtr,
        GetElementPtr,
        ExtractValue,
        InsertValue,

        Branch,
        CondBranch,
        Call,
        Return,

        Const,
        Phi,
    };

    struct Inst
    {
        Opcode opcode;
        ValueId dst{kNoValue};
        TypeRef type{nullptr};

        std::vector<ValueId> operands;

        struct ConstData
        {
            std::variant<int64_t, uint64_t, double, bool> value;
        };

        struct AllocaData
        {
            TypeRef alloc_type;
            uint32_t count{1};
        };

        struct BranchData
        {
            BlockRef target{nullptr};
        };

        struct CondBranchData
        {
            BlockRef then_block{nullptr};
            BlockRef else_block{nullptr};
        };

        struct CallData
        {
            std::string callee;
            Function* callee_func{nullptr};
        };

        struct FieldData
        {
            uint32_t field_index;
        };

        struct CastData
        {
            TypeRef from_type;
        };

        struct PhiData
        {
            struct Entry
            {
                BlockRef block;
                ValueId value;
            };
            std::vector<Entry> entries;
        };

        std::variant<std::monostate, ConstData, AllocaData, BranchData, CondBranchData, CallData, FieldData, CastData, PhiData> payload;

        [[nodiscard]] const ConstData& as_const() const { return std::get<ConstData>(payload); }
        [[nodiscard]] const AllocaData& as_alloca() const { return std::get<AllocaData>(payload); }
        [[nodiscard]] const BranchData& as_branch() const { return std::get<BranchData>(payload); }
        [[nodiscard]] const CondBranchData& as_cond_branch() const { return std::get<CondBranchData>(payload); }
        [[nodiscard]] const CallData& as_call() const { return std::get<CallData>(payload); }
        [[nodiscard]] const FieldData& as_field() const { return std::get<FieldData>(payload); }
        [[nodiscard]] const CastData& as_cast() const { return std::get<CastData>(payload); }
        [[nodiscard]] const PhiData& as_phi() const { return std::get<PhiData>(payload); }

        [[nodiscard]] CallData& as_call() { return std::get<CallData>(payload); }
        [[nodiscard]] PhiData& as_phi() { return std::get<PhiData>(payload); }
        [[nodiscard]] CondBranchData& as_cond_branch() { return std::get<CondBranchData>(payload); }

        [[nodiscard]] bool is_terminator() const noexcept { return opcode == Opcode::Branch || opcode == Opcode::CondBranch || opcode == Opcode::Return; }

        [[nodiscard]] bool has_value() const noexcept { return dst != kNoValue; }
    };

    struct BasicBlock
    {
        std::string label;
        std::vector<Inst> insts;
        Function* parent{nullptr};

        explicit BasicBlock(std::string lbl) : label{std::move(lbl)} {}

        Inst& append(Inst inst)
        {
            insts.push_back(std::move(inst));
            return insts.back();
        }

        [[nodiscard]] bool is_terminated() const noexcept { return !insts.empty() && insts.back().is_terminator(); }
    };

    enum class Linkage : uint8_t
    {
        Internal,
        External,
        ExternDecl,
    };

    struct Global
    {
        std::string name;
        TypeRef type;
        Linkage linkage{Linkage::Internal};
        bool is_const{false};

        std::optional<std::variant<int64_t, uint64_t, double, bool>> init;
    };

    struct Function
    {
        std::string name;
        FunctionType* type;
        Linkage linkage{Linkage::Internal};

        struct Param
        {
            std::string name;
            TypeRef type;
            ValueId value;
        };

        std::vector<Param> params;
        std::vector<std::unique_ptr<BasicBlock>> blocks;

        [[nodiscard]] bool is_declaration() const noexcept { return blocks.empty(); }

        [[nodiscard]] BasicBlock* entry() const { return blocks.empty() ? nullptr : blocks.front().get(); }

        BasicBlock* add_block(std::string label);

        ValueId next_value() { return ++m_next_value; }

    private:
        ValueId m_next_value{kNoValue};
    };

    struct Module
    {
        std::string name;

        TypeArena types;
        std::vector<std::unique_ptr<Function>> functions;
        std::vector<Global> globals;

        Function* add_function(std::string name, FunctionType* type, Linkage linkage = Linkage::Internal);
        Function* find_function(std::string_view name) const;

        void add_global(Global g);
    };

    class IRBuilder
    {
    public:
        IRBuilder(Function& func, TypeArena& types);
        IRBuilder(Function& func, TypeArena& types, BasicBlock* block);

        void set_block(BasicBlock* block) { m_block = block; }
        [[nodiscard]] BasicBlock* current_block() const { return m_block; }
        [[nodiscard]] Function& function() const { return m_func; }

        ValueId build_const_int(int64_t value, TypeRef type);
        ValueId build_const_uint(uint64_t value, TypeRef type);
        ValueId build_const_float(double value, TypeRef type);
        ValueId build_const_bool(bool value);

        ValueId build_alloca(TypeRef alloc_type, TypeRef result_ptr_type, uint32_t count = 1);
        ValueId build_load(ValueId ptr, TypeRef result_type);
        void build_store(ValueId ptr, ValueId value);

        ValueId build_add(ValueId lhs, ValueId rhs, TypeRef type);
        ValueId build_sub(ValueId lhs, ValueId rhs, TypeRef type);
        ValueId build_mul(ValueId lhs, ValueId rhs, TypeRef type);
        ValueId build_div(ValueId lhs, ValueId rhs, TypeRef type);
        ValueId build_mod(ValueId lhs, ValueId rhs, TypeRef type);

        ValueId build_bit_and(ValueId lhs, ValueId rhs, TypeRef type);
        ValueId build_bit_or(ValueId lhs, ValueId rhs, TypeRef type);
        ValueId build_bit_xor(ValueId lhs, ValueId rhs, TypeRef type);
        ValueId build_bit_not(ValueId operand, TypeRef type);
        ValueId build_shl(ValueId lhs, ValueId rhs, TypeRef type);
        ValueId build_shr(ValueId lhs, ValueId rhs, TypeRef type);

        ValueId build_eq(ValueId lhs, ValueId rhs, TypeRef operand_type);
        ValueId build_ne(ValueId lhs, ValueId rhs, TypeRef operand_type);
        ValueId build_lt(ValueId lhs, ValueId rhs, TypeRef operand_type);
        ValueId build_le(ValueId lhs, ValueId rhs, TypeRef operand_type);
        ValueId build_gt(ValueId lhs, ValueId rhs, TypeRef operand_type);
        ValueId build_ge(ValueId lhs, ValueId rhs, TypeRef operand_type);

        ValueId build_log_and(ValueId lhs, ValueId rhs);
        ValueId build_log_or(ValueId lhs, ValueId rhs);
        ValueId build_log_not(ValueId operand);

        ValueId build_neg(ValueId operand, TypeRef type);

        ValueId build_int_to_int(ValueId operand, TypeRef from, TypeRef to);
        ValueId build_int_to_float(ValueId operand, TypeRef from, TypeRef to);
        ValueId build_float_to_int(ValueId operand, TypeRef from, TypeRef to);
        ValueId build_float_to_float(ValueId operand, TypeRef from, TypeRef to);
        ValueId build_int_to_ptr(ValueId operand, TypeRef ptr_type);
        ValueId build_ptr_to_int(ValueId operand, TypeRef int_type);
        ValueId build_bitcast(ValueId operand, TypeRef to);

        ValueId build_get_field_ptr(ValueId base_ptr, uint32_t field_index, TypeRef result_ptr_type);
        ValueId build_get_element_ptr(ValueId base_ptr, ValueId index, TypeRef result_ptr_type);
        ValueId build_extract_value(ValueId aggregate, uint32_t field_index, TypeRef field_type);
        ValueId build_insert_value(ValueId aggregate, ValueId value, uint32_t field_index, TypeRef agg_type);

        void build_branch(BasicBlock* target);
        void build_cond_branch(ValueId condition, BasicBlock* then_bb, BasicBlock* else_bb);
        ValueId build_call(const std::string& callee, Function* callee_func, std::span<const ValueId> args, TypeRef return_type);
        void build_call_void(const std::string& callee, Function* callee_func, std::span<const ValueId> args);
        void build_return(ValueId value);
        void build_return_void();

    private:
        Function& m_func;
        TypeArena& m_types;
        BasicBlock* m_block{nullptr};

        ValueId emit(Inst inst);
        void emit_void(Inst inst);
    };

    std::string print_module(const Module& module);
    std::string print_function(const Function& func);
    std::string print_inst(const Inst& inst);

} // namespace dcc::ir

#endif /* DCC_IR_IR_HH */
