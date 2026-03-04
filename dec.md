Immutability by design,
i8, u8, i16, u16, i32, u32, i64, u64, f32, f64, bool, void, null_t.
Struct, union, enum with tagged enum support.
`mutable`, `restrict`, `volatile` qualifiers.
`using` for type aliases.
`import`, `module` and `public` for visbility, and module system, similar to rust.
`defer <statement>` runs at scope exit.

C Like `{}` is just a block statement rule taken to extremene, even with functions.
Explicit casts with `as`.
Rust like procedural macros called `semantic translators` with `@` for function like invocations and `#[]` for derive like functionality for structs enums and unions.
Structs don't have ctor/dtor, can have static & nonstatic member functions.
Only `.`, no `->`.
D like syntax for templates, `T helloTemplate(T)(T value);` and for template constants: `void serialWrite(u8 COMN)(u8 byte)`.
`static if` for template constant checks guaranteed on compile time.
UFCS

Slices VS Arrays VS FAM:
```
struct name {
    []u8 slice;
    u8[256] array;
    u8[] fam;
};

extern []u8 extern_slice;
extern u8[256] extern_array;
extern u8[] extern_fam;
```

```
match value {
    // literal
    1 => do_something(), 
    
    // binding with a guard
    x if x > 10 => {
        print(x);
    },
    
    // tagged enums
    Optional.Some(val) => print(val),
    
    // struct unbinding.
    Point { x, .. } => print(x),
    
    // wildcards
    _ => default_behavior(),
}
```
