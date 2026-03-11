#ifndef DCC_IR_MANGLE_HH
#define DCC_IR_MANGLE_HH

#include <sema/types.hh>
#include <string>
#include <string_view>
#include <util/si.hh>

namespace dcc::ir
{
    // _D <module path> _ <function name> <template args> <param types>
    //   Primitives:    i8->a, u8->h, i16->s, u16->t, i32->i, u32->j, i64->l, u64->m, f32->f, f64->d, bool->b, void->v, null_t->n
    //   Pointer:       P <pointee>
    //   Slice:         S <element>
    //   Array:         A <length> _ <element>
    //   Function type: F <param-types> Z <return-type>

    //   module math; fn add(i32, i32) -> i32  =>  _D4math_3addii
    //   module io; fn write([]u8) -> void     =>  _D2io_5writeShv

    class Mangler
    {
    public:
        [[nodiscard]] static std::string mangle_function(std::span<const si::InternedString> module_path, std::string_view func_name,
                                                         std::span<sema::SemaType* const> param_types, std::span<sema::SemaType* const> template_args = {});

        [[nodiscard]] static std::string demangle(std::string_view mangled);

    private:
        static void encode_module_path(std::string& out, std::span<const si::InternedString> path);
        static void encode_identifier(std::string& out, std::string_view name);
        static void encode_type(std::string& out, const sema::SemaType* ty);
        static void encode_template_args(std::string& out, std::span<sema::SemaType* const> args);
    };

} // namespace dcc::ir

#endif /* DCC_IR_MANGLE_HH */
