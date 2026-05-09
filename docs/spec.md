# DCC Language Specification - Draft v0.1

## 1. Primitive Types

| Type                      | Size       | Description                                          |
|---------------------------|------------|------------------------------------------------------|
| `i8`, `i16`, `i32`, `i64` | 1, 2, 4, 8 | Signed two's complement integers                     |
| `u8`, `u16`, `u32`, `u64` | 1, 2, 4, 8 | Unsigned integers                                    |
| `f32`, `f64`              | 4, 8       | IEEE 754 floating point                              |
| `bool`                    | 1          | `true` / `false`                                     |
| `char`                    | 1          | ASCII character                                      |
| `void`                    | 0          | Unit / absence of value                              |
| `null_t`                  | ptr        | Type of `null`, inhabits all pointer/optional types  |

---

## 2. Composite Types

### 2.1 Structs

No constructors, destructors, or member functions. Data-only aggregates. Initialization via struct literals. Behavior is attached externally via free functions and UFCS.

```dc
struct Point {
    f32 x;
    f32 y;
}

// factory pattern: called as Point::new(1.0, 2.0)
public Point new_point(f32 x, f32 y) {
    return Point { x = x, y };
}
using Point::new = new_point;

// UFCS method: called as p.length()
public f64 length(const Point* self) {
    return sqrt((self.x * self.x + self.y * self.y) as f64);
}
```

### 2.2 Unions

Untagged, C-style. Unsafe by nature. Access to the wrong field is undefined behavior.

```dc
union Register {
    u32 full;
    u16[2] halves;
    u8[4] bytes;
}
```

### 2.3 Enums

Two forms: plain enums and tagged enums exist.

```dc
// plain enum: default backing type is i32, overridable
enum Color : u8 {
    Red,        // 0
    Green,      // 1
    Blue = 10,  // explicit
}

// tagged enum: each variant optionally carries data
enum Result(T, E) {
    Ok(T),
    Err(E),
    Cancelled,    // no payload
}

enum MaybeInt {
    Int(i32),
    NoInt,
}

enum Optional(T) {
    Some(T),
    None,
}
```

---

## 3. Pointers & References

Only raw pointers, no references. No `->`, only `.` with automatic dereference through any number of pointer indirections.

---

## 4. Qualifiers

`const`, `restrict`, `volatile` applied to declarations.

```dc
i32*          // pointer to i32
const i32*    // pointer to const i32
volatile u8*  // pointer to volatile u8
i32* const    // const pointer to mutable i32
```

---

## 5. Arrays, Slices & Flexible Array Members

Three distinct compound types with different syntax positions:

```dc
u8[256] array;       // fixed-size array: 256 bytes, value type, stack-allocable
[]u8 slice;          // slice: fat pointer (ptr + len), does NOT own memory
u8[] fam;            // flexible array member: must be last field in a struct or extern global variable, unsized
```

The grammar pattern:

| Syntax | Meaning          | Size known at     | Owns memory |
|--------|------------------|-------------------|-------------|
| `T[N]` | Fixed array      | Compile time      | Yes         |
| `[]T`  | Slice            | Runtime (fat ptr) | No          |
| `T[]`  | FAM              | Unkown            | No          |

---

## 6. Type Aliases, Concepts & Using

`using` is the multi-tool keyword. Its meaning is determined by what follows it.

### 6.1 Type alias

```dc
using Vec3 = f32[3];
using Callback = void(*)(i32, i32);   // function pointer type
```

### 6.2 Concept definition

```dc
using Printable(T) = compiles(T t) {
    print(t);
};

using Numeric(T) = compiles(T a, T b) {
    a + b;
    a * b;
};
```

### 6.3 Module symbol import

```dc
using Vec3 = math::Vec3;                 // bring math::Vec3 as Vec3
using geom::Point = math::Point;         // bring as geom::Point (custom namespace)
using Point = math::geom::Point;         // shorthand
```

### 6.4 Re-export into importer's scope

```dc
// In module `graphics`:

// Makes `math::Vec3` appear as `Vec3` in anyone who imports `graphics`
using public Vec3 = math::Vec3;

// Makes it appear as `graphics::Vec3` in the importer (normal re-export) 
public using Vec3 = math::Vec3;
```

The distinction:

| Syntax                    | Effect                                            |
|---------------------------|---------------------------------------------------|
| `using X = Y;`            | Private alias, local scope only                   |
| `public using X = Y;`     | Alias is importable: importers see `ourmod::X`    |
| `using public X = Y;`     | Direct injection: importers see `X` unqualified   |

---

## 7. Module System

### 7.1 Declaration

```dc
module graphics;              // every file begins with a module declaration

import math;                  // import module, compiler finds file `math.dc`, folders are ignored.
import std::io;               // sub-module import
```

Module and import paths must match a walkable path by the compiler from any of the configured import paths.
The folder in which the currently-being-compiled file is located is automatically added to the import path.

If file is located at `/std/math/sin.dc` relative to an import path, it can be declared as either `module sin;` or `module std::math::sin;`. (the latter is advised)
In both cases, the importer must write the full qualified path; `import std::math::sin;`.
The name of a module may collide with a folder name along a qualified path; `/std/math.dc` and `/std/math/sin.dc` are both allowed, importable as `import std::math;` and `import std::math::sin;` respectively.

#### 7.1.1 Path Resolution and Common Parent Stripping

When resolving the namespace of an imported module, the compiler automatically
calculates the common prefix between the current module's path and the target
module's path and strips that prefix from the visibility namespace. This
prevents redundant scoping and allows for cleaner access to deep module
hierarchies.

- If the current module is `a` at `project/f1/f2/a.dc` (Namespace: `f1::f2::a`).
- And the target module is `b` at `project/f1/f2/f3/b.dc` (Namespace: `f1::f2::f3::b`).
- An import statement import `f1::f2::f3::b;` (or shorthand `import f3::b;`) inside `a.dc` will result in `b` being accessible as `f3::b`.

### 7.2 Circular imports

Modules can import each other.

### 7.3 Visibility

Functions, structs, unions, enums, type alias and other top-level declarations are private by default. The `public` keyword as the first keyword of a declaration must be used to allow the produced name to be imported and used.

```dc
public struct Point {
    f32 x;
    f32 y;
}

public f32 distance(Point a, Point b) { ... }

// private
f32 helper(f32 x) { ... }
```

---

## 8. Templates

### 8.1 Function templates

```dc
// template type parameter
T max(T)(T a, T b) {
    return if a > b { a } else { b };
}

const u8 COM_DATA = 0x0;

// template value parameter
void serial_write(u8 COM_BASE)(u8 byte) {
    volatile u8* port = COM_PORT + COM_DATA as u64 as volatile u8*;
    *port = byte;
}

// multiple parameters
void copy(T, u64 N)(T* dst, const T* src) {
    for u64 i = 0; i < N; i++ {
        dst[i] = src[i];
    }
}
```

**Call syntax:**

For single parameter functions automatic template type deduction and the following syntax is allowed:

```dc
T max(T)(T a, T b) {
    return if a > b { a } else { b };
}

void switch_ctime(u64 a)() {
    static if a == 2 {
        ...
    } else {
        ...
    }
}

max!i32(32, 33);
max(32, 33); // due to the inability of the compiler to guess the literal type, it is defaulted to i32.

i32 value = 32;
max(value, 33);
max!u64(value as u64, 33);

switch_ctime!1();
switch_ctime!2();
```

For multiple parameter template functions, an explicit `()` around the template parameters is required:

```dc
void copy(T, u64 N)(T* dst, const T* src) {
    for u64 i = 0; i < N; i++ {
        dst[i] = src[i];
    }
}

u8* dst1 = ...;
const u8* src1 = ...;
copy!(u8, 23)(dst1, src1);
```

### 8.2 Struct templates

```dc
struct Pair(T, U) {
    T first;
    U second;
}

Pair(i32, f64) p = { first = 1, second = 2.0 }; // type of rhs is implicitly guessed from the right hand side.
Pair(u8, bool) p2 = { 0xFF, false };
```

### 8.3 Constrained templates

```dc
using Addable(T) = compiles(T a, T b) { a + b; };

T sum(T)([]const T items) if Addable(T) {
    T acc = items[0];
    for u64 i = 1; i < items.len; i++ {
        acc += items[i];
    }

    return acc;
}
```

### 8.4 `static if` and `static match`

```dc
void process(T)(T value) {
    static if compiles(T t) { t.serialize(); } {
        value.serialize();
    } else static if T == u8 {
        raw_write(value);
    } else {
        static match T {
            u16 => raw_write16(value);
            _ => compile_error("T must be serializable, u8 or u16");
        }
    }
}

void execute(u8 command)() {
    static if command == 1 {
        ...
    } else if command == 2 {
        ...
    } else {
        static match command {
            3..=10 => ...,
            11..13 => ...,
            13 => ...,
            _ => ...
        }
    }
}
```

---

## 9. Expressions

### 9.1 Block expressions

Braces form a block expression. The last expression (without `;`) is the block's value. Pasted from rust.

```dc
i32 x = {
    i32 a = compute();
    i32 b = transform(a);
    a + b    // no semicolon: this is the block's value
};

// if-else is an expression
i32 y = if cond { 1 } else { 2 };

// match is an expression
i32 z = match opt {
    Optional::Some(v) => v,
    Optional::None => 0,
};
```

### 9.2 Casts

```dc
f64 x = 3.14;
i32 y = x as i32;        // truncating cast
u8 b = 256 as u8;        // error
u8* p = addr as u8*;     // integer to pointer
```

### 9.3 Literals & type inference

No implicit conversions, so literals need to be smart:

```dc
i32 x = 42;        // 42 is i32
u8 y = 42;         // 42 is u8
u8 z = 256;        // error
f64 w = 3.14;      // 3.14 is f64
f32 v = 3.14;      // 3.14 is f32

// ambiguous context:
T function(T)(T a) { ... }

i32 a = function(32); // inferred to i32
u8 b = function(32); // inferred to u8
function(32); // value discarded, guessed to i32.
```

---

## 10. Match Expressions

```dc
match value {
    // literal patterns
    0 => handle_zero(),
    1 | 2 | 3 => handle_small(),

    // range patterns
    4..10 => handle_medium(),

    // binding with guard
    x if x > 100 => handle_large(x),

    // tagged enum destructuring
    Result::Ok(inner) => use(inner),
    Result::Err(e) => panic(e),

    // struct destructuring
    Point { x, y => 0 } => on_x_axis(x),
    Point { x, _ } => general(x),

    // nested patterns
    Optional::Some(Point { x, y }) => plot(x, y),

    // wildcard
    _ => default(),
}
```

**Match on pointers:**

```dc
match ptr {
    null => handle_null(),
    p => use(*p), // p is guaranteed non-null here
}
```

---

## 11. UFCS (Uniform Function Call Syntax)

Any free function whose first parameter is `T`, `const T`, `T*`, or `const T*` can be called with dot syntax on a value of type `T`.
When selecting candidates, priority to the exact match followed by auto referencing/dereferencing.

```dc
f64 length(Point* self) { ... }

Point p = Point { x = 3.0, y = 4.0 };
f64 len = p.length(); // auto-reference
```

```dc
f64 length(const Point* self) { ... }
f64 length(Point self) { ... }

Point p = Point { x = 3.0, y = 4.0 };
f64 len = p.length(); // passing by value is preferred
```

```dc
f64 length(const Point* self) { ... }
f64 length(Point* self) { ... }

const Point p = Point { x = 3.0, y = 4.0 };
f64 len = p.length(); // passing const is preferred
```

```dc
f64 length(Point* self) { ... }

const Point p = Point { x = 3.0, y = 4.0 };
f64 len = p.length(); // error
```

**Resolution order:**

1. Struct field access
2. Functions in the current module
3. Functions in imported modules

---

## 12. Defer

```dc
void process_file([]const u8 path) {
    File* f = open(path);
    defer close(f);

    u8* buf = alloc(1024);
    defer free(buf);

    // ... use f and buf ...
}
```

`defer` takes a single statement. Deferred statements execute in reverse order at scope exit. This includes early `return`, `break`, `continue`.

---

## 13. Control Flow

```dc
// if expression or statement
if cond {
    body();
}

if cond {
    a()
} else if other {
    b()
} else {
    c()
}

// while
while cond {
    body();
}

// do-while
do {
    body();
} while cond;

// for c style
for i32 i = 0; i < n; i++ {
    body(i);
}

// for range based for slices
for item in items {
    process(item);
}
```

---

## 14. Attributes

```dc
@[packed]
struct Header {
    u32 magic;
    u16 version;
}

@align(16)
u8[64] buffer;

@inline
void hot_path() { ... }

@[deprecated("use new_api instead")]
void old_api() { ... }

@nomangle
void c_interop_func(i32 x);
```

`[]` can be freely ommitted for one attribute lists.

---

## 15. Function Pointers

```dc
// function pointer type
using BinOp = i32(*)(i32, i32);

i32 apply(BinOp op, i32 a, i32 b) {
    return op(a, b);
}

i32 add(i32 a, i32 b) { return a + b; }

apply(add, 1, 2);
```

---
