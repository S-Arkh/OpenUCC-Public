#pragma once
#include <array>
#include <functional>

#include "package.hpp"
#include "static_mesh.hpp"
#include "animation.hpp"
#include "skeletal_mesh.hpp"
#include "model.hpp"

namespace openucc {
struct ResourceMatrixPlane {
  std::string name;
  std::vector<std::pair<std::string, std::uint32_t>> components;
};
struct ResourceTag {
  std::string name, structure, nameValue;
  int kind = 0, subscript = -1, reference = 0;
  bool boolean = false;
  int booleanSize = 0;
  Bytes value;
  std::vector<std::vector<ResourceTag>> elements;
  std::vector<ResourceMatrixPlane> matrix;
  std::vector<int> references;
  std::vector<std::string> strings;
  std::vector<std::uint32_t> words;
  std::vector<std::pair<std::string, std::uint8_t>> byteFields;
};
struct ResourceMip {
  Bytes pixels;
  std::uint32_t width = 0, height = 0;
  std::uint8_t widthBits = 0, heightBits = 0;
};
struct ResourceGlyph {
  std::array<std::uint32_t, 4> rectangle{};
  std::uint8_t page = 0;
};
struct ResourcePolygon {
  std::array<std::uint32_t, 12> vectors{};
  std::vector<std::array<std::uint32_t, 3>> vertices;
  std::uint32_t flags = 0, lightMapScale = 0, licenseWord = 0;
  int actor = 0, material = 0, link = 0, brushPolygon = 0;
  std::string itemName;
};
struct ResourceBody {
  std::string kind;
  std::vector<ResourceTag> tags;
  std::array<std::uint8_t, 2> materialHeader{};
  std::uint32_t obsoleteMaterialWord = 0;
  // Native legacy loading discards these individually serialized byte fields.
  // Their fixed shape is known; they contain no archive references or offsets.
  int legacyMaterialLicensee = 0;
  std::uint8_t legacyMaterialFlag = 0;
  std::array<std::uint8_t, 18> legacyMaterialHeader{};
  struct LegacyStage {
    std::uint8_t first = 0, second = 0;
    std::array<std::uint8_t, 126> fields{};
  };
  std::array<LegacyStage, 8> legacyMaterialStages{};
  std::array<std::string, 16> legacyMaterialStrings{};
  int textureLayout = 133;
  std::array<std::uint8_t, 2> singleStageBytes{};
  std::array<std::uint32_t, 3> singleStageWords{};
  std::array<std::array<std::uint8_t, 2>, 5> transitionalStageBytes{};
  std::array<std::array<std::uint32_t, 3>, 5> transitionalStageWords{};
  std::array<std::array<std::uint8_t, 6>, 5> stageBytes{};
  std::array<std::array<std::uint32_t, 2>, 5> stageWords{};
  std::array<std::uint8_t, 8> materialColors{};
  std::array<std::uint32_t, 3> materialWords{};
  std::vector<std::pair<std::string, std::vector<std::string>>> stageStrings;
  std::string shader;
  std::uint32_t materialVersion = 0;
  std::vector<ResourceMip> mips;
  std::vector<std::array<std::uint8_t, 4>> colors;
  std::vector<ResourceGlyph> glyphs;
  std::vector<int> pages;
  StaticMeshBody mesh;
  ModelBody model;
  int modelLayout = 123;
  Model133Body model133;
  // Unverified Model layouts are exposed as opaque saved bytes. Rewriting
  // an individual payload cannot safely relocate unknown references/offsets.
  bool modelOpaque = false;
  std::string modelWarning;
  Bytes modelOpaquePayload;
  AnimationBody animation;
  SkeletalMeshBody skeletalMesh;
  VertMeshBody vertMesh;
  std::string soundFormat;
  Bytes soundData;
  std::uint32_t polygonCapacity = 0;
  std::vector<ResourcePolygon> polygons;
  std::array<std::uint32_t, 9> karmaScalarBits{};
};
// Strict native codecs for the observed 123/37 and 133/40 Texture, Palette and
// Font layouts and empty Package groups, plus plain 126/0 Texture streams
// and observed early 123/{12,19,23} embedded textures.
// Unknown property payloads and font suffixes are rejected.
ResourceBody decodeResource(const Package&, int id);
bool materialResource(const std::string& kind);
bool animationNotifyResource(const std::string& kind);
void encodeResource(const ResourceBody&, Package&, Export&,
                    const std::function<int(int)>& reference);
}  // namespace openucc
