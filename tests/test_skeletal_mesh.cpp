#include "openucc/resources.hpp"
#include <iostream>
#include <stdexcept>
using namespace openucc;
namespace {
void check(bool ok) { if (!ok) throw std::runtime_error("Skeletal mesh check failed"); }
template<class F> void rejects(F f) {
  try { f(); } catch (const std::exception&) { return; }
  throw std::runtime_error("Malformed skeletal mesh accepted");
}
}
int main() {
  try {
    Package p; p.version = 133; p.licensee = 40; p.header.resize(56);
    Import cls; cls.package = p.intern("Engine"); cls.type = p.intern("Class");
    cls.name = p.intern("SkeletalMesh"); p.imports.push_back(cls);
    Export e; e.type = -1; e.name = p.intern("Mesh"); p.exports.push_back(e);
    for (unsigned version : {5u, 6u, 7u}) {
    ResourceBody resource; resource.kind = "SkeletalMesh";
    auto& m = resource.skeletalMesh;
    m.boxValid = 1; m.vertexCount = 1; m.textures = {1}; m.animation = 1;
    m.version = version;
    m.extensionBytes = {std::uint8_t(version >= 6 ? 17 : 0), std::uint8_t(version >= 7 ? 23 : 0)};
    m.bones.push_back({"Root", 5, {}, 0, 0});
    m.bones[0].pose[0] = 0x7fc01234; m.attachAliases = {"Socket"}; m.attachBones = {"Root"};
    m.attachCoords.push_back({}); m.attachCoords[0][0] = 0x80000000;
    m.points = {{{0x80000000, 0x7fc01234, 0}}}; m.meshWedges.push_back({0, {0, 0x3f800000}});
    m.influences.push_back({0x3f800000, 0, 0}); m.triangles.push_back({{0, 0, 0}, 0, 2, 0x12345678});
    m.collapse = {0}; m.faceRemap = {0}; m.tailFloats = {0x7fc05678};
    SkeletalLOD lod; lod.points = m.points; lod.wedges = m.meshWedges; lod.influences = m.influences;
    lod.faces.push_back({0, 0, 0, 0}); lod.skinning = {0xabcdef01}; lod.skinPoints.push_back({});
    lod.softSections.push_back({{}, {0}}); lod.softIndices.indices = {0, 0, 0};
    lod.lineageWedges.push_back({{}, {0, 0, 0, 0}, {0x3f800000, 0, 0, 0}});
    m.lods.push_back(lod);
    encodeResource(resource, p, p.exports[0], [](int id) { return id; });
    auto lazy = p.exports[0].lazyOffsets;
    check(lazy.size() == 10);
    auto original = Package::parse(p.serialize()); auto decoded = decodeResource(original, 1);
    check(decoded.skeletalMesh.points == m.points);
    check(decoded.skeletalMesh.extensionBytes == m.extensionBytes);
    auto moved = original;
    for (int i = 0; i < 130; ++i) moved.intern("Padding" + std::to_string(i));
    moved.exports.push_back(moved.exports[0]); moved.exports[1].name = moved.intern("Other");
    for (auto& output : moved.exports)
      encodeResource(decoded, moved, output, [](int id) { return id == 1 ? 2 : id; });
    auto parsed = Package::parse(moved.serialize()); auto result = decodeResource(parsed, 1).skeletalMesh;
    check(parsed.exports[0].sourceOffset != original.exports[0].sourceOffset);
    check(result.animation == 2 && result.textures == std::vector<int>{2});
    check(result.bones[0].pose[0] == 0x7fc01234 && result.attachCoords[0][0] == 0x80000000);
    check(result.lods[0].skinning[0] == 0xabcdef01 && result.triangles[0].auxiliaryMaterial == 2);
    for (auto location : lazy) {
      auto corrupt = original; corrupt.exports[0].payload[location.first] ^= 1;
      rejects([&] { decodeResource(corrupt, 1); });
    }
    auto corrupt = original; corrupt.exports[0].payload.push_back(0);
    rejects([&] { decodeResource(corrupt, 1); });
    if (version < 7) {
      resource.skeletalMesh.extensionBytes[version - 5] = 1;
      rejects([&] { encodeResource(resource, p, p.exports[0], [](int id) { return id; }); });
      resource.skeletalMesh.extensionBytes[version - 5] = 0;
    }
    resource.skeletalMesh.version = 4;
    rejects([&] { encodeResource(resource, p, p.exports[0], [](int id) { return id; }); });
    if (version == 5) {
      for (auto profile : {std::pair<int,int>{131,40}, {132,40}, {123,36}}) {
        auto legacy = decoded; auto target = original;
        target.version = profile.first; target.licensee = profile.second;
        legacy.skeletalMesh.layout = profile.first;
        legacy.skeletalMesh.originalPackedVertices = {0xffffffff, 0x80000000};
        encodeResource(legacy, target, target.exports[0], [](int id) { return id; });
        auto loaded = decodeResource(Package::parse(target.serialize()), 1);
        check(loaded.skeletalMesh.originalPackedVertices == legacy.skeletalMesh.originalPackedVertices);
        check(loaded.skeletalMesh.bones[0].pose == legacy.skeletalMesh.bones[0].pose);
        legacy.skeletalMesh.vertices.push_back({});
        rejects([&] { encodeResource(legacy, target, target.exports[0], [](int id) { return id; }); });
      }
    }
    }
    {
      p.imports[0].name = p.intern("VertMesh");
      ResourceBody resource; resource.kind = "VertMesh";
      auto& m = resource.vertMesh;
      m.vertices = {{{0x80000000, 0x7fc01234, 0x3f800000}}};
      m.transforms[8] = 0x12345678; m.textures = {1}; m.spriteMaterial = 1;
      m.packedVertices = {0xffffffff, 0x80000000}; m.normals = {0xaabbccdd};
      m.animatedVertices.push_back({}); m.animatedVertices[0][4] = 0x7fc05678;
      m.animatedVertexCount = 1; m.frameCount = 2;
      m.boxes.push_back({{0x80000000}, 1}); m.spheres.push_back({0x7fc01234});
      AnimationSequence sequence; sequence.name = "Run"; sequence.groups = {"Movement"};
      sequence.object = 1; sequence.events.push_back({0x3f000000, "Trigger", 1, {65, 0, 0xd800}});
      m.sequences.push_back(sequence);
      ResourceTag materials; materials.name = "Materials"; materials.kind = 9; materials.references = {1};
      resource.tags.push_back(materials);
      encodeResource(resource, p, p.exports[0], [](int id) { return id; });
      auto original = Package::parse(p.serialize()); auto decoded = decodeResource(original, 1);
      auto moved = original;
      for (int i = 0; i < 130; ++i) moved.intern("VertPadding" + std::to_string(i));
      moved.exports.push_back(moved.exports[0]); moved.exports[1].name = moved.intern("OtherVert");
      for (auto& output : moved.exports)
        encodeResource(decoded, moved, output, [](int id) { return id == 1 ? 2 : id; });
      auto parsed = Package::parse(moved.serialize()); auto result = decodeResource(parsed, 1);
      const auto& v = result.vertMesh;
      check(parsed.exports[0].sourceOffset != original.exports[0].sourceOffset);
      check(v.vertices == m.vertices && v.transforms == m.transforms && v.packedVertices == m.packedVertices);
      check(v.normals == m.normals && v.animatedVertices == m.animatedVertices && v.spheres == m.spheres);
      check(v.boxes[0].valid == 1 && v.boxes[0].words[0] == 0x80000000);
      check(v.textures == std::vector<int>{2} && v.spriteMaterial == 2 && result.tags[0].references == std::vector<int>{2});
      check(v.sequences[0].object == 2 && v.sequences[0].events[0].object == 2);
      check(v.sequences[0].name == "Run" && v.sequences[0].events[0].text == sequence.events[0].text);
      auto corrupt = original; corrupt.exports[0].payload.push_back(0);
      rejects([&] { decodeResource(corrupt, 1); });
      corrupt = original; corrupt.exports[0].payload.pop_back();
      rejects([&] { decodeResource(corrupt, 1); });
      resource.vertMesh.version = 8;
      rejects([&] { encodeResource(resource, p, p.exports[0], [](int id) { return id; }); });
    }
    std::cout << "Skeletal and vertex mesh native and relocation checks passed\n";
  } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
