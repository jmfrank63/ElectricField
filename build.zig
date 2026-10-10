const std = @import("std");

// Core flags: numerics policy (D-04) + strict warnings as errors. Applied to the physics core AND the tests.
const core_flags = [_][]const u8{
    "-std=c++17",
    "-ffp-contract=off", // never fuse a*b+c (aarch64 would otherwise differ from the MSVC x64 reference)
    "-Wall",
    "-Wextra",
    "-Wshadow",
    "-Wabsolute-value",
    "-Wconversion",
    "-Wsign-conversion",
    "-Wuninitialized",
    "-pedantic",
    "-Werror",
};

// GUI / third-party code: no -Werror. -DImDrawIdx keeps 32-bit indices (field-line meshes exceed 64k vertices);
// the define must be identical in every TU that includes imgui.h.
const gui_flags = [_][]const u8{ "-std=c++17", "-DImDrawIdx=unsigned int" };

const core_sources = [_][]const u8{
    "ef/seeds.cpp",
    "ef/presets.cpp",
    "ef/finalizer.cpp",
    "ef/session.cpp",
};

const test_sources = [_][]const u8{
    "test_main.cpp",
    "test_vec2.cpp",
    "test_field.cpp",
    "test_rk_order.cpp",
    "test_seeds.cpp",
    "test_presets.cpp",
    "test_session.cpp",
};

const Sanitize = enum { none, thread };

const Ctx = struct {
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    tsan: bool,
};

pub fn build(b: *std.Build) void {
    const ctx = Ctx{
        .target = b.standardTargetOptions(.{}),
        .optimize = b.standardOptimizeOption(.{}),
        .tsan = (b.option(Sanitize, "sanitize", "Sanitizer for the core and tests (none | thread). Thread = ThreadSanitizer, Linux/macOS only") orelse .none) == .thread,
    };
    const with_gui = b.option(bool, "gui", "Build the SDL3 + Dear ImGui targets (default: true; -Dgui=false = core + tests only, fetches no GUI dependency)") orelse true;

    const core_lib = addCore(b, ctx);
    addTests(b, ctx, core_lib);

    var gui: ?GuiDeps = null;
    if (with_gui) gui = addGuiDeps(b, ctx); // null while the lazy dependencies are not fetched yet (Zig re-runs build() after the fetch)
    if (gui) |g| addGui(b, ctx, core_lib, g);
}

fn coreModule(b: *std.Build, ctx: Ctx) *std.Build.Module {
    return b.createModule(.{
        .target = ctx.target,
        .optimize = ctx.optimize,
        .link_libcpp = true,
        .sanitize_thread = if (ctx.tsan) true else null,
    });
}

/// efcore: GUI-free, std-only C++17 static library.
fn addCore(b: *std.Build, ctx: Ctx) *std.Build.Step.Compile {
    const mod = coreModule(b, ctx);
    mod.addCSourceFiles(.{ .root = b.path("src/core"), .files = &core_sources, .flags = &core_flags });
    mod.addIncludePath(b.path("src/core"));
    const lib = b.addLibrary(.{ .name = "efcore", .root_module = mod, .linkage = .static });
    b.installArtifact(lib);
    return lib;
}

/// `zig build test`: doctest suite (no SDL/ImGui involved).
fn addTests(b: *std.Build, ctx: Ctx, core_lib: *std.Build.Step.Compile) void {
    const doctest_dep = b.dependency("doctest", .{});
    const mod = coreModule(b, ctx);
    mod.addCSourceFiles(.{ .root = b.path("tests"), .files = &test_sources, .flags = &core_flags });
    mod.addIncludePath(b.path("src/core"));
    mod.addSystemIncludePath(doctest_dep.path(".")); // -isystem: doctest's own warnings are not ours
    mod.linkLibrary(core_lib);
    const exe = b.addExecutable(.{ .name = "efcore-tests", .root_module = mod });
    const run = b.addRunArtifact(exe);
    run.setCwd(b.path(".")); // test_presets.cpp reads ElectricField/ConfigDocs.cpp relative to the repo root
    if (ctx.tsan) run.setEnvironmentVariable("TSAN_OPTIONS", "halt_on_error=1:second_deadlock_stack=1");
    b.step("test", "Run the physics-core tests (doctest)").dependOn(&run.step);
}

const GuiDeps = struct {
    sdl_lib: *std.Build.Step.Compile,
    imgui_lib: *std.Build.Step.Compile,
    imgui_path: std.Build.LazyPath,
};

/// SDL3 (castholm/SDL) + Dear ImGui + sdlrenderer3 backend. Both are `.lazy = true` in build.zig.zon (D-14): they are
/// only fetched when this function actually asks for them, i.e. never with -Dgui=false.
fn addGuiDeps(b: *std.Build, ctx: Ctx) ?GuiDeps {
    const sdl_dep = b.lazyDependency("sdl3", .{ .target = ctx.target, .optimize = ctx.optimize }) orelse return null;
    const imgui_dep = b.lazyDependency("imgui", .{}) orelse return null;
    const sdl_lib = sdl_dep.artifact("SDL3");

    const imgui_mod = b.createModule(.{ .target = ctx.target, .optimize = ctx.optimize, .link_libcpp = true });
    imgui_mod.addCSourceFiles(.{
        .root = imgui_dep.path("."),
        .files = &.{
            "imgui.cpp",
            "imgui_draw.cpp",
            "imgui_tables.cpp",
            "imgui_widgets.cpp",
            "backends/imgui_impl_sdl3.cpp",
            "backends/imgui_impl_sdlrenderer3.cpp",
        },
        .flags = &gui_flags,
    });
    imgui_mod.addIncludePath(imgui_dep.path("."));
    imgui_mod.addIncludePath(imgui_dep.path("backends"));
    imgui_mod.linkLibrary(sdl_lib);
    const imgui_lib = b.addLibrary(.{ .name = "imgui", .root_module = imgui_mod, .linkage = .static });
    return .{ .sdl_lib = sdl_lib, .imgui_lib = imgui_lib, .imgui_path = imgui_dep.path(".") };
}

fn addGui(b: *std.Build, ctx: Ctx, core_lib: *std.Build.Step.Compile, g: GuiDeps) void {
    const mod = b.createModule(.{ .target = ctx.target, .optimize = ctx.optimize, .link_libcpp = true });
    mod.addCSourceFiles(.{ .root = b.path("src/gui"), .files = &.{"main.cpp"}, .flags = &gui_flags });
    mod.addIncludePath(b.path("src/core"));
    mod.addIncludePath(g.imgui_path);
    mod.addIncludePath(g.imgui_path.path(b, "backends"));
    mod.linkLibrary(core_lib);
    mod.linkLibrary(g.imgui_lib);
    mod.linkLibrary(g.sdl_lib);
    const exe = b.addExecutable(.{ .name = "ElectricField", .root_module = mod });
    exe.subsystem = .windows; // GUI subsystem on Windows (needs <SDL3/SDL_main.h> + int main(int, char**)); ignored elsewhere
    b.installArtifact(exe);

    const run = b.addRunArtifact(exe);
    run.step.dependOn(b.getInstallStep());
    run.addPassthruArgs(); // 0.17: replaces `b.args`
    b.step("run", "Run the hello-window GUI").dependOn(&run.step);

    // `zig build smoke`: headless start-up check (dummy video driver + software renderer), exits 0 after 2 frames.
    const smoke = b.addRunArtifact(exe);
    smoke.addArg("--smoke");
    smoke.setEnvironmentVariable("SDL_VIDEODRIVER", "dummy");
    smoke.setEnvironmentVariable("SDL_RENDER_DRIVER", "software");
    b.step("smoke", "Headless start-up check of the GUI (CI)").dependOn(&smoke.step);
}
