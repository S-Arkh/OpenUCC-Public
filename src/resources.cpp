#include "openucc/resources.hpp"

#include <algorithm>
#include <set>
#include <stdexcept>

namespace openucc {
bool animationNotifyResource(const std::string& kind) {
  static const std::set<std::string> kinds = {
      "AnimNotify_CameraLocation", "AnimNotify_Sound", "AnimNotify_RandomSound",
      "AnimNotify_AttackShot", "AnimNotify_Illusion", "AnimNotify_BoneScaleScript",
      "AnimNotify_Effect", "AnimNotify_SwimSound", "AnimNotify_Channeling",
      "AnimNotify_PlayCustomAnimation", "AnimNotify_AttackPreShot", "AnimNotify_DestroyEffect",
      "AnimNotify_AttackItem", "AnimNotify_ViewShake", "AnimNotify_Light", "AnimNotify_BoneScale",
      "AnimNotify_AttackVoice", "AnimNotify_JumpSound", "AnimNotify_LaunchAccumulativeBeam",
      "AnimNotify_ModifyVisBound", "AnimNotify_JumpDown", "AnimNotify_JumpUp", "AnimNotify_PawnStatusVoice",
      "AnimNotify_EventAnimEnd", "AnimNotify_BoneDirect", "AnimNotify_Ribbon", "AnimNotify_HideWeapon",
      "AnimNotify_PendingPawnEffectState", "AnimNotify_SkillEffect", "AnimNotify_HidePawn",
      "AnimNotify_ScreenFade", "AnimNotify_IdleSound"};
  return kinds.count(kind) != 0;
}
bool materialResource(const std::string& kind) {
  static const std::set<std::string> kinds = {
      "Shader", "FinalBlend", "Combiner", "TexCoordSource", "TexPanner",
      "FadeColor", "TexOscillator", "TexEnvMap", "VertexColor", "TexRotator",
      "ColorModifier", "TexPannerTriggered", "TexScaler", "ConstantColor",
      "TexOscillatorTriggered", "TexPlanningPanner", "FresnelMaterial", "MaterialSequence", "L2ColorModifier"};
  return kinds.count(kind) != 0;
}
namespace {
bool soundProfile(const Package& p) {
  return ((p.version == 130 || p.version == 131 || p.version == 133) && p.licensee == 40) ||
         (p.version == 128 && p.licensee == 39) || (p.version == 123 && p.licensee == 23);
}
bool resourceProfile(const Package& p, const std::string& kind) {
  if (kind == "Model")
    return (p.version == 123 && p.licensee == 37) ||
           isModel133FamilyProfile(p);
  if (kind == "Polys" || kind == "KMeshProps")
    return (p.version == 123 && p.licensee == 37) ||
        (p.version == 127 && p.licensee == 39) ||
        (kind == "Polys" && p.version == 128 && p.licensee == 39) ||
        ((p.version == 130 || p.version == 132 || p.version == 133) && p.licensee == 40);
  if (kind == "Package" && staticMeshProfile(p.version, p.licensee)) return true;
  if (kind == "StaticMesh") return staticMeshProfile(p.version, p.licensee);
  if (p.version == 123 && (p.licensee == 12 || p.licensee == 19 || p.licensee == 23))
    if (kind == "Texture" || (kind == "Palette" && p.licensee == 12) ||
        (kind == "Shader" && p.licensee == 23)) return true;
  if (kind == "WetTexture") return p.version == 127 && p.licensee == 39;
  if (kind == "MaskTexture") return p.version == 133 && p.licensee == 40;
  if (p.version == 123 && p.licensee >= 33 && p.licensee <= 36)
    if (kind == "Texture" || kind == "Shader" || kind == "Palette") return true;
  if (((p.version >= 130 && p.version <= 132 && p.licensee == 40) ||
       (p.version == 123 && p.licensee == 38) ||
       (p.version == 125 && (p.licensee == 38 || p.licensee == 39)) ||
       (p.version >= 126 && p.version <= 129 && p.licensee == 39)) &&
      (kind == "Texture" || kind == "Cubemap" || kind == "Palette" ||
       kind == "Package" || materialResource(kind))) return true;
  if ((p.version == 133 && p.licensee == 40) || (p.version == 123 && p.licensee == 37)) return true;
  if ((kind == "Sound" || kind == "Package") && soundProfile(p)) return true;
  if (p.version == 126 && p.licensee == 0) return kind == "Texture";
  if (p.version == 132 && p.licensee == 40) return kind == "SkeletalMesh";
  if ((p.version == 131 && p.licensee == 40) || (p.version == 123 && p.licensee == 36))
    return kind == "SkeletalMesh" || kind == "MeshAnimation" ||
           (p.version == 131 && animationNotifyResource(kind));
  return false;
}
bool dropField(const std::string& name) {
  static const std::set<std::string> names = {"type", "depth", "x", "y", "bytea", "byteb", "bytec", "byted"};
  return names.count(lower(name)) != 0;
}
bool proceduralMip(const ResourceBody& body, const ResourceMip& mip, int format, std::size_t levels) {
  return body.kind == "WetTexture" && format == 0 && levels == 1 && mip.pixels.empty() &&
      ((mip.width == 128 && mip.widthBits == 7) || (mip.width == 256 && mip.widthBits == 8)) &&
      mip.height == mip.width && mip.heightBits == mip.widthBits &&
      std::any_of(body.tags.begin(), body.tags.end(), [](const ResourceTag& tag) {
        return lower(tag.name) == "bparametric" && tag.kind == 3 && tag.boolean;
      });
}
void require(bool value, const char* message) {
  if (!value) throw std::runtime_error(message);
}
int propertySize(Reader& r, int info) {
  const int sizes[] = {1, 2, 4, 12, 16};
  int code = (info >> 4) & 7;
  int size = code < 5 ? sizes[code] : code == 5 ? r.byte() : code == 6 ? r.u16() : r.i32();
  require(size >= 0, "Negative matrix property size");
  return size;
}
bool planeName(const std::string& name) {
  auto n = lower(name); return n == "xplane" || n == "yplane" || n == "zplane" || n == "wplane";
}
bool componentName(const std::string& name) {
  auto n = lower(name); return n == "x" || n == "y" || n == "z" || n == "w";
}
std::vector<ResourceMatrixPlane> readMatrix(const Package& p, Reader& r) {
  std::vector<ResourceMatrixPlane> result; std::set<std::string> planes;
  for (;;) {
    auto name = p.name(r.index()); if (lower(name) == "none") break;
    require(planeName(name) && planes.insert(lower(name)).second, "Invalid matrix plane name");
    int info = r.byte();
    require((info & 143) == 10 && lower(p.name(r.index())) == "plane", "Invalid matrix plane tag");
    auto data = r.take(std::size_t(propertySize(r, info))); Reader plane{data};
    ResourceMatrixPlane field; field.name = name; std::set<std::string> components;
    for (;;) {
      auto component = p.name(plane.index()); if (lower(component) == "none") break;
      require(componentName(component) && components.insert(lower(component)).second, "Invalid plane component name");
      int tag = plane.byte();
      require((tag & 143) == 4 && propertySize(plane, tag) == 4, "Invalid plane float tag");
      field.components.push_back({component, plane.u32()});
    }
    require(plane.pos == data.size(), "Trailing matrix plane bytes"); result.push_back(std::move(field));
  }
  return result;
}
Bytes writeMatrix(const std::vector<ResourceMatrixPlane>& matrix, Package& p) {
  Bytes result; std::set<std::string> planes;
  for (const auto& plane : matrix) {
    require(planeName(plane.name) && planes.insert(lower(plane.name)).second, "Invalid matrix plane name");
    Bytes value; std::set<std::string> components;
    for (const auto& component : plane.components) {
      require(componentName(component.first) && components.insert(lower(component.first)).second, "Invalid plane component name");
      index(value, p.intern(component.first)); value.push_back(0x24); u32(value, component.second);
    }
    index(value, p.intern("None"));
    index(result, p.intern(plane.name)); result.push_back(0x5a); index(result, p.intern("Plane"));
    require(value.size() <= 255, "Oversized matrix plane"); result.push_back(std::uint8_t(value.size()));
    result.insert(result.end(), value.begin(), value.end());
  }
  index(result, p.intern("None")); return result;
}
int count(Reader& r, int maximum) {
  auto n = r.index();
  require(n >= 0 && n <= maximum, "Invalid resource array count");
  return n;
}
template <std::size_t N>
void bytes(Reader& r, std::array<std::uint8_t, N>& out) {
  for (auto& value : out) value = r.byte();
}
template <class T>
void append(Bytes& out, const T& data) {
  out.insert(out.end(), data.begin(), data.end());
}
int subscript(Reader& r) {
  int b = r.byte();
  if (b < 128) return b;
  if (b < 192) return ((b & 63) << 8) | r.byte();
  int result = (b & 63) << 24;
  result |= int(r.byte()) << 16;
  result |= int(r.byte()) << 8;
  return result | r.byte();
}
void subscript(Bytes& out, int n) {
  require(n >= 0 && n < 0x40000000, "Invalid resource property subscript");
  if (n < 128)
    out.push_back(std::uint8_t(n));
  else if (n < 16384) {
    out.push_back(std::uint8_t(128 | (n >> 8)));
    out.push_back(std::uint8_t(n));
  } else {
    out.push_back(std::uint8_t(192 | (n >> 24)));
    out.push_back(std::uint8_t(n >> 16));
    out.push_back(std::uint8_t(n >> 8));
    out.push_back(std::uint8_t(n));
  }
}
std::size_t mipSize(int format, unsigned w, unsigned h) {
  require(w && h && w <= 8192 && h <= 8192, "Invalid mip dimensions");
  auto area = std::size_t(w) * h;
  if (format == 0) return area;
  if (format == 10) return area * 2;  // G16 terrain height samples.
  if (format == 5) return area * 4;
  if (format == 3) return std::size_t((w + 3) / 4) * ((h + 3) / 4) * 8;
  if (format == 7 || format == 8)
    return std::size_t((w + 3) / 4) * ((h + 3) / 4) * 16;
  throw std::runtime_error("Unsupported resource pixel format");
}
}  // namespace

static ResourceBody decodeResourceStrict(const Package& p, int id) {
  ResourceBody body;
  body.kind = p.kind(id);
  require(resourceProfile(p, body.kind), "Unsupported resource reconstruction profile");
  require(p.version != 126 || p.licensee != 0 || body.kind == "Texture",
          "Only Texture reconstruction is verified for profile 126/0");
  require(body.kind == "Model" || body.kind == "Polys" || body.kind == "KMeshProps" || body.kind == "Texture" || body.kind == "Cubemap" || body.kind == "MaskTexture" || body.kind == "WetTexture" || body.kind == "Palette" ||
              body.kind == "Font" || body.kind == "Package" || body.kind == "StaticMesh" || body.kind == "MeshAnimation" || body.kind == "SkeletalMesh" || body.kind == "VertMesh" || body.kind == "Sound" || materialResource(body.kind) || animationNotifyResource(body.kind),
          "Unsupported resource class");
  const auto& e = p.exports.at(std::size_t(id - 1));
  require(!animationNotifyResource(body.kind) || ((p.version == 131 || p.version == 133) && p.licensee == 40),
          "Unsupported animation notification profile");
  require(!(e.flags & 0x02000000), "Stacked resource is unsupported");
  Reader r{e.payload};
  if (body.kind == "Package") {
    require(e.super == 0 && lower(p.name(r.index())) == "none" &&
                r.pos == r.data.size(),
            "Unsupported resource group payload");
    return body;
  }
  for (;;) {
    ResourceTag tag;
    tag.name = p.name(r.index());
    if (lower(tag.name) == "none") break;
    require(body.tags.size() < 10000, "Excessive resource properties");
    int info = r.byte(), code = (info >> 4) & 7;
    tag.kind = info & 15;
    if (tag.kind == 10) tag.structure = p.name(r.index());
    const int sizes[] = {1, 2, 4, 12, 16};
    int size = code < 5    ? sizes[code]
               : code == 5 ? r.byte()
               : code == 6 ? r.u16()
                           : r.i32();
    require(size >= 0, "Negative resource property size");
    if (tag.kind == 3) {
      tag.boolean = (info & 128) != 0;
      tag.booleanSize = size;
    }
    else {
      if (info & 128) tag.subscript = subscript(r);
      tag.value = r.take(std::size_t(size));
      Reader value{tag.value};
      if (tag.kind == 10 && lower(tag.structure) == "matrix" && materialResource(body.kind)) {
        tag.matrix = readMatrix(p, value); tag.value.clear();
      } else if (tag.kind == 9 && body.kind == "StaticMesh" && lower(tag.name) == "materials") {
        auto n = count(value, 65536);
        for (int i = 0; i < n; ++i) {
          std::vector<ResourceTag> element;
          for (;;) {
            ResourceTag field;
            field.name = p.name(value.index());
            if (lower(field.name) == "none") break;
            require(element.size() < 10000, "Excessive material properties");
            int fieldInfo = value.byte(), fieldCode = (fieldInfo >> 4) & 7;
            field.kind = fieldInfo & 15;
            require(field.kind == 3 || field.kind == 5, "Unsupported material property");
            int fieldSize = fieldCode < 5 ? sizes[fieldCode] : fieldCode == 5 ? value.byte() : fieldCode == 6 ? value.u16() : value.i32();
            require(fieldSize >= 0, "Negative material property size");
            if (field.kind == 3) {
              field.boolean = (fieldInfo & 128) != 0; field.booleanSize = fieldSize;
            }
            else {
              if (fieldInfo & 128) field.subscript = subscript(value);
              auto data = value.take(std::size_t(fieldSize)); Reader ref{data};
              field.reference = ref.index(); p.ref(field.reference);
              require(ref.pos == data.size(), "Trailing material reference bytes");
            }
            element.push_back(std::move(field));
          }
          tag.elements.push_back(std::move(element));
        }
        tag.value.clear();
      } else if (tag.kind == 9 && body.kind == "MaskTexture" && lower(tag.name) == "rledata") {
        auto n = count(value, 1000000);
        for (int i = 0; i < n; ++i) tag.words.push_back(value.u32());
        tag.value.clear();
      } else if (tag.kind == 9 && body.kind == "MaterialSequence" && lower(tag.name) == "sequenceitems") {
        auto n = count(value, 65536);
        for (int i = 0; i < n; ++i) {
          std::vector<ResourceTag> fields;
          for (;;) {
            ResourceTag field; field.name = p.name(value.index());
            if (lower(field.name) == "none") break;
            require(fields.size() < 64, "Excessive material sequence fields");
            auto info = value.byte(); field.kind = info & 15; auto code = (info >> 4) & 7;
            require(!(info & 128), "Indexed material sequence field");
            auto name = lower(field.name);
            require((name == "material" && field.kind == 5) || (name == "time" && field.kind == 4) ||
                    (name == "action" && field.kind == 1), "Unknown material sequence field");
            int size = code < 5 ? sizes[code] : code == 5 ? value.byte() : code == 6 ? value.u16() : value.i32();
            require(size >= 0, "Negative sequence field size");
            auto data = value.take(std::size_t(size)); Reader item{data};
            if (field.kind == 5) { field.reference = item.index(); p.ref(field.reference); }
            else { require(size == (field.kind == 1 ? 1 : 4), "Invalid sequence scalar width"); field.value = item.take(size); }
            require(item.pos == data.size(), "Trailing material sequence field bytes");
            fields.push_back(std::move(field));
          }
          tag.elements.push_back(std::move(fields));
        }
        tag.value.clear();
      } else if (tag.kind == 9 && ((body.kind == "AnimNotify_RandomSound" && lower(tag.name) == "sounds") ||
                                 (body.kind == "VertMesh" && lower(tag.name) == "materials"))) {
        auto n = count(value, 65536);
        for (int i = 0; i < n; ++i) {
          int ref = value.index(); p.ref(ref); tag.references.push_back(ref);
        }
        tag.value.clear();
      } else if (tag.kind == 9 && body.kind == "AnimNotify_PawnStatusVoice" && lower(tag.name) == "soundname") {
        auto n = count(value, 65536);
        for (int i = 0; i < n; ++i) tag.strings.push_back(value.string());
        tag.value.clear();
      } else if (tag.kind == 10 && body.kind == "WetTexture" && lower(tag.structure) == "adrop") {
        require(lower(tag.name) == "drops" && tag.subscript < 256, "Invalid water drop selector");
        std::set<std::string> seen;
        for (;;) {
          auto name = p.name(value.index()); if (lower(name) == "none") break;
          require(dropField(name) && seen.insert(lower(name)).second, "Unknown or repeated water drop field");
          auto info = value.byte(); auto code = (info >> 4) & 7;
          require((info & 143) == 1, "Invalid water drop field type");
          int size = code < 5 ? sizes[code] : code == 5 ? value.byte() : code == 6 ? value.u16() : value.i32();
          require(size == 1, "Invalid water drop byte width");
          tag.byteFields.push_back({name, value.byte()});
        }
        tag.value.clear();
      } else if (tag.kind == 5) {
        tag.reference = value.index();
        p.ref(tag.reference);
      } else if (tag.kind == 6)
        tag.nameValue = p.name(value.index());
      else if (tag.kind == 13)
        tag.nameValue = value.string();
      else {
        require((tag.kind == 1 && size == 1) ||
                    ((tag.kind == 2 || tag.kind == 4) && size == 4) ||
                     (tag.kind == 10 && ((lower(tag.structure) == "color" && size == 4) ||
                      ((materialResource(body.kind) || animationNotifyResource(body.kind)) && lower(tag.structure) == "rotator" && size == 12) ||
                      (animationNotifyResource(body.kind) && lower(tag.structure) == "vector" && size == 12))),
                "Unsupported resource property encoding");
        value.take(std::size_t(size));
      }
      require(value.pos == std::size_t(size),
              "Trailing resource property bytes");
    }
    body.tags.push_back(std::move(tag));
  }
  if (body.kind == "Model") {
    require(body.tags.empty(), "Unsupported Model properties");
    body.modelLayout = p.version;
    if (isModel133FamilyProfile(p)) body.model133 = decodeModel133(r, p, e);
    else body.model = decodeModel(r, p);
  } else if (body.kind == "Polys") {
    require(body.tags.empty(), "Unsupported Polys properties");
    auto n = r.u32(); body.polygonCapacity = r.u32();
    require(n <= 1000000 && body.polygonCapacity >= n && body.polygonCapacity <= 1000000,
            "Invalid polygon count or capacity");
    for (std::uint32_t i = 0; i < n; ++i) {
      ResourcePolygon poly; auto vertices = count(r, 16);
      for (auto& word : poly.vectors) word = r.u32();
      poly.vertices.resize(vertices);
      for (auto& vertex : poly.vertices) for (auto& word : vertex) word = r.u32();
      poly.flags = r.u32(); poly.actor = r.index(); p.ref(poly.actor);
      poly.material = r.index(); p.ref(poly.material); poly.itemName = p.name(r.index());
      poly.link = r.index(); poly.brushPolygon = r.index();
      poly.lightMapScale = r.u32(); poly.licenseWord = r.u32();
      body.polygons.push_back(std::move(poly));
    }
  } else if (body.kind == "KMeshProps") {
    require(body.tags.empty(), "Unsupported KMeshProps properties");
    for (auto& word : body.karmaScalarBits) word = r.u32();
    for (int i = 0; i < 4; ++i) require(r.index() == 0, "Unsupported nonempty Karma geometry");
  } else if (animationNotifyResource(body.kind)) {
    require(r.pos == r.data.size(), "Trailing animation notification bytes");
  } else if (body.kind == "StaticMesh") {
    body.mesh = decodeStaticMesh(r, p, e);
  } else if (body.kind == "MeshAnimation") {
    body.animation = decodeAnimation(r, p, e);
  } else if (body.kind == "SkeletalMesh") {
    body.skeletalMesh = decodeSkeletalMesh(r, p, e);
  } else if (body.kind == "VertMesh") {
    body.vertMesh = decodeVertMesh(r, p, e);
  } else if (body.kind == "Sound") {
    require(soundProfile(p), "Unsupported Sound profile");
    body.soundFormat = p.name(r.index());
    require(lower(body.soundFormat) == "wav", "Unsupported Sound data format");
    auto end = r.u32();
    body.soundData = r.take(std::size_t(count(r, 256 * 1024 * 1024)));
    require(end == e.sourceOffset + r.pos, "Invalid Sound lazy boundary");
  } else if (body.kind == "Palette") {
    require(count(r, 256) == 256, "Palette must have 256 colors");
    body.colors.resize(256);
    for (auto& color : body.colors) bytes(r, color);
  } else if (body.kind == "Font") {
    body.glyphs.resize(std::size_t(count(r, 65536)));
    for (auto& glyph : body.glyphs) {
      for (auto& coordinate : glyph.rectangle) coordinate = r.u32();
      glyph.page = r.byte();
    }
    body.pages.resize(std::size_t(count(r, 256)));
    for (auto& page : body.pages) {
      page = r.index();
      require(page > 0 && p.kind(page) == "Texture",
              "Invalid font texture page");
    }
    for (const auto& glyph : body.glyphs)
      require(glyph.page < body.pages.size(), "Invalid glyph page index");
    // The corpus has only the empty suffix. Nonempty remapping/extra metrics
    // require separate format evidence rather than an opaque copy.
    require(r.take(9) == Bytes(9, 0), "Unsupported nonempty font suffix");
  } else {
    body.textureLayout = p.version;
    if (p.version == 123 && p.licensee < 30) {
      if (p.licensee >= 16) body.obsoleteMaterialWord = r.u32();
    } else if (p.version == 123 && p.licensee >= 33 && p.licensee <= 36) {
      body.legacyMaterialLicensee = p.licensee;
      body.obsoleteMaterialWord = r.u32();
      if (p.licensee < 36) body.legacyMaterialFlag = r.byte();
      bytes(r, body.legacyMaterialHeader);
      for (auto& stage : body.legacyMaterialStages) {
        stage.first = r.byte();
        if (p.licensee < 36) stage.second = r.byte();
        bytes(r, stage.fields);
      }
      bytes(r, body.materialColors);
      for (auto& word : body.materialWords) word = r.u32();
      for (auto& text : body.legacyMaterialStrings) text = r.string();
      body.shader = r.string();
      body.materialVersion = r.u32();
    } else if (p.licensee != 0) {
      bytes(r, body.materialHeader);
      if (body.textureLayout < 129) {
        bytes(r, body.singleStageBytes);
        for (auto& word : body.singleStageWords) word = r.u32();
      } else if (body.textureLayout == 129) {
        for (std::size_t i = 0; i < 5; ++i) {
          bytes(r, body.transitionalStageBytes[i]);
          for (auto& word : body.transitionalStageWords[i]) word = r.u32();
        }
      } else
        for (std::size_t i = 0; i < 5; ++i) {
          bytes(r, body.stageBytes[i]);
          for (auto& word : body.stageWords[i]) word = r.u32();
        }
      bytes(r, body.materialColors);
      for (auto& word : body.materialWords) word = r.u32();
      auto stages = count(r, 1024);
      for (int i = 0; i < stages; ++i) {
        auto name = r.string();
        std::vector<std::string> values;
        auto n = count(r, 1024);
        for (int j = 0; j < n; ++j) values.push_back(r.string());
        body.stageStrings.push_back({std::move(name), std::move(values)});
      }
      body.shader = r.string();
      body.materialVersion = r.u32();
    }
    if (materialResource(body.kind)) {
      require(r.pos == r.data.size(), "Trailing material native bytes");
      return body;
    }
    int format = 0;
    for (const auto& tag : body.tags)
      if (lower(tag.name) == "format") {
        require(tag.kind == 1, "Invalid texture format property");
        format = tag.value.at(0);
      }
    auto n = count(r, 32);
    for (int i = 0; i < n; ++i) {
      auto end = r.u32();
      ResourceMip mip;
      mip.pixels = r.take(std::size_t(count(r, 256 * 1024 * 1024)));
      require(!end || end == e.sourceOffset + r.pos,
              "Invalid mip lazy-array offset");
      mip.width = r.u32();
      mip.height = r.u32();
      mip.widthBits = r.byte();
      mip.heightBits = r.byte();
      // Observed terminal DXT3/RGBA8 placeholder: no data is stored for the final
      // 1x1 level. Preserve the absent data rather than inventing a pixel block.
      bool emptyTerminal = p.licensee == 40 &&
          ((p.version == 133 && (format == 7 || format == 5)) || (p.version == 132 && format == 5)) &&
          n > 1 && i == n - 1 && mip.pixels.empty() && mip.width == 1 &&
          mip.height == 1 && mip.widthBits == 0 && mip.heightBits == 0;
      require(mip.widthBits <= 16 && mip.heightBits <= 16 &&
                  (mip.pixels.size() == mipSize(format, mip.width, mip.height) || emptyTerminal || proceduralMip(body, mip, format, n)),
              "Invalid mip storage");
      body.mips.push_back(std::move(mip));
    }
  }
  require(r.pos == r.data.size(), "Trailing resource native bytes");
  return body;
}

ResourceBody decodeResource(const Package& p, int id) {
  const auto opaqueModel = [&](const std::string& reason) {
    ResourceBody body;
    body.kind = "Model";
    body.modelLayout = p.version;
    body.modelOpaque = true;
    body.modelWarning = reason;
    body.modelOpaquePayload = p.exports.at(std::size_t(id - 1)).payload;
    return body;
  };
  if (p.kind(id) == "Model") {
    const auto& e = p.exports.at(std::size_t(id - 1));
    if (!resourceProfile(p, "Model"))
      return opaqueModel("Unsupported resource reconstruction profile");
    if (e.flags & 0x02000000)
      return opaqueModel("Stacked resource is unsupported");
    // Only empty property lists have been verified for these Model codecs.
    // Do not interpret unknown tags as known scalar or reference encodings:
    // even a malformed-looking tag may use an unverified property layout.
    Reader tags{e.payload};
    if (lower(p.name(tags.index())) != "none")
      return opaqueModel("Unsupported Model properties");
  }
  try {
    return decodeResourceStrict(p, id);
  } catch (const std::runtime_error& error) {
    if (p.kind(id) != "Model") throw;
    const std::string reason = error.what();
    const bool unverified = reason.rfind("Unsupported Model", 0) == 0 ||
        reason.rfind("Unsupported adjacent-profile Model", 0) == 0 ||
        reason == "Unsupported resource reconstruction profile" ||
        reason == "Stacked resource is unsupported" ||
        reason == "Unsupported resource property encoding" ||
        reason == "Unsupported resource array" ||
        reason == "Trailing Model trailer bytes" ||
        reason == "Trailing Model native bytes";
    if (!unverified) throw;
    return opaqueModel(reason);
  }
}

void encodeResource(const ResourceBody& body, Package& p, Export& e,
                    const std::function<int(int)>& reference) {
  require(!body.modelOpaque,
          "Opaque Model requires whole-package preservation; relocation is not established");
  require(resourceProfile(p, body.kind), "Unsupported resource reconstruction profile");
  require(p.version != 126 || p.licensee != 0 || body.kind == "Texture",
          "Only Texture reconstruction is verified for profile 126/0");
  Bytes out;
  e.lazyOffsets.clear();
  if (body.kind == "Package") {
    require(body.tags.empty(), "Resource group properties are unsupported");
    index(out, p.intern("None"));
    e.payload = std::move(out);
    return;
  }
  for (const auto& tag : body.tags) {
    index(out, p.intern(tag.name));
    Bytes value = tag.value;
    const bool referenceArray = tag.kind == 9 &&
        ((body.kind == "AnimNotify_RandomSound" && lower(tag.name) == "sounds") ||
         (body.kind == "VertMesh" && lower(tag.name) == "materials"));
    if (referenceArray) {
      require(tag.references.size() <= 65536, "Excessive notification sounds");
      value.clear(); index(value, int(tag.references.size()));
      for (int ref : tag.references) { int mapped = reference(ref); p.ref(mapped); index(value, mapped); }
    }
    if (tag.kind == 10 && lower(tag.structure) == "matrix") {
      require(materialResource(body.kind), "Matrix requires a supported material");
      value = writeMatrix(tag.matrix, p);
    }
    if (tag.kind == 10 && lower(tag.structure) == "adrop") {
      require(body.kind == "WetTexture" && lower(tag.name) == "drops" && tag.subscript < 256, "Invalid water drop selector");
      value.clear(); std::set<std::string> seen;
      for (const auto& field : tag.byteFields) {
        require(dropField(field.first) && seen.insert(lower(field.first)).second, "Unknown or repeated water drop field");
        index(value, p.intern(field.first)); value.push_back(1); value.push_back(field.second);
      }
      index(value, p.intern("None"));
    }
    const bool stringArray = tag.kind == 9 && body.kind == "AnimNotify_PawnStatusVoice" && lower(tag.name) == "soundname";
    if (stringArray) {
      require(tag.strings.size() <= 65536, "Excessive notification sound names");
      value.clear(); index(value, int(tag.strings.size()));
      for (const auto& text : tag.strings) string(value, text);
    }
    const bool wordArray = tag.kind == 9 && body.kind == "MaskTexture" && lower(tag.name) == "rledata";
    if (wordArray) {
      require(tag.words.size() <= 1000000, "Excessive mask RLE words");
      value.clear(); index(value, int(tag.words.size()));
      for (auto word : tag.words) u32(value, word);
    }
    if (tag.kind == 9 && !referenceArray && !stringArray && !wordArray) {
      const bool sequence = body.kind == "MaterialSequence" && lower(tag.name) == "sequenceitems";
      require(sequence || (body.kind == "StaticMesh" && lower(tag.name) == "materials"), "Unsupported resource array");
      require(tag.elements.size() <= 65536, "Excessive material entries");
      value.clear(); index(value, int(tag.elements.size()));
      for (const auto& element : tag.elements) {
        for (const auto& field : element) {
          index(value, p.intern(field.name));
          if (sequence) {
            auto name = lower(field.name);
            require(field.subscript < 0 && ((name == "material" && field.kind == 5) ||
                    (name == "time" && field.kind == 4) || (name == "action" && field.kind == 1)),
                    "Unsupported material sequence field");
          } else require(field.kind == 3 || field.kind == 5, "Unsupported material property");
          if (sequence && (field.kind == 1 || field.kind == 4)) {
            require(field.value.size() == (field.kind == 1 ? 1u : 4u), "Invalid sequence scalar width");
            value.push_back(field.kind == 1 ? 0x01 : 0x24); append(value, field.value);
          } else if (field.kind == 3) {
            require(field.booleanSize >= 0, "Negative boolean property size");
            value.push_back(std::uint8_t(0x73 | (field.boolean ? 128 : 0)));
            u32(value, std::uint32_t(field.booleanSize));
          }
          else {
            Bytes ref; index(ref, reference(field.reference));
            value.push_back(std::uint8_t(0x55 | (field.subscript >= 0 ? 128 : 0)));
            value.push_back(std::uint8_t(ref.size()));
            if (field.subscript >= 0) subscript(value, field.subscript);
            append(value, ref);
          }
        }
        index(value, p.intern("None"));
      }
    }
    if (tag.kind == 5 || tag.kind == 6 || tag.kind == 13) {
      value.clear();
      if (tag.kind == 5) index(value, reference(tag.reference));
      if (tag.kind == 6) index(value, p.intern(tag.nameValue));
      if (tag.kind == 13) string(value, tag.nameValue);
    }
    if (tag.kind == 3) {
      require(tag.booleanSize >= 0, "Negative boolean property size");
      out.push_back(std::uint8_t(0x73 | (tag.boolean ? 128 : 0)));
      u32(out, std::uint32_t(tag.booleanSize));
      continue;
    }
    const std::size_t sizes[] = {1, 2, 4, 12, 16};
    int code = 5;
    for (int i = 0; i < 5; ++i)
      if (value.size() == sizes[i]) {
        code = i;
        break;
      }
    if (code == 5 && value.size() > 255) code = value.size() <= 65535 ? 6 : 7;
    out.push_back(
        std::uint8_t(tag.kind | (code << 4) | (tag.subscript >= 0 ? 128 : 0)));
    if (tag.kind == 10) index(out, p.intern(tag.structure));
    if (code == 5) out.push_back(std::uint8_t(value.size()));
    if (code == 6) u16(out, std::uint16_t(value.size()));
    if (code == 7) u32(out, std::uint32_t(value.size()));
    if (tag.subscript >= 0) subscript(out, tag.subscript);
    append(out, value);
  }
  index(out, p.intern("None"));
  if (body.kind == "Model") {
    require(body.tags.empty(), "Unsupported Model properties");
    require(body.modelLayout == p.version, "Mismatched Model layout");
    if (isModel133FamilyProfile(p)) encodeModel133(body.model133, p, out, e, reference);
    else encodeModel(body.model, p, out, reference);
  } else if (body.kind == "Polys") {
    require(body.tags.empty(), "Unsupported Polys properties");
    require(body.polygons.size() <= body.polygonCapacity && body.polygonCapacity <= 1000000,
            "Invalid polygon count or capacity");
    u32(out, std::uint32_t(body.polygons.size())); u32(out, body.polygonCapacity);
    for (const auto& poly : body.polygons) {
      require(poly.vertices.size() <= 16, "Excessive polygon vertices");
      index(out, int(poly.vertices.size()));
      for (auto word : poly.vectors) u32(out, word);
      for (const auto& vertex : poly.vertices) for (auto word : vertex) u32(out, word);
      u32(out, poly.flags); index(out, reference(poly.actor)); index(out, reference(poly.material));
      index(out, p.intern(poly.itemName)); index(out, poly.link); index(out, poly.brushPolygon);
      u32(out, poly.lightMapScale); u32(out, poly.licenseWord);
    }
  } else if (body.kind == "KMeshProps") {
    require(body.tags.empty(), "Unsupported KMeshProps properties");
    for (auto word : body.karmaScalarBits) u32(out, word);
    for (int i = 0; i < 4; ++i) index(out, 0);
  } else if (animationNotifyResource(body.kind)) {
    require((p.version == 131 || p.version == 133) && p.licensee == 40, "Unsupported animation notification profile");
  } else if (body.kind == "StaticMesh") {
    require(staticMeshProfile(p.version, p.licensee) && body.mesh.layout == p.version &&
                body.mesh.licensee == p.licensee, "Unsupported or mismatched StaticMesh profile");
    encodeStaticMesh(body.mesh, out, e, reference);
  } else if (body.kind == "MeshAnimation") {
    encodeAnimation(body.animation, p, out, e, reference);
  } else if (body.kind == "SkeletalMesh") {
    encodeSkeletalMesh(body.skeletalMesh, p, out, e, reference);
  } else if (body.kind == "VertMesh") {
    encodeVertMesh(body.vertMesh, p, out, e, reference);
  } else if (body.kind == "Sound") {
    require(soundProfile(p), "Unsupported Sound profile");
    require(lower(body.soundFormat) == "wav", "Unsupported Sound data format");
    require(body.soundData.size() <= 256 * 1024 * 1024, "Excessive Sound data");
    index(out, p.intern(body.soundFormat));
    auto offset = out.size(); u32(out, 0);
    index(out, int(body.soundData.size())); append(out, body.soundData);
    e.lazyOffsets.push_back({offset, out.size()});
  } else if (body.kind == "Palette") {
    index(out, int(body.colors.size()));
    for (const auto& color : body.colors) append(out, color);
  } else if (body.kind == "Font") {
    index(out, int(body.glyphs.size()));
    for (const auto& glyph : body.glyphs) {
      for (auto coordinate : glyph.rectangle) u32(out, coordinate);
      out.push_back(glyph.page);
    }
    index(out, int(body.pages.size()));
    for (int page : body.pages) index(out, reference(page));
    out.insert(out.end(), 9, 0);
  } else if (body.kind == "Texture" || body.kind == "Cubemap" || body.kind == "MaskTexture" || body.kind == "WetTexture" || materialResource(body.kind)) {
    require(body.textureLayout == p.version,
            "Texture material layout does not match the output profile");
    const bool legacy = p.version == 123 && p.licensee >= 33 && p.licensee <= 36;
    require(body.legacyMaterialLicensee == (legacy ? p.licensee : 0),
            "Legacy material layout does not match the output profile");
    if (p.version == 123 && p.licensee < 30) {
      if (p.licensee >= 16) u32(out, body.obsoleteMaterialWord);
    } else if (legacy) {
      u32(out, body.obsoleteMaterialWord);
      if (p.licensee < 36) out.push_back(body.legacyMaterialFlag);
      else require(body.legacyMaterialFlag == 0, "Legacy flag absent from licensee 36");
      append(out, body.legacyMaterialHeader);
      for (const auto& stage : body.legacyMaterialStages) {
        out.push_back(stage.first);
        if (p.licensee < 36) out.push_back(stage.second);
        else require(stage.second == 0, "Legacy stage flag absent from licensee 36");
        append(out, stage.fields);
      }
      append(out, body.materialColors);
      for (auto word : body.materialWords) u32(out, word);
      for (const auto& text : body.legacyMaterialStrings) string(out, text);
      string(out, body.shader);
      u32(out, body.materialVersion);
    } else if (p.licensee != 0) {
      append(out, body.materialHeader);
      if (body.textureLayout < 129) {
        append(out, body.singleStageBytes);
        for (auto word : body.singleStageWords) u32(out, word);
      } else if (body.textureLayout == 129) {
        for (std::size_t i = 0; i < 5; ++i) {
          append(out, body.transitionalStageBytes[i]);
          for (auto word : body.transitionalStageWords[i]) u32(out, word);
        }
      } else
        for (std::size_t i = 0; i < 5; ++i) {
          append(out, body.stageBytes[i]);
          for (auto word : body.stageWords[i]) u32(out, word);
        }
      append(out, body.materialColors);
      for (auto word : body.materialWords) u32(out, word);
      index(out, int(body.stageStrings.size()));
      for (const auto& stage : body.stageStrings) {
        string(out, stage.first);
        index(out, int(stage.second.size()));
        for (const auto& value : stage.second) string(out, value);
      }
      string(out, body.shader);
      u32(out, body.materialVersion);
    }
    if (materialResource(body.kind)) {
      require(body.mips.empty(), "Material cannot contain texture mipmaps");
      e.payload = std::move(out); return;
    }
    int format = 0;
    for (const auto& tag : body.tags)
      if (lower(tag.name) == "format") {
        require(tag.kind == 1 && tag.value.size() == 1, "Invalid texture format property");
        format = tag.value[0];
      }
    require(body.mips.size() <= 32, "Excessive texture mipmaps");
    index(out, int(body.mips.size()));
    for (const auto& mip : body.mips) {
      bool emptyTerminal = p.licensee == 40 &&
          ((p.version == 133 && (format == 7 || format == 5)) || (p.version == 132 && format == 5)) &&
          body.mips.size() > 1 && &mip == &body.mips.back() && mip.pixels.empty() &&
          mip.width == 1 && mip.height == 1 && mip.widthBits == 0 && mip.heightBits == 0;
      require(mip.widthBits <= 16 && mip.heightBits <= 16 &&
                  (mip.pixels.size() == mipSize(format, mip.width, mip.height) || emptyTerminal || proceduralMip(body, mip, format, body.mips.size())), "Invalid mip storage");
      auto word = out.size();
      u32(out, 0);
      index(out, int(mip.pixels.size()));
      append(out, mip.pixels);
      e.lazyOffsets.push_back({word, out.size()});
      u32(out, mip.width);
      u32(out, mip.height);
      out.push_back(mip.widthBits);
      out.push_back(mip.heightBits);
    }
  } else
    throw std::runtime_error("Unsupported resource encoder class");
  e.payload = std::move(out);
}
}  // namespace openucc
