#pragma once
#include <optional>

#include "package.hpp"
namespace openucc {
struct StateInfo {
  int super = 0, next = 0, children = 0;
  std::uint64_t probe = 0, ignore = UINT64_MAX;
  std::uint16_t labelOffset = 0xffff;
  std::uint32_t flags = 0;
};
StateInfo stateInfo(const Package&, int);
struct PropertyInfo {
  int id = 0, next = 0, dimension = 0, reference = 0, metaclass = 0,
      replicationOffset = -1;
  std::uint32_t flags = 0;
  std::string kind;
};
struct NativeInfo {
  int id = 0, native = 0, precedence = 0, replicationOffset = -1;
  std::uint32_t flags = 0;
  std::string symbol;
  std::vector<PropertyInfo> parameters;
  std::optional<PropertyInfo> result;
};
PropertyInfo propertyInfo(const Package&, int);
NativeInfo functionInfo(const Package&, int);
std::vector<std::string> enumNames(const Package&, int);
struct StructInfo {
  int super = 0, next = 0;
  std::vector<PropertyInfo> fields;
};
StructInfo structInfo(const Package&, int);
struct DefaultTag {
  std::string name;
  int kind = 0, subscript = -1;
  Bytes data;
  std::string structName;
};
struct ClassInfo {
  std::uint32_t flags = 0;
  int within = 0;
  std::string config;
};
ClassInfo classInfo(const Package&, int);
std::vector<DefaultTag> classDefaults(const Package&, int);
std::vector<DefaultTag> defaultTags(const Package&, Reader&);
// Bytecode is structurally decoded; dependency source is never required.
std::vector<NativeInfo> nativeOperators(const Package&, int owner);
}  // namespace openucc
