#include "openucc/model.hpp"
#include <algorithm>
#include <map>
#include <stdexcept>

namespace openucc {
bool isModel133AdjacentProfile(const Package& p) {
  return ((p.version == 127 || p.version == 128) && p.licensee == 39) ||
         ((p.version == 130 || p.version == 132) && p.licensee == 40);
}
bool isModel133FamilyProfile(const Package& p) {
  return (p.version == 133 && p.licensee == 40) || isModel133AdjacentProfile(p);
}
namespace {
void check133(bool value, const char* message) {
  if (!value) throw std::runtime_error(message);
}
int index133(Reader& r) {
  auto start = r.pos;
  int value = r.index();
  Bytes canonical; index(canonical, value);
  check133(r.pos - start == canonical.size() &&
           std::equal(canonical.begin(), canonical.end(), r.data.begin() + start),
           "Noncanonical Model 133 compact index");
  return value;
}
std::size_t count133(Reader& r, std::size_t stride, std::size_t maximum = 1000000) {
  int n = index133(r);
  check133(n >= 0 && std::size_t(n) <= maximum &&
           std::size_t(n) <= (r.data.size() - r.pos) / stride,
           "Invalid Model 133 array count");
  return std::size_t(n);
}
void size133(Bytes& out, std::size_t n, std::size_t maximum = 1000000) {
  check133(n <= maximum, "Excessive Model 133 array");
  index(out, int(n));
}
template<class T> void readWords133(Reader& r, T& values) {
  for (auto& value : values) value = r.u32();
}
template<class T> void writeWords133(Bytes& out, const T& values) {
  for (auto value : values) u32(out, value);
}
MeshBox readBox133(Reader& r) {
  MeshBox box; readWords133(r, box.coordinates); box.valid = r.byte();
  check133(box.valid <= 1, "Invalid Model 133 box flag");
  return box;
}
void writeBox133(Bytes& out, const MeshBox& box) {
  check133(box.valid <= 1, "Invalid Model 133 box flag");
  writeWords133(out, box.coordinates); out.push_back(box.valid);
}
int readRef133(Reader& r, const Package& p) {
  int result = index133(r); p.ref(result); return result;
}
void writeRef133(Bytes& out, const Package& p, const std::function<int(int)>& reference, int id) {
  int result = reference(id); p.ref(result); index(out, result);
}
bool ordinal133(int value, std::size_t count) {
  return value >= 0 && std::size_t(value) < count;
}
void validateGeometry133(const ModelBody& m) {
  std::map<int, const BspHull*> hullOffsets;
  std::size_t offset = 0;
  for (const auto& hull : m.hulls) {
    check133(offset <= 1000000, "Excessive Model 133 hull words");
    hullOffsets.emplace(int(offset), &hull);
    for (auto plane : hull.planes)
      check133(plane != 0xffffffffU, "Model 133 hull plane is a terminator");
    offset += hull.planes.size() + 7;
  }
  check133(offset <= 1000000, "Excessive Model 133 hull words");
  for (const auto& surface : m.surfaces) {
    check133(ordinal133(surface.indices[0], m.points.size()), "Invalid Model 133 surface point");
    for (int i = 1; i <= 3; ++i)
      check133(ordinal133(surface.indices[i], m.vectors.size()), "Invalid Model 133 surface vector");
  }
  for (const auto& node : m.nodes) {
    check133(ordinal133(node.indices[1], m.surfaces.size()), "Invalid Model 133 surface index");
    check133(node.indices[0] >= 0 && std::size_t(node.indices[0]) <= m.vertices.size() &&
             node.vertexCount <= m.vertices.size() - std::size_t(node.indices[0]),
             "Invalid Model 133 vertex range");
    for (int i = 2; i <= 4; ++i)
      check133(node.indices[i] == -1 || ordinal133(node.indices[i], m.nodes.size()),
               "Invalid Model 133 node link");
    if (node.indices[5] != -1) {
      auto hull = hullOffsets.find(node.indices[5]);
      check133(hull != hullOffsets.end(), "Invalid Model 133 hull offset");
      for (auto plane : hull->second->planes)
        check133((plane & 0x3fffffffU) < m.nodes.size(), "Invalid Model 133 node-linked hull plane");
    }
    check133(node.indices[6] == -1 || ordinal133(node.indices[6], m.renderBounds.size()),
             "Invalid Model 133 render bound");
    for (std::size_t i = 0; i < node.vertexCount; ++i)
      check133(ordinal133(m.vertices[std::size_t(node.indices[0]) + i][0], m.points.size()),
               "Invalid Model 133 node-used point");
  }
}
void validateProfile133(const Package& p) {
  check133(isModel133FamilyProfile(p), "Unsupported Model 133-family profile");
}
void validateAdjacent133(const Model133Body& m, const Package& p) {
  if (!isModel133AdjacentProfile(p)) return;
  check133(m.zones.empty() && m.leaves.empty() && m.lights.empty() &&
           m.sections.empty() && m.lightmaps.empty() && m.lightmapGroups.empty() &&
           m.trailer.pairs.empty() && m.trailer.finalWord == 0,
           "Unsupported nonempty adjacent-profile Model arrays or suffix");
}
}

Model133Body decodeModel133(Reader& r, const Package& p, const Export& e) {
  validateProfile133(p);
  Model133Body model; auto& m = model.geometry;
  m.bounds = readBox133(r); readWords133(r, m.sphere);
  for (auto* array : {&m.vectors, &m.points}) {
    array->resize(count133(r, 12));
    for (auto& value : *array) readWords133(r, value);
  }
  m.nodes.resize(count133(r, 87));
  for (auto& node : m.nodes) {
    readWords133(r, node.plane); readWords133(r, node.zoneMask); node.flags = r.byte();
    for (auto& value : node.indices) value = index133(r);
    readWords133(r, node.spheres);
    for (auto& zone : node.zones) zone = r.byte();
    node.vertexCount = r.byte(); readWords133(r, node.leaves); readWords133(r, node.renderIndices);
  }
  m.surfaces.resize(count133(r, 35));
  for (auto& surface : m.surfaces) {
    surface.material = readRef133(r, p); surface.flags = r.u32();
    for (auto& value : surface.indices) value = index133(r);
    surface.actor = readRef133(r, p); readWords133(r, surface.plane);
    surface.lightMapScale = r.u32(); surface.licenseWord = r.u32();
  }
  m.vertices.resize(count133(r, 2));
  for (auto& vertex : m.vertices) for (auto& value : vertex) value = index133(r);
  m.sharedSides = r.u32();
  auto zoneCount = r.u32();
  check133(zoneCount <= 64 && std::size_t(zoneCount) <= (r.data.size() - r.pos) / 21,
            "Invalid Model 133 zone count");
  check133(!isModel133AdjacentProfile(p) || zoneCount == 0,
           "Unsupported adjacent-profile Model zones");
  model.zones.resize(zoneCount);
  for (auto& zone : model.zones) { zone.actor = readRef133(r, p); readWords133(r, zone.words); }
  m.polys = readRef133(r, p);
  m.renderBounds.resize(count133(r, 25));
  for (auto& bounds : m.renderBounds) bounds = readBox133(r);
  auto hullWords = count133(r, 4);
  while (hullWords) {
    BspHull hull;
    for (;;) {
      check133(hullWords > 0, "Missing Model 133 hull terminator");
      auto word = r.u32(); --hullWords;
      if (word == 0xffffffffU) break;
      hull.planes.push_back(word);
    }
    check133(hullWords >= 6, "Truncated Model 133 hull bounds");
    readWords133(r, hull.bounds); hullWords -= 6; m.hulls.push_back(std::move(hull));
  }
  auto leafCount = count133(r, 11);
  check133(!isModel133AdjacentProfile(p) || leafCount == 0,
           "Unsupported adjacent-profile Model leaves");
  model.leaves.resize(leafCount);
  for (auto& leaf : model.leaves) {
    for (auto& value : leaf.indices) value = index133(r);
    readWords133(r, leaf.visibleZones);
  }
  auto lightCount = count133(r, 1);
  check133(!isModel133AdjacentProfile(p) || lightCount == 0,
           "Unsupported adjacent-profile Model lights");
  model.lights.resize(lightCount);
  for (auto& light : model.lights) light = readRef133(r, p);
  m.rootOutside = r.u32(); m.linked = r.u32();
  auto sectionCount = count133(r, 17, 65536);
  check133(!isModel133AdjacentProfile(p) || sectionCount == 0,
           "Unsupported adjacent-profile Model sections");
  model.sections.resize(sectionCount);
  for (auto& section : model.sections) {
    section.vertices.resize(count133(r, 40));
    for (auto& vertex : section.vertices) readWords133(r, vertex);
    section.revision = r.u32(); section.material = readRef133(r, p);
    readWords133(r, section.words);
  }
  auto lightmapCount = count133(r, 113);
  check133(!isModel133AdjacentProfile(p) || lightmapCount == 0,
           "Unsupported adjacent-profile Model lightmaps");
  model.lightmaps.resize(lightmapCount);
  for (auto& lightmap : model.lightmaps) {
    for (auto& value : lightmap.indices) value = index133(r);
    readWords133(r, lightmap.matrixAndVectors);
    lightmap.masks.resize(count133(r, 30, 100000));
    for (auto& mask : lightmap.masks) {
      mask.light = readRef133(r, p);
      mask.bitmap = r.take(count133(r, 1, 64 * 1024 * 1024));
      readWords133(r, mask.words);
    }
    lightmap.level = readRef133(r, p); lightmap.revision = r.u32();
  }
  auto groupCount = count133(r, 3, 65536);
  check133(!isModel133AdjacentProfile(p) || groupCount == 0,
           "Unsupported adjacent-profile Model lightmap groups");
  model.lightmapGroups.resize(groupCount);
  for (auto& group : model.lightmapGroups) {
    group.pages.resize(count133(r, 28, 4096));
    for (auto& page : group.pages) {
      page.level = readRef133(r, p);
      page.indices.resize(count133(r, 4)); readWords133(r, page.indices);
      readWords133(r, page.words);
      for (std::size_t i = 0; i < page.data.size(); ++i) {
        auto end = r.u32(); page.hasLazyOffset[i] = end != 0;
        page.data[i] = r.take(count133(r, 1, 64 * 1024 * 1024));
        check133(!end || std::uint64_t(end) == std::uint64_t(e.sourceOffset) + r.pos,
                 "Invalid Model 133 lazy offset");
      }
      page.format = r.byte(); readWords133(r, page.dimensionsAndRevision);
    }
    group.indices.resize(count133(r, 4)); readWords133(r, group.indices);
    group.level = readRef133(r, p);
  }
  model.trailer = decodeModel133Trailer(r, p);
  validateGeometry133(m);
  validateAdjacent133(model, p);
  return model;
}

void encodeModel133(const Model133Body& model, const Package& p, Bytes& out, Export& e,
                    const std::function<int(int)>& reference) {
  validateProfile133(p);
  const auto& m = model.geometry;
  validateGeometry133(m);
  validateAdjacent133(model, p);
  check133(model.zones.size() <= 64, "Excessive Model 133 zones");
  writeBox133(out, m.bounds); writeWords133(out, m.sphere);
  for (auto* array : {&m.vectors, &m.points}) {
    size133(out, array->size());
    for (const auto& value : *array) writeWords133(out, value);
  }
  size133(out, m.nodes.size());
  for (const auto& node : m.nodes) {
    writeWords133(out, node.plane); writeWords133(out, node.zoneMask); out.push_back(node.flags);
    for (auto value : node.indices) index(out, value);
    writeWords133(out, node.spheres);
    for (auto zone : node.zones) out.push_back(zone);
    out.push_back(node.vertexCount); writeWords133(out, node.leaves); writeWords133(out, node.renderIndices);
  }
  size133(out, m.surfaces.size());
  for (const auto& surface : m.surfaces) {
    writeRef133(out, p, reference, surface.material); u32(out, surface.flags);
    for (auto value : surface.indices) index(out, value);
    writeRef133(out, p, reference, surface.actor); writeWords133(out, surface.plane);
    u32(out, surface.lightMapScale); u32(out, surface.licenseWord);
  }
  size133(out, m.vertices.size());
  for (const auto& vertex : m.vertices) for (auto value : vertex) index(out, value);
  u32(out, m.sharedSides); u32(out, std::uint32_t(model.zones.size()));
  for (const auto& zone : model.zones) {
    writeRef133(out, p, reference, zone.actor); writeWords133(out, zone.words);
  }
  writeRef133(out, p, reference, m.polys);
  size133(out, m.renderBounds.size());
  for (const auto& bounds : m.renderBounds) writeBox133(out, bounds);
  std::size_t hullWords = 0;
  for (const auto& hull : m.hulls) hullWords += hull.planes.size() + 7;
  size133(out, hullWords);
  for (const auto& hull : m.hulls) {
    writeWords133(out, hull.planes); u32(out, 0xffffffffU); writeWords133(out, hull.bounds);
  }
  size133(out, model.leaves.size());
  for (const auto& leaf : model.leaves) {
    for (auto value : leaf.indices) index(out, value);
    writeWords133(out, leaf.visibleZones);
  }
  size133(out, model.lights.size());
  for (auto light : model.lights) writeRef133(out, p, reference, light);
  u32(out, m.rootOutside); u32(out, m.linked);
  size133(out, model.sections.size(), 65536);
  for (const auto& section : model.sections) {
    size133(out, section.vertices.size());
    for (const auto& vertex : section.vertices) writeWords133(out, vertex);
    u32(out, section.revision); writeRef133(out, p, reference, section.material);
    writeWords133(out, section.words);
  }
  size133(out, model.lightmaps.size());
  for (const auto& lightmap : model.lightmaps) {
    for (auto value : lightmap.indices) index(out, value);
    writeWords133(out, lightmap.matrixAndVectors);
    size133(out, lightmap.masks.size(), 100000);
    for (const auto& mask : lightmap.masks) {
      writeRef133(out, p, reference, mask.light);
      size133(out, mask.bitmap.size(), 64 * 1024 * 1024);
      out.insert(out.end(), mask.bitmap.begin(), mask.bitmap.end());
      writeWords133(out, mask.words);
    }
    writeRef133(out, p, reference, lightmap.level); u32(out, lightmap.revision);
  }
  size133(out, model.lightmapGroups.size(), 65536);
  for (const auto& group : model.lightmapGroups) {
    size133(out, group.pages.size(), 4096);
    for (const auto& page : group.pages) {
      writeRef133(out, p, reference, page.level);
      size133(out, page.indices.size()); writeWords133(out, page.indices);
      writeWords133(out, page.words);
      for (std::size_t i = 0; i < page.data.size(); ++i) {
        auto start = out.size(); u32(out, 0);
        size133(out, page.data[i].size(), 64 * 1024 * 1024);
        out.insert(out.end(), page.data[i].begin(), page.data[i].end());
        if (page.hasLazyOffset[i]) e.lazyOffsets.push_back({start, out.size()});
      }
      out.push_back(page.format); writeWords133(out, page.dimensionsAndRevision);
    }
    size133(out, group.indices.size()); writeWords133(out, group.indices);
    writeRef133(out, p, reference, group.level);
  }
  encodeModel133Trailer(model.trailer, p, out);
}
}  // namespace openucc
