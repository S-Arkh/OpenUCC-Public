#pragma once
#include <map>
#include <memory>
#include <optional>

#include "metadata.hpp"
namespace openucc {
struct Symbol {
  std::size_t package = 0;
  int id = 0;
  bool operator<(const Symbol& b) const {
    return package < b.package || (package == b.package && id < b.id);
  }
  bool operator==(const Symbol& b) const {
    return package == b.package && id == b.id;
  }
};
struct TargetPackage {
  std::string name;
  Package data;
  std::map<std::string, std::vector<int>> exports;
  std::map<std::pair<int, std::string>, std::vector<int>> members;
  std::map<int, std::vector<int>> enums;
  bool referenceOnly = false;
};
// A graph of serialized target identities, never host/native UObject storage.
class TargetModel {
 public:
  std::vector<TargetPackage> packages;
  void add(std::string name, Package package);
  void addReference(std::string name, Package package);
  std::optional<Symbol> resolve(Symbol) const;
  bool isIntrinsicClass(Symbol) const;
  std::optional<Symbol> findClass(
      const std::string& name, std::optional<std::size_t> preferred = {}) const;
  std::vector<Symbol> ancestry(Symbol) const;
  std::optional<Symbol> member(Symbol, const std::string& name) const;
  std::string identity(Symbol) const;

 private:
  std::map<std::string, std::size_t> byName;
};
class TargetImports {
 public:
  TargetImports(const TargetModel& target, Package& output,
                std::string selfPackage = {})
      : target(target), output(output), selfPackage(lower(selfPackage)) {}
  void bindLocal(const std::string& path, int id);
  void bindReference(Symbol source, int id);
  int symbol(Symbol);
  int referenceObject(Symbol);
  int nativeClass(const std::string& package, const std::string& name);

 private:
  const TargetModel& target;
  Package& output;
  std::map<Symbol, int> imported;
  std::string selfPackage;
  std::map<std::string, int> locals;
  int entry(const std::string& classPackage, const std::string& kind, int outer,
            const std::string& name);
  int root(const std::string& package);
};
}  // namespace openucc
