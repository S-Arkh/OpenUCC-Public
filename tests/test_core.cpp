#include <iostream>
#include <stdexcept>

#include "openucc/compiler.hpp"
#include "openucc/ir.hpp"
#include "openucc/target.hpp"
#include "openucc/resources.hpp"
using namespace openucc;
void check(bool b, const char* text) {
  if (!b) throw std::runtime_error(text);
}
template <class F>
void rejectsAt(int line, F f) {
  bool failed = false;
  try {
    f();
  } catch (const std::exception&) {
    failed = true;
  }
  if (!failed)
    throw std::runtime_error("Invalid input accepted at test line " +
                             std::to_string(line));
}
#define rejects(...) rejectsAt(__LINE__, __VA_ARGS__)
int main() {
  try {
    auto ints = Type::array(Type::Int);
    auto fixedInts = Type::array(Type::Int, 3);
    check(ints != Type::Int && ints != fixedInts &&
              ints != Type::array(Type::Float),
          "Array shape and element identity");
    check(fixedInts.dimension() == 3 && fixedInts.element() == Type::Int &&
              ints.isDynamicArray(),
          "Unified array descriptor");
    auto objectType = Type::named(Type::Object, "Engine.Actor");
    check(objectType == Type::named(Type::Object, "engine.actor") &&
              objectType != Type::named(Type::Class, "Engine.Actor") &&
              objectType != Type::named(Type::Object, "Other.Actor"),
          "Qualified type identity includes kind and package");
    rejects([&] { Type::array(ints); });
    rejects([&] { Type::array(Type::Void); });
    for (auto n :
         {INT32_MIN, -8192, -64, -1, 0, 1, 63, 64, 8191, 8192, INT32_MAX}) {
      Bytes b;
      index(b, n);
      Reader r{b};
      check(r.index() == n && r.pos == b.size(), "Compact index boundary");
    }
    for (auto b : {Bytes{0x40}, Bytes{0x40, 0x80, 0x80, 0x80, 0x20},
                   Bytes{0x40, 0xff, 0xff, 0xff, 0x1f}})
      rejects([&] {
        Reader r{b};
        r.index();
      });
    Package core;
    core.version = 133;
    core.licensee = 40;
    core.header.resize(56);
    core.names.push_back({"Object", 0, {}});
    core.names.push_back({"None", 0, {}});
    Bytes objectMetadata;
    for (int i = 0; i < 6; ++i) index(objectMetadata, 0);
    u32(objectMetadata, 0xffffffff);
    u32(objectMetadata, 0xffffffff);
    u32(objectMetadata, 0);
    objectMetadata.insert(objectMetadata.end(), 22, 0);
    u32(objectMetadata, 0x12);
    objectMetadata.insert(objectMetadata.end(), 16, 0);
    index(objectMetadata, 0);
    index(objectMetadata, 0);
    index(objectMetadata, 0);
    index(objectMetadata, 1);
    index(objectMetadata, 0);
    index(objectMetadata, 1);
    core.exports.push_back({0, 0, 0, 0, 0, objectMetadata});
    auto build = [&](const std::string& s) {
      return compile({{"Probe.uc", Bytes(s.begin(), s.end())}}, core);
    };
    auto p = build(
        "class Probe extends Object; var int Count; var bool Active; function "
        "int Choose(bool b, int x) { local int v; v=x; if(b) { return v; } "
        "else { return 17; } } function Reset() { Count=0; Active=false; } "
        "defaultproperties { Count=42 Active=true }");
    auto bytes = p.serialize();
    auto q = Package::parse(bytes);
    check(q.exports.size() == p.exports.size(), "Export count");
    auto collidingCore = core;
    collidingCore.exports.push_back({1, 0, 0, 0, 0, {}});
    // UE2 permits identical object paths with different classes (e.g.
    // class/texture).
    TargetModel model;
    model.add("Core", collidingCore);
    model.add("ProbePkg", q);
    auto probe = model.findClass("ProbePkg.Probe");
    check(bool(probe), "Qualified class resolution");
    check(model.ancestry(*probe).size() == 2, "Cross-package inheritance");
    auto choose = model.member(*probe, "choose");
    check(bool(choose), "Case-insensitive member lookup");
    auto signature =
        functionInfo(model.packages[choose->package].data, choose->id);
    check(signature.parameters.size() == 2 && signature.result &&
              signature.result->kind == "IntProperty",
          "Full bytecode/signature read");
    auto cyclic = q;
    cyclic.exports[0].super = 1;
    TargetModel cycle;
    cycle.add("Cycle", cyclic);
    rejects([&] { cycle.ancestry({0, 1}); });
    TargetModel incomplete;
    incomplete.add("ProbePkg", q);
    rejects([&] { incomplete.ancestry({0, 1}); });
    model.add("OtherPkg", q);
    rejects([&] { model.findClass("Probe"); });
    check(model.findClass("Probe", 2)->package == 2,
          "Preferred package resolution");
    for (std::size_t i = 0; i < p.exports.size(); ++i) {
      check(q.exports[i].payload == p.exports[i].payload, "Payload changed");
      check(q.ref(int(i) + 1) == p.ref(int(i) + 1), "Identity changed");
    }
    check(bytes == q.serialize(), "Writer is not stable");
    auto scalar = build(
        "class Scalar extends Object; var byte B; var float F; var string S; "
        "var name N; function string Empty(){return \"\";} function float "
        "Convert(int V){return V;} defaultproperties {B=255 F=1.25 "
        "S=\"\xD0\x9F\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82\" "
        "N='ProbeName'}");
    auto scalarRead = Package::parse(scalar.serialize());
    check(scalarRead.exports.size() == scalar.exports.size(),
          "Scalar metadata roundtrip");
    for (const std::string text : {"ASCII", u8"Привет 🚀"}) {
      Bytes encoded;
      string(encoded, text);
      Reader r{encoded};
      check(r.string() == text, "Unicode serialized string roundtrip");
    }
    rejects([&] {
      build("class P extends Object; var byte B; defaultproperties {B=256}");
    });
    rejects([&] {
      build(
          "class P extends Object; function string F(){return "
          "\"unterminated;}");
    });
    rejects([&] {
      build("class P extends Object; function float F(){return 1e999;}");
    });
    auto forward = build(
        "class P extends Object; function int Call() { local int X; Set(X); "
        "return Later(, X); } function Set(out int X) { X=7; } function int "
        "Later(optional int A, optional int B) { return B; }");
    rejects([&] {
      build(
          "class A extends Object; function F(){switch(1){case 1: continue;}}");
    });
    rejects([&] {
      build(
          "class A extends Object; function F(){switch(1){default: break; "
          "default: break;}}");
    });
    rejects([&] {
      build(
          "class A extends Object; function F(){switch(1){case \"wrong\": "
          "break;}}");
    });
    rejects([&] { build("class A extends Object; function F(){break;}"); });
    check(Package::parse(forward.serialize()).exports.size() ==
              forward.exports.size(),
          "Forward calls and parameter exports");
    auto parent = build(
        "class Parent extends Object; var int Value; function int ReadProbe() "
        "{ return Value; } function int DispatchProbe() { return ReadProbe(); "
        "} defaultproperties { Value=4 }");
    TargetModel external;
    external.add("Core", core);
    external.add("ParentPkg", parent);
    auto inherit = [&](const std::string& s) {
      return compile({{"Child.uc", Bytes(s.begin(), s.end())}}, external);
    };
    auto child = inherit(
        "class Child extends ParentPkg.Parent; function int ReadProbe() { "
        "return 7; } function int CallProbe() { Value=9; return "
        "DispatchProbe(); } defaultproperties { Value=5 }");
    check(child.ref(child.exports[0].super) == "ParentPkg.Parent",
          "External superclass import");
    bool overrideFound = false;
    for (auto& e : child.exports)
      if (child.name(e.name) == "ReadProbe") {
        check(child.ref(e.super) == "ParentPkg.Parent.ReadProbe",
              "Override function super import");
        overrideFound = true;
      }
    check(overrideFound, "Override export");
    rejects([&] {
      inherit(
          "class Child extends ParentPkg.Parent; function bool ReadProbe() { "
          "return true; }");
    });
    inherit("class Child extends ParentPkg.Parent; var int Value;");
    auto propertyNameFunction = inherit(
        "class Child extends ParentPkg.Parent; function int Value(){return "
        "1;}");
    for (const auto& entry : propertyNameFunction.exports)
      if (propertyNameFunction.name(entry.name) == "Value")
        check(entry.super == 0,
              "Inherited property is not a function override target");
    auto localBuild = [&](const std::vector<std::string>& texts) {
      std::vector<std::pair<std::string, Bytes>> sources;
      for (auto& text : texts)
        sources.push_back({"Local.uc", Bytes(text.begin(), text.end())});
      return compile(sources, external);
    };
    auto bitDefaults = localBuild(
        {"class Bits extends Object; struct Vector {var float X,Y,Z;}; "
         "var float F; var array<float> A; var Vector V; "
         "defaultproperties { F=__FloatBits(\"FFFFFFFF\") "
         "A(0)=__FloatBits(\"7F800001\") A(1)=__FloatBits(\"FF800000\") "
         "V=(X=__FloatBits(\"80000000\"),Y=__FloatBits(\"7F800000\"),Z=1.0) "
         "}"});
    auto bitTags = classDefaults(bitDefaults, 1);
    check(bitTags.size() == 3, "Missing raw float defaults");
    for (const auto& tag : bitTags) {
      Reader r{tag.data};
      if (tag.name == "F")
        check(r.u32() == 0xffffffffu, "NaN payload changed");
      else if (tag.name == "A")
        check(
            r.index() == 2 && r.u32() == 0x7f800001u && r.u32() == 0xff800000u,
            "Array signaling NaN/infinity bits changed");
      else if (tag.name == "V") {
        auto fields = defaultTags(bitDefaults, r);
        check(fields.size() == 3, "Raw float struct fields missing");
        for (const auto& field : fields) {
          Reader value{field.data};
          auto expected = lower(field.name) == "x"   ? 0x80000000u
                          : lower(field.name) == "y" ? 0x7f800000u
                                                     : 0x3f800000u;
          check(value.u32() == expected, "Struct raw float bits changed");
        }
      }
      check(r.pos == r.data.size(), "Raw float default not fully checked");
    }
    for (const auto& value :
         {"__FloatBits(\"FFF\")", "__FloatBits(\"GGFFFFFF\")",
          "__FloatBits(12345678)"})
      rejects([&] {
        localBuild({std::string("class Bits extends Object; var float F; "
                                "defaultproperties {F=") +
                    value + "}"});
      });
    rejects([&] {
      localBuild(
          {"class Bits extends Object; var int I; defaultproperties "
           "{I=__FloatBits(\"FFFFFFFF\")}"});
    });
    {
      auto types =
          localBuild({"class AssetValue extends Object; var float F; var "
                      "AssetValue Next; var array<AssetValue> Items;"});
      TargetModel assets = external;
      assets.add("AssetTypes", types);
      Package reference;
      reference.version = 133;
      reference.licensee = 40;
      reference.header.resize(64);
      reference.intern("None");
      TargetImports im(assets, reference);
      int type = im.symbol(*assets.findClass("AssetTypes.AssetValue"));
      reference.exports.push_back(
          {type, 0, 0, reference.intern("Root"), 0xe0004, {0}});
      reference.exports.push_back(
          {type, 0, 1, reference.intern("Child"), 0x60000, {0}});
      reference.exports.push_back(
          {type, 0, 1, reference.intern("Peer"), 0x60000, {0}});
      std::string child =
          "object Class=AssetTypes.AssetValue Name=\"Child\" Outer=\"Root\" "
          "{F=__FloatBits(\"FFFFFFFF\") Next=AssetValue'Root.Peer'}";
      std::string root =
          "object Class=AssetTypes.AssetValue Name=\"Root\" Outer=\"None\" "
          "{Items(0)=AssetValue'Root.Child' Items(1)=AssetValue'Root.Peer'}";
      std::string peer =
          "object Class=AssetTypes.AssetValue Name=\"Peer\" Outer=\"Root\" "
          "{Next=AssetValue'Root'}";
      auto buildAssets = [&](const std::string& text, const Package& ref) {
        auto model = assets;
        model.addReference("Assets", ref);
        return compileObjects(
            {{"Assets.uobj", Bytes(text.begin(), text.end())}}, model,
            "Assets");
      };
      auto result = buildAssets(child + root + peer, reference);
      check(result.exports.size() == 3 && result.ref(1) == "Root.Child" &&
                result.exports[0].outer == 2 &&
                result.exports[0].flags == 0x60000 &&
                result.exports[1].outer == 0 &&
                result.exports[1].flags == 0xe0004,
            "Package object scopes or flags changed");
      Reader childBytes{result.exports[0].payload};
      auto tags = defaultTags(result, childBytes);
      check(childBytes.pos == childBytes.data.size() && tags.size() == 2,
            "Object properties incomplete");
      for (const auto& tag : tags) {
        Reader value{tag.data};
        if (tag.name == "F")
          check(value.u32() == 0xffffffffu, "Package object NaN bits changed");
        else
          check(tag.name == "Next" && value.index() == 3,
                "Forward object reference changed");
      }
      Reader rootBytes{result.exports[1].payload};
      auto arrays = defaultTags(result, rootBytes);
      check(arrays.size() == 1, "Root object array missing");
      Reader elements{arrays[0].data};
      check(elements.index() == 2 && elements.index() == 1 &&
                elements.index() == 3,
            "Object array links changed");
      rejects([&] { buildAssets(child + root, reference); });
      rejects([&] { buildAssets(child + root + peer + child, reference); });
      auto tail = reference;
      tail.exports[0].payload.push_back(0);
      rejects([&] { buildAssets(child + root + peer, tail); });
      auto badFlags = reference;
      badFlags.exports[0].flags |= 0x02000000;
      rejects([&] { buildAssets(child + root + peer, badFlags); });
      rejects([&] {
        compileObjects({{"Assets.uobj", Bytes(child.begin(), child.end())}},
                       assets, "Assets");
      });
    }
    const std::vector<std::string> objectSources = {
        "class Value extends Object; var int Count; var Value Next; "
        "defaultproperties {Count=3}",
        "class Owner extends Object; var Value Child; defaultproperties {"
        "Begin Object Class=Value Name=First Count=7 Next=Value'Owner.Second' "
        "End Object "
        "Begin Object Class=Value Name=Second Count=9 End Object "
        "Child=Value'First'}"};
    auto objectDefaults = localBuild(objectSources);
    int firstObject = 0, secondObject = 0, ownerClass = 0;
    for (std::size_t i = 0; i < objectDefaults.exports.size(); ++i) {
      auto id = int(i) + 1;
      auto path = objectDefaults.ref(id);
      if (path == "Owner.First") firstObject = id;
      if (path == "Owner.Second") secondObject = id;
      if (path == "Owner") ownerClass = id;
    }
    check(firstObject && secondObject && ownerClass,
          "Default subobject exports missing");
    check(objectDefaults.kind(firstObject) == "Value" &&
              objectDefaults.exports[firstObject - 1].outer == ownerClass &&
              objectDefaults.exports[firstObject - 1].flags == 0x70004,
          "Default subobject identity/header differs from SDK");
    Reader firstDefaults{objectDefaults.exports[firstObject - 1].payload};
    check(objectDefaults.name(firstDefaults.index()) == "Count" &&
              firstDefaults.byte() == 0x22 && firstDefaults.i32() == 7,
          "Subobject scalar override not serialized");
    check(objectDefaults.name(firstDefaults.index()) == "Next" &&
              firstDefaults.byte() == 5 &&
              firstDefaults.index() == secondObject &&
              objectDefaults.name(firstDefaults.index()) == "None" &&
              firstDefaults.pos == firstDefaults.data.size(),
          "Subobject forward reference or property terminator incorrect");
    auto ownerDefaults = classDefaults(objectDefaults, ownerClass);
    check(ownerDefaults.size() == 1, "Unexpected owner default count");
    Reader childDefault{ownerDefaults[0].data};
    check(childDefault.index() == firstObject,
          "Unqualified default subobject reference lost");
    auto objectReference = objectDefaults;
    objectReference.exports[firstObject - 1].flags &= ~4u;
    TargetModel objectTarget = external;
    objectTarget.addReference("Probe", objectReference);
    std::vector<std::pair<std::string, Bytes>> objectInputs;
    for (const auto& text : objectSources)
      objectInputs.push_back({"Object.uc", Bytes(text.begin(), text.end())});
    auto referencedObjects = compile(objectInputs, objectTarget, "Probe");
    bool checkedObjectVisibility = false;
    for (std::size_t i = 0; i < referencedObjects.exports.size(); ++i)
      if (referencedObjects.ref(int(i) + 1) == "Owner.First") {
        check(!(referencedObjects.exports[i].flags & 4),
              "Attested object visibility lost");
        checkedObjectVisibility = true;
      }
    check(checkedObjectVisibility,
          "Missing referenced object visibility fixture");
    rejects([&] {
      localBuild(
          {"class A extends Object; defaultproperties {Begin Object "
           "Class=Missing Name=X End Object}"});
    });
    rejects([&] {
      localBuild(
          {"class A extends Object; defaultproperties {Begin Object Class=A "
           "Name=X End Object Begin Object Class=A Name=X End Object}"});
    });
    rejects([&] {
      localBuild(
          {"class A extends Object; defaultproperties {Begin Object Class=A "
           "Name=X Unknown=1 End Object}"});
    });
    rejects([&] {
      localBuild(
          {"class A extends Object; defaultproperties {Begin Object Class=A "
           "Name=X Begin Object Class=A Name=Y End Object End Object}"});
    });
    TargetModel bootstrap;
    {
      auto legacyCore = core;
      legacyCore.version = 123;
      legacyCore.licensee = 37;
      auto reference = build("class MixedOwner extends Object; var int Value;");
      reference.version = 126;
      reference.licensee = 0;
      TargetModel mixed;
      mixed.add("Core", legacyCore);
      mixed.addReference("MixedPkg", reference);
      std::string source = "class MixedOwner extends Object; var int Value;";
      auto result = Package::parse(
          compile({{"MixedOwner.uc", Bytes(source.begin(), source.end())}},
                  mixed, "mixedpkg")
              .serialize());
      check(result.version == 126 && result.licensee == 0,
            "Self-reference profile lost to Core dependency");
      auto unrelated =
          compile({{"MixedOwner.uc", Bytes(source.begin(), source.end())}},
                  mixed, "OtherPkg");
      check(unrelated.version == 123 && unrelated.licensee == 37,
            "Unrelated reference changed output profile");
    }
    auto nativeReference = build("class NativeOwner extends Object;");
    TargetModel coreOnly;
    coreOnly.add("Core", core);
    TargetImports nativeImports(coreOnly, nativeReference);
    nativeImports.nativeClass("Core", "Language");
    nativeImports.nativeClass("Core", "TextBuffer");
    nativeImports.nativeClass("Core", "Package");
    nativeImports.nativeClass("Core", "UnknownNativeClass");
    nativeImports.nativeClass("NativePkg", "LocalNative");
    TargetModel nativeTarget;
    nativeTarget.add("Core", core);
    nativeTarget.addReference("NativePkg", nativeReference);
    auto language = nativeTarget.findClass("Language");
    check(language && nativeTarget.isIntrinsicClass(*language) &&
              nativeTarget.identity(*language) == "Core.Language",
          "Explicit reference resolves attested native Core.Language");
    check(!nativeTarget.findClass("UnknownNativeClass"),
          "Unattested external native import remains unresolved");
    auto proxyPackage = build("class Proxy extends Object;");
    TargetImports proxyImports(nativeTarget, proxyPackage);
    int proxyNative = proxyImports.nativeClass("NativePkg", "LocalNative");
    int proxyMissing = proxyImports.nativeClass("NativePkg", "MissingNative");
    auto proxyIndex = nativeTarget.packages.size();
    nativeTarget.add("ProxyPkg", proxyPackage);
    check(
        nativeTarget.isIntrinsicClass({proxyIndex, proxyNative}),
        "External resource native class requires defining-module attestation");
    check(!nativeTarget.isIntrinsicClass({proxyIndex, proxyMissing}),
          "Missing defining-module native class must remain unresolved");
    std::string nativeSource =
        "class NativeOwner extends Object; var class<Language> LanguageClass;";
    auto nativeResult = compile(
        {{"NativeOwner.uc", Bytes(nativeSource.begin(), nativeSource.end())}},
        nativeTarget, "NativePkg");
    bool nativeBound = false;
    for (std::size_t i = 0; i < nativeResult.exports.size(); ++i)
      if (nativeResult.ref(int(i) + 1) == "NativeOwner.LanguageClass")
        nativeBound = nativeResult.ref(
                          propertyInfo(nativeResult, int(i) + 1).metaclass) ==
                      "Core.Language";
    check(nativeBound,
          "Native metaclass import preserves its original package");
    std::string nativeObjects =
        "class NativeOwner extends Object; var TextBuffer Buffer; "
        "var Package Container; function Object GetBuffer(){return Buffer;} "
        "function Object GetPackage(){return Container;}";
    auto nativeObjectResult = compile(
        {{"NativeOwner.uc", Bytes(nativeObjects.begin(), nativeObjects.end())}},
        nativeTarget, "NativePkg");
    int checkedNativeObjects = 0;
    for (std::size_t i = 0; i < nativeObjectResult.exports.size(); ++i) {
      auto path = nativeObjectResult.ref(int(i) + 1);
      if (path != "NativeOwner.Buffer" && path != "NativeOwner.Container")
        continue;
      check(nativeObjectResult.ref(
                propertyInfo(nativeObjectResult, int(i) + 1).reference) ==
                (path == "NativeOwner.Buffer" ? "Core.TextBuffer"
                                              : "Core.Package"),
            "Native Core property changes its attested identity");
      ++checkedNativeObjects;
    }
    check(checkedNativeObjects == 2, "Missing native Core object properties");
    auto meshReference = build("class MeshOwner extends Object;");
    TargetImports meshImports(coreOnly, meshReference);
    meshImports.nativeClass("Engine", "SkeletalMesh");
    meshImports.nativeClass("Engine", "MissingMesh");
    TargetModel meshTarget = coreOnly;
    meshTarget.addReference("MeshPkg", meshReference);
    check(!meshTarget.findClass("Engine.SkeletalMesh"),
          "Native mesh requires its defining module");
    meshTarget.add("Engine", build("class EngineProbe extends Object;"));
    check(bool(meshTarget.findClass("Engine.SkeletalMesh")),
          "SDK-attested skeletal mesh import not resolved");
    check(!meshTarget.findClass("Engine.MissingMesh"),
          "Unattested mesh class accepted");
    std::string meshSource =
        "class MeshOwner extends Object; function class<Object> "
        "GetType(){return class'Engine.SkeletalMesh';}";
    compile({{"MeshOwner.uc", Bytes(meshSource.begin(), meshSource.end())}},
            meshTarget, "MeshPkg");
    TargetModel resourceMeshTarget = coreOnly;
    auto nativeMeshModule = build("class EngineProbe extends Object;");
    TargetImports nativeMeshModuleImports(coreOnly, nativeMeshModule);
    nativeMeshModuleImports.nativeClass("Engine", "Mesh");
    resourceMeshTarget.add("Engine", nativeMeshModule);
    resourceMeshTarget.add("Effects", meshReference);
    check(!resourceMeshTarget.findClass("SkeletalMesh"),
          "Dependency native import requires explicit reference attestation");
    auto resourceMeshReference = build("class MeshOwner extends Object;");
    resourceMeshReference.imports.push_back(
        {resourceMeshReference.intern("Core"),
         resourceMeshReference.intern("Package"), 0,
         resourceMeshReference.intern("Meshes")});
    int meshRoot = -int(resourceMeshReference.imports.size());
    resourceMeshReference.imports.push_back(
        {resourceMeshReference.intern("Engine"),
         resourceMeshReference.intern("SkeletalMesh"), meshRoot,
         resourceMeshReference.intern("Shape")});
    resourceMeshTarget.addReference("MeshPkg", resourceMeshReference);
    check(bool(resourceMeshTarget.findClass("SkeletalMesh")) &&
              !resourceMeshTarget.findClass("MissingMesh"),
          "Typed resource attestation must only resolve the verified native "
          "class");
    std::string resourceMeshSource =
        "class MeshOwner extends Object; var Mesh Shape; var SkeletalMesh "
        "Asset; "
        "function Mesh GetMesh(){return Asset;} "
        "function SkeletalMesh CastMesh(Mesh Value){return "
        "SkeletalMesh(Value);} "
        "defaultproperties {Shape=SkeletalMesh'Meshes.Shape'}";
    compile({{"MeshOwner.uc",
              Bytes(resourceMeshSource.begin(), resourceMeshSource.end())}},
            resourceMeshTarget, "MeshPkg");
    auto externalObjectReference =
        build("class AssetOwner extends Object; var Object Item;");
    auto addObjectImport = [&](const std::string& kind, int outer,
                               const std::string& name) {
      externalObjectReference.imports.push_back(
          {externalObjectReference.intern("Core"),
           externalObjectReference.intern(kind), outer,
           externalObjectReference.intern(name)});
      return -int(externalObjectReference.imports.size());
    };
    auto assetRoot = addObjectImport("Package", 0, "Assets");
    auto assetGroup = addObjectImport("Package", assetRoot, "Group");
    addObjectImport("Object", assetGroup, "Item");
    TargetModel externalObjectTarget = external;
    externalObjectTarget.addReference("Probe", externalObjectReference);
    auto compileAssetLiteral = [&](const std::string& path,
                                   const TargetModel& target) {
      std::string text =
          "class AssetOwner extends Object; var Object Item; defaultproperties "
          "{Item=Object'" +
          path + "'}";
      return compile({{"AssetOwner.uc", Bytes(text.begin(), text.end())}},
                     target, "Probe");
    };
    auto assetLiteral =
        compileAssetLiteral("Assets.Group.Item", externalObjectTarget);
    auto importedAssetDefaults = classDefaults(assetLiteral, 1);
    check(importedAssetDefaults.size() == 1,
          "Missing external resource default");
    Reader assetValue{importedAssetDefaults[0].data};
    check(assetLiteral.ref(assetValue.index()) == "Assets.Group.Item",
          "Explicit resource import loses its package-group path");
    rejects([&] {
      compileAssetLiteral("Assets.Group.Missing", externalObjectTarget);
    });
    rejects([&] { compileAssetLiteral("Assets.Group.Item", external); });
    externalObjectTarget.add("Assets", build("class Other extends Object;"));
    rejects([&] {
      compileAssetLiteral("Assets.Group.Item", externalObjectTarget);
    });
    std::string paletteSource = "class Palette extends Object;";
    std::string assetOwner = "class AssetOwner extends Object; var Palette Image;";
    auto resourceReference = compile(
        {{"Palette.uc", Bytes(paletteSource.begin(), paletteSource.end())},
         {"AssetOwner.uc", Bytes(assetOwner.begin(), assetOwner.end())}},
        external, "AssetPkg");
    int paletteType = 0;
    for (std::size_t i = 0; i < resourceReference.exports.size(); ++i)
      if (resourceReference.ref(int(i) + 1) == "Palette") paletteType = int(i) + 1;
    Export paletteObject;
    paletteObject.type = paletteType;
    paletteObject.name = resourceReference.intern("Palette");
    ResourceBody paletteBody; paletteBody.kind = "Palette";
    paletteBody.colors.resize(256); paletteBody.colors[17] = {1, 2, 3, 4};
    encodeResource(paletteBody, resourceReference, paletteObject,
                   [](int id) { return id; });
    resourceReference.exports.push_back(paletteObject);
    TargetModel reconstructedResourceTarget;
    reconstructedResourceTarget.add("Core", core);
    reconstructedResourceTarget.addReference("AssetPkg", resourceReference);
    assetOwner += " defaultproperties {Image=Palette'Palette'}";
    auto resourceResult = compile(
        {{"Palette.uc", Bytes(paletteSource.begin(), paletteSource.end())},
         {"AssetOwner.uc", Bytes(assetOwner.begin(), assetOwner.end())}},
        reconstructedResourceTarget, "AssetPkg");
    int resourceId = 0, resourceOwner = 0;
    for (std::size_t i = 0; i < resourceResult.exports.size(); ++i) {
      int id = int(i) + 1;
      if (resourceResult.kind(id) == "Palette") resourceId = id;
      if (resourceResult.ref(id) == "AssetOwner") resourceOwner = id;
    }
    check(resourceId && resourceOwner &&
              decodeResource(resourceResult, resourceId).colors[17] ==
                  paletteBody.colors[17],
          "Source class and reconstructed asset can share a path");
    auto assetDefaults = classDefaults(resourceResult, resourceOwner);
    check(assetDefaults.size() == 1, "Missing reconstructed asset default");
    Reader assetDefault{assetDefaults[0].data};
    check(assetDefault.index() == resourceId,
          "Self-package object default binds the reconstructed resource");
    auto fontReference = build("class FontOwner extends Object;");
    TargetImports fontImports(coreOnly, fontReference);
    Export fontObject;
    fontObject.type = fontImports.nativeClass("FontPkg", "Font");
    fontObject.name = fontReference.intern("DefaultFont");
    ResourceBody fontBody;
    fontBody.kind = "Font";
    encodeResource(fontBody, fontReference, fontObject,
                   [](int id) { return id; });
    fontReference.exports.push_back(fontObject);
    TargetModel fontTarget;
    fontTarget.add("Core", core);
    fontTarget.addReference("FontPkg", fontReference);
    std::string fontSource =
        "class FontOwner extends Object; var Font Face; "
        "defaultproperties {Face=Font'DefaultFont'}";
    auto fontResult = compile(
        {{"FontOwner.uc", Bytes(fontSource.begin(), fontSource.end())}},
        fontTarget, "FontPkg");
    auto fontDefaults = classDefaults(fontResult, 1);
    check(fontDefaults.size() == 1, "Intrinsic font default missing");
    Reader fontDefault{fontDefaults[0].data};
    check(fontResult.kind(fontDefault.index()) == "Font",
          "Intrinsic class literal binds the reconstructed font");
    auto overrideReference =
        localBuild({"class Base extends Object; function F(out int Value);",
                    "class Child extends Base; function F(out int Value);"});
    for (std::size_t i = 0; i < overrideReference.exports.size(); ++i)
      if (overrideReference.ref(int(i) + 1) == "Child.F.Value") {
        auto& bytes = overrideReference.exports[i].payload;
        Reader reader{bytes};
        reader.index();
        reader.index();
        reader.index();
        reader.i32();
        bytes[reader.pos + 1] &= ~1u;
      }
    TargetModel overrideTarget;
    overrideTarget.add("Core", core);
    overrideTarget.addReference("Probe", overrideReference);
    auto referenceOverride = [&](const std::string& parameter,
                                 const TargetModel& target) {
      std::string base =
          "class Base extends Object; function F(out int Value);";
      std::string child =
          "class Child extends Base; function F(" + parameter + " Value);";
      return compile({{"Base.uc", Bytes(base.begin(), base.end())},
                      {"Child.uc", Bytes(child.begin(), child.end())}},
                     target, "Probe");
    };
    auto verifiedOverride = referenceOverride("int", overrideTarget);
    bool linkedOverride = false;
    for (std::size_t i = 0; i < verifiedOverride.exports.size(); ++i)
      if (verifiedOverride.ref(int(i) + 1) == "Child.F")
        linkedOverride =
            verifiedOverride.ref(verifiedOverride.exports[i].super) == "Base.F";
    check(linkedOverride, "Reference-attested out override retains super");
    rejects([&] { referenceOverride("int", external); });
    rejects([&] { referenceOverride("optional int", overrideTarget); });
    rejects([&] { referenceOverride("float", overrideTarget); });
    std::string externalBaseText =
        "class ExternalBase extends Object; function F(out int Value);";
    auto externalBase =
        compile({{"ExternalBase.uc",
                  Bytes(externalBaseText.begin(), externalBaseText.end())}},
                coreOnly, "BasePkg");
    TargetModel externalOverrideTarget;
    externalOverrideTarget.add("Core", core);
    externalOverrideTarget.add("BasePkg", externalBase);
    auto externalChild = [&](const std::string& parameter,
                             const TargetModel& target) {
      std::string text = "class Child extends ExternalBase; function F(" +
                         parameter + " Value);";
      return compile({{"Child.uc", Bytes(text.begin(), text.end())}}, target,
                     "ChildPkg");
    };
    auto externalOverrideReference =
        externalChild("out int", externalOverrideTarget);
    for (std::size_t i = 0; i < externalOverrideReference.exports.size(); ++i)
      if (externalOverrideReference.ref(int(i) + 1) == "Child.F.Value") {
        auto& bytes = externalOverrideReference.exports[i].payload;
        Reader r{bytes};
        r.index();
        r.index();
        r.index();
        r.u32();
        bytes[r.pos + 1] &= ~1u;
      }
    rejects([&] { externalChild("int", externalOverrideTarget); });
    externalOverrideTarget.addReference("ChildPkg", externalOverrideReference);
    auto externalOverride = externalChild("int", externalOverrideTarget);
    bool checkedExternalOverride = false;
    for (std::size_t i = 0; i < externalOverride.exports.size(); ++i)
      if (externalOverride.ref(int(i) + 1) == "Child.F") {
        check(externalOverride.ref(externalOverride.exports[i].super) ==
                  "BasePkg.ExternalBase.F",
              "Reference-attested external override loses its super");
        checkedExternalOverride = true;
      }
    check(checkedExternalOverride, "Missing external override fixture");
    rejects([&] { externalChild("float", externalOverrideTarget); });
    bootstrap.addReference("Core", core);
    auto rebuildCore = [&](const std::vector<std::string>& texts) {
      std::vector<std::pair<std::string, Bytes>> sources;
      for (auto& text : texts)
        sources.push_back({"Bootstrap.uc", Bytes(text.begin(), text.end())});
      return compile(sources, bootstrap, "Core");
    };
    auto rootPackage = rebuildCore(
        {"class Object; struct Vector {var() config float X; var float Y; "
         "var float Z;}; var Core.Object Peer; var Core.Object.Vector Origin;",
         "class Child extends Core.Object; function Object GetSelf(){return "
         "self;}"});
    check(rootPackage.exports[0].super == 0 &&
              rootPackage.ref(rootPackage.exports[0].super) == "None",
          "Root Object has no superclass");
    bool localRootProperty = false, localRootParent = false;
    for (std::size_t i = 0; i < rootPackage.exports.size(); ++i) {
      int id = int(i) + 1;
      if (rootPackage.ref(id) == "Object.Peer")
        localRootProperty = propertyInfo(rootPackage, id).reference == 1;
      if (rootPackage.ref(id) == "Child")
        localRootParent = rootPackage.exports[i].super == 1;
      if (rootPackage.ref(id) == "Object.Vector.X") {
        auto field = propertyInfo(rootPackage, id);
        Reader raw{rootPackage.exports[i].payload};
        raw.index();
        raw.index();
        raw.index();
        raw.i32();
        raw.u32();
        check(
            field.flags == 0x4001 && rootPackage.name(raw.index()) == "Vector",
            "Root intrinsic struct preserves config/category metadata");
      }
    }
    check(localRootProperty && localRootParent,
          "Qualified self-package references bind local exports");
    check((rootPackage.exports[0].flags & 0xf0000) ==
              (core.exports[0].flags & 0xf0000),
          "Reference load-context flags are preserved");
    auto inlineEnum = rebuildCore(
        {"class Object; struct Scale {var enum EAxis {AX_None, AX_X} Axis;}; "
         "var editconst name Label;"});
    bool nestedEnum = false, editConstant = false;
    for (std::size_t i = 0; i < inlineEnum.exports.size(); ++i) {
      int id = int(i) + 1;
      if (inlineEnum.ref(id) == "Object.Scale.Axis")
        nestedEnum = inlineEnum.ref(propertyInfo(inlineEnum, id).reference) ==
                     "Object.Scale.EAxis";
      if (inlineEnum.ref(id) == "Object.Label")
        editConstant = (propertyInfo(inlineEnum, id).flags & 0x20000) != 0;
    }
    check(nestedEnum && editConstant,
          "Struct-owned enum identity and editconst field metadata");
    TargetModel nestedEnumTarget;
    nestedEnumTarget.add("Core", inlineEnum);
    std::string nestedEnumSource =
        "class UsesScale extends Object; var Object.Scale S; "
        "defaultproperties {S=(Axis=AX_X)}";
    auto nestedEnumDefault = compile(
        {{"UsesScale.uc", Bytes(nestedEnumSource.begin(), nestedEnumSource.end())}},
        nestedEnumTarget, "EnumProbe");
    check(!classDefaults(nestedEnumDefault, 1).empty(),
          "Imported struct enum default resolves by the field type");
    for (std::size_t i = 0; i < inlineEnum.exports.size(); ++i)
      if (inlineEnum.ref(int(i) + 1) == "Object.Scale") {
        auto info = structInfo(inlineEnum, int(i) + 1);
        check(info.fields.size() == 1 && info.fields[0].kind == "ByteProperty",
              "Struct metadata excludes nested enum declarations from "
              "properties");
      }
    for (std::size_t i = 0; i < rootPackage.imports.size(); ++i)
      check(lower(rootPackage.ref(-int(i) - 1)) != "core.object",
            "Root Object must not import the original definition");
    rejects([&] { rebuildCore({"class Other;"}); });
    rejects([&] { build("class Object;"); });
    Package selfOutput;
    TargetImports selfImports(bootstrap, selfOutput, "Core");
    rejects([&] { selfImports.symbol(*bootstrap.findClass("Core.Object")); });
    selfImports.bindLocal("Object", 7);
    check(selfImports.symbol(*bootstrap.findClass("Core.Object")) == 7,
          "Explicit reference export binding");
    rejects([&] { selfImports.bindLocal("Object", 8); });
    auto nativeIterator = build(
        "class A extends Object; native(304) final iterator function "
        "AllObjects(class<Object> BaseClass, out Object Value);");
    check((functionInfo(nativeIterator, 2).flags & 0x405) == 0x405 &&
              functionInfo(nativeIterator, 2).native == 304,
          "Native iterator declaration metadata");
    auto latentFunction = build(
        "class A extends Object; native(256) final latent function Sleep(float T); "
        "state() Waiting {Begin: Sleep(1.0); stop;}");
    check((functionInfo(latentFunction, 2).flags & 0x409) == 0x409,
          "Native latent flag preserved");
    auto nativeReplication =
        build("class A extends Object native nativereplication;");
    check((classInfo(nativeReplication, 1).flags & 0x800) != 0,
          "Native replication class modifier preserved");
    check(
        (classInfo(build("class PlaceableObject extends Object placeable;"), 1)
             .flags &
         0x200) != 0,
        "SDK permits placeable on non-Actor classes");
    auto propertyModifiers = build(
        "class Flags extends Object; var input byte InputValue; "
        "var travel int TravelValue; var export Object ExportValue; "
        "var() editconstarray array<int> FixedItems; "
        "var() editinline Object InlineValue; "
        "var() editinlineuse Object InlineUseValue; "
        "var() editinlinenotify Object InlineNotifyValue; "
        "var() edfindable Object FindValue; var deprecated int OldValue; "
        "var() export editinline Object OwnedInline; "
        "var native const array<Object> NativeItems; var native string "
        "NativeText;");
    const std::map<std::string, std::uint32_t> expectedPropertyFlags = {
        {"InputValue", 4},
        {"TravelValue", 0x10000},
        {"ExportValue", 8},
        {"FixedItems", 0x400041},
        {"InlineValue", 0x4000001},
        {"InlineUseValue", 0x14000001},
        {"InlineNotifyValue", 0x44000001},
        {"FindValue", 0x8000001},
        {"OldValue", 0x20000000},
        {"OwnedInline", 0x4400009},
        {"NativeItems", 0x1002},
        {"NativeText", 0x1000}};
    std::size_t checkedPropertyFlags = 0;
    for (std::size_t i = 0; i < propertyModifiers.exports.size(); ++i) {
      const auto& entry = propertyModifiers.exports[i];
      auto expected =
          expectedPropertyFlags.find(propertyModifiers.name(entry.name));
      if (entry.outer == 1 && expected != expectedPropertyFlags.end()) {
        check(propertyInfo(propertyModifiers, int(i) + 1).flags ==
                  expected->second,
              "Property flags differ from isolated SDK evidence");
        ++checkedPropertyFlags;
      }
    }
    check(checkedPropertyFlags == expectedPropertyFlags.size(),
          "Missing property flag fixture");
    auto stateMasks = build(
        "class Masks extends Object; function Timer(){} function Bump(){} "
        "function Tick(float Delta){} "
        "state BaseState {ignores Bump; function Tick(float Delta){}} "
        "state DerivedState extends BaseState {function Timer(){}}");
    int stateMaskFunctions = 0, checkedStateMasks = 0;
    for (std::size_t i = 0; i < stateMasks.exports.size(); ++i) {
      auto id = int(i) + 1;
      if (stateMasks.kind(id) == "Function") ++stateMaskFunctions;
      if (stateMasks.kind(id) != "State") continue;
      auto state = stateInfo(stateMasks, id);
      auto derived = stateMasks.ref(id) == "Masks.DerivedState";
      check(state.probe ==
                    ((UINT64_C(1) << 36) | (derived ? UINT64_C(1) << 8 : 0)) &&
                state.ignore ==
                    (derived ? UINT64_MAX : UINT64_MAX ^ (UINT64_C(1) << 15)),
            "State probe inheritance and local ignore mask differ from SDK");
      ++checkedStateMasks;
    }
    check(checkedStateMasks == 2 && stateMaskFunctions == 5,
          "Ignored probe must not create a function export");
    auto stateNetwork = build(
        "class NetState extends Object; replication {reliable if(true) "
        "Message;} "
        "function Message(int Value){} state Test {function Message(int "
        "Value){}}");
    bool stateNetworkChecked = false;
    for (std::size_t i = 0; i < stateNetwork.exports.size(); ++i)
      if (stateNetwork.ref(int(i) + 1) == "NetState.Test.Message") {
        auto info = functionInfo(stateNetwork, int(i) + 1);
        check((info.flags & 0xc0) == 0xc0 && info.replicationOffset == 65535,
              "State override loses inherited replication contract");
        stateNetworkChecked = true;
      }
    check(stateNetworkChecked, "State network fixture missing");
    auto arrayParameters = build(
        "class ArrayParams extends Object; "
        "native function Send(byte Data[255]); "
        "native function Read(out byte Data[255]); "
        "event Received(byte Data[255]) { Send(Data); Read(Data); }");
    int checkedArrayParameters = 0;
    for (std::size_t i = 0; i < arrayParameters.exports.size(); ++i) {
      auto id = int(i) + 1;
      if (arrayParameters.kind(id) != "Function") continue;
      auto info = functionInfo(arrayParameters, id);
      check(info.parameters.size() == 1 &&
                info.parameters[0].kind == "ByteProperty" &&
                info.parameters[0].dimension == 255,
            "Static array parameter loses element type or dimension");
      ++checkedArrayParameters;
    }
    check(checkedArrayParameters == 3,
          "Missing static array parameter functions");
    rejects([&] {
      build(
          "class A extends Object; native function Read(out byte Data[255]); "
          "function F() { local byte Data[254]; Read(Data); }");
    });
    rejects([&] {
      build(
          "class A extends Object; native function Send(byte Data[255]); "
          "function F() { local int Data[255]; Send(Data); }");
    });
    auto compileConstructorReference = [&](const TargetModel& target,
                                           const std::string& extraType) {
      std::string value = "class Value extends Object instanced;";
      std::string owner =
          "class Owner extends Object; native function Get(out Value Result, " +
          extraType + " Count);";
      return compile({{"Value.uc", Bytes(value.begin(), value.end())},
                      {"Owner.uc", Bytes(owner.begin(), owner.end())}},
                     target, "CtorPkg");
    };
    auto constructorReference = compileConstructorReference(coreOnly, "int");
    for (std::size_t i = 0; i < constructorReference.exports.size(); ++i)
      if (constructorReference.ref(int(i) + 1) == "Owner.Get.Result") {
        check(propertyInfo(constructorReference, int(i) + 1).flags == 0x400180,
              "Instanced output constructor fixture changed");
        auto& bytes = constructorReference.exports[i].payload;
        Reader r{bytes};
        r.index();
        r.index();
        r.index();
        r.u32();
        bytes[r.pos + 2] &= ~0x40u;
      }
    TargetModel constructorTarget;
    constructorTarget.add("Core", core);
    constructorTarget.addReference("CtorPkg", constructorReference);
    auto constructorResult =
        compileConstructorReference(constructorTarget, "int");
    bool checkedConstructorReference = false;
    for (std::size_t i = 0; i < constructorResult.exports.size(); ++i)
      if (constructorResult.ref(int(i) + 1) == "Owner.Get.Result") {
        check(propertyInfo(constructorResult, int(i) + 1).flags == 0x180,
              "Native parameter loses attested constructor-link bit");
        checkedConstructorReference = true;
      }
    check(checkedConstructorReference, "Constructor reference fixture missing");
    rejects([&] { compileConstructorReference(constructorTarget, "float"); });
    auto compileInlineReference = [&](const TargetModel& target, int dimension) {
      std::string text = "class InlineOwner extends Object; "
                         "var export editinline Object Value[" +
                         std::to_string(dimension) + "];";
      return compile({{"InlineOwner.uc", Bytes(text.begin(), text.end())}},
                     target, "InlinePkg");
    };
    auto inlineReference = compileInlineReference(coreOnly, 2);
    int inlineId = 0;
    for (std::size_t i = 0; i < inlineReference.exports.size(); ++i)
      if (inlineReference.ref(int(i) + 1) == "InlineOwner.Value")
        inlineId = int(i) + 1;
    check(inlineId != 0, "Inline constructor fixture missing");
    check(propertyInfo(inlineReference, inlineId).flags == 0x4400008,
          "Ordinary inline constructor flag changed");
    auto& inlineBytes = inlineReference.exports[inlineId - 1].payload;
    Reader inlineReader{inlineBytes};
    inlineReader.index();
    inlineReader.index();
    inlineReader.index();
    inlineReader.u32();
    inlineBytes[inlineReader.pos + 2] &= ~0x40u;
    TargetModel inlineTarget;
    inlineTarget.add("Core", core);
    inlineTarget.addReference("InlinePkg", inlineReference);
    auto inlineResult = compileInlineReference(inlineTarget, 2);
    check(propertyInfo(inlineResult, inlineId).flags == 0x4000008,
          "Attested inline constructor-link bit lost");
    rejects([&] { compileInlineReference(inlineTarget, 3); });
    auto stateLinkSource = [](const std::string& type) {
      return "class LinkProbe extends Object; function F(" + type +
             " Value){} "
             "state S {function F(" +
             type + " Value){}}";
    };
    auto linkText = stateLinkSource("int");
    auto linkReference =
        compile({{"LinkProbe.uc", Bytes(linkText.begin(), linkText.end())}},
                coreOnly, "LinkPkg");
    int linkedStateFunction = 0;
    for (std::size_t i = 0; i < linkReference.exports.size(); ++i)
      if (linkReference.ref(int(i) + 1) == "LinkProbe.S.F")
        linkedStateFunction = int(i) + 1;
    check(linkedStateFunction != 0, "State link fixture missing");
    auto& stateFunctionExport = linkReference.exports[linkedStateFunction - 1];
    check(stateFunctionExport.super != 0,
          "Ordinary compilation keeps SDK state super");
    Reader linkReader{stateFunctionExport.payload};
    linkReader.index();
    auto superStart = linkReader.pos;
    linkReader.index();
    Bytes linkPayload(stateFunctionExport.payload.begin(),
                      stateFunctionExport.payload.begin() + superStart);
    index(linkPayload, 0);
    linkPayload.insert(linkPayload.end(),
                       stateFunctionExport.payload.begin() + linkReader.pos,
                       stateFunctionExport.payload.end());
    stateFunctionExport.payload = std::move(linkPayload);
    stateFunctionExport.super = 0;
    TargetModel stateLinkTarget;
    stateLinkTarget.add("Core", core);
    stateLinkTarget.addReference("LinkPkg", linkReference);
    auto linkedResult =
        compile({{"LinkProbe.uc", Bytes(linkText.begin(), linkText.end())}},
                stateLinkTarget, "LinkPkg");
    for (std::size_t i = 0; i < linkedResult.exports.size(); ++i)
      if (linkedResult.ref(int(i) + 1) == "LinkProbe.S.F") {
        Reader r{linkedResult.exports[i].payload};
        r.index();
        check(linkedResult.exports[i].super == 0 && r.index() == 0,
              "Verified state super must agree in export and field metadata");
      }
    rejects([&] {
      auto changed = stateLinkSource("float");
      compile({{"LinkProbe.uc", Bytes(changed.begin(), changed.end())}},
              stateLinkTarget, "LinkPkg");
    });
    rejects([&] { build("class A extends Object; latent function Invalid();"); });
    rejects([&] { build("class A extends Object; native(256) final latent "
                        "function Sleep(float T); function F(){Sleep(1.0);}"); });
    rejects(
        [&] { build("class A extends Object; iterator function Invalid();"); });
    auto iteratorControl = build(
        "class A extends Object; native(304) final iterator function Items(); "
        "function Visit(){foreach Items(){foreach Items(){return;} break;}} "
        "function Skip(){foreach Items(){continue;}} "
        "function Halt(){foreach Items(){break;}}\n"
        "function Check(){assert(true);}");
    auto checkScript = [&](const std::string& path, const Bytes& expected) {
      for (std::size_t i = 0; i < iteratorControl.exports.size(); ++i) {
        if (iteratorControl.ref(int(i) + 1) != path) continue;
        functionInfo(iteratorControl, int(i) + 1);
        Reader r{iteratorControl.exports[i].payload};
        for (int j = 0; j < 7; ++j) r.index();
        r.u32();
        r.u32();
        check(r.u32() == expected.size() && r.take(expected.size()) == expected,
              "Iterator cleanup/jump bytecode differs from its VM contract");
        return;
      }
      throw std::runtime_error("Missing iterator test function");
    };
    // No object/name operands: serialized offsets equal VM program counters.
    checkScript("A.Visit", {0x2f, 0x61, 0x30, 0x16, 22,   0, 0x2f, 0x61, 0x30,
                            0x16, 17,   0,    0x30, 0x30, 4, 0x0b, 0x31, 0x30,
                            6,    22,   0,    0x31, 0x30, 4, 0x0b});
    checkScript("A.Skip",
                {0x2f, 0x61, 0x30, 0x16, 10, 0, 6, 9, 0, 0x31, 0x30, 4, 0x0b});
    checkScript("A.Halt",
                {0x2f, 0x61, 0x30, 0x16, 10, 0, 6, 10, 0, 0x31, 0x30, 4, 0x0b});
    checkScript("A.Check", {9, 2, 0, 0x27, 4, 0x0b});
    rejects([&] {
      build(
          "class A extends Object; function Plain(); "
          "function F(){foreach Plain(){}} ");
    });
    rejects([&] {
      build(
          "class A extends Object; native(304) final iterator "
          "function Items(); function F(){Items();}");
    });
    rejects([&] {
      build(
          "class A extends Object; native(304) final iterator "
          "function Items(); state S {Begin: foreach Items(){stop;}}");
    });
    rejects([&] {
      build(
          "class A extends Object; native(304) final iterator "
          "function Items(); state S {Begin: foreach Items(){goto 'End';} End: "
          "stop;}");
    });
    auto iteratorTypes = [&](const std::string& selected,
                             const std::string& output) {
      return localBuild(
          {"class Base extends Object; native(304) final iterator function "
           "Items(class<Base> Kind, out Base Value);",
           "class Derived extends Base;",
           "class Uses extends Base; function F(){local " + output +
               " Value; foreach Items(class'" + selected +
               "',Value){break;}}"});
    };
    iteratorTypes("Derived", "Derived");
    rejects([&] { iteratorTypes("Base", "Derived"); });
    rejects([&] { iteratorTypes("Derived", "Base"); });
    const auto vectorType = Type::named(Type::Struct, "Core.Object.Vector");
    const auto rotatorType = Type::named(Type::Struct, "Core.Object.Rotator");
    check(primitiveCast(rotatorType, vectorType) == 0x39 &&
              primitiveCast(vectorType, rotatorType) == 0x50,
          "SDK-attested vector/rotator cast tokens");
    rejects([&] {
      primitiveCast(Type::named(Type::Struct, "Other.Vector"), rotatorType);
    });
    auto castCore =
        rebuildCore({"class Object; struct Vector {var float X,Y,Z;}; "
                     "struct Rotator {var int Pitch,Yaw,Roll;}; var const "
                     "class<Object> Class;"});
    TargetModel castTarget;
    castTarget.add("Core", castCore);
    std::string castSource =
        "class CastProbe extends Object; "
        "function vector Direction(rotator R){return vector(R);} "
        "function rotator Angles(vector V){return rotator(V);} "
        "function string VectorText(vector V){return string(V);} "
        "function string RotatorText(rotator R){return string(R);} "
        "function vector ParseVector(string S){return vector(S);} "
        "function rotator ParseRotator(string S){return rotator(S);} "
        "function string ClassText(){return string(Class);} "
        "function rotator NamedRot(){local rotator Rot; Rot.Roll=256; return "
        "Rot;} "
        "static function class<CastProbe> OwnClass(){return default.Class;} "
        "function class<CastProbe> OtherClass(CastProbe Other){return "
        "Other.Class;} "
        "var int Count; static function SetCount(int N){switch(N){"
        "case 1: default.Count=2; break; default: default.Count=3; break;}}";
    auto casts =
        compile({{"CastProbe.uc", Bytes(castSource.begin(), castSource.end())}},
                castTarget, "Casts");
    int castFunctions = 0;
    for (std::size_t i = 0; i < casts.exports.size(); ++i)
      if (casts.kind(int(i) + 1) == "Function") {
        functionInfo(casts, int(i) + 1);
        ++castFunctions;
      }
    check(castFunctions == 11,
          "Intrinsic casts, context properties and default labels compile");
    std::string playerSource = "class Player extends Object;";
    auto connectionReference = compile(
        {{"Player.uc", Bytes(playerSource.begin(), playerSource.end())}},
        external, "Engine");
    TargetImports connectionImports(coreOnly, connectionReference);
    connectionImports.nativeClass("Engine", "NetConnection");
    connectionImports.nativeClass("Engine", "Viewport");
    TargetModel connectionTarget;
    connectionTarget.add("Core", core);
    connectionTarget.addReference("Engine", connectionReference);
    auto connection = connectionTarget.findClass("Engine.NetConnection");
    check(connection && connectionTarget.ancestry(*connection).size() == 3 &&
              connectionTarget.identity(
                  connectionTarget.ancestry(*connection)[1]) == "Engine.Player",
          "SDK-attested native NetConnection ancestry");
    std::string connectionSource =
        "class ConnectionProbe extends Object; "
        "function Player Up(NetConnection N){return N;} "
        "function NetConnection Down(Player P){return NetConnection(P);} "
        "function Player ViewUp(Viewport V){return V;} "
        "function Viewport ViewDown(Player P){return Viewport(P);}";
    compile({{"Player.uc", Bytes(playerSource.begin(), playerSource.end())},
             {"ConnectionProbe.uc",
              Bytes(connectionSource.begin(), connectionSource.end())}},
            connectionTarget, "Engine");
    auto spawnTypes = [&](const std::string& call) {
      std::vector<std::pair<std::string, Bytes>> sources;
      for (const std::string& text :
           {std::string(
                "class Actor extends Object; native(278) final function "
                "Actor Spawn(class<Actor> Kind); "
                "function Actor Make(class<Actor> Kind){return None;}"),
            std::string("class Pawn extends Actor; var float Radius;"),
            "class Uses extends Actor; var Pawn Pawn; "
            "function Pawn Create(){return " +
                call +
                "(class'Pawn');} "
                "function float Size(){return Pawn.default.Radius;}"})
        sources.push_back({"SpawnTypes.uc", Bytes(text.begin(), text.end())});
      return compile(sources, coreOnly, "Engine");
    };
    auto spawnResult = spawnTypes("Spawn");
    for (std::size_t i = 0; i < spawnResult.exports.size(); ++i)
      if (spawnResult.ref(int(i) + 1) == "Uses.Size") {
        Reader r{spawnResult.exports[i].payload};
        for (int j = 0; j < 7; ++j) r.index();
        r.u32();
        r.u32();
        r.u32();
        check(r.take(3) == Bytes({4, 0x19, 1}),
              "Object.default must use the instance, not a same-name class");
      }
    rejects([&] { spawnTypes("Make"); });
    auto operatorCore = build(
        "class Object extends Core.Object; native(130) final static function "
        "bool AndAnd_BoolBool(bool A, skip bool B);");
    operatorCore.exports[0].super = 0;
    operatorCore.exports[0].payload[0] = 0;
    auto& operatorBytes = operatorCore.exports[1].payload;
    Reader operatorReader{operatorBytes};
    for (int i = 0; i < 5; ++i) operatorReader.index();
    int friendly = operatorCore.intern("&&");
    check(friendly < 64, "Small native operator fixture name index");
    operatorBytes[operatorReader.pos] = std::uint8_t(friendly);
    operatorBytes[operatorBytes.size() - 5] = 30;
    for (int i = 0; i < 4; ++i)
      operatorBytes[operatorBytes.size() - 4 + i] =
          std::uint8_t(0x23401u >> (8 * i));
    TargetModel operatorReference;
    operatorReference.addReference("Core", operatorCore);
    auto rebuildOperator = [&](const std::string& declaration) {
      std::string source = "class Object; " + declaration;
      return compile({{"Object.uc", Bytes(source.begin(), source.end())}},
                     operatorReference, "Core");
    };
    auto rebuiltOperator = rebuildOperator(
        "native(130) final static operator(30) bool &&(bool A, skip bool B);");
    auto operatorInfo = functionInfo(rebuiltOperator, 2);
    check(rebuiltOperator.ref(2) == "Object.AndAnd_BoolBool" &&
              operatorInfo.native == 130 && operatorInfo.precedence == 30 &&
              operatorInfo.symbol == "&&" &&
              (operatorInfo.parameters[1].flags & 0x200),
          "Native operator contract and short-circuit modifier");
    rejects([&] {
      rebuildOperator(
          "native(130) final static operator(30) bool &&(bool A, bool B);");
    });
    rejects([&] {
      rebuildOperator(
          "native(130) final static operator(31) bool &&(bool A, skip bool "
          "B);");
    });
    auto objects =
        localBuild({"class A extends Object; var B Peer; function int Read(B "
                    "V){return V.Value;} function Object Myself(){return "
                    "self;} defaultproperties {Peer=None}",
                    "class B extends Object; var A Peer; var int Value;"});
    bool sawForward = false, sawBack = false;
    for (std::size_t i = 0; i < objects.exports.size(); ++i)
      if (objects.kind(int(i) + 1) == "ObjectProperty") {
        auto prop = propertyInfo(objects, int(i) + 1);
        auto identity = objects.ref(int(i) + 1);
        if (identity == "A.Peer")
          sawForward = objects.ref(prop.reference) == "B";
        if (identity == "B.Peer") sawBack = objects.ref(prop.reference) == "A";
      }
    check(sawForward && sawBack, "Mutual forward object class references");
    auto constants =
        localBuild({"class A extends Object; const Size=3; const "
                    "Text=\"hello\"; var int Items[Size];",
                    "class B extends A; function int F(){return Size;}"});
    bool sawConstant = false;
    for (std::size_t i = 0; i < constants.exports.size(); ++i)
      if (constants.kind(int(i) + 1) == "Const" &&
          constants.ref(int(i) + 1) == "A.Size") {
        Reader data{constants.exports[i].payload};
        check(data.index() == 0 && data.index() == 0, "Const field prefix");
        data.index();
        check(data.string() == "3" && data.pos == data.data.size(),
              "Const literal metadata");
        sawConstant = true;
      }
    check(sawConstant, "Const export");
    TargetModel resourceTarget;
    resourceTarget.add("Core", core);
    auto resources = build("class ResourceContainer extends Object;");
    TargetImports resourceImports(resourceTarget, resources);
    auto objectClass =
        resourceImports.symbol(*resourceTarget.findClass("Core.Object"));
    Bytes emptyObject;
    index(emptyObject, resources.intern("None"));
    resources.exports.push_back({objectClass, 0, 0, resources.intern("Sample"),
                                 0x000f0004, emptyObject});
    resourceTarget.add("Assets", resources);
    std::string resourceSource =
        "class A extends Object; const Asset=Object'Assets.Sample'; function "
        "Object F(){return Asset;} var Object Saved; defaultproperties "
        "{Saved=Object'Assets.Sample'}";
    auto resourceConsumer =
        compile({{"A.uc", Bytes(resourceSource.begin(), resourceSource.end())}},
                resourceTarget);
    bool importedAsset = false;
    for (std::size_t i = 0; i < resourceConsumer.imports.size(); ++i)
      importedAsset |= resourceConsumer.ref(-int(i) - 1) == "Assets.Sample";
    check(importedAsset, "Typed object literal import");
    rejects([&] {
      std::string source =
          "class A extends Object; const Bad=A'Assets.Sample';";
      compile({{"A.uc", Bytes(source.begin(), source.end())}}, resourceTarget);
    });
    auto enums = localBuild(
        {"class A extends Object; enum E {One,Two}; var E Value; function E "
         "F(){return Two;} defaultproperties {Value=Two}",
         "class B extends A; var E Other; function E F(){return One;}"});
    check(enumNames(enums, 2) == std::vector<std::string>({"One", "Two"}),
          "Enum names metadata");
    check(propertyInfo(enums, 3).reference == 2,
          "Enum byte property reference");
    rejects([&] { build("class A extends Object; enum E {One,one};"); });
    rejects([&] {
      build(
          "class A extends Object; enum E {One}; enum F {Two}; function E G(F "
          "V){return V;}");
    });
    rejects([&] {
      build(
          "class A extends Object; enum E {One}; function E F(byte V){return "
          "V;}");
    });
    rejects([&] {
      build(
          "class A extends Object; enum E {One}; function F(out byte V){} "
          "function G(E V){F(V);}");
    });
    rejects([&] {
      build(
          "class A extends Object; enum E {One}; var E V; defaultproperties "
          "{V=17}");
    });
    auto brokenEnum = enums;
    brokenEnum.exports[1].payload.push_back(0);
    rejects([&] { enumNames(brokenEnum, 2); });
    auto structs = localBuild(
        {"class A extends Object; struct Base {var int X; var string Text;}; "
         "struct Derived extends Base {var byte Y;}; struct Wrapper {struct "
         "Inner {var bool Flag;}; var Inner Item;}; var Derived Value; "
         "function Base Copy(Base V){return V;}",
         "class B extends A; function int F(Derived V){return V.X;}"});
    int baseStruct = 0, derivedStruct = 0, wrapperStruct = 0;
    for (std::size_t i = 0; i < structs.exports.size(); ++i) {
      auto id = int(i) + 1;
      if (structs.ref(id) == "A.Base") baseStruct = id;
      if (structs.ref(id) == "A.Derived") derivedStruct = id;
      if (structs.ref(id) == "A.Wrapper") wrapperStruct = id;
      if (structs.kind(id) == "Struct") structInfo(structs, id);
      if (structs.kind(id) == "StructProperty" && structs.ref(id) == "A.Value")
        check((propertyInfo(structs, id).flags & 0x400000) != 0,
              "Inherited struct constructor-link flag");
    }
    check(baseStruct && derivedStruct && wrapperStruct, "Struct exports");
    check(structInfo(structs, derivedStruct).super == baseStruct,
          "Struct inheritance metadata");
    auto wrapper = structInfo(structs, wrapperStruct);
    check(wrapper.fields.size() == 1 &&
              structs.ref(wrapper.fields[0].reference) == "A.Wrapper.Inner",
          "Nested struct metadata and field chain");
    auto brokenStruct = structs;
    auto compound = build(
        "class A extends Object; struct S {var int X; var string Text;}; var S "
        "Value; var array<S> Items; defaultproperties {Value=(X=7,Text=\"ok\") "
        "Items(1)=(X=9)}");
    auto compoundTags = classDefaults(compound, 1);
    bool sawStructDefault = false, sawStructArray = false;
    for (auto& tag : compoundTags) {
      if (tag.kind == 10) {
        check(tag.structName == "S", "Struct default tag type name");
        Reader data{tag.data};
        auto members = defaultTags(compound, data);
        check(members.size() == 2 && data.pos == data.data.size(),
              "Nested struct tags");
        sawStructDefault = true;
      }
      if (tag.kind == 9) {
        Reader data{tag.data};
        check(data.index() == 2 && defaultTags(compound, data).empty(),
              "Struct array gap initialization");
        auto members = defaultTags(compound, data);
        check(members.size() == 1 && data.pos == data.data.size(),
              "Struct array tagged element");
        sawStructArray = true;
      }
    }
    check(sawStructDefault && sawStructArray, "Compound default tags");
    auto implicitArray = build(
        "class A extends Object; struct S {var int Slots[2];}; var S Value; "
        "defaultproperties {Value=(Slots=7,Slots[1]=9)}");
    auto explicitArray = build(
        "class A extends Object; struct S {var int Slots[2];}; var S Value; "
        "defaultproperties {Value=(Slots[0]=7,Slots[1]=9)}");
    check(classDefaults(implicitArray, 1)[0].data ==
              classDefaults(explicitArray, 1)[0].data,
          "Implicit first struct array element changed defaults");
    rejects([&] {
      build(
          "class A extends Object; struct S {var int Slots[2];}; var S Value; "
          "defaultproperties {Value=(Slots=7,Slots[0]=9)}");
    });
    auto intrinsicCore = build(
        "class Holder extends Object; struct Vector {var float x,y,z;}; struct "
        "INT64 {var int Left,Right;};");
    intrinsicCore.exports[0].name = intrinsicCore.intern("Object");
    intrinsicCore.exports[0].super = 0;
    intrinsicCore.exports[0].payload[0] = 0;
    TargetModel intrinsicTarget;
    intrinsicTarget.add("Core", intrinsicCore);
    auto wideBuild = [&](const std::string& source) {
      return compile({{"Wide.uc", Bytes(source.begin(), source.end())}},
                     intrinsicTarget);
    };
    auto wideDefaults = wideBuild(
        "class Wide extends Object; const Maximum=9223372036854775807; "
        "var INT64 A,B; function INT64 F(){return Maximum;} "
        "function INT64 G(){return -9223372036854775808;} "
        "defaultproperties {A=9223372036854775807 B=-9223372036854775808}");
    auto wideTags = classDefaults(wideDefaults, 1);
    check(wideTags.size() == 2, "INT64 default count");
    for (const auto& tag : wideTags) {
      Reader data{tag.data};
      auto fields = defaultTags(wideDefaults, data);
      check(fields.size() == 2, "INT64 uses tagged Left/Right storage");
      for (const auto& field : fields) {
        Reader word{field.data};
        auto expected = tag.name == "A"
                            ? (field.name == "Left" ? INT32_MAX : -1)
                            : (field.name == "Left" ? INT32_MIN : 0);
        check(word.i32() == expected, "INT64 defaults preserve all 64 bits");
      }
    }
    rejects([&] {
      wideBuild("class Wide extends Object; const X=9223372036854775808;");
    });
    rejects([&] {
      wideBuild("class Wide extends Object; const X=-9223372036854775809;");
    });
    rejects([&] {
      wideBuild("class Wide extends Object; const X=184467440737095516160000;");
    });
    rejects([&] {
      wideBuild(
          "class Wide extends Object; function int F(INT64 V){return V;}");
    });
    rejects([&] {
      wideBuild(
          "class Wide extends Object; var INT64 V; defaultproperties {V=1.5}");
    });
    std::string intrinsicParentSource =
        "class Parent extends Object; var Vector Position; "
        "defaultproperties {Position=(X=1.5,Z=2.5)}";
    auto intrinsicParent = compile(
        {{"Parent.uc",
          Bytes(intrinsicParentSource.begin(), intrinsicParentSource.end())}},
        intrinsicTarget);
    intrinsicTarget.add("ParentPkg", intrinsicParent);
    std::string intrinsicChildSource =
        "class Child extends ParentPkg.Parent; "
        "defaultproperties {Position=(y=7.5)}";
    auto intrinsicChild = compile(
        {{"Child.uc",
          Bytes(intrinsicChildSource.begin(), intrinsicChildSource.end())}},
        intrinsicTarget);
    auto intrinsicTags = classDefaults(intrinsicChild, 1);
    check(intrinsicTags.size() == 1 && intrinsicTags[0].kind == 10,
          "Inherited native Vector default tag");
    Reader intrinsicData{intrinsicTags[0].data};
    check(intrinsicData.i32() == 0x3fc00000 &&
              intrinsicData.i32() == 0x40f00000 &&
              intrinsicData.i32() == 0x40200000 &&
              intrinsicData.pos == intrinsicData.data.size(),
          "Case-insensitive native defaults preserve inherited components");
    auto forwardTypes = localBuild(
        {"class A extends Object; var B.S Value; var B.E Mode; struct Own {var "
         "int X;}; defaultproperties {Value=(X=7) Mode=B.E.Second}",
         "class B extends Object; struct S {var int X;}; var A.Own Other; var "
         "enum E {First,Second} State;"});
    for (std::size_t i = 0; i < forwardTypes.exports.size(); ++i)
      if (forwardTypes.kind(int(i) + 1) == "StructProperty")
        check(forwardTypes.kind(
                  propertyInfo(forwardTypes, int(i) + 1).reference) == "Struct",
              "Forward struct export resolution");
    auto within = localBuild({"class Inner extends Object within Outer;",
                              "class Outer extends Object;"});
    auto enumLiteral =
        localBuild({"class Enum extends Object;",
                    "class A extends Object; const Empty=Enum'None';"});
    check(enumLiteral.exports.size() == 3,
          "Type prescan distinguishes Enum object literal from declaration");
    std::string enumObjectSource =
        "class A extends Object; enum E {One}; function Object F(){return "
        "Enum'Owned.A.E';}";
    auto enumObjectSources = std::vector<std::pair<std::string, Bytes>>{
        {"A.uc", Bytes(enumObjectSource.begin(), enumObjectSource.end())}};
    auto enumObjects = compile(enumObjectSources, external, "Owned");
    check(enumObjects.kind(2) == "Enum", "Qualified source enum object export");
    rejects([&] { compile(enumObjectSources, external, "Other"); });
    auto dynamicClass = build("class A extends Object dynamicrecompile;");
    {
      auto intrinsicCore = core;
      const int coreName = intrinsicCore.intern("Core");
      const int packageKind = intrinsicCore.intern("Package");
      const int classKind = intrinsicCore.intern("Class");
      intrinsicCore.imports.push_back({coreName, packageKind, 0, coreName});
      const int intrinsicRoot = -int(intrinsicCore.imports.size());
      intrinsicCore.imports.push_back({coreName, classKind, intrinsicRoot,
                                       intrinsicCore.intern("NativeHandle")});
      const int intrinsicId = -int(intrinsicCore.imports.size());
      intrinsicCore.imports.push_back(
          {coreName, packageKind, 0, intrinsicCore.intern("MissingPackage")});
      const int missingRoot = -int(intrinsicCore.imports.size());
      intrinsicCore.imports.push_back({coreName, classKind, missingRoot,
                                       intrinsicCore.intern("MissingClass")});
      TargetModel intrinsicModel;
      intrinsicModel.add("Core", intrinsicCore);
      check(
          intrinsicModel.findClass("NativeHandle") == Symbol{0, intrinsicId} &&
              !intrinsicModel.findClass("MissingClass"),
          "Only self-package intrinsic imports are available as native "
          "classes");
      const std::string intrinsicSource =
          "class A extends Object; var NativeHandle Handle; "
          "function Object AsObject(NativeHandle Value){return Value;}";
      auto intrinsicOutput = compile(
          {{"A.uc", Bytes(intrinsicSource.begin(), intrinsicSource.end())}},
          intrinsicModel, "IntrinsicProbe");
      check(intrinsicOutput.ref(propertyInfo(intrinsicOutput, 2).reference) ==
                "Core.NativeHandle",
            "Native class references preserve their original module identity");
    }
    auto instancedClass = build("class A extends Object instanced;");
    check((classInfo(instancedClass, 1).flags & 0x200000) != 0,
          "Client instanced class flag");
    auto instancedProperty =
        localBuild({"class Value extends Object instanced;",
                    "class Holder extends Object; var Value Stored;"});
    for (std::size_t i = 0; i < instancedProperty.exports.size(); ++i)
      if (instancedProperty.ref(int(i) + 1) == "Holder.Stored")
        check(propertyInfo(instancedProperty, int(i) + 1).flags == 0x400000,
              "Instanced object property requires constructor linking");
    auto pointerFields = build(
        "class A extends Object native; var native pointer Storage; "
        "var transient pointer Slots[2];");
    check(pointerFields.kind(2) == "PtrProperty" &&
              propertyInfo(pointerFields, 2).flags == 0x1000 &&
              propertyInfo(pointerFields, 3).dimension == 2 &&
              propertyInfo(pointerFields, 3).flags == 0x2000,
          "Native pointer fields retain property kind, flags and dimension");
    rejects([&] {
      build(
          "class A extends Object; var native pointer Storage; "
          "function F(){Storage=1;}");
    });
    rejects([&] {
      build(
          "class A extends Object; var native pointer Storage; "
          "function int F(){return int(Storage);}");
    });
    rejects([&] {
      build(
          "class A extends Object; var native pointer Storage; "
          "function F(){Storage=Storage+1;}");
    });
    check((classInfo(dynamicClass, 1).flags & 0x400000) != 0,
          "Client dynamicrecompile flag");
    auto states = localBuild(
        {"class A extends Object; function int F(){return 1;} auto state Idle "
         "{function int F(){return 7;} Begin: goto 'Done'; Done: stop;} state "
         "Active extends Idle {ignores F;}",
         "class B extends A; state Idle {function int F(){return 9;}}"});
    int stateCount = 0;
    for (std::size_t i = 0; i < states.exports.size(); ++i)
      if (states.kind(int(i) + 1) == "State") {
        auto info = stateInfo(states, int(i) + 1);
        if (states.ref(int(i) + 1) == "A.Idle")
          check(info.flags == 2 && info.labelOffset != 0xffff,
                "Auto state and label table");
        if (states.ref(int(i) + 1) == "B.Idle")
          check(states.ref(info.super) == "A.Idle",
                "Inherited source state super");
        ++stateCount;
      }
    check(stateCount == 3, "States remain distinct class-owned exports");
    auto replication = localBuild(
        {"class A extends Object; var bool Enabled; var int Value; replication "
         "{reliable if(Enabled) Value,Send;} function Send(int V){Value=V;}",
         "class B extends A; function Send(int V){Value=V;}"});
    for (std::size_t i = 0; i < replication.exports.size(); ++i) {
      auto id = int(i) + 1;
      if (replication.ref(id) == "A.Value")
        check(propertyInfo(replication, id).replicationOffset == 0,
              "Replicated property condition offset");
      if (replication.ref(id) == "A.Send")
        check(functionInfo(replication, id).replicationOffset == 0,
              "Replicated function condition offset");
      if (replication.ref(id) == "B.Send")
        check(functionInfo(replication, id).replicationOffset == 65535 &&
                  (functionInfo(replication, id).flags & 0xc0) == 0xc0,
              "Inherited reliable RPC has inherited condition sentinel");
    }
    rejects([&] {
      build(
          "class A extends Object; var int V; replication {reliable if(1) V;}");
    });
    rejects([&] {
      build(
          "class A extends Object; var int V; replication {reliable if(true) "
          "V,V;}");
    });
    rejects([&] {
      build("class A extends Object; replication {reliable if(true) Missing;}");
    });
    rejects([&] {
      build(
          "class A extends Object; var array<int> V; delegate int Compare(int "
          "A,string B); function F(){V.Sort(Compare);}");
    });
    std::string sortSource =
        "class A extends Object; var array<int> V; delegate int Compare(int "
        "A,int B); function F(){V.Sort(Compare);}";
    auto sortPackage = build(sortSource);
    for (std::size_t i = 0; i < sortPackage.exports.size(); ++i)
      if (sortPackage.kind(int(i) + 1) == "Function")
        functionInfo(sortPackage, int(i) + 1);
    auto legacyCore = core;
    legacyCore.version = 126;
    legacyCore.licensee = 0;
    rejects([&] {
      compile({{"A.uc", Bytes(sortSource.begin(), sortSource.end())}},
              legacyCore);
    });
    rejects([&] {
      build(
          "class A extends Object; state X {function Hidden(){}} function F(A "
          "V){V.Hidden();}");
    });
    rejects([&] {
      build(
          "class A extends Object; state X extends Y {} state Y extends X {}");
    });
    rejects([&] {
      build("class A extends Object; state X {Begin: stop; Begin: stop;}");
    });
    rejects([&] {
      build("class A extends Object; auto state X {} auto state Y {}");
    });
    rejects(
        [&] { build("class A extends Object; state X {ignores Missing;}"); });
    rejects([&] {
      build(
          "class A extends Object; delegate int D(int V); function H(){} "
          "function F(){D=H;}");
    });
    rejects([&] {
      build(
          "class A extends Object; delegate int D(out int V); function int "
          "H(int V){return V;} function F(){D=H;}");
    });
    {
      auto stateSuper = localBuild(
          {"class SuperBase extends Object; function F(){}",
           "class SuperChild extends SuperBase; function F(){} "
           "state Plain {function F(){super.F();}} "
           "state ParentState {function F(){}} "
           "state DerivedState extends ParentState {function F(){super.F();}}",
           "class SuperLeaf extends SuperChild; function "
           "G(){super(SuperBase).F();}"});
      auto checkTarget = [&](const std::string& function,
                             const std::string& target) {
        for (std::size_t i = 0; i < stateSuper.exports.size(); ++i) {
          if (stateSuper.ref(int(i) + 1) != function) continue;
          Reader r{stateSuper.exports[i].payload};
          for (int j = 0; j < 7; ++j) r.index();
          r.u32();
          r.u32();
          r.u32();
          check(r.byte() == 0x1c && stateSuper.ref(r.index()) == target,
                "State super call selected the current class global function");
          return;
        }
        throw std::runtime_error("Missing state super regression function");
      };
      checkTarget("SuperChild.Plain.F", "SuperBase.F");
      checkTarget("SuperChild.DerivedState.F", "SuperChild.ParentState.F");
      checkTarget("SuperLeaf.G", "SuperBase.F");
      TargetModel qualifiedTarget;
      qualifiedTarget.add("Core", core);
      qualifiedTarget.add("Parents", stateSuper);
      std::string qualifiedSource =
          "class External extends Parents.SuperChild; "
          "function G(){super(SuperBase).F();}";
      stateSuper = compile({{"External.uc", Bytes(qualifiedSource.begin(),
                                                  qualifiedSource.end())}},
                           qualifiedTarget, "ChildPkg");
      checkTarget("External.G", "Parents.SuperBase.F");
      rejects([&] {
        localBuild({"class A extends Object; function F(){}",
                    "class B extends Object; function G(){super(A).F();}"});
      });
    }
    localBuild(
        {"class A extends Object; function int Min(){return 3;}",
         "class B extends A dependsOn(A) dependsOn(C); var int Min; function "
         "int F(){return Min;} function int G(){return super.Min();}",
         "class C extends Object;"});
    rejects([&] {
      localBuild(
          {"class A extends Object; function int Min(){return 3;}",
           "class B extends A; var int Min; function int F(){return Min();}"});
    });
    localBuild({"class A extends Object; var int Min;",
                "class B extends A; var string Min;"});
    localBuild(
        {"class A extends Object; enum First {One,Max}; enum Second {Two,Max}; "
         "function byte F(){return First.Max;}"});
    rejects([&] {
      localBuild(
          {"class A extends Object; enum First {One,Max}; enum Second "
           "{Two,Max}; function byte F(){return Max;}"});
    });
    check(within.ref(classInfo(within, 1).within) == "Outer",
          "Forward within class reference");
    auto classModifiers = build(
        "class A extends Object native noexport safereplace editinlinenew "
        "collapsecategories;");
    check(classInfo(classModifiers, 1).flags == 0x3152 &&
              (classModifiers.exports[0].flags & 0x4000000),
          "SDK class modifier metadata");
    auto localized =
        localBuild({"class A extends Object; var localized string Text;",
                    "class B extends A;"});
    for (std::size_t i = 0; i < localized.exports.size(); ++i)
      if (localized.kind(int(i) + 1) == "Class")
        check((classInfo(localized, int(i) + 1).flags & 0x20) != 0,
              "Inherited automatic localized class flag");
    auto access = build(
        "class A extends Object; private function int Secret(){return 7;} "
        "protected function int Family(){return 9;}");
    auto privateFields = build(
        "class A extends Object; var private int Hidden; var protected int "
        "Family;");
    check((privateFields.exports[1].flags & 0x84) == 0x80 &&
              (privateFields.exports[2].flags & 0x84) == 0,
          "Property access uses export flags like SDK");
    TargetModel privacyTarget;
    privacyTarget.add("Core", core);
    privacyTarget.add("Privacy", privateFields);
    rejects([&] {
      std::string source =
          "class B extends Privacy.A; function int F(){return Hidden;}";
      compile({{"B.uc", Bytes(source.begin(), source.end())}}, privacyTarget);
    });
    for (std::size_t i = 0; i < access.exports.size(); ++i)
      if (access.kind(int(i) + 1) == "Function") {
        auto info = functionInfo(access, int(i) + 1);
        check(!(info.flags & 0x20000) && (info.flags & 0xc0000),
              "Function access flags");
      }
    rejects([&] {
      localBuild({"class A extends Object within B;",
                  "class B extends Object within A;"});
    });
    rejects([&] {
      localBuild({"class A extends Object; private function F(){}",
                  "class B extends A; function G(){F();}"});
    });
    rejects([&] {
      localBuild({"class A extends Object; var private int X;",
                  "class B extends A; function int G(){return X;}"});
    });
    rejects([&] {
      localBuild({"class A extends Object; protected function F(){}",
                  "class B extends Object; function G(A V){V.F();}"});
    });
    rejects([&] {
      localBuild({"class A extends Object; function F(){}",
                  "class B extends A; private function F(){}"});
    });
    auto privateRedeclaration = localBuild(
        {"class A extends Object; private function int Hidden(){return 1;}",
         "class B extends A; private function int Hidden(){return 2;}"});
    for (std::size_t i = 0; i < privateRedeclaration.exports.size(); ++i)
      if (privateRedeclaration.ref(int(i) + 1) == "B.Hidden")
        check(privateRedeclaration.exports[i].super == 0,
              "Private parent function is not an override target");
    rejects([&] {
      localBuild({"class A extends Object;",
                  "class Inner extends Object within A;",
                  "class B extends Object; function Object F(){return "
                  "new(self) class'Inner';}"});
    });
    rejects([&] {
      build(
          "class A extends Object; struct S {var int X;}; var S V; "
          "defaultproperties {V=(Unknown=1)}");
    });
    rejects([&] {
      build(
          "class A extends Object; struct S {var int X;}; var S V; "
          "defaultproperties {V=(X=1,X=2)}");
    });
    rejects([&] {
      build(
          "class A extends Object; struct S {var int X[2];}; var S V; "
          "defaultproperties {V=(X[2]=1)}");
    });
    brokenStruct.exports[baseStruct - 1].payload.push_back(0);
    rejects([&] { structInfo(brokenStruct, baseStruct); });
    rejects([&] { build("class A extends Object; struct S {var S Self;};"); });
    rejects([&] {
      build("class A extends Object; struct S {var int X; var int x;};");
    });
    rejects([&] {
      build(
          "class A extends Object; struct S {var int X;}; struct T extends S "
          "{var int X;};");
    });
    rejects([&] {
      build(
          "class A extends Object; struct S {var int X;}; struct T {var int "
          "X;}; function S F(T V){return V;}");
    });
    rejects([&] {
      build(
          "class A extends Object; struct S {var int X;}; struct T extends S "
          "{var int Y;}; function F(out S V){} function G(T V){F(V);}");
    });
    rejects([&] {
      build(
          "class A extends Object; struct S {var const int X;}; var S V; "
          "function F(){V.X=1;}");
    });
    rejects([&] {
      build(
          "class A extends Object; struct S {var int X;}; function S F(S "
          "V){return V;} function G(S V){F(V).X=1;}");
    });
    rejects([&] { build("class A extends Object; const X=1; var int X;"); });
    rejects([&] {
      build("class A extends Object; const X=1; function F(){X=2;}");
    });
    rejects([&] { build("class A extends Object; const X=1+2;"); });
    rejects([&] { build("class A extends Object; const X=1; var int V[X];"); });
    auto classValues = build(
        "class A extends Object; var class<A> Factory; function class<A> "
        "F(){return class'A';} function A Make(){return new class'A';} "
        "defaultproperties {Factory=class'A'}");
    auto factory = propertyInfo(classValues, 2);
    check(factory.kind == "ClassProperty" && factory.metaclass == 1 &&
              classValues.ref(factory.reference) == "Core.Class",
          "Class property intrinsic class and metaclass");
    auto classTags = classDefaults(classValues, 1);
    Reader classValue{classTags.at(0).data};
    check(classTags[0].kind == 5 && classValue.index() == 1,
          "Class literal default export reference");
    rejects([&] {
      localBuild({"class A extends Object;",
                  "class B extends Object; var class<B> Factory; "
                  "defaultproperties {Factory=class'A'}"});
    });
    rejects([&] {
      build(
          "class A extends Object; function F(){local class<A> C; local Object "
          "V; C=V;}");
    });
    rejects([&] {
      build("class A extends Object; function Object F(){return new 1;}");
    });
    rejects([&] { build("class A extends Object; var class<bool> Invalid;"); });
    rejects([&] {
      build(
          "class A extends Object; function F(){} function G(class<A> "
          "T){T.static.F();}");
    });
    for (std::size_t i = 0; i < objects.exports.size(); ++i)
      if (objects.kind(int(i) + 1) == "Function")
        functionInfo(objects, int(i) + 1);
    rejects([&] {
      localBuild({"class A extends Object;",
                  "class B extends A; function B F(A V){return V;}"});
    });
    rejects([&] {
      localBuild({"class A extends Object;",
                  "class B extends Object; function B F(A V){return B(V);}"});
    });
    rejects([&] {
      localBuild({"class A extends Object;",
                  "class B extends A; function F(B V){Set(V);} function "
                  "Set(out A V){}"});
    });
    rejects([&] {
      localBuild(
          {"class A extends Object;",
           "class B extends A; function array<B> F(array<A> V){return V;}"});
    });
    rejects([&] {
      localBuild({"class A extends Object; function F(B V){V.Value=1;}",
                  "class B extends Object; var const int Value;"});
    });
    rejects([&] {
      localBuild(
          {"class A extends Object; function int F(B V){return V.Values[0];}",
           "class B extends Object; var int Values[100];"});
    });
    auto local = localBuild(
        {"class Leaf extends Middle; function int ReadProbe(){return "
         "super.ReadProbe();} defaultproperties {Value=8}",
         "class Middle extends ParentPkg.Parent; function int "
         "ReadProbe(){return Value;}"});
    TargetModel linked;
    linked.add("Core", core);
    linked.add("ParentPkg", parent);
    linked.add("LocalPkg", Package::parse(local.serialize()));
    auto leaf = linked.findClass("LocalPkg.Leaf");
    check(leaf && linked.ancestry(*leaf).size() == 4,
          "Mixed source/external ancestry");
    auto leafRead = linked.member(*leaf, "ReadProbe");
    check(leafRead && local.ref(local.exports[leafRead->id - 1].super) ==
                          "Middle.ReadProbe",
          "Local override super export");
    rejects([&] { localBuild({"class A extends B;", "class B extends A;"}); });
    rejects([&] { localBuild({"class A extends A;"}); });
    auto flags = localBuild({"class B extends A;",
                             "class A extends Object abstract transient "
                             "config(TestConfig) perobjectconfig;"});
    check(classInfo(flags, 1).flags == 0x41f &&
              classInfo(flags, 2).flags == 0x41e,
          "Source class flag inheritance excludes abstract");
    check(classInfo(flags, 2).config == "TestConfig",
          "Source class config name inheritance");
    TargetModel flagModel;
    flagModel.add("Core", core);
    flagModel.add("Flags", flags);
    std::string flagChild = "class C extends Flags.B;";
    auto externalFlags = compile(
        {{"C.uc", Bytes(flagChild.begin(), flagChild.end())}}, flagModel);
    check(classInfo(externalFlags, 1).flags == 0x41e &&
              classInfo(externalFlags, 1).config == "TestConfig",
          "External class flag/config inheritance");
    rejects([&] { build("class A extends Object abstract abstract;"); });
    rejects([&] {
      localBuild({"class B extends A; function F(){}",
                  "class A extends Object; final function F(){}"});
    });
    rejects([&] {
      localBuild({"class B extends A; function F(){X=1;}",
                  "class A extends Object; var const int X;"});
    });
    rejects([&] {
      build("class A extends Object; var const int X; function F(){X=1;}");
    });
    auto declarations = build(
        "class A extends Object; var(Edit) const int X,Y; simulated event E(); "
        "final function F(){local int A,B; A=1; B=A;} ");
    auto array = build(
        "class A extends Object; var int Items[16385]; function int Read(int "
        "I){return Items[I];} defaultproperties {Items(16383)=3 "
        "Items(16384)=4}");
    check(propertyInfo(array, 2).dimension == 16385,
          "Static array dimension serialized");
    auto dynamic = build(
        "class A extends Object; var array<string> Words; function "
        "array<string> Copy(array<string> V){local array<string> W; W=V; "
        "return W;} defaultproperties {Words(1)=\"tail\"}");
    auto boolArray = build(
        "class A extends Object; var array<bool> Bits; "
        "function bool ReadBit(){return Bits[0];} "
        "function WriteBit(bool Value){Bits[0]=Value;} "
        "defaultproperties {Bits(0)=true Bits(2)=true}");
    auto boolArrayRead = Package::parse(boolArray.serialize());
    auto boolDefaults = classDefaults(boolArrayRead, 1);
    check(boolDefaults.size() == 1 && boolDefaults[0].kind == 9,
          "Dynamic bool array default tag");
    Reader boolData{boolDefaults[0].data};
    check(boolData.index() == 3 && boolData.byte() == 1 &&
              boolData.byte() == 0 && boolData.byte() == 1 &&
              boolData.pos == boolDefaults[0].data.size(),
          "Dynamic bool array default bytes");
    TargetModel boolParent;
    boolParent.add("Core", core);
    boolParent.add("BoolPkg", boolArrayRead);
    bool rejectedInheritedBoolDefault = false;
    try {
      std::string text =
          "class B extends BoolPkg.A; defaultproperties {Bits(1)=true}";
      compile({{"B.uc", Bytes(text.begin(), text.end())}}, boolParent);
    } catch (const std::exception& error) {
      rejectedInheritedBoolDefault =
          std::string(error.what()).find("Unsupported array default element") !=
          std::string::npos;
    }
    check(rejectedInheritedBoolDefault,
          "Imported bool array defaults must not be silently misread");
    TargetModel boolParentWithoutDefaults;
    boolParentWithoutDefaults.add("Core", core);
    boolParentWithoutDefaults.add(
        "EmptyBoolPkg",
        build("class A extends Object; var array<bool> Bits;"));
    std::string childBoolText =
        "class B extends EmptyBoolPkg.A; defaultproperties {Bits(1)=true}";
    auto childBool = compile(
        {{"B.uc", Bytes(childBoolText.begin(), childBoolText.end())}},
        boolParentWithoutDefaults);
    auto childBoolDefaults = classDefaults(childBool, 1);
    check(childBoolDefaults.size() == 1, "Missing child bool array default");
    Reader childBoolData{childBoolDefaults[0].data};
    check(childBoolData.index() == 2 && childBoolData.byte() == 0 &&
              childBoolData.byte() == 1 &&
              childBoolData.pos == childBoolDefaults[0].data.size(),
          "Child bool array default with no inherited value");
    rejects([&] {
      build(
          "class A extends Object; var array<bool> Bits; function "
          "F(){if(Bits){return;}}");
    });
    auto dynamicProperty = propertyInfo(dynamic, 2);
    check(dynamicProperty.kind == "ArrayProperty" &&
              (dynamicProperty.flags & 0x400000) &&
              dynamicProperty.reference == 3,
          "Array inner identity and cleanup");
    check(propertyInfo(dynamic, 3).kind == "StrProperty" &&
              dynamic.exports[2].outer == 2,
          "String inner ownership");
    auto dynamicFunction = functionInfo(dynamic, 4);
    check(dynamicFunction.result &&
              dynamicFunction.result->kind == "ArrayProperty" &&
              dynamicFunction.parameters.size() == 1 &&
              dynamicFunction.parameters[0].kind == "ArrayProperty",
          "Array signatures and field chain");
    auto defaults = classDefaults(dynamic, 1);
    check(defaults.size() == 1 && defaults[0].kind == 9, "Dynamic default tag");
    Reader defaultData{defaults[0].data};
    check(defaultData.index() == 2 && defaultData.string() == "" &&
              defaultData.string() == "tail" &&
              defaultData.pos == defaultData.data.size(),
          "Sparse dynamic string defaults");
    for (const auto& text :
         {"class A extends Object; var array<int> X; function F(){X=1;}",
          "class A extends Object; var array<int> X; function int F(){return "
          "X;}",
          "class A extends Object; var const array<int> X; function "
          "F(){X.Length=1;}",
          "class A extends Object; var const array<int> X; function "
          "F(){X.Insert(0,1);}",
          "class A extends Object; var const array<int> X; function "
          "F(){Set(X.Length);} function Set(out int V){}",
          "class A extends Object; var const array<int> X; function "
          "F(){X.Length++;}",
          "class A extends Object; var array<string> X; function F(){Set(X);} "
          "function Set(out array<int> V){}",
          "class A extends Object; var array<int> X; defaultproperties {X(0)=1 "
          "X(0)=2}"})
      rejects([&] { build(text); });
    check(Package::parse(array.serialize()).exports.size() ==
              array.exports.size(),
          "Wide array default container");
    for (const auto& text :
         {"class A extends Object; var bool Bits[2];",
          "class A extends Object; var int X[0];",
          "class A extends Object; var int X[2]; function int F(){return X;}",
          "class A extends Object; var int X; function int F(){return X[0];}",
          "class A extends Object; var int X[2]; defaultproperties {X(2)=1}",
          "class A extends Object; var const int X[2]; function F(){X[0]=1;}"})
      rejects([&] { build(text); });
    for (std::size_t i = 0; i < declarations.exports.size(); ++i) {
      auto& e = declarations.exports[i];
      auto name = declarations.name(e.name);
      if (name == "E")
        check(functionInfo(declarations, int(i) + 1).flags == 0x20900,
              "Undefined simulated event flags");
      if (name == "F")
        check(functionInfo(declarations, int(i) + 1).flags == 0x20003,
              "Final function flags");
      if (name == "X")
        check(propertyInfo(declarations, int(i) + 1).flags == 3,
              "Editable const property flags");
    }
    rejects([&] {
      localBuild({"class A extends Object;", "class a extends Object;"});
    });
    localBuild({"class B extends A; var int X;",
                "class A extends Object; var int X;"});
    rejects([&] {
      localBuild({"class B extends A; function bool F(){return true;}",
                  "class A extends Object; function int F(){return 0;}"});
    });
    for (const auto& s : {"class P extends Object; function F(){ break; }",
                          "class P extends Object; function F(){ continue; }",
                          "class P extends Object; function F(){ Set(1); } "
                          "function Set(out int V) { V=2; }",
                          "class P extends Object; function F(){ Set(); } "
                          "function Set(int V) {}",
                          "class P extends Object; static function F(){ Set(); "
                          "} function Set() {}",
                          "class P extends Object; function F(){ Set(true); } "
                          "function Set(int V) {}"})
      rejects([&] { build(s); });
    for (bool le : {false, true}) {
      Bytes utf16 = le ? Bytes{255, 254} : Bytes{254, 255};
      std::u16string source =
          u"// Unicode comment: \u041f\u0440\u043e\u0431\u0430 "
          u"\U0001F680\nclass UnicodeProbe extends Object;";
      for (auto c : source) {
        utf16.push_back(std::uint8_t(le ? c : c >> 8));
        utf16.push_back(std::uint8_t(le ? c >> 8 : c));
      }
      auto unicode = compile({{"UnicodeProbe.uc", utf16}}, core);
      check(unicode.ref(1) == "UnicodeProbe", "UTF-16 source transport");
    }
    for (const auto& s :
         {"class P extends Missing;",
          "class P extends Object; function int F(){return true;}",
          "class P extends Object; var int X; var bool x;",
          "class P extends Object; function F(){Unknown=1;}",
          "class P extends Object; var int X; static function int F(){return "
          "X;}",
          "class P extends Object; function int F(){return 2147483648;}",
          "class P extends Object; native(4096) function F();",
          "class P extends Object; /* unclosed"})
      rejects([&] { build(s); });
    auto malformed = p;
    malformed.exports[0].outer = 1;
    rejects([&] { malformed.serialize(); });
    malformed = p;
    malformed.exports[0].super = INT32_MIN;
    rejects([&] { malformed.serialize(); });
    auto truncated = bytes;
    truncated.resize(35);
    rejects([&] { Package::parse(truncated); });
    auto unsupported = bytes;
    unsupported[4] = 134;
    rejects([&] { Package::parse(unsupported); });
    std::cout << "PASS: compact indices, containers, compiler types/control "
                 "flow, negative inputs\n";
    return 0;
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
