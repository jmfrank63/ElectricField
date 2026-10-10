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
const gui_flags = [_][]const u8{ "-std=c++17", "-DImDrawIdx=unsigned int", "-DIMGUI_USER_CONFIG=\"ef_imconfig.h\"" };

const core_sources = [_][]const u8{
    "ef/seeds.cpp",
    "ef/presets.cpp",
    "ef/finalizer.cpp",
    "ef/session.cpp",
    "ef/scene.cpp",
    "ef/svg.cpp",
};

const app_sources = [_][]const u8{
    "app/workspace.cpp",
    "app/ui_logic.cpp",
};

const test_sources = [_][]const u8{
    "test_main.cpp",
    "test_app.cpp",
    "test_vec2.cpp",
    "test_field.cpp",
    "test_rk_order.cpp",
    "test_seeds.cpp",
    "test_presets.cpp",
    "test_tracer.cpp",
    "test_bezier.cpp",
    "test_finalizer.cpp",
    "test_session.cpp",
    "test_scene.cpp",
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
        .tsan = (b.option(Sanitize, "sanitize", "Sanitizer for the core, tests and ef-cli (none | thread). Thread = ThreadSanitizer, Linux/macOS only") orelse .none) == .thread,
    };
    const with_gui = b.option(bool, "gui", "Build the SDL3 + Dear ImGui targets and PNG export (default: true; -Dgui=false = core + tests + SVG-only ef-cli, fetches no GUI dependency)") orelse true;

    const core_lib = addCore(b, ctx);
    const app_lib = addApp(b, ctx, core_lib);
    const test_step = addTests(b, ctx, core_lib, app_lib);

    var gui: ?GuiDeps = null;
    if (with_gui) gui = addGuiDeps(b, ctx); // null while the lazy dependencies are not fetched yet (Zig re-runs build() after the fetch)
    const render_lib: ?*std.Build.Step.Compile = if (gui) |g| addRender(b, ctx, g) else null;
    addCli(b, ctx, core_lib, render_lib);
    if (gui) |g| addRenderTests(b, ctx, test_step, core_lib, render_lib.?, g);
    if (gui) |g| addGui(b, ctx, core_lib, app_lib, render_lib.?, g);
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

/// efapp: std-only app logic (tab model over ComputeSession + pure UI decisions). Same strict flags as the core, no SDL/ImGui.
fn addApp(b: *std.Build, ctx: Ctx, core_lib: *std.Build.Step.Compile) *std.Build.Step.Compile {
    const mod = coreModule(b, ctx);
    mod.addCSourceFiles(.{ .root = b.path("src"), .files = &app_sources, .flags = &core_flags });
    mod.addIncludePath(b.path("src/core"));
    mod.addIncludePath(b.path("src"));
    mod.linkLibrary(core_lib);
    return b.addLibrary(.{ .name = "efapp", .root_module = mod, .linkage = .static });
}

/// `zig build test`: doctest suite (no SDL/ImGui involved).
fn addTests(b: *std.Build, ctx: Ctx, core_lib: *std.Build.Step.Compile, app_lib: *std.Build.Step.Compile) *std.Build.Step {
    const doctest_dep = b.dependency("doctest", .{});
    const mod = coreModule(b, ctx);
    mod.addCSourceFiles(.{ .root = b.path("tests"), .files = &test_sources, .flags = &core_flags });
    mod.addIncludePath(b.path("src/core"));
    mod.addIncludePath(b.path("src"));
    mod.addSystemIncludePath(doctest_dep.path(".")); // -isystem: doctest's own warnings are not ours
    mod.linkLibrary(app_lib);
    mod.linkLibrary(core_lib);
    const exe = b.addExecutable(.{ .name = "efcore-tests", .root_module = mod });
    const run = b.addRunArtifact(exe);
    run.setCwd(b.path(".")); // test_presets.cpp reads ElectricField/ConfigDocs.cpp relative to the repo root
    if (ctx.tsan) run.setEnvironmentVariable("TSAN_OPTIONS", "halt_on_error=1:second_deadlock_stack=1");
    const step = b.step("test", "Run the unit tests (doctest): core + app logic; + render/context tests when the GUI deps are enabled");
    step.dependOn(&run.step);
    return step;
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
    imgui_mod.addIncludePath(b.path("src/imgui_config"));
    imgui_mod.addIncludePath(imgui_dep.path("."));
    imgui_mod.addIncludePath(imgui_dep.path("backends"));
    imgui_mod.linkLibrary(sdl_lib);
    const imgui_lib = b.addLibrary(.{ .name = "imgui", .root_module = imgui_mod, .linkage = .static });
    return .{ .sdl_lib = sdl_lib, .imgui_lib = imgui_lib, .imgui_path = imgui_dep.path(".") };
}

/// efrender: ImGuiPainter + offscreen software-renderer export (shared by ef-cli and the GUI self-checks).
fn addRender(b: *std.Build, ctx: Ctx, g: GuiDeps) *std.Build.Step.Compile {
    const render = b.createModule(.{ .target = ctx.target, .optimize = ctx.optimize, .link_libcpp = true });
    render.addCSourceFiles(.{ .root = b.path("src/render"), .files = &.{"png_export.cpp"}, .flags = &gui_flags });
    render.addIncludePath(b.path("src/core"));
    render.addIncludePath(b.path("src"));
    render.addIncludePath(b.path("src/imgui_config"));
    render.addIncludePath(g.imgui_path);
    render.addIncludePath(g.imgui_path.path(b, "backends"));
    render.linkLibrary(g.imgui_lib);
    render.linkLibrary(g.sdl_lib);
    return b.addLibrary(.{ .name = "efrender", .root_module = render, .linkage = .static });
}

const cli_flags = core_flags;
const cli_flags_png = core_flags ++ [_][]const u8{"-DEF_CLI_PNG=1"};

/// ef-cli: headless preset renderer. SVG always; PNG (ImGui ImDrawList on SDL's software renderer) when the GUI deps are present.
fn addCli(b: *std.Build, ctx: Ctx, core_lib: *std.Build.Step.Compile, render_lib: ?*std.Build.Step.Compile) void {
    const mod = coreModule(b, ctx);
    mod.addIncludePath(b.path("src/core"));
    mod.addIncludePath(b.path("src"));
    mod.linkLibrary(core_lib);
    if (render_lib) |r| {
        mod.addCSourceFiles(.{ .root = b.path("src/cli"), .files = &.{"main.cpp"}, .flags = &cli_flags_png });
        mod.linkLibrary(r);
    } else {
        mod.addCSourceFiles(.{ .root = b.path("src/cli"), .files = &.{"main.cpp"}, .flags = &cli_flags });
    }
    const exe = b.addExecutable(.{ .name = "ef-cli", .root_module = mod });
    b.installArtifact(exe);
}

const zon = @import("build.zig.zon");

/// Directory holding a generated `ef_git_hash.h` with one line, `#define EF_GIT_HASH "<7 hex>"`, for the About box.
/// The hash is produced when the step runs (make time, `has_side_effects`), not when build.zig is configured, because
/// Zig caches the configure phase and a configure-time hash would go stale after the next commit. Without git, a
/// repository or a commit the header defines the empty string (the About box then shows "(unknown)") and the build
/// still succeeds. The configure-time probe and the make-time step run the IDENTICAL command in the SAME directory (the
/// build root), so a probe success cannot be followed by a make-time failure (an unrelated repository in the process cwd,
/// or git refusing the build root, e.g. safe.directory, makes the probe fail and select the empty hash).
fn gitHashHeader(b: *std.Build) std.Build.LazyPath {
    const argv = [_][]const u8{ "git", "-c", "core.abbrev=7", "log", "-1", "--format=#define EF_GIT_HASH \"%h\"" };
    const wf = b.addWriteFiles();
    switch (b.runFallible(&argv, .{ .stderr_behavior = .ignore, .cwd = .{ .dir = b.root.root_dir.handle } })) {
        .success => {
            const run = b.addSystemCommand(&argv);
            run.setCwd(b.path("."));
            run.has_side_effects = true; // re-run on every build so the hash follows HEAD
            _ = wf.addCopyFile(run.captureStdOut(.{ .basename = "ef_git_hash.h" }), "ef_git_hash.h");
        },
        else => _ = wf.add("ef_git_hash.h", "#define EF_GIT_HASH \"\"\n"),
    }
    return wf.getDirectory();
}

fn addGui(b: *std.Build, ctx: Ctx, core_lib: *std.Build.Step.Compile, app_lib: *std.Build.Step.Compile, render_lib: *std.Build.Step.Compile, g: GuiDeps) void {
    const info = b.addConfigHeader(.{ .style = .blank, .include_path = "ef_build_info.h" }, .{
        .EF_VERSION = zon.version,
    });
    const mod = b.createModule(.{ .target = ctx.target, .optimize = ctx.optimize, .link_libcpp = true });
    mod.addCSourceFiles(.{ .root = b.path("src/gui"), .files = &.{ "main.cpp", "app.cpp", "ui.cpp" }, .flags = &gui_flags });
    mod.addCSourceFiles(.{ .root = b.path("src/gui"), .files = &.{"icon.cpp"}, .flags = &(gui_flags ++ [_][]const u8{"-Wno-c23-extensions"}) });
    mod.addIncludePath(b.path("src"));
    mod.addIncludePath(b.path("src/core"));
    mod.addConfigHeader(info);
    mod.addIncludePath(gitHashHeader(b)); // ef_git_hash.h, regenerated at make time
    mod.addIncludePath(b.path("src/imgui_config"));
    mod.addIncludePath(g.imgui_path);
    mod.addIncludePath(g.imgui_path.path(b, "backends"));
    mod.linkLibrary(render_lib);
    mod.linkLibrary(app_lib);
    mod.linkLibrary(core_lib);
    mod.linkLibrary(g.imgui_lib);
    mod.linkLibrary(g.sdl_lib);
    mod.addWin32ResourceFile(.{ .file = b.path("assets/ElectricField.rc") }); // exe icon; ignored off Windows
    const exe = b.addExecutable(.{ .name = "ElectricField", .root_module = mod });
    exe.subsystem = .windows; // GUI subsystem on Windows (needs <SDL3/SDL_main.h> + int main(int, char**)); ignored elsewhere
    b.installArtifact(exe);

    const run = b.addRunArtifact(exe);
    run.step.dependOn(b.getInstallStep());
    run.addPassthruArgs(); // 0.17: replaces `b.args`
    b.step("run", "Run the GUI").dependOn(&run.step);

    // `zig build smoke`: headless start-up + tab lifecycle check (dummy video driver + software renderer).
    const smoke = b.addRunArtifact(exe);
    smoke.addArg("--smoke");
    smoke.setEnvironmentVariable("SDL_VIDEODRIVER", "dummy");
    smoke.setEnvironmentVariable("SDL_RENDER_DRIVER", "software");
    b.step("smoke", "Headless start-up and tab lifecycle check of the GUI (CI)").dependOn(&smoke.step);
}

/// Headless render/context regression tests (need SDL3 + ImGui, so they exist only with -Dgui=true). Also run by `zig build test`.
fn addRenderTests(b: *std.Build, ctx: Ctx, test_step: *std.Build.Step, core_lib: *std.Build.Step.Compile, render_lib: *std.Build.Step.Compile, g: GuiDeps) void {
    const doctest_dep = b.dependency("doctest", .{});
    const mod = b.createModule(.{ .target = ctx.target, .optimize = ctx.optimize, .link_libcpp = true });
    mod.addCSourceFiles(.{ .root = b.path("tests_gui"), .files = &.{ "test_main.cpp", "test_render_ctx.cpp" }, .flags = &gui_flags });
    mod.addIncludePath(b.path("src/core"));
    mod.addIncludePath(b.path("src"));
    mod.addIncludePath(b.path("tests"));
    mod.addIncludePath(b.path("src/imgui_config"));
    mod.addSystemIncludePath(doctest_dep.path("."));
    mod.addIncludePath(g.imgui_path);
    mod.addIncludePath(g.imgui_path.path(b, "backends"));
    mod.linkLibrary(render_lib);
    mod.linkLibrary(core_lib);
    mod.linkLibrary(g.imgui_lib);
    mod.linkLibrary(g.sdl_lib);
    const exe = b.addExecutable(.{ .name = "efrender-tests", .root_module = mod });
    const run = b.addRunArtifact(exe);
    run.setEnvironmentVariable("SDL_VIDEODRIVER", "dummy");
    run.setEnvironmentVariable("SDL_RENDER_DRIVER", "software");
    const step = b.step("test-render", "Run the headless render/ImGui-context regression tests");
    step.dependOn(&run.step);
    test_step.dependOn(&run.step);
}
