#pragma once
#include "static_mesh.hpp"

namespace openucc {
struct BspNode {
  std::array<std::uint32_t, 4> plane{};
  std::array<std::uint32_t, 2> zoneMask{};
  std::uint8_t flags = 0;
  // Vertex pool, surface, back, front, coplanar, collision bound, render bound.
  std::array<int, 7> indices{};
  std::array<std::uint32_t, 8> spheres{};
  std::array<std::uint8_t, 2> zones{};
  std::uint8_t vertexCount = 0;
  std::array<std::uint32_t, 2> leaves{};
  std::array<std::uint32_t, 3> renderIndices{};
};
struct BspSurface {
  int material = 0, actor = 0;
  std::uint32_t flags = 0;
  // Base point, normal, texture U, texture V, brush polygon.
  std::array<int, 5> indices{};
  std::array<std::uint32_t, 4> plane{};
  std::uint32_t lightMapScale = 0, licenseWord = 0;
};
struct BspHull {
  std::vector<std::uint32_t> planes;
  std::array<std::uint32_t, 6> bounds{};
};
struct ModelBody {
  MeshBox bounds;
  std::array<std::uint32_t, 4> sphere{};
  std::vector<std::array<std::uint32_t, 3>> vectors, points;
  std::vector<BspNode> nodes;
  std::vector<BspSurface> surfaces;
  std::vector<std::array<int, 2>> vertices;
  std::uint32_t sharedSides = 0;
  int polys = 0;
  std::vector<MeshBox> renderBounds;
  std::vector<BspHull> hulls;
  std::uint32_t rootOutside = 0, linked = 0;
};
// Final 133/40 fields only. The caller must independently establish the
// preceding boundary; this does not decode or validate a complete Model.
struct Model133Trailer {
  std::vector<std::array<std::uint32_t, 2>> pairs;
  std::uint32_t finalWord = 0;
};
// The four adjacent profiles are restricted to the observed empty-array
// brush form. This identifies a bounded wire layout, not runtime semantics.
bool isModel133AdjacentProfile(const Package&);
bool isModel133FamilyProfile(const Package&);
Model133Trailer decodeModel133Trailer(Reader&, const Package&);
void encodeModel133Trailer(const Model133Trailer&, const Package&, Bytes&);

// Structured 133/40 native fields. Names for unknown scalar meanings are
// intentionally neutral; no runtime or collision behavior is inferred.
struct Model133Zone {
  int actor = 0;
  std::array<std::uint32_t, 5> words{};
};
struct Model133Leaf {
  std::array<int, 3> indices{};
  std::array<std::uint32_t, 2> visibleZones{};
};
struct Model133Section {
  std::vector<std::array<std::uint32_t, 10>> vertices;
  std::uint32_t revision = 0;
  int material = 0;
  std::array<std::uint32_t, 3> words{};
};
struct Model133LightMask {
  int light = 0;
  Bytes bitmap;
  std::array<std::uint32_t, 7> words{};
};
struct Model133Lightmap {
  std::array<int, 7> indices{};
  std::array<std::uint32_t, 25> matrixAndVectors{};
  std::vector<Model133LightMask> masks;
  int level = 0;
  std::uint32_t revision = 0;
};
struct Model133LightmapPage {
  int level = 0;
  std::vector<std::uint32_t> indices;
  std::array<std::uint32_t, 3> words{};
  std::array<Bytes, 2> data;
  std::array<bool, 2> hasLazyOffset{{true, true}};
  std::uint8_t format = 0;
  std::array<std::uint32_t, 3> dimensionsAndRevision{};
};
struct Model133LightmapGroup {
  std::vector<Model133LightmapPage> pages;
  std::vector<std::uint32_t> indices;
  int level = 0;
};
struct Model133Body {
  ModelBody geometry;
  std::vector<Model133Zone> zones;
  std::vector<Model133Leaf> leaves;
  std::vector<int> lights;
  std::vector<Model133Section> sections;
  std::vector<Model133Lightmap> lightmaps;
  std::vector<Model133LightmapGroup> lightmapGroups;
  Model133Trailer trailer;
};
// Complete structural fields for 133/40 and the bounded empty-array brush
// form of four adjacent profiles. No runtime or collision behavior is inferred.
Model133Body decodeModel133(Reader&, const Package&, const Export&);
void encodeModel133(const Model133Body&, const Package&, Bytes&, Export&,
                    const std::function<int(int)>&);
// Observed 123/37 brush models only: empty zones, leaves, lights and render arrays.
ModelBody decodeModel(Reader&, const Package&);
void encodeModel(const ModelBody&, const Package&, Bytes&, const std::function<int(int)>&);
}
