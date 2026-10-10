// Scene + Painter + SVG (D-11): geometry decisions are made once in renderScene().
#include "doctest/doctest.h"

#include <cmath>
#include <string>
#include <vector>

#include "ef/presets.hpp"
#include "ef/scene.hpp"
#include "ef/session.hpp"
#include "ef/svg.hpp"
#include "test_params.hpp"

using namespace ef;

namespace {
struct Recorder final : Painter {
  struct Stroke {
    std::vector<PointF> pts;
    std::size_t segs;
    bool closed;
    Color color;
    float width;
  };
  struct Circle {
    PointF c;
    float r;
    Color color;
  };
  int w = 0, h = 0;
  Color bg;
  std::vector<Stroke> strokes;
  std::vector<Circle> circles;
  bool ended = false;
  void begin(int ww, int hh, Color b) override { w = ww; h = hh; bg = b; }
  void strokePath(const PointF* p, std::size_t n, std::size_t segs, bool closed, Color c, float width) override {
    strokes.push_back({std::vector<PointF>(p, p + n), segs, closed, c, width});
  }
  void fillCircle(PointF c, float r, Color color) override { circles.push_back({c, r, color}); }
  void end() override { ended = true; }
};

ScenePath line(std::initializer_list<Vec2> pts, bool closed = false) { return ScenePath{std::vector<Vec2>(pts), closed}; }
}  // namespace

TEST_CASE("renderScene: y-up, centred, 100 px/unit, MFC colours/thickness/order") {
  FieldScene scene;
  scene.electric.push_back(line({{0, 0}, {0.1, 0.1}, {0.2, 0.2}, {0.3, 0.3}}));        // 1 cubic segment
  scene.potential.push_back(line({{-1, 0}, {-1, 1}, {1, 1}, {1, 0}, {-1, 0}}, true));    // 4 pts+1: 1 segment + 1 tail line, closed
  scene.charges = {{Vec2(-2, 0), +1}, {Vec2(2, 0), -1}};
  Recorder rec;
  renderScene(rec, scene, DrawStyle{}, View{1024, 768, 100.});
  CHECK(rec.w == 1024);
  CHECK(rec.h == 768);
  CHECK(rec.ended);
  REQUIRE(rec.strokes.size() == 2);
  // electric first (black), potential second (grey 192)
  CHECK(rec.strokes[0].color.r == 0);
  CHECK(rec.strokes[1].color.r == 192);
  CHECK(rec.strokes[0].width == 1.f);
  // world (0.3, 0.3) -> device (512 + 30, 384 - 30)
  CHECK(rec.strokes[0].pts.back().x == doctest::Approx(542.f));
  CHECK(rec.strokes[0].pts.back().y == doctest::Approx(354.f));
  CHECK(rec.strokes[0].segs == 1);
  CHECK_FALSE(rec.strokes[0].closed);
  // potential: 5 points = 1 Bezier segment + 1 trailing line point; first == last => closed
  CHECK(rec.strokes[1].segs == 1);
  CHECK(rec.strokes[1].pts.size() == 5);
  CHECK(rec.strokes[1].closed);
  // charges: radius 25 px, + blue, - red, at (+-200 px, 0)
  REQUIRE(rec.circles.size() == 2);
  CHECK(rec.circles[0].c.x == doctest::Approx(312.f));
  CHECK(rec.circles[0].c.y == doctest::Approx(384.f));
  CHECK(rec.circles[0].r == 25.f);
  CHECK(rec.circles[0].color.b == 255);
  CHECK(rec.circles[0].color.r == 0);
  CHECK(rec.circles[1].color.r == 255);
  CHECK(rec.circles[1].color.b == 0);
}

TEST_CASE("renderScene: segments outside the view are culled and split the stroke; a clipped closed figure is not closed") {
  FieldScene scene;
  // electric: seg0 (0..0.3) visible, seg1 (50..80 units = 5000..8000 px) fully outside, seg2 back near the origin
  scene.electric.push_back(line({{0, 0}, {0.1, 0}, {0.2, 0}, {50, 0},   {60, 0}, {70, 0}, {80, 0},   {70, 5}, {10, 5}, {0, 1}}));
  // closed potential loop with the same fully-outside middle segment: must not be closed any more (MFC `broken`)
  scene.potential.push_back(line({{0, 0}, {1, 1}, {2, 1}, {50, 0},   {60, 0}, {70, 0}, {80, 0},   {70, 5}, {10, 5}, {0, 0}}, true));
  Recorder rec;
  renderScene(rec, scene, DrawStyle{}, View{1024, 768, 100.});
  int electric = 0, potential = 0;
  for (const auto& s : rec.strokes) {
    if (s.color.r == 0) { ++electric; CHECK_FALSE(s.closed); }
    else { ++potential; CHECK_FALSE(s.closed); }
  }
  CHECK(electric == 2);   // the culled middle segment splits the stroke in two runs
  CHECK(potential == 2);
}

TEST_CASE("renderScene: single-point or empty paths draw nothing; no NaN reaches the painter") {
  FieldScene scene;
  scene.electric.push_back(line({{0, 0}}));
  scene.electric.push_back(ScenePath{});
  Recorder rec;
  renderScene(rec, scene, DrawStyle{}, View{});
  CHECK(rec.strokes.empty());
}

TEST_CASE("SVG: well-formed document with paths, circles, y flip and colours") {
  FieldScene scene;
  scene.electric.push_back(line({{0, 0}, {0.1, 0.1}, {0.2, 0.2}, {0.3, 0.3}, {0.4, 0.4}}));
  scene.charges = {{Vec2(0, 0), +1}};
  const std::string svg = renderSvg(scene, DrawStyle{}, View{200, 100, 100.});
  CHECK(svg.rfind("<svg ", 0) == 0);
  CHECK(svg.find("viewBox=\"0 0 200 100\"") != std::string::npos);
  CHECK(svg.find("d=\"M100 50C110 40 120 30 130 20L140 10\"") != std::string::npos);  // cubic + one trailing line, y flipped
  CHECK(svg.find("stroke=\"#000000\"") != std::string::npos);
  CHECK(svg.find("<circle cx=\"100\" cy=\"50\" r=\"25\" fill=\"#0000ff\"/>") != std::string::npos);
  CHECK(svg.find("nan") == std::string::npos);
  CHECK(svg.find("inf") == std::string::npos);
  CHECK(svg.compare(svg.size() - 7, 7, "</svg>\n") == 0);
}

TEST_CASE("every preset renders to a finite SVG (headless end to end, default options)") {
  for (const auto& pr : presets()) {
    CAPTURE(pr.slug);
    const ComputeParams p = lightParams();
    const ChargeSystem field = makeSystem(pr);
    ComputeSession s(field, p);
    s.wait();
    FieldResult r;
    REQUIRE(s.takeResult(r));
    const std::string svg = renderSvg(buildScene(field, r), DrawStyle{}, View{});
    CHECK(svg.size() > 500);
    CHECK(svg.find("nan") == std::string::npos);
    CHECK(svg.find("inf") == std::string::npos);
  }
}
