// PDF 1.4 writer (v2.5 EXP-03). No fonts, no images: only path operators (m/l/c/re/h), non-stroking
// and stroking colors, and the page-wide y-down transform. Page titles (configuration names) become
// outline bookmarks as PDFDocEncoding strings, which need no font resources; anything outside the
// single byte range degrades to '?'.
//
// Objects are built in memory; the xref offsets are recorded while appending (objects are emitted in
// number order), so the table is exact. File I/O is the caller's job and must be BINARY ("wb": text
// mode translates newlines and corrupts the streams). Coordinates arrive as device pixels (y down)
// per the Painter contract; one page-wide mirror transform (1 0 0 -1 0 H cm) keeps the backend dumb.
// One point == one device pixel: stroke widths and the MediaBox stay directly in canvas pixels (as
// in the SVG export).
//
// Object plan (N pages): 1 Catalog, 2 Pages; per page i (1-based) the content stream is object 2i+1
// and the page object 2i+2; with bookmarks the outlines root is 2N+3 and item i is 2N+3+i.
#include "ef/pdf.hpp"

#include <cmath>
#include <cstdio>

namespace ef {

namespace {

// printf-style numbers are locale-dependent only if the program calls setlocale(); the core never does.
// Trailing zeros trimmed like the SVG backend, so the documents stay compact and legible.
void appendNum(std::string& s, const double raw) {
  const double v = std::isfinite(raw) ? raw : 0.0;  // NaN/Inf would break the operators
  char buf[64];
  const int n = std::snprintf(buf, sizeof buf, "%.2f", v);
  if (n <= 0) {
    s += '0';
    return;
  }
  int len = n < static_cast<int>(sizeof buf) ? n : static_cast<int>(sizeof buf) - 1;
  while (len > 0 && buf[len - 1] == '0') --len;
  if (len > 0 && buf[len - 1] == '.') --len;
  s.append(buf, static_cast<std::size_t>(len));
}

void appendPt(std::string& s, const PointF& p) {
  appendNum(s, p.x);
  s += ' ';
  appendNum(s, p.y);
}

// Outline titles are PDFDocEncoding strings: ASCII is identity; the characters PDF reserves (\, ( and
// )) are escaped; anything outside the single-byte range becomes '?' (no embedded fonts).
std::string pdfString(const std::string& text) {
  std::string out = "(";
  for (const char raw : text) {
    const unsigned char c = static_cast<unsigned char>(raw);  // NOLINT(readability-identifier-length)
    if (c >= 0x80) {
      out += '?';
      continue;
    }
    if (c == '\\' || c == '(' || c == ')') out += '\\';
    out += static_cast<char>(c);
  }
  out += ')';
  return out;
}

}  // namespace

void PdfPainter::begin(const int width, const int height, const Color background) {
  open_ = true;
  Page page;
  page.width = width;
  page.height = height;
  // Device pixels are y-down (Painter contract); one page transform makes PDF's bottom-left origin
  // match, and every following operator uses Painter coordinates verbatim.
  page.content += "q 1 0 0 -1 0 ";
  appendNum(page.content, static_cast<double>(height));
  page.content += " cm\n";
  page.content += "q ";
  appendNum(page.content, background.r / 255.0);
  page.content += ' ';
  appendNum(page.content, background.g / 255.0);
  page.content += ' ';
  appendNum(page.content, background.b / 255.0);
  page.content += " rg 0 0 ";
  appendNum(page.content, static_cast<double>(width));
  page.content += ' ';
  appendNum(page.content, static_cast<double>(height));
  page.content += " re f Q\n";
  pages_.push_back(std::move(page));
  titles_.emplace_back();
}

void PdfPainter::strokePath(const PointF* pts, const std::size_t n, const std::size_t bezierSegments, const bool closed,
                            const Color color, const float width) {
  if (!open_ || n < 2) return;
  std::string& s = pages_.back().content;
  s += "q ";
  appendNum(s, color.r / 255.0);
  s += ' ';
  appendNum(s, color.g / 255.0);
  s += ' ';
  appendNum(s, color.b / 255.0);
  s += " RG ";
  appendNum(s, width);
  s += " w\n";
  appendPt(s, pts[0]);
  s += " m\n";
  // pts[0..3*bezierSegments] chains cubic segments (p0, then c1,c2,p3 each); the rest are line-tos.
  std::size_t i = 1;
  for (std::size_t seg = 0; seg < bezierSegments && i + 2 < n; ++seg, i += 3) {
    appendPt(s, pts[i]);
    s += ' ';
    appendPt(s, pts[i + 1]);
    s += ' ';
    appendPt(s, pts[i + 2]);
    s += " c\n";
  }
  if (i < n) {
    for (; i < n; ++i) {
      appendPt(s, pts[i]);
      s += " l\n";
    }
  }
  if (closed) s += "h\n";
  s += "S Q\n";
}

void PdfPainter::fillCircle(const PointF c, const float radius, const Color color) {
  if (!open_ || !(radius > 0.f)) return;
  std::string& s = pages_.back().content;
  s += "q ";
  appendNum(s, color.r / 255.0);
  s += ' ';
  appendNum(s, color.g / 255.0);
  s += ' ';
  appendNum(s, color.b / 255.0);
  s += " rg\n";
  // The 4-arc circle approximation (kappa controls), in local y-down coordinates relative to the
  // centre: (r,0) -> (0,-r) -> (-r,0) -> (0,r) -> (r,0), one "c" per quarter; operands are
  // x1 y1 x2 y2 x3 y3 (a[0..5] already follow that pairing: even = x, odd = y).
  const double r = static_cast<double>(radius);  // NOLINT(readability-identifier-length)
  const double k = 0.552284749830794 * r;        // NOLINT(readability-identifier-length)
  const double arcs[4][6] = {
      {r, -k, k, -r, 0, -r},  //
      {-k, -r, -r, -k, -r, 0},
      {-r, k, -k, r, 0, r},
      {k, r, r, k, r, 0},
  };
  appendNum(s, c.x + r);
  s += ' ';
  appendNum(s, c.y);
  s += " m\n";
  for (const auto& a : arcs) {
    appendNum(s, c.x + a[0]);
    s += ' ';
    appendNum(s, c.y + a[1]);
    s += ' ';
    appendNum(s, c.x + a[2]);
    s += ' ';
    appendNum(s, c.y + a[3]);
    s += ' ';
    appendNum(s, c.x + a[4]);
    s += ' ';
    appendNum(s, c.y + a[5]);
    s += " c\n";
  }
  s += "h f Q\n";
}

void PdfPainter::end() { open_ = false; }

void PdfPainter::setTitle(std::string title) {
  if (!titles_.empty()) titles_.back() = std::move(title);
}

std::string PdfPainter::build() const {
  static const std::string header("\x25\xE2\xE3\xCF\xD3\n", 6);  // "%\xE2\xE3\xCF\xD3\n": the binary marker line
  std::string doc = "%PDF-1.4\n";
  doc += header;
  const int N = static_cast<int>(pages_.size());
  const bool outlines = N > 1;  // bookmarks only matter for multi-page collection exports

  std::vector<std::size_t> offsets;  // index = object number - 1 (objects emit in number order)
  auto emit = [&](const int obj, const std::string& body) {
    offsets.push_back(doc.size());
    doc += std::to_string(obj);
    doc += " 0 obj\n";
    doc += body;
    doc += "endobj\n";
  };
  auto contentObj = [](const int i) { return 2 * i + 1; };
  auto pageObj = [](const int i) { return 2 * i + 2; };
  const int lastPageObj = 2 * N + 2;
  const int lastObj = outlines ? lastPageObj + 1 + N : lastPageObj;

  std::string kids;
  for (int i = 1; i <= N; ++i) {
    if (i > 1) kids += ' ';
    kids += std::to_string(pageObj(i));
    kids += " 0 R";
  }
  std::string catalogBody = "<< /Type /Catalog /Pages 2 0 R";
  if (outlines) catalogBody += " /Outlines 3 0 R";
  catalogBody += " >>\n";
  emit(1, catalogBody);
  emit(2, "<< /Type /Pages /Kids [" + kids + "] /Count " + std::to_string(N) + " >>\n");
  for (int i = 1; i <= N; ++i) {
    const Page& page = pages_[static_cast<std::size_t>(i - 1)];
    // The stream is emitted before its page object so a reader never needs forward knowledge; object
    // numbers do not have to follow file order (the xref table is what counts).
    offsets.push_back(doc.size());
    doc += std::to_string(contentObj(i));
    doc += " 0 obj\n<< /Length ";
    doc += std::to_string(page.content.size());
    doc += " >>\nstream\n";
    doc += page.content;
    doc += "\nendstream\nendobj\n";
    emit(pageObj(i), "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 " + std::to_string(page.width) + ' ' +
                         std::to_string(page.height) + "] /Resources << /ProcSet [/PDF] >> /Contents " +
                         std::to_string(contentObj(i)) + " 0 R >>\n");
  }
  if (outlines) {
    const int rootObj = lastPageObj + 1;
    const int firstItem = rootObj + 1;
    emit(rootObj, "<< /Type /Outlines /First " + std::to_string(firstItem) + " 0 R /Last " +
                      std::to_string(lastObj) + " 0 R /Count " + std::to_string(N) + " >>\n");
    for (int i = 1; i <= N; ++i) {
      std::string body = "<< /Title " + pdfString(titles_[static_cast<std::size_t>(i - 1)]) + " /Parent 3 0 R /Dest [";
      body += std::to_string(pageObj(i));
      body += " 0 R /Fit]";
      if (i > 1) {
        body += " /Prev ";
        body += std::to_string(firstItem + i - 2);
        body += " 0 R";
      }
      if (i < N) {
        body += " /Next ";
        body += std::to_string(firstItem + i);
        body += " 0 R";
      }
      body += " >>\n";
      emit(firstItem + i - 1, body);
    }
  }

  const std::size_t xrefAt = doc.size();
  doc += "xref\n0 ";
  doc += std::to_string(lastObj + 1);
  doc += "\n0000000000 65535 f \n";  // free object 0, the standard 20-byte entry
  for (int i = 1; i <= lastObj; ++i) {
    char entry[24];  // "%010zu 00000 n \n" is exactly 20 chars + NUL; gcc's min expansion fits in 24
    const int len = std::snprintf(entry, sizeof entry, "%010zu 00000 n \n", offsets[static_cast<std::size_t>(i - 1)]);
    doc.append(entry, static_cast<std::size_t>(len));
  }
  doc += "trailer\n<< /Size ";
  doc += std::to_string(lastObj + 1);
  doc += " /Root 1 0 R >>\nstartxref\n";
  doc += std::to_string(xrefAt);
  doc += "\n%%EOF\n";
  return doc;
}

std::string renderPdf(const FieldScene& scene, const DrawStyle& style, const View& view) {
  PdfPainter painter;
  renderScene(painter, scene, style, view);
  return painter.build();
}

}  // namespace ef