# `using`

`using` is a multi-purpose declaration keyword. It's meaning is determined by the
context around it.

## Type aliases

```dc
using Name = Type;
```

Creates a type alias.

```dc
using Vec3 = f32[3];
using Callback = void(*)(i32, i32);
```

## Concepts

```dc
using Name(Params) = compiles(...) { ... };
using Name(Params) = expression;
```

Defines a compile-time predicate usable in template constraints.

```dc
using Addable(T) = compiles(T a, T b) {
    a + b;
};

using Arithmetic(T) = Addable(T) & !PointerLike(T);
```

## Bare imports

```dc
using path::name;
```

Brings one existing binding into the current scope under its original name.

```dc
using math::sin;
```

## Wildcard imports

```dc
using path::*;
```

Brings all exported bindings from a module or namespace into the current scope.

```dc
using math::*;
```

## Aliased wildcard imports

```dc
using Alias = path::*;
```

Creates a namespace group alias for all bindings in the target namespace.

```dc
using math_group = math::*;
// math_group::sin, math_group::cos, ...
```

## List imports

```dc
using { path::a, path::b };
using prefix::{a, b};
```

Brings several selected bindings into the current scope.

```dc
using { math::sin, math::cos };
using lib::{stdout, write};
```

Lists may be nested:

```dc
using lib::{io::{stdout, stderr}, sys::{write, read}};
```

## Aliased list imports

```dc
using Alias = { path::a, path::b };
```

Creates a namespace group containing the selected bindings.

```dc
using math_group = {math::sin, math::cos};
// math_group::sin, math_group::cos
```

## Path and module aliases

```dc
using Alias = path::target;
```

Creates an alias to an existing binding. The target may be a type, value,
function overload set, namespace, or module.

```dc
using V = math::Vec3;
using platform = std::win;
```

## Namespace

```dc
using Namespace::name = target;
```

Binds an existing target into a namespace path. This is used to attach free
functions to a type namespace for namespaced construction for syntax sake.

```dc
public Pair make(i32 a, i32 b) {
    return Pair { first = a, second = b };
}

public using Pair::make = make;
// Pair::make(1, 2)
```

## Visibility forms

```dc
public using Alias = target;
```

Re-exports the alias normally. Importers can access it through this module's
namespace, for example `this_module::Alias`.

```dc
using public Alias = target;
```

Spills the alias into importers' scopes. Importers can access `Alias`
unqualified after importing this module.

```dc
using public name;
```

Spills an already-resolved binding under its original name.
