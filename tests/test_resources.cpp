#include <iostream>
#include <stdexcept>

#include "openucc/resources.hpp"
#include "openucc/target.hpp"

using namespace openucc;
void check(bool value, const char* message) {
  if (!value) throw std::runtime_error(message);
}
template <class F>
void rejects(F function) {
  bool failed = false;
  try {
    function();
  } catch (const std::exception&) {
    failed = true;
  }
  check(failed, "Malformed resource accepted");
}
int main() {
  try {
    Package p;
    p.version = 133;
    p.licensee = 40;
    p.header.resize(56);
    p.intern("None");
    TargetModel target;
    TargetImports imports(target, p);
    int textureClass = imports.nativeClass("Engine", "Texture");
    int paletteClass = imports.nativeClass("Engine", "Palette");
    int fontClass = imports.nativeClass("Engine", "Font");
    p.exports.resize(3);
    p.exports[0].type = textureClass;
    p.exports[0].name = p.intern("Image");
    p.exports[1].type = paletteClass;
    p.exports[1].name = p.intern("Colors");
    p.exports[2].type = fontClass;
    p.exports[2].name = p.intern("Typeface");
    ResourceBody texture;
    texture.kind = "Texture";
    ResourceTag palette;
    palette.name = "Palette";
    palette.kind = 5;
    palette.reference = 2;
    texture.tags.push_back(palette);
    texture.mips.push_back({{1, 2, 3, 4}, 2, 2, 1, 1});
    texture.mips.push_back({{2}, 1, 1, 0, 0});
    ResourceBody colors;
    colors.kind = "Palette";
    colors.colors.resize(256);
    colors.colors[1] = {12, 34, 56, 78};
    ResourceBody font;
    font.kind = "Font";
    font.pages = {1};
    font.glyphs.push_back({{0, 0, 2, 2}, 0});
    auto identity = [](int id) { return id; };
    for (int license : {33, 34, 35, 36}) {
      auto legacy = p; legacy.version = 123; legacy.licensee = license;
      auto body = texture; body.textureLayout = 123; body.legacyMaterialLicensee = license;
      body.obsoleteMaterialWord = 0x89abcdef;
      body.legacyMaterialFlag = license < 36 ? 91 : 0;
      for (std::size_t i = 0; i < body.legacyMaterialHeader.size(); ++i)
        body.legacyMaterialHeader[i] = std::uint8_t(i + 17);
      for (std::size_t i = 0; i < body.legacyMaterialStages.size(); ++i) {
        auto& stage = body.legacyMaterialStages[i];
        stage.first = std::uint8_t(i + 20);
        stage.second = license < 36 ? std::uint8_t(i + 40) : 0;
        for (std::size_t j = 0; j < stage.fields.size(); ++j)
          stage.fields[j] = std::uint8_t(i * 31 + j);
      }
      body.legacyMaterialStrings[7] = "Texture.Name";
      body.shader = "legacy shader text";
      body.materialWords = {0x80000000, 0x7fc12345, 0x3f800000};
      body.materialVersion = 0x12345678;
      legacy.exports[0].sourceOffset = 4096;
      encodeResource(body, legacy, legacy.exports[0], identity);
      auto decoded = decodeResource(legacy, 1);
      check(decoded.legacyMaterialStages[7].fields == body.legacyMaterialStages[7].fields &&
                decoded.legacyMaterialHeader == body.legacyMaterialHeader &&
                decoded.legacyMaterialStrings == body.legacyMaterialStrings &&
                decoded.materialWords == body.materialWords && decoded.shader == body.shader &&
                decoded.mips[0].pixels == body.mips[0].pixels, "Legacy material fields changed");
      auto wrong = legacy; wrong.licensee = license == 36 ? 35 : 36;
      rejects([&] { encodeResource(body, wrong, wrong.exports[0], identity); });
      auto broken = legacy; broken.exports[0].payload.pop_back();
      rejects([&] { decodeResource(broken, 1); });
      if (license == 36) {
        auto absent = body; absent.legacyMaterialStages[2].second = 1;
        rejects([&] { encodeResource(absent, legacy, legacy.exports[0], identity); });
      }
    }
    {
      // This codec starts at an already-established final boundary; it is not
      // a full 133/40 Model reader.
      for (int count : {0, 1, 18, 63, 64, 129}) {
        Model133Trailer trailer;
        for (int i = 0; i < count; ++i)
          trailer.pairs.push_back({std::uint32_t(0x12340001 + i),
                                   std::uint32_t(0xfedc0000 + i)});
        trailer.finalWord = 0x89abcdef;
        Bytes encoded;
        encodeModel133Trailer(trailer, p, encoded);
        Reader input{encoded};
        auto decoded = decodeModel133Trailer(input, p);
        check(decoded.pairs == trailer.pairs && decoded.finalWord == trailer.finalWord,
              "Model 133 trailer fields changed");
        Bytes reencoded;
        encodeModel133Trailer(decoded, p, reencoded);
        check(reencoded == encoded, "Model 133 trailer round-trip changed bytes");
        for (std::size_t length = 0; length < encoded.size(); ++length) {
          Bytes truncated(encoded.begin(), encoded.begin() + length);
          rejects([&] { Reader r{truncated}; decodeModel133Trailer(r, p); });
        }
        auto extra = encoded; extra.push_back(0);
        rejects([&] { Reader r{extra}; decodeModel133Trailer(r, p); });
      }
      for (Bytes bad : {Bytes{0x80, 0, 0, 0, 0},
                        Bytes{0x40, 0, 0, 0, 0, 0},
                        Bytes{0x81, 0, 0, 0, 0}}) {
        rejects([&] { Reader r{bad}; decodeModel133Trailer(r, p); });
      }
      Bytes oversized; index(oversized, 1000001); u32(oversized, 0);
      rejects([&] { Reader r{oversized}; decodeModel133Trailer(r, p); });
      auto other = p; other.licensee = 39;
      Bytes empty; index(empty, 0); u32(empty, 0);
      rejects([&] { Reader r{empty}; decodeModel133Trailer(r, other); });
      rejects([&] { Bytes out; encodeModel133Trailer({}, other, out); });
    }
    {
      auto legacy = p; legacy.version = 123; legacy.licensee = 37;
      ModelBody model; model.bounds.valid = 1;
      model.points.push_back({0x7fc01234, 0x80000000, 0x3f800000});
      model.vectors.push_back({0, 0, 0x3f800000});
      model.vertices.push_back({0, -1});
      BspSurface surface; surface.material = textureClass;
      surface.indices = {0, 0, 0, 0, -1}; model.surfaces.push_back(surface);
      BspNode node; node.indices = {0, 0, -1, -1, -1, 0, -1};
      node.vertexCount = 1; model.nodes.push_back(node);
      BspHull hull; hull.planes = {0x40000000}; model.hulls.push_back(hull);
      model.polys = 1;
      Bytes encoded;
      encodeModel(model, legacy, encoded, identity);
      Reader reader{encoded}; auto decoded = decodeModel(reader, legacy);
      check(decoded.points == model.points && decoded.hulls[0].planes == hull.planes,
            "Model packed/float bits lost");
      Bytes relocated;
      encodeModel(model, legacy, relocated, [](int id) { return id == 1 ? 2 : id; });
      Reader relocatedReader{relocated};
      check(decodeModel(relocatedReader, legacy).polys == 2, "Model reference not relocated");
      for (std::size_t length = 0; length < encoded.size(); ++length) {
        Bytes truncated(encoded.begin(), encoded.begin() + length);
        rejects([&] { Reader input{truncated}; decodeModel(input, legacy); });
      }
      auto extra = encoded; extra.push_back(0);
      rejects([&] { Reader input{extra}; decodeModel(input, legacy); });
      auto bad = model; bad.nodes[0].indices[5] = 1;
      rejects([&] { Bytes out; encodeModel(bad, legacy, out, identity); });
      bad = model; bad.vertices[0][0] = 1;
      rejects([&] { Bytes out; encodeModel(bad, legacy, out, identity); });
      bad = model; bad.hulls[0].planes[0] = 1;
      rejects([&] { Bytes out; encodeModel(bad, legacy, out, identity); });
      legacy.version = 133; legacy.licensee = 40;
      rejects([&] { Reader input{encoded}; decodeModel(input, legacy); });
    }
    for (int license : {12, 19, 23}) {
      auto early = p;
      early.version = 123;
      early.licensee = license;
      Bytes raw;
      index(raw, early.intern("None"));
      if (license >= 16) u32(raw, 0x89abcdef);
      index(raw, 1);
      u32(raw, 0);  // A zero lazy boundary is valid.
      index(raw, 4);
      raw.insert(raw.end(), {9, 8, 7, 6});
      u32(raw, 2); u32(raw, 2);
      raw.insert(raw.end(), {1, 1});
      early.exports[0].payload = raw;
      auto decoded = decodeResource(early, 1);
      check(decoded.mips.size() == 1 && decoded.mips[0].pixels == Bytes({9, 8, 7, 6}),
            "Early texture payload boundary");
      check(decoded.obsoleteMaterialWord == (license >= 16 ? 0x89abcdefu : 0),
            "Early reserved material scalar");
      encodeResource(decoded, early, early.exports[0], identity);
      auto reparsed = Package::parse(early.serialize());
      check(decodeResource(reparsed, 1).mips[0].pixels == decoded.mips[0].pixels,
            "Early texture relocated lazy boundary");
      early.exports[0].payload.push_back(0);
      rejects([&] { decodeResource(early, 1); });
      early.exports[0].payload = raw;
      early.licensee = 20;
      rejects([&] { decodeResource(early, 1); });
    }
    {
      auto grouped = p;
      TargetImports groupImports(target, grouped);
      Export group;
      group.type = groupImports.nativeClass("Core", "Package");
      group.name = grouped.intern("Icons");
      ResourceBody groupBody;
      groupBody.kind = "Package";
      encodeResource(groupBody, grouped, group, identity);
      grouped.exports.push_back(group);
      check(decodeResource(grouped, 4).kind == "Package",
            "Resource group not decoded");
      grouped.exports.back().payload.push_back(0);
      rejects([&] { decodeResource(grouped, 4); });
    }
    encodeResource(texture, p, p.exports[0], identity);
    encodeResource(colors, p, p.exports[1], identity);
    encodeResource(font, p, p.exports[2], identity);
    auto first = Package::parse(p.serialize());
    for (const std::string kind : {"Polys", "KMeshProps"}) {
      auto package = p; TargetImports geometryImports(target, package);
      package.exports[0].type = geometryImports.nativeClass("Engine", kind);
      ResourceBody geometry; geometry.kind = kind;
      geometry.karmaScalarBits[0] = 0x7fc01234;
      if (kind == "Polys") {
        geometry.polygonCapacity = 2; ResourcePolygon polygon;
        polygon.vertices = {{{0x80000000,0x7fc01234,0}}}; polygon.vectors[11] = 0xffffffff;
        polygon.actor = 2; polygon.material = 3; polygon.itemName = "PolygonItem";
        polygon.link = -1; polygon.brushPolygon = 12345; polygon.lightMapScale = 0x80000000;
        polygon.licenseWord = 0xffffffff; geometry.polygons.push_back(polygon);
      }
      encodeResource(geometry, package, package.exports[0], identity);
      auto parsed = Package::parse(package.serialize()); auto decoded = decodeResource(parsed, 1);
      Package moved = package;
      for (int i = 0; i < 100; ++i) moved.intern("GeometryShift" + std::to_string(i));
      encodeResource(decoded, moved, moved.exports[0], [](int id) { return id == 2 ? 3 : id == 3 ? 2 : id; });
      auto result = decodeResource(Package::parse(moved.serialize()), 1);
      if (kind == "Polys") {
        check(result.polygonCapacity == 2 && result.polygons[0].vertices == geometry.polygons[0].vertices &&
              result.polygons[0].itemName == "PolygonItem" && result.polygons[0].actor == 3 &&
              result.polygons[0].material == 2 && result.polygons[0].brushPolygon == 12345,
              "Polygon bits or relocation lost");
        auto bad = geometry; bad.polygonCapacity = 0;
        rejects([&] { encodeResource(bad, moved, moved.exports[0], identity); });
        parsed.exports[0].payload[9] = 17;
      } else {
        check(result.karmaScalarBits == geometry.karmaScalarBits, "Karma scalar bits lost");
        parsed.exports[0].payload.back() = 1;
      }
      rejects([&] { decodeResource(parsed, 1); });
    }
    {
      auto package = p; TargetImports maskImports(target, package);
      package.exports[0].type = maskImports.nativeClass("Engine", "MaskTexture");
      ResourceBody mask; mask.kind = "MaskTexture";
      ResourceTag rle; rle.name = "RLEData"; rle.kind = 9; rle.words = {0,8192,0xffffffff};
      mask.tags = {palette,rle};
      encodeResource(mask, package, package.exports[0], identity);
      auto decoded = decodeResource(Package::parse(package.serialize()), 1);
      check(decoded.tags[1].words == rle.words && decoded.mips.empty(), "Mask RLE words lost");
      encodeResource(decoded, package, package.exports[0], [](int id) { return id == 2 ? 3 : id; });
      check(decodeResource(Package::parse(package.serialize()), 1).tags[0].reference == 3,
            "Mask reference relocation lost");
    }
    {
      auto package = p; package.version = 127; package.licensee = 39;
      TargetImports wetImports(target, package); package.exports[0].type = wetImports.nativeClass("Fire", "WetTexture");
      ResourceBody wet; wet.kind = "WetTexture"; wet.textureLayout = 127;
      ResourceTag flag; flag.name = "bParametric"; flag.kind = 3; flag.boolean = true;
      ResourceTag drop; drop.name = "Drops"; drop.kind = 10; drop.structure = "ADrop"; drop.subscript = 3;
      drop.byteFields = {{"Type",19},{"depth",255},{"X",17},{"ByteD",128}};
      wet.tags = {flag,drop}; wet.mips = {{{},128,128,7,7}};
      encodeResource(wet, package, package.exports[0], identity);
      auto decoded = decodeResource(Package::parse(package.serialize()), 1);
      check(decoded.tags[1].byteFields == drop.byteFields && decoded.tags[1].subscript == 3 &&
            decoded.mips[0].pixels.empty(), "WetTexture fields lost");
      wet.tags[0].boolean = false;
      rejects([&] { encodeResource(wet, package, package.exports[0], identity); });
      wet.tags[0].boolean = true; wet.tags[1].byteFields.push_back({"Type",0});
      rejects([&] { encodeResource(wet, package, package.exports[0], identity); });
    }
    for (auto profile : {std::pair<int,int>{123,38}, {125,38}, {125,39}, {126,39},
                         {127,39}, {128,39}, {129,39}, {130,40}, {131,40}, {132,40}}) {
      auto package = p; package.version = profile.first; package.licensee = profile.second;
      package.exports.resize(1);
      auto sample = texture; sample.tags.clear(); sample.textureLayout = profile.first;
      sample.singleStageBytes = {0x80, 0xff};
      sample.singleStageWords = {0x80000000, 0x7fc01234, 0xffffffff};
      sample.stageBytes[4][5] = 0x81; sample.stageWords[4][1] = 0x7fc01234;
      sample.transitionalStageBytes[4] = {0xff,0x80};
      sample.transitionalStageWords[4] = {0x80000000,0x7fc01234,0xffffffff};
      sample.stageStrings = {{"Stage", {"Value", ""}}};
      encodeResource(sample, package, package.exports[0], identity);
      auto result = decodeResource(Package::parse(package.serialize()), 1);
      check(result.stageStrings == sample.stageStrings && result.mips[0].pixels == sample.mips[0].pixels,
            "Expanded material layout lost strings or mips");
      check(profile.first < 129 ? result.singleStageWords == sample.singleStageWords :
            profile.first == 129 ? result.transitionalStageWords == sample.transitionalStageWords :
                                 result.stageWords == sample.stageWords,
            "Expanded material layout lost stage words");
    }
    for (auto profile : {std::pair<int,int>{133,5}, {133,7}, {132,5}}) {
      auto storage = profile.second;
      auto package = p; package.version = profile.first;
      ResourceBody sparse; sparse.kind = "Texture"; sparse.textureLayout = profile.first;
      ResourceTag format; format.name = "Format"; format.kind = 1; format.value = {std::uint8_t(storage)};
      sparse.tags = {format}; sparse.mips = {{Bytes(storage == 5 ? 8 : 16, 0x5a), 2, 1, 1, 0}, {{}, 1, 1, 0, 0}};
      encodeResource(sparse, package, package.exports[0], identity);
      auto parsed = Package::parse(package.serialize()); auto decoded = decodeResource(parsed, 1);
      check(decoded.mips.size() == 2 && decoded.mips.back().pixels.empty(), "Empty terminal mip lost");
      sparse.mips.front().pixels.clear();
      rejects([&] { encodeResource(sparse, package, package.exports[0], identity); });
      sparse.mips = {{{}, 1, 1, 0, 0}};
      rejects([&] { encodeResource(sparse, package, package.exports[0], identity); });
    }
    {
      auto cubePackage = p;
      TargetImports cubeImports(target, cubePackage);
      cubePackage.exports[0].type = cubeImports.nativeClass("Engine", "Cubemap");
      auto cube = texture; cube.kind = "Cubemap";
      for (int i = 0; i < 6; ++i) {
        ResourceTag face; face.kind = 5; face.name = "Faces"; face.subscript = i; face.reference = 2;
        cube.tags.push_back(face);
      }
      encodeResource(cube, cubePackage, cubePackage.exports[0], identity);
      auto parsed = Package::parse(cubePackage.serialize()); auto body = decodeResource(parsed, 1);
      check(body.mips[0].pixels == cube.mips[0].pixels && body.tags[6].subscript == 5,
            "Cubemap face slots or inherited mips lost");
      encodeResource(body, cubePackage, cubePackage.exports[0], [](int id) { return id == 2 ? 3 : id; });
      auto movedCube = decodeResource(Package::parse(cubePackage.serialize()), 1);
      check(movedCube.tags[6].reference == 3, "Cubemap face reference not relocated");
    }
    for (const std::string kind : {"Shader", "FinalBlend", "Combiner", "TexCoordSource",
         "TexPanner", "FadeColor", "TexOscillator", "TexEnvMap", "VertexColor",
         "TexRotator", "ColorModifier", "TexPannerTriggered", "TexScaler", "ConstantColor",
         "TexOscillatorTriggered", "TexPlanningPanner", "FresnelMaterial", "MaterialSequence", "L2ColorModifier"}) {
      auto original = p;
      TargetImports materialImports(target, original);
      original.exports[0].type = materialImports.nativeClass("Engine", kind);
      ResourceBody material; material.kind = kind;
      ResourceTag source; source.name = "Material"; source.kind = 5; source.reference = 2;
      ResourceTag matrix; matrix.name = "M"; matrix.kind = 10; matrix.structure = "Matrix";
      matrix.matrix = {{"XPlane", {{"W", 0x80000000}, {"X", 0x7fc01234}}},
                       {"WPlane", {{"Z", 0x3f800000}}}};
      material.tags = {source, matrix}; material.stageStrings = {{"stage", {"texture"}}};
      if (kind == "MaterialSequence") {
        ResourceTag items; items.name = "SequenceItems"; items.kind = 9;
        ResourceTag time; time.name = "Time"; time.kind = 4; time.value = {0x34,0x12,0xc0,0x7f};
        ResourceTag action; action.name = "Action"; action.kind = 1; action.value = {0xff};
        items.elements = {{source,time,action},{}}; material.tags.push_back(items);
      }
      material.materialWords[0] = 0x80000000;
      encodeResource(material, original, original.exports[0], identity);
      auto parsed = Package::parse(original.serialize()); auto body = decodeResource(parsed, 1);
      Package shifted; shifted.version = 133; shifted.licensee = 40; shifted.header.resize(56);
      for (int i = 0; i < 130; ++i) shifted.intern("MaterialNamePadding" + std::to_string(i));
      TargetImports shiftedImports(target, shifted);
      Export object; object.type = shiftedImports.nativeClass("Engine", kind); object.name = shifted.intern("MaterialObject");
      shifted.exports.push_back(object);
      encodeResource(body, shifted, shifted.exports[0], [](int id) { return id == 2 ? 1 : id; });
      auto relocated = Package::parse(shifted.serialize()); auto result = decodeResource(relocated, 1);
      check(result.tags[0].reference == 1 && result.tags[1].matrix[0].components == matrix.matrix[0].components &&
            result.tags[1].matrix[1].name == "WPlane" && result.materialWords == material.materialWords,
             "Material reference, matrix names or float bits lost");
      if (kind == "MaterialSequence") {
        const auto& items = result.tags[2].elements;
        check(items.size() == 2 && items[1].empty() && items[0][0].reference == 1 &&
              items[0][1].value == material.tags[2].elements[0][1].value && items[0][2].value == Bytes{0xff},
              "Material sequence relocation or scalar bits lost");
        auto bad = body; bad.tags[2].elements[0][1].name = "UnknownTime";
        rejects([&] { encodeResource(bad, shifted, shifted.exports[0], identity); });
      }
      relocated.exports[0].payload.push_back(0);
      rejects([&] { decodeResource(relocated, 1); });
      body.tags[1].matrix[0].components.push_back({"X", 0});
      rejects([&] { encodeResource(body, shifted, shifted.exports[0], identity); });
    }
    {
      auto terrain = texture;
      ResourceTag format;
      format.name = "Format"; format.kind = 1; format.value = {10};
      terrain.tags = {format};
      terrain.mips = {{Bytes{0, 0, 255, 255, 0, 128, 52, 18}, 2, 2, 1, 1}};
      auto package = p;
      encodeResource(terrain, package, package.exports[0], identity);
      auto parsed = Package::parse(package.serialize());
      check(decodeResource(parsed, 1).mips[0].pixels == terrain.mips[0].pixels,
            "G16 height sample bits changed");
      terrain.mips[0].pixels.pop_back();
      rejects([&] { encodeResource(terrain, package, package.exports[0], identity); });
    }
    {
      auto meshes = p;
      TargetImports meshImports(target, meshes);
      Export object;
      object.type = meshImports.nativeClass("Engine", "StaticMesh");
      object.name = meshes.intern("Mesh");
      meshes.exports.push_back(object);
      ResourceBody body; body.kind = "StaticMesh";
      ResourceTag materials; materials.name = "Materials"; materials.kind = 9;
      ResourceTag material; material.name = "Material"; material.kind = 5; material.reference = 1;
      ResourceTag collision; collision.name = "EnableCollision"; collision.kind = 3; collision.boolean = true;
      materials.elements = {{material, collision}}; body.tags = {materials};
      auto& m = body.mesh;
      m.vertices.resize(3); m.vertices[0][0] = 0x80000000; m.vertices[1][2] = 0x7fc01234;
      m.bounds.valid = m.meshBounds.valid = 1;
      m.sections.push_back({0, {0, 0, 2, 1, 1}}); m.indices[0] = {0, 1, 2};
      m.uv.push_back({{{0x80000000, 0x3f800000}}, 0, 7});
      MeshCollisionFace face; face.indices = {0, 1, 2, 0}; m.collisionFaces.push_back(face);
      MeshCollisionNode node; node.indices = {0, -1, -1, -1}; node.bounds.valid = 1; m.collisionNodes.push_back(node);
      m.licenseReferences = {1, 0}; m.trailingReference = 1; m.licenseWords[4] = 0x80000000;
      MeshTriangle triangle; triangle.uv.resize(8); triangle.uv[7][5] = 0x3f800000;
      triangle.smoothing = 0xffffffff; m.triangles.push_back(triangle); m.internalVersion = 8;
      m.auxiliaryUv = {{{0x3f000000, 0x80000000}}}; m.auxiliaryWords = {2, 6, 512, 0};
      encodeResource(body, meshes, meshes.exports[3], identity);
      check(meshes.exports[3].lazyOffsets.size() == 3, "Missing mesh lazy fixups");
      auto original = Package::parse(meshes.serialize());
      auto decoded = decodeResource(original, 4);
      check(decoded.mesh.vertices == m.vertices && decoded.mesh.auxiliaryWords == m.auxiliaryWords &&
                decoded.mesh.triangles[0].uv == triangle.uv, "Mesh bits or channels changed");
      for (auto fixup : meshes.exports[3].lazyOffsets) {
        auto corrupt = original; corrupt.exports[3].payload.at(fixup.first) ^= 1;
        rejects([&] { decodeResource(corrupt, 4); });
      }
      auto corrupt = original; corrupt.exports[3].payload.push_back(0);
      rejects([&] { decodeResource(corrupt, 4); });
      std::swap(meshes.exports[0], meshes.exports[1]);
      for (int i = 0; i < 100; ++i) meshes.intern("RelocatedMeshName" + std::to_string(i));
      encodeResource(decoded, meshes, meshes.exports[3], [](int id) { return id == 1 ? 2 : id; });
      auto relocated = Package::parse(meshes.serialize());
      auto rebuilt = decodeResource(relocated, 4);
      check(relocated.exports[3].sourceOffset != original.exports[3].sourceOffset &&
                rebuilt.mesh.licenseReferences[0] == 2 && rebuilt.mesh.trailingReference == 2 &&
                rebuilt.tags[0].elements[0][0].reference == 2 && rebuilt.tags[0].elements[0][1].boolean &&
                rebuilt.mesh.vertices == m.vertices, "Mesh relocation or nested reference failed");
      body.mesh.indices[0][1] = 3;
      rejects([&] { encodeResource(body, meshes, meshes.exports[3], identity); });
      meshes.version = 123; meshes.licensee = 37;
      rejects([&] { decodeResource(meshes, 4); });
      auto legacyMesh = decoded; legacyMesh.mesh.layout = 123; legacyMesh.mesh.licensee = 37;
      rejects([&] { encodeResource(legacyMesh, meshes, meshes.exports[3], identity); });
      legacyMesh.mesh.auxiliaryUv.clear(); legacyMesh.mesh.auxiliaryWords = {};
      encodeResource(legacyMesh, meshes, meshes.exports[3], [](int id) { return id == 1 ? 2 : id; });
      auto legacyPackage = Package::parse(meshes.serialize());
      auto legacyResult = decodeResource(legacyPackage, 4);
      check(legacyResult.mesh.layout == 123 && legacyResult.mesh.auxiliaryUv.empty() &&
                legacyResult.mesh.vertices == decoded.mesh.vertices &&
                legacyResult.mesh.trailingReference == 2, "Legacy StaticMesh fields or references lost");
      legacyPackage.exports[3].payload.push_back(0);
      rejects([&] { decodeResource(legacyPackage, 4); });
      for (auto profile : {std::pair<int,int>{118,0}, {123,12}, {123,14}, {123,15},
                           {123,16}, {123,18}, {123,36}, {123,37}, {125,38},
                           {126,39}, {128,39}, {130,40}, {132,40}}) {
        auto package = meshes; package.version = profile.first; package.licensee = profile.second;
        auto sample = decoded; auto& mesh = sample.mesh;
        mesh.layout = profile.first; mesh.licensee = profile.second;
        mesh.licenseWords = {};
        if (mesh.layout < 125) { mesh.auxiliaryUv.clear(); mesh.auxiliaryWords = {}; }
        if (!mesh.licensee) { mesh.licenseWord = 0; mesh.licenseReferences = {}; }
        else mesh.licenseWords[mesh.licensee < 13 ? 4 : mesh.licensee < 15 ? 7 : mesh.licensee < 37 ? 8 : 9] = 0x7fc01234;
        encodeResource(sample, package, package.exports[3], identity);
        check(package.exports[3].lazyOffsets.size() == (mesh.licensee < 17 ? 1u : 3u),
              "Wrong legacy collision fixup count");
        auto firstMesh = Package::parse(package.serialize());
        for (int i = 0; i < 80; ++i) package.intern("ProfileRelocation" + std::to_string(i));
        encodeResource(sample, package, package.exports[3], [](int id) { return id == 1 ? 2 : id; });
        auto moved = Package::parse(package.serialize()); auto result = decodeResource(moved, 4);
        check(moved.exports[3].sourceOffset != firstMesh.exports[3].sourceOffset &&
              result.mesh.collisionFaces[0].indices == mesh.collisionFaces[0].indices &&
              result.mesh.collisionNodes[0].indices == mesh.collisionNodes[0].indices &&
              result.mesh.licenseWords == mesh.licenseWords && result.mesh.trailingReference == 2 &&
              result.mesh.auxiliaryWords == mesh.auxiliaryWords, "Expanded mesh layout or relocation lost");
        auto bad = sample; bad.mesh.licensee = 40;
        if (mesh.licensee != 40) rejects([&] { encodeResource(bad, package, package.exports[3], identity); });
        if (mesh.licensee < 37) {
          bad = sample; bad.mesh.licenseWords[9] = 1;
          rejects([&] { encodeResource(bad, package, package.exports[3], identity); });
        }
        if (mesh.layout == 118) {
          bad = sample; bad.mesh.trailingWord = 1;
          rejects([&] { encodeResource(bad, package, package.exports[3], identity); });
          bad = sample; bad.mesh.licenseReferences[0] = 1;
          rejects([&] { encodeResource(bad, package, package.exports[3], identity); });
        }
      }
    }
    {
      auto legacy = p;
      legacy.version = 123;
      legacy.licensee = 37;
      auto legacyTexture = texture;
      legacyTexture.textureLayout = 123;
      legacyTexture.singleStageBytes = {7, 19};
      legacyTexture.singleStageWords = {0x12345678, 0x87654321, 0xffffffff};
      legacyTexture.stageStrings = {{"Stage", {"Value"}}};
      legacyTexture.shader = "Shader";
      encodeResource(legacyTexture, legacy, legacy.exports[0], identity);
      auto decoded = Package::parse(legacy.serialize());
      auto body = decodeResource(decoded, 1);
      check(body.textureLayout == 123 &&
                body.singleStageBytes == legacyTexture.singleStageBytes &&
                body.singleStageWords == legacyTexture.singleStageWords &&
                body.stageStrings == legacyTexture.stageStrings &&
                body.shader == legacyTexture.shader,
            "Single-stage material fields lost");
      legacy.intern("ForceLegacyMipRelocation");
      auto relocated = Package::parse(legacy.serialize());
      check(relocated.exports[0].sourceOffset !=
                    decoded.exports[0].sourceOffset &&
                decodeResource(relocated, 1).mips[1].pixels ==
                    texture.mips[1].pixels,
            "Legacy mip relocation lost data");
      check(decodeResource(relocated, 2).colors[1] == colors.colors[1] &&
                decodeResource(relocated, 3).pages == font.pages,
            "Legacy palette/font data lost");
      auto incompatible = p;
      rejects([&] {
        encodeResource(legacyTexture, incompatible, incompatible.exports[0],
                       identity);
      });
      legacy.licensee = 36;
      rejects([&] { decodeResource(legacy, 1); });
      rejects([&] {
        encodeResource(legacyTexture, legacy, legacy.exports[0], identity);
      });
    }
    {
      auto plain = p;
      plain.version = 126;
      plain.licensee = 0;
      plain.exports.resize(1);
      auto plainTexture = texture;
      plainTexture.textureLayout = 126;
      plainTexture.tags.clear();
      encodeResource(plainTexture, plain, plain.exports[0], identity);
      auto decoded = Package::parse(plain.serialize());
      auto body = decodeResource(decoded, 1);
      check(body.textureLayout == 126 && body.mips.size() == 2 &&
                body.mips[0].pixels == texture.mips[0].pixels,
            "Plain texture stream acquired a Lineage material prefix");
      plain.intern("RelocatePlainTexture");
      auto movedPlain = Package::parse(plain.serialize());
      check(movedPlain.exports[0].sourceOffset !=
                    decoded.exports[0].sourceOffset &&
                decodeResource(movedPlain, 1).mips[1].pixels ==
                    texture.mips[1].pixels,
            "Plain texture lazy offsets were not relocated");
      rejects(
          [&] { encodeResource(colors, plain, plain.exports[0], identity); });
    }
    auto image = decodeResource(first, 1);
    check(image.mips.size() == 2 &&
              image.mips[0].pixels == texture.mips[0].pixels,
          "Mip data lost");
    check(decodeResource(first, 2).colors[1] == colors.colors[1],
          "Palette alpha lost");
    check(decodeResource(first, 3).pages == font.pages,
          "Font page reference lost");
    // Force a new name-table size. Relocations must follow the new payload
    // address, including the second lazy array within the same export.
    p.intern("AdditionalNameChangingAllSerializedOffsets");
    auto moved = Package::parse(p.serialize());
    check(moved.exports[0].sourceOffset != first.exports[0].sourceOffset,
          "Relocation fixture did not move");
    check(decodeResource(moved, 1).mips[1].pixels == texture.mips[1].pixels,
          "Mip relocation lost data");
    auto broken = moved;
    auto word = p.exports[0].lazyOffsets[0].first;
    broken.exports[0].payload[word] ^= 1;
    rejects([&] { decodeResource(broken, 1); });
    broken = moved;
    broken.exports[2].payload.back() = 1;
    rejects([&] { decodeResource(broken, 3); });
    font.glyphs[0].page = 1;
    encodeResource(font, p, p.exports[2], identity);
    rejects([&] { decodeResource(Package::parse(p.serialize()), 3); });
    std::cout << "Resource codecs and relocations passed\n";
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
