#include "openucc/resources.hpp"
#include "openucc/target.hpp"
#include <iostream>
#include <stdexcept>

using namespace openucc;

void check(bool value, const char* message) {
  if (!value) throw std::runtime_error(message);
}
template<class F> void rejects(F function) {
  bool failed = false;
  try { function(); } catch (const std::exception&) { failed = true; }
  check(failed, "Malformed Model 133 was accepted");
}
void relocate(Bytes& bytes, const Export& e) {
  for (const auto& entry : e.lazyOffsets) {
    check(entry.first + 4 <= bytes.size() && entry.second <= bytes.size(), "Invalid test fixup");
    auto end = e.sourceOffset + entry.second;
    for (int i = 0; i < 4; ++i) bytes[entry.first + i] = std::uint8_t(end >> (8 * i));
  }
}

int main(int argc, char** argv) {
  try {
    Package p; p.version = 133; p.licensee = 40; p.header.resize(56);
    p.intern("None");
    p.exports.resize(3);
    for (std::size_t i = 0; i < p.exports.size(); ++i)
      p.exports[i].name = p.intern("Object" + std::to_string(i));
    Model133Body model;
    auto& geometry = model.geometry;
    geometry.bounds.valid = 1;
    geometry.points.push_back({0x7fc01234, 0x80000000, 0x3f800000});
    geometry.vectors.push_back({0, 0, 0x3f800000});
    geometry.vertices.push_back({0, -1});
    geometry.vertices.push_back({999, -1}); // Outside every node-used range.
    BspSurface surface; surface.material = 1; surface.actor = 2;
    surface.indices = {0, 0, 0, 0, -1}; geometry.surfaces.push_back(surface);
    BspNode node; node.indices = {0, 0, -1, -1, -1, 0, -1};
    node.vertexCount = 1; geometry.nodes.push_back(node);
    BspHull hull; hull.planes = {0x40000000}; geometry.hulls.push_back(hull);
    geometry.polys = 1;
    Model133Zone zone; zone.actor = 1; zone.words = {1, 2, 3, 4, 5};
    model.zones.push_back(zone);
    Model133Leaf leaf; leaf.indices = {0, -1, 2}; leaf.visibleZones = {3, 4};
    model.leaves.push_back(leaf);
    model.lights = {1, 2};
    Model133Section section; section.material = 1; section.revision = 9;
    section.vertices.push_back({0, 1, 2, 3, 4, 5, 6, 7, 8, 0x80000000});
    section.words = {11, 12, 13}; model.sections.push_back(section);
    Model133LightMask mask; mask.light = 1; mask.bitmap = {0, 1, 2, 255};
    mask.words = {3, 4, 5, 6, 7, 8, 9};
    Model133Lightmap lightmap; lightmap.indices = {0, 0, -1, 2, 3, 4, 5};
    lightmap.matrixAndVectors[10] = 0x7fc12345;
    lightmap.level = 1; lightmap.revision = 0x89abcdef;
    lightmap.masks.push_back(mask); model.lightmaps.push_back(lightmap);
    Model133LightmapPage page; page.level = 1; page.indices = {0, 0xffffffff};
    page.words = {7, 8, 9}; page.data[0] = {10, 20, 30};
    page.data[1] = {40, 50}; page.hasLazyOffset[1] = false;
    page.format = 3; page.dimensionsAndRevision = {256, 128, 2};
    Model133LightmapGroup group; group.level = 1; group.indices = {0};
    group.pages.push_back(page);
    auto secondPage = page; secondPage.hasLazyOffset = {{true, true}};
    secondPage.data[0].resize(64, 0xa5); secondPage.data[1] = {0xfe, 0xed};
    group.pages.push_back(secondPage); model.lightmapGroups.push_back(group);
    model.trailer.pairs = {{512, 512}}; model.trailer.finalWord = 1;

    auto identity = [](int id) { return id; };
    Export first; first.sourceOffset = 4096;
    Bytes encoded{0};
    encodeModel133(model, p, encoded, first, identity);
    check(first.lazyOffsets.size() == 3, "Wrong lazy fixup count");
    relocate(encoded, first);
    Reader reader{encoded, 1};
    auto decoded = decodeModel133(reader, p, first);
    check(decoded.geometry.vertices == geometry.vertices &&
          decoded.geometry.points == geometry.points, "Model geometry changed");
    check(decoded.zones[0].words == zone.words && decoded.leaves[0].indices == leaf.indices &&
          decoded.sections[0].vertices == section.vertices, "Model structural fields changed");
    check(decoded.lightmaps[0].masks[0].bitmap == mask.bitmap &&
          decoded.lightmaps[0].matrixAndVectors == lightmap.matrixAndVectors,
          "Model lightmap changed");
    check(decoded.lightmapGroups[0].pages[0].data == page.data &&
          decoded.lightmapGroups[0].pages[0].hasLazyOffset == page.hasLazyOffset &&
          decoded.lightmapGroups[0].pages[1].data == secondPage.data &&
          decoded.lightmapGroups[0].pages[1].hasLazyOffset == secondPage.hasLazyOffset &&
          decoded.trailer.finalWord == 1, "Model lazy data or trailer changed");

    Export moved; moved.sourceOffset = 8192;
    Bytes rewritten{0};
    encodeModel133(decoded, p, rewritten, moved, identity);
    relocate(rewritten, moved);
    Reader movedReader{rewritten, 1};
    auto movedModel = decodeModel133(movedReader, p, moved);
    check(movedModel.lightmapGroups[0].pages[0].data == page.data &&
          rewritten != encoded, "Model lazy relocation was not applied");
    Bytes mappedBytes{0}; Export mapped; mapped.sourceOffset = 8192;
    encodeModel133(decoded, p, mappedBytes, mapped,
                   [](int id) { return id == 1 ? 3 : id == 2 ? 1 : id; });
    relocate(mappedBytes, mapped);
    Reader mappedReader{mappedBytes, 1};
    auto mappedModel = decodeModel133(mappedReader, p, mapped);
    check(mappedModel.geometry.polys == 3 && mappedModel.zones[0].actor == 3 &&
          mappedModel.geometry.surfaces[0].material == 3 &&
          mappedModel.geometry.surfaces[0].actor == 1 &&
          mappedModel.sections[0].material == 3 && mappedModel.lightmaps[0].masks[0].light == 3 &&
          mappedModel.lightmaps[0].level == 3 &&
          mappedModel.lightmapGroups[0].pages[0].level == 3 &&
          mappedModel.lightmapGroups[0].pages[1].level == 3 &&
          mappedModel.lightmapGroups[0].level == 3 && mappedModel.lights[0] == 3 &&
          mappedModel.lights[1] == 1,
          "Model references were not relocated");
    for (std::size_t n = 1; n < encoded.size(); ++n) {
      Bytes truncated(encoded.begin(), encoded.begin() + n);
      rejects([&] { Reader r{truncated, 1}; decodeModel133(r, p, first); });
    }
    auto extra = encoded; extra.push_back(0);
    rejects([&] { Reader r{extra, 1}; decodeModel133(r, p, first); });
    auto wrongOffset = encoded; ++wrongOffset[first.lazyOffsets[0].first];
    rejects([&] { Reader r{wrongOffset, 1}; decodeModel133(r, p, first); });
    auto noncanonical = encoded;
    const std::size_t vectorCount = 1 + 25 + 16;
    check(noncanonical[vectorCount] == 1, "Invalid compact-index fixture");
    noncanonical[vectorCount] = 0x41;
    noncanonical.insert(noncanonical.begin() + vectorCount + 1, 0);
    rejects([&] { Reader r{noncanonical, 1}; decodeModel133(r, p, first); });
    auto bad = model; bad.geometry.vertices[0][0] = 999;
    rejects([&] { Bytes output; Export e; encodeModel133(bad, p, output, e, identity); });
    bad = model; bad.geometry.hulls[0].planes[0] = 99;
    rejects([&] { Bytes output; Export e; encodeModel133(bad, p, output, e, identity); });
    bad.geometry.nodes[0].indices[5] = -1;
    Bytes unlinkedHull; Export unlinkedExport;
    encodeModel133(bad, p, unlinkedHull, unlinkedExport, identity);
    relocate(unlinkedHull, unlinkedExport);
    Reader unlinkedReader{unlinkedHull};
    check(decodeModel133(unlinkedReader, p, unlinkedExport).geometry.hulls[0].planes[0] == 99,
          "Unlinked hull word was not preserved");
    bad.geometry.hulls[0].planes[0] = 0xffffffffU;
    rejects([&] { Bytes output; Export e; encodeModel133(bad, p, output, e, identity); });
    auto wrongProfile = p; wrongProfile.version = 131;
    rejects([&] { Reader r{encoded, 1}; decodeModel133(r, wrongProfile, first); });
    wrongProfile = p; wrongProfile.licensee = 39;
    rejects([&] { Reader r{encoded, 1}; decodeModel133(r, wrongProfile, first); });

    auto adjacent = model;
    adjacent.zones.clear(); adjacent.leaves.clear(); adjacent.lights.clear();
    adjacent.sections.clear(); adjacent.lightmaps.clear(); adjacent.lightmapGroups.clear();
    adjacent.trailer = {};
    const std::vector<std::pair<int, int>> profiles = {
        {127, 39}, {128, 39}, {130, 40}, {132, 40}};
    const std::vector<std::function<void(Model133Body&)>> unsupported = {
        [&](auto& value) { value.zones.push_back(zone); },
        [&](auto& value) { value.leaves.push_back(leaf); },
        [&](auto& value) { value.lights.push_back(1); },
        [&](auto& value) { value.sections.push_back(section); },
        [&](auto& value) { value.lightmaps.push_back(lightmap); },
        [&](auto& value) { value.lightmapGroups.push_back(group); },
        [&](auto& value) { value.trailer.pairs.push_back({1, 2}); },
        [&](auto& value) { value.trailer.finalWord = 1; },
    };
    for (auto profile : profiles) {
      auto older = p; older.version = profile.first; older.licensee = profile.second;
      Bytes raw; Export entry; entry.sourceOffset = 4096;
      encodeModel133(adjacent, older, raw, entry, identity);
      Reader r{raw};
      auto parsed = decodeModel133(r, older, entry);
      check(r.pos == raw.size() && parsed.geometry.points == adjacent.geometry.points &&
            parsed.geometry.vertices == adjacent.geometry.vertices &&
            parsed.zones.empty() && parsed.trailer.pairs.empty() && !parsed.trailer.finalWord,
            "Adjacent-profile Model roundtrip failed");
      for (const auto& populate : unsupported) {
        auto badAdjacent = adjacent; populate(badAdjacent);
        rejects([&] { Bytes bytes; Export e; encodeModel133(badAdjacent, older, bytes, e, identity); });
        Bytes broad; Export broadEntry; broadEntry.sourceOffset = 4096;
        encodeModel133(badAdjacent, p, broad, broadEntry, identity); relocate(broad, broadEntry);
        rejects([&] { Reader input{broad}; decodeModel133(input, older, broadEntry); });
      }
    }
    TargetModel target; TargetImports imports(target, p);
    Export object; object.type = imports.nativeClass("Engine", "Model");
    object.name = p.intern("ModelObject"); p.exports.push_back(object);
    ResourceBody resource; resource.kind = "Model"; resource.modelLayout = 133;
    resource.model133 = model;
    encodeResource(resource, p, p.exports.back(), identity);
    auto firstPackage = Package::parse(p.serialize());
    auto resourceRead = decodeResource(firstPackage, 4);
    check(resourceRead.modelLayout == 133 &&
          resourceRead.model133.lightmapGroups[0].pages[0].data == page.data,
          "Resource Model 133 decode failed");
    auto originalOffset = firstPackage.exports[3].sourceOffset;
    firstPackage.intern("Padding forces a new payload offset");
    encodeResource(resourceRead, firstPackage, firstPackage.exports[3], identity);
    auto movedPackage = Package::parse(firstPackage.serialize());
    check(movedPackage.exports[3].sourceOffset != originalOffset &&
          decodeResource(movedPackage, 4).model133.trailer.finalWord == 1,
          "Resource Model 133 package relocation failed");
    for (auto profile : profiles) {
      auto older = p; older.version = profile.first; older.licensee = profile.second;
      auto olderResource = resource;
      olderResource.modelLayout = older.version; olderResource.model133 = adjacent;
      encodeResource(olderResource, older, older.exports[3], identity);
      auto parsedPackage = Package::parse(older.serialize());
      auto parsedResource = decodeResource(parsedPackage, 4);
      check(parsedResource.modelLayout == older.version &&
            parsedResource.model133.geometry.vertices == adjacent.geometry.vertices,
             "Adjacent-profile Model resource roundtrip failed");
    }
    auto unverified = p; unverified.version = 129; unverified.licensee = 39;
    auto opaque = decodeResource(unverified, 4);
    check(opaque.modelOpaque && !opaque.modelWarning.empty() &&
          opaque.modelOpaquePayload == unverified.exports[3].payload,
          "Unverified Model was not retained as opaque bytes");
    auto unchanged = unverified.exports[3];
    rejects([&] { encodeResource(opaque, unverified, unverified.exports[3], identity); });
    check(unverified.exports[3].payload == unchanged.payload &&
          unverified.exports[3].lazyOffsets == unchanged.lazyOffsets,
          "Unsafe opaque relocation changed output");
    auto stacked = p; stacked.exports[3].flags |= 0x02000000;
    check(decodeResource(stacked, 4).modelOpaque, "Stacked Model did not produce an opaque warning");
    auto tagged = p;
    tagged.exports[3].payload.clear();
    index(tagged.exports[3].payload, tagged.intern("UnknownTag"));
    auto taggedOpaque = decodeResource(tagged, 4);
    check(taggedOpaque.modelOpaque && taggedOpaque.modelWarning == "Unsupported Model properties" &&
          taggedOpaque.modelOpaquePayload == tagged.exports[3].payload,
          "Unverified Model property frame was interpreted or rejected");
    auto legacyProfile = p; legacyProfile.version = 126; legacyProfile.licensee = 0;
    check(decodeResource(legacyProfile, 4).modelOpaque,
          "Unverified Model 126/0 did not produce an opaque warning");
    auto malformed = p; malformed.exports[3].payload = noncanonical;
    malformed.exports[3].sourceOffset = first.sourceOffset;
    rejects([&] { decodeResource(malformed, 4); });
    resource.modelLayout = 123;
    rejects([&] { encodeResource(resource, p, p.exports[3], identity); });
    if (argc == 2) {
      Package fixture; fixture.version = 133; fixture.licensee = 40; fixture.header.resize(56);
      fixture.intern("None");
      TargetModel fixtureTarget; TargetImports fixtureImports(fixtureTarget, fixture);
      Export modelExport; modelExport.type = fixtureImports.nativeClass("Engine", "Model");
      modelExport.name = fixture.intern("ModelFixture"); fixture.exports.push_back(modelExport);
      ResourceBody simple; simple.kind = "Model"; simple.modelLayout = 133;
      encodeResource(simple, fixture, fixture.exports[0], identity);
      fixture.version = 129; fixture.licensee = 39;
      auto normal = fixture.serialize();
      writeNewFile(std::filesystem::u8path(argv[1]), normal);
      const auto verPath = std::filesystem::u8path(std::string(argv[1]) + ".ver121");
      std::uint8_t key = 0;
      for (unsigned char c : lower(verPath.filename().u8string())) key = std::uint8_t(key + c);
      Bytes wrapped;
      for (char c : std::string("Lineage2Ver121")) { wrapped.push_back(std::uint8_t(c)); wrapped.push_back(0); }
      for (auto byte : normal) wrapped.push_back(byte ^ key);
      writeNewFile(verPath, wrapped);
      fixture.version = 133; fixture.licensee = 40;
      check(fixture.exports[0].payload.size() > 42, "Short test Model fixture");
      auto validPayload = fixture.exports[0].payload;
      fixture.exports[0].payload[42] = 0x40;  // Noncanonical zero vector count.
      writeNewFile(std::filesystem::u8path(std::string(argv[1]) + ".invalid"), fixture.serialize());
      auto invalidExport = fixture.exports[0];
      invalidExport.name = fixture.intern("MalformedModel");
      fixture.exports[0].payload = std::move(validPayload);
      fixture.exports[0].flags |= 0x02000000;  // Unverified first, malformed second.
      fixture.exports.push_back(std::move(invalidExport));
      writeNewFile(std::filesystem::u8path(std::string(argv[1]) + ".mixed"), fixture.serialize());
    } else check(argc == 1, "Unexpected fixture arguments");
    std::cout << "Model 133 codec tests passed\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
