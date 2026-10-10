#pragma once
// Dear ImGui user config (IMGUI_USER_CONFIG) for EVERY translation unit that includes imgui.h or builds the ImGui sources.
// IM_ASSERT must never open a dialog: the default is the C `assert`, which on Windows (GUI subsystem, CRT) pops a modal
// "Assertion failed" message box, and it is compiled out entirely under NDEBUG (ReleaseFast), turning API misuse into silent
// memory corruption. This one writes file:line:expression to stderr and terminates the process with exit code 3, in every build mode.
#include <cstdio>
#include <cstdlib>

[[noreturn]] inline void ef_imgui_assert_fail(const char* expr, const char* file, int line) {
  std::fprintf(stderr, "IM_ASSERT failed: %s:%d: %s\n", file, line, expr);
  std::fflush(stderr);
  std::_Exit(3);  // no atexit handlers, no CRT abort() message box, no Windows Error Reporting dialog
}

#define IM_ASSERT(_EXPR) ((void)((_EXPR) ? 0 : (ef_imgui_assert_fail(#_EXPR, __FILE__, __LINE__), 0)))
