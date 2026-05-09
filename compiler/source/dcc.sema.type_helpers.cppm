export module dcc.sema.type_helpers;

import dcc.ast;
import dcc.types;

export namespace dcc::sema
{
    [[nodiscard]] types::TypePtr get_canonical(ast::TypeSema const& ts) noexcept
    {
        return reinterpret_cast<types::TypePtr>(ts.canonical);
    }

    void set_canonical(ast::TypeSema& ts, types::TypePtr tp) noexcept
    {
        ts.canonical = reinterpret_cast<decltype(ts.canonical)>(tp);
    }

    [[nodiscard]] types::TypePtr get_resolved_type(ast::ExprSema const& es) noexcept
    {
        return reinterpret_cast<types::TypePtr>(es.resolved_type);
    }

    void set_resolved_type(ast::ExprSema& es, types::TypePtr tp) noexcept
    {
        es.resolved_type = reinterpret_cast<decltype(es.resolved_type)>(tp);
    }

} // namespace dcc::sema
