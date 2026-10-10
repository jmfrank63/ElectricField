// ef-cli: headless renderer / CI parity oracle (D-10..D-12).
//   ef-cli --list
//   ef-cli --preset <slug> [--svg out.svg] [--png out.png] [--threads N] [--method <name>] [--width W --height H]
//   ef-cli --all --out-dir DIR [--expect tools/parity/expected-counts.csv]
// Prints one summary line per rendered preset: preset=<slug> charges=N field_lines=N equipotentials=N ms=N
#include <algorithm>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "ef/method.hpp"
#include "ef/presets.hpp"
#include "ef/scene.hpp"
#include "ef/session.hpp"
#include "ef/svg.hpp"
#ifdef EF_CLI_PNG
#include "render/png_export.hpp"
#endif

namespace {

struct Args {
  bool list = false, all = false;
  std::string preset, svg, png, outDir, expect, methodName = "euler";
  unsigned threads = 4;
  int width = 1024, height = 768;
};

struct Counts {
  int charges = 0, fieldLines = 0, equipotentials = 0;
};

const ef::Preset* findPreset(const std::string& slug) {
  for (const auto& p : ef::presets())
    if (p.slug == slug) return &p;
  return nullptr;
}

// Strict integer parse: whole string, no sign-less wraparound, no overflow, within [lo, hi].
bool parseInt(const std::string& text, long long lo, long long hi, long long& out) {
  if (text.empty() || std::isspace(static_cast<unsigned char>(text[0]))) return false;  // strtoll would skip leading blanks
  errno = 0;
  char* end = nullptr;
  const long long v = std::strtoll(text.c_str(), &end, 10);
  if (errno == ERANGE || end == text.c_str() || *end != '\0' || v < lo || v > hi) return false;
  out = v;
  return true;
}

bool parseArgs(int argc, char** argv, Args& a, std::string& err) {
  for (int i = 1; i < argc; ++i) {
    const std::string k = argv[i];
    auto next = [&](std::string& v) {
      if (i + 1 >= argc) { err = "missing value for " + k; return false; }
      v = argv[++i];
      return true;
    };
    auto nextInt = [&](long long lo, long long hi, long long& out) {
      std::string v;
      if (!next(v)) return false;
      if (!parseInt(v, lo, hi, out)) {
        err = "invalid value '" + v + "' for " + k + " (expected an integer in " + std::to_string(lo) + ".." + std::to_string(hi) + ")";
        return false;
      }
      return true;
    };
    long long n = 0;
    if (k == "--list") a.list = true;
    else if (k == "--all") a.all = true;
    else if (k == "--preset") { if (!next(a.preset)) return false; }
    else if (k == "--svg") { if (!next(a.svg)) return false; }
    else if (k == "--png") { if (!next(a.png)) return false; }
    else if (k == "--out-dir") { if (!next(a.outDir)) return false; }
    else if (k == "--expect") { if (!next(a.expect)) return false; }
    else if (k == "--method") { if (!next(a.methodName)) return false; }
    else if (k == "--threads") { if (!nextInt(1, 128, n)) return false; a.threads = static_cast<unsigned>(n); }
    else if (k == "--width") { if (!nextInt(16, 8192, n)) return false; a.width = static_cast<int>(n); }
    else if (k == "--height") { if (!nextInt(16, 8192, n)) return false; a.height = static_cast<int>(n); }
    else { err = "unknown argument " + k; return false; }
  }
  // Reject contradictory combinations instead of silently ignoring half of the command line.
  if (a.list && (a.all || !a.preset.empty() || !a.svg.empty() || !a.png.empty() || !a.outDir.empty() || !a.expect.empty())) {
    err = "--list cannot be combined with other output/selection options";
    return false;
  }
  if (a.all && (!a.preset.empty() || !a.svg.empty() || !a.png.empty())) {
    err = "--all renders every preset into --out-dir; it cannot be combined with --preset/--svg/--png";
    return false;
  }
  if (!a.all && (!a.expect.empty() || !a.outDir.empty()) && !a.list) {
    err = "--expect and --out-dir are only valid together with --all";
    return false;
  }
  return true;
}

bool renderOne(const ef::Preset& preset, const Args& a, const std::string& svgPath, const std::string& pngPath, Counts& counts, std::string& err) {
  ef::ComputeParams params;  // MFC defaults
  params.numThreads = a.threads;
  const auto m = ef::methodFromName(a.methodName);
  if (!m) { err = "unknown method '" + a.methodName + "'"; return false; }
  params.method = *m;

  const ef::ChargeSystem field = ef::makeSystem(preset);
  const auto t0 = std::chrono::steady_clock::now();
  ef::ComputeSession session(field, params);
  const ef::SessionStatus st = session.wait();
  ef::FieldResult result;
  if (st.failed) { err = "compute failed: " + st.error; return false; }
  if (!session.takeResult(result)) { err = "no result"; return false; }
  const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();

  counts = Counts{static_cast<int>(field.charges.size()), static_cast<int>(result.electric.size()), static_cast<int>(result.potential.size())};
  const ef::FieldScene scene = ef::buildScene(field, result);
  const ef::DrawStyle style;
  const ef::View view{a.width, a.height, 100.};

  if (!svgPath.empty()) {
    std::ofstream f(svgPath, std::ios::binary);
    if (!f) { err = "cannot open " + svgPath + " for writing"; return false; }
    f << ef::renderSvg(scene, style, view);
    f.close();  // flush before checking: disk-full / I/O errors only surface here
    if (!f) { err = "cannot write " + svgPath; return false; }
  }
  if (!pngPath.empty()) {
#ifdef EF_CLI_PNG
    const std::string e = efrender::renderPng(scene, style, view, pngPath.c_str());
    if (!e.empty()) { err = e; return false; }
#else
    err = "this ef-cli was built without PNG support (-Dgui=false)";
    return false;
#endif
  }
  std::printf("preset=%.*s charges=%d field_lines=%d equipotentials=%d ms=%lld\n", static_cast<int>(preset.slug.size()), preset.slug.data(),
              counts.charges, counts.fieldLines, counts.equipotentials, static_cast<long long>(ms));
  return true;
}

// expected-counts.csv: slug,charges,field_lines,equipotentials ; charges and field_lines exact, equipotentials +-max(2, 3%)
// Strict: every data row must have exactly four fully-parsed fields, no slug may repeat or be unknown, and the set of
// slugs in the file must equal the set of rendered presets (a duplicate row can no longer mask a missing preset).
bool checkExpected(const std::string& path, const std::map<std::string, Counts>& got, std::string& err) {
  std::ifstream in(path);
  if (!in) { err = "cannot open " + path; return false; }
  std::string line;
  bool ok = true;
  std::set<std::string> seen;
  std::size_t lineNo = 0;
  while (std::getline(in, line)) {
    ++lineNo;
    while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) line.pop_back();  // CRLF checkouts
    if (line.empty() || line[0] == '#' || line.rfind("slug", 0) == 0) continue;
    std::vector<std::string> f;
    {
      std::istringstream ss(line);
      std::string field;
      while (std::getline(ss, field, ',')) f.push_back(field);
      if (!line.empty() && line.back() == ',') f.emplace_back();  // trailing empty field
    }
    long long v[3] = {0, 0, 0};
    if (f.size() != 4 || f[0].empty() || !parseInt(f[1], 0, 1000000, v[0]) || !parseInt(f[2], 0, 1000000, v[1]) ||
        !parseInt(f[3], 0, 1000000, v[2])) {
      std::fprintf(stderr, "PARITY FAIL: %s:%zu: malformed row '%s' (expected slug,charges,field_lines,equipotentials)\n", path.c_str(), lineNo,
                   line.c_str());
      ok = false;
      continue;
    }
    const std::string& slug = f[0];
    const auto it = got.find(slug);
    if (it == got.end()) {
      std::fprintf(stderr, "PARITY FAIL: %s:%zu: unknown preset slug '%s'\n", path.c_str(), lineNo, slug.c_str());
      ok = false;
      continue;
    }
    if (!seen.insert(slug).second) {
      std::fprintf(stderr, "PARITY FAIL: %s:%zu: duplicate row for preset '%s'\n", path.c_str(), lineNo, slug.c_str());
      ok = false;
      continue;
    }
    const int ec = static_cast<int>(v[0]), ef_ = static_cast<int>(v[1]), ee = static_cast<int>(v[2]);
    const int tol = std::max(2, ee * 3 / 100);
    const Counts& g = it->second;
    if (g.charges != ec || g.fieldLines != ef_ || std::abs(g.equipotentials - ee) > tol) {
      std::fprintf(stderr, "PARITY FAIL %s: got charges=%d field_lines=%d equipotentials=%d, expected %d/%d/%d (equipotential tol %d)\n", slug.c_str(),
                   g.charges, g.fieldLines, g.equipotentials, ec, ef_, ee, tol);
      ok = false;
    }
  }
  for (const auto& kv : got)
    if (seen.find(kv.first) == seen.end()) {
      std::fprintf(stderr, "PARITY FAIL: no expected row for preset '%s'\n", kv.first.c_str());
      ok = false;
    }
  return ok;
}

int run(int argc, char** argv) {
  Args a;
  std::string err;
  if (!parseArgs(argc, argv, a, err)) {
    std::fprintf(stderr, "ef-cli: %s\n", err.c_str());
    return 2;
  }
  if (a.list) {
    for (const auto& p : ef::presets()) std::printf("%.*s\n", static_cast<int>(p.slug.size()), p.slug.data());
    return 0;
  }
  if (a.all) {
    if (a.outDir.empty()) { std::fprintf(stderr, "ef-cli: --all needs --out-dir\n"); return 2; }
    std::map<std::string, Counts> got;
    for (const auto& p : ef::presets()) {
      Counts c;
      const std::string slug(p.slug);
      const std::string base = a.outDir + "/" + slug;
      if (!renderOne(p, a, base + ".svg",
#ifdef EF_CLI_PNG
                     base + ".png",
#else
                     std::string(),
#endif
                     c, err)) {
        std::fprintf(stderr, "ef-cli: %s: %s\n", slug.c_str(), err.c_str());
        return 1;
      }
      got[slug] = c;
    }
    if (!a.expect.empty() && !checkExpected(a.expect, got, err)) {
      if (!err.empty()) std::fprintf(stderr, "ef-cli: %s\n", err.c_str());
      return 1;
    }
    return 0;
  }
  const ef::Preset* p = findPreset(a.preset);
  if (!p) { std::fprintf(stderr, "ef-cli: unknown or missing --preset (see --list)\n"); return 2; }
  Counts c;
  if (!renderOne(*p, a, a.svg, a.png, c, err)) { std::fprintf(stderr, "ef-cli: %s\n", err.c_str()); return 1; }
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  // Anything thrown below (makeSystem, std::thread creation in ComputeSession, buildScene, renderSvg, allocations) must end
  // in a documented exit code, not std::terminate / a crash dialog.
  try {
    return run(argc, argv);
  } catch (const std::exception& e) {
    std::fprintf(stderr, "ef-cli: fatal: %s\n", e.what());
  } catch (...) {
    std::fprintf(stderr, "ef-cli: fatal: unknown exception\n");
  }
  return 1;
}
