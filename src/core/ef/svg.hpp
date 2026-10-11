#pragma once
// SVG backend of Painter (D-11): writes the document into a std::string; file I/O is the caller's job.
#include <string>

#include "ef/scene.hpp"

namespace ef {

class SvgPainter final : public Painter {
 public:
  void begin(int widthPx, int heightPx, Color background) override;
  void strokePath(const PointF* pts, std::size_t n, std::size_t bezierSegments, bool closed, Color color, float width) override;
  void fillCircle(PointF center, float radius, Color color) override;
  void end() override;

  const std::string& str() const { return out_; }

 private:
  std::string out_;
};

// Convenience: scene -> complete SVG document.
std::string renderSvg(const FieldScene& scene, const DrawStyle& style, const View& view);

}  // namespace ef
