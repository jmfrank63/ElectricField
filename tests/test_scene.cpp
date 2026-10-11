// Scene + Painter + SVG (D-11): geometry decisions are made once in renderScene().
#include "doctest/doctest.h"

#include <cmath>
#include <string>
#include <vector>

#include "ef/presets.hpp"
#include "ef/scene.hpp"
#include "ef/session.hpp"
#include "ef/svg.hpp"
#include "ef/pdf.hpp"
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

TEST_CASE("PDF: well-formed one-page document with a y-down transform, path operators and an exact xref") {
  FieldScene scene;
  scene.electric.push_back(line({{0, 0}, {0.1, 0.1}, {0.2, 0.2}, {0.3, 0.3}, {0.4, 0.4}}));
  scene.charges = {{Vec2(0, 0), +1}};
  const std::string pdf = renderPdf(scene, DrawStyle{}, View{200, 100, 100.});
  CHECK(pdf.rfind("%PDF-1.4", 0) == 0);
  CHECK(pdf.find("<< /Type /Catalog /Pages 2 0 R >>") != std::string::npos);
  CHECK(pdf.find("/Type /Pages /Kids [4 0 R] /Count 1") != std::string::npos);  // content 3, page 4
  CHECK(pdf.find("/MediaBox [0 0 200 100]") != std::string::npos);
  CHECK(pdf.find("1 0 0 -1 0 100 cm") != std::string::npos);   // the page-wide y-down transform
  CHECK(pdf.find("0 0 200 100 re f") != std::string::npos);    // the background rect
  CHECK(pdf.find("100 50 m") != std::string::npos);            // the path starts at the charge-free point y-down
  CHECK(pdf.find(" c\n") != std::string::npos);                // the cubic segment
  CHECK(pdf.find("S Q\n") != std::string::npos);
  CHECK(pdf.find("h f Q\n") != std::string::npos);             // the charge circle completes and fills
  CHECK(pdf.find("/Length ") != std::string::npos);
  CHECK(pdf.compare(pdf.size() - 6, 6, "%%EOF\n") == 0);
  CHECK(pdf.find("nan") == std::string::npos);
  CHECK(pdf.find("inf") == std::string::npos);

  // startxref points at the xref keyword, and the entry for object 1 points back at "1 0 obj".
  const std::size_t startxrefAt = pdf.find("startxref\n") + std::string("startxref\n").size();
  const std::size_t xrefAt = static_cast<std::size_t>(std::stoll(pdf.substr(startxrefAt)));
  REQUIRE(pdf.compare(xrefAt, 5, "xref\n") == 0);
  const std::size_t obj1 = pdf.find("\n1 0 obj\n");
  REQUIRE(obj1 != std::string::npos);
  const std::string entry1 = pdf.substr(pdf.find("0000000000 65535 f \n") + std::string("0000000000 65535 f \n").size(), 20);
  CHECK(std::stoll(entry1) == obj1 + 1);  // the recorded offset is the object's start byte
  // The content stream length counts its own bytes exactly.
  const std::size_t lenAt = pdf.find("<< /Length ") + std::string("<< /Length ").size();
  const std::size_t len = static_cast<std::size_t>(std::stoll(pdf.substr(lenAt)));
  const std::size_t streamAt = pdf.find("stream\n") + std::string("stream\n").size();
  const std::size_t endStreamAt = pdf.find("\nendstream\n", streamAt);
  CHECK(endStreamAt - streamAt == len);
}

TEST_CASE("PDF: multi-page collection with outline bookmarks in page order") {
  FieldScene a, b;
  a.charges = {{Vec2(0, 0), +1}};
  b.charges = {{Vec2(-1, 0), -1}, {Vec2(1, 0), -1}};
  ef::PdfPainter p;
  renderScene(p, a, DrawStyle{}, View{1024, 768, 100.});
  p.setTitle("Dipole");
  renderScene(p, b, DrawStyle{}, View{1024, 768, 100.});
  p.setTitle("My \"Field\" (x)");
  const std::string pdf = p.build();
  CHECK(pdf.find("/Type /Catalog /Pages 2 0 R /Outlines 3 0 R") != std::string::npos);
  CHECK(pdf.find("/Type /Pages /Kids [4 0 R 6 0 R] /Count 2") != std::string::npos);
  CHECK(pdf.find("/Type /Outlines /First 8 0 R /Last 9 0 R /Count 2") != std::string::npos);
  CHECK(pdf.find("/Title (Dipole) /Parent 3 0 R /Dest [4 0 R /Fit] /Next 9 0 R") != std::string::npos);
  CHECK(pdf.find("/Title (My \"Field\" \\(x\\))") != std::string::npos);  // quotes stay, ( ) \ escape
  CHECK(pdf.find("/Prev 8 0 R") != std::string::npos);
  // every object's xref entry points back at its "obj" line (offset == the object's start byte)
  const std::size_t xrefAt = static_cast<std::size_t>(std::stoll(pdf.substr(pdf.find("startxref\n") + 10)));
  REQUIRE(pdf.compare(xrefAt, 5, "xref\n") == 0);
  for (int obj = 1; obj <= 9; ++obj) {
    const std::string marker = "\n" + std::to_string(obj) + " 0 obj\n";
    const std::size_t at = pdf.find(marker);
    REQUIRE(at != std::string::npos);
    const std::size_t tableAt = pdf.find("0000000000 65535 f \n") + std::string("0000000000 65535 f \n").size();
    const std::string entry = pdf.substr(tableAt + static_cast<std::size_t>(obj - 1) * 20, 10);
    CHECK(std::stoll(entry) == at + 1);
  }
}

TEST_CASE("every preset renders to a finite PDF (headless end to end, default options)") {
  for (const auto& pr : presets()) {
    CAPTURE(pr.slug);
    const ComputeParams p = lightParams();
    const ChargeSystem field = makeSystem(pr);
    ComputeSession s(field, p);
    s.wait();
    FieldResult r;
    REQUIRE(s.takeResult(r));
    const std::string pdf = renderPdf(buildScene(field, r), DrawStyle{}, View{});
    CHECK(pdf.size() > 500);
    CHECK(pdf.find("nan") == std::string::npos);
    CHECK(pdf.find("inf") == std::string::npos);
    CHECK(pdf.compare(pdf.size() - 6, 6, "%%EOF\n") == 0);
  }
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

TEST_CASE("renderScene honours the view centre (v2.2 zoom/pan)") {
  Recorder r;
  FieldScene scene;
  scene.charges.push_back(SceneCharge{Vec2{0, 0}, 1});
  const View view{200, 200, 100.};
  renderScene(r, scene, DrawStyle{}, view);
  REQUIRE(r.circles.size() == 1);
  CHECK(r.circles[0].c.x == doctest::Approx(100));
  CHECK(r.circles[0].c.y == doctest::Approx(100));

  Recorder shifted;
  View off{200, 200, 100.};
  off.centre = Vec2{1, 0};  // world (1, 0) sits at the canvas centre -> the origin charge moves left by 100 px
  renderScene(shifted, scene, DrawStyle{}, off);
  REQUIRE(shifted.circles.size() == 1);
  CHECK(shifted.circles[0].c.x == doctest::Approx(0));
  CHECK(shifted.circles[0].c.y == doctest::Approx(100));
}

TEST_CASE("renderScene draws the grid between background and scene (v2.4 BUG-01)") {
  Recorder r;
  FieldScene scene;
  scene.charges.push_back(SceneCharge{Vec2{0, 0}, 1});
  ef::GridSettings g;
  g.visible = true;
  g.active = false;
  g.spacing = 1.0;  // lines every 100 px on a 200 px view
  renderScene(r, scene, DrawStyle{}, View{200, 200, 100.}, &g);
  // 3 vertical + 3 horizontal lines cross the 200x200 view at spacing 1.0 (centred origin).
  CHECK(r.strokes.size() >= 4);
  bool gridColorSeen = false;
  for (const auto& s : r.strokes)
    if (s.color.r == 120 && s.color.g == 210 && s.color.b == 120) gridColorSeen = true;  // v2.5 GRID-05: light green
  CHECK(gridColorSeen);
  // The charge is still drawn on top.
  CHECK(r.circles.size() == 1);
  CHECK(r.ended);

  // nullptr (the ef-cli path) draws no grid.
  Recorder r2;
  renderScene(r2, scene, DrawStyle{}, View{200, 200, 100.});
  bool gridInOracle = false;
  for (const auto& s : r2.strokes)
    if (s.color.r == 120 && s.color.g == 210 && s.color.b == 120) gridInOracle = true;
  CHECK_FALSE(gridInOracle);

  // v2.5 GRID-06: intersections-only mode draws dots (distinct colour) and NO lines.
  Recorder r4;
  ef::GridSettings d = g;
  d.visible = true;
  d.intersectionsOnly = true;
  renderScene(r4, scene, DrawStyle{}, View{200, 200, 100.}, &d);
  CHECK(r4.strokes.empty());
  // lattice dots: 3x3 = 9 dots (orange) PLUS the charge circle (blue) - tell them apart by colour.
  CHECK(r4.circles.size() >= 9);
  long dots = 0;
  for (const auto& c : r4.circles)
    if (c.color.r == 217 && c.color.g == 119 && c.color.b == 6) ++dots;
  CHECK(dots == 9);
  bool anyDotOffLattice = false;
  for (const auto& c : r4.circles) {
    if (c.color.r == 217) {
      const bool onLattice = (std::abs(c.c.x - std::round(c.c.x / 100.0) * 100.0) < 0.01) && (std::abs(c.c.y - std::round(c.c.y / 100.0) * 100.0) < 0.01);
      if (!onLattice) anyDotOffLattice = true;
    }
  }
  CHECK_FALSE(anyDotOffLattice);

  // visible=false draws nothing even when passed.
  Recorder r3;
  g.visible = false;
  renderScene(r3, scene, DrawStyle{}, View{200, 200, 100.}, &g);
  CHECK(r3.strokes.empty());
  CHECK(r3.circles.size() == 1);  // only the charge, no grid
}
