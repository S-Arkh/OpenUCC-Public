#pragma once
#include <array>
#include <functional>
#include "package.hpp"

namespace openucc {
// Float components are stored as bits. These records describe serialization,
// including ordinal scalar slots whose engine semantics are not yet named.
struct MeshBox {
  std::array<std::uint32_t, 6> coordinates{};
  std::uint8_t valid = 0;
};
struct MeshSection {
  std::uint32_t flags = 0;
  std::array<std::uint16_t, 5> values{};
};
struct MeshUvStream {
  std::vector<std::array<std::uint32_t, 2>> coordinates;
  std::uint32_t index = 0, revision = 0;
};
struct MeshCollisionFace {
  std::array<std::uint32_t, 16> planes{};
  std::array<int, 4> indices{};
};
struct MeshCollisionNode {
  std::array<int, 4> indices{};
  MeshBox bounds;
};
struct MeshTriangle {
  std::array<std::uint32_t, 9> positions{};
  std::vector<std::array<std::uint32_t, 6>> uv;
  std::array<std::uint32_t, 3> colors{};
  std::uint32_t material = 0, smoothing = 0;
};
struct StaticMeshBody {
  int layout = 133;
  int licensee = 40;
  MeshBox bounds, meshBounds;
  std::array<std::uint32_t, 4> sphere{};
  std::vector<MeshSection> sections;
  std::vector<std::array<std::uint32_t, 6>> vertices;
  std::uint32_t vertexRevision = 0;
  std::array<std::vector<std::uint32_t>, 2> colors;
  std::array<std::uint32_t, 2> colorRevisions{};
  std::vector<MeshUvStream> uv;
  std::array<std::vector<std::uint16_t>, 2> indices;
  std::array<std::uint32_t, 2> indexRevisions{};
  int collisionModel = 0;
  std::vector<MeshCollisionFace> collisionFaces;
  std::vector<MeshCollisionNode> collisionNodes;
  std::uint32_t licenseWord = 0;
  std::array<int, 2> licenseReferences{};
  std::array<std::uint32_t, 10> licenseWords{};
  std::vector<MeshTriangle> triangles;
  std::uint32_t internalVersion = 0;
  int trailingReference = 0;
  std::uint32_t trailingWord = 0;
  std::vector<std::array<std::uint32_t, 2>> auxiliaryUv;
  std::array<std::uint32_t, 4> auxiliaryWords{};
};
StaticMeshBody decodeStaticMesh(Reader&, const Package&, const Export&);
bool staticMeshProfile(int version, int licensee);
void encodeStaticMesh(const StaticMeshBody&, Bytes&, Export&,
                      const std::function<int(int)>&);
}
