#include "openucc/static_mesh.hpp"
#include <limits>
#include <stdexcept>
#include <set>

namespace openucc {
bool staticMeshProfile(int version, int licensee) {
  static const std::set<int> legacy = {12,14,15,16,18,19,20,22,23,25,28,31,33,35,36,37,38};
  return (version == 118 && licensee == 0) || (version == 123 && legacy.count(licensee)) ||
      (version == 125 && (licensee == 38 || licensee == 39)) ||
      (version >= 126 && version <= 128 && licensee == 39) ||
      (version >= 130 && version <= 133 && licensee == 40);
}
namespace {
std::size_t licenseWords(int licensee) {
  if (licensee < 6) return 0;
  return 2 + (licensee >= 7 ? 2 : 0) + (licensee >= 11) + (licensee >= 13) +
      (licensee >= 14 ? 2 : 0) + (licensee >= 15) + (licensee >= 37);
}
void check(bool v, const char* message) {
  if (!v) throw std::runtime_error(message);
}
std::size_t count(Reader& r, std::size_t stride, std::size_t maximum = 1000000) {
  auto n = r.index();
  check(n >= 0 && std::size_t(n) <= maximum &&
            std::size_t(n) <= (r.data.size() - r.pos) / stride,
        "Invalid StaticMesh array size");
  return std::size_t(n);
}
template <class T> void readWords(Reader& r, T& values) {
  for (auto& value : values) value = r.u32();
}
template <class T> void writeWords(Bytes& b, const T& values) {
  for (auto value : values) u32(b, value);
}
MeshBox readBox(Reader& r) {
  MeshBox b; readWords(r, b.coordinates); b.valid = r.byte();
  check(b.valid <= 1, "Invalid StaticMesh box flag"); return b;
}
void writeBox(Bytes& out, const MeshBox& b) {
  check(b.valid <= 1, "Invalid StaticMesh box flag");
  writeWords(out, b.coordinates); out.push_back(b.valid);
}
void arraySize(Bytes& b, std::size_t n) {
  check(n <= 1000000, "Excessive StaticMesh array size"); index(b, int(n));
}
void lazyEnd(const Reader& r, const Export& e, std::uint32_t end) {
  check(!end || end == e.sourceOffset + r.pos, "Invalid StaticMesh lazy offset");
}
void validate(const StaticMeshBody& m) {
  check(m.vertices.size() <= 65536 && m.uv.size() <= 8,
        "Invalid StaticMesh stream count");
  for (const auto& stream : m.indices)
    for (auto i : stream) check(i < m.vertices.size(), "Invalid StaticMesh vertex index");
  for (const auto& s : m.sections) {
    std::size_t first = s.values[0], faces = s.values[4];
    check(first <= m.indices[0].size() && faces <= (m.indices[0].size() - first) / 3,
          "Invalid StaticMesh section range");
    check(!faces || (s.values[1] <= s.values[2] && s.values[2] < m.vertices.size()),
          "Invalid StaticMesh section vertices");
  }
  for (const auto& f : m.collisionFaces) {
    for (int i = 0; i < 3; ++i)
      check(f.indices[i] >= 0 && std::size_t(f.indices[i]) < m.vertices.size(),
            "Invalid StaticMesh collision index");
    check(f.indices[3] >= 0, "Invalid StaticMesh collision material");
  }
  for (const auto& t : m.triangles) check(t.uv.size() <= 8, "Invalid StaticMesh triangle UV count");
}
}

StaticMeshBody decodeStaticMesh(Reader& r, const Package& p, const Export& e) {
  check(staticMeshProfile(p.version, p.licensee), "Unsupported StaticMesh profile");
  StaticMeshBody m;
  m.layout = p.version;
  m.licensee = p.licensee;
  m.bounds = readBox(r); readWords(r, m.sphere);
  m.sections.resize(count(r, 14, 65536));
  for (auto& s : m.sections) { s.flags = r.u32(); for (auto& v : s.values) v = r.u16(); }
  m.meshBounds = readBox(r);
  m.vertices.resize(count(r, 24, 65536));
  for (auto& v : m.vertices) readWords(r, v);
  m.vertexRevision = r.u32();
  for (std::size_t i = 0; i < 2; ++i) {
    m.colors[i].resize(count(r, 4, 65536)); readWords(r, m.colors[i]);
    m.colorRevisions[i] = r.u32();
  }
  m.uv.resize(count(r, 9, 8));
  for (auto& stream : m.uv) {
    stream.coordinates.resize(count(r, 8, 65536));
    for (auto& uv : stream.coordinates) readWords(r, uv);
    stream.index = r.u32(); stream.revision = r.u32();
  }
  for (std::size_t i = 0; i < 2; ++i) {
    m.indices[i].resize(count(r, 2));
    for (auto& v : m.indices[i]) v = r.u16();
    m.indexRevisions[i] = r.u32();
  }
  auto reference = [&]() { int id = r.index(); p.ref(id); return id; };
  m.collisionModel = reference();
  auto end = m.licensee >= 17 ? r.u32() : 0; m.collisionFaces.resize(count(r, 68));
  for (auto& f : m.collisionFaces) {
    readWords(r, f.planes); for (auto& v : f.indices) v = r.index();
  }
  lazyEnd(r, e, end);
  end = m.licensee >= 17 ? r.u32() : 0; m.collisionNodes.resize(count(r, 29));
  for (auto& n : m.collisionNodes) {
    for (auto& v : n.indices) v = r.index();
    n.bounds = readBox(r);
  }
  lazyEnd(r, e, end);
  if (m.licensee >= 6) {
    m.licenseWord = r.u32(); for (auto& ref : m.licenseReferences) ref = reference();
    for (std::size_t i = 0; i < licenseWords(m.licensee); ++i) m.licenseWords[i] = r.u32();
  }
  end = r.u32(); m.triangles.resize(count(r, 60));
  for (auto& t : m.triangles) {
    readWords(r, t.positions); auto channels = r.i32();
    check(channels >= 0 && channels <= 8, "Invalid StaticMesh triangle UV count");
    t.uv.resize(std::size_t(channels)); for (auto& uv : t.uv) readWords(r, uv);
    readWords(r, t.colors); t.material = r.u32(); t.smoothing = r.u32();
  }
  lazyEnd(r, e, end);
  m.internalVersion = r.u32(); m.trailingReference = reference();
  if (m.layout >= 120) m.trailingWord = r.u32();
  if (m.layout >= 125) {
    m.auxiliaryUv.resize(count(r, 8)); for (auto& uv : m.auxiliaryUv) readWords(r, uv);
    readWords(r, m.auxiliaryWords);
  }
  check(r.pos == r.data.size(), "Trailing StaticMesh native bytes"); validate(m); return m;
}

void encodeStaticMesh(const StaticMeshBody& m, Bytes& b, Export& e,
                      const std::function<int(int)>& reference) {
  validate(m);
  check(staticMeshProfile(m.layout, m.licensee), "Unsupported StaticMesh layout");
  if (m.layout < 125) {
    check(m.auxiliaryUv.empty(), "Legacy StaticMesh cannot store auxiliary UVs");
    for (auto word : m.auxiliaryWords) check(word == 0, "Legacy StaticMesh cannot store auxiliary words");
  }
  if (m.licensee < 6) {
    check(m.licenseWord == 0 && m.licenseReferences == std::array<int,2>{}, "Absent StaticMesh license fields must be empty");
  }
  for (std::size_t i = licenseWords(m.licensee); i < m.licenseWords.size(); ++i)
    check(m.licenseWords[i] == 0, "Absent StaticMesh license word must be zero");
  check(m.layout >= 120 || m.trailingWord == 0, "Absent StaticMesh trailing word must be zero");
  writeBox(b, m.bounds); writeWords(b, m.sphere); arraySize(b, m.sections.size());
  for (const auto& s : m.sections) { u32(b, s.flags); for (auto v : s.values) u16(b, v); }
  writeBox(b, m.meshBounds); arraySize(b, m.vertices.size());
  for (const auto& v : m.vertices) writeWords(b, v);
  u32(b, m.vertexRevision);
  for (std::size_t i = 0; i < 2; ++i) {
    arraySize(b, m.colors[i].size()); writeWords(b, m.colors[i]); u32(b, m.colorRevisions[i]);
  }
  arraySize(b, m.uv.size());
  for (const auto& stream : m.uv) {
    arraySize(b, stream.coordinates.size()); for (const auto& uv : stream.coordinates) writeWords(b, uv);
    u32(b, stream.index); u32(b, stream.revision);
  }
  for (std::size_t i = 0; i < 2; ++i) {
    arraySize(b, m.indices[i].size()); for (auto v : m.indices[i]) u16(b, v); u32(b, m.indexRevisions[i]);
  }
  index(b, reference(m.collisionModel));
  auto start = b.size(); if (m.licensee >= 17) u32(b, 0); arraySize(b, m.collisionFaces.size());
  for (const auto& f : m.collisionFaces) { writeWords(b, f.planes); for (auto v : f.indices) index(b, v); }
  if (m.licensee >= 17) e.lazyOffsets.push_back({start, b.size()});
  start = b.size(); if (m.licensee >= 17) u32(b, 0); arraySize(b, m.collisionNodes.size());
  for (const auto& n : m.collisionNodes) { for (auto v : n.indices) index(b, v); writeBox(b, n.bounds); }
  if (m.licensee >= 17) e.lazyOffsets.push_back({start, b.size()});
  if (m.licensee >= 6) {
    u32(b, m.licenseWord); for (auto ref : m.licenseReferences) index(b, reference(ref));
    for (std::size_t i = 0; i < licenseWords(m.licensee); ++i) u32(b, m.licenseWords[i]);
  }
  start = b.size(); u32(b, 0); arraySize(b, m.triangles.size());
  for (const auto& t : m.triangles) {
    writeWords(b, t.positions); u32(b, std::uint32_t(t.uv.size()));
    for (const auto& uv : t.uv) writeWords(b, uv);
    writeWords(b, t.colors); u32(b, t.material); u32(b, t.smoothing);
  }
  e.lazyOffsets.push_back({start, b.size()});
  u32(b, m.internalVersion); index(b, reference(m.trailingReference));
  if (m.layout >= 120) u32(b, m.trailingWord);
  if (m.layout >= 125) {
    arraySize(b, m.auxiliaryUv.size()); for (const auto& uv : m.auxiliaryUv) writeWords(b, uv);
    writeWords(b, m.auxiliaryWords);
  }
}
}
