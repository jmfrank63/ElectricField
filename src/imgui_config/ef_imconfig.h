#pragma once
// Dear ImGui user config (IMGUI_USER_CONFIG) for EVERY translation unit that includes imgui.h or builds the ImGui sources.
// IM_ASSERT must never open a CRT dialog: the default is the C `assert`, which on Windows (GUI subsystem, CRT) pops a modal
// "Assertion failed" message box, and it is compiled out entirely under NDEBUG (ReleaseFast), turning API misuse into silent
// memory corruption. This one writes file:line:expression to stderr and terminates the process with exit code 3, in every build mode.
//
// Mode-dependent reporting: stderr is invisible in the Windows GUI-subsystem application, so the interactive app installs
// `ef_imgui_assert_hook` (src/gui/main.cpp) which shows ONE message box with the failed expression before the process exits.
// Headless modes (--smoke, --screenshot), the unit tests and ef-cli never install it: they stay non-interactive (stderr + exit 3),
// which the render-context regression tests rely on.
#include <cstdio>
#include <cstdlib>
#include <string>

// Called with the full report text just before exit; installed only by the interactive application.
inline void (*ef_imgui_assert_hook)(const char* report) = nullptr;

[[noreturn]] inline void ef_imgui_assert_fail(const char* expr, const char* file, int line) {
  std::fprintf(stderr, "IM_ASSERT failed: %s:%d: %s\n", file, line, expr);
  std::fflush(stderr);
  if (void (*hook)(const char*) = ef_imgui_assert_hook) {
    ef_imgui_assert_hook = nullptr;  // never recurse if the hook itself trips an assert
    const std::string report = std::string("Electric Field hit an internal error and must close.\n\n") + file + ":" + std::to_string(line) + "\n" + expr;
    hook(report.c_str());
  }
  std::_Exit(3);  // no atexit handlers, no CRT abort() message box, no Windows Error Reporting dialog
}

#define IM_ASSERT(_EXPR) ((void)((_EXPR) ? 0 : (ef_imgui_assert_fail(#_EXPR, __FILE__, __LINE__), 0)))
