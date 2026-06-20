const std = @import("std");

pub const CodeModel = enum { default, small, kernel, medium, large };
pub const DebugFormat = enum { none, auto, dwarf, pdb };

pub const TargetTriple = enum {
    @"x86_64-elf",
    @"x86-elf",
    @"x86_64-coff",
    @"x86-coff",

    pub fn fromZigTarget(target: std.Target) !TargetTriple {
        const arch = target.cpu.arch;
        const os = target.os.tag;

        return match: {
            if (arch == .x86_64 and (os == .linux or os == .freestanding)) break :match .@"x86_64-elf";
            if (arch == .x86 and (os == .linux or os == .freestanding)) break :match .@"x86-elf";
            if (arch == .x86_64 and os == .windows) break :match .@"x86_64-coff";
            if (arch == .x86 and os == .windows) break :match .@"x86-coff";

            return error.UnsupportedDccTarget;
        };
    }
};

pub const CompileOptions = struct {
    dcc_exe: []const u8 = "dcc",

    name: []const u8,
    source_file: std.Build.LazyPath,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,

    compile_only: bool = false,
    libdcext: bool = false,
    position_independent_code: bool = false,

    no_red_zone: bool = false,
    no_simd: bool = false,
    no_x87: bool = false,
    no_stack_protector: bool = false,
    no_stack_probe: bool = false,
    code_model: ?CodeModel = null,

    bounds_check: ?bool = null,
    emit_debug_info: ?bool = null,
    debug_format: ?DebugFormat = null,
    omit_frame_pointer: ?bool = null,

    include_dirs: []const std.Build.LazyPath = &.{},
};

pub const DccArtifact = struct {
    step: *std.Build.Step.Run,
    output_file: std.Build.LazyPath,
};

pub fn compile(b: *std.Build, options: CompileOptions) DccArtifact {
    const run = b.addSystemCommand(&.{options.dcc_exe});

    const dcc_triple = TargetTriple.fromZigTarget(options.target.result) catch {
        std.debug.print(
            \\
            \\ Error: Target "{}" is not supported by dcc.
            \\ Supported targets are: x86_64-elf, x86-elf, x86_64-coff, x86-coff
            \\
            \\
        , .{options.target.result});
        @panic("Unsupported compilation target");
    };
    run.addArgs(&.{ "-target", @tagName(dcc_triple) });

    const is_debug = options.optimize == .Debug;
    const do_bounds_check = options.bounds_check orelse (is_debug or options.optimize == .ReleaseSafe);
    const do_emit_debug = options.emit_debug_info orelse is_debug;
    const do_omit_fp = options.omit_frame_pointer orelse !is_debug;

    if (do_bounds_check) run.addArg("-fbounds-check");

    if (do_omit_fp) {
        run.addArg("-fomit-frame-pointer");
    } else run.addArg("-fno-omit-frame-pointer");

    if (do_emit_debug) {
        if (options.debug_format) |fmt| {
            run.addArg(b.fmt("-g{s}", .{@tagName(fmt)}));
        } else run.addArg("-g3");
    } else run.addArg("-g0");

    if (options.libdcext) run.addArg("-flibdcext");
    if (options.position_independent_code) run.addArg("-fPIC");
    if (options.no_red_zone) run.addArg("-fno-red-zone");
    if (options.no_simd) run.addArg("-fno-simd");
    if (options.no_x87) run.addArg("-fno-x87");
    if (options.no_stack_protector) run.addArg("-fno-stack-protector");
    if (options.no_stack_probe) run.addArg("-fno-stack-probe");

    if (options.code_model) |model| {
        run.addArgs(&.{ "-mcmodel", @tagName(model) });
    }

    for (options.include_dirs) |dir| {
        run.addArg("-I");
        run.addDirectoryArg(dir);
    }

    if (options.compile_only) run.addArg("-c");
    run.addFileArg(options.source_file);
    run.addArg("-o");

    const ext = if (options.compile_only) ".o" else if (options.target.result.os.tag == .windows) ".exe" else "";
    const out_name = b.fmt("{s}{s}", .{ options.name, ext });

    const out_file = run.addOutputFileArg(out_name);

    return .{
        .step = run,
        .output_file = out_file,
    };
}
