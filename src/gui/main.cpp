// Electric Field GUI entry point.
//   ElectricField                      interactive application
//   ElectricField --smoke              headless start-up + tab lifecycle + UI-surface check (CI)
//   ElectricField --screenshot <slug> <out.png> [--size WxH] [--scale k] [--modal about|chooser]   headless render of one finished tab
//   ElectricField --export <dir>       headless PNG + SVG export of the finished Dipole (CI parity check)
//   ElectricField --script <file> [--user-dir <dir>] [--out <results>] [--lenient]   headless command-API runner (v2.4)
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
// Windows Error Reporting dialog. (IM_ASSERT is already non-interactive here, see src/imgui_config/ef_imconfig.h.)
void quietCrashDialogs() {
#ifdef _WIN32
  SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
  _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
  _set_error_mode(_OUT_TO_STDERR);
#endif
}

// Interactive mode only: IM_ASSERT terminates the process (ef_imconfig.h), and stderr is invisible in the Windows GUI subsystem, so
// tell the user once instead of vanishing silently.
void interactiveAssertReport(const char* report) {
  SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Electric Field", report, nullptr);
}

const char* const kUsage =
    "Usage:\n"
    "  ElectricField                                  interactive application\n"
    "  ElectricField --smoke                          headless start-up check\n"
    "  ElectricField --screenshot <slug> <out.png> [--size WxH] [--scale k] [--modal about|chooser]\n"
    "  ElectricField --export <dir>                   headless PNG + SVG export of the finished Dipole\n"
    "  ElectricField --script <file> [--user-dir <dir>] [--out <results>] [--lenient]\n"
    "                                                 headless command-API runner (see docs/api.md)\n";

// Malformed command line: usage on stderr, exit code 2. The message box (the only channel on the Windows GUI subsystem, which has no
// console) is shown only for the interactive mode; any invocation that names a headless mode must never block on a modal box.
int usageError(const std::string& problem, bool headlessRequested) {
  const std::string text = problem + "\n\n" + kUsage;
  std::fprintf(stderr, "%s", text.c_str());
  if (!headlessRequested) SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Electric Field", text.c_str(), nullptr);
  return 2;
}

int run(int argc, char** argv) {
  efgui::AppOptions opt;
  bool headlessRequested = false;  // even a malformed headless invocation counts: it must not show a message box
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--smoke") == 0 || std::strcmp(argv[i], "--screenshot") == 0 || std::strcmp(argv[i], "--export") == 0 ||
        std::strcmp(argv[i], "--script") == 0)
      headlessRequested = true;
  }
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    if (a == "--smoke") {
      opt.smoke = true;
    } else if (a == "--script") {
      if (i + 1 >= argc) return usageError("--script needs <file>.", headlessRequested);
      opt.script = true;
      opt.scriptFile = argv[++i];
    } else if (a == "--user-dir") {
      if (i + 1 >= argc) return usageError("--user-dir needs <dir>.", headlessRequested);
      opt.userDir = argv[++i];
    } else if (a == "--lenient") {
      opt.lenient = true;
    } else if (a == "--export") {
      if (i + 1 >= argc) return usageError("--export needs <dir>.", headlessRequested);
      opt.exportMode = true;
      opt.exportDir = argv[++i];
    } else if (a == "--screenshot") {
      if (i + 2 >= argc) return usageError("--screenshot needs <slug> and <out.png>.", headlessRequested);
      opt.screenshot = true;
      opt.slug = argv[++i];
      opt.out = argv[++i];
    } else if (a == "--size") {
      if (i + 1 >= argc) return usageError("--size needs WxH.", headlessRequested);
      int w = 0, h = 0;
      if (std::sscanf(argv[++i], "%dx%d", &w, &h) != 2 || w <= 0 || h <= 0)
        return usageError(std::string("Invalid --size \"") + argv[i] + "\" (expected WxH, e.g. 1024x820).", headlessRequested);
      opt.width = w;
      opt.height = h;
    } else if (a == "--modal") {
      if (i + 1 >= argc) return usageError("--modal needs about or chooser.", headlessRequested);
      opt.modal = argv[++i];
      if (opt.modal != "about" && opt.modal != "chooser")
        return usageError("Invalid --modal \"" + opt.modal + "\" (expected about or chooser).", headlessRequested);
    } else if (a == "--scale") {
      if (i + 1 >= argc) return usageError("--scale needs a factor.", headlessRequested);
      char* endp = nullptr;
      const double k = std::strtod(argv[++i], &endp);
      if (endp == argv[i] || *endp != 0 || !(k > 0.0))
        return usageError(std::string("Invalid --scale \"") + argv[i] + "\" (expected a number > 0).", headlessRequested);
      opt.scaleOverride = static_cast<float>(k);
    } else if (a == "--out") {
      if (i + 1 >= argc) return usageError("--out needs <file>.", headlessRequested);
      opt.resultsFile = argv[++i];
    } else if (a.rfind("-psn_", 0) == 0) {
      // macOS passes a process serial number to apps launched from Finder; not an option
    } else {
      return usageError("Unknown option \"" + a + "\".", headlessRequested);
    }
  }
  if (opt.headless())
    quietCrashDialogs();
  else
    ef_imgui_assert_hook = interactiveAssertReport;

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
  if (opt.exportMode) return app.runExportMode();
  if (opt.script) return app.runScript();
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
