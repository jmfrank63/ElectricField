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
#include "ef/bezier.hpp"
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

// 261012-h21: a realistic post-finalize equipotential: 64 on-curve steps around a unit circle plus the exact
// closure point (the tracer pushes_back the exact start point to close the loop), with the inner control points
// fitted by adjustForBezier (as finalize() leaves the layout p0,[c1,c2,p3],...). Anchor spacing is 3 raw steps
// = 16.875 degrees of arc: ~29 px chords at ppu 100 (legacy branch), ~73 px at ppu 250, ~293 px at ppu 1000.
ScenePath closedUnitCircle() {
  const std::size_t steps = 64;
  std::vector<Vec2> raw;
  raw.reserve(steps + 1);
  for (std::size_t k = 0; k < steps; ++k) {
    const double a = static_cast<double>(k) * 6.283185307179586 / static_cast<double>(steps);
    raw.push_back(Vec2{std::cos(a), std::sin(a)});
  }
  raw.push_back(raw.front());  // the tracer closes the loop with the exact start point
  std::vector<Vec2> fitted = raw;
  ef::adjustForBezier(fitted);
  return ScenePath{fitted, true};
}
}  // namespace

TEST_CASE("renderScene: y-up, centred, 100 px/unit, MFC colours/thickness/order") {
  FieldScene scene;
  scene.electric.push_back(line({{0, 0}, {0.1, 0.1}, {0.2, 0.2}, {0.3, 0.3}}));        // 1 cubic segment
  // 261012-h21: shrunk from the former +-1-unit square (whose 200 px anchor chords now cross the refinement
  // gate and take the refined branch) to a 0.4-unit arch: still 1 cubic + 1 tail line, closed, but with
  // 40 px anchor chords so it stays on the legacy branch this test pins.
  scene.potential.push_back(line({{-0.2, 0}, {-0.2, 0.1}, {0.2, 0.1}, {0.2, 0}, {-0.2, 0}}, true));  // 4 pts+1: 1 segment + 1 tail line, closed
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

// v2.6 phase 3: a charge with its own sizePx draws at that radius; 0 inherits the style radius. Every
// exporter (svg/pdf/png) funnels through renderScene, so one Recorder pins all of them.
TEST_CASE("renderScene draws SceneCharge.sizePx when set, style.chargeRadiusPx when 0 (v2.6 phase 3)") {
  FieldScene scene;
  scene.charges.push_back(SceneCharge{Vec2{-2, 0}, +1, 60.f});  // custom radius
  scene.charges.push_back(SceneCharge{Vec2{2, 0}, -1, 0.f});    // inherit the style default
  Recorder rec;
  renderScene(rec, scene, DrawStyle{}, View{1024, 768, 100.});
  REQUIRE(rec.circles.size() == 2);
  CHECK(rec.circles[0].r == 60.f);
  CHECK(rec.circles[0].color.b == 255);  // still the positive-charge colour
  CHECK(rec.circles[1].r == 25.f);       // DrawStyle{} default
  CHECK(rec.circles[1].color.r == 255);

  // The SVG exporter picks the custom radius up through the same renderScene site.
  const std::string svg = renderSvg(scene, DrawStyle{}, View{1024, 768, 100.});
  CHECK(svg.find("<circle cx=\"312\" cy=\"384\" r=\"60\"") != std::string::npos);
  CHECK(svg.find("r=\"25\"") != std::string::npos);
}

// v2.6 phase 3 (CR-01): per-charge sizePx is LOGICAL px and is scaled by view.styleScale, while
// style.chargeRadiusPx arrives from the callers already device-scaled. At k=2 a custom-sized charge
// and an inherited charge of the same logical value must produce the SAME drawn dot extent - the
// exact defect F1 pinned (half-size dots on HiDPI displays). Every unscaled caller (styleScale 1,
// the default) is byte-identical to the pre-CR-01 behaviour.
TEST_CASE("renderScene scales per-charge sizePx by view.styleScale (v2.6 phase 3 CR-01, HiDPI)") {
  FieldScene scene;
  scene.charges.push_back(SceneCharge{Vec2{-2, 0}, +1, 25.f});  // custom: 25 LOGICAL px
  scene.charges.push_back(SceneCharge{Vec2{2, 0}, -1, 0.f});    // inherited
  ef::DrawStyle style;      // chargeRadiusPx 25 logical...
  style.chargeRadiusPx = 50.f;  // ...pre-scaled by k=2, exactly what scaledStyle(base, 2) produces
  View view{1024, 768, 200.};   // device ppu at k=2, zoom 1 (canvasView gives 100 * k)
  view.styleScale = 2.f;        // what the GUI canvas / runExport stamp
  Recorder rec;
  renderScene(rec, scene, style, view);
  REQUIRE(rec.circles.size() == 2);
  CHECK(rec.circles[0].r == doctest::Approx(50.f));  // 25 logical * 2 = the sibling's extent
  CHECK(rec.circles[1].r == doctest::Approx(50.f));  // inherited: the pre-scaled style radius
  CHECK(rec.circles[0].color.b == 255);              // colours untouched
  CHECK(rec.circles[1].color.r == 255);

  // Default styleScale 1 (every core/exporter caller): raw sizePx, byte-identical to pre-CR-01.
  Recorder unscaled;
  renderScene(unscaled, scene, DrawStyle{}, View{1024, 768, 200.});
  REQUIRE(unscaled.circles.size() == 2);
  CHECK(unscaled.circles[0].r == doctest::Approx(25.f));
  CHECK(unscaled.circles[1].r == doctest::Approx(25.f));
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

// ---------------------------------------------------------------------------
// 261012-h21: zoom-adaptive drawPath refinement (legacy below the 48 px anchor-chord gate,
// uniform Catmull-Rom through the anchors above it).
// ---------------------------------------------------------------------------

TEST_CASE("drawPath legacy pin: while every anchor chord is <= 48 px the stroke stays the direct device mapping (261012-h21)") {
  FieldScene scene;
  // two complete cubic groups, anchors ~30 px apart at ppu 100
  scene.electric.push_back(line({{0, 0}, {0.1, 0.05}, {0.2, 0}, {0.3, 0.05}, {0.4, 0}, {0.5, 0.05}, {0.6, 0}}));
  // one cubic + one trailing line point
  scene.electric.push_back(line({{0, 0}, {0.1, 0.05}, {0.2, 0}, {0.3, 0.05}, {0.4, 0.1}}));
  Recorder rec;
  const View view{1024, 768, 100.};
  renderScene(rec, scene, DrawStyle{}, view);
  REQUIRE(rec.strokes.size() == 2);
  const double cx = view.widthPx / 2.0 - view.centre.X * view.pixelsPerUnit;
  const double cy = view.heightPx / 2.0 + view.centre.Y * view.pixelsPerUnit;
  for (std::size_t s = 0; s < rec.strokes.size(); ++s) {
    const auto& path = scene.electric[s];
    REQUIRE(rec.strokes[s].pts.size() == path.points.size());
    for (std::size_t i = 0; i < path.points.size(); ++i) {
      CHECK(rec.strokes[s].pts[i].x == doctest::Approx(static_cast<float>(cx + path.points[i].X * view.pixelsPerUnit)));
      CHECK(rec.strokes[s].pts[i].y == doctest::Approx(static_cast<float>(cy - path.points[i].Y * view.pixelsPerUnit)));
    }
  }
  CHECK(rec.strokes[0].segs == 2);  // (7-1)/3
  CHECK_FALSE(rec.strokes[0].closed);
  CHECK(rec.strokes[1].segs == 1);  // (5-1)/3, tail point kept as a straight line
  CHECK_FALSE(rec.strokes[1].closed);
}

TEST_CASE("drawPath refinement: emitted cubics interpolate the anchors and are tangent-continuous at every join (261012-h21)") {
  const ScenePath circle = closedUnitCircle();
  FieldScene scene;
  scene.potential.push_back(circle);
  Recorder rec;
  const View view{3000, 3000, 1000.};  // fully inside, every anchor chord ~293 px -> refined
  renderScene(rec, scene, DrawStyle{}, view);
  REQUIRE(rec.strokes.size() == 1);
  const auto& s = rec.strokes[0];

  // anchors: device points of fitted[0],fitted[3],...,fitted[63] (the duplicate closure anchor is dropped)
  const std::size_t m = 22;
  const double cx = view.widthPx / 2.0 - view.centre.X * view.pixelsPerUnit;
  const double cy = view.heightPx / 2.0 + view.centre.Y * view.pixelsPerUnit;
  auto anchor = [&](std::size_t k) {
    const Vec2& w = circle.points[3 * k];
    return PointF{static_cast<float>(cx + w.X * view.pixelsPerUnit), static_cast<float>(cy - w.Y * view.pixelsPerUnit)};
  };

  REQUIRE(s.segs == m);
  REQUIRE(s.pts.size() == 1 + 3 * m);
  CHECK(s.closed);
  // every group starts and ends exactly on its anchors (wrap: the last group returns to the first anchor)
  for (std::size_t k = 0; k < m; ++k) {
    const std::size_t endIdx = (k + 1) % m;
    CHECK(s.pts[3 * k].x == doctest::Approx(anchor(k).x));
    CHECK(s.pts[3 * k].y == doctest::Approx(anchor(k).y));
    CHECK(s.pts[3 * k + 3].x == doctest::Approx(anchor(endIdx).x));
    CHECK(s.pts[3 * k + 3].y == doctest::Approx(anchor(endIdx).y));
  }
  // C1 at every interior join AND at the wrap seam: incoming 3*(p3-c2) equals outgoing 3*(c1'-p0).
  // Not exact in floats (control points are stored as device floats, so each carries an ulp-scale rounding);
  // a wrong construction would be off by O(anchor chord), so 1e-3 px pins the construction tightly.
  for (std::size_t k = 0; k + 1 < m; ++k) {  // interior joins at pts[3k+3] (anchor k+1)
    const double inx = 3.0 * (static_cast<double>(s.pts[3 * k + 3].x) - static_cast<double>(s.pts[3 * k + 2].x));
    const double iny = 3.0 * (static_cast<double>(s.pts[3 * k + 3].y) - static_cast<double>(s.pts[3 * k + 2].y));
    const double outx = 3.0 * (static_cast<double>(s.pts[3 * k + 4].x) - static_cast<double>(s.pts[3 * k + 3].x));
    const double outy = 3.0 * (static_cast<double>(s.pts[3 * k + 4].y) - static_cast<double>(s.pts[3 * k + 3].y));
    CHECK(std::abs(inx - outx) < 1e-3);
    CHECK(std::abs(iny - outy) < 1e-3);
  }
  {  // the wrap seam: last group ends at the first anchor; its incoming tangent matches group 0's outgoing
    const double inx = 3.0 * (static_cast<double>(s.pts[3 * m].x) - static_cast<double>(s.pts[3 * m - 1].x));
    const double iny = 3.0 * (static_cast<double>(s.pts[3 * m].y) - static_cast<double>(s.pts[3 * m - 1].y));
    const double outx = 3.0 * (static_cast<double>(s.pts[1].x) - static_cast<double>(s.pts[0].x));
    const double outy = 3.0 * (static_cast<double>(s.pts[1].y) - static_cast<double>(s.pts[0].y));
    CHECK(std::abs(inx - outx) < 1e-3);
    CHECK(std::abs(iny - outy) < 1e-3);
  }
  // the closed loop is one stroke through the first anchor, and the final group is not zero-length
  // (the duplicate closure anchor was dropped; the wrap segment spans the last anchor back to the first)
  CHECK(s.pts[0].x == doctest::Approx(anchor(0).x));
  CHECK(s.pts[0].y == doctest::Approx(anchor(0).y));
  CHECK(s.pts[3 * m].x == doctest::Approx(anchor(0).x));
  CHECK(s.pts[3 * m].y == doctest::Approx(anchor(0).y));
  CHECK_FALSE(s.pts[3 * m - 3].x == doctest::Approx(s.pts[3 * m].x).epsilon(1e-9));
  CHECK_FALSE(s.pts[3 * m - 3].y == doctest::Approx(s.pts[3 * m].y).epsilon(1e-9));
}

TEST_CASE("drawPath gate: ppu 100 stays legacy, ppu 250 refines (261012-h21)") {
  const ScenePath circle = closedUnitCircle();
  FieldScene scene;
  scene.potential.push_back(circle);

  // ppu 100: anchor chords ~29 px -> legacy branch: the stroke is the direct device mapping
  Recorder legacy;
  const View v100{1024, 768, 100.};
  renderScene(legacy, scene, DrawStyle{}, v100);
  REQUIRE(legacy.strokes.size() == 1);
  const double cx = v100.widthPx / 2.0, cy = v100.heightPx / 2.0;
  REQUIRE(legacy.strokes[0].pts.size() == circle.points.size());
  for (std::size_t i = 0; i < circle.points.size(); ++i) {
    CHECK(legacy.strokes[0].pts[i].x == doctest::Approx(static_cast<float>(cx + circle.points[i].X * 100.0)));
    CHECK(legacy.strokes[0].pts[i].y == doctest::Approx(static_cast<float>(cy - circle.points[i].Y * 100.0)));
  }
  CHECK(legacy.strokes[0].segs == (circle.points.size() - 1) / 3);
  CHECK(legacy.strokes[0].closed);

  // ppu 250: anchor chords ~73 px -> refined branch: one extra group (22 wrap-aware segments, not 21)
  // and inner points that are no longer the direct mapping of the input points
  Recorder refined;
  const View v250{2048, 2048, 250.};
  renderScene(refined, scene, DrawStyle{}, v250);
  REQUIRE(refined.strokes.size() == 1);
  CHECK(refined.strokes[0].segs == 22);
  CHECK(refined.strokes[0].pts.size() == 67);
  CHECK(refined.strokes[0].pts[1].x != doctest::Approx(static_cast<float>(2048.0 / 2.0 + circle.points[1].X * 250.0)));
  CHECK(refined.strokes[0].pts[1].y != doctest::Approx(static_cast<float>(2048.0 / 2.0 - circle.points[1].Y * 250.0)));
}

TEST_CASE("drawGrid polar: ring segment counts adapt to the on-screen radius (261012-h21)") {
  ef::GridSettings g;
  g.visible = true;
  g.polar = true;
  g.spacing = 1.0;
  const auto isGridRing = [](const Recorder::Stroke& s) {
    return s.color.r == 120 && s.color.g == 210 && s.color.b == 120 && s.pts.size() > 2;  // rays have 2 points
  };

  // default view: the 96-segment floor pins the current behavior exactly
  Recorder small;
  renderScene(small, FieldScene{}, DrawStyle{}, View{200, 200, 100.}, &g);
  std::vector<std::size_t> smallRings;
  for (const auto& s : small.strokes)
    if (isGridRing(s)) smallRings.push_back(s.pts.size());
  REQUIRE(smallRings.size() == 2);  // maxR ~ 1.41 units -> rings 1 and 2
  CHECK(smallRings[0] == 97);       // ring 1: 100 px radius -> N = 96 floor
  CHECK(smallRings[1] == 97);       // ring 2: 200 px radius -> still the floor

  // ppu 1000: ring 3 has a 3000 px radius -> N ~ 122 (sagitta <= 1 px); every ring <= 2049 points
  Recorder zoomed;
  renderScene(zoomed, FieldScene{}, DrawStyle{}, View{4000, 4000, 1000.}, &g);
  std::vector<std::size_t> bigRings;
  for (const auto& s : zoomed.strokes)
    if (isGridRing(s)) bigRings.push_back(s.pts.size());
  REQUIRE(bigRings.size() == 3);  // maxR ~ 2.83 units -> rings 1..3
  CHECK(bigRings[0] == 97);       // ring 1: 1000 px radius -> ceil(2.2214*31.6) = 71 -> floor 96
  CHECK(bigRings[1] == 101);      // ring 2: 2000 px -> ceil(2.2214*44.7) = 100
  CHECK(bigRings[2] == 123);      // ring 3: 3000 px -> ceil(2.2214*54.8) = 122
  for (const std::size_t cnt : bigRings) CHECK(cnt <= 2049);
}
