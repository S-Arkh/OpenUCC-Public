#include "openucc/target.hpp"

#include <set>
#include <stdexcept>
namespace openucc {
void TargetModel::add(std::string name, Package p) {
  p.validate();
  auto key = lower(name);
  if (key.empty() || byName.count(key))
    throw std::runtime_error("Duplicate/empty dependency package: " + name);
  TargetPackage item{std::move(name), std::move(p), {}};
  for (std::size_t i = 0; i < item.data.exports.size(); ++i) {
    item.exports[lower(item.data.ref(int(i) + 1))].push_back(int(i) + 1);
    const auto& e = item.data.exports[i];
    item.members[{e.outer, lower(item.data.name(e.name))}].push_back(int(i) +
                                                                     1);
    if (item.data.kind(int(i) + 1) == "Enum")
      item.enums[e.outer].push_back(int(i) + 1);
  }
  byName.emplace(key, packages.size());
  packages.push_back(std::move(item));
}
std::optional<Symbol> TargetModel::resolve(Symbol s) const {
  if (!s.id) return {};
  if (s.package >= packages.size())
    throw std::runtime_error("Invalid target package identity");
  auto& source = packages[s.package];
  if (s.id > 0) {
    source.data.ref(s.id);
    return s;
  }
  auto path = source.data.ref(s.id);
  auto dot = path.find('.');
  if (dot == path.npos) return {};
  auto pkg = byName.find(lower(path.substr(0, dot)));
  if (pkg == byName.end()) return {};
  auto& target = packages[pkg->second];
  auto found = target.exports.find(lower(path.substr(dot + 1)));
  if (found == target.exports.end()) return {};
  auto& im = source.data.imports[std::size_t(-std::int64_t(s.id) - 1)];
  auto expected =
      lower(source.data.name(im.package) + "." + source.data.name(im.type));
  std::optional<Symbol> result;
  for (auto id : found->second) {
    auto cls = target.data.exports[id - 1].type;
    auto actual = cls == 0  ? "Core.Class"
                  : cls < 0 ? target.data.ref(cls)
                            : target.name + "." + target.data.ref(cls);
    if (lower(actual) != expected) continue;
    if (result) throw std::runtime_error("Ambiguous typed import: " + path);
    result = Symbol{pkg->second, id};
  }
  return result;
}
void TargetModel::addReference(std::string name, Package package) {
  auto found = byName.find(lower(name));
  if (found != byName.end()) {
    auto& existing = packages[found->second];
    if (existing.data.serialize() != package.serialize())
      throw std::runtime_error("Reference differs from loaded dependency: " +
                               name);
    existing.referenceOnly = true;
    return;
  }
  add(std::move(name), std::move(package));
  packages.back().referenceOnly = true;
}
std::string TargetModel::identity(Symbol s) const {
  if (s.package >= packages.size())
    throw std::runtime_error("Invalid target package identity");
  auto& p = packages[s.package];
  return s.id > 0 ? p.name + "." + p.data.ref(s.id) : p.data.ref(s.id);
}
bool TargetModel::isIntrinsicClass(Symbol s) const {
  if (s.package >= packages.size() || s.id >= 0) return false;
  const auto& package = packages[s.package];
  const auto& p = package.data;
  p.ref(s.id);  // Validate the reference before indexing the import table.
  const auto& item = p.imports[std::size_t(-std::int64_t(s.id) - 1)];
  // These Core classes are native registrations absent from its export table.
  // Isolated SDK probes attest their identities and UObject compatibility.
  // Require an explicit original import, not arbitrary unresolved classes.
  const auto identity = lower(p.ref(s.id));
  const bool nativeCore =
      package.referenceOnly && byName.count("core") &&
      (identity == "core.language" || identity == "core.textbuffer" ||
       identity == "core.package");
  // SDK compilation attests this native-only Engine class and its UObject
  // compatibility. Keep the explicit-reference and loaded-module requirements.
  bool nativeMesh = false;
  if (byName.count("engine") && identity == "engine.skeletalmesh") {
    nativeMesh = package.referenceOnly;
    // A self reference can attest the same native identity through a typed
    // resource import, while a loaded dependency supplies its Class import.
    for (const auto& reference : packages) {
      if (nativeMesh) break;
      if (!reference.referenceOnly) continue;
      for (const auto& resource : reference.data.imports)
        if (resource.outer < 0 &&
            lower(reference.data.name(resource.package)) == "engine" &&
            lower(reference.data.name(resource.type)) == "skeletalmesh") {
          nativeMesh = true;
          break;
        }
    }
  }
  if (lower(p.name(item.package)) != "core" ||
      lower(p.name(item.type)) != "class" || item.outer >= 0 || resolve(s))
    return false;
  if (lower(p.ref(item.outer)) == lower(package.name) || nativeCore ||
      nativeMesh)
    return true;
  // A resource package may reference a native class registered by a loaded
  // defining module. Require that module's explicit self-Class import.
  bool declaredNative = false;
  if (item.outer < 0) {
    auto owner = byName.find(lower(p.ref(item.outer)));
    if (owner != byName.end() && owner->second != s.package) {
      const auto& definition = packages[owner->second];
      for (std::size_t i = 0; i < definition.data.imports.size(); ++i) {
        const auto& candidate = definition.data.imports[i];
        if (candidate.outer < 0 &&
            lower(definition.data.ref(candidate.outer)) ==
                lower(definition.name) &&
            lower(definition.data.name(candidate.package)) == "core" &&
            lower(definition.data.name(candidate.type)) == "class" &&
            lower(definition.data.ref(-int(i) - 1)) == identity) {
          declaredNative = true;
          break;
        }
      }
    }
  }
  return declaredNative;
}
std::optional<Symbol> TargetModel::findClass(
    const std::string& text, std::optional<std::size_t> preferred) const {
  auto name = lower(text);
  auto dot = name.find('.');
  std::optional<Symbol> match;
  if (preferred && *preferred >= packages.size())
    throw std::runtime_error("Invalid preferred package");
  if (preferred && dot == name.npos) {
    auto& p = packages[*preferred];
    auto it = p.exports.find(name);
    if (it != p.exports.end())
      for (auto id : it->second)
        if (!p.data.exports[id - 1].type) {
          if (match) throw std::runtime_error("Ambiguous class: " + text);
          match = Symbol{*preferred, id};
        }
    if (match) return match;
  }
  for (std::size_t i = 0; i < packages.size(); ++i) {
    auto& p = packages[i];
    if (dot != name.npos && lower(p.name) != name.substr(0, dot)) continue;
    auto found = p.exports.find(dot == name.npos ? name : name.substr(dot + 1));
    if (found == p.exports.end()) continue;
    for (auto id : found->second) {
      if (p.data.exports[id - 1].type) continue;
      Symbol s{i, id};
      if (match) throw std::runtime_error("Ambiguous class: " + text);
      match = s;
    }
  }
  if (match) return match;
  // Self-package Class imports represent intrinsic classes supplied by the
  // package's native module. Do not accept arbitrary unresolved imports.
  for (std::size_t i = 0; i < packages.size(); ++i) {
    const auto& p = packages[i];
    for (std::size_t j = 0; j < p.data.imports.size(); ++j) {
      Symbol s{i, -int(j) - 1};
      const auto leaf = lower(p.data.name(p.data.imports[j].name));
      if (leaf != (dot == name.npos ? name : name.substr(dot + 1))) continue;
      const auto qualified = lower(identity(s));
      if ((dot == name.npos ? leaf : qualified) != name) continue;
      if (!isIntrinsicClass(s)) continue;
      if (match && lower(identity(*match)) != qualified)
        throw std::runtime_error("Ambiguous intrinsic class: " + text);
      match = s;
    }
  }
  return match;
}
std::vector<Symbol> TargetModel::ancestry(Symbol input) const {
  std::vector<Symbol> chain;
  if (!input.id) return chain;
  std::set<Symbol> seen;
  auto current = resolve(input);
  if (!current && isIntrinsicClass(input)) {
    chain.push_back(input);
    // An isolated SDK assignment and checked-cast probe attests this native
    // inheritance edge; Mesh may itself be an attested native Class import.
    if (lower(identity(input)) == "engine.skeletalmesh") {
      auto mesh = findClass("Engine.Mesh");
      if (mesh) {
        auto bases = ancestry(*mesh);
        chain.insert(chain.end(), bases.begin(), bases.end());
        return chain;
      }
    }
    // SDK compilation attests NetConnection/Viewport -> Player assignments
    // and checked downcasts from Player. Require the explicit native
    // import and a serialized Player class; other native chains stay unknown.
    if (lower(identity(input)) == "engine.netconnection" ||
        lower(identity(input)) == "engine.viewport") {
      auto player = findClass("Engine.Player");
      if (player && player->id > 0) {
        auto bases = ancestry(*player);
        chain.insert(chain.end(), bases.begin(), bases.end());
        return chain;
      }
    }
    auto object = findClass("Core.Object");
    if (object && object->id > 0) chain.push_back(*object);
    return chain;
  }
  if (!current)
    throw std::runtime_error("Unresolved class: " + identity(input));
  while (current) {
    auto s = *current;
    if (!seen.insert(s).second || seen.size() > 256)
      throw std::runtime_error("Cyclic/excessive inheritance: " + identity(s));
    auto& p = packages[s.package].data;
    auto& e = p.exports[s.id - 1];
    if (e.type)
      throw std::runtime_error("Superclass is not a class: " + identity(s));
    chain.push_back(s);
    if (!e.super) break;
    Symbol next{s.package, e.super};
    current = resolve(next);
    if (!current)
      throw std::runtime_error("Unresolved superclass: " + identity(next));
  }
  return chain;
}
std::optional<Symbol> TargetModel::member(Symbol owner,
                                          const std::string& name) const {
  for (auto s : ancestry(owner)) {
    const auto& members = packages[s.package].members;
    auto found = members.find({s.id, lower(name)});
    if (found == members.end()) continue;
    if (found->second.size() != 1)
      throw std::runtime_error("Ambiguous member: " + name);
    return Symbol{s.package, found->second.front()};
  }
  return {};
}
int TargetImports::entry(const std::string& cp, const std::string& kind,
                         int outer, const std::string& name) {
  int packageId = output.intern(cp), kindId = output.intern(kind),
      nameId = output.intern(name);
  for (std::size_t i = 0; i < output.imports.size(); ++i) {
    auto& e = output.imports[i];
    if (e.package == packageId && e.type == kindId && e.outer == outer &&
        e.name == nameId)
      return -int(i) - 1;
  }
  output.imports.push_back({packageId, kindId, outer, nameId});
  return -int(output.imports.size());
}
int TargetImports::root(const std::string& package) {
  return entry("Core", "Package", 0, package);
}
int TargetImports::nativeClass(const std::string& package,
                               const std::string& name) {
  auto owner = root(package);
  return entry("Core", "Class", owner, name);
}
int TargetImports::symbol(Symbol s) {
  auto resolved = target.resolve(s);
  if (!resolved && target.isIntrinsicClass(s)) {
    const auto& p = target.packages[s.package];
    const auto& item = p.data.imports[std::size_t(-std::int64_t(s.id) - 1)];
    return nativeClass(p.data.ref(item.outer), p.data.name(item.name));
  }
  if (!resolved)
    throw std::runtime_error("Cannot import unresolved identity: " +
                             target.identity(s));
  s = *resolved;
  auto bound = imported.find(s);
  if (bound != imported.end()) return bound->second;
  if (lower(target.packages[s.package].name) == selfPackage) {
    auto path = lower(target.packages[s.package].data.ref(s.id));
    auto local = locals.find(path);
    if (local == locals.end())
      throw std::runtime_error(
          "Reference export requires a source declaration: " +
          target.identity(s));
    return local->second;
  }
  auto cached = imported.find(s);
  if (cached != imported.end()) return cached->second;
  auto& p = target.packages[s.package];
  auto& e = p.data.exports[s.id - 1];
  int owner = e.outer ? symbol({s.package, e.outer}) : root(p.name);
  std::string classPackage = "Core", kind = "Class";
  if (e.type) {
    auto path =
        e.type < 0 ? p.data.ref(e.type) : p.name + "." + p.data.ref(e.type);
    auto dot = path.find('.');
    if (dot == path.npos)
      throw std::runtime_error("Unqualified target metaclass");
    classPackage = path.substr(0, dot);
    kind = path.substr(path.rfind('.') + 1);
  }
  int result = entry(classPackage, kind, owner, p.data.name(e.name));
  imported.emplace(s, result);
  return result;
}
int TargetImports::referenceObject(Symbol s) {
  if (s.id >= 0 || s.package >= target.packages.size())
    throw std::runtime_error("Expected an explicit reference object import");
  const auto& source = target.packages[s.package];
  const auto& p = source.data;
  if (!source.referenceOnly || lower(source.name) != selfPackage)
    throw std::runtime_error(
        "External object declaration requires the self reference");
  p.ref(s.id);
  std::vector<int> chain;
  std::set<int> seen;
  int id = s.id;
  while (id) {
    if (id > 0 || !seen.insert(id).second)
      throw std::runtime_error("Invalid external reference object owner chain");
    p.ref(id);
    chain.push_back(id);
    const auto& item = p.imports.at(std::size_t(-std::int64_t(id) - 1));
    if (id != s.id && (lower(p.name(item.package)) != "core" ||
                       lower(p.name(item.type)) != "package"))
      throw std::runtime_error(
          "External reference object requires package groups");
    id = item.outer;
  }
  const auto& rootImport =
      p.imports.at(std::size_t(-std::int64_t(chain.back()) - 1));
  if (lower(p.name(rootImport.package)) != "core" ||
      lower(p.name(rootImport.type)) != "package")
    throw std::runtime_error("External object import has no package root");
  auto rootName = p.name(rootImport.name);
  for (const auto& loaded : target.packages)
    if (lower(loaded.name) == lower(rootName))
      throw std::runtime_error("Loaded dependency must resolve object: " +
                               p.ref(s.id));
  const auto& leaf = p.imports.at(std::size_t(-std::int64_t(s.id) - 1));
  if (lower(p.name(leaf.type)) == "class" ||
      !target.findClass(p.name(leaf.package) + "." + p.name(leaf.type)))
    throw std::runtime_error("Unresolved external object class declaration");
  int owner = 0;
  for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
    const auto& item = p.imports.at(std::size_t(-std::int64_t(*it) - 1));
    owner = entry(p.name(item.package), p.name(item.type), owner,
                  p.name(item.name));
  }
  return owner;
}
void TargetImports::bindLocal(const std::string& path, int id) {
  if (id <= 0) throw std::runtime_error("Invalid local export binding");
  auto result = locals.emplace(lower(path), id);
  if (!result.second && result.first->second != id)
    throw std::runtime_error("Conflicting local export binding: " + path);
}
void TargetImports::bindReference(Symbol source, int id) {
  if (id <= 0 || source.id <= 0 ||
      lower(target.packages.at(source.package).name) != selfPackage)
    throw std::runtime_error("Invalid typed reference export binding");
  target.packages[source.package].data.ref(source.id);
  auto result = imported.emplace(source, id);
  if (!result.second && result.first->second != id)
    throw std::runtime_error("Conflicting typed reference export binding");
}
}  // namespace openucc
