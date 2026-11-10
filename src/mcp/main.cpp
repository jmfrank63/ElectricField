// ef-mcp: Model Context Protocol server (stdio) exposing the headless simulation to agents. See docs/mcp.md.
// stdout carries protocol messages only; diagnostics go to stderr.
#include <cstdio>
#include <cstring>
#include <exception>
#include <iostream>
#include <string>

#include "ef_build_info.h"
#include "mcp/server.hpp"
#ifdef EF_MCP_PNG
#include "render/png_export.hpp"
#endif

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

int main(int argc, char** argv) {
  if (argc == 2 && std::strcmp(argv[1], "--version") == 0) {
    std::printf("ef-mcp %s\n", EF_VERSION);
    return 0;
  }
  if (argc != 1) {
    std::fprintf(stderr, "ef-mcp is an MCP server on stdin/stdout and takes no arguments (except --version). See docs/mcp.md.\n");
    return 2;
  }
#ifdef _WIN32
  // Text mode would rewrite the newline delimiter and stop reading at a stray Ctrl-Z.
  _setmode(_fileno(stdin), _O_BINARY);
  _setmode(_fileno(stdout), _O_BINARY);
#endif
  try {
    efmcp::PngEncoder encodePng;
#ifdef EF_MCP_PNG
    encodePng = [](const ef::FieldScene& scene, const ef::DrawStyle& style, const ef::View& view, std::string& png) { return efrender::renderPngBytes(scene, style, view, png); };
#endif
    efmcp::Server server(EF_VERSION, encodePng);
    return efmcp::serve(std::cin, std::cout, server);
  } catch (const std::exception& e) {
    std::fprintf(stderr, "ef-mcp: fatal: %s\n", e.what());
  } catch (...) {
    std::fprintf(stderr, "ef-mcp: fatal: unknown exception\n");
  }
  return 1;
}
