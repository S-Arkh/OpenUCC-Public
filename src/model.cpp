#include "openucc/model.hpp"
#include <algorithm>
#include <set>
#include <stdexcept>

namespace openucc {
namespace {
void check(bool value, const char* message) {
  if (!value) throw std::runtime_error(message);
}
template<class T> void readWords(Reader& r, T& a) {
  for (auto& v : a) v = r.u32();
}
template<class T> void writeWords(Bytes& out, const T& a) {
  for (auto v : a) u32(out, v);
}
std::size_t count(Reader& r, std::size_t stride) {
  int n = r.index();
  check(n >= 0 && n <= 1000000 &&
        std::size_t(n) <= (r.data.size() - r.pos) / stride, "Invalid Model array count");
  return std::size_t(n);
}
void size(Bytes& out, std::size_t n) {
  check(n <= 1000000, "Excessive Model array"); index(out, int(n));
}
MeshBox box(Reader& r) {
  MeshBox b; readWords(r, b.coordinates); b.valid = r.byte();
  check(b.valid <= 1, "Invalid Model box flag"); return b;
}
void box(Bytes& out, const MeshBox& b) {
  check(b.valid <= 1, "Invalid Model box flag");
  writeWords(out, b.coordinates); out.push_back(b.valid);
}
bool ordinal(int i, std::size_t n) { return i >= 0 && std::size_t(i) < n; }
void validate(const ModelBody& m) {
  std::set<int> hullOffsets; std::size_t offset = 0;
  for (const auto& h : m.hulls) {
    check(offset <= 1000000, "Excessive Model hull words");
    hullOffsets.insert(int(offset));
    for (auto plane : h.planes)
      check((plane & 0x3fffffffU) < m.nodes.size(), "Invalid Model hull plane");
    offset += h.planes.size() + 7;
  }
  check(offset <= 1000000, "Excessive Model hull words");
  for (const auto& s : m.surfaces) {
    check(ordinal(s.indices[0], m.points.size()), "Invalid Model surface point");
    for (int i = 1; i <= 3; ++i)
      check(ordinal(s.indices[i], m.vectors.size()), "Invalid Model surface vector");
  }
  for (const auto& n : m.nodes) {
    check(ordinal(n.indices[1], m.surfaces.size()), "Invalid Model surface index");
    check(n.indices[0] >= 0 && std::size_t(n.indices[0]) <= m.vertices.size() &&
          n.vertexCount <= m.vertices.size() - std::size_t(n.indices[0]),
          "Invalid Model vertex range");
    for (int i = 2; i <= 4; ++i)
      check(n.indices[i] == -1 || ordinal(n.indices[i], m.nodes.size()), "Invalid Model node link");
    check(n.indices[5] == -1 || hullOffsets.count(n.indices[5]), "Invalid Model hull offset");
    check(n.indices[6] == -1 || ordinal(n.indices[6], m.renderBounds.size()),
          "Invalid Model render bound");
  }
  // This narrow profile has no observed unused invalid point records.
  for (const auto& v : m.vertices)
    check(ordinal(v[0], m.points.size()), "Invalid Model vertex point");
}
}
Model133Trailer decodeModel133Trailer(Reader& r, const Package& p) {
  check(isModel133FamilyProfile(p), "Unsupported Model trailer profile");
  auto start = r.pos;
  int n = r.index();
  check(n >= 0 && n <= 1000000, "Invalid Model trailer count");
  Bytes canonical; index(canonical, n);
  check(r.pos - start == canonical.size() &&
        std::equal(canonical.begin(), canonical.end(), r.data.begin() + start),
        "Noncanonical Model trailer count");
  check(r.data.size() - r.pos >= 4 &&
         std::size_t(n) <= (r.data.size() - r.pos - 4) / 8,
         "Truncated Model trailer");
  check(!isModel133AdjacentProfile(p) || n == 0,
        "Unsupported adjacent-profile Model suffix");
  Model133Trailer trailer; trailer.pairs.resize(std::size_t(n));
  for (auto& pair : trailer.pairs) readWords(r, pair);
  trailer.finalWord = r.u32();
  check(r.pos == r.data.size(), "Trailing Model trailer bytes");
  check(!isModel133AdjacentProfile(p) || (trailer.pairs.empty() && trailer.finalWord == 0),
        "Unsupported adjacent-profile Model suffix");
  return trailer;
}
void encodeModel133Trailer(const Model133Trailer& trailer, const Package& p, Bytes& out) {
  check(isModel133FamilyProfile(p), "Unsupported Model trailer profile");
  check(!isModel133AdjacentProfile(p) || (trailer.pairs.empty() && trailer.finalWord == 0),
        "Unsupported adjacent-profile Model suffix");
  size(out, trailer.pairs.size());
  for (const auto& pair : trailer.pairs) writeWords(out, pair);
  u32(out, trailer.finalWord);
}
ModelBody decodeModel(Reader& r, const Package& p) {
  check(p.version == 123 && p.licensee == 37, "Unsupported Model profile");
  ModelBody m; m.bounds = box(r); readWords(r, m.sphere);
  for (auto* a : {&m.vectors, &m.points}) {
    a->resize(count(r, 12)); for (auto& v : *a) readWords(r, v);
  }
  m.nodes.resize(count(r, 87));
  for (auto& n : m.nodes) {
    readWords(r, n.plane); readWords(r, n.zoneMask); n.flags = r.byte();
    for (auto& i : n.indices) i = r.index();
    readWords(r, n.spheres);
    for (auto& z : n.zones) z = r.byte();
    n.vertexCount = r.byte(); readWords(r, n.leaves); readWords(r, n.renderIndices);
  }
  m.surfaces.resize(count(r, 35));
  for (auto& s : m.surfaces) {
    s.material = r.index(); p.ref(s.material); s.flags = r.u32();
    for (auto& i : s.indices) i = r.index();
    s.actor = r.index(); p.ref(s.actor); readWords(r, s.plane);
    s.lightMapScale = r.u32(); s.licenseWord = r.u32();
  }
  m.vertices.resize(count(r, 2));
  for (auto& v : m.vertices) for (auto& i : v) i = r.index();
  m.sharedSides = r.u32();
  check(r.u32() == 0, "Unsupported Model zones");
  m.polys = r.index(); p.ref(m.polys);
  m.renderBounds.resize(count(r, 25));
  for (auto& b : m.renderBounds) b = box(r);
  auto hullWords = count(r, 4);
  while (hullWords) {
    BspHull h;
    for (;;) {
      check(hullWords > 0, "Missing Model hull terminator");
      auto word = r.u32(); --hullWords;
      if (word == 0xffffffffU) break;
      h.planes.push_back(word);
    }
    check(hullWords >= 6, "Truncated Model hull bounds");
    readWords(r, h.bounds); hullWords -= 6; m.hulls.push_back(std::move(h));
  }
  check(r.index() == 0, "Unsupported Model leaves");
  check(r.index() == 0, "Unsupported Model lights");
  m.rootOutside = r.u32(); m.linked = r.u32();
  for (int i = 0; i < 3; ++i)
    check(r.index() == 0, "Unsupported Model render arrays");
  check(r.pos == r.data.size(), "Trailing Model native bytes");
  validate(m); return m;
}
void encodeModel(const ModelBody& m, const Package& p, Bytes& out,
                 const std::function<int(int)>& reference) {
  check(p.version == 123 && p.licensee == 37, "Unsupported Model profile");
  validate(m); box(out, m.bounds); writeWords(out, m.sphere);
  for (auto* a : {&m.vectors, &m.points}) {
    size(out, a->size()); for (const auto& v : *a) writeWords(out, v);
  }
  size(out, m.nodes.size());
  for (const auto& n : m.nodes) {
    writeWords(out, n.plane); writeWords(out, n.zoneMask); out.push_back(n.flags);
    for (auto i : n.indices) index(out, i);
    writeWords(out, n.spheres);
    for (auto z : n.zones) out.push_back(z);
    out.push_back(n.vertexCount); writeWords(out, n.leaves); writeWords(out, n.renderIndices);
  }
  size(out, m.surfaces.size());
  for (const auto& s : m.surfaces) {
    index(out, reference(s.material)); u32(out, s.flags);
    for (auto i : s.indices) index(out, i);
    index(out, reference(s.actor)); writeWords(out, s.plane);
    u32(out, s.lightMapScale); u32(out, s.licenseWord);
  }
  size(out, m.vertices.size());
  for (const auto& v : m.vertices) for (auto i : v) index(out, i);
  u32(out, m.sharedSides); u32(out, 0); index(out, reference(m.polys));
  size(out, m.renderBounds.size()); for (const auto& b : m.renderBounds) box(out, b);
  std::size_t hullWords = 0;
  for (const auto& h : m.hulls) hullWords += h.planes.size() + 7;
  size(out, hullWords);
  for (const auto& h : m.hulls) {
    writeWords(out, h.planes); u32(out, 0xffffffffU); writeWords(out, h.bounds);
  }
  index(out, 0); index(out, 0); u32(out, m.rootOutside); u32(out, m.linked);
  for (int i = 0; i < 3; ++i) index(out, 0);
}
}
