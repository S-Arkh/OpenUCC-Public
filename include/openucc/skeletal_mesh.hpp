#pragma once
#include "openucc/package.hpp"
#include "openucc/animation.hpp"
#include <array>
#include <functional>

namespace openucc {
struct SkeletalWedge {
  std::uint16_t vertex = 0;
  std::array<std::uint32_t, 2> uv{};
};
struct SkeletalInfluence {
  std::uint32_t weight = 0;
  std::uint16_t point = 0, bone = 0;
};
struct SkeletalSection {
  std::array<std::uint16_t, 9> fields{};
  std::vector<std::uint32_t> bones;
};
struct SkeletalIndexBuffer {
  std::vector<std::uint16_t> indices;
  std::uint32_t revision = 0;
};
struct SkeletalLineageWedge {
  std::array<std::uint32_t, 8> vectors{};
  std::array<std::uint8_t, 4> bones{};
  std::array<std::uint32_t, 4> weights{};
};
struct SkeletalLOD {
  std::vector<std::uint32_t> skinning;
  std::vector<std::array<std::uint32_t, 4>> skinPoints;
  std::uint32_t softWedges = 0;
  std::vector<SkeletalSection> softSections, rigidSections;
  SkeletalIndexBuffer softIndices, rigidIndices;
  std::array<std::uint32_t, 3> vertexHeader{};
  std::vector<std::array<std::uint32_t, 8>> vertices;
  std::vector<SkeletalInfluence> influences;
  std::vector<SkeletalWedge> wedges;
  std::vector<std::array<std::uint16_t, 4>> faces;
  std::vector<std::array<std::uint32_t, 3>> points;
  std::array<std::uint32_t, 6> fields{};
  std::uint32_t newWedges = 0;
  std::vector<SkeletalLineageWedge> lineageWedges;
};
struct SkeletalBone {
  std::string name;
  std::uint32_t flags = 0;
  std::array<std::uint32_t, 11> pose{};
  std::uint32_t children = 0, parent = 0;
};
struct SkeletalWeightIndex {
  std::vector<std::uint16_t> indices;
  std::uint32_t start = 0;
};
struct SkeletalTriangle {
  std::array<std::uint16_t, 3> wedges{};
  std::uint8_t material = 0, auxiliaryMaterial = 0;
  std::uint32_t smoothing = 0;
};
// Observed 133/40 LodMesh versions 5/6/7 with zero/one/two extension bytes.
// Floating-point components are represented by their original 32-bit words.
struct LodMeshBody {
  int layout = 133;
  std::array<std::uint32_t, 6> box{};
  std::uint8_t boxValid = 0;
  std::array<std::uint32_t, 4> sphere{};
  std::uint32_t version = 7, vertexCount = 0;
  std::vector<std::array<std::uint32_t, 3>> vertices;
  std::vector<std::uint32_t> originalPackedVertices;
  std::vector<int> textures;
  std::array<std::uint32_t, 9> transforms{};
  std::vector<std::uint16_t> faceLevels, collapseWedges;
  std::vector<std::array<std::uint16_t, 4>> faces;
  std::vector<SkeletalWedge> wedges;
  std::vector<std::array<std::uint32_t, 2>> materials;
  std::array<std::uint32_t, 6> lodFields{};
  std::uint32_t hasImpostor = 0;
  int spriteMaterial = 0;
  std::array<std::uint32_t, 9> impostorTransforms{};
  std::array<std::uint8_t, 4> impostorColor{};
  std::array<std::uint32_t, 3> impostorModes{};
  std::uint32_t skinTessellation = 0, extensionWord = 0;
  std::array<std::uint8_t, 2> extensionBytes{};
};
struct SkeletalMeshBody : LodMeshBody {
  std::vector<std::array<std::uint32_t, 3>> points2;
  std::vector<SkeletalBone> bones;
  int animation = 0;
  std::uint32_t depth = 0;
  std::vector<SkeletalWeightIndex> weightIndices;
  std::vector<std::array<std::uint16_t, 2>> boneInfluences;
  std::vector<std::string> attachAliases, attachBones;
  std::vector<std::array<std::uint32_t, 12>> attachCoords;
  std::vector<SkeletalLOD> lods;
  int auxiliaryObject = 0;
  std::vector<std::array<std::uint32_t, 3>> points;
  std::vector<SkeletalWedge> meshWedges;
  std::vector<SkeletalTriangle> triangles;
  std::vector<SkeletalInfluence> influences;
  std::vector<std::uint16_t> collapse, faceRemap;
  std::uint32_t tailFirst = 0;
  std::vector<std::uint32_t> tailFloats;
  std::array<std::uint32_t, 2> tailWords{};
};
struct VertMeshBox {
  std::array<std::uint32_t, 6> words{};
  std::uint8_t valid = 0;
};
struct VertMeshBody : LodMeshBody {
  std::vector<std::array<std::uint32_t, 8>> animatedVertices;
  std::uint32_t streamVersion = 0;
  std::vector<std::uint32_t> packedVertices, floats, normals;
  std::vector<AnimationSequence> sequences;
  std::uint32_t animatedVertexCount = 0, frameCount = 0;
  std::vector<VertMeshBox> boxes;
  std::vector<std::array<std::uint32_t, 4>> spheres;
};
VertMeshBody decodeVertMesh(Reader&, const Package&, const Export&);
void encodeVertMesh(const VertMeshBody&, Package&, Bytes&, Export&, const std::function<int(int)>&);
SkeletalMeshBody decodeSkeletalMesh(Reader&, const Package&, const Export&);
void encodeSkeletalMesh(const SkeletalMeshBody&, Package&, Bytes&, Export&,
                        const std::function<int(int)>&);
}
