#pragma once
// PDF backend of Painter (v2.5 EXP-03): minimal, font-free (vector-only) PDF 1.4 writer used by
// "Export Configuration" (PNG/SVG/PDF) and "Export Collection" (one page per configuration,
// rendered with a fitted view). Like the SVG backend the document is produced in memory; file I/O
// is the caller's job and must be BINARY ("wb"). Std-only: unit-testable like SvgPainter.
//
// Coordinates arrive as device pixels (y down) per the Painter contract; one page-wide mirror
// transform (1 0 0 -1 0 H cm) keeps the backend dumb. One point == one device pixel, which keeps
// stroke widths and the MediaBox directly in canvas pixels (same numbers as the SVG export).
//
// Multi-page: renderScene pairs one begin() with one end(), so one renderScene invocation is one
// page - call it once per page, then setTitle for the page just rendered (bookmarks render best in
// page order; build() assembles the pages that accumulated).
#include <string>
#include <vector>

#include "ef/scene.hpp"

namespace ef {

class PdfPainter final : public Painter {
 public:
  void begin(int widthPx, int heightPx, Color background) override;  // starts a page (a renderScene calls it)
  void strokePath(const PointF* pts, std::size_t n, std::size_t bezierSegments, bool closed, Color color, float width) override;
  void fillCircle(PointF center, float radius, Color color) override;
  void end() override;  // closes the page (a renderScene calls it too - one renderScene is one page)

  void setTitle(std::string title);  // bookmark title of the page most recently rendered
  std::string build() const;         // the complete document: objects, exact xref, %%EOF

 private:
  struct Page {
    int width = 0, height = 0;
    std::string content;  // raw content-stream bytes (exactly what /Length counts)
  };
  std::vector<Page> pages_;
  std::vector<std::string> titles_;  // per page, parallel to pages_ ("" = no bookmark text)
  bool open_ = false;
};

// Scene -> complete one-page PDF document.
std::string renderPdf(const FieldScene& scene, const DrawStyle& style, const View& view);

}  // namespace ef