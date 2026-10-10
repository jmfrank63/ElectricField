// Electric Field GUI entry point.
//   ElectricField                      interactive application
//   ElectricField --smoke              headless start-up + tab lifecycle + UI-surface check (CI)
//   ElectricField --screenshot <slug> <out.png> [--size WxH] [--scale k] [--modal about|chooser]   headless render of one finished tab
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>  // Windows GUI subsystem: must be in the TU that defines main

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <string>

#include "gui/app.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

// Headless modes (smoke, screenshot) run unattended in CI and on developer desktops: no CRT assert / abort message box, no
// Windows Error Reporting dialog. (IM_ASSERT is already non-interactive, see src/imgui_config/ef_imconfig.h.)
void quietCrashDialogs() {
#ifdef _WIN32
  SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
  _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
  _set_error_mode(_OUT_TO_STDERR);
#endif
}

int run(int argc, char** argv) {
  efgui::AppOptions opt;
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--smoke") == 0) {
      opt.smoke = true;
    } else if (std::strcmp(argv[i], "--screenshot") == 0 && i + 2 < argc) {
      opt.screenshot = true;
      opt.slug = argv[++i];
      opt.out = argv[++i];
    } else if (std::strcmp(argv[i], "--size") == 0 && i + 1 < argc) {
      std::sscanf(argv[++i], "%dx%d", &opt.width, &opt.height);
    } else if (std::strcmp(argv[i], "--modal") == 0 && i + 1 < argc) {
      opt.modal = argv[++i];
    } else if (std::strcmp(argv[i], "--scale") == 0 && i + 1 < argc) {
      opt.scaleOverride = static_cast<float>(std::atof(argv[++i]));
    }
  }
  if (opt.headless()) quietCrashDialogs();

  efgui::App app;
  std::string step, message;
  if (!app.init(opt, step, message)) {
    const std::string text = "Electric Field could not start.\n" + step + ": " + message;
    std::fprintf(stderr, "%s\n", text.c_str());
    // No console on Windows (GUI subsystem): the message box is the only way the user learns why nothing appeared. Never in the
    // headless modes (CI, tests): a modal box would hang them.
    if (!opt.headless()) SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Electric Field", text.c_str(), nullptr);
    return 1;
  }
  if (opt.smoke) return app.runSmoke();
  if (opt.screenshot) return app.runScreenshot();
  return app.runInteractive();
}
}  // namespace

int main(int argc, char** argv) {
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  try {
    return run(argc, argv);
  } catch (const std::exception& e) {
    std::fprintf(stderr, "fatal: %s\n", e.what());
  } catch (...) {
    std::fprintf(stderr, "fatal: unknown exception\n");
  }
  return 1;
}
