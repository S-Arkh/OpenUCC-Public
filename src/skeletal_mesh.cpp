#include "openucc/skeletal_mesh.hpp"
#include <stdexcept>

namespace openucc {
namespace {
void check(bool ok, const char* text) { if (!ok) throw std::runtime_error(text); }
void profile(const Package& p) {
  check(((p.version >= 131 && p.version <= 133) && p.licensee == 40) ||
        (p.version == 123 && (p.licensee == 36 || p.licensee == 37)), "Unsupported mesh profile");
}
struct Decode {
  Reader& r;
  const Package& p;
  const Export& e;
  void value(std::uint32_t& v) { v = r.u32(); }
  void value(std::uint16_t& v) { v = r.u16(); }
  void value(std::uint8_t& v) { v = r.byte(); }
  void name(std::string& v) { v = p.name(r.index()); }
  void reference(int& v) { v = r.index(); p.ref(v); }
  template<class T, std::size_t N> void value(std::array<T, N>& a) {
    for (auto& v : a) value(v);
  }
  template<class T, class F> void array(std::vector<T>& a, F element, std::size_t minimum) {
    int n = r.index();
    check(n >= 0 && n <= 1000000 && std::size_t(n) <= (r.data.size() - r.pos) / minimum,
          "Invalid skeletal array count");
    a.resize(std::size_t(n)); for (auto& v : a) element(v);
  }
  template<class T> void value(std::vector<T>& a) {
    array(a, [&](T& v) { value(v); }, sizeof(T));
  }
  template<class T, class F> void lazy(std::vector<T>& a, F element, std::size_t minimum) {
    auto end = r.u32(); array(a, element, minimum);
    check(end == e.sourceOffset + r.pos, "Invalid skeletal lazy boundary");
  }
};
struct Encode {
  Bytes& out;
  Package& p;
  Export& e;
  const std::function<int(int)>& ref;
  void value(std::uint32_t& v) { u32(out, v); }
  void value(std::uint16_t& v) { u16(out, v); }
  void value(std::uint8_t& v) { out.push_back(v); }
  void name(std::string& v) { index(out, p.intern(v)); }
  void reference(int& v) { int mapped = ref(v); p.ref(mapped); index(out, mapped); }
  template<class T, std::size_t N> void value(std::array<T, N>& a) {
    for (auto& v : a) value(v);
  }
  template<class T, class F> void array(std::vector<T>& a, F element, std::size_t) {
    check(a.size() <= 1000000, "Excessive skeletal array");
    index(out, int(a.size())); for (auto& v : a) element(v);
  }
  template<class T> void value(std::vector<T>& a) {
    array(a, [&](T& v) { value(v); }, sizeof(T));
  }
  template<class T, class F> void lazy(std::vector<T>& a, F element, std::size_t minimum) {
    auto word = out.size(); u32(out, 0); array(a, element, minimum);
    e.lazyOffsets.push_back({word, out.size()});
  }
};
template<class A> void wedge(A& a, SkeletalWedge& v) { a.value(v.vertex); a.value(v.uv); }
template<class A> void influence(A& a, SkeletalInfluence& v) {
  a.value(v.weight); a.value(v.point); a.value(v.bone);
}
template<class A> void section(A& a, SkeletalSection& v) { a.value(v.fields); a.value(v.bones); }
template<class A> void indices(A& a, SkeletalIndexBuffer& v) { a.value(v.indices); a.value(v.revision); }
template<class A> void lod(A& a, SkeletalLOD& v) {
  a.value(v.skinning); a.value(v.skinPoints); a.value(v.softWedges);
  a.array(v.softSections, [&](SkeletalSection& s) { section(a, s); }, 19);
  a.array(v.rigidSections, [&](SkeletalSection& s) { section(a, s); }, 19);
  indices(a, v.softIndices); indices(a, v.rigidIndices);
  a.value(v.vertexHeader); a.value(v.vertices);
  a.lazy(v.influences, [&](SkeletalInfluence& i) { influence(a, i); }, 8);
  a.lazy(v.wedges, [&](SkeletalWedge& w) { wedge(a, w); }, 10);
  a.lazy(v.faces, [&](auto& f) { a.value(f); }, 8);
  a.lazy(v.points, [&](auto& p) { a.value(p); }, 12);
  a.value(v.fields); a.value(v.newWedges);
  a.array(v.lineageWedges, [&](SkeletalLineageWedge& w) {
    a.value(w.vectors); a.value(w.bones); a.value(w.weights);
  }, 52);
}
template<class A> void prefix(A& a, LodMeshBody& v) {
  check(v.layout == a.p.version, "Mismatched LodMesh layout");
  a.value(v.box); a.value(v.boxValid); a.value(v.sphere);
  a.value(v.version); check(v.version >= 5 && v.version <= 7, "Unsupported skeletal LodMesh version");
  a.value(v.vertexCount);
  if (v.layout >= 133) {
    check(v.originalPackedVertices.empty(), "Modern LodMesh cannot store packed original vertices");
    a.value(v.vertices);
  } else {
    check(v.vertices.empty(), "Legacy LodMesh cannot store float original vertices");
    a.value(v.originalPackedVertices);
  }
  a.array(v.textures, [&](int& ref) { a.reference(ref); }, 1);
  a.value(v.transforms); a.value(v.faceLevels); a.value(v.faces); a.value(v.collapseWedges);
  a.array(v.wedges, [&](SkeletalWedge& w) { wedge(a, w); }, 10);
  a.value(v.materials); a.value(v.lodFields); a.value(v.hasImpostor); a.reference(v.spriteMaterial);
  a.value(v.impostorTransforms); a.value(v.impostorColor); a.value(v.impostorModes);
  a.value(v.skinTessellation); a.value(v.extensionWord);
  for (std::size_t i = 0; i < v.extensionBytes.size(); ++i) {
    if (i < v.version - 5) a.value(v.extensionBytes[i]);
    else check(v.extensionBytes[i] == 0, "Unused LodMesh extension byte is nonzero");
  }
}
template<class A> void body(A& a, SkeletalMeshBody& v) {
  prefix(a, v); a.value(v.points2);
  a.array(v.bones, [&](SkeletalBone& b) {
    a.name(b.name); a.value(b.flags); a.value(b.pose); a.value(b.children); a.value(b.parent);
  }, 57);
  a.reference(v.animation); a.value(v.depth);
  a.array(v.weightIndices, [&](SkeletalWeightIndex& i) { a.value(i.indices); a.value(i.start); }, 5);
  a.value(v.boneInfluences);
  a.array(v.attachAliases, [&](std::string& n) { a.name(n); }, 1);
  a.array(v.attachBones, [&](std::string& n) { a.name(n); }, 1);
  a.value(v.attachCoords); a.array(v.lods, [&](SkeletalLOD& l) { lod(a, l); }, 1);
  a.reference(v.auxiliaryObject);
  a.lazy(v.points, [&](auto& p) { a.value(p); }, 12);
  a.lazy(v.meshWedges, [&](SkeletalWedge& w) { wedge(a, w); }, 10);
  a.lazy(v.triangles, [&](SkeletalTriangle& t) {
    a.value(t.wedges); a.value(t.material); a.value(t.auxiliaryMaterial); a.value(t.smoothing);
  }, 12);
  a.lazy(v.influences, [&](SkeletalInfluence& i) { influence(a, i); }, 8);
  a.lazy(v.collapse, [&](std::uint16_t& i) { a.value(i); }, 2);
  a.lazy(v.faceRemap, [&](std::uint16_t& i) { a.value(i); }, 2);
  a.value(v.tailFirst); a.value(v.tailFloats); a.value(v.tailWords);
}
}
SkeletalMeshBody decodeSkeletalMesh(Reader& r, const Package& p, const Export& e) {
  check(p.version != 123 || p.licensee == 36, "Unsupported SkeletalMesh profile");
  profile(p); SkeletalMeshBody result; result.layout = p.version; Decode decoder{r, p, e}; body(decoder, result);
  check(r.pos == r.data.size(), "Trailing SkeletalMesh native bytes"); return result;
}
void encodeSkeletalMesh(const SkeletalMeshBody& mesh, Package& p, Bytes& out, Export& e,
                        const std::function<int(int)>& ref) {
  check(p.version != 123 || p.licensee == 36, "Unsupported SkeletalMesh profile");
  profile(p);
  auto copy = mesh; Encode encoder{out, p, e, ref}; body(encoder, copy);
}
VertMeshBody decodeVertMesh(Reader& r, const Package& p, const Export& e) {
  check((p.version == 133 && p.licensee == 40) || (p.version == 123 && p.licensee == 37), "Unsupported VertMesh profile");
  profile(p); VertMeshBody v; v.layout = p.version; Decode a{r, p, e}; prefix(a, v);
  a.value(v.animatedVertices); a.value(v.streamVersion); a.value(v.packedVertices); a.value(v.floats);
  v.sequences = decodeAnimationSequences(r, p);
  a.value(v.normals); a.value(v.animatedVertexCount); a.value(v.frameCount);
  a.array(v.boxes, [&](VertMeshBox& b) { a.value(b.words); a.value(b.valid); }, 25);
  a.value(v.spheres);
  check(r.pos == r.data.size(), "Trailing VertMesh native bytes"); return v;
}
void encodeVertMesh(const VertMeshBody& mesh, Package& p, Bytes& out, Export& e,
                    const std::function<int(int)>& ref) {
  check((p.version == 133 && p.licensee == 40) || (p.version == 123 && p.licensee == 37), "Unsupported VertMesh profile");
  profile(p); auto v = mesh; Encode a{out, p, e, ref}; prefix(a, v);
  a.value(v.animatedVertices); a.value(v.streamVersion); a.value(v.packedVertices); a.value(v.floats);
  encodeAnimationSequences(v.sequences, p, out, ref);
  a.value(v.normals); a.value(v.animatedVertexCount); a.value(v.frameCount);
  a.array(v.boxes, [&](VertMeshBox& b) { a.value(b.words); a.value(b.valid); }, 25);
  a.value(v.spheres);
}
}
